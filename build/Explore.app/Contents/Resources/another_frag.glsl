#version 330

out vec4 color;

in vec3 inNormals;
in vec4 inPosition;

uniform vec3 in_color;

uniform vec3 eye_position;

void main() {
  vec3 normals = normalize(inNormals);

  vec3 light_position = vec3(1.0, 0.7, 0.4);
  vec3 light_direction = normalize(light_position);

  float directional_light = max(dot(light_direction, normals), 0.0);
  vec3 final_directional_light = vec3(0.7) * vec3(directional_light);

  vec3 V = normalize(eye_position - inPosition.xyz);

  float rim = smoothstep(0.5, 1.0, 1.0 - max(dot(V, normals), 0.0));
  vec3 final_rim = vec3(0.8) * vec3(rim);

  vec3 light = vec3(0.8) + final_rim + final_directional_light;

  color = vec4(in_color * light, 1.0);
}
