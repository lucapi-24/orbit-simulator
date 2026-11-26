#include "PlanetMesh.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <new>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include "Noise3D.hpp"

int OctavesForDepth(int depth) {
    const float edgeKm = PLANET_R * 1.05f / (float)(1 << depth);
    int octavas = 0;
    while (octavas < FBM_OCTAVES &&
           PLANET_R / (FREC_BASE * powf(FBM_LACUN, (float)octavas)) >= 2.0f * edgeKm)
        octavas++;
    return octavas;
}

float AlturaRelieve(Vector3 dir, int octavas) {
    return RELIEVE_K * Altitud3DEx(dir, CONT_BIAS, NORMALIZACION, octavas);
}

std::vector<Crater> GenerateCrateres() {
    std::vector<Crater> crateres;

    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> zDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> radioDist(0.08f, 0.15f);
    for (int i = 0; i < 10; ++i) {
        float z = zDist(rng);
        float angle = angleDist(rng);
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        Vector3 centro = {r * std::cos(angle), z, r * std::sin(angle)};
        float radio = radioDist(rng);
        float profundidad = 75.0f * radio / 0.20f;
        float bordeAltura = profundidad * 0.20f;
        crateres.push_back({centro, profundidad, bordeAltura, radio});
    }

    return crateres;
}

float AlturaCrater(Vector3 dir, Vector3 centro, float profundidad, float bordeAltura, float radio) {
    float escala = radio/0.35f;
    float distancia = Vector3Distance(dir, centro);
    float crater = 1.0f - SmoothStep(0.45f*radio, 0.35f * escala, distancia);
    float borde = SmoothStep(0.23f * escala, 0.30f * escala, distancia)
            * (1.0f - SmoothStep(0.35f * escala, 0.42f * escala, distancia));
    return -profundidad * crater + bordeAltura * borde;
}

static float AltitudLunarBase(Vector3 dir, int octavas) {
    float altura = 0.0f;
    float frecuencia = FREC_BASE;
    float amplitud = 18.0f;
    for (int i = 0; i < octavas; ++i) {
        altura += Noise3D(Vector3Scale(dir, frecuencia)) * amplitud;
        frecuencia *= FBM_LACUN;
        amplitud *= FBM_GAIN;
    }
    return altura;
}

float AlturaRelieveLunar(Vector3 dir, int octavas) {

    static const std::vector<Crater> crateres = GenerateCrateres();

    float altura = AltitudLunarBase(dir, octavas);
    for (const Crater& crater : crateres) {
        altura += AlturaCrater(dir, crater.centro, crater.profundidad, crater.bordeAltura, crater.radio);
    }
    return altura;
}

PlanetGeometry BuildPlanetGeometry(int depth, TerrainType terrainType){
    PlanetGeometry planetGeom;
    std::vector<IcoVertex> icoVertices;
    std::vector<int> icoIndices;
    BuildIcosphere(depth, icoVertices, icoIndices);

    planetGeom.sphereMesh = BuildPlanetMesh(icoVertices, icoIndices, depth, terrainType);
    planetGeom.icoFaces = SplitByBaseFace(planetGeom.sphereMesh, icoIndices, depth);
    planetGeom.faceCenters.clear();
    for (const Mesh& face : planetGeom.icoFaces) {
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
        planetGeom.faceCenters.push_back(center);
    }

    return planetGeom;
}

