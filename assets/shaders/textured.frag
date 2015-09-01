#version 330

out vec4 color;

in vec2 TexCoords;

uniform sampler2D textureImage;

void main() {
  vec4 texture_color = texture(textureImage, TexCoords);

  if (texture_color.a < 1.0) {
    discard;
  }

  color = texture_color;
}
