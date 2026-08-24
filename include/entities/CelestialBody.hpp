#pragma once
#include "physics/PhysicalState.hpp"
#include "Vec2d.hpp"
#include <string>

class CelestialBody {
public:
    std::string name;
    PhysicalState physics;
    double radiusM; // radio REAL en metros
    Color color;
    bool isStatic = false; // Indica si el cuerpo es estático (no se mueve)
    CelestialBody(std::string name, Vec2d pos, Vec2d vel, double mass, double radiusMeters, Color color, bool isStatic = false)
    : name(name), physics{pos, vel, mass}, radiusM(radiusMeters), color(color), isStatic(isStatic) {}

    // invZoom = 1/camera.zoom. Escala real; mínimo 1 px en pantalla para no desaparecer.
    void Draw(float invZoom = 1.0f) const {
        float rWorld = static_cast<float>(radiusM * Vec2d::renderScale);
        if (rWorld < invZoom) rWorld = invZoom;
        DrawCircleV(physics.position.toRaylib(), rWorld, color);
    }
};