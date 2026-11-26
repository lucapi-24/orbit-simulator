#include <raylib.h>
#include <rlgl.h>       // rlSetClipPlanes
#include <raymath.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include "CameraRig.hpp"
#include "IcoSphere.hpp"
#include "Noise3D.hpp"
#include "PlanetMesh.hpp"
#include "PlanetNoise.hpp"
#include "PlanetVisual.hpp"



int main() {
    // ══ CONFIGURACIÓN ═══════════════════════════════════════════════════════
    const int   screenWidth  = 1800;
    const int   screenHeight = 960;
    const int   TEX_W = 2048, TEX_H = 1024;

    // Escala: 1 unidad = 1 km (radio tipo Tierra)
    const float sphereR   = 6000.0f;
    Vector3     spherePos = {0, 0, 0};
    const int ICO_DEPTH = 7; // profundidad de subdivisión de la icosfera

    // Luna
    const float moonR   = 1737.0f;
    Vector3     moonPos = {20000.0f, 0, 0};
    const int MOON_ICO_DEPTH = 6;
    const int PATCH_MAIN_DIVISIONS = 64;
    const int PATCH_NEIGHBOUR_DIVISIONS = 16;

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
    Shader lunaShader    = LoadShader("proto/3d/shaders/luna.vs", "proto/3d/shaders/luna.fs");
    // Uniforms: se resuelven UNA vez (buscar por nombre cada frame es desperdicio)
    int sunDirLoc     = GetShaderLocation(terrenoShader, "sunDir");
    int ambientLoc    = GetShaderLocation(terrenoShader, "ambient");
    int cloudSunLoc   = GetShaderLocation(cloudShader,  "sunDir");
    int cloudAmbLoc   = GetShaderLocation(cloudShader,  "ambient");
    int atmoSunLoc    = GetShaderLocation(atmoShader,   "sunDir");
    int atmoPowerLoc  = GetShaderLocation(atmoShader,   "power");
    int camPosLoc     = GetShaderLocation(atmoShader,   "camPos");
    int lunaSunDirLoc = GetShaderLocation(lunaShader,   "sunDir");
    int lunaAmbientLoc= GetShaderLocation(lunaShader,   "ambient");


    PlanetVisual planetVisual = BuildPlanetVisual(ICO_DEPTH, terrenoShader, TerrainType::Earth);
    PlanetVisual moonVisual = BuildPlanetVisual(MOON_ICO_DEPTH, lunaShader, TerrainType::Moon);
    const std::vector<Crater> moonCraters = GenerateCrateres();
    // ══ MODELOS ═════════════════════════════════════════════════════════════
    
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

    TerrainType cameraBody = TerrainType::Earth;
    Vector3 renderOrigin = spherePos;
    CameraEnv env;


    std::vector<std::pair<PatchTriangle, Model>> parches;
    std::vector<std::pair<int, Model>> carasParche;
    bool showParche = false;
    int caraParche = -1;
    int triangleParche = -1;

    auto unloadPatchFaces = [](std::vector<std::pair<int, Model>>& faces) {
        for (auto& face : faces) UnloadModel(face.second);
        faces.clear();
    };
    auto unloadPatches = [](std::vector<std::pair<PatchTriangle, Model>>& patches) {
        for (auto& patch : patches) UnloadModel(patch.second);
        patches.clear();
    };

    auto groundLocal = [&](Vector3 dir) {
    if (cameraBody == TerrainType::Moon && showParche) {
        bestHit global = MeshGroundHitLocal(moonVisual.planetGeom.icoFaces,
                                            moonVisual.planetGeom.faceCenters, dir);
        if (!global.hit) throw std::runtime_error("No se encontro la superficie lunar");
        for (const auto& patch : parches) {
            if (global.faceIndex != patch.first.faceIndex ||
                global.triangleIndex != patch.first.triangleIndex) continue;
            bestHit local = MeshHitLocal(patch.second.meshes[0], dir);
            if (!local.hit) local = MeshHitLocal(patch.second.meshes[0], dir, true);
            if (local.hit) return local.distance * env.sphereR;
            TraceLog(LOG_WARNING, "No se encontro la colision del parche lunar; se usa la malla global.");
            break;
        }
        return global.distance * env.sphereR;
    }

    const PlanetGeometry& geometry = cameraBody == TerrainType::Earth
        ? planetVisual.planetGeom : moonVisual.planetGeom;
    return MeshGroundRadiusLocal(geometry.icoFaces, geometry.faceCenters, dir) * env.sphereR;
    };

    // Los callbacks capturan por referencia: `main` conoce el terreno y se lo
    // presta a la cámara sin que CameraRig.hpp tenga que incluirlo.
    env.groundApprox = [&](Vector3 dir) {
    Vector3 localDir = Vector3RotateByQuaternion(dir, QuaternionInvert(env.spin));
    return groundLocal(localDir);
    };

    env.groundExact = groundLocal;


    // Punto de prueba (tecla P): cima más alta de la malla, vista desde P_VIEW_DIST_KM
    Vector3 peakLocal = {0, 0, 1};
    float peakRadius = 0.0f;   // radio de la cima en unidades de malla
    for (int v = 0; v < planetVisual.planetGeom.sphereMesh.vertexCount; ++v) {
        Vector3 p = { planetVisual.planetGeom.sphereMesh.vertices[v*3], planetVisual.planetGeom.sphereMesh.vertices[v*3+1], planetVisual.planetGeom.sphereMesh.vertices[v*3+2] };
        if (Vector3Length(p) > peakRadius) { peakRadius = Vector3Length(p); peakLocal = p; }
    }
    peakLocal = Vector3Normalize(peakLocal);

    auto crearParche = [&](const bestHit& hit) {
        if (hit.hit){
            const std::vector<Mesh>& faces = moonVisual.planetGeom.icoFaces;
            const std::vector<PatchFace> patchFaces = FindPatchFaces(faces, hit.faceIndex, hit.triangleIndex);
            auto freeMeshData = [](Mesh* mesh) {
                MemFree(mesh->vertices);
                MemFree(mesh->normals);
                MemFree(mesh->texcoords);
                MemFree(mesh->indices);
            };
            std::vector<PatchTriangle> patchTriangles;
            patchTriangles.push_back({hit.faceIndex, hit.triangleIndex, PATCH_MAIN_DIVISIONS});
            for (const PatchFace& patchFace : patchFaces) {
                for (const PatchFaceEdge& edge : patchFace.edges) {
                    const auto existing = std::find_if(
                        patchTriangles.begin(), patchTriangles.end(),
                        [&](const PatchTriangle& patch) {
                            return patch.faceIndex == patchFace.faceIndex &&
                                   patch.triangleIndex == edge.triangleIndex;
                        });
                    if (existing == patchTriangles.end()) {
                        patchTriangles.push_back({
                            patchFace.faceIndex, edge.triangleIndex, PATCH_NEIGHBOUR_DIVISIONS
                        });
                    }
                }
            }

            auto indicesBorde = [&](int edge, int divisions) {
                if(edge == 0){
                    return PatchEdgeABIndices(divisions);
                }else if(edge == 1){
                    return PatchEdgeBCIndices(divisions);
                }else if(edge == 2){
                    return PatchEdgeCAIndices(divisions);
                }else{
                    throw std::runtime_error("Invalid edge index");
                }
            };

            if (PATCH_MAIN_DIVISIONS % PATCH_NEIGHBOUR_DIVISIONS != 0) {
                throw std::runtime_error("Patch resolutions must be evenly divisible for edge comparison");
            }
            const size_t bordeStride =
                static_cast<size_t>(PATCH_MAIN_DIVISIONS / PATCH_NEIGHBOUR_DIVISIONS);
            std::vector<Mesh> patchMeshStorage;
            std::vector<std::unique_ptr<Mesh, decltype(freeMeshData)>> patchMeshGuards;
            std::vector<const Mesh*> patchMeshes;
            std::vector<int> patchDivisions;
            patchMeshStorage.reserve(patchTriangles.size());
            patchMeshGuards.reserve(patchTriangles.size());
            patchMeshes.reserve(patchTriangles.size());
            patchDivisions.reserve(patchTriangles.size());
            for (const PatchTriangle& patch : patchTriangles) {
                patchMeshStorage.push_back(BuildLocalTriangleMesh(
                    faces[patch.faceIndex], patch.triangleIndex, patch.divisions));
                patchMeshGuards.emplace_back(&patchMeshStorage.back(), freeMeshData);
                patchMeshes.push_back(&patchMeshStorage.back());
                patchDivisions.push_back(patch.divisions);
            }

            float maxDistancia = 0.0f;
            int verticesDistintos = 0;
            for (const PatchFace& patchFace : patchFaces) {
                for (const PatchFaceEdge& edge : patchFace.edges) {
                    const auto neighbourPatch = std::find_if(
                        patchTriangles.begin() + 1, patchTriangles.end(),
                        [&](const PatchTriangle& patch) {
                            return patch.faceIndex == patchFace.faceIndex &&
                                   patch.triangleIndex == edge.triangleIndex;
                        });
                    if (neighbourPatch == patchTriangles.end()) {
                        throw std::runtime_error("Neighbour triangle is missing from patch ring");
                    }
                    const size_t neighbourPatchIndex =
                        static_cast<size_t>(std::distance(patchTriangles.begin(), neighbourPatch));

                    int neighbourEdge = -1;
                    const Mesh& neighbourFace = faces[patchFace.faceIndex];
                    for (int i = 0; i < 3; ++i) {
                        int a = neighbourFace.indices[edge.triangleIndex * 3 + i];
                        int b = neighbourFace.indices[edge.triangleIndex * 3 + (i + 1) % 3];
                        if (a == edge.edgeB && b == edge.edgeA) {
                            neighbourEdge = i;
                            break;
                        }
                    }
                    if (neighbourEdge == -1) {
                        throw std::runtime_error("No neighbour edge found for the patch face");
                    }

                    const std::vector<int> mainEdge = indicesBorde(edge.patchEdge, PATCH_MAIN_DIVISIONS);
                    const std::vector<int> neighbourEdgeIndices =
                        indicesBorde(neighbourEdge, PATCH_NEIGHBOUR_DIVISIONS);
                    const Mesh& mainMesh = patchMeshStorage[0];
                    const Mesh& neighbourMesh = patchMeshStorage[neighbourPatchIndex];
                    for (size_t k = 0; k < neighbourEdgeIndices.size(); ++k) {
                        int a = mainEdge[k * bordeStride];
                        int b = neighbourEdgeIndices[neighbourEdgeIndices.size() - 1 - k];

                        Vector3 posA = {
                            mainMesh.vertices[a * 3], mainMesh.vertices[a * 3 + 1], mainMesh.vertices[a * 3 + 2]
                        };
                        Vector3 posB = {
                            neighbourMesh.vertices[b * 3],
                            neighbourMesh.vertices[b * 3 + 1],
                            neighbourMesh.vertices[b * 3 + 2]
                        };

                        if (posA.x != posB.x || posA.y != posB.y || posA.z != posB.z) ++verticesDistintos;
                        float distancia = Vector3Distance(posA, posB) * moonR * 1000.0f;
                        if (distancia > maxDistancia) maxDistancia = distancia;
                    }
                }
            }
            TraceLog(LOG_INFO, "Diferencia maxima entre bordes: %.4f metros", maxDistancia);
            TraceLog(LOG_INFO, "Vertices distintos entre bordes: %d", verticesDistintos);


            std::vector<std::vector<int>> huecosPorCara(faces.size());

            for (const PatchTriangle& triangulo : patchTriangles) {
                huecosPorCara[triangulo.faceIndex].push_back(triangulo.triangleIndex);
            }

            std::vector<std::vector<PatchStitch>> bordesPorCara(faces.size());
            auto recogerBordes = [&](const std::vector<PatchFace>& patchFaces, int patchIndex) {
                for (const PatchFace& patchFace : patchFaces) {
                    for (const PatchFaceEdge& edge : patchFace.edges) {
                        const std::vector<int>& huecos = huecosPorCara[patchFace.faceIndex];
                        if (std::find(huecos.begin(), huecos.end(), edge.triangleIndex) == huecos.end()) {
                            bordesPorCara[patchFace.faceIndex].push_back({edge, patchIndex, patchTriangles[patchIndex].divisions});
                        }
                    }
                }
            };
            for (size_t patchIndex = 0; patchIndex < patchTriangles.size(); ++patchIndex) {
                const PatchTriangle& patch = patchTriangles[patchIndex];
                const std::vector<PatchFace> patchFaceEdges =
                    FindPatchFaces(faces, patch.faceIndex, patch.triangleIndex);
                recogerBordes(patchFaceEdges, static_cast<int>(patchIndex));
            }

            std::vector<PatchFaceBuild> carasAReconstruir;

            for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                if (huecosPorCara[faceIndex].empty() && bordesPorCara[faceIndex].empty()) {
                    continue;
                }

                carasAReconstruir.push_back({
                    static_cast<int>(faceIndex),
                    huecosPorCara[faceIndex],
                    bordesPorCara[faceIndex]
                });
            }

            std::vector<Mesh> meshes;
            std::vector<std::unique_ptr<Mesh, decltype(freeMeshData)>> meshGuards;
            std::vector<std::pair<int, Model>> nuevasCaras;
            meshes.reserve(carasAReconstruir.size());
            meshGuards.reserve(carasAReconstruir.size());
            nuevasCaras.reserve(carasAReconstruir.size());
            for (const PatchFaceBuild& patchFace : carasAReconstruir) {
                meshes.push_back(BuildStitchedFace(faces[patchFace.faceIndex], patchFace,
                                                   patchMeshes, patchDivisions));
                meshGuards.emplace_back(&meshes.back(), freeMeshData);
            }

            std::vector<std::pair<PatchTriangle, Model>> nuevosParches;
            nuevosParches.reserve(patchTriangles.size());
            for (size_t i = 0; i < patchMeshStorage.size(); ++i) {
                UploadMesh(&patchMeshStorage[i], false);
                Model patchModel = LoadModelFromMesh(patchMeshStorage[i]);
                patchMeshGuards[i].release();
                patchModel.materials[0].shader = lunaShader;
                nuevosParches.emplace_back(patchTriangles[i], patchModel);
            }

            for (size_t i = 0; i < meshes.size(); ++i) {
                UploadMesh(&meshes[i], false);
                Model cara = LoadModelFromMesh(meshes[i]);
                meshGuards[i].release();
                cara.materials[0].shader = lunaShader;
                nuevasCaras.emplace_back(carasAReconstruir[i].faceIndex, cara);
            }
            
            if (showParche) {
                unloadPatches(parches);
                unloadPatchFaces(carasParche);
            }

            parches = std::move(nuevosParches);
            carasParche = std::move(nuevasCaras);
            caraParche = hit.faceIndex;
            triangleParche = hit.triangleIndex;
            showParche = true;
        }
        

    };

    auto moonHitInDirection = [&](Vector3 worldDir) {
        Vector3 localDir = Vector3RotateByQuaternion(worldDir, QuaternionInvert(env.spin));
        return MeshGroundHitLocal(moonVisual.planetGeom.icoFaces,
                                  moonVisual.planetGeom.faceCenters, localDir);
    };

    env.prepareGround = [&](Vector3 worldDir) {
        if (!showParche || cameraBody != TerrainType::Moon) return;
        bestHit hit = moonHitInDirection(worldDir);
        if (!hit.hit) throw std::runtime_error("No se encontro el triangulo lunar bajo la camara");
        const bool covered = std::any_of(parches.begin(), parches.end(), [&](const auto& patch) {
            return patch.first.faceIndex == hit.faceIndex &&
                   patch.first.triangleIndex == hit.triangleIndex;
        });
        if (!covered)
            crearParche(hit);
    };

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

        bool goToPeak = IsKeyPressed(KEY_P);
        bool goToCrater = IsKeyPressed(KEY_L);
        if (goToCrater && moonCraters.empty()) {
            TraceLog(LOG_WARNING, "No lunar craters available for the camera");
            goToCrater = false;
        }
        TerrainType nextBody = cameraBody;
        if (goToPeak) nextBody = TerrainType::Earth;
        else if (goToCrater) nextBody = TerrainType::Moon;
        else if (rig.mode == CamMode::FLY && IsKeyPressed(KEY_M))
            nextBody = cameraBody == TerrainType::Earth ? TerrainType::Moon : TerrainType::Earth;

        Vector3 nextOrigin = nextBody == TerrainType::Earth ? spherePos : moonPos;
        Vector3 originShift = Vector3Subtract(renderOrigin, nextOrigin);
        rig.cam.position = Vector3Add(rig.cam.position, originShift);
        rig.cam.target = Vector3Add(rig.cam.target, originShift);
        renderOrigin = nextOrigin;
        cameraBody = nextBody;
        Vector3 earthRenderPos = Vector3Subtract(spherePos, renderOrigin);
        Vector3 moonRenderPos = Vector3Subtract(moonPos, renderOrigin);

        env.spherePos = cameraBody == TerrainType::Earth ? earthRenderPos : moonRenderPos;
        env.sphereR = cameraBody == TerrainType::Earth ? sphereR : moonR;
        env.spin = q;
        env.gravity = cameraBody == TerrainType::Earth ? 9.81e-3f : 1.62e-3f;

        if (goToPeak) {
            // Cima en mundo, con el planeta rotando en este instante
            Vector3 up = Vector3RotateByQuaternion(peakLocal, q);
            Vector3 peakWorld = Vector3Add(earthRenderPos, Vector3Scale(up, peakRadius * sphereR));
            Vector3 side = Vector3Normalize(Vector3CrossProduct(up, {0, 1, 0}));
            Vector3 viewPos = Vector3Add(Vector3Add(peakWorld, Vector3Scale(side, -P_VIEW_DIST_KM)), Vector3Scale(up, CameraRig::EYE_HEIGHT));
            rig.cam.position = viewPos;
            rig.EnterGround(env, Vector3Subtract(peakWorld, viewPos));
        } else if (goToCrater) {
            const Crater& crater = moonCraters.front();
            Vector3 reference = fabsf(crater.centro.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
            Vector3 tangent = Vector3Normalize(Vector3CrossProduct(reference, crater.centro));
            Vector3 rimDir = Vector3Normalize(Vector3Add(crater.centro, Vector3Scale(tangent, crater.radio * 0.9f)));
            Vector3 rimUp = Vector3RotateByQuaternion(rimDir, q);
            Vector3 centerUp = Vector3RotateByQuaternion(crater.centro, q);
            if (env.prepareGround) env.prepareGround(rimUp);
            float rimRadius = static_cast<float>(env.groundExact(rimDir));
            float centerRadius = static_cast<float>(env.groundExact(crater.centro));
            Vector3 craterCenter = Vector3Add(moonRenderPos, Vector3Scale(centerUp, centerRadius));
            rig.cam.position = Vector3Add(moonRenderPos, Vector3Scale(rimUp, rimRadius + CameraRig::EYE_HEIGHT));
            rig.EnterGround(env, Vector3Subtract(craterCenter, rig.cam.position));
        }

        if (IsKeyPressed(KEY_K)) shellTest = (shellTest + 1) % 4;
        bool drawClouds = shellTest != 2 && shellTest != 3;
        bool drawAtmo   = shellTest != 1 && shellTest != 3;

        if (IsKeyPressed(KEY_T)){
            if (showParche){
                unloadPatches(parches);
                unloadPatchFaces(carasParche);
                showParche = false;
                caraParche = -1;
                triangleParche = -1;
            }else{
                bestHit hit = {false, 0.0, -1, -1};
                if (cameraBody == TerrainType::Moon) {
                    Vector3 dir = Vector3Normalize(Vector3Subtract(rig.cam.position, moonRenderPos));
                    hit = moonHitInDirection(dir);
                }
                if (hit.hit) crearParche(hit);
                else TraceLog(LOG_WARNING, "No se ha encontrado el impacto en la superficie lunar.");
            }
        }

        rig.Update(dt, env);

        bestHit moonHit = {false, 0.0, -1, -1};
        if (cameraBody == TerrainType::Moon && !showParche) {
            Vector3 dir = Vector3Normalize(Vector3Subtract(rig.cam.position, moonRenderPos));
            moonHit = moonHitInDirection(dir);
        }
        float altitude = Vector3Distance(rig.cam.position, env.spherePos) - env.sphereR;

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

        SetShaderValue(lunaShader, lunaSunDirLoc,   &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(lunaShader, lunaAmbientLoc,  &ambient, SHADER_UNIFORM_FLOAT);

        BeginDrawing();
        ClearBackground({0, 0, 20, 255});

        // Near menor en superficie para no recortar el terreno cercano.
        float distCam = Vector3Distance(rig.cam.position, env.spherePos);
        float distTierra = Vector3Distance(rig.cam.position, earthRenderPos) - sphereR;
        float distLuna = Vector3Distance(rig.cam.position, moonRenderPos) - moonR;
        float superficieCercana = fminf(distTierra, distLuna);

        float nearPlane = rig.mode == CamMode::GROUND
            ? 0.0001f
            : Clamp(superficieCercana * 0.01f, 0.001f, 1.0f);
        rlSetClipPlanes(nearPlane, 200000.0f);   // planos GLOBALES de raylib

        BeginMode3D(rig.cam);

        

        // ── DIBUJO: terreno opaco y capas transparentes ──
        Vector3 axis, cloudAxis; float angle, cloudAngleRad;
        QuaternionToAxisAngle(QuaternionNormalize(q),      &axis,       &angle);
        QuaternionToAxisAngle(QuaternionNormalize(qCloud), &cloudAxis,  &cloudAngleRad);

        // 1) Terreno: opaco, con test de profundidad normal
        DrawPlanetVisual(planetVisual, earthRenderPos, sphereR, axis, angle);
        if (showParche) {
            for (size_t i = 0; i < moonVisual.icoFaceModels.size(); ++i) {
                const Model* face = &moonVisual.icoFaceModels[i];
                for (const auto& patchedFace : carasParche)
                    if (patchedFace.first == static_cast<int>(i)) {
                        face = &patchedFace.second;
                        break;
                    }
                DrawModelEx(*face, moonRenderPos, axis, angle * RAD2DEG,
                    Vector3{moonR, moonR, moonR}, WHITE);
            }
            for (const auto& patch : parches) {
                DrawModelEx(patch.second, moonRenderPos, axis, angle * RAD2DEG,
                    Vector3{moonR, moonR, moonR}, WHITE);
            }
        } else {
            DrawPlanetVisual(moonVisual, moonRenderPos, moonR, axis, angle);
        }
        
        if (moonHit.hit && !showParche) {
            const Mesh& face = moonVisual.planetGeom.icoFaces[moonHit.faceIndex];
            int t = moonHit.triangleIndex;
            int i0 = face.indices[3 * t];
            int i1 = face.indices[3 * t + 1];
            int i2 = face.indices[3 * t + 2];
            Vector3 a = {
                face.vertices[3 * i0],
                face.vertices[3 * i0 + 1],
                face.vertices[3 * i0 + 2]
            };
            a = Vector3Add(moonRenderPos,
                Vector3RotateByQuaternion(Vector3Scale(a, moonR), q));
            Vector3 b = {
                face.vertices[3 * i1],
                face.vertices[3 * i1 + 1],
                face.vertices[3 * i1 + 2]
            };
            b = Vector3Add(moonRenderPos,
                Vector3RotateByQuaternion(Vector3Scale(b, moonR), q));

            Vector3 c = {
                face.vertices[3 * i2],
                face.vertices[3 * i2 + 1],
                face.vertices[3 * i2 + 2]
            };
            c = Vector3Add(moonRenderPos,
                Vector3RotateByQuaternion(Vector3Scale(c, moonR), q));

            for (int k = 1; k < 4; ++k) {
                float t = k / 4.0f;
                DrawLine3D(Vector3Lerp(a, b, t), Vector3Lerp(a, c, t), GREEN);
                DrawLine3D(Vector3Lerp(b, a, t), Vector3Lerp(b, c, t), GREEN);
                DrawLine3D(Vector3Lerp(c, a, t), Vector3Lerp(c, b, t), GREEN);  
            }

            DrawLine3D(a, b, RED);
            DrawLine3D(b, c, RED);
            DrawLine3D(c, a, RED);
        }

        // Poste de referencia sobre la cima: la base está en el vértice de la malla,
        // así que queda exactamente sobre el terreno que se dibuja
        Vector3 poleUp   = Vector3RotateByQuaternion(peakLocal, q);
        Vector3 poleBase = Vector3Add(earthRenderPos, Vector3Scale(poleUp, peakRadius * sphereR));
        Vector3 poleTop  = Vector3Add(poleBase, Vector3Scale(poleUp, POLE_HEIGHT_KM));
        DrawCylinderEx(poleBase, poleTop, POLE_RADIUS_KM, POLE_RADIUS_KM, 8, RED);
        rlDrawRenderBatchActive();
        rlDisableDepthMask();

        // 2) Nubes: BLEND_ALPHA, con test de profundidad
        if (drawClouds) {
            BeginBlendMode(BLEND_ALPHA);
            DrawModelEx(cloudModel, earthRenderPos, cloudAxis, cloudAngleRad * RAD2DEG,
                (Vector3){ sphereR*CLOUD_SHELL, sphereR*CLOUD_SHELL, sphereR*CLOUD_SHELL }, WHITE);
            EndBlendMode();
        }

        // 3) Atmósfera: BLEND_ADDITIVE, con test de profundidad
        if (drawAtmo) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawModelEx(atmoModel, earthRenderPos, axis, angle * RAD2DEG,
                (Vector3){ sphereR*ATMO_SHELL, sphereR*ATMO_SHELL, sphereR*ATMO_SHELL }, WHITE);
            EndBlendMode();
        }
        rlEnableDepthMask();

        EndMode3D();

        // UI
        DrawText("WASD mover | Raton mirar | Q/E altura | F: superficie | M: cuerpo | P/L/T: pruebas", 20, 20, 20, RAYWHITE);
        if (rig.mode == CamMode::GROUND) {
            DrawText(TextFormat("%s | Radio: %d km | Altitud: %.1f km | Vel: %.1f m/s",
                      cameraBody == TerrainType::Earth ? "Tierra" : "Luna", (int)env.sphereR, altitude, rig.walkSpeed * 1000.0f), 20, 44, 20, SKYBLUE);
        } else {
            DrawText(TextFormat("%s | Radio: %d km | Altitud: %.1f km | Vel: %.0f km/s",
                      cameraBody == TerrainType::Earth ? "Tierra" : "Luna", (int)env.sphereR, altitude, rig.moveSpeed), 20, 44, 20, SKYBLUE);
        }
        DrawText(TextFormat(" Modo: %s",
                  (rig.mode == CamMode::FLY) ? "FLY" : "GROUND"), 20, 68, 20, SKYBLUE);

        // Depuración: superficie de la malla frente a la de colisión bajo la cámara
        Vector3 camDir = Vector3Normalize(Vector3Subtract(rig.cam.position, env.spherePos));
        double meshKm = env.groundExact(Vector3RotateByQuaternion(camDir, QuaternionInvert(q)));
        double collKm = env.groundApprox(camDir);
        DrawText(TextFormat(" Malla: %.4f km | Colision: %.4f km | Radio cam: %.4f km",
                  meshKm - env.sphereR, collKm - env.sphereR, distCam - env.sphereR), 20, 92, 20, LIME);
        DrawText(TextFormat(" FPS: %d | K capas: %s", GetFPS(), shellTestNames[shellTest]), 20, 116, 20, LIME);
        if (showParche) DrawText(TextFormat("Parche lunar activo | Cara: %d (T para desactivar)", caraParche),
                                 20, 140, 20, LIME);
        EndDrawing();
    }

    UnloadPlanetVisual(planetVisual);
    UnloadPlanetVisual(moonVisual);
    if (showParche) {
        unloadPatches(parches);
        unloadPatchFaces(carasParche);
    }
    CloseWindow();
    EnableCursor();
    return 0;
}
