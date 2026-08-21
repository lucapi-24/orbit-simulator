#include "physics/GravityEngine.hpp"
#include <raymath.h>
#include <cmath>

void GravityEngine::UpdateOrbits(std::vector<CelestialBody>& bodies, float dt) {
    int n = bodies.size();
    
    // Usamos Vector2 para almacenar las aceleraciones de este frame
    std::vector<Vector2> accelerations(n, Vector2{0.0f, 0.0f});

    // 1. Calcular la gravedad entre todos los cuerpos
    for (int i = 0; i < n; ++i) {
        // OPTIMIZACIÓN/REGLA: Si un cuerpo es un "Sol" masivo o estático, 
        // podemos decidir no aplicarle físicas para que no se mueva del centro.
    

        for (int j = 0; j < n; ++j) {
            if (i == j) continue; 

            const CelestialBody& bodyA = bodies[i];
            const CelestialBody& bodyB = bodies[j];

            // Vector dirección: de A hacia B
            double dx = (double)bodyB.physics.position.x - (double)bodyA.physics.position.x;
            double dy = (double)bodyB.physics.position.y - (double)bodyA.physics.position.y;
            
            // Distancia al cuadrado usando double para evitar perder precisión
            double distSq = (dx * dx) + (dy * dy);
            double distance = std::sqrt(distSq);

            // Evitar división por cero o distancias absurdamente pequeñas (colisiones estropean la física)
            if (distance < 5.0) continue; 

            // Fuerza/Aceleración de gravedad: a = (G * mB) / r^2
            double magnitude = (G * bodyB.physics.mass) / distSq;

            // Vector unitario de dirección (dx / distance, dy / distance) multiplicado por la magnitud
            double accX = (dx / distance) * magnitude;
            double accY = (dy / distance) * magnitude;

            // Acumulamos en el cuerpo A
            accelerations[i].x += (float)accX;
            accelerations[i].y += (float)accY;
        }
    }

    // 2. Aplicar la aceleración integrada en el tiempo (dt)
    for (int i = 0; i < n; ++i) {
        // El Sol no se mueve, se queda clavado en el centro de la pantalla
        if (bodies[i].name == "Sol") {
            bodies[i].physics.velocity = Vector2{0.0f, 0.0f};
            continue;
        }

        // v = v + a * dt
        bodies[i].physics.velocity.x += accelerations[i].x * dt;
        bodies[i].physics.velocity.y += accelerations[i].y * dt;

        // x = x + v * dt
        bodies[i].physics.position.x += bodies[i].physics.velocity.x * dt;
        bodies[i].physics.position.y += bodies[i].physics.velocity.y * dt;
    }
}

void GravityEngine::UpdateSpaceship(Spaceship& ship, const std::vector<CelestialBody>& bodies, float dt) {
    Vector2 totalForce = {0.0f, 0.0f};

    for (const auto& body : bodies) {
        Vector2 direction = Vector2Subtract(body.physics.position, ship.physics.position);
        float distance = Vector2Length(direction);

        if (distance < 5.0f) continue; // Evitar división por cero si colisionan

        // Ley de gravitación: F = G * (m1 * m2) / d^2
        // Nota: Como la masa de la nave no afecta la fuerza sobre sí misma en la aceleración (a = F/m),
        // simplificamos directamente a la aceleración provocada por el cuerpo celeste: a = G * M_cuerpo / d^2
        double forceMagnitude = (G * body.physics.mass) / (distance * distance);
        Vector2 forceDir = Vector2Normalize(direction);

        totalForce.x += forceDir.x * forceMagnitude;
        totalForce.y += forceDir.y * forceMagnitude;
    }

    // Aplicamos la aceleración gravitatoria a la velocidad de la nave
    ship.physics.velocity.x += totalForce.x * dt;
    ship.physics.velocity.y += totalForce.y * dt;

    // Actualizamos la posición de la nave
    ship.physics.position.x += ship.physics.velocity.x * dt;
    ship.physics.position.y += ship.physics.velocity.y * dt;
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
        for (const auto& body : virtualBodies) {
            Vector2 direction = Vector2Subtract(body.physics.position, ship.physics.position);
            float distance = Vector2Length(direction);
            if (distance > 5.0f) {
                // a = G * M_planeta / d^2
                double accMag = (10.0 * body.physics.mass) / (distance * distance); // Usa tu constante G real (aquí puse 10.0)
                Vector2 unitDir = Vector2Normalize(direction);
                
                ship.physics.velocity.x += unitDir.x * accMag * dt;
                ship.physics.velocity.y += unitDir.y * accMag * dt;
            }
        }
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
Vector2 ComputeAcceleration(const Vector2& pos, const std::vector<CelestialBody>& bodies){
    
}