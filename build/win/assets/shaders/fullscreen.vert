#version 330

layout (location = 0) in vec2 position;

out vec2 pos;

void main() {
  pos = position;
  gl_Position = vec4(position, 1.0, 1.0);
}
