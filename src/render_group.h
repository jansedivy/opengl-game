#pragma once

#include <algorithm>

struct RenderCommand {
  Shader *shader;

  glm::mat4 model_view;
  glm::mat3 normal;
  glm::vec4 color;
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

  u32 model_change;
  u32 shader_change;
  u32 draw_calls;

  GLenum depth_mode;
  GLenum cull_face;

  Camera *camera;
  bool transparent_pass;
  bool shadow_pass;
};
