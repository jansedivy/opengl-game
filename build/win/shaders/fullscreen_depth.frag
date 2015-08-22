#version 330

uniform sampler2D uDepth;

uniform float znear;
uniform float zfar;

out vec4 color;

in vec2 pos;

void main() {
  vec2 uv = pos * 0.5 + 0.5;

  float depth = texture(uDepth, uv).r;

  depth = clamp((2.0 * znear) / (zfar + znear - depth * (zfar - znear)), 0.0, 1.0);

  color = vec4(vec3(depth), 1.0);
}
