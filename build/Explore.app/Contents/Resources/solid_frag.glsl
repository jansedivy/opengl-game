#version 330

out vec4 color;

uniform vec4 in_color;

void main() {
  color = vec4(in_color);
}
