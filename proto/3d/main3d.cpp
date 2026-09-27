#include <raylib.h>
#include <rlgl.h>       // rlSetClipPlanes
#include <raymath.h>
#include <cmath>
#include <vector>
#include <string>

struct NoiseGrid {
    int cell, texW, gw, gh;
    std::vector<float> vert;
};

// x entre 0 y 1 (altura). Paleta por tramos: paradas de (altura, color)
// interpoladas linealmente entre vecinas. Estilo "mapa de biomas".
static Color HeightColor(double x) {
    static double alt[]   = { 0.00, 0.02, 0.10, 0.25, 0.55, 0.80, 1.00 };
    static Color   colors[] = {
        { 18,  52, 110, 255 },   // océano profundo
        { 45, 105, 170, 255 },   // océano somero (plataforma)
        {205, 195, 145, 255 },   // arena / costa
        { 95, 165,  85, 255 },   // tierras bajas verdes
        { 60, 115,  55, 255 },   // tierras altas verde oscuro
        {140, 125, 110, 255 },   // montaña rocosa
        {235, 240, 250, 255 },   // nieve
    };

    double h = x;
    if (h <= alt[0]) return colors[0];
    if (h >= alt[6]) return colors[6];

    for (int i = 0; i < 6; ++i) {
        if (h <= alt[i + 1]) {
            double t = (h - alt[i]) / (alt[i + 1] - alt[i]);   // 0..1 en el tramo
            float r = colors[i].r + (colors[i+1].r - colors[i].r) * (float)t;
            float g = colors[i].g + (colors[i+1].g - colors[i].g) * (float)t;
            float b = colors[i].b + (colors[i+1].b - colors[i].b) * (float)t;
            return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
        }
    }
    return colors[6];
}

double NoiseAt(int x, int y, const struct NoiseGrid& g) {
    // Clampear el PÍXEL antes de dividir, no la celda. Si y==TEX_H (texcoord
    // de longitud = 1.0, costura) clampar aquí a TEX_H-1 conserva la
    // interpolación: cae en la última celda, cuyo segundo dato es la fila 0
    // duplicada → converge al mismo valor que y==0. Clampear gy rompía eso.
    if (x > g.texW - 1) x = g.texW - 1;
    int yMax = (g.gh - 1) * g.cell - 1;   // = TEX_H - 1
    if (y > yMax) y = yMax;

    int gx = x / g.cell;
    int gy = y / g.cell;

    // cuánto he avanzado dentro de la celda (0..1), suavizado
    float tx = (float)(x % g.cell) / (float)g.cell;
    float ty = (float)(y % g.cell) / (float)g.cell;
    tx = tx * tx * (3.0f - 2.0f * tx);        // smoothstep
    ty = ty * ty * (3.0f - 2.0f * ty);

    // las 4 esquinas de la celda (índices en vert)
    int i00 = gy * g.gw + gx;
    int i10 = i00 + 1;
    int i01 = i00 + g.gw;
    int i11 = i01 + 1;

    float arriba = g.vert[i00] + (g.vert[i10] - g.vert[i00]) * tx;   // topo izq→der
    float abajo  = g.vert[i01] + (g.vert[i11] - g.vert[i01]) * tx;   // bottom izq→der
    return arriba + (abajo - arriba) * ty;                     // y mezcla vertical
}

struct NoiseGrid MakeNoiseGrid(int cell, int texW, int texH, unsigned int seed = 0) {
    struct NoiseGrid ng;
    ng.cell = cell;
    ng.texW = texW;
    int gw = texW / cell + 1;
    int gh = texH / cell + 1;
    ng.gw = gw;
    ng.gh = gh;
    ng.vert.resize(gw * gh);
    srand(seed);
    for (size_t i = 0; i < ng.vert.size(); ++i) ng.vert[i] = (float)(rand() % 1000) / 1000.0f;
    for (int c = 0; c < gw; ++c) ng.vert[(gh - 1) * gw + c] = ng.vert[c];
    return ng;
}
static double SmoothStep(double edge0, double edge1, double x) {
    double t = (x - edge0) / (edge1 - edge0);   // normaliza a 0..1 dentro del rango
    if (t < 0.0) t = 0.0;                       // clamp
    if (t > 1.0) t = 1.0;
    return t * t * (3.0 - 2.0 * t);             // la curva que ya usabas
}

