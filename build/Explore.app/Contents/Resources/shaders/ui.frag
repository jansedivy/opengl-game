#version 330

out vec4 color;

in vec2 TexCoords;
uniform vec4 image_color;
uniform vec4 background_color;

uniform sampler2D textureImage;

void main() {
  color = background_color + image_color * texture(textureImage, TexCoords);
}
