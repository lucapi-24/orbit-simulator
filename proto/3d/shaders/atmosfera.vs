#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matModel;    // ← nueva: la transformación del modelo (la calcula raylib)
uniform mat4 matNormal;   // ← nueva: inversa-transpuesta de matModel, para normales
uniform vec3 camPos;

// Output vertex attributes (to fragment shader)
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragWorldPos;      // dónde está este vértice en el mundo
out vec3 fragWorldNormal;   // su normal en el mundo
// NOTE: Add your custom variables here

void main()
{
    // Send vertex attributes to fragment shader
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    

    fragWorldPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragWorldNormal = normalize((matModel * vec4(vertexNormal, 1.0)).xyz);

    // Calculate final vertex position
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}