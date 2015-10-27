#version 330

out vec4 color;

uniform vec4 in_color;

in vec2 TexCoords;

uniform sampler2D textureImage;

void main() {
  vec4 texture_color = texture(textureImage, TexCoords);

  if (texture_color.a < 0.5) {
    discard;
  }

  color = vec4(in_color);
}
