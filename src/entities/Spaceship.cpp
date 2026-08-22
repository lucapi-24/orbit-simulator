#include "entities/Spaceship.hpp"
#include "raymath.h"
#include <cmath>

Spaceship::Spaceship(std::string name, Vector2 pos, Vector2 vel, double mass, float radius, Color color)
    : CelestialBody(name, pos, vel, mass, radius, color),
      fuel(100.0f),            // Combustible inicial
      angle(-PI / 2.0f),       // Apuntando hacia arriba por defecto
      rotationSpeed(4.0f),     // Giro ágil
      thrustPower(150.0f),     // Potencia del propulsor
      isThrusting(false) {}

void Spaceship::HandleInput(float dt) {
    // Rotación de la nave
    if (IsKeyDown(KEY_LEFT) or IsKeyDown(KEY_A)) {
        angle -= rotationSpeed * dt;
    }
    if (IsKeyDown(KEY_RIGHT) or IsKeyDown(KEY_D)) {
        angle += rotationSpeed * dt;
    }

    // Propulsión
    isThrusting = (IsKeyDown(KEY_UP) or IsKeyDown(KEY_W)) && fuel > 0.0f;
    if (isThrusting) {
        // Calculamos la dirección del empuje basado en el ángulo actual
        Vector2 thrustDirection = { std::cos(angle), std::sin(angle) };
        physics.velocity.x += thrustDirection.x * thrustPower * dt;
        physics.velocity.y += thrustDirection.y * thrustPower * dt;

        // Consumo de combustible
        fuel -= 10.0f * dt; // Consumo de combustible por segundo
        if (fuel < 0.0f) fuel = 0.0f; // Evitar combustible negativo
    }
}

void Spaceship::Draw() const{
    Vector2 pos = physics.position;
    float size = radius * 1.8f; // Escalamos visualmente según el radio físico asignado

    Vector2 v1 = { pos.x + cosf(angle) * size, pos.y + sinf(angle) * size };
    Vector2 v2 = { pos.x + cosf(angle + 2.5f) * (size * 0.8f), pos.y + sinf(angle + 2.5f) * (size * 0.8f) };
    Vector2 v3 = { pos.x + cosf(angle - 2.5f) * (size * 0.8f), pos.y + sinf(angle - 2.5f) * (size * 0.8f) };

    DrawTriangle(v1, v2, v3, color);
    DrawTriangleLines(v1, v2, v3, WHITE);

    // Si el motor está activo, dibujamos la llama de propulsión
    if (isThrusting) {
        Vector2 firePos = {
            pos.x - cosf(angle) * (size * 0.9f),
            pos.y - sinf(angle) * (size * 0.9f)
        };
        DrawCircleV(firePos, size * 0.4f, ORANGE);
    }
}   