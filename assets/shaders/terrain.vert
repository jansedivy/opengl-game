#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normals;

uniform mat4 uPMatrix;
uniform mat4 uMVMatrix;
uniform mat3 uNMatrix;

out vec3 inNormals;
out vec4 inPosition;

void main() {
  inNormals = uNMatrix * normals;

  inPosition = uMVMatrix * vec4(position, 1.0);

  gl_Position = uPMatrix * inPosition;
}
