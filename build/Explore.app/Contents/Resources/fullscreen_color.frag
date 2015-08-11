#version 330

uniform sampler2D uSampler;
uniform sampler3D color_correction_texture;

out vec4 color;

in vec2 pos;

const vec3 scale = vec3((16.0 - 1.0) / 16.0);
const vec3 offset = vec3(1.0 / (2.0 * 16.0));

void main() {
  vec2 uv = pos * 0.5 + 0.5;

  vec4 col = texture(uSampler, uv);

  color = texture(color_correction_texture, scale * col.xzy + offset);
}
