#include <raylib.h>
#include <rlgl.h>       // rlSetClipPlanes
#include <raymath.h>
#include <vector>
#include "CameraRig.hpp"
#include "IcoSphere.hpp"
#include "Noise3D.hpp"
#include "PlanetMesh.hpp"
#include "PlanetNoise.hpp"

int main() {
    // ══ CONFIGURACIÓN ═══════════════════════════════════════════════════════
    const int   screenWidth  = 1800;
    const int   screenHeight = 960;
    const int   TEX_W = 2048, TEX_H = 1024;

    // Escala: 1 unidad = 1 km (radio tipo Tierra)
    const float sphereR   = 6000.0f;
    Vector3     spherePos = {0, 0, 0};
    const int ICO_DEPTH = 7; // profundidad de subdivisión de la icosfera

    // Grosor de cada shell respecto al radio del planeta (de dentro afuera)
    const float CLOUD_SHELL = 1.03f;
    const float ATMO_SHELL  = 1.06f;
    float       atmoPower   = 2.0f;      // foco del halo (menor = más ancho)

    // Poste de referencia: 10 m de altura y 0,5 m de radio (unidades: km)
    const float POLE_HEIGHT_KM = 0.01f;
    const float POLE_RADIUS_KM = 0.0005f;
    const float P_VIEW_DIST_KM = 0.05f;   // distancia de la cámara al pulsar P (50 m)

    // Experimento temporal (tecla K): 0 todo, 1 sin atmósfera, 2 sin nubes, 3 sin capas
    int shellTest = 0;
    const char* shellTestNames[] = { "todo", "sin atmosfera", "sin nubes", "sin capas" };

    // Rotaciones independientes (rad/s)
    float       rotAngle   = 0.0f;
    const float rot_speed  = 0.0f;       // 0 = planeta quieto; súbelo para ver la deriva
    float       cloudAngle = 0.0f;
    const float cloud_speed = 0.02f;     // 0.5 para verlo rápido, 0.15 para el efecto creíble
    float       sunAngle   = 0.0f;
    const float sun_speed  = 0.05f;      // el terminador recorre el planeta

    // Sol
    const float SUN_DIST = 20000.0f;     // ficticia: solo importa la DIRECCIÓN
    const float SUN_TILT = 3000.0f;      // altura sobre el plano del planeta
    const float ambient  = 0.06f;        // luz residual en la noche

    // Semillas del ruido procedural (0 = aleatoria)
    unsigned int planetSeed = 0;
    float cloudUmbral = 0.2f;            // > umbral = nube (fbm máx ≈ 0.42)
    float cloudDureza = 0.1f;            // margen del smoothstep (mayor = borde más suave)

    InitWindow(screenWidth, screenHeight, "Proto 3D - sandbox camara orbital");
    SetTargetFPS(60);

    // ══ GEOMETRÍA: icosfera con relieve, partida en 20 caras base ═══════════
    std::vector<IcoVertex> icoVertices;
    std::vector<int> icoIndices;
    BuildIcosphere(ICO_DEPTH, icoVertices, icoIndices);

    Mesh sphereMesh = BuildPlanetMesh(icoVertices, icoIndices, ICO_DEPTH);
    std::vector<Mesh> icoFaces = SplitByBaseFace(sphereMesh, icoIndices, ICO_DEPTH);
    std::vector<Vector3> faceCenters;
    for (const Mesh& face : icoFaces) {
        Vector3 center = {0, 0, 0};
        for (int i = 0; i < face.vertexCount; ++i) {
            center.x += face.vertices[i*3+0];
            center.y += face.vertices[i*3+1];
            center.z += face.vertices[i*3+2];
        }
        center.x /= face.vertexCount;
        center.y /= face.vertexCount;
        center.z /= face.vertexCount;
        center = Vector3Normalize(center);
        faceCenters.push_back(center);
    }
    const int octavas = OctavesForDepth(ICO_DEPTH);

    // ══ TEXTURAS ════════════════════════════════════════════════════════════
    // Terreno: la altura va en texcoords; el color se calcula en terreno.fs.
    Image cloudImg = GenerateCloudImage(TEX_W, TEX_H, cloudUmbral, cloudDureza);
    Texture2D cloudTex = LoadTextureFromImage(cloudImg);
    SetTextureWrap(cloudTex, TEXTURE_WRAP_REPEAT);
    ExportImage(cloudImg, "nubes.png");
    UnloadImage(cloudImg);

    // ══ SHADERS ═════════════════════════════════════════════════════════════
    Shader terrenoShader = LoadShader("proto/3d/shaders/terreno.vs", "proto/3d/shaders/terreno.fs");
    Shader cloudShader  = LoadShader("proto/3d/shaders/nubes.vs",    "proto/3d/shaders/nubes.fs");
    Shader atmoShader   = LoadShader("proto/3d/shaders/atmosfera.vs", "proto/3d/shaders/atmosfera.fs");

    // Uniforms: se resuelven UNA vez (buscar por nombre cada frame es desperdicio)
    int sunDirLoc     = GetShaderLocation(terrenoShader, "sunDir");
    int ambientLoc    = GetShaderLocation(terrenoShader, "ambient");
    int cloudSunLoc   = GetShaderLocation(cloudShader,  "sunDir");
    int cloudAmbLoc   = GetShaderLocation(cloudShader,  "ambient");
    int atmoSunLoc    = GetShaderLocation(atmoShader,   "sunDir");
    int atmoPowerLoc  = GetShaderLocation(atmoShader,   "power");
    int camPosLoc     = GetShaderLocation(atmoShader,   "camPos");

    // ══ MODELOS ═════════════════════════════════════════════════════════════
    // Un modelo por cara del icosaedro: cada malla tiene menos de 65 535 vértices.
    std::vector<Model> planetFaces;
    for (const Mesh& face : icoFaces) {
        Model m = LoadModelFromMesh(face);
        m.materials[0].shader = terrenoShader;
        planetFaces.push_back(m);
    }

    // Shell de nubes: sin relieve, solo la textura proyectada
    Model cloudModel = LoadModelFromMesh(GenMeshSphere(1.0f, 128, 64));
    cloudModel.materials[0].shader = cloudShader;
    cloudModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = cloudTex;

    // Shell atmosférico: baja resolución (es un degradado suave)
    Model atmoModel = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 32));
    atmoModel.materials[0].shader = atmoShader;

    // ══ CÁMARA FPS ══════════════════════════════════════════════════════════
    CameraRig rig;
    rig.Init({sphereR*3.0f, sphereR*2.5f, sphereR*2.0f}, {0,0,0});
    DisableCursor();

    CameraEnv env;
    // Los callbacks capturan por referencia: `main` conoce el terreno y se lo
    // presta a la cámara sin que CameraRig.hpp tenga que incluirlo.
    env.groundApprox = [&](Vector3 dir) {
        // Superficie real de la malla (triángulos), igual que la que se dibuja
        Vector3 localDir = Vector3RotateByQuaternion(dir, QuaternionInvert(env.spin));
        return MeshGroundRadiusLocal(icoFaces, faceCenters, localDir) * sphereR;   // mundial, km
    };
    env.groundExact = [&](Vector3 localDir) {
        return MeshGroundRadiusLocal(icoFaces, faceCenters, localDir) * sphereR;      // local
    };


    // Punto de prueba (tecla P): cima más alta de la malla, vista desde P_VIEW_DIST_KM
    Vector3 peakLocal = {0, 0, 1};
    float peakRadius = 0.0f;   // radio de la cima en unidades de malla
    for (int v = 0; v < sphereMesh.vertexCount; ++v) {
        Vector3 p = { sphereMesh.vertices[v*3], sphereMesh.vertices[v*3+1], sphereMesh.vertices[v*3+2] };
        if (Vector3Length(p) > peakRadius) { peakRadius = Vector3Length(p); peakLocal = p; }
    }
    peakLocal = Vector3Normalize(peakLocal);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        rotAngle   += rot_speed * dt;
        cloudAngle += cloud_speed * dt;
        sunAngle   += sun_speed * dt;

        // ── Orientaciones: el -90° X alinea la malla (polo norte en +Z) ──
        Quaternion qOrient    = QuaternionFromAxisAngle({1, 0, 0}, -90.0f * DEG2RAD);
        Quaternion qSpin      = QuaternionFromAxisAngle({0, 1, 0},  rotAngle);
        Quaternion qCloudSpin = QuaternionFromAxisAngle({0, 1, 0},  cloudAngle);
        Quaternion q          = QuaternionMultiply(qSpin, qOrient);
        Quaternion qCloud     = QuaternionMultiply(qCloudSpin, qOrient);

        env.spherePos = spherePos;
        env.sphereR   = sphereR;
        env.spin         = q;

        if (IsKeyPressed(KEY_P)) {
            // Cima en mundo, con el planeta rotando en este instante
            Vector3 up = Vector3RotateByQuaternion(peakLocal, q);
            Vector3 peakWorld = Vector3Add(spherePos, Vector3Scale(up, peakRadius * sphereR));
            Vector3 side = Vector3Normalize(Vector3CrossProduct(up, {0, 1, 0}));
            Vector3 viewPos = Vector3Add(Vector3Add(peakWorld, Vector3Scale(side, -P_VIEW_DIST_KM)), Vector3Scale(up, CameraRig::EYE_HEIGHT));
            rig.cam.position = viewPos;
            rig.EnterGround(env, Vector3Subtract(peakWorld, viewPos));
        }

        if (IsKeyPressed(KEY_K)) shellTest = (shellTest + 1) % 4;
        bool drawClouds = shellTest != 2 && shellTest != 3;
        bool drawAtmo   = shellTest != 1 && shellTest != 3;

        rig.Update(dt, env);

        // Altitud sobre el nivel del mar (km): el relieve solo sube (r >= R), así que
        // dist - R da la elevación del terreno y crece al volar.
        float altitude = Vector3Length(Vector3Subtract(rig.cam.position, spherePos)) - sphereR;
        if (altitude < 0.0f) altitude = 0.0f;

        // ── UNIFORMS: un único bloque para los tres shaders ──
        // El sol es UN vector compartido: los tres shaders reciben el mismo.
        Vector3 sunPos = { cosf(sunAngle) * SUN_DIST, SUN_TILT, sinf(sunAngle) * SUN_DIST };
        Vector3 sunDir = Vector3Normalize(sunPos);

        SetShaderValue(terrenoShader, sunDirLoc,   &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(terrenoShader, ambientLoc,  &ambient, SHADER_UNIFORM_FLOAT);

        SetShaderValue(cloudShader,  cloudSunLoc,  &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(cloudShader,  cloudAmbLoc,  &ambient, SHADER_UNIFORM_FLOAT);

        SetShaderValue(atmoShader,   atmoSunLoc,   &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(atmoShader,   atmoPowerLoc, &atmoPower, SHADER_UNIFORM_FLOAT);
        SetShaderValue(atmoShader,   camPosLoc,    &rig.cam.position, SHADER_UNIFORM_VEC3);

        BeginDrawing();
        ClearBackground({0, 0, 20, 255});

        // Near adaptativo: 0.001 pegado al suelo (precisión al caminar),
        // 0.1 en órbita (el z-buffer se estira y gana resolución).
        float distCam   = Vector3Length(Vector3Subtract(rig.cam.position, spherePos));
        float nearPlane = (distCam < sphereR * 1.1f) ? 0.001f : 0.1f;
        rlSetClipPlanes(nearPlane, 200000.0f);   // planos GLOBALES de raylib

        BeginMode3D(rig.cam);

        // ── DIBUJO: de dentro afuera, cada shell sin test de profundidad ──
        // Sin depth test porque las capas están separadas por ~km y el z-buffer
        // no da esa precisión a distancia de órbita (evita z-fighting).
        Vector3 axis, cloudAxis; float angle, cloudAngleRad;
        QuaternionToAxisAngle(QuaternionNormalize(q),      &axis,       &angle);
        QuaternionToAxisAngle(QuaternionNormalize(qCloud), &cloudAxis,  &cloudAngleRad);

        // 1) Terreno: opaco, con test de profundidad normal
        for (const Model& face : planetFaces) {
            DrawModelEx(face, spherePos, axis, angle * RAD2DEG,
                (Vector3){sphereR, sphereR, sphereR}, WHITE);   // WHITE = no sobretiñe
        }

        // Poste de referencia sobre la cima: la base está en el vértice de la malla,
        // así que queda exactamente sobre el terreno que se dibuja
        Vector3 poleUp   = Vector3RotateByQuaternion(peakLocal, q);
        Vector3 poleBase = Vector3Add(spherePos, Vector3Scale(poleUp, peakRadius * sphereR));
        Vector3 poleTop  = Vector3Add(poleBase, Vector3Scale(poleUp, POLE_HEIGHT_KM));
        DrawCylinderEx(poleBase, poleTop, POLE_RADIUS_KM, POLE_RADIUS_KM, 8, RED);

        // 2) Nubes: BLEND_ALPHA (tapan lo de detrás) + sin depth test
        if (drawClouds) {
            BeginBlendMode(BLEND_ALPHA);
            rlDisableDepthTest();
            DrawModelEx(cloudModel, spherePos, cloudAxis, cloudAngleRad * RAD2DEG,
                (Vector3){ sphereR*CLOUD_SHELL, sphereR*CLOUD_SHELL, sphereR*CLOUD_SHELL }, WHITE);
            rlEnableDepthTest();
            EndBlendMode();
        }

        // 3) Atmósfera: BLEND_ADDITIVE (suma luz) + sin depth test
        if (drawAtmo) {
            BeginBlendMode(BLEND_ADDITIVE);
            rlDisableDepthTest();
            DrawModelEx(atmoModel, spherePos, axis, angle * RAD2DEG,
                (Vector3){ sphereR*ATMO_SHELL, sphereR*ATMO_SHELL, sphereR*ATMO_SHELL }, WHITE);
            rlEnableDepthTest();
            EndBlendMode();
        }

        EndMode3D();

        // UI
        DrawText("WASD mover | Raton mirar | Q/E altura | F: superficie | Rueda velocidad", 20, 20, 20, RAYWHITE);
        DrawText(TextFormat("Radio: %d km | Altitud: %.1f km | Vel: %.0f km/s",
                  (int)sphereR, altitude, rig.moveSpeed), 20, 44, 20, SKYBLUE);
        DrawText(TextFormat(" Modo: %s",
                  (rig.mode == CamMode::FLY) ? "FLY" : "GROUND"), 20, 68, 20, SKYBLUE);

        // Depuración: superficie de la malla frente a la de colisión bajo la cámara
        Vector3 camDir = Vector3Normalize(Vector3Subtract(rig.cam.position, spherePos));
        float meshKm = MeshGroundRadiusLocal(icoFaces, faceCenters,
            Vector3RotateByQuaternion(camDir, QuaternionInvert(q))) * sphereR;
        float collKm = env.groundApprox(camDir);
        DrawText(TextFormat(" Malla: %.4f km | Colision: %.4f km | Radio cam: %.4f km",
                  meshKm - sphereR, collKm - sphereR, distCam - sphereR), 20, 92, 20, LIME);
        DrawText(TextFormat(" FPS: %d | K capas: %s", GetFPS(), shellTestNames[shellTest]), 20, 116, 20, LIME);
        EndDrawing();
    }

    for (Model& face : planetFaces) UnloadModel(face);
    MemFree(sphereMesh.vertices);
    MemFree(sphereMesh.normals);
    MemFree(sphereMesh.texcoords);
    CloseWindow();
    EnableCursor();
    return 0;
}
