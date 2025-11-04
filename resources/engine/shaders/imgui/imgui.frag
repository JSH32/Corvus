#version 330
uniform sampler2D u_Texture;
in vec2           vUV;
in vec4           vColor;
out vec4          FragColor;

void main() { FragColor = vColor * texture(u_Texture, vUV); }
