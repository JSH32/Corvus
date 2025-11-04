#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 viewProjection;
uniform vec3 cameraPos;

out vec3 worldPos;
out vec3 cameraPosition;

void main() {
    vec3 pos = aPos;
    pos.xz += cameraPos.xz;

    worldPos       = pos;
    cameraPosition = cameraPos;
    // gl_Position    = vec4(aPos.xy, 0.0, 1.0);
    gl_Position = viewProjection * vec4(pos, 1.0);
}
