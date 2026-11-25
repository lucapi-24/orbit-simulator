#include "PlanetNoise.hpp"
#include <raymath.h>
#include "Noise3D.hpp"

Image GenerateCloudImage(int texW, int texH, float umbral, float dureza) {
    Image img = GenImageColor(texW, texH, (Color){0, 0, 0, 0});
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            // z añade variación sinusoidal hacia los polos (nubes con profundidad, no mapa plano)
            float z = sinf((float)y / texH * PI);
            float n = Noise3D(Vector3Scale({ (float)x / texW, (float)y / texH, z }, 10.0f));
            float fbm = 0.6f * n;
            float a = (float)Clamp(SmoothStep(umbral, umbral + dureza, fbm), 0.0f, 1.0f);
            ImageDrawPixel(&img, x, y, (Color){255, 255, 255, (unsigned char)(a*255)});
        }
    }
    return img;
}
