#include "physics/GravityEngine.hpp"
#include <raymath.h>
#include <cmath>

void GravityEngine::UpdateOrbits(std::vector<CelestialBody>& bodies, float dt) {
    int n = bodies.size();
    
    // Usamos Vector2 para almacenar las aceleraciones de este frame
    std::vector<Vector2> accelerations(n, Vector2{0.0f, 0.0f});
    std::vector<Vector2> accNew(n, Vector2{0.0f, 0.0f});

    // 1. Calcular la gravedad entre todos los cuerpos
    for (int i = 0; i < n; ++i) {
        const Vector2 acc = ComputeAcceleration(bodies[i].physics.position, bodies);
        accelerations[i] = acc;
    }

    // 2. Aplicar la aceleración integrada en el tiempo (dt)
    for (int i = 0; i < n; ++i) {
        // El Sol no se mueve, se queda clavado en el centro de la pantalla
        if (bodies[i].isStatic) continue;

        // x += v·dt + ½·a₀·dt²
        bodies[i].physics.position.x += bodies[i].physics.velocity.x * dt + 0.5f * accelerations[i].x * dt * dt;
        bodies[i].physics.position.y += bodies[i].physics.velocity.y * dt + 0.5f * accelerations[i].y * dt * dt;
    }

    for (int i = 0; i < n; ++i) {
        const Vector2 acc = ComputeAcceleration(bodies[i].physics.position, bodies);
        accNew[i] = acc;
    }

    for (int i = 0; i < n; ++i) {
        // El Sol no se mueve, se queda clavado en el centro de la pantalla
        if (bodies[i].isStatic) continue;

        // v += ½·(a₀ + a₁)·dt
        bodies[i].physics.velocity.x += 0.5f * (accelerations[i].x + accNew[i].x) * dt; // Usamos la aceleración promedio
        bodies[i].physics.velocity.y += 0.5f * (accelerations[i].y + accNew[i].y) * dt; // Usamos la aceleración promedio
    }
}

void GravityEngine::UpdateSpaceship(Spaceship& ship, const std::vector<CelestialBody>& bodies, float dt) {
    Vector2 acceleration = ComputeAcceleration(ship.physics.position, bodies);

    // Actualizamos la posición de la nave
    ship.physics.position.x += ship.physics.velocity.x * dt + 0.5f * acceleration.x * dt * dt;
    ship.physics.position.y += ship.physics.velocity.y * dt + 0.5f * acceleration.y * dt * dt;


    Vector2 accNew = ComputeAcceleration(ship.physics.position, bodies);

    // Aplicamos la aceleración gravitatoria a la velocidad de la nave
    ship.physics.velocity.x += (0.5f * (acceleration.x + accNew.x) * dt);
    ship.physics.velocity.y += (0.5f * (acceleration.y + accNew.y) * dt);
}

std::vector<std::vector<Vector2>> GravityEngine::PredictTrajectories(
    std::vector<CelestialBody> virtualBodies, 
    Spaceship ship, // <-- ¡Importante! Sin '&'
    float dt, 
    int steps
) {
    int n = virtualBodies.size() + 1; // planetas + nave
    std::vector<std::vector<Vector2>> trajectories(n);

    for (int i = 0; i < n; ++i) {
        trajectories[i].reserve(steps);
    }

    for (int step = 0; step < steps; ++step) {
        // 1. Calculamos la gravedad mutua de los planetas y los movemos
        UpdateOrbits(virtualBodies, dt);

        // 2. IMPORTANTE: Aplicamos la gravedad de los planetas a la nave "fantasma" y la movemos
        // Aquí debes replicar la física de la gravedad para la nave:
        const Vector2 acc = ComputeAcceleration(ship.physics.position, virtualBodies);
        ship.physics.velocity.x += acc.x * dt;
        ship.physics.velocity.y += acc.y * dt;
        // Actualizamos la posición de la nave virtual
        ship.physics.position.x += ship.physics.velocity.x * dt;
        ship.physics.position.y += ship.physics.velocity.y * dt;

        // 3. Guardamos las posiciones resultantes de los planetas
        for (size_t i = 0; i < virtualBodies.size(); ++i) {
            trajectories[i].push_back(virtualBodies[i].physics.position);
        }

        // 4. Guardamos la posición de la nave (que ahora sí tiene coordenadas distintas en cada paso)
        trajectories[n - 1].push_back(ship.physics.position); 
    }

    return trajectories;
}

Vector2 GravityEngine::ComputeAcceleration(const Vector2& pos, const std::vector<CelestialBody>& bodies) {
    static constexpr double EPSILON2 = 25.0; // softening de Plummer al cuadrado (ε = 5)

    double ax = 0.0;
    double ay = 0.0;

    for (const auto& body : bodies) {
        // r_vec: del punto que siente la gravedad, hacia el cuerpo que atrae
        double rx = (double)body.physics.position.x - pos.x;
        double ry = (double)body.physics.position.y - pos.y;

        // a = G·M·r_vec / (r² + ε²)^{3/2}
        double d = rx * rx + ry * ry + EPSILON2;   // (r² + ε²)
        double invDenom = 1.0 / (d * std::sqrt(d)); // 1/(r²+ε²)^{3/2}

        ax += G * body.physics.mass * rx * invDenom;
        ay += G * body.physics.mass * ry * invDenom;
    }

    return { (float)ax, (float)ay };
}
