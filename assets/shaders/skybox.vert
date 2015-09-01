#version 330

layout (location = 0) in vec3 position;

uniform mat4 projection;

out vec3 texturePosition;

void main() {
  texturePosition = position.xyz * vec3(-1.0, 1.0, 1.0);
  gl_Position = projection * vec4(position, 1.0);
}
