#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aUV;
layout (location = 3) in float aTextureId;
out vec3 FragColor;
out vec2 FragUV;
flat out int FragTextureId;
uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
    FragColor = aColor;
    FragUV = aUV;
    FragTextureId = int(aTextureId + 0.5);
}
