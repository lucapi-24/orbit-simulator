#pragma once
#include <raylib.h>
#include <vector>
#include "IcoSphere.hpp"

// Octavas de ruido que la malla puede muestrear a esta profundidad (Nyquist: λ ≥ 2 × arista).
// La colisión usa el mismo número para que el suelo coincida con lo que se dibuja.
int OctavesForDepth(int depth);

// Altura del relieve (km) con exageración visual RELIEVE_K, usada por malla y colisión.
constexpr float RELIEVE_K = 1.5f;
float AlturaRelieve(Vector3 dir, int octavas);

// Malla del planeta: vértices desplazados por el relieve (radio ≈ 1), normales
// recalculadas y la altura (km) en texcoords.x. `indices` son los de BuildIcosphere.
Mesh BuildPlanetMesh(const std::vector<IcoVertex>& vertices, const std::vector<int>& indices, int depth);

// Parte la malla en 20 submallas, una por cara base del icosaedro, para que cada
// una quede dentro del límite de 65 535 índices (unsigned short) de raylib.
std::vector<Mesh> SplitByBaseFace(const Mesh& mesh, const std::vector<int>& indices, int depth);

// Radio real (radio ≈ 1) en la dirección `dir` del centro, por ray-triangle contra la malla.
// Prueba primero las 3 caras cuyo centro está más cerca de `dir` y, si no hay impacto,
// todas. `faceCenters` debe tener un centro normalizado por cada cara de `faces`.
float MeshGroundRadiusLocal(const std::vector<Mesh>& faces, const std::vector<Vector3>& faceCenters, Vector3 dir);
