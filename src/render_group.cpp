#include "render_group.h"

bool sort_function(const RenderCommand &a, const RenderCommand &b) {
  int depth_a = (a.flags & EntityFlags::RENDER_IGNORE_DEPTH) != 0 ? 1 : 0;
  int depth_b = (b.flags & EntityFlags::RENDER_IGNORE_DEPTH) != 0 ? 1 : 0;

  if (depth_a < depth_b) { return true; }
  if (depth_b < depth_a) { return false; }

  if (a.shader < b.shader) { return true; }
  if (b.shader < a.shader) { return false; }

  if (a.model_mesh < b.model_mesh) { return true; }
  if (b.model_mesh < a.model_mesh) { return false; }

  return false;
}

void start_render_group(RenderGroup *group) {
  group->commands.erase(group->commands.begin(), group->commands.end());
}

inline void set_depth_mode(RenderGroup *group, GLenum mode, bool force=false) {
  if (force || group->depth_mode != mode) {
    glDepthFunc(mode);
    group->depth_mode = mode;
  }
}

void end_render_group(App *app, RenderGroup *group) {
  std::sort(group->commands.begin(), group->commands.end(), sort_function);

  glEnable(GL_CULL_FACE);

  group->model_change = 0;
  group->shader_change = 0;

  group->last_model = 0;
  group->last_shader = 0;
  group->draw_calls = 0;

  set_depth_mode(group, GL_LESS, true);

  glCullFace(GL_FRONT);
  group->cull_face = GL_FRONT;

  for (auto it = group->commands.begin(); it != group->commands.end(); it++) {
    if (group->last_shader != it->shader) {
      use_program(app, it->shader);
      group->last_shader = it->shader;
      group->shader_change += 1;
    }

    if (it->cull_type != group->cull_face) {
      glCullFace(it->cull_type);
      group->cull_face = it->cull_type;
    }

    if ((it->flags & EntityFlags::RENDER_IGNORE_DEPTH) != 0) {
      set_depth_mode(group, GL_ALWAYS);
    } else {
      set_depth_mode(group, GL_LESS);
    }

    if (shader_has_uniform(app->current_program, "uNMatrix")) {
      send_shader_uniform(app->current_program, "uNMatrix", it->normal);
    }

    if (shader_has_uniform(app->current_program, "uMVMatrix")) {
      send_shader_uniform(app->current_program, "uMVMatrix", it->model_view);
    }

    if (shader_has_uniform(app->current_program, "in_color")) {
      send_shader_uniform(app->current_program, "in_color", it->color);
    }

    group->draw_calls += 1;

    if (group->last_model != it->model_mesh) {
      group->last_model = it->model_mesh;
      group->model_change += 1;
      use_model_mesh(app, it->model_mesh);
    }

    glDrawElements(GL_TRIANGLES, it->model_mesh->data.indices_count, GL_UNSIGNED_INT, 0);
  }
}

void add_command_to_render_group(RenderGroup *group, RenderCommand command) {
  group->commands.push_back(command);
}
