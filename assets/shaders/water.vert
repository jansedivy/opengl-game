#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normals;
layout (location = 2) in vec2 uv;

uniform mat4 uPMatrix;
uniform mat4 uMVMatrix;
uniform mat3 uNMatrix;

out vec3 inNormals;
out vec4 inPosition;
out vec2 uvs;

out vec4 clip_space;

void main() {
  inNormals = uNMatrix * normals;

  inPosition = uMVMatrix * vec4(position, 1.0);
  uvs = uv;

  clip_space = uPMatrix * inPosition;
  gl_Position = clip_space;
}
