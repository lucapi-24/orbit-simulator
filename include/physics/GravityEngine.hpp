#pragma once
#include "entities/CelestialBody.hpp"
#include "entities/Spaceship.hpp"
#include <vector>

struct OrbitInfo {
    double mu, r, v, eps, h;   // μ, distancia, velocidad, energía, momento angular
    double a, e;                // semieje mayor, excentricidad
    double rp, ra;              // pericentro, apocentro
    double T;                   // periodo
    bool hyperbolic;            // true si ε ≥ 0
};

class GravityEngine {
private: 
    static constexpr double G = 6.67430e-11; // Gravitational constant (SI)
public:
    static void UpdateOrbits(std::vector<CelestialBody>& bodies, double dt);
    static void UpdateSpaceship(Spaceship& ship, const std::vector<CelestialBody>& bodies, double dt);
    static void UpdateAll(std::vector<CelestialBody>& bodies, Spaceship& ship, double dt);
    static std::vector<Vec2d> PredictConic(
        const Vec2d& rRel, 
        const Vec2d& vRel, 
        double mu,
        const Vec2d& primaryPos,
        int numPoints);
    static Vec2d ComputeAcceleration(const Vec2d& pos, const std::vector<CelestialBody>& bodies);
    static const CelestialBody* GetDominantBody(const Vec2d& pos, const std::vector<CelestialBody>& bodies);
    static const OrbitInfo ComputeOrbitInfo(const Vec2d& r_rel, const Vec2d& v_rel, double mu);
};