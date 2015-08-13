#version 330

uniform sampler2D uSampler;
uniform sampler2D color_correction_texture;

out vec4 color;

in vec2 pos;

uniform float lut_size;

vec3 scale = vec3((lut_size - 1.0) / lut_size);
vec3 offset = vec3(1.0 / (2.0 * lut_size));

vec4 unwrapped_texture_3d_sample(sampler2D sampler, vec3 uvw, float size) {
  float int_w = floor(uvw.z * size - 0.5);
  float frac_w = uvw.z * size - 0.5 - int_w;

  float u = (uvw.x + int_w) / size;
  float v = uvw.y;

  vec4 rg0 = texture(sampler, vec2(u, v));
  vec4 rg1 = texture(sampler, vec2(u + 1.0 / size, v));

  return mix(rg0, rg1, frac_w);
}

void main() {
  vec2 uv = pos * 0.5 + 0.5;

  vec4 col = texture(uSampler, uv);
  color = unwrapped_texture_3d_sample(color_correction_texture, scale * col.xyz + offset, lut_size);
}
