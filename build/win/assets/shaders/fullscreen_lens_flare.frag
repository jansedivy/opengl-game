#version 330

uniform sampler2D uSampler;

out vec4 color;

in vec2 pos;

float DiscMask(vec2 ScreenPos) {
  float x = clamp(1.0f - dot(ScreenPos, ScreenPos), 0.0, 1.0);
  return x * x;
}

void main() {
  vec2 uv = pos * 0.5 + 0.5;

  float ScreenborderMask = DiscMask(pos);
  ScreenborderMask *= DiscMask(pos * 0.8f);

  vec3 col = texture(uSampler, uv).rgb * vec3(1.0, 1.0, 1.0) * ScreenborderMask;

  color.rgb = col;
  color.a = 0.0;
}
