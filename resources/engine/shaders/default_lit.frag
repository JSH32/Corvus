#version 330

// Inputs from vertex shader
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;

// Material properties
uniform sampler2D texture0; // _MainTex
uniform vec4      _MainColor;
uniform float     _Metallic;
uniform float     _Smoothness;

// Output
out vec4 finalColor;

// Basic directional light
const vec3 lightDir     = normalize(vec3(0.5, 1.0, 0.3));
const vec3 lightColor   = vec3(1.0, 1.0, 1.0);
const vec3 ambientColor = vec3(0.2, 0.2, 0.2);

// Simple PBR-ish lighting
vec3 calculateLighting(vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float smoothness) {
    // Normalize inputs
    vec3 N = normalize(normal);
    vec3 L = lightDir;
    vec3 V = normalize(viewDir);
    vec3 H = normalize(L + V);

    // Diffuse (Lambert)
    float NdotL   = max(dot(N, L), 0.0);
    vec3  diffuse = albedo * lightColor * NdotL;

    // Specular (Blinn-Phong, simplified)
    float roughness = 1.0 - smoothness;
    float shininess = mix(32.0, 256.0, smoothness);
    float spec      = pow(max(dot(N, H), 0.0), shininess);

    // Metals have no diffuse, only specular reflection
    vec3 F0       = mix(vec3(0.04), albedo, metallic); // Base reflectivity
    vec3 specular = F0 * spec * lightColor;

    // Combine diffuse and specular (metals lose diffuse)
    vec3 kD = mix(vec3(1.0), vec3(0.0), metallic);

    // Ambient
    vec3 ambient = ambientColor * albedo;

    // Final lighting
    return ambient + (kD * diffuse) + specular;
}

void main() {
    // Sample base texture
    vec4 texelColor = texture(texture0, fragTexCoord);

    // Apply main color tint
    vec3  albedo = texelColor.rgb * _MainColor.rgb;
    float alpha  = texelColor.a * _MainColor.a;

    // Calculate view direction
    vec3 viewDir = -normalize(fragPosition);

    // Calculate lighting
    vec3 litColor = calculateLighting(fragNormal, viewDir, albedo, _Metallic, _Smoothness);

    // Output final color
    finalColor = vec4(litColor, alpha);
}
