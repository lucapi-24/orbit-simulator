#pragma once
#include "raylib.h"
#include "Vec2d.hpp"

struct PhysicalState {
    Vec2d position;
    Vec2d velocity;
    double mass;
};