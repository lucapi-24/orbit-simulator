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


void main()
{
    // Difuso lambertiano: 0 en la cara nocturna, 1 con el sol perpendicular
    float lambert = max(dot(fragWorldNormal, sunDir), 0.0);

    float light = ambient + (1.0 - ambient) * lambert;

    float altura = clamp((fragH + 3.0) / 12.0, 0.0, 1.0);
    float tono = 0.5 + 0.04 * clamp(fragH / 10.0, -1.0, 1.0);
    vec3 color = vec3(tono);


    finalColor = vec4(color * light, 1.0);
}
