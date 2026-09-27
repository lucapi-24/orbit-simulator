#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;      // ← nuevo
in vec3 fragWorldNormal; 

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 camPos;
uniform float power;

// Output fragment color
out vec4 finalColor;

// NOTE: Add your custom variables here

void main()
{
    // 1. Dirección de la cámara hacia este fragmento
    vec3 viewDir = normalize(camPos - fragWorldPos);

    // 2. Fresnel: 0 de frente (centro del disco), 1 en el borde (limb)
    float fresnel = pow(1.0 - abs(dot(fragWorldNormal, viewDir)), power);

    // 3. Azul atmosférico, intensidad = fresnel
    finalColor = vec4(0.35, 0.60, 1.0, fresnel);
}

