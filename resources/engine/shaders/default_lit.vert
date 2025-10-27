#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

// Output to fragment shader
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;

void main() {
    // Send texture coordinates to fragment shader
    fragTexCoord = vertexTexCoord;

    // Send vertex color to fragment shader
    fragColor = vertexColor;

    // Calculate fragment position in world space
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));

    // Calculate normal in world space
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));

    // Calculate final vertex position
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
