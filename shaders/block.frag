
#version 330 core

in vec3 FragColor;
in vec2 FragUV;
flat in int FragTextureId;

out vec4 FragColorOut;

uniform sampler2D uAtlas;
uniform int uUseTexture;
uniform vec3 uTint;

void main() {
    if (uUseTexture == 1) {
        const float ATLAS_COLS = 4.0;
        const float ATLAS_ROWS = 4.0;
        const float TILE_SIZE = 16.0;

        // Komşu atlas hücrelerinden renk sızmasını önle.
        vec2 localUV = mix(
            vec2(0.5 / TILE_SIZE),
            vec2(15.5 / TILE_SIZE),
            clamp(FragUV, 0.0, 1.0)
        );

        vec2 cell = vec2(
            float(FragTextureId % 4),
            float(FragTextureId / 4)
        );

        vec2 tileSize = vec2(
            1.0 / ATLAS_COLS,
            1.0 / ATLAS_ROWS
        );

        vec2 atlasUV = (cell + localUV) * tileSize;
        vec4 texel = texture(uAtlas, atlasUV);

        // Tam şeffaf ve yarı şeffaf dokuların boş
        // piksellerini koru.
        if (texel.a < 0.08) {
            discard;
        }

        FragColorOut = vec4(
            texel.rgb * uTint,
            texel.a
        );
    } else {
        FragColorOut = vec4(uTint, 1.0);
    }
}