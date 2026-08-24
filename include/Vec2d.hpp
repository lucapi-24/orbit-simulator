#pragma once
#include <cmath>
#include "raylib.h"

struct Vec2d {
    double x = 0.0;
    double y = 0.0;

    // Factor de conversión metros→píxeles (se define en main.cpp)
    inline static double renderScale = 1.0;

    // Constructores
    Vec2d() = default;
    Vec2d(double x_, double y_) : x(x_), y(y_) {}

    // Conversión a Vector2 de raylib aplicando la escala de render
    Vector2 toRaylib() const {
        return { float(x * renderScale), float(y * renderScale) };
    }

    // Operadores aritméticos — devuelven NUEVO Vec2d
    Vec2d operator+(const Vec2d& other) const { return { x + other.x, y + other.y }; }
    Vec2d operator-(const Vec2d& other) const { return { x - other.x, y - other.y }; }
    Vec2d operator*(double scalar) const { return { x * scalar, y * scalar }; }
    Vec2d operator/(double scalar) const { return { x / scalar, y / scalar }; }

    // Operadores compuestos — modifican this in-place
    Vec2d& operator+=(const Vec2d& other) { x += other.x; y += other.y; return *this; }
    Vec2d& operator-=(const Vec2d& other) { x -= other.x; y -= other.y; return *this; }
    Vec2d& operator*=(double scalar) { x *= scalar; y *= scalar; return *this; }
    Vec2d& operator/=(double scalar) { x /= scalar; y /= scalar; return *this; }

    // Utilidades
    double length() const { return std::sqrt(x * x + y * y); }
    double lengthSq() const { return x * x + y * y; }
    Vec2d normalized() const {
        double len = length();
        return (len > 0) ? *this / len : Vec2d{0, 0};
    }
};

// Multiplicación escalar por la izquierda: 2.5 * vec
inline Vec2d operator*(double scalar, const Vec2d& v) {
    return v * scalar;
}