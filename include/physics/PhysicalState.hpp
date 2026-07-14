#pragma once
#include "raylib.h"

struct PhysicalState {
    Vector2 position;
    Vector2 velocity;
    double mass;
};