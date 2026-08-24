#include <raylib.h>
#include "entities/CelestialBody.hpp"
#include "physics/GravityEngine.hpp"
#include "entities/Spaceship.hpp"
#include <vector>
#include <cstdio>

int main() {
    const int screenWidth = 1000;
    const int screenHeight = 800;

    constexpr double FIXED_DT = 1.0 / 240.0;      // paso de RELOJ (input, acumulador)
    constexpr int MAX_PASOS = 5;
    constexpr double PX_PER_M = 1.0e-9;
    constexpr double TIME_SCALE = 600.0;          // aceleración temporal de la física
    constexpr double PHYS_DT = FIXED_DT * TIME_SCALE;  // paso de FÍSICA (2.5 s)

    InitWindow(screenWidth, screenHeight, "Orbit Simulator");
    Vec2d::renderScale = PX_PER_M;
    SetTargetFPS(60);

    std::vector<CelestialBody> bodies;

    // Constantes SI
    const double G = 6.67430e-11;
    const double M_SUN = 1.989e30;
    const double M_EARTH = 5.972e24;
    const double M_ASTEROID = 1e15;
    const double AU = 1.496e11;
    const double EARTH_RADIUS = 6.371e6;
    const double EARTH_ORBIT_V = std::sqrt(G * M_SUN / AU);  // ~29.78 km/s

    bodies.emplace_back("Sun", Vec2d{0, 0}, Vec2d{0, 0}, M_SUN, 6.96e8, YELLOW, true);
    bodies.emplace_back("Earth", Vec2d{AU, 0}, Vec2d{0, EARTH_ORBIT_V}, M_EARTH, EARTH_RADIUS, BLUE);
    bodies.emplace_back("Asteroid", Vec2d{AU + 2e9, 0}, Vec2d{0, EARTH_ORBIT_V * 0.95}, M_ASTEROID, 5e4, BROWN);

    const Color deepSpaceColor = {0, 0, 20, 255};

    // Nave en órbita baja terrestre (~400 km)
    const double LEO_R = EARTH_RADIUS + 400e3;
    const double LEO_V = std::sqrt(G * M_EARTH / LEO_R);
    Spaceship playerShip("Nave", 
        Vec2d{AU + LEO_R, 0}, 
        Vec2d{0, EARTH_ORBIT_V + LEO_V}, 
        1000.0, 8.0f, RED);

    Camera2D camera = {0};
    camera.target = playerShip.physics.position.toRaylib();
    camera.offset = {screenWidth / 2.0f, screenHeight / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    double accumulator = 0.0;

    while (!WindowShouldClose()) {
        // Cámara y zoom (exponencial: necesario para ver LEO desde escala solar)
        camera.zoom *= expf(GetMouseWheelMove() * 0.25f);
        camera.target = playerShip.physics.position.toRaylib();
        if (camera.zoom < 0.05) camera.zoom = 0.05;

        // Acumulador de tiempo real
        accumulator += GetFrameTime();
        if (accumulator > 0.1) accumulator = 0.1;

        int steps = 0;
        while (accumulator >= FIXED_DT && steps < MAX_PASOS) {
            playerShip.HandleInput(FIXED_DT);
            GravityEngine::UpdateOrbits(bodies, PHYS_DT);
            GravityEngine::UpdateSpaceship(playerShip, bodies, PHYS_DT);
            accumulator -= FIXED_DT;
            steps++;
        }

        auto predictedOrbits = GravityEngine::PredictTrajectories(bodies, playerShip, PHYS_DT, 1000);

        // Telemetría por consola (cada ~1 s real): verifica la física con números
        static int diagFrame = 0;
        if (++diagFrame % 60 == 0) {
            Vec2d relR = playerShip.physics.position - bodies[1].physics.position;
            Vec2d relV = playerShip.physics.velocity - bodies[1].physics.velocity;
            printf("zoom=%9.2f | nave-Tierra=%9.1f km | vRel=%7.1f m/s | Tierra-Sol=%.1f km\n",
                   camera.zoom, relR.length() / 1e3, relV.length(), bodies[1].physics.position.length() / 1e3);
        }
        float invZoom = 1.0f / camera.zoom;

        BeginDrawing();
        ClearBackground(deepSpaceColor);
        BeginMode2D(camera);

        // Órbitas predichas
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].isStatic) continue;

            Color orbitColor = Fade(bodies[i].color, 0.4f);
            std::vector<Vector2> drawPoints;
            drawPoints.reserve(predictedOrbits[i].size());
            for (const auto& p : predictedOrbits[i]) {
                drawPoints.push_back(p.toRaylib());
            }
            DrawLineStrip(drawPoints.data(), drawPoints.size(), orbitColor);
        }

        // Nave predicha
        const auto& shipPoints = predictedOrbits.back();
        if (shipPoints.size() > 1) {
            for (size_t i = 0; i < shipPoints.size() - 1; ++i) {
                float factor = 1.0f - static_cast<float>(i) / shipPoints.size();
                Color segmentColor = Fade(playerShip.color, 0.6f * factor);
                DrawLineV(shipPoints[i].toRaylib(), shipPoints[i + 1].toRaylib(), segmentColor);
            }
        }

        // Cuerpos y nave
        for (const auto& body : bodies) {
            body.Draw(invZoom);
        }
        playerShip.Draw(invZoom);

        EndMode2D();

        // UI
        float fuel = playerShip.GetFuel();
        DrawText("Orbit Simulator - SI units", 20, 20, 20, RAYWHITE);
        DrawRectangle(15, 65, 200, 25, GRAY);
        DrawRectangle(15, 65, static_cast<int>(fuel * 2), 25, GREEN);
        DrawRectangleLines(15, 65, 200, 25, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}