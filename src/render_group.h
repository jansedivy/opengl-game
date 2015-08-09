#pragma once

#include <algorithm>

struct RenderCommand {
  Shader *shader;

  glm::mat4 model_view;
  glm::mat3 normal;
  glm::vec4 color;

  GLenum cull_type = GL_BACK;

  u32 render_flags;
  Mesh *model_mesh;
};

struct RenderGroup {
  std::vector<RenderCommand> commands;

  Mesh *last_model;
  Shader *last_shader;

  u32 model_change;
  u32 shader_change;
  u32 draw_calls;

  GLenum depth_mode;
};
