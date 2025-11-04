#version 330
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

uniform mat4 u_ProjectionMatrix;

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV         = aUV;
    vColor      = aColor;
    gl_Position = u_ProjectionMatrix * vec4(aPos, 0.0, 1.0);
}
