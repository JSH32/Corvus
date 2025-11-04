#version 330

// Input vertex attributes
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTexCoord;
layout(location = 3) in vec4 vertexColor;

// Input uniform values
uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_ViewProjection;
uniform mat4 u_NormalMatrix;

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
    fragPosition = vec3(u_Model * vec4(vertexPosition, 1.0));
    fragNormal   = normalize(vec3(u_NormalMatrix * vec4(vertexNormal, 0.0)));
    gl_Position  = u_ViewProjection * u_Model * vec4(vertexPosition, 1.0);
}
