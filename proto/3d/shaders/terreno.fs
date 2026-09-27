#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldNormal;

// Input uniform values
uniform sampler2D texture0;   // raylib liga la textura del material aquí solo
uniform vec4 colDiffuse;
uniform vec3 sunDir;          // dirección normalizada planeta → sol
uniform float ambient;        // luz residual: la noche nunca es negra total

// Output fragment color
out vec4 finalColor;

void main()
{
    // Difuso lambertiano: 0 en la cara nocturna, 1 con el sol perpendicular
    float lambert = max(dot(fragWorldNormal, sunDir), 0.0);

    vec4 texel = texture(texture0, fragTexCoord);
    float light = ambient + (1.0 - ambient) * lambert;

    finalColor = vec4(texel.rgb * light, 1.0);
}