Mesh BuildPlanetMesh(const std::vector<IcoVertex>& vertices, const std::vector<int>& indices, int depth, TerrainType terrainType) {
    Mesh mesh = {0};
    mesh.vertexCount = (int)vertices.size();
    mesh.triangleCount = (int)indices.size() / 3;
    mesh.vertices  = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals   = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));

    for (int i = 0; i < mesh.vertexCount; ++i) {
        mesh.vertices[i*3]   = vertices[i].x;
        mesh.vertices[i*3+1] = vertices[i].y;
        mesh.vertices[i*3+2] = vertices[i].z;
    }

    const int octavas = OctavesForDepth(depth);

    // Desplazamiento radial: la unidad de malla es PLANET_R km (1 unidad de radio = 6000 km)
    for (int v = 0; v < mesh.vertexCount; ++v) {
        Vector3 dir = { mesh.vertices[v*3], mesh.vertices[v*3+1], mesh.vertices[v*3+2] };
        float h = (terrainType == TerrainType::Moon) ? AlturaRelieveLunar(dir, octavas) : AlturaRelieve(dir, octavas);
        float r = 1.0f + h / PLANET_R;
        mesh.vertices[v*3+0] = dir.x * r;
        mesh.vertices[v*3+1] = dir.y * r;
        mesh.vertices[v*3+2] = dir.z * r;

        // La altura viaja como coordenada de textura: el shader pinta agua/tierra/hielo por píxel
        mesh.texcoords[v*2+0] = h;
        mesh.texcoords[v*2+1] = 0.0f;
    }

    // Normales: cada cara aporta su producto vectorial (ponderado por área) a sus 3 vértices
    for (int i = 0; i < mesh.vertexCount * 3; ++i) mesh.normals[i] = 0.0f;
    for (int t = 0; t < mesh.triangleCount; ++t) {
        int idx[3] = { indices[t*3], indices[t*3+1], indices[t*3+2] };
        Vector3 p[3];
        for (int k = 0; k < 3; ++k)
            p[k] = { mesh.vertices[idx[k]*3], mesh.vertices[idx[k]*3+1], mesh.vertices[idx[k]*3+2] };
        Vector3 n = Vector3CrossProduct(Vector3Subtract(p[1], p[0]), Vector3Subtract(p[2], p[0]));
        for (int k = 0; k < 3; ++k) {
            mesh.normals[idx[k]*3]   += n.x;
            mesh.normals[idx[k]*3+1] += n.y;
            mesh.normals[idx[k]*3+2] += n.z;
        }
    }
    for (int v = 0; v < mesh.vertexCount; ++v) {
        Vector3 n = Vector3Normalize({ mesh.normals[v*3], mesh.normals[v*3+1], mesh.normals[v*3+2] });
        mesh.normals[v*3]   = n.x;
        mesh.normals[v*3+1] = n.y;
        mesh.normals[v*3+2] = n.z;
    }
    return mesh;
}

int FindEdgeNeighbour(const Mesh& face, int triangleIndex, int edgeA, int edgeB) {
    for (int t = 0; t < face.triangleCount; ++t) {
        if (t == triangleIndex) continue;
        int idx[3] = { face.indices[t*3], face.indices[t*3+1], face.indices[t*3+2] };
        for (int e = 0; e < 3; ++e) {
            int a = idx[e];
            int b = idx[(e+1)%3];
            if ((a == edgeA && b == edgeB) || (a == edgeB && b == edgeA))
                return t;
        }
    }
    return -1;
}

int FindOppositeVertex(const Mesh& face, int triangleIndex, int edgeA, int edgeB) {
    int idx[3] = { face.indices[triangleIndex*3], face.indices[triangleIndex*3+1], face.indices[triangleIndex*3+2] };
    for (int i = 0; i < 3; ++i) {
        int v = idx[i];
        if (v != edgeA && v != edgeB)
            return v;
    }
    return -1;
}

std::vector<int> PatchEdgeABIndices(int divisions) {
    std::vector<int> indices;
    if (divisions <= 0) throw std::invalid_argument("Divisions must be positive");
    for (int i = 0; i <= divisions; ++i) {
        int indice = i * (divisions + 1) - i * (i - 1) / 2;
        indices.push_back(indice);
    }
    return indices;
}

std::vector<int> PatchEdgeBCIndices(int divisions) {
    std::vector<int> indices;
    if (divisions <= 0) throw std::invalid_argument("Divisions must be positive");
    for (int k = 0; k <= divisions; ++k) {
        int i = divisions - k;
        int j = k;
        int indice = i * (divisions + 1) - i * (i - 1) / 2 + j;
        indices.push_back(indice);
    }
    return indices;
}

std::vector<int> PatchEdgeCAIndices(int divisions) {
    std::vector<int> indices;
    if (divisions <= 0) throw std::invalid_argument("Divisions must be positive");
    for (int k = 0; k <= divisions; ++k) {
        int i = 0;
        int j = divisions -k;
        int indice = i * (divisions + 1) - i * (i - 1) / 2 + j;
        indices.push_back(indice);
    }
    return indices;
}

std::vector<int> BuildNeighbourFanIndices(int opposite, const std::vector<int>& edge){
    std::vector<int> fanIndices;
    for (size_t i = 0; i + 1 < edge.size(); ++i) {
        fanIndices.push_back(opposite);
        fanIndices.push_back(edge[i+1]);
        fanIndices.push_back(edge[i]);
    }
    return fanIndices;
}

