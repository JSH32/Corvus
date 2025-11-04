#version 330

// Inputs from vertex shader
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;

// Material properties
uniform sampler2D texture0;
uniform vec4      _MainColor;
uniform float     _Metallic;
uniform float     _Smoothness;

// Lighting uniforms
uniform vec3 u_ViewPos;
uniform vec3 u_AmbientColor;
uniform vec3 u_DirLightDir;
uniform vec3 u_DirLightColor;

// Point lights
struct PointLight {
    vec3  position;
    vec3  color;
    float range;
};
uniform int        u_PointLightCount;
uniform PointLight u_PointLights[16];

// Spot lights
struct SpotLight {
    vec3  position;
    vec3  direction;
    vec3  color;
    float range;
    float innerCutoff;
    float outerCutoff;
};
uniform int       u_SpotLightCount;
uniform SpotLight u_SpotLights[16];

// Standard shadow uniforms (directional/spot)
uniform int       u_ShadowMapCount;
uniform sampler2D u_ShadowMaps[4];
uniform mat4      u_LightSpaceMatrices[4];
uniform float     u_ShadowBias[4];
uniform float     u_ShadowStrength[4];

// Point light shadow uniforms
uniform int         u_PointLightShadowCount;
uniform samplerCube u_PointLightShadowMaps[4];
uniform vec3        u_PointLightShadowPositions[4];
uniform float       u_PointLightShadowFarPlanes[4];
uniform int         u_PointLightShadowIndices[4]; // Maps shadow index to point light index

// Output
out vec4 finalColor;

// Simple PBR-ish lighting for directional light
vec3 calculateDirectionalLight(
    vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float smoothness) {
    vec3 N = normalize(normal);
    vec3 L = normalize(-u_DirLightDir);
    vec3 V = normalize(viewDir);
    vec3 H = normalize(L + V);

    float NdotL   = max(dot(N, L), 0.0);
    vec3  diffuse = albedo * u_DirLightColor * NdotL;

    float shininess = mix(32.0, 256.0, smoothness);
    float spec      = pow(max(dot(N, H), 0.0), shininess);

    vec3 F0       = mix(vec3(0.04), albedo, metallic);
    vec3 specular = F0 * spec * u_DirLightColor;

    vec3 kD = mix(vec3(1.0), vec3(0.0), metallic);

    return (kD * diffuse) + specular;
}

// Point light calculation (without shadows, applied seperately)
vec3 calculatePointLight(PointLight light,
                         vec3       normal,
                         vec3       fragPos,
                         vec3       viewDir,
                         vec3       albedo,
                         float      metallic,
                         float      smoothness) {
    vec3  L        = light.position - fragPos;
    float distance = length(L);

    if (distance > light.range)
        return vec3(0.0);

    L = normalize(L);

    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    attenuation *= smoothstep(light.range, 0.0, distance);

    vec3 N = normalize(normal);
    vec3 V = normalize(viewDir);
    vec3 H = normalize(L + V);

    float NdotL   = max(dot(N, L), 0.0);
    vec3  diffuse = albedo * light.color * NdotL;

    float shininess = mix(32.0, 256.0, smoothness);
    float spec      = pow(max(dot(N, H), 0.0), shininess);

    vec3 F0       = mix(vec3(0.04), albedo, metallic);
    vec3 specular = F0 * spec * light.color;

    vec3 kD = mix(vec3(1.0), vec3(0.0), metallic);

    return ((kD * diffuse) + specular) * attenuation;
}

// Spot light calculation
vec3 calculateSpotLight(SpotLight light,
                        vec3      normal,
                        vec3      fragPos,
                        vec3      viewDir,
                        vec3      albedo,
                        float     metallic,
                        float     smoothness) {
    vec3  L        = light.position - fragPos;
    float distance = length(L);

    if (distance > light.range)
        return vec3(0.0);

    L = normalize(L);

    float theta         = dot(L, normalize(-light.direction));
    float epsilon       = light.innerCutoff - light.outerCutoff;
    float spotIntensity = clamp((theta - light.outerCutoff) / epsilon, 0.0, 1.0);

    if (spotIntensity <= 0.0)
        return vec3(0.0);

    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    attenuation *= smoothstep(light.range, 0.0, distance);

    vec3 N = normalize(normal);
    vec3 V = normalize(viewDir);
    vec3 H = normalize(L + V);

    float NdotL   = max(dot(N, L), 0.0);
    vec3  diffuse = albedo * light.color * NdotL;

    float shininess = mix(32.0, 256.0, smoothness);
    float spec      = pow(max(dot(N, H), 0.0), shininess);

    vec3 F0       = mix(vec3(0.04), albedo, metallic);
    vec3 specular = F0 * spec * light.color;

    vec3 kD = mix(vec3(1.0), vec3(0.0), metallic);

    return ((kD * diffuse) + specular) * attenuation * spotIntensity;
}

