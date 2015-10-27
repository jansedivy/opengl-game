#version 330
out vec4 color;

in vec3 inNormals;
in vec4 inPosition;
in vec2 TexCoords;

uniform sampler2DShadow uShadow;
uniform vec2 texmapscale;
uniform vec3 shadow_light_position;
uniform vec3 tint;

uniform sampler2D textureImage;
uniform mat4 shadow_matrix;

float offset_lookup(sampler2DShadow map, vec4 loc, vec2 offset) {
  return textureProj(map, vec4(loc.xy + offset * texmapscale * loc.w, loc.z, loc.w));
}

const mat4 depthScaleMatrix = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.5, 0.5, 0.5, 1.0);

vec2 get_shadow_offsets(vec3 N, vec3 L) {
  float cos_alpha = clamp(dot(N, L), 0.0, 1.0);
  float offset_scale_N = sqrt(1 - cos_alpha*cos_alpha); // sin(acos(L·N))
  float offset_scale_L = offset_scale_N / cos_alpha;    // tan(acos(L·N))
  return vec2(offset_scale_N, min(2, offset_scale_L));
}

void main() {
  vec4 texture_color = texture(textureImage, TexCoords);

  if (texture_color.a < 0.5) {
    discard;
  }

  vec3 normals = normalize(inNormals);

  vec3 direction_to_light = normalize(shadow_light_position - inPosition.xyz);

  float directional_light = 0.8;
  vec3 final_directional_light = vec3(directional_light);

  vec3 light = final_directional_light;

  vec2 shadow_offset = get_shadow_offsets(normals, direction_to_light);

  vec4 sc = depthScaleMatrix * shadow_matrix * (inPosition + vec4(normals * 2, 0.0));

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

    sum += uw0 * vw0 * offset_lookup(uShadow, sc, shadow_offset + vec2(u0, v0));
    sum += uw1 * vw0 * offset_lookup(uShadow, sc, shadow_offset + vec2(u1, v0));
    sum += uw2 * vw0 * offset_lookup(uShadow, sc, shadow_offset + vec2(u2, v0));

    sum += uw0 * vw1 * offset_lookup(uShadow, sc, shadow_offset + vec2(u0, v1));
    sum += uw1 * vw1 * offset_lookup(uShadow, sc, shadow_offset + vec2(u1, v1));
    sum += uw2 * vw1 * offset_lookup(uShadow, sc, shadow_offset + vec2(u2, v1));

    sum += uw0 * vw2 * offset_lookup(uShadow, sc, shadow_offset + vec2(u0, v2));
    sum += uw1 * vw2 * offset_lookup(uShadow, sc, shadow_offset + vec2(u1, v2));
    sum += uw2 * vw2 * offset_lookup(uShadow, sc, shadow_offset + vec2(u2, v2));

    shadow = sum * 1.0 / 144;
  }

  color = vec4((tint * texture_color.xyz) * (vec3(0.4) + light * shadow), texture_color.a);
}
