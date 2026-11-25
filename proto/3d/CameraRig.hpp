#pragma once

#include "raylib.h"      // Vector3, Quaternion, Camera3D
#include <raymath.h>     // Vector3Normalize, Vector3Subtract, QuaternionInvert...
#include <functional>    // std::function



// Lo que la cámara necesita del mundo. `main` rellena los dos callbacks porque
// él sí conoce el terreno (Mesh, ruido) y la cámara no debe conocerlo.
struct CameraEnv {
    Vector3     spherePos;
    float       sphereR;
    Quaternion  spin;               // q — para pasar mundial → local en el raycast

    // Cada frame, en el clamp. Recibe una dirección MUNDIAL.
    std::function<float(Vector3)> groundApprox;

    // Solo al pulsar F, en el aterrizaje. Recibe una dirección LOCAL (sin rotar).
    std::function<float(Vector3)> groundExact;
};

enum class CamMode { FLY, GROUND };

struct CameraRig {
    static constexpr float EYE_HEIGHT = 0.0017f;   // 1.7 m (la unidad es 1 km)
    static constexpr float GRAVITY    = 9.81e-3f;  // km/s² (= 9.81 m/s²)
    static constexpr float JUMP_SPEED = 0.01f;     // km/s (~5 m de salto, con g real)
    static constexpr float WALK_SPEED = 0.002f;    // km/s (2 m/s)

    Camera3D cam;
    float yaw, pitch, moveSpeed;
    CamMode mode = CamMode::FLY;
    Vector3 vel{0, 0, 0};
    bool grounded = false;
    Vector3 tNorth{0, 0, 1};   // eje tangente "norte" del marco local (modo GROUND)

    void EnterGround(const CameraEnv& env, Vector3 lookDir);

    void Init(Vector3 pos, Vector3 target);
    void Update(float dt, const CameraEnv& env);
};

// Proyecta v sobre el plano tangente de 'up' y normaliza
inline Vector3 TangentDir(Vector3 v, Vector3 up) {
    Vector3 t = Vector3Subtract(v, Vector3Scale(up, Vector3DotProduct(v, up)));
    if (Vector3Length(t) < 1e-4f) t = Vector3Subtract({1, 0, 0}, Vector3Scale(up, up.x));
    return Vector3Normalize(t);
}

// Pasa a GROUND en la posición actual mirando hacia lookDir (proyectado en el suelo para el yaw)
void CameraRig::EnterGround(const CameraEnv& env, Vector3 lookDir) {
    mode = CamMode::GROUND;
    vel = {0, 0, 0};
    Vector3 up   = Vector3Normalize(Vector3Subtract(cam.position, env.spherePos));
    tNorth = TangentDir(lookDir, up);
    Vector3 east = Vector3CrossProduct(up, tNorth);
    Vector3 d    = Vector3Normalize(lookDir);
    yaw   = atan2f(Vector3DotProduct(d, east), Vector3DotProduct(d, tNorth));
    pitch = asinf(Clamp(Vector3DotProduct(d, up), -1.0f, 1.0f));
}

void CameraRig::Init(Vector3 pos, Vector3 target) {
    cam.position = pos;
    cam.target = target;
    cam.up = {0, 1, 0};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    yaw   = atan2f(fwd.x, fwd.z);
    pitch = asinf(fwd.y);
    moveSpeed = 150.0f;                       // km/s base
}