// Standard shadow calculation (directional/spot) with PCF
float calculateShadow(vec3 fragPos, int shadowIndex) {
    if (u_ShadowMapCount == 0 || shadowIndex >= u_ShadowMapCount || shadowIndex < 0)
        return 0.0;

    vec4 fragPosLightSpace = u_LightSpaceMatrices[shadowIndex] * vec4(fragPos, 1.0);
    vec3 projCoords        = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords             = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0
        || projCoords.y > 1.0) {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    if (currentDepth > 1.0) {
        return 0.0;
    }

    float bias      = u_ShadowBias[shadowIndex];
    float shadow    = 0.0;
    vec2  texelSize = 1.0 / textureSize(u_ShadowMaps[shadowIndex], 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 sampleCoord = projCoords.xy + vec2(x, y) * texelSize;
            sampleCoord      = clamp(sampleCoord, 0.0, 1.0);

            float pcfDepth = texture(u_ShadowMaps[shadowIndex], sampleCoord).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow * u_ShadowStrength[shadowIndex];
}

// Point light cubemap shadow calculation
float calculatePointLightShadow(vec3 fragPos, int shadowIndex) {
    if (u_PointLightShadowCount == 0 || shadowIndex >= u_PointLightShadowCount || shadowIndex < 0)
        return 0.0;

    // Get direction from light to fragment
    vec3 fragToLight = fragPos - u_PointLightShadowPositions[shadowIndex];

    // Sample the cubemap
    float closestDepth = texture(u_PointLightShadowMaps[shadowIndex], fragToLight).r;

    // Convert back to original depth value
    closestDepth *= u_PointLightShadowFarPlanes[shadowIndex];

    // Get current distance
    float currentDepth = length(fragToLight);

    // Shadow bias
    float bias   = 0.05;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    // Add PCF for softer shadows here?

    return shadow * 0.8; // Shadow strength
}

void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);

    vec3  albedo = texelColor.rgb * _MainColor.rgb;
    float alpha  = texelColor.a * _MainColor.a;

    vec3 viewDir = normalize(u_ViewPos - fragPosition);

    // Ambient
    vec3 ambient = u_AmbientColor * albedo;

    // Directional light
    vec3 directionalLighting
        = calculateDirectionalLight(fragNormal, viewDir, albedo, _Metallic, _Smoothness);

    // Directional shadow
    float dirShadow = 0.0;
    if (u_ShadowMapCount > 0) {
        dirShadow = calculateShadow(fragPosition, 0);
    }
    vec3 shadedDirectional = (1.0 - dirShadow) * directionalLighting;

    // Point lights WITH shadows
    vec3 pointLighting = vec3(0.0);
    for (int i = 0; i < u_PointLightCount && i < 16; ++i) {
        vec3 lightContribution = calculatePointLight(
            u_PointLights[i], fragNormal, fragPosition, viewDir, albedo, _Metallic, _Smoothness);

        // Check if this point light casts shadows
        float pointShadow = 0.0;
        for (int s = 0; s < u_PointLightShadowCount; ++s) {
            if (u_PointLightShadowIndices[s] == i) {
                pointShadow = calculatePointLightShadow(fragPosition, s);
                break;
            }
        }

        // Apply shadow to this light
        lightContribution *= (1.0 - pointShadow);
        pointLighting += lightContribution;
    }

    // Spot lights
    vec3 spotLighting = vec3(0.0);
    for (int i = 0; i < u_SpotLightCount && i < 16; ++i) {
        spotLighting += calculateSpotLight(
            u_SpotLights[i], fragNormal, fragPosition, viewDir, albedo, _Metallic, _Smoothness);
    }

    // Combine all lighting
    vec3 finalLighting = ambient + shadedDirectional + pointLighting + spotLighting;

    finalColor = vec4(finalLighting, alpha);
}