std::vector<Mesh> SplitByBaseFace(const Mesh& mesh, const std::vector<int>& indices, int depth) {
    std::vector<Mesh> faces;
    const int trianglesPerFace = 1 << (2 * depth);

    for (int j = 0; j < 20; ++j) {
        std::map<int,int> localIndicesCache;     // índice global → índice local
        std::vector<int> localIndices;
        std::vector<Vector3> localPositions;
        std::vector<Vector3> localNormals;
        std::vector<Vector2> localTexcoords;

        auto localIndexOf = [&](int g) -> int {
            auto it = localIndicesCache.find(g);
            if (it != localIndicesCache.end()) return it->second;
            int local = (int)localPositions.size();
            localIndicesCache[g] = local;
            localPositions.push_back(Vector3{ mesh.vertices[g*3], mesh.vertices[g*3+1], mesh.vertices[g*3+2] });
            localNormals.push_back(Vector3{ mesh.normals[g*3], mesh.normals[g*3+1], mesh.normals[g*3+2] });
            localTexcoords.push_back(Vector2{ mesh.texcoords[g*2], mesh.texcoords[g*2+1] });
            return local;
        };

        for (int k = j * trianglesPerFace; k < (j+1) * trianglesPerFace; ++k)
            for (int c = 0; c < 3; ++c)
                localIndices.push_back(localIndexOf(indices[k*3+c]));

        Mesh localMesh = {0};
        localMesh.vertexCount = (int)localPositions.size();
        localMesh.triangleCount = (int)localIndices.size() / 3;
        localMesh.vertices  = (float*)MemAlloc(localMesh.vertexCount * 3 * sizeof(float));
        localMesh.normals   = (float*)MemAlloc(localMesh.vertexCount * 3 * sizeof(float));
        localMesh.texcoords = (float*)MemAlloc(localMesh.vertexCount * 2 * sizeof(float));
        localMesh.indices   = (unsigned short*)MemAlloc(localMesh.triangleCount * 3 * sizeof(unsigned short));

        for (int v = 0; v < localMesh.vertexCount; ++v) {
            localMesh.vertices[v*3]    = localPositions[v].x;
            localMesh.vertices[v*3+1]  = localPositions[v].y;
            localMesh.vertices[v*3+2]  = localPositions[v].z;
            localMesh.normals[v*3]     = localNormals[v].x;
            localMesh.normals[v*3+1]   = localNormals[v].y;
            localMesh.normals[v*3+2]   = localNormals[v].z;
            localMesh.texcoords[v*2]   = localTexcoords[v].x;
            localMesh.texcoords[v*2+1] = localTexcoords[v].y;
        }
        for (int i = 0; i < localMesh.triangleCount * 3; ++i)
            localMesh.indices[i] = (unsigned short)localIndices[i];

        UploadMesh(&localMesh, false);
        faces.push_back(localMesh);
    }
    return faces;
}

struct SurfaceRay {
    double x, y, z;
};

static SurfaceRay MakeSurfaceRay(Vector3 dir) {
    double length = std::sqrt(static_cast<double>(dir.x) * dir.x +
                              static_cast<double>(dir.y) * dir.y +
                              static_cast<double>(dir.z) * dir.z);
    if (!std::isfinite(length) || length == 0.0)
        throw std::invalid_argument("Invalid surface ray direction");
    return {dir.x / length, dir.y / length, dir.z / length};
}

static bool RayTriangleHitPrecise(const SurfaceRay& ray, Vector3 a, Vector3 b,
                                  Vector3 c, double& distance, bool tolerateEdges){
    double e1x = static_cast<double>(b.x) - a.x;
    double e1y = static_cast<double>(b.y) - a.y;
    double e1z = static_cast<double>(b.z) - a.z;
    double e2x = static_cast<double>(c.x) - a.x;
    double e2y = static_cast<double>(c.y) - a.y;
    double e2z = static_cast<double>(c.z) - a.z;
    double px = ray.y * e2z - ray.z * e2y;
    double py = ray.z * e2x - ray.x * e2z;
    double pz = ray.x * e2y - ray.y * e2x;
    double det = e1x * px + e1y * py + e1z * pz;
    double length1Sq = e1x*e1x + e1y*e1y + e1z*e1z;
    double length2Sq = e2x*e2x + e2y*e2y + e2z*e2z;
    double escala2 = length1Sq * length2Sq;
    if (escala2 == 0.0 || det * det <= 1e-24 * escala2) return false;
    double tolerance = 0.0;
    if (tolerateEdges) {
        double nx = e1y * e2z - e1z * e2y;
        double ny = e1z * e2x - e1x * e2z;
        double nz = e1x * e2y - e1y * e2x;
        double area2 = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (area2 == 0.0) return false;
        double scale = std::max({std::fabs(static_cast<double>(a.x)), std::fabs(static_cast<double>(a.y)),
                                 std::fabs(static_cast<double>(a.z)), std::fabs(static_cast<double>(b.x)),
                                 std::fabs(static_cast<double>(b.y)), std::fabs(static_cast<double>(b.z)),
                                 std::fabs(static_cast<double>(c.x)), std::fabs(static_cast<double>(c.y)),
                                 std::fabs(static_cast<double>(c.z))});
        // The extra margin is bounded by float vertex quantization, relative to triangle altitude.
        tolerance = std::min(0.01, 2.0 * std::numeric_limits<float>::epsilon() *
                             scale * std::sqrt(std::max(length1Sq, length2Sq)) / area2);
    }
    double invDet = 1.0 / det;
    double tx = -static_cast<double>(a.x);
    double ty = -static_cast<double>(a.y);
    double tz = -static_cast<double>(a.z);
    
    double u = (tx * px + ty * py + tz * pz) * invDet;
    if (u < -tolerance || u > 1.0 + tolerance) return false;
    double qx = ty * e1z - tz * e1y;
    double qy = tz * e1x - tx * e1z;
    double qz = tx * e1y - ty * e1x;
    double v = (ray.x * qx + ray.y * qy + ray.z * qz) * invDet;
    if (v < -tolerance || u + v > 1.0 + tolerance) return false;
    distance = (e2x * qx + e2y * qy + e2z * qz) * invDet;
    return distance > 0.0;
}

