#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldNormal;

// Input uniform values
uniform sampler2D texture0;   // textura de nubes: el ALPHA es la cobertura
uniform vec4 colDiffuse;
uniform vec3 sunDir;
uniform float ambient;

// Output fragment color
out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    float a = texel.a * colDiffuse.a * fragColor.a;   // cobertura de nube
    if (a < 0.02) discard;            // sin nube → no se dibuja (ahorra fill rate)

    // Mismo lambert que el terreno. La normal es la radial de la esfera, así que
    // las nubes se apagan por completo en la cara nocturna (correcto en silueta).
    float lambert = max(dot(fragWorldNormal, sunDir), 0.0);
    float light = ambient + (1.0 - ambient) * lambert;

    finalColor = vec4(vec3(1.0, 1.0, 1.0) * light, a);
}
