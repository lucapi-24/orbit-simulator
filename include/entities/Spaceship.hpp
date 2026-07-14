#pragma once
#include "entities/CelestialBody.hpp"

class Spaceship : public CelestialBody {
private:
    float fuel;
    float angle;
    float rotationSpeed;
    float thrustPower;
    bool isThrusting;
public:
    Spaceship(std::string name, Vector2 pos, Vector2 vel, double mass, float radius, Color color);

    // Métodos específicos de la nave
    void HandleInput(float dt);
    void Draw() const; // Sobrescribimos el método de dibujo para que pinte un triángulo en lugar de un círculo
    
    // Getters útiles
    float GetAngle() const { return angle; }
    bool IsThrusting() const { return isThrusting; }
    float GetFuel() const { return fuel; }
};    
