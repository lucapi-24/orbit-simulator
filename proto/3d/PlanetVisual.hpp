#include "PlanetMesh.hpp"
#include <vector>
#include <raylib.h>

struct PlanetVisual{
    PlanetGeometry planetGeom;
    std::vector<Model> icoFaceModels;
};

PlanetVisual BuildPlanetVisual(int icoDepth, Shader terrenoShader, TerrainType terrainType);
void DrawPlanetVisual(const PlanetVisual& planetVisual, Vector3 position, float radius, Vector3 axis, float angle);
void UnloadPlanetVisual(PlanetVisual& planetVisual);