#pragma once

#include <algorithm>

struct RenderCommand {
  Shader *shader;

  mat4 model_view;
  mat3 normal;
  vec4 color;
  vec3 tint;
  float distance_from_camera;

  GLenum cull_type = GL_BACK;

  u32 flags;
  Mesh *model_mesh;
  Texture *texture = 0;
};

struct RenderGroup {
  std::vector<RenderCommand> commands;

  Mesh *last_model;
  Shader *last_shader;
  Texture *last_texture;
  Shader *force_shader;

  GLenum depth_mode;
  GLenum cull_face;

  Camera *camera;
  bool shadow_pass;
};
