#version 330

out vec4 color;

uniform vec3 in_color;

void main() {
  color = vec4(in_color, 1.0);
}
