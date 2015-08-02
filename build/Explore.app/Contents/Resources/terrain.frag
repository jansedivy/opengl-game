#version 330

out vec4 color;

in vec3 inNormals;
in vec4 inPosition;

uniform mat4 shadow_matrix;
uniform sampler2D uShadow;

uniform vec3 in_color;

const mat4 depthScaleMatrix = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.5, 0.5, 0.5, 1.0);

uniform vec2 texmapscale;

float offset_lookup(sampler2D map, vec4 loc, vec2 offset) {
  return textureProj(map, vec4(loc.xy + offset * texmapscale * loc.w, loc.z, loc.w)).x;
}

void main() {
  vec3 normals = normalize(inNormals);

  vec3 light_position = vec3(1.0, 0.7, 0.4);
  vec3 light_direction = normalize(light_position);

  float directional_light = max(dot(light_direction, normals), 0.0);

  float light = 0.8 + directional_light;

  vec4 UVinShadowMap = shadow_matrix * vec4(inPosition.xyz, 1.0);
  vec4 lookup = depthScaleMatrix * UVinShadowMap;

  float sum = 0;
  float shadow = 1.0;

  float x, y;

  for (y = -1.5; y <= 1.5; y += 1.0)
    for (x = -1.5; x <= 1.5; x += 1.0)
      sum += offset_lookup(uShadow, lookup, vec2(x, y));

  float shadow_coeff = sum / 16.0;
  shadow = 1.0 - (shadow_coeff - 0.19);

  color = vec4(in_color * light * clamp((shadow + 0.5), 0.0, 1.0), 1.0);
}
