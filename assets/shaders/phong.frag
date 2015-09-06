#version 330 
out vec4 color;

in vec3 final_color;

void main() {
  color = vec4(final_color, 1.0);
}
