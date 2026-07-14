#pragma once
#include "entities/CelestialBody.hpp"
#include "entities/Spaceship.hpp"
#include <vector>

class GravityEngine {
private: 
    static constexpr double G = 10.0; // Gravitational constant
public:
    static void UpdateOrbits(std::vector<CelestialBody>& bodies, float dt);
    static void UpdateSpaceship(Spaceship& ship, const std::vector<CelestialBody>& bodies, float dt);
    static std::vector<std::vector<Vector2>> PredictTrajectories(
        std::vector<CelestialBody> virtualBodies, 
        Spaceship ship, 
        float dt, 
        int steps);
};