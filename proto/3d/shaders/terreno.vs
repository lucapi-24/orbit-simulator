#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform values
// raylib inyecta estas matrices automáticamente si el shader las declara
// con estos nombres exactos (ver SHADER_LOC_MATRIX_* en rmodels.c).
uniform mat4 mvp;
uniform mat4 matNormal;   // inversa-transpuesta de matModel, para normales

// Output vertex attributes (to fragment shader)
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragWorldNormal;   // normal en el mundo, para el lambertiano

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    // matNormal y no matModel: una normal es una DIRECCIÓN, se transforma con
    // la inversa-transpuesta y con w = 0 (si no, la desplazaría en el espacio).
    fragWorldNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
