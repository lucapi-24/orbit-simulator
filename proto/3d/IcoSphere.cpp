#include "IcoSphere.hpp"
#include <array>
#include <cmath>
#include <map>
#include <utility>

namespace {

IcoVertex IcoVertexMake(float x, float y, float z) {
    return {x, y, z};
}

float IcoVertexLen(const IcoVertex& v) {
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

IcoVertex IcoVertexNorm(const IcoVertex& v) {
    float len = IcoVertexLen(v);
    return {v.x/len, v.y/len, v.z/len};
}

// Los 12 vértices de un icosaedro regular (φ = razón áurea)
const float PHI = 1.61803398875f;
const IcoVertex VERTICES_ICOSAEDRO[12] = {
    IcoVertexMake( 0,  1,  PHI), IcoVertexMake( 0,  1, -PHI),
    IcoVertexMake( 0, -1,  PHI), IcoVertexMake( 0, -1, -PHI),
    IcoVertexMake( 1,  PHI,  0), IcoVertexMake( 1, -PHI,  0),
    IcoVertexMake(-1,  PHI,  0), IcoVertexMake(-1, -PHI,  0),
    IcoVertexMake( PHI,  0,  1), IcoVertexMake( PHI,  0, -1),
    IcoVertexMake(-PHI,  0,  1), IcoVertexMake(-PHI,  0, -1)
};

// Las 20 caras (índices de los vértices de arriba)
const int TRIANGLES_ICOSAEDRO[20][3] = {
    {0, 2, 8}, {0, 10, 2}, {0, 4, 6}, {0, 8, 4}, {0, 6, 10},
    {1, 9, 3}, {1, 3, 11}, {1, 6, 4}, {1, 4, 9}, {1, 11, 6},
    {2, 7, 5}, {2, 5, 8}, {2, 10, 7}, {3, 5, 7}, {3, 9, 5},
    {3, 7, 11}, {4, 8, 9}, {5, 9, 8}, {6, 11, 10}, {7, 10, 11}
};

// Punto medio de la arista (i,j) proyectado a la esfera. Si ya existe, se reutiliza.
int getMidpoint(int i, int j, std::vector<IcoVertex>& vertices, std::map<std::pair<int,int>, int>& cache) {
    if (i > j) std::swap(i, j);   // (i,j) y (j,i) son la misma arista
    std::pair<int,int> key = {i, j};
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    IcoVertex a = vertices[i];
    IcoVertex b = vertices[j];
    IcoVertex mid = IcoVertexNorm({(a.x + b.x)/2.0f, (a.y + b.y)/2.0f, (a.z + b.z)/2.0f});
    int index = (int)vertices.size();
    vertices.push_back(mid);
    cache[key] = index;
    return index;
}

}  // namespace

void BuildIcosphere(int depth, std::vector<IcoVertex>& vertices, std::vector<int>& indices) {
    vertices.clear();
    for (int i = 0; i < 12; ++i) vertices.push_back(IcoVertexNorm(VERTICES_ICOSAEDRO[i]));

    std::vector<std::array<int, 3>> faces;
    for (int j = 0; j < 20; ++j)
        faces.push_back({TRIANGLES_ICOSAEDRO[j][0], TRIANGLES_ICOSAEDRO[j][1], TRIANGLES_ICOSAEDRO[j][2]});

    // Cada nivel divide cada triángulo en 4
    for (int i = 0; i < depth; ++i) {
        std::map<std::pair<int,int>, int> midpointCache;
        std::vector<std::array<int, 3>> newFaces;
        for (const auto& f : faces) {
            int a = f[0], b = f[1], c = f[2];
            int ab = getMidpoint(a, b, vertices, midpointCache);
            int bc = getMidpoint(b, c, vertices, midpointCache);
            int ca = getMidpoint(c, a, vertices, midpointCache);
            newFaces.push_back({a, ab, ca});
            newFaces.push_back({b, bc, ab});
            newFaces.push_back({c, ca, bc});
            newFaces.push_back({ab, bc, ca});
        }
        faces = newFaces;
    }

    indices.clear();
    for (const auto& f : faces) {
        indices.push_back(f[0]);
        indices.push_back(f[1]);
        indices.push_back(f[2]);
    }
}
