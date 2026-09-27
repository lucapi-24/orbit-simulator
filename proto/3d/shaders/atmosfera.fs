#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;      // posición del fragmento en el mundo
in vec3 fragWorldNormal;   // normal en el mundo

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 camPos;
uniform vec3 sunDir;
uniform float power;       // foco del halo: menor = más ancho

// Output fragment color
out vec4 finalColor;

void main()
{
    // 1. Dirección de la cámara hacia este fragmento
    vec3 viewDir = normalize(camPos - fragWorldPos);

    // 2. Fresnel: 0 de frente (centro del disco), 1 en el borde (limb)
    float fresnel = pow(1.0 - abs(dot(fragWorldNormal, viewDir)), power);

    // 3. El halo solo existe en la cara que mira al sol; en la nocturna se apaga.
    //    (sin esto, el lado oscuro del planeta tendría un halo brillante)
    float sunFace = max(dot(fragWorldNormal, sunDir), 0.0);

    // 4. Azul atmosférico, intensidad = fresnel × cara diurna
    finalColor = vec4(0.35, 0.60, 1.0, fresnel * sunFace);
}
