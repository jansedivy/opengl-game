#version 330

out vec4 color;

in vec2 TexCoords;

uniform sampler2D textureImage;

void main() {
  color = texture(textureImage, TexCoords);
}
