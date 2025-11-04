#version 330 core
in vec3  worldPos;
in vec3  cameraPosition;
out vec4 FragColor;

uniform float gridSize          = 1.0;
uniform float cellSize          = 1.0;
uniform float subcellSize       = 0.1;
uniform vec4  cellColor         = vec4(0.75, 0.75, 0.75, 0.5);
uniform vec4  subcellColor      = vec4(0.5, 0.5, 0.5, 0.5);
uniform float heightToFadeRatio = 25.0;

const vec4 X_AXIS_COLOR = vec4(1.0, 0.2, 0.2, 0.8);
const vec4 Z_AXIS_COLOR = vec4(0.2, 0.2, 1.0, 0.8);

const float CELL_LINE_THICKNESS    = 0.01;
const float SUBCELL_LINE_THICKNESS = 0.001;
const float AXIS_THICKNESS         = 0.02;

bool isOnGridLine(vec2 coords, float lineSpacing, float thickness) {
    vec2 cellCoords        = mod(coords + lineSpacing * 0.5, lineSpacing);
    vec2 distanceToEdge    = abs(cellCoords - lineSpacing * 0.5);
    vec2 adjustedThickness = 0.5 * (thickness + fwidth(coords));
    return any(lessThan(distanceToEdge, adjustedThickness));
}

void main() {
    // Only render on ground plane
    if (abs(worldPos.y) > 0.001) {
        discard;
    }

    vec2 coords = worldPos.xz;
    vec4 color  = vec4(0.0);

    // Draw grid lines (subcell first, then cell to override)
    if (isOnGridLine(coords, subcellSize, SUBCELL_LINE_THICKNESS)) {
        color = subcellColor;
    }
    if (isOnGridLine(coords, cellSize, CELL_LINE_THICKNESS)) {
        color = cellColor;
    }

    // Draw coordinate axes at origin
    if (abs(coords.x) < AXIS_THICKNESS) {
        color = Z_AXIS_COLOR; // Blue Z-axis
    }
    if (abs(coords.y) < AXIS_THICKNESS) {
        color = X_AXIS_COLOR; // Red X-axis
    }

    // Apply distance-based fade
    float distanceToCamera = length(coords - cameraPosition.xz);
    float fadeDistance
        = clamp(abs(cameraPosition.y) * heightToFadeRatio, gridSize * 0.05, gridSize * 0.5);
    float fade = smoothstep(1.0, 0.0, distanceToCamera / fadeDistance);

    FragColor = color * fade;
}
