#version 330

layout (location = 0) out vec4 color_out;
layout (location = 1) out vec4 alpha_out;

uniform vec4 in_color;

vec4 w(vec4 color, float depth) {
  return vec4(1.0); // TODO(sedivy): fill actual value
}

void main() {
  vec4 zi = vec4(0.0); // TODO(sedivy): fill actual value

  color_out = vec4(in_color.rgb, in_color.a) * w(zi, in_color.a);
  alpha_out = vec4(in_color.a);
}
