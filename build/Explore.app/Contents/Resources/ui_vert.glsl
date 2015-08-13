#version 330 core

layout (location = 0) in vec2 position;

uniform mat4 uPMatrix;

void main() {
  gl_Position = uPMatrix * vec4(position, 0.0, 1.0);
}