float HeightAt(int x, int y, const NoiseGrid& gA, const NoiseGrid& gB,
               const NoiseGrid& gC, const NoiseGrid& gD) {
    double cont = NoiseAt(x, y, gA);                                   // 0..1 forma maestra
    double det  = (0.30*NoiseAt(x,y,gB) + 0.20*NoiseAt(x,y,gC)
                + 0.10*NoiseAt(x,y,gD)) / 0.6;                        // 0..1 detalle

    // Curva de costa: empuja cont hacia 0 (mar) o 1 (tierra) alrededor del umbral
    double land = SmoothStep(0.45, 0.65, cont);                        // costa nítida

    double x01 = land * (0.35 + det * 0.4);                            // relieve SOLO en tierra
    return (float)Clamp(x01, 0.0, 1.0);                                // mar → ~0, tierra → 0.35..1
}

float CloudAt(int x, int y, const struct NoiseGrid& gE, const struct NoiseGrid& gF,
              const struct NoiseGrid& gG, double umbral, double dureza) {
    double fbm = 0.6*NoiseAt(x,y,gE) + 0.3*NoiseAt(x,y,gF) + 0.1*NoiseAt(x,y,gG);
    return (float)Clamp(SmoothStep(umbral, umbral + dureza, fbm), 0.0, 1.0);
}

// Radio del suelo (km) bajo una dirección unitaria del centro del planeta.
// Replica el mapeo del displacement: convierte la dirección mundial al marco
// del mesh (rotación inversa del render, -90° X) y muestrea la misma altura.
float GroundRadius(Vector3 dir, float sphereR, int texW, int texH,
                   const NoiseGrid& gA, const NoiseGrid& gB,
                   const NoiseGrid& gC, const NoiseGrid& gD) {
    Vector3 l = {dir.x, -dir.z, dir.y};               // marco del mesh
    float u  = acosf(Clamp(l.z, -1.0f, 1.0f)) / PI;   // latitud 0(norte)..1(sur)
    float w  = (atan2f(l.y, l.x) / PI + 1.0f) * 0.5f; // longitud 0..1
    float h  = HeightAt((int)(u * texW), (int)(w * texH), gA, gB, gC, gD);
    return sphereR * (1.0f + h * 0.01f);              // 1% de amplitud del relieve
}

// Radio REAL (unidades locales, radio ≈ 1) en la dirección `dir` del centro,
// probando contra los triángulos de la malla desplazada con ray-triangle.
// Devuelve lo que GroundRadius aproxima analíticamente, pero exacto para la
// geometría que se dibuja. Coste O(triángulos) → solo para una tecla (F).
float MeshGroundRadiusLocal(const Mesh& mesh, Vector3 dir) {
    Ray ray = { (Vector3){0, 0, 0}, dir };
    float best = 1e9f;
    for (int t = 0; t < mesh.triangleCount; ++t) {
        int i0 = 3 * t, i1 = 3 * t + 1, i2 = 3 * t + 2;
        Vector3 A = { mesh.vertices[i0*3+0], mesh.vertices[i0*3+1], mesh.vertices[i0*3+2] };
        Vector3 B = { mesh.vertices[i1*3+0], mesh.vertices[i1*3+1], mesh.vertices[i1*3+2] };
        Vector3 C = { mesh.vertices[i2*3+0], mesh.vertices[i2*3+1], mesh.vertices[i2*3+2] };
        RayCollision c = GetRayCollisionTriangle(ray, A, B, C);
        if (c.hit && c.distance > 0.0f && c.distance < best) best = c.distance;
    }
    return best;
}

