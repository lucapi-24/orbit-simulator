
#include "PlanetVisual.hpp"
#include "PlanetMesh.hpp"


PlanetVisual BuildPlanetVisual(int icoDepth, Shader terrenoShader, TerrainType terrainType) {
    PlanetVisual planetVisual;
    planetVisual.planetGeom = BuildPlanetGeometry(icoDepth, terrainType);
    for (const Mesh& face : planetVisual.planetGeom.icoFaces) {
        Model m = LoadModelFromMesh(face);
        m.materials[0].shader = terrenoShader;
        planetVisual.icoFaceModels.push_back(m);
    }
    return planetVisual;
}

void DrawPlanetVisual(const PlanetVisual& planetVisual, Vector3 position, float radius, Vector3 axis, float angle) {
    for (const Model& face : planetVisual.icoFaceModels) {
            DrawModelEx(face, position, axis, angle * RAD2DEG,
                (Vector3){radius, radius, radius}, WHITE);   // WHITE = no sobretiñe
        }
}
void UnloadPlanetVisual(PlanetVisual& planetVisual) {
    for (Model& face : planetVisual.icoFaceModels) {
        UnloadModel(face);
    }
    MemFree(planetVisual.planetGeom.sphereMesh.vertices);
    MemFree(planetVisual.planetGeom.sphereMesh.normals);
    MemFree(planetVisual.planetGeom.sphereMesh.texcoords);
}