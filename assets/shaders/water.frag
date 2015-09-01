#version 330 
out vec4 color;

in vec3 inNormals;
in vec4 inPosition;
in vec2 uvs;
in vec4 clip_space;

uniform vec4 in_color;
uniform float znear;
uniform float zfar;

uniform sampler2D camera_depth_texture;

void main() {

  vec2 texture_uv = (clip_space.xy / clip_space.w) / 2.0 + 0.5;

  vec3 normals = normalize(inNormals);
  float depth = texture(camera_depth_texture, texture_uv).r;
  float depth_a = clamp((2.0 * znear) / (zfar + znear - depth * (zfar - znear)), 0.0, 1.0);

  depth = gl_FragCoord.z;
  float depth_b = clamp((2.0 * znear) / (zfar + znear - depth * (zfar - znear)), 0.0, 1.0);

  color = vec4(vec3(depth_a - depth_b), color.a);
}
