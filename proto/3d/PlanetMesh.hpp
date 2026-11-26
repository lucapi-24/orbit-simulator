#pragma once
#include <raylib.h>
#include <vector>
#include "IcoSphere.hpp"

enum class TerrainType { Earth, Moon };
struct PlanetGeometry {
    Mesh sphereMesh;
    std::vector<Mesh> icoFaces;
    std::vector<Vector3> faceCenters;
};

struct PatchTriangle {
    int faceIndex;
    int triangleIndex;
    int divisions;
};

struct Crater {
    Vector3 centro;
    float profundidad;
    float bordeAltura;
    float radio;
};

struct bestHit {
        bool hit;
        double distance;
        int faceIndex;
        int triangleIndex;
};



// Octavas de ruido que la malla puede muestrear a esta profundidad (Nyquist: λ ≥ 2 × arista).
// La colisión usa el mismo número para que el suelo coincida con lo que se dibuja.
int OctavesForDepth(int depth);

// Altura del relieve (km) con exageración visual RELIEVE_K, usada por malla y colisión.
constexpr float RELIEVE_K = 1.5f;
float AlturaRelieve(Vector3 dir, int octavas);
std::vector<Crater> GenerateCrateres();
float AlturaRelieveLunar(Vector3 dir, int octavas);
float AlturaCrater(Vector3 dir, Vector3 centro, float profundidad, float bordeAltura, float radio);
//Cálculo de la geometría del planeta: malla esférica y caras del icosaedro.
PlanetGeometry BuildPlanetGeometry(int depth, TerrainType terrainType);

// Malla del planeta: vértices desplazados por el relieve (radio ≈ 1), normales
// recalculadas y la altura (km) en texcoords.x. `indices` son los de BuildIcosphere.
Mesh BuildPlanetMesh(const std::vector<IcoVertex>& vertices, const std::vector<int>& indices, int depth, TerrainType terrainType);
        
int FindEdgeNeighbour(const Mesh& face, int triangleIndex, int edgeA, int edgeB);

int FindOppositeVertex(const Mesh& face, int triangleIndex, int edgeA, int edgeB);

std::vector<int> PatchEdgeABIndices(int divisions);
std::vector<int> PatchEdgeBCIndices(int divisions);
std::vector<int> PatchEdgeCAIndices(int divisions);

std::vector<int> BuildNeighbourFanIndices(int opposite, const std::vector<int>& edge);

// Parte la malla en 20 submallas, una por cara base del icosaedro, para que cada
// una quede dentro del límite de 65 535 índices (unsigned short) de raylib.
std::vector<Mesh> SplitByBaseFace(const Mesh& mesh, const std::vector<int>& indices, int depth);

// Radio real (radio ≈ 1) en la dirección `dir` del centro, por ray-triangle contra la malla.
// Prueba primero las 3 caras cuyo centro está más cerca de `dir` y, si no hay impacto,
// todas. `faceCenters` debe tener un centro normalizado por cada cara de `faces`.
bestHit MeshGroundHitLocal(const std::vector<Mesh>& faces, const std::vector<Vector3>& faceCenters, Vector3 dir);

// Lanza una excepcion si no se encuentra la superficie.
double MeshGroundRadiusLocal(const std::vector<Mesh>& faces, const std::vector<Vector3>& faceCenters, Vector3 dir);

std::vector<Vector3> MeshFaceCentersLocal(Vector3 a, Vector3 b, Vector3 c, int divisions);

std::vector<int> MeshFaceIndicesLocal(int divisions);

// Malla en CPU; el llamador debe liberar sus arrays con MemFree o transferirlos a un Model.
Mesh BuildLocalTriangleMesh(const Mesh& face, int triangleIndex, int divisions);

struct PatchFaceEdge {
    int triangleIndex;
    int edgeA;
    int edgeB;
    int patchEdge;
};

struct PatchFace {
    int faceIndex;
    int removedTriangle;
    std::vector<PatchFaceEdge> edges;
};

struct PatchStitch {
    PatchFaceEdge edge;
    int patchIndex;
    int divisions;
};

struct PatchFaceBuild {
    int faceIndex;
    std::vector<int> removedTriangles;
    std::vector<PatchStitch> stitches;
};

std::vector<PatchFace> FindPatchFaces(const std::vector<Mesh>& faces, int faceIndex, int triangleIndex);
Mesh BuildStitchedFace(
    const Mesh& face,
    const PatchFaceBuild& patchFace,
    const std::vector<const Mesh*>& patches,
    const std::vector<int>& patchDivisions);

// La tolerancia solo debe habilitarse si la malla global confirma que dir pertenece al parche.
bestHit MeshHitLocal(const Mesh& mesh, Vector3 dir, bool tolerateEdges = false);

