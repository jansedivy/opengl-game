#version 330

uniform sampler2D uSampler;

out vec4 color;

in vec2 pos;

void main() {
  vec2 uv = pos * 0.5 + 0.5;

  color = vec4(pow(texture(uSampler, uv).rgb, vec3(1.0 / 2.2)), 1.0);
}