static bestHit MinHitInFace(const Mesh& face, const SurfaceRay& ray, double best, int faceIndex,
                            bool tolerateEdges = false) {
    bestHit bestHit = { false, best, -1, -1 };
    double distancia = 0.0;
    for (int t = 0; t < face.triangleCount; ++t) {
        int i0 = face.indices[3*t], i1 = face.indices[3*t+1], i2 = face.indices[3*t+2];
        Vector3 A = { face.vertices[i0*3+0], face.vertices[i0*3+1], face.vertices[i0*3+2] };
        Vector3 B = { face.vertices[i1*3+0], face.vertices[i1*3+1], face.vertices[i1*3+2] };
        Vector3 C = { face.vertices[i2*3+0], face.vertices[i2*3+1], face.vertices[i2*3+2] };
        if (RayTriangleHitPrecise(ray, A, B, C, distancia, tolerateEdges) && distancia < bestHit.distance){
            bestHit.hit = true;
            bestHit.distance = distancia;
            bestHit.triangleIndex = t;
            bestHit.faceIndex = faceIndex;
        }
    }
    return bestHit;
}

struct bestHit MeshGroundHitLocal(const std::vector<Mesh>& faces, const std::vector<Vector3>& faceCenters, Vector3 dir) {
    SurfaceRay ray = MakeSurfaceRay(dir);

    std::vector<int> order(faces.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return Vector3DotProduct(dir, faceCenters[a]) > Vector3DotProduct(dir, faceCenters[b]);
    });

    const size_t NEAREST = 3;
    double best = std::numeric_limits<double>::infinity();
    bestHit closestHit = { false, best, -1, -1 };
    for (size_t k = 0; k < NEAREST && k < order.size(); ++k) {
        bestHit hit = MinHitInFace(faces[order[k]], ray, best, order[k]);
        if (hit.hit) {
            best = hit.distance;
            closestHit = hit;
        }
    }

    if (!closestHit.hit)
        for (size_t i = 0; i < faces.size(); ++i) {
            bestHit hit = MinHitInFace(faces[i], ray, best, static_cast<int>(i));
            if (hit.hit) {
                best = hit.distance;
                closestHit = hit;
            }
        }
    return closestHit;
}

double MeshGroundRadiusLocal(const std::vector<Mesh>& faces, const std::vector<Vector3>& faceCenters, Vector3 dir) {
    bestHit hit = MeshGroundHitLocal(faces, faceCenters, dir);
    if (!hit.hit) throw std::runtime_error("No se encontro la superficie del planeta");
    return hit.distance;
}

std::vector<Vector3> MeshFaceCentersLocal(Vector3 a, Vector3 b, Vector3 c, int divisions) {
    std::vector<Vector3> centers;
    if (divisions <= 0) throw std::invalid_argument("Divisions must be positive");
    for (int i = 0; i <= divisions; ++i) {
        for (int j = 0; j <= divisions - i; ++j) {
            double u = static_cast<double>(i) / divisions;
            double v = static_cast<double>(j) / divisions;
            centers.push_back({
                static_cast<float>(a.x + (static_cast<double>(b.x) - a.x) * u + (static_cast<double>(c.x) - a.x) * v),
                static_cast<float>(a.y + (static_cast<double>(b.y) - a.y) * u + (static_cast<double>(c.y) - a.y) * v),
                static_cast<float>(a.z + (static_cast<double>(b.z) - a.z) * u + (static_cast<double>(c.z) - a.z) * v)
            });
        }
    }
    return centers;
}

