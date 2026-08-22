#include <raylib.h>
#include "entities/CelestialBody.hpp"
#include "physics/GravityEngine.hpp"
#include "entities/Spaceship.hpp"
#include <vector>
#include <cstdio>

int main(){
    const int screenWidth = 1000;
    const int screenHeight = 800;
    

    InitWindow(screenWidth, screenHeight, "Orbit Simulator");
    SetTargetFPS(60);

    std::vector<CelestialBody> bodies;

    bodies.push_back(CelestialBody("Sun", {500, 400}, {0.0f, 0.0f}, 100000, 30.0f, YELLOW, true));
    bodies.push_back(CelestialBody("Earth", {700, 400}, {0.0f, -70.3f}, 1.0, 12.0f, BLUE));
    bodies.push_back(CelestialBody("Asteroid", {710, 400}, {0.0f, -60.0f}, 0.1, 5.0f, BROWN));
    const Color deepSpaceColor = {0, 0, 20, 255}; // Dark blue color for deep space background

    Spaceship playerShip("Nave", {730.0f, 400.0f}, {0.0f, -27.0f}, 0.0001, 8.0f, RED);

    Camera2D camera = {0};
    camera.target = playerShip.physics.position;
    camera.offset = (Vector2){screenWidth / 2.0f, screenHeight / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    while (!WindowShouldClose()){
        camera.zoom += GetMouseWheelMove() * 0.05f;
        camera.target = playerShip.physics.position; // Mantener la cámara centrada en la nave
        if (camera.zoom < 0.1f) camera.zoom = 0.1f;
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f; // Cap delta time to avoid large jumps

        playerShip.HandleInput(dt);

        

        GravityEngine::UpdateOrbits(bodies, dt);
        GravityEngine::UpdateSpaceship(playerShip, bodies, dt);
        
        
        
        auto predictedOrbits = GravityEngine::PredictTrajectories(bodies, playerShip, 0.016f, 1000);
        BeginDrawing();
        ClearBackground(deepSpaceColor);
        BeginMode2D(camera);


        for (size_t i = 0; i < bodies.size(); ++i) {
                if (bodies[i].isStatic) continue;
                
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
        EndMode2D();
        float fuel = playerShip.GetFuel();
        DrawText("Simulador Orbital - Versión 0.1", 20, 20, 20, RAYWHITE);
        DrawRectangle(15, 65, 200, 25, GRAY);
        DrawRectangle(15, 65, (int)(fuel * 2), 25, GREEN);
        DrawRectangleLines(15, 65, 200, 25, WHITE);
        
        EndDrawing();
    }

    CloseWindow();
    return 0;
}