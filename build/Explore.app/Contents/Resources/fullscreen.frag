#version 330

uniform sampler2D uSampler;
uniform sampler2D uDepth;
uniform sampler2D uGradient;

uniform float znear;
uniform float zfar;

out vec4 color;

in vec2 pos;

void main() {
  vec2 uv = pos * 0.5 + 0.5;

  float depth = texture(uDepth, uv).r;
  vec3 image = texture(uSampler, uv).rgb;

  depth = clamp((2.0 * znear) / (zfar + znear - depth * (zfar - znear)), 0.0, 1.0);

  vec4 gradient = texture(uGradient, vec2(clamp(depth, 0.001, 1.0), 0.0));

  color = vec4(mix(image, gradient.rgb, gradient.a), 1.0);
}
