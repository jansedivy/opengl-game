#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normals;
layout (location = 2) in vec2 uv;

layout (location = 3) in vec4 position_data;
layout (location = 4) in vec3 rotation;
layout (location = 5) in vec3 tint;

uniform mat4 uPMatrix;
uniform float time;

/* uniform mat3 uNMatrix; */

out vec3 inNormals;
out vec4 inPosition;
out vec2 TexCoords;
out vec3 inTint;

void main() {
  vec3 test = rotation;
  inTint = tint;
  inNormals = normalize(normals);

  vec3 grass_position = position_data.xyz + position * position_data.w;

  float size = 0.1;

  float up = position.y;
  float time_offset = (position_data.x + position_data.z) / 2;

  grass_position += vec3(sin(time / 2 + time_offset) * size * up, 0.0, sin(time / 2 + time_offset) * size * up);

  inPosition = vec4(grass_position, 1.0);
  TexCoords = uv * vec2(1.0, -1.0);

  gl_Position = uPMatrix * inPosition;
}
