#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normals;

uniform mat4 uPMatrix;
uniform mat4 uMVMatrix;
uniform mat3 uNMatrix;

out vec3 final_color;

uniform vec4 in_color;

void main() {
  vec3 omgwhy = normalize(uNMatrix * normals);

  vec3 direction_to_light = normalize(vec3(-0.1, 1.0, 0.4));

  float light = min(max(dot(direction_to_light, omgwhy), 0.0), 1.0);
  vec3 omg = vec3(1.0) * pow(light, 10);

  final_color = in_color.rgb * light;

  gl_Position = uPMatrix * uMVMatrix * vec4(position, 1.0);
}