void CameraRig::Update(float dt, const CameraEnv& env) {
    // ── Rotación: ratón bloqueado al centro → delta sin tocar bordes ──
    Vector2 md = GetMouseDelta();
    yaw   -= md.x * 0.0025f;
    pitch -= md.y * 0.0025f;
    if (pitch >  1.55f) pitch =  1.55f;      // ±~89° (no girar de cabeza)
    if (pitch < -1.55f) pitch = -1.55f;
    float cp = cosf(pitch), sp = sinf(pitch);
    Vector3 fwd = {cp * sinf(yaw), sp, cp * cosf(yaw)};   // hacia dónde mira

    // ── Movimiento ──
    if (mode == CamMode::FLY) {
        // Rueda: velocidad escalada al radio del planeta
        moveSpeed *= 1.0f + GetMouseWheelMove() * 0.5f;
        if (moveSpeed < 1.0f) moveSpeed = 1.0f;
        if (moveSpeed > env.sphereR * 20.0f) moveSpeed = env.sphereR * 20.0f;
        float step = moveSpeed * dt;

        // WASD relativo a la vista + Q/E bajar/subir
        Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, {0, 1, 0}));
        if (IsKeyDown(KEY_W)) cam.position = Vector3Add(cam.position, Vector3Scale(fwd, step));
        if (IsKeyDown(KEY_S)) cam.position = Vector3Subtract(cam.position, Vector3Scale(fwd, step));
        if (IsKeyDown(KEY_D)) cam.position = Vector3Add(cam.position, Vector3Scale(right, step));
        if (IsKeyDown(KEY_A)) cam.position = Vector3Subtract(cam.position, Vector3Scale(right, step));
        if (IsKeyDown(KEY_E)) cam.position.y += step;
        if (IsKeyDown(KEY_Q)) cam.position.y -= step;

        cam.target = Vector3Add(cam.position, fwd);
        cam.up = {0, 1, 0};

        // ── Colisión: no dejar la cámara dentro del planeta ──
        {
            Vector3 rel  = Vector3Subtract(cam.position, env.spherePos);
            Vector3 relN = Vector3Normalize(rel);
            float ground = env.groundApprox(relN);          // mundial
            if (Vector3Length(rel) < ground + EYE_HEIGHT)
                cam.position = Vector3Add(env.spherePos, Vector3Scale(relN, ground + EYE_HEIGHT));
        }
    } else if (mode == CamMode::GROUND) {
        Vector3& pos = cam.position;

        // Marco local: 'up' radial y 'tNorth' transportado (sin saltos al cruzar meridianos)
        Vector3 up = Vector3Normalize(Vector3Subtract(pos, env.spherePos));
        tNorth = TangentDir(tNorth, up);
        Vector3 east  = Vector3CrossProduct(up, tNorth);
        Vector3 fwdH  = Vector3Add(Vector3Scale(east, sinf(yaw)), Vector3Scale(tNorth, cosf(yaw)));
        Vector3 right = Vector3CrossProduct(fwdH, up);   // mismo convenio que en FLY

        Vector3 move = {0, 0, 0};
        if (IsKeyDown(KEY_W)) move = Vector3Add(move, fwdH);
        if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, fwdH);
        if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
        if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
        if (Vector3Length(move) > 0.0f)
            pos = Vector3Add(pos, Vector3Scale(Vector3Normalize(move), WALK_SPEED * dt));
        
        if (grounded && IsKeyPressed(KEY_SPACE)) {
            vel = Vector3Add(vel, Vector3Scale(up, JUMP_SPEED)); // O vel += up * JUMP_SPEED si tienes operadores
        }

        vel = Vector3Add(vel, Vector3Scale(up, -GRAVITY * dt));          // gravedad
        pos = Vector3Add(pos, Vector3Scale(vel, dt));

        Vector3 relN = Vector3Normalize(Vector3Subtract(pos, env.spherePos));
        float ground = env.groundApprox(relN);
        
        if (Vector3Length(Vector3Subtract(pos, env.spherePos)) <= ground + EYE_HEIGHT) {
            pos = Vector3Add(env.spherePos, Vector3Scale(relN, ground + EYE_HEIGHT));
            
            // Usamos Vector3DotProduct en lugar de Vector3One
            float vr = Vector3DotProduct(vel, relN); 
            
            if (vr < 0) {
                vel = Vector3Subtract(vel, Vector3Scale(relN, vr)); // solo cancela lo que entra al suelo
            }
            grounded = true;
        } else {
            grounded = false;
        }

        Vector3 fwdG = Vector3Add(Vector3Scale(fwdH, cosf(pitch)), Vector3Scale(up, sinf(pitch)));
        cam.up     = up;
        cam.target = Vector3Add(pos, fwdG);
    }
    

    // ── F: aterrizar en la superficie y mirar al horizonte ──
    if (IsKeyPressed(KEY_F)) {
        if (mode == CamMode::FLY) {
            mode = CamMode::GROUND;
            Vector3 up = Vector3Normalize(Vector3Subtract(cam.position, env.spherePos));

            // Colisión EXACTA contra la malla real (ray-triangle, O(triángulos)):
            // la dirección se lleva al marco local (rotación inversa del render).
            Vector3 upLocal = Vector3Normalize(Vector3RotateByQuaternion(up, QuaternionInvert(env.spin)));
            float ground = env.groundExact(upLocal);        // local, el * sphereR va en el lambda

            cam.position = Vector3Add(env.spherePos, Vector3Scale(up, ground + EYE_HEIGHT));
            EnterGround(env, TangentDir({0, 0, 1}, up));
        } else {
            mode = CamMode::FLY;
        }
        
        
    }
}