int main() {
    // ══ CONFIGURACIÓN ═══════════════════════════════════════════════════════
    const int   screenWidth  = 1800;
    const int   screenHeight = 960;
    const float EYE_HEIGHT   = 0.0017f;   // 1.7 m (la unidad es 1 km)
    const int   TEX_W = 2048, TEX_H = 1024;

    // Escala: 1 unidad = 1 km (radio tipo Tierra)
    const float sphereR   = 6000.0f;
    Vector3     spherePos = {0, 0, 0};

    // Grosor de cada shell respecto al radio del planeta (de dentro afuera)
    const float CLOUD_SHELL = 1.03f;
    const float ATMO_SHELL  = 1.06f;
    float       atmoPower   = 2.0f;      // foco del halo (menor = más ancho)

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
    unsigned int cloudSeed  = 24;
    double cloudUmbral = 0.5;            // > umbral = nube
    double cloudDureza = 0.1;            // margen del smoothstep (mayor = borde más suave)

    // ══ RUIDO ═══════════════════════════════════════════════════════════════
    // Terreno: 4 octavas, de baja a alta frecuencia
    struct NoiseGrid gA = MakeNoiseGrid(256, TEX_W, TEX_H, planetSeed);
    struct NoiseGrid gB = MakeNoiseGrid(128, TEX_W, TEX_H, planetSeed);
    struct NoiseGrid gC = MakeNoiseGrid(64,  TEX_W, TEX_H, planetSeed);
    struct NoiseGrid gD = MakeNoiseGrid(32,  TEX_W, TEX_H, planetSeed);

    // Nubes: fBm de alta frecuencia (detalle, no geografía)
    struct NoiseGrid gE = MakeNoiseGrid(64, TEX_W, TEX_H, cloudSeed);
    struct NoiseGrid gF = MakeNoiseGrid(32, TEX_W, TEX_H, cloudSeed);
    struct NoiseGrid gG = MakeNoiseGrid(16, TEX_W, TEX_H, cloudSeed);

    InitWindow(screenWidth, screenHeight, "Proto 3D - sandbox camara orbital");
    SetTargetFPS(60);

    // ══ GEOMETRÍA: esfera del planeta con relieve ═══════════════════════════
    Mesh sphereMesh = GenMeshSphere(1.0f, 256, 128);

    for (int v = 0; v < sphereMesh.vertexCount; ++v) {
        // dirección: el vértice de radio 1 ES la dirección (ya normalizada por la malla)
        Vector3 dir = { sphereMesh.vertices[v*3+0], sphereMesh.vertices[v*3+1], sphereMesh.vertices[v*3+2] };
        // (GenMeshSphere de par_shapes da puntos ya unitarios → no hace falta normalizar)

        // altura desde el texcoord real del vértice:
        float u = sphereMesh.texcoords[v*2 + 0];      // latitud
        float w = sphereMesh.texcoords[v*2 + 1];      // longitud
        float h = HeightAt((int)(u * TEX_W), (int)(w * TEX_H), gA, gB, gC, gD);   // 0..1

        float r = 1.0f + h * 0.01f;
        sphereMesh.vertices[v*3+0] = dir.x * r;
        sphereMesh.vertices[v*3+1] = dir.y * r;
        sphereMesh.vertices[v*3+2] = dir.z * r;
    }

    UpdateMeshBuffer(sphereMesh, 0, sphereMesh.vertices,
                 sphereMesh.vertexCount * 3 * sizeof(float), 0);

    for (int t = 0; t < sphereMesh.triangleCount; ++t) {
        int i0 = 3*t, i1 = 3*t+1, i2 = 3*t+2;
        Vector3 A = {sphereMesh.vertices[i0*3], sphereMesh.vertices[i0*3+1], sphereMesh.vertices[i0*3+2]};
        Vector3 B = {sphereMesh.vertices[i1*3], sphereMesh.vertices[i1*3+1], sphereMesh.vertices[i1*3+2]};
        Vector3 C = {sphereMesh.vertices[i2*3], sphereMesh.vertices[i2*3+1], sphereMesh.vertices[i2*3+2]};
        Vector3 N = Vector3CrossProduct(Vector3Subtract(B, A), Vector3Subtract(C, A));

        // orientación correcta garantizada: que apunte hacia fuera
        if (Vector3DotProduct(N, A) < 0) N = Vector3Negate(N);   // A es el radial (esfera en origen)
        N = Vector3Normalize(N);

        sphereMesh.normals[i0*3] = sphereMesh.normals[i1*3] = sphereMesh.normals[i2*3] = N.x;
        sphereMesh.normals[i0*3+1] = sphereMesh.normals[i1*3+1] = sphereMesh.normals[i2*3+1] = N.y;
        sphereMesh.normals[i0*3+2] = sphereMesh.normals[i1*3+2] = sphereMesh.normals[i2*3+2] = N.z;
    }
    UpdateMeshBuffer(sphereMesh, 2, sphereMesh.normals, sphereMesh.vertexCount*3*sizeof(float), 0);

    // ══ TEXTURAS ════════════════════════════════════════════════════════════
    // Terreno: HeightAt → paleta de biomas. Ojo: x es LATITUD e y es LONGITUD
    // (UVs transpuestas de GenMeshSphere), por eso el hielo polar va por x.
    Image terrainImg = GenImageColor(TEX_W, TEX_H, (Color){0, 0, 0, 255});
    for (int y = 0; y < terrainImg.height; ++y) {
        for (int x = 0; x < terrainImg.width; ++x) {
            double x01 = HeightAt(x, y, gA, gB, gC, gD);   // 0..1
            double lat = (double)x / terrainImg.width;     // latitud (eje X)
            Color c = (lat < 0.03 || lat > 0.97)
                    ? (Color){240, 244, 250, 255}          // casquetes polares
                    : HeightColor(x01);
            ImageDrawPixel(&terrainImg, x, y, c);
        }
    }
    Texture2D terrainTex = LoadTextureFromImage(terrainImg);
    SetTextureWrap(terrainTex, TEXTURE_WRAP_REPEAT);      // el terreno da la vuelta

    // Nubes: solo el canal ALPHA importa (cobertura). ExportImage → nubes.png
    // sirve para iterar el patrón sin abrir el juego.
    Image cloudImg = GenImageColor(TEX_W, TEX_H, (Color){0, 0, 0, 0});
    for (int y = 0; y < cloudImg.height; ++y) {
        for (int x = 0; x < cloudImg.width; ++x) {
            float a = CloudAt(x, y, gE, gF, gG, cloudUmbral, cloudDureza);
            ImageDrawPixel(&cloudImg, x, y, (Color){255, 255, 255, (unsigned char)(a*255)});
        }
    }
    Texture2D cloudTex = LoadTextureFromImage(cloudImg);
    SetTextureWrap(cloudTex, TEXTURE_WRAP_REPEAT);
    ExportImage(cloudImg, "nubes.png");

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
    Model sphereModel = LoadModelFromMesh(sphereMesh);
    sphereModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = terrainTex;
    sphereModel.materials[0].shader = terrenoShader;

    // Shell de nubes: sin relieve, solo la textura proyectada
    Model cloudModel = LoadModelFromMesh(GenMeshSphere(1.0f, 128, 64));
    cloudModel.materials[0].shader = cloudShader;
    cloudModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = cloudTex;

    // Shell atmosférico: baja resolución (es un degradado suave)
    Model atmoModel = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 32));
    atmoModel.materials[0].shader = atmoShader;

    // ══ CÁMARA FPS ══════════════════════════════════════════════════════════
    Camera3D camera = {0};
    camera.position = {sphereR * 3.0f, sphereR * 2.5f, sphereR * 2.0f};
    camera.target = {0, 0, 0};
    camera.up = {0, 1, 0};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // yaw/pitch derivados de la vista inicial, editados por el ratón
    Vector3 startDir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    float yaw   = atan2f(startDir.x, startDir.z);
    float pitch = asinf(startDir.y);
    float moveSpeed = 150.0f;                       // km/s base

    // Anillo orbital de referencia (aún no se dibuja)
    float orbitR = sphereR * 1.6f;
    std::vector<Vector3> ring(64);
    for (int i = 0; i < 64; ++i) {
        float a = 2.0f * PI * i / 64.0f;
        ring[i] = {orbitR * std::cos(a), 0.0f, orbitR * std::sin(a)};
    }

    DisableCursor();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        rotAngle   += rot_speed * dt;
        cloudAngle += cloud_speed * dt;
        sunAngle   += sun_speed * dt;

        // ── Cámara FPS: rotación ──
        // Ratón bloqueado al centro → delta sin tocar bordes
        Vector2 md = GetMouseDelta();
        yaw   -= md.x * 0.0025f;
        pitch -= md.y * 0.0025f;
        if (pitch >  1.55f) pitch =  1.55f;      // ±~89° (no girar de cabeza)
        if (pitch < -1.55f) pitch = -1.55f;
        float cp = cosf(pitch), sp = sinf(pitch);
        Vector3 fwd = {cp * sinf(yaw), sp, cp * cosf(yaw)};   // hacia dónde mira

        // ── Cámara FPS: movimiento ──
        // Rueda: velocidad escalada al radio del planeta
        moveSpeed *= 1.0f + GetMouseWheelMove() * 0.5f;
        if (moveSpeed < 1.0f) moveSpeed = 1.0f;
        if (moveSpeed > sphereR * 20.0f) moveSpeed = sphereR * 20.0f;
        float step = moveSpeed * dt;

        // WASD relativo a la vista + Q/E bajar/subir
        Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, {0, 1, 0}));
        if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(fwd, step));
        if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(fwd, step));
        if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, step));
        if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, step));
        if (IsKeyDown(KEY_E)) camera.position.y += step;
        if (IsKeyDown(KEY_Q)) camera.position.y -= step;

        camera.target = Vector3Add(camera.position, fwd);
        camera.up = {0, 1, 0};

        // ── Colisión: no dejar la cámara dentro del planeta ──
        {
            Vector3 rel  = Vector3Subtract(camera.position, spherePos);
            Vector3 relN = Vector3Normalize(rel);
            float ground = GroundRadius(relN, sphereR, TEX_W, TEX_H, gA, gB, gC, gD);
            if (Vector3Length(rel) < ground + EYE_HEIGHT)
                camera.position = Vector3Add(spherePos, Vector3Scale(relN, ground + EYE_HEIGHT));
        }

        // ── Orientaciones: el -90° X alinea la malla (polo norte en +Z) ──
        Quaternion qOrient    = QuaternionFromAxisAngle({1, 0, 0}, -90.0f * DEG2RAD);
        Quaternion qSpin      = QuaternionFromAxisAngle({0, 1, 0},  rotAngle);
        Quaternion qCloudSpin = QuaternionFromAxisAngle({0, 1, 0},  cloudAngle);
        Quaternion q          = QuaternionMultiply(qSpin, qOrient);
        Quaternion qCloud     = QuaternionMultiply(qCloudSpin, qOrient);

        // ── F: aterrizar en la superficie y mirar al horizonte ──
        if (IsKeyPressed(KEY_F)) {
            Vector3 up  = Vector3Normalize(Vector3Subtract(camera.position, spherePos));
            Vector3 fwdH = Vector3Normalize(Vector3CrossProduct(up, {0, 0, 1}));   // tangente
            if (Vector3Length(fwdH) < 1e-4f)
                fwdH = Vector3Normalize(Vector3CrossProduct(up, {0, 1, 0}));

            // Colisión EXACTA contra la malla real (ray-triangle, O(triángulos)):
            // la dirección se lleva al marco local (rotación inversa del render).
            Vector3 upLocal = Vector3Normalize(Vector3RotateByQuaternion(up, QuaternionInvert(q)));
            float ground = MeshGroundRadiusLocal(sphereMesh, upLocal) * sphereR;

            camera.position = Vector3Add(spherePos, Vector3Scale(up, ground + EYE_HEIGHT));
            yaw   = atan2f(fwdH.x, fwdH.z);
            pitch = 0.0f;
        }

        // Altitud sobre el nivel del mar (km): el relieve solo sube (r >= R), así que
        // dist - R da la elevación del terreno y crece al volar.
        float altitude = Vector3Length(Vector3Subtract(camera.position, spherePos)) - sphereR;
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
        SetShaderValue(atmoShader,   camPosLoc,    &camera.position, SHADER_UNIFORM_VEC3);

        BeginDrawing();
        ClearBackground({0, 0, 20, 255});

        // Near adaptativo: 0.001 pegado al suelo (precisión al caminar),
        // 0.1 en órbita (el z-buffer se estira y gana resolución).
        float distCam   = Vector3Length(Vector3Subtract(camera.position, spherePos));
        float nearPlane = (distCam < sphereR * 1.1f) ? 0.001f : 0.1f;
        rlSetClipPlanes(nearPlane, 200000.0f);   // planos GLOBALES de raylib

        BeginMode3D(camera);

        // ── DIBUJO: de dentro afuera, cada shell sin test de profundidad ──
        // Sin depth test porque las capas están separadas por ~km y el z-buffer
        // no da esa precisión a distancia de órbita (evita z-fighting).
        Vector3 axis, cloudAxis; float angle, cloudAngleRad;
        QuaternionToAxisAngle(QuaternionNormalize(q),      &axis,       &angle);
        QuaternionToAxisAngle(QuaternionNormalize(qCloud), &cloudAxis,  &cloudAngleRad);

        // 1) Terreno: opaco, con test de profundidad normal
        DrawModelEx(sphereModel, spherePos, axis, angle * RAD2DEG,
            (Vector3){sphereR, sphereR, sphereR}, WHITE);   // WHITE = no sobretiñe

        // 2) Nubes: BLEND_ALPHA (tapan lo de detrás) + sin depth test
        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthTest();
        DrawModelEx(cloudModel, spherePos, cloudAxis, cloudAngleRad * RAD2DEG,
            (Vector3){ sphereR*CLOUD_SHELL, sphereR*CLOUD_SHELL, sphereR*CLOUD_SHELL }, WHITE);
        rlEnableDepthTest();
        EndBlendMode();

        // 3) Atmósfera: BLEND_ADDITIVE (suma luz) + sin depth test
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthTest();
        DrawModelEx(atmoModel, spherePos, axis, angle * RAD2DEG,
            (Vector3){ sphereR*ATMO_SHELL, sphereR*ATMO_SHELL, sphereR*ATMO_SHELL }, WHITE);
        rlEnableDepthTest();
        EndBlendMode();

        EndMode3D();

        // UI
        DrawText("WASD mover | Raton mirar | Q/E altura | F: superficie | Rueda velocidad", 20, 20, 20, RAYWHITE);
        DrawText(TextFormat("Radio: %d km | Altitud: %.1f km | Vel: %.0f km/s",
                  (int)sphereR, altitude, moveSpeed), 20, 44, 20, SKYBLUE);
        EndDrawing();
    }

    CloseWindow();
    EnableCursor();
    return 0;
}