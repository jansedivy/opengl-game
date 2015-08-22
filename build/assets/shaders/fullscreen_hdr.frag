#version 330

uniform sampler2D uSampler;

out vec4 color;

in vec2 pos;

void main() {
  /* const float exposure = 0.5; */

  vec2 uv = pos * 0.5 + 0.5;

  /* vec3 hdr_color = texture(uSampler, uv).rgb; */

  /* vec3 mapped = vec3(1.0) - exp(-hdr_color * exposure); */

  float gamma = 2.2;
  color = vec4(pow(texture(uSampler, uv).rgb, vec3(1.0/gamma)), 1.0);

/*   color = vec4(mapped, 1.0); */
}
