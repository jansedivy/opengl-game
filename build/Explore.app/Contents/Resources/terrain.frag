#version 330

out vec4 color;

in vec3 inNormals;
in vec4 inPosition;

uniform vec3 in_color;

void main() {
  vec3 normals = normalize(inNormals);

  vec3 light_position = vec3(1.0, 0.7, 0.4);
  vec3 light_direction = normalize(light_position);

  float directional_light = max(dot(light_direction, normals), 0.0);

  float light = 0.8 + directional_light;

  color = vec4(in_color * light, 1.0);
}
