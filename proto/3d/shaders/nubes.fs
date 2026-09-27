#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;    // ← raylib liga la textura del material aquí solo
uniform vec4 colDiffuse;
out vec4 finalColor;
void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    float a = texel.a * colDiffuse.a * fragColor.a;   // cobertura
    if (a < 0.02) discard;          // sin nube → no dibuja nada (ahorra fill rate)
    finalColor = vec4(1.0, 1.0, 1.0, a);   // blanco puro (el sol dará color después)
}