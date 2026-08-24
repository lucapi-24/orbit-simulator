#include "entities/Spaceship.hpp"
#include "Vec2d.hpp"
#include <cmath>

Spaceship::Spaceship(std::string name, Vec2d pos, Vec2d vel, double mass, float radius, Color color)
    : CelestialBody(name, pos, vel, mass, radius, color),
      fuel(100.0f),
      angle(-PI / 2.0f),
      rotationSpeed(4.0f),
      thrustPower(200.0),  // m/s^2 - ajustado a unidades SI
      isThrusting(false) {}

void Spaceship::HandleInput(double dt) {
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
        Vec2d thrustDirection = { std::cos(angle), std::sin(angle) };
        physics.velocity += thrustDirection * thrustPower * dt;

        fuel -= 10.0 * dt;
        if (fuel < 0.0) fuel = 0.0;
    }
}

void Spaceship::Draw(float invZoom) const {
    Vector2 c = physics.position.toRaylib();  // centro en píxeles-mundo (UNA sola conversión)
    float size = 14.0f * invZoom;             // tamaño constante en pantalla

    Vector2 v1 = { c.x + std::cos(angle) * size,           c.y + std::sin(angle) * size };
    Vector2 v2 = { c.x + std::cos(angle + 2.5f) * size * 0.8f, c.y + std::sin(angle + 2.5f) * size * 0.8f };
    Vector2 v3 = { c.x + std::cos(angle - 2.5f) * size * 0.8f, c.y + std::sin(angle - 2.5f) * size * 0.8f };

    DrawTriangle(v1, v2, v3, color);
    DrawTriangleLines(v1, v2, v3, WHITE);

    if (isThrusting) {
        Vector2 firePos = { c.x - std::cos(angle) * (size * 0.9f), c.y - std::sin(angle) * (size * 0.9f) };
        DrawCircleV(firePos, size * 0.4f, ORANGE);
    }
}   