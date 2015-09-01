#version 330 core

layout (location = 0) in vec3 data;
layout (location = 1) in vec4 position;
layout (location = 2) in vec4 color;

uniform mat4 uPMatrix;

uniform vec3 camera_up;
uniform vec3 camera_right;

out vec4 inColor;

void main() {
  vec3 final_position = position.xyz + normalize(camera_right) * data.x * position.w + normalize(camera_up) * data.y * position.w;

  inColor = color;

  gl_Position = uPMatrix * vec4(final_position, 1.0);
}
