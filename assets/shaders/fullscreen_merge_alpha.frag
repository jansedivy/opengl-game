#version 330

uniform sampler2D color_texture;
uniform sampler2D transparent_color_texture;

out vec4 color;

in vec2 pos;

void main() {
  vec2 uv = pos * 0.5 + 0.5;

  vec4 accum = texture(color_texture, uv);
  float r = texture(transparent_color_texture, uv).r;

  color = vec4(accum.rgb / clamp(accum.a, 1e-4, 5e4), r);
}