std::vector<int> MeshFaceIndicesLocal(int divisions) {
    if (divisions <= 0) throw std::invalid_argument("Divisions must be positive");
    const long long n = divisions;
    if ((n + 1) * (n + 2) / 2 - 1 > std::numeric_limits<int>::max())
        throw std::invalid_argument("Too many divisions for int indices");
    auto inicioFila = [divisions](int i) {
        const long long row = i;
        return static_cast<int>(row * (divisions + 1LL) - row * (row - 1) / 2);
    };
    std::vector<int> indices;
    for (int i = 0; i < divisions; ++i) {
        for (int j = 0; j < divisions - i; ++j) {
            int a = inicioFila(i) + j;
            int b = inicioFila(i + 1) + j;
            int c = a + 1;
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
            if (j < divisions - i - 1) {
                indices.push_back(b);
                indices.push_back(b + 1);
                indices.push_back(c);
            }
        }
    }
    return indices;
}

Mesh BuildLocalTriangleMesh(const Mesh& face, int triangleIndex, int divisions) {
    if (divisions <= 0 || divisions > 360) throw std::invalid_argument("Divisions must be between 1 and 360");
    if (triangleIndex < 0 || triangleIndex >= face.triangleCount) throw std::invalid_argument("Invalid triangle index");
    if (!face.indices || !face.vertices || !face.normals || !face.texcoords)
        throw std::invalid_argument("Face is missing mesh data");

    const size_t triangleOffset = static_cast<size_t>(triangleIndex) * 3;
    const int ia = face.indices[triangleOffset];
    const int ib = face.indices[triangleOffset + 1];
    const int ic = face.indices[triangleOffset + 2];
    if (ia >= face.vertexCount || ib >= face.vertexCount || ic >= face.vertexCount)
        throw std::invalid_argument("Face contains an invalid vertex index");

    Vector3 a = {face.vertices[ia * 3], face.vertices[ia * 3 + 1], face.vertices[ia * 3 + 2]};
    Vector3 b = {face.vertices[ib * 3], face.vertices[ib * 3 + 1], face.vertices[ib * 3 + 2]};
    Vector3 c = {face.vertices[ic * 3], face.vertices[ic * 3 + 1], face.vertices[ic * 3 + 2]};

    const std::vector<Vector3> vertices = MeshFaceCentersLocal(a, b, c, divisions);
    const std::vector<int> indices = MeshFaceIndicesLocal(divisions);

    Mesh localMesh = {};
    std::vector<Vector3> normalesRelieve(vertices.size(), Vector3{0, 0, 0});
    std::vector<float> pesos(vertices.size());  
    localMesh.vertexCount = static_cast<int>(vertices.size());
    localMesh.triangleCount = static_cast<int>(indices.size() / 3);
    localMesh.vertices = (float*)MemAlloc(vertices.size() * 3 * sizeof(float));
    localMesh.normals = (float*)MemAlloc(vertices.size() * 3 * sizeof(float));
    localMesh.texcoords = (float*)MemAlloc(vertices.size() * 2 * sizeof(float));
    localMesh.indices = (unsigned short*)MemAlloc(indices.size() * sizeof(unsigned short));
    if (!localMesh.vertices || !localMesh.normals || !localMesh.texcoords || !localMesh.indices) {
        MemFree(localMesh.vertices);
        MemFree(localMesh.normals);
        MemFree(localMesh.texcoords);
        MemFree(localMesh.indices);
        throw std::bad_alloc();
    }

    size_t vertex = 0;
    for (int i = 0; i <= divisions; ++i) {
        for (int j = 0; j <= divisions - i; ++j, ++vertex) {
            float u = static_cast<float>(i) / divisions;
            float v = static_cast<float>(j) / divisions;
            float w = static_cast<float>(divisions - i - j) / divisions;
            float peso = SmoothStep(0.0f, 0.1f, std::min({u, v, w}));
            pesos[vertex] = peso;
            Vector3 dir = Vector3Normalize(vertices[vertex]);
            float ruido = Noise3D(Vector3Scale(dir, 512.0f));
            float desplazamiento = ruido * peso * 0.00005f;
            Vector3 p = Vector3Add(vertices[vertex], Vector3Scale(dir, desplazamiento));
            if (i == 0 && j == 0) p = a;
            else if (i == divisions && j == 0) p = b;
            else if (i == 0 && j == divisions) p = c;
            localMesh.vertices[vertex * 3] = p.x;
            localMesh.vertices[vertex * 3 + 1] = p.y;
            localMesh.vertices[vertex * 3 + 2] = p.z;
            for (int k = 0; k < 3; ++k) {
                float na = face.normals[ia * 3 + k];
                localMesh.normals[vertex * 3 + k] = na + (face.normals[ib * 3 + k] - na) * u
                                                        + (face.normals[ic * 3 + k] - na) * v;
            }
            for (int k = 0; k < 2; ++k) {
                float ta = face.texcoords[ia * 2 + k];
                localMesh.texcoords[vertex * 2 + k] = ta + (face.texcoords[ib * 2 + k] - ta) * u
                                                          + (face.texcoords[ic * 2 + k] - ta) * v;
            }
        }
    }
    for (size_t i = 0; i < indices.size(); ++i) {
        localMesh.indices[i] = static_cast<unsigned short>(indices[i]);
    }
    for (int t = 0; t < localMesh.triangleCount; ++t) {
        int i0 = localMesh.indices[3 * t];
        int i1 = localMesh.indices[3 * t + 1];
        int i2 = localMesh.indices[3 * t + 2];
        Vector3 p0 = {
            localMesh.vertices[3 * i0],
            localMesh.vertices[3 * i0 + 1],
            localMesh.vertices[3 * i0 + 2]
        };
        Vector3 p1 = {
            localMesh.vertices[3 * i1],
            localMesh.vertices[3 * i1 + 1],
            localMesh.vertices[3 * i1 + 2]
        };
        Vector3 p2 = {
            localMesh.vertices[3 * i2],
            localMesh.vertices[3 * i2 + 1],
            localMesh.vertices[3 * i2 + 2]
        };
        Vector3 n = Vector3CrossProduct(
            Vector3Subtract(p1, p0), Vector3Subtract(p2, p0));

        normalesRelieve[i0] = Vector3Add(normalesRelieve[i0], n);
        normalesRelieve[i1] = Vector3Add(normalesRelieve[i1], n);
        normalesRelieve[i2] = Vector3Add(normalesRelieve[i2], n);
    }
    for (int i = 0; i < localMesh.vertexCount; ++i) {
        Vector3 original = {
            localMesh.normals[3 * i],
            localMesh.normals[3 * i + 1],
            localMesh.normals[3 * i + 2]
        };
        Vector3 relieve = Vector3Normalize(normalesRelieve[i]);
        Vector3 normal = Vector3Normalize(
            Vector3Lerp(Vector3Normalize(original), relieve, pesos[i]));

        localMesh.normals[3 * i] = normal.x;
        localMesh.normals[3 * i + 1] = normal.y;
        localMesh.normals[3 * i + 2] = normal.z;
    }
    return localMesh;
}

