#version 330

out vec4 color;

in vec2 TexCoords;

uniform sampler2D textureImage;

uniform vec4 in_color;

void main() {
  color = in_color * texture(textureImage, TexCoords);
}
