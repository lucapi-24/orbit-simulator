#pragma once
#include "physics/PhysicalState.hpp"
#include <string>

class CelestialBody {
public:
    std::string name;
    PhysicalState physics;
    float radius;
    Color color;
    CelestialBody(std::string name, Vector2 pos, Vector2 vel, double mass, float radius, Color color)
    : name(name), physics{pos, vel, mass}, radius(radius), color(color) {}

    void Draw() const {
        DrawCircleV(physics.position, radius, color);
    }
};