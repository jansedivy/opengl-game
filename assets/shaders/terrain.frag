#version 330

out vec4 color;

in vec3 inNormals;
in vec4 inPosition;
in vec3 inColors;

uniform vec4 in_color;

uniform vec3 eye_position;

uniform sampler2DShadow uShadow;
uniform vec2 texmapscale;

float offset_lookup(sampler2DShadow map, vec4 loc, vec2 offset) {
  return textureProj(map, vec4(loc.xy + offset * texmapscale, loc.z, loc.w));
}

const mat4 depthScaleMatrix = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.5, 0.5, 0.5, 1.0);
uniform mat4 shadow_matrix;

void main() {
  vec3 normals = normalize(inNormals);

  vec3 light_position = vec3(1.0, 0.7, 0.4);
  vec3 light_direction = normalize(light_position);

  float directional_light = max(dot(light_direction, normals), 0.0);
  vec3 final_directional_light = vec3(directional_light);

  vec3 V = normalize(eye_position - inPosition.xyz);

  vec3 rim = vec3(smoothstep(0.5, 1.0, 1.0 - max(dot(V, normals), 0.0)));
  vec3 light = final_directional_light;

  vec4 sc = depthScaleMatrix * shadow_matrix * (inPosition + vec4(normals * 4.0, 0.0));
  float shadow = 1.0;

  if (sc.w > 0.0 && (sc.x > 0 && sc.y > 0) && (sc.x < 1 && sc.y < 1)) {
    float sum = 0.0;

    vec2 uv = sc.xy * texmapscale; // 1 unit - 1 texel

    float s = (uv.x + 0.5 - sc.x);
    float t = (uv.y + 0.5 - sc.y);

    float uw0 = (4 - 3 * s);
    float uw1 = 7;
    float uw2 = (1 + 3 * s);

    float u0 = (3 - 2 * s) / uw0 - 2;
    float u1 = (3 + s) / uw1;
    float u2 = s / uw2 + 2;

    float vw0 = (4 - 3 * t);
    float vw1 = 7;
    float vw2 = (1 + 3 * t);

    float v0 = (3 - 2 * t) / vw0 - 2;
    float v1 = (3 + t) / vw1;
    float v2 = t / vw2 + 2;

    sum += uw0 * vw0 * offset_lookup(uShadow, sc, vec2(u0, v0));
    sum += uw1 * vw0 * offset_lookup(uShadow, sc, vec2(u1, v0));
    sum += uw2 * vw0 * offset_lookup(uShadow, sc, vec2(u2, v0));

    sum += uw0 * vw1 * offset_lookup(uShadow, sc, vec2(u0, v1));
    sum += uw1 * vw1 * offset_lookup(uShadow, sc, vec2(u1, v1));
    sum += uw2 * vw1 * offset_lookup(uShadow, sc, vec2(u2, v1));

    sum += uw0 * vw2 * offset_lookup(uShadow, sc, vec2(u0, v2));
    sum += uw1 * vw2 * offset_lookup(uShadow, sc, vec2(u1, v2));
    sum += uw2 * vw2 * offset_lookup(uShadow, sc, vec2(u2, v2));

    shadow = sum * 1.0 / 144;
  }

  color = vec4(inColors, 1.0);
  color = vec4(in_color.xyz * (vec3(0.8) + light * shadow), 1.0);
}