bestHit MeshHitLocal(const Mesh& mesh, Vector3 dir, bool tolerateEdges){
    SurfaceRay ray = MakeSurfaceRay(dir);
    bestHit hit = MinHitInFace(mesh, ray, std::numeric_limits<double>::infinity(), 0);
    if (!hit.hit && tolerateEdges)
        hit = MinHitInFace(mesh, ray, std::numeric_limits<double>::infinity(), 0, true);
    return hit;
}

std::vector<PatchFace> FindPatchFaces(const std::vector<Mesh>& faces, int faceIndex, int triangleIndex) {
    if (faceIndex < 0 || faceIndex >= static_cast<int>(faces.size()))
        throw std::invalid_argument("Invalid patch face index");
    const Mesh& source = faces[faceIndex];
    if (!source.indices || !source.vertices || triangleIndex < 0 || triangleIndex >= source.triangleCount)
        throw std::invalid_argument("Invalid patch triangle");

    int vertices[3] = {source.indices[3 * triangleIndex], source.indices[3 * triangleIndex + 1],
                       source.indices[3 * triangleIndex + 2]};
    for (int vertex : vertices)
        if (vertex >= source.vertexCount) throw std::invalid_argument("Invalid patch vertex");

    std::vector<PatchFace> result = {{faceIndex, triangleIndex, {}}};
    for (int edge = 0; edge < 3; ++edge) {
        int a = vertices[edge], b = vertices[(edge + 1) % 3];
        int neighbourFace = faceIndex;
        int neighbour = FindEdgeNeighbour(source, triangleIndex, a, b);
        int neighbourA = a, neighbourB = b;

        if (neighbour == -1) {
            for (size_t f = 0; f < faces.size(); ++f) {
                if (static_cast<int>(f) == faceIndex) continue;
                const Mesh& candidate = faces[f];
                if (!candidate.vertices || !candidate.indices)
                    throw std::invalid_argument("Face is missing mesh data");
                int candidateA = -1, candidateB = -1;
                for (int v = 0; v < candidate.vertexCount; ++v) {
                    // SplitByBaseFace copies shared positions from the same global vertices.
                    bool matchA = true, matchB = true;
                    for (int c = 0; c < 3; ++c) {
                        matchA = matchA && candidate.vertices[3 * v + c] == source.vertices[3 * a + c];
                        matchB = matchB && candidate.vertices[3 * v + c] == source.vertices[3 * b + c];
                    }
                    if (matchA) candidateA = v;
                    if (matchB) candidateB = v;
                }
                if (candidateA == -1 || candidateB == -1) continue;
                int found = FindEdgeNeighbour(candidate, -1, candidateA, candidateB);
                if (found == -1) continue;
                if (neighbour != -1) throw std::runtime_error("Multiple neighbours for patch edge");
                neighbourFace = static_cast<int>(f);
                neighbour = found;
                neighbourA = candidateA;
                neighbourB = candidateB;
            }
        }
        if (neighbour == -1) throw std::runtime_error("No neighbour found for patch edge");

        auto it = std::find_if(result.begin(), result.end(), [neighbourFace](const PatchFace& item) {
            return item.faceIndex == neighbourFace;
        });
        if (it == result.end()) {
            result.push_back({neighbourFace, -1, {}});
            it = result.end() - 1;
        }
        it->edges.push_back({neighbour, neighbourA, neighbourB, edge});
    }
    return result;
}

