#pragma once
#include "raylib.h"
#include "raymath.h"    // Vector3Scale: raylib.h NO lo trae, hay que incluirlo
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// Ruido de gradiente 3D (Perlin) + fBm auto-afín, evaluado sobre la DIRECCIÓN
// del vértice (vector unitario) en lugar de sobre UVs.
//
// Esto reemplaza a HeightAt/NoiseAt: al vivir en 3D no hay rejilla 2D que
// delate la estructura, ni costura de UV, ni compresión en los polos. La misma
// función alimenta color, desplazamiento, colisión y (futuro) chunk local.
//
// ESCALA: si el ruido se evalúa en dir*f, una celda mide R/f en la superficie.
//   f = 2      → λ = 3000 km  (montaña continental)
//   f = 131072 → λ = 45 m     (pedrusco)
//
// AMPLITUD: a_k = A * gain^k, gain = 0.5 → razón amplitud/tamaño constante
// (fractal auto-afín, la estadística del relieve real de la Tierra).
// ─────────────────────────────────────────────────────────────────────────────

// Configuración del planeta. Todo en km (1 unidad = 1 km).
static const float PLANET_R      = 6000.0f;
static const float FREC_BASE     = 2.0f;      // λ0 = R/f = 3000 km
static const int   FBM_OCTAVES   = 17;        // hasta λ ≈ 45 m
static const float FBM_GAIN      = 0.5f;      // auto-afín
static const float FBM_LACUN     = 2.0f;      // cada octava duplica frecuencia

static const float AMP_BASE      = 9.0f;      // elevación objetivo en tierra
static const float AMP_MACRO     = 2.5f;      // boost de las montañas grandes
static const int   AMP_MACRO_OCT = 3;         // (primeras 3 octavas)
// Valores calibrados con noise_test (TestCalibracion barrido). No tocar a ojo:
// si cambias FBM_OCTAVES, AMP_BASE o FREC_BASE, hay que re-barrer.
static const float NORMALIZACION = 0.4220f;   // escala el relieve → pico de 9 km
static const float OCEAN_DEPTH   = 1.0f;      // fondo medio bajo el nivel del mar
static const float CONT_BIAS     = 0.25f;     // → 28.2% de tierra (Tierra real 29%)

static const int   CONTINENT_OCT = 4;         // continentes: baja frecuencia
static const float CONTINENT_FREC = 1.3f;
static const unsigned int SEED    = 1337u;

// SmoothStep: reimplementado aquí para que el header sea autónomo (la copia
// de main3d.cpp es static y no visible desde fuera).
static inline float SmoothStep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

// ── Hash entero 3D → [0,1). Sin sin() a propósito: sin() es impreciso en
//    float32 y cambia de máquina a máquina. Aritmética entera, determinista.
static inline float Hash3(int x, int y, int z) {
    unsigned int h = (unsigned int)x * 374761393u
                   + (unsigned int)y * 668265263u
                   + (unsigned int)z * 2147483647u
                   + SEED * 1013904223u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return (float)(h & 0x00FFFFFFu) / 16777216.0f;
}

// ── Los 12 gradientes: aristas de un cubo. Conjunto clásico de Perlin; son
//    suficientes para que la suma de offsets no se anule y no hace falta
//    aleatorizar la longitud.
static inline Vector3 Gradient(int x, int y, int z, int dx, int dy, int dz) {
    int h = (int)(Hash3(x + dx, y + dy, z + dz) * 12.0f);
    if (h > 11) h = 11;
    switch (h) {
        case  0: return { 1,  1,  0};
        case  1: return {-1,  1,  0};
        case  2: return { 1, -1,  0};
        case  3: return {-1, -1,  0};
        case  4: return { 1,  0,  1};
        case  5: return {-1,  0,  1};
        case  6: return { 1,  0, -1};
        case  7: return {-1,  0, -1};
        case  8: return { 0,  1,  1};
        case  9: return { 0, -1,  1};
        case 10: return { 0,  1, -1};
        default: return { 0, -1, -1};
    }
}

static inline float Fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);   // 6t⁵-15t⁴+10t³
}

