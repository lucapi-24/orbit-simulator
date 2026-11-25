#version 330

// Input vertex attributes (from vertex shader)
in float fragH;               // altura en km (interpolada)
in vec3 fragDir;              // dirección del píxel en espacio de malla
in vec3 fragWorldNormal;

// Input uniform values
uniform vec4 colDiffuse;
uniform vec3 sunDir;          // dirección normalizada planeta → sol
uniform float ambient;        // luz residual: la noche nunca es negra total

// Output fragment color
out vec4 finalColor;

// Paleta por tramos (mismas paradas que la antigua HeightColor de main3d.cpp), en 0..1.
vec3 paleta(float x)
{
    const float alt[7] = float[7](0.00, 0.02, 0.10, 0.25, 0.55, 0.80, 1.00);
    const vec3 col[7] = vec3[7](
        vec3( 18.0,  52.0, 110.0) / 255.0,   // océano profundo
        vec3( 45.0, 105.0, 170.0) / 255.0,   // océano somero
        vec3(205.0, 195.0, 145.0) / 255.0,   // arena / costa
        vec3( 95.0, 165.0,  85.0) / 255.0,   // tierras bajas verdes
        vec3( 60.0, 115.0,  55.0) / 255.0,   // tierras altas verde oscuro
        vec3(140.0, 125.0, 110.0) / 255.0,   // montaña rocosa
        vec3(235.0, 240.0, 250.0) / 255.0    // nieve
    );

    if (x <= alt[0]) return col[0];
    if (x >= alt[6]) return col[6];
    for (int i = 0; i < 6; ++i) {
        if (x <= alt[i + 1]) {
            float t = (x - alt[i]) / (alt[i + 1] - alt[i]);
            return mix(col[i], col[i + 1], t);
        }
    }
    return col[6];
}

void main()
{
    // Difuso lambertiano: 0 en la cara nocturna, 1 con el sol perpendicular
    float lambert = max(dot(fragWorldNormal, sunDir), 0.0);

    float light = ambient + (1.0 - ambient) * lambert;

    // Agua/tierra por píxel: la transición ocupa ~1 píxel (fwidth), así la costa
    // sale recta respecto a la pantalla y no sigue la rejilla de la malla.
    float w = max(fwidth(fragH), 1e-4);
    vec3 sea  = paleta(clamp((fragH + 3.0) / 3.0, 0.0, 1.0) * 0.02);
    vec3 land = paleta(0.10 + clamp(fragH / 9.0, 0.0, 1.0) * 0.90);
    vec3 color = mix(sea, land, smoothstep(-w, w, fragH));

    // Casquetes polares: test sobre la dirección del píxel (mismo criterio que en CPU)
    float ice = smoothstep(0.965, 0.975, abs(normalize(fragDir).z));
    color = mix(color, vec3(240.0, 244.0, 250.0) / 255.0, ice);

    finalColor = vec4(color * light, 1.0);
}
