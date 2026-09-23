#include <raylib.h>
#include "entities/CelestialBody.hpp"
#include "physics/GravityEngine.hpp"
#include "entities/Spaceship.hpp"
#include <vector>
#include <cstdio>
#include <iostream>

int main() {
    const int screenWidth = 1000;
    const int screenHeight = 800;

    constexpr double FIXED_DT = 1.0 / 240.0;      // paso de RELOJ (input, acumulador)
    constexpr int MAX_PASOS = 5;
    constexpr double VERLET_SAFETY = 0.2;         // max dt*omega para estabilidad de Verlet
    constexpr double PX_PER_M = 1.0e-9;
    static constexpr double WARP_TABLE[] = {1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0};
    int warpIndex = 0;
    int MAX_WARP = sizeof(WARP_TABLE) / sizeof(WARP_TABLE[0]) - 1;
    double PHYS_DT = FIXED_DT * WARP_TABLE[warpIndex];

    InitWindow(screenWidth, screenHeight, "Orbit Simulator");
    Vec2d::renderScale = PX_PER_M;
    SetTargetFPS(60);

    std::vector<CelestialBody> bodies;

    // Constantes SI
    const double G = 6.67430e-11;
    const double M_SUN = 1.989e30;
    const double M_EARTH = 5.972e24;
    const double M_ASTEROID = 1e15;
    const double M_MOON = 7.34767309e22;
    const double AU = 1.496e11;
    const double EARTH_RADIUS = 6.371e6;
    const double MOON_DIST = 384.4e6;
    const double EARTH_ORBIT_V = std::sqrt(G * M_SUN / AU);  // ~29.78 km/s
    const double MOON_ORBIT_V = std::sqrt(G * M_EARTH / MOON_DIST);  // ~1.02 km/s

    bodies.emplace_back("Sun", Vec2d{0, 0}, Vec2d{0, 0}, M_SUN, 6.96e8, YELLOW, true);
    bodies.emplace_back("Earth", Vec2d{AU, 0}, Vec2d{0, EARTH_ORBIT_V}, M_EARTH, EARTH_RADIUS, BLUE);
    bodies.emplace_back("Asteroid", Vec2d{AU + 2e9, 0}, Vec2d{0, EARTH_ORBIT_V * 0.95}, M_ASTEROID, 5e4, BROWN);
    bodies.emplace_back("Moon", Vec2d{AU + MOON_DIST, 0}, Vec2d{0, EARTH_ORBIT_V + MOON_ORBIT_V}, M_MOON, 1.737e6, GRAY);

    const Color deepSpaceColor = {0, 0, 20, 255};

    bodies[1].primary = &bodies[0];  // La Tierra orbita alrededor del Sol
    bodies[2].primary = &bodies[0];  // El asteroide orbita alrededor del Sol
    bodies[3].primary = &bodies[1];  // La Luna orbita alrededor de la Tierra

    for (auto& body : bodies) {
        body.UpdateSOIRadius();
    }

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
    OrbitInfo orbitInfo ;
    
    // Estado de la maniobra de Hohmann (objetivo: órbita de la Luna)
    bool hohmannActive = false;          // ignición 1 aplicada, esperando dónde circularizar
    double hohmannDv2 = 0.0;             // Δv de circularización (ignición 2)
    std::vector<Vec2d> transferConic;    // cónica de transferencia (coords. relativas al primario)
    Vec2d burn1Pos{0, 0};                // punto de ignición 1 (relativo al primario)
    playerShip.primary = GravityEngine::GetDominantBody(playerShip.physics.position, bodies);
    playerShip.UpdateSOIRadius();
    while (!WindowShouldClose()) {
        // Cámara y zoom (exponencial: necesario para ver LEO desde escala solar)
        camera.zoom *= expf(GetMouseWheelMove() * 0.25f);
        camera.target = playerShip.physics.position.toRaylib();
        if (camera.zoom < 0.05) camera.zoom = 0.05;

        if (IsKeyPressed(KEY_PERIOD)) {           // subir
            int next = warpIndex + 1;
            bool altaSOI = (WARP_TABLE[next] > 100000) &&          // niveles 6-7 (dt 417s+)
                        playerShip.primary &&
                        playerShip.primary != &bodies[0];        // dentro de una SOI planetaria
            if (next <= MAX_WARP && !altaSOI){
                warpIndex = next;
            }
        }
        if (IsKeyPressed(KEY_COMMA) && warpIndex > 0) warpIndex--;  
        
        PHYS_DT = FIXED_DT * WARP_TABLE[warpIndex];

        // ── Botón Hohmann → Luna (solo dentro de la SOI terrestre) ──
        const Rectangle hohBtn = {screenWidth - 150.0f, screenHeight - 50.0f, 130.0f, 34.0f};
        bool hohHover = CheckCollisionPointRec(GetMousePosition(), hohBtn);
        bool canHohmann = playerShip.primary == &bodies[1];
        if (hohHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && canHohmann && !hohmannActive) {
            Vec2d rRel  = playerShip.physics.position - playerShip.primary->physics.position;
            Vec2d vRel  = playerShip.physics.velocity - playerShip.primary->physics.velocity;
            double mu   = G * playerShip.primary->physics.mass;
            double r1   = rRel.length();
            double r2   = MOON_DIST;
            double vc1  = std::sqrt(mu / r1);
            double vpT  = vc1 * std::sqrt((2.0 * r2) / (r1 + r2));
            double dv1  = vpT - vc1;
            double vc2  = std::sqrt(mu / r2);
            double vaT  = vc2 * std::sqrt((2.0 * r1) / (r1 + r2));
            hohmannDv2  = vc2 - vaT;

            Vec2d vHat = vRel * (1.0 / vRel.length());
            playerShip.ApplyImpulse(dv1, vHat);
            burn1Pos = rRel;
            transferConic = GravityEngine::PredictConic(rRel, vRel + vHat * dv1, mu,
                                                        Vec2d{0, 0}, 720);
            hohmannActive = true;
        }
        // Previsualización de la transferencia al pasar el ratón por el botón
        std::vector<Vec2d> previewConic;
        if (hohHover && canHohmann && !hohmannActive) {
            Vec2d rRel  = playerShip.physics.position - playerShip.primary->physics.position;
            Vec2d vRel  = playerShip.physics.velocity - playerShip.primary->physics.velocity;
            double mu   = G * playerShip.primary->physics.mass;
            double r1   = rRel.length();
            double r2   = MOON_DIST;
            double vc1  = std::sqrt(mu / r1);
            double dv1  = vc1 * std::sqrt((2.0 * r2) / (r1 + r2)) - vc1;
            Vec2d vHat  = vRel * (1.0 / vRel.length());
            previewConic = GravityEngine::PredictConic(rRel, vRel + vHat * dv1, mu,
                                                       Vec2d{0, 0}, 720);
        }

        // Acumulador de tiempo real
        accumulator += GetFrameTime();
        if (accumulator > 0.1) accumulator = 0.1;

        int steps = 0;
        while (accumulator >= FIXED_DT && steps < MAX_PASOS) {
            playerShip.HandleInput(FIXED_DT);

            // Substepping: PHYS_DT completo (warp) se divide en pasos estables.
            // N = ceil(PHYS_DT * omegaMax / VERLET_SAFETY) => h_sub*omegaMax <= VERLET_SAFETY.
            double omegaMax = 0.0;
            for (const auto& body : bodies) {
                double d = (playerShip.physics.position - body.physics.position).length();
                if (d < 1.0) d = 1.0;
                double omega = std::sqrt(G * body.physics.mass / (d * d * d));
                if (omega > omegaMax) omegaMax = omega;
            }
            int nSub = static_cast<int>(std::ceil(PHYS_DT * omegaMax / VERLET_SAFETY));
            if (nSub < 1) nSub = 1;
            double hSub = PHYS_DT / nSub;

            for (int k = 0; k < nSub; ++k) {
                GravityEngine::UpdateAll(bodies, playerShip, hSub);
            }

            playerShip.primary = GravityEngine::GetDominantBody(playerShip.physics.position, bodies);  // Actualiza la SOI de la nave
            playerShip.UpdateSOIRadius();
            orbitInfo = GravityEngine::ComputeOrbitInfo(
                playerShip.physics.position - playerShip.primary->physics.position,
                playerShip.physics.velocity - playerShip.primary->physics.velocity,
                G * playerShip.primary->physics.mass);

            // Ignición 2 (circularización) automática al alcanzar |r_rel| ≈ r2
            if (hohmannActive) {
                double rr = (playerShip.physics.position - playerShip.primary->physics.position).length();
                if (rr > 0.99 * MOON_DIST) {
                    Vec2d vRel = playerShip.physics.velocity - playerShip.primary->physics.velocity;
                    Vec2d vHat = vRel * (1.0 / vRel.length());
                    playerShip.ApplyImpulse(hohmannDv2, vHat);
                    hohmannActive = false;
                    transferConic.clear();
                }
            }
            
            accumulator -= FIXED_DT;
            steps++;
        }

       
        // ── Cálculo de cónicas predichas (datos puros, sin dibujar) ──
        std::vector<Vec2d> shipConic;
        if (playerShip.primary) {
            Vec2d shipRelR = playerShip.physics.position - playerShip.primary->physics.position;
            Vec2d shipRelV = playerShip.physics.velocity - playerShip.primary->physics.velocity;
            double muShip = G * playerShip.primary->physics.mass;
            shipConic = GravityEngine::PredictConic(shipRelR, shipRelV, muShip,
                                                    playerShip.primary->physics.position, 720);
        }

        std::vector<std::vector<Vec2d>> bodyConics(bodies.size());
        for (size_t i = 0; i < bodies.size(); ++i) {
            const auto& body = bodies[i];
            if (body.isStatic || !body.primary) continue;   // Sun no tiene primary
            Vec2d bRelR = body.physics.position - body.primary->physics.position;
            Vec2d bRelV = body.physics.velocity - body.primary->physics.velocity;
            double muB = G * body.primary->physics.mass;
            bodyConics[i] = GravityEngine::PredictConic(bRelR, bRelV, muB,
                                                        body.primary->physics.position, 360);
        }

        // Telemetría por consola (cada ~1 s real): verifica la física con números
        static int diagFrame = 0;
        if (++diagFrame % 60 == 0) {
            Vec2d relR = playerShip.physics.position - bodies[1].physics.position;
            Vec2d relV = playerShip.physics.velocity - bodies[1].physics.velocity;
            printf("SOI: %s | ", playerShip.primary ? playerShip.primary->name.c_str() : "N/A");
            printf("zoom=%9.2f | nave-Tierra=%9.1f km | vRel=%7.1f m/s | Tierra-Sol=%.1f km\n",
                   camera.zoom, relR.length() / 1e3, relV.length(), bodies[1].physics.position.length() / 1e3);
            printf("  Orbita: a=%.1f km | e=%.4f | rp=%.1f km | ra=%.1f km | T=%.1f h | eps=%+.3e J/kg | h=%.3e m^2/s\n",
                   orbitInfo.a / 1e3, orbitInfo.e, orbitInfo.rp / 1e3, orbitInfo.ra / 1e3, orbitInfo.T / 3600.0,
                   orbitInfo.eps, orbitInfo.h);
        }
        float invZoom = 1.0f / camera.zoom;

        BeginDrawing();
        ClearBackground(deepSpaceColor);
        Vector2 origin = playerShip.primary ?
        playerShip.primary->physics.position.toRaylib() : Vector2{0, 0};

        Vector2 shipOffset = {
            playerShip.physics.position.toRaylib().x - origin.x,
            playerShip.physics.position.toRaylib().y - origin.y
        };
        camera.target = shipOffset;
        
        BeginMode2D(camera);

        // Cónicas predichas (solo dibujo, datos ya calculados arriba)
        std::vector<Vector2> pts;
        pts.reserve(shipConic.size());
        for (const auto& p : shipConic) {
            pts.push_back({ p.toRaylib().x - origin.x, p.toRaylib().y - origin.y });
        }
        if (pts.size() > 1) DrawLineStrip(pts.data(), pts.size(), Fade(playerShip.color, 0.5f));

        // Cónica de transferencia de Hohmann (relativa al primario; el origen ya está centrado en él)
        const std::vector<Vec2d>* conicPtr = nullptr;
        if (hohmannActive) conicPtr = &transferConic;
        else if (!previewConic.empty()) conicPtr = &previewConic;
        if (conicPtr) {
            std::vector<Vector2> tPts;
            tPts.reserve(conicPtr->size());
            for (const auto& p : *conicPtr) tPts.push_back(p.toRaylib());
            if (tPts.size() > 1) DrawLineStrip(tPts.data(), tPts.size(), Fade(GREEN, 0.6f));
            if (hohmannActive) {
                DrawCircleLinesV(burn1Pos.toRaylib(), 6.0f * invZoom, GREEN);      // ignición 1
                Vec2d apo = (*conicPtr)[0];                                          // burn 2 ≈ apoyo más lejano
                for (const auto& p : *conicPtr) if (p.length() > apo.length()) apo = p;
                DrawCircleLinesV(apo.toRaylib(), 6.0f * invZoom, GREEN);
            }
        }

        for (size_t i = 0; i < bodies.size(); ++i) {
            std::vector<Vector2> bPts;
            bPts.reserve(bodyConics[i].size());
            for (const auto& p : bodyConics[i]) {
                bPts.push_back({ p.toRaylib().x - origin.x, p.toRaylib().y - origin.y });
            }
            if (bPts.size() > 1) DrawLineStrip(bPts.data(), bPts.size(), Fade(bodies[i].color, 0.4f));
        }

        // Cuerpos y nave
        for (const auto& body : bodies) {
            body.Draw(invZoom, origin);
        }
        playerShip.Draw(invZoom, origin);

        EndMode2D();

        // UI
        float fuel = playerShip.GetFuel();
        DrawText("Orbit Simulator - SI units", 20, 20, 20, RAYWHITE);
        if (orbitInfo.T / PHYS_DT < 50) {
            DrawText(TextFormat("Órbitas inestables, baja warp"), 20, 45, 20, RED);
        } 
        DrawRectangle(15, 65, 200, 25, GRAY);
        DrawRectangle(15, 65, static_cast<int>(fuel * 2), 25, GREEN);
        DrawRectangleLines(15, 65, 200, 25, WHITE);

        // Botón Hohmann → Luna
        Color btnColor = canHohmann ? (hohHover ? LIME : GREEN) : DARKGRAY;
        DrawRectangleRec(hohBtn, btnColor);
        DrawRectangleLinesEx(hohBtn, 2, WHITE);
        DrawText(hohmannActive ? "Hohmann activa" : "Hohmann -> Luna",
                 hohBtn.x + 8, hohBtn.y + 9, 14, BLACK);

        // Panel de telemetría orbital
        int px = screenWidth - 320;
        int py = 20;
        int lh = 20;  // line height

        DrawRectangle(px - 10, py - 5, 310, 8 * lh + 10, Fade(BLACK, 0.7f));

        DrawText(TextFormat("Cuerpo: %s", playerShip.primary->name.c_str()), px, py, 18, YELLOW);
        DrawText(TextFormat("Altitud: %.0f km", (orbitInfo.r - playerShip.primary->radiusM) / 1e3), px, py + lh, 18, RAYWHITE);
        DrawText(TextFormat("vRel: %.1f m/s", orbitInfo.v), px, py + lh*2, 18, RAYWHITE);
        DrawText(TextFormat("Pericentro: %.0f km", orbitInfo.rp / 1e3), px, py + lh*3, 18, GREEN);
        DrawText(TextFormat("Apocentro: %.0f km", orbitInfo.ra / 1e3), px, py + lh*4, 18, orbitInfo.hyperbolic ? RED : GREEN);
        DrawText(TextFormat("Excentricidad: %.4f", orbitInfo.e), px, py + lh*5, 18, RAYWHITE);
        DrawText(TextFormat("Periodo: %.1f min", orbitInfo.T / 60.0), px, py + lh*6, 18, RAYWHITE);
        DrawText(TextFormat("Energia: %.2e J/kg", orbitInfo.eps), px, py + lh*7, 18, RAYWHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}