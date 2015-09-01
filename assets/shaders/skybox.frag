#version 330

in vec3 texturePosition;

uniform samplerCube uSampler;

out vec4 fragColor;

void main() {
  fragColor = texture(uSampler, texturePosition);
}
