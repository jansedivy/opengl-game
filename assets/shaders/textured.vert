#version 330 core

layout (location = 0) in vec3 position;
layout (location = 2) in vec2 uv;
layout (location = 1) in vec3 normals;

out vec2 TexCoords;

uniform mat4 uPMatrix;
uniform mat4 uMVMatrix;
uniform mat3 uNMatrix;

out vec3 inNormals;
out vec4 inPosition;

void main() {
  TexCoords = uv * vec2(1.0, -1.0);
  inNormals = uNMatrix * normals;

  inPosition = uMVMatrix * vec4(position, 1.0);

  gl_Position = uPMatrix * inPosition;
}