// ── Perlin 3D. Rango teórico ±√3/2, práctico ~±0.7.
static inline float Noise3D(Vector3 p) {
    int xi = (int)floorf(p.x), yi = (int)floorf(p.y), zi = (int)floorf(p.z);
    float xf = p.x - xi, yf = p.y - yi, zf = p.z - zi;

    // El degradado hace que las derivadas se anulen en los 8 vértices: sin esto
    // el campo revela la rejilla al ampliar.
    float u = Fade(xf), v = Fade(yf), w = Fade(zf);

    #define DOT(dx, dy, dz) ( \
        Gradient(xi, yi, zi, dx, dy, dz).x * (xf - dx) + \
        Gradient(xi, yi, zi, dx, dy, dz).y * (yf - dy) + \
        Gradient(xi, yi, zi, dx, dy, dz).z * (zf - dz))

    float n000 = DOT(0, 0, 0), n100 = DOT(1, 0, 0);
    float n010 = DOT(0, 1, 0), n110 = DOT(1, 1, 0);
    float n001 = DOT(0, 0, 1), n101 = DOT(1, 0, 1);
    float n011 = DOT(0, 1, 1), n111 = DOT(1, 1, 1);
    #undef DOT

    // Interpolación trilineal
    float nx00 = n000 + u * (n100 - n000);
    float nx10 = n010 + u * (n110 - n010);
    float nx01 = n001 + u * (n101 - n001);
    float nx11 = n011 + u * (n111 - n011);
    float nxy0 = nx00 + v * (nx10 - nx00);
    float nxy1 = nx01 + v * (nx11 - nx01);

    return nxy0 + w * (nxy1 - nxy0);
}

// ── Altura en km respecto al nivel del mar. Positiva = tierra, negativa = mar.
//    IMPORTANTE: la mezcla océano/tierra es CONTINUA (sin early-return). Un
//    if que salta de rama produce un acantilado de km justo en la costa.
//
//    contBias y norm se exponen para que el test pueda barrerlos y calibrar
//    sin recompilar; el juego llama siempre a Altitud3D.
static inline float Altitud3DEx(Vector3 dir, float contBias, float norm) {
    // Continentes: ruido aparte, MUCHO más suave. Un fBm de 17 octavas a
    // escala continental daría más detalle del que la tectónica soporta.
    float cont = Noise3D(Vector3Scale(dir, CONTINENT_FREC)) * 2.0f;
    // Segundo pase a 2.4x para romper la regularidad del primer octava
    cont += Noise3D(Vector3Scale(dir, CONTINENT_FREC * 2.4f)) * 1.0f;
    cont = cont * 0.66f + contBias;

    // Máscara tierra/agua: 0 = océano abierto, 1 = tierra firme
    float mask = SmoothStep(0.10f, 0.35f, cont);

    float ocean = -OCEAN_DEPTH
                - Noise3D(Vector3Scale(dir, 4.0f)) * 2.0f
                - Noise3D(Vector3Scale(dir, 9.0f)) * 0.9f;


    // Relieve: 17 octavas con decaimiento 0.5 (auto-afín)
    float h = 0.0f, amp = AMP_BASE, f = FREC_BASE;
    for (int o = 0; o < FBM_OCTAVES; o++) {
        // Las 3 primeras octavas (λ 3000, 1500, 750 km) llevan boost: son las
        // que se ven desde órbita y necesitan más presencia.
        float a = (o < AMP_MACRO_OCT) ? amp * AMP_MACRO : amp;
        h += Noise3D(Vector3Scale(dir, f)) * a;
        f *= FBM_LACUN;
        amp *= FBM_GAIN;
    }
    h *= norm;

    // Mezcla continua. En mask=0 puro océano, en mask=1 puro relieve; en el
    // borde la mezcla cruza el nivel del mar → costa sin escalón.
    return ocean * (1.0f - mask) + h * mask;
}

static inline float Altitud3D(Vector3 dir) {
    return Altitud3DEx(dir, CONT_BIAS, NORMALIZACION);
}

// Radio de la superficie (km) en una dirección dada.
static inline float Radio3D(Vector3 dir) {
    return PLANET_R + Altitud3D(dir);
}
