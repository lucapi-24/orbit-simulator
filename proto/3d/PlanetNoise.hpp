#pragma once
#include <raylib.h>

// Textura de nubes en blanco con alfa según fBm 3D.
Image GenerateCloudImage(int texW, int texH, float umbral, float dureza);
