#version 330

out vec4 color;

in vec2 TexCoords;
uniform vec3 font_color;

uniform sampler2D textureImage;

void main() {
  color = vec4(font_color, texture(textureImage, TexCoords).r);
}
