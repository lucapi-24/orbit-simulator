#include "PlanetMesh.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
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

Mesh BuildPlanetMesh(const std::vector<IcoVertex>& vertices, const std::vector<int>& indices, int depth) {
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
        float h = AlturaRelieve(dir, octavas);
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

static float MinHitInFace(const Mesh& face, const Ray& ray, float best) {
    for (int t = 0; t < face.triangleCount; ++t) {
        int i0 = face.indices[3*t], i1 = face.indices[3*t+1], i2 = face.indices[3*t+2];
        Vector3 A = { face.vertices[i0*3+0], face.vertices[i0*3+1], face.vertices[i0*3+2] };
        Vector3 B = { face.vertices[i1*3+0], face.vertices[i1*3+1], face.vertices[i1*3+2] };
        Vector3 C = { face.vertices[i2*3+0], face.vertices[i2*3+1], face.vertices[i2*3+2] };
        RayCollision c = GetRayCollisionTriangle(ray, A, B, C);
        if (c.hit && c.distance > 0.0f && c.distance < best) best = c.distance;
    }
    return best;
}

float MeshGroundRadiusLocal(const std::vector<Mesh>& faces, const std::vector<Vector3>& faceCenters, Vector3 dir) {
    Ray ray = { (Vector3){0, 0, 0}, dir };

    std::vector<int> order(faces.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return Vector3DotProduct(dir, faceCenters[a]) > Vector3DotProduct(dir, faceCenters[b]);
    });

    const size_t NEAREST = 3;
    float best = 1e9f;
    for (size_t k = 0; k < NEAREST && k < order.size(); ++k)
        best = MinHitInFace(faces[order[k]], ray, best);

    if (best == 1e9f)
        for (const Mesh& face : faces)
            best = MinHitInFace(face, ray, best);
    return best;
}

