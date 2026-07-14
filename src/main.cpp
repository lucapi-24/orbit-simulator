#include <raylib.h>
#include "entities/CelestialBody.hpp"
#include "physics/GravityEngine.hpp"
#include "entities/Spaceship.hpp"
#include <vector>

int main(){
    const int screenWidth = 1000;
    const int screenHeight = 800;
    InitWindow(screenWidth, screenHeight, "Orbit Simulator");
    SetTargetFPS(60);

    std::vector<CelestialBody> bodies;

    bodies.push_back(CelestialBody("Sun", {500, 400}, {0.0f, 0.0f}, 100000, 30.0f, YELLOW));
    bodies.push_back(CelestialBody("Earth", {700, 400}, {0.0f, -70.3f}, 1.0, 12.0f, BLUE));
    bodies.push_back(CelestialBody("Asteroid", {710, 400}, {0.0f, -60.0f}, 0.1, 5.0f, BROWN));
    const Color deepSpaceColor = {0, 0, 20, 255}; // Dark blue color for deep space background

    Spaceship playerShip("Nave", {730.0f, 400.0f}, {0.0f, -27.0f}, 0.0001, 8.0f, RED);

    while (!WindowShouldClose()){
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f; // Cap delta time to avoid large jumps

        playerShip.HandleInput(dt);

        bodies.push_back(playerShip); // Añadimos la nave a la simulación para que sea afectada por la gravedad

        GravityEngine::UpdateOrbits(bodies, dt);
        GravityEngine::UpdateSpaceship(playerShip, bodies, dt);

        playerShip.physics = bodies.back().physics;
        bodies.pop_back();
        
        auto predictedOrbits = GravityEngine::PredictTrajectories(bodies, playerShip, 0.016f, 1000);
        BeginDrawing();
        ClearBackground(deepSpaceColor);


        for (size_t i = 0; i < bodies.size(); ++i) {
                if (bodies[i].name == "Sol") continue; // El sol está quieto, no necesita órbita

                // Usamos Fade para que la línea de la órbita sea un poco translúcida y quede elegante
                Color orbitColor = Fade(bodies[i].color, 0.4f);

                // DrawLineStrip toma un puntero a los datos del vector y los conecta con líneas continuas
                DrawLineStrip(predictedOrbits[i].data(), predictedOrbits[i].size(), orbitColor);
            }
        
        const auto& shipPoints = predictedOrbits.back();
        if (shipPoints.size() > 1) {
            for (size_t i = 0; i < shipPoints.size() - 1; ++i) {
                // El factor va de 1.0 (inicio en la nave) a 0.0 (el final del futuro simulado)
                float factor = 1.0f - ((float)i / shipPoints.size());
                
                // Multiplicamos por la opacidad máxima que queramos (ej. 0.6f)
                Color segmentColor = Fade(playerShip.color, 0.6f * factor);
                
                // Dibujamos un pequeño segmento entre el punto actual y el siguiente
                DrawLineV(shipPoints[i], shipPoints[i + 1], segmentColor);
            }
        }

        for (const auto& body : bodies) {
            body.Draw();
        }

        playerShip.Draw();
        DrawText("Simulador Orbital - Versión 0.1", 20, 20, 20, RAYWHITE);
        DrawText("Física Newtoniana Activa a 60 FPS", 20, 50, 16, GRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}