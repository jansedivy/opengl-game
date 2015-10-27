#version 330 core

layout (location = 0) in vec3 position;
layout (location = 2) in vec2 uv;

uniform mat4 uPMatrix;
uniform mat4 uMVMatrix;

out vec2 TexCoords;

void main() {
  TexCoords = uv * vec2(1.0, -1.0);
  gl_Position = uPMatrix * uMVMatrix * vec4(position, 1.0);
}
