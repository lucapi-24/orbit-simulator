#include "physics/GravityEngine.hpp"
#include "Vec2d.hpp"
#include <cmath>

void GravityEngine::UpdateOrbits(std::vector<CelestialBody>& bodies, double dt) {
    int n = bodies.size();
    
    std::vector<Vec2d> accelerations(n, Vec2d{0.0, 0.0});
    std::vector<Vec2d> accNew(n, Vec2d{0.0, 0.0});

    for (int i = 0; i < n; ++i) {
        accelerations[i] = ComputeAcceleration(bodies[i].physics.position, bodies);
    }

    for (int i = 0; i < n; ++i) {
        if (bodies[i].isStatic) continue;
        bodies[i].physics.position += bodies[i].physics.velocity * dt + accelerations[i] * (0.5 * dt * dt);
    }

    for (int i = 0; i < n; ++i) {
        accNew[i] = ComputeAcceleration(bodies[i].physics.position, bodies);
    }

    for (int i = 0; i < n; ++i) {
        if (bodies[i].isStatic) continue;
        bodies[i].physics.velocity += (accelerations[i] + accNew[i]) * (0.5 * dt);
    }
}

void GravityEngine::UpdateSpaceship(Spaceship& ship, const std::vector<CelestialBody>& bodies, double dt) {
    Vec2d acceleration = ComputeAcceleration(ship.physics.position, bodies);
    ship.physics.position += ship.physics.velocity * dt + acceleration * (0.5 * dt * dt);

    Vec2d accNew = ComputeAcceleration(ship.physics.position, bodies);
    ship.physics.velocity += (acceleration + accNew) * (0.5 * dt);
}

std::vector<std::vector<Vec2d>> GravityEngine::PredictTrajectories(
    std::vector<CelestialBody> virtualBodies, 
    Spaceship ship,
    double dt, 
    int steps
) {
    int n = virtualBodies.size() + 1;
    std::vector<std::vector<Vec2d>> trajectories(n);

    for (int i = 0; i < n; ++i) {
        trajectories[i].reserve(steps);
    }

    for (int step = 0; step < steps; ++step) {
        UpdateOrbits(virtualBodies, dt);

        Vec2d acc = ComputeAcceleration(ship.physics.position, virtualBodies);
        ship.physics.velocity += acc * dt;
        ship.physics.position += ship.physics.velocity * dt;

        for (size_t i = 0; i < virtualBodies.size(); ++i) {
            trajectories[i].push_back(virtualBodies[i].physics.position);
        }
        trajectories[n - 1].push_back(ship.physics.position);
    }

    return trajectories;
}

Vec2d GravityEngine::ComputeAcceleration(const Vec2d& pos, const std::vector<CelestialBody>& bodies) {
    static constexpr double EPSILON2 = 25.0;

    double ax = 0.0;
    double ay = 0.0;

    for (const auto& body : bodies) {
        double rx = body.physics.position.x - pos.x;
        double ry = body.physics.position.y - pos.y;

        double d = rx * rx + ry * ry + EPSILON2;
        double invDenom = 1.0 / (d * std::sqrt(d));

        ax += G * body.physics.mass * rx * invDenom;
        ay += G * body.physics.mass * ry * invDenom;
    }

    return { ax, ay };
}