#version 330 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 uv;

uniform mat4 uPMatrix;
uniform mat4 uMVMatrix;

out vec2 TexCoords;

void main() {
  TexCoords = uv;

  gl_Position = uPMatrix * uMVMatrix * vec4(position.xy, 0.0, 1.0);
}
