#pragma once
#include "entities/CelestialBody.hpp"
#include "Vec2d.hpp"

class Spaceship : public CelestialBody {
private:
    float fuel;
    float angle;
    float rotationSpeed;
    float thrustPower;
    bool isThrusting;
public:
    Spaceship(std::string name, Vec2d pos, Vec2d vel, double mass, float radius, Color color);

    // Métodos específicos de la nave
    void HandleInput(double dt);
    void Draw(float invZoom) const; // Triángulo de tamaño constante en pantalla
    
    // Getters útiles
    float GetAngle() const { return angle; }
    bool IsThrusting() const { return isThrusting; }
    float GetFuel() const { return fuel; }
};    