Mesh BuildStitchedFace(const Mesh& face, const PatchFaceBuild& patchFace,
                       const std::vector<const Mesh*>& patches, const std::vector<int>& patchDivisions) {
    if (patchDivisions.empty())
        throw std::invalid_argument("Patch divisions must be provided");
    if (!face.vertices || !face.normals || !face.texcoords || !face.indices ||
        face.vertexCount <= 0 || face.triangleCount <= 0)
        throw std::invalid_argument("Invalid face mesh");
    if (patches.empty())
        throw std::invalid_argument("At least one patch mesh is required");

    if (patchDivisions.size() != patches.size())
        throw std::invalid_argument("Mismatch between number of patches and patch divisions");
    for (size_t i = 0; i < patches.size(); ++i) {
        const Mesh* patch = patches[i];
        const int divisions = patchDivisions[i];
        if (divisions <= 0 || divisions > 360)
            throw std::invalid_argument("Invalid patch divisions");

        const int expectedPatchVertices = (divisions + 1) * (divisions + 2) / 2;
        if (!patch || !patch->vertices || !patch->normals || !patch->texcoords ||
            patch->vertexCount != expectedPatchVertices)
            throw std::invalid_argument("Invalid patch mesh");
    }

    std::vector<bool> removed(face.triangleCount, false);
    for (int triangle : patchFace.removedTriangles) {
        if (triangle < 0 || triangle >= face.triangleCount || removed[triangle])
            throw std::invalid_argument("Invalid or duplicate removed triangle");
        removed[triangle] = true;
    }

    struct PreparedStitch {
        const PatchStitch* stitch;
        std::vector<int> patchEdge;
        std::vector<int> faceEdge;
        std::vector<int> fan;
    };
    std::vector<PreparedStitch> prepared;
    prepared.reserve(patchFace.stitches.size());
    int nextVertex = face.vertexCount;
    for (const PatchStitch& stitch : patchFace.stitches) {
        const PatchFaceEdge& edge = stitch.edge;
        if (stitch.patchIndex < 0 || stitch.patchIndex >= static_cast<int>(patches.size()) ||
            stitch.divisions != patchDivisions[stitch.patchIndex] ||
            edge.triangleIndex < 0 || edge.triangleIndex >= face.triangleCount || removed[edge.triangleIndex] ||
            stitch.divisions <= 0 || stitch.divisions > 360 ||
            edge.patchEdge < 0 || edge.patchEdge > 2 ||
            edge.edgeA < 0 || edge.edgeA >= face.vertexCount ||
            edge.edgeB < 0 || edge.edgeB >= face.vertexCount)
            throw std::invalid_argument("Invalid stitched edge");

        for (const PreparedStitch& previous : prepared) {
            if (previous.stitch->edge.triangleIndex == edge.triangleIndex ||
                (previous.stitch->patchIndex == stitch.patchIndex &&
                 previous.stitch->edge.patchEdge == edge.patchEdge))
                throw std::invalid_argument("Duplicate stitched triangle or patch edge");
        }

        bool reverseEdgeFound = false;
        for (int corner = 0; corner < 3; ++corner) {
            if (face.indices[3 * edge.triangleIndex + corner] == edge.edgeB &&
                face.indices[3 * edge.triangleIndex + (corner + 1) % 3] == edge.edgeA)
                reverseEdgeFound = true;
        }
        if (!reverseEdgeFound)
            throw std::invalid_argument("Neighbour edge has invalid orientation");

        std::vector<int> patchEdges[3] = {
            PatchEdgeABIndices(stitch.divisions),
            PatchEdgeBCIndices(stitch.divisions),
            PatchEdgeCAIndices(stitch.divisions)
        };
        const std::vector<int>& patchEdge = patchEdges[edge.patchEdge];
        const Mesh& patch = *patches[stitch.patchIndex];
        for (int component = 0; component < 3; ++component) {
            if (face.vertices[3 * edge.edgeA + component] != patch.vertices[3 * patchEdge.front() + component] ||
                face.vertices[3 * edge.edgeB + component] != patch.vertices[3 * patchEdge.back() + component])
                throw std::invalid_argument("Patch edge endpoints do not match face");
        }

        int opposite = FindOppositeVertex(face, edge.triangleIndex, edge.edgeA, edge.edgeB);
        if (opposite < 0 || opposite >= face.vertexCount)
            throw std::invalid_argument("Neighbour is missing an opposite vertex");

        std::vector<int> faceEdge = {edge.edgeA};
        for (int k = 1; k < stitch.divisions; ++k) faceEdge.push_back(nextVertex++);
        faceEdge.push_back(edge.edgeB);
        prepared.push_back({&stitch, patchEdge, faceEdge, BuildNeighbourFanIndices(opposite, faceEdge)});
    }

    const size_t vertexCount = static_cast<size_t>(nextVertex);
    size_t stitchTriangleCount = 0;
    for (const PreparedStitch& item : prepared) {
        stitchTriangleCount += static_cast<size_t>(item.stitch->divisions - 1);
    }

    const size_t triangleCount =
        static_cast<size_t>(face.triangleCount) - patchFace.removedTriangles.size() +
        stitchTriangleCount;
    if (vertexCount > 65535 || triangleCount == 0 ||
        triangleCount > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        triangleCount > std::numeric_limits<unsigned int>::max() / (3 * sizeof(unsigned short)))
        throw std::invalid_argument("Stitched face exceeds mesh limits");

    Mesh modifiedFace = {};
    modifiedFace.vertexCount = static_cast<int>(vertexCount);
    modifiedFace.triangleCount = static_cast<int>(triangleCount);
    modifiedFace.vertices = (float*)MemAlloc(vertexCount * 3 * sizeof(float));
    modifiedFace.normals = (float*)MemAlloc(vertexCount * 3 * sizeof(float));
    modifiedFace.texcoords = (float*)MemAlloc(vertexCount * 2 * sizeof(float));
    modifiedFace.indices = (unsigned short*)MemAlloc(triangleCount * 3 * sizeof(unsigned short));
    if (!modifiedFace.vertices || !modifiedFace.normals || !modifiedFace.texcoords || !modifiedFace.indices) {
        MemFree(modifiedFace.vertices);
        MemFree(modifiedFace.normals);
        MemFree(modifiedFace.texcoords);
        MemFree(modifiedFace.indices);
        throw std::bad_alloc();
    }

    for (int i = 0; i < face.vertexCount * 3; ++i) {
        modifiedFace.vertices[i] = face.vertices[i];
        modifiedFace.normals[i] = face.normals[i];
    }
    for (int i = 0; i < face.vertexCount * 2; ++i)
        modifiedFace.texcoords[i] = face.texcoords[i];

    for (const PreparedStitch& stitch : prepared) {
        const Mesh& patch = *patches[stitch.stitch->patchIndex];
        const std::vector<int>& patchEdge = stitch.patchEdge;
        for (size_t k = 1; k + 1 < patchEdge.size(); ++k) {
            int source = patchEdge[k];
            int destination = stitch.faceEdge[k];
            for (int component = 0; component < 3; ++component) {
                modifiedFace.vertices[3 * destination + component] = patch.vertices[3 * source + component];
                modifiedFace.normals[3 * destination + component] = patch.normals[3 * source + component];
            }
            for (int component = 0; component < 2; ++component)
                modifiedFace.texcoords[2 * destination + component] = patch.texcoords[2 * source + component];
        }
    }

    size_t outputIndex = 0;
    for (int triangle = 0; triangle < face.triangleCount; ++triangle) {
        if (removed[triangle]) continue;

        const PreparedStitch* stitchForTriangle = nullptr;
        for (const PreparedStitch& stitch : prepared) {
            if (stitch.stitch->edge.triangleIndex == triangle) {
                stitchForTriangle = &stitch;
                break;
            }
        }
        if (stitchForTriangle) {
            for (int vertex : stitchForTriangle->fan)
                modifiedFace.indices[outputIndex++] = static_cast<unsigned short>(vertex);
        } else {
            for (int corner = 0; corner < 3; ++corner)
                modifiedFace.indices[outputIndex++] = face.indices[3 * triangle + corner];
        }
    }

    if (outputIndex != triangleCount * 3) {
        MemFree(modifiedFace.vertices);
        MemFree(modifiedFace.normals);
        MemFree(modifiedFace.texcoords);
        MemFree(modifiedFace.indices);
        throw std::runtime_error("Stitched face triangle count mismatch");
    }
    return modifiedFace;
}
