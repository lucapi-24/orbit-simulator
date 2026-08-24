#pragma once
#include "entities/CelestialBody.hpp"
#include "entities/Spaceship.hpp"
#include <vector>

class GravityEngine {
private: 
    static constexpr double G = 6.67430e-11; // Gravitational constant (SI)
public:
    static void UpdateOrbits(std::vector<CelestialBody>& bodies, double dt);
    static void UpdateSpaceship(Spaceship& ship, const std::vector<CelestialBody>& bodies, double dt);
    static std::vector<std::vector<Vec2d>> PredictTrajectories(
        std::vector<CelestialBody> virtualBodies, 
        Spaceship ship, 
        double dt, 
        int steps);
    static Vec2d ComputeAcceleration(const Vec2d& pos, const std::vector<CelestialBody>& bodies);
};