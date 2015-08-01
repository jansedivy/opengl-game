#include "render_group.h"

bool sort_function(const RenderCommand &a, const RenderCommand &b) {
  if (a.shader < b.shader) { return true; }
  if (b.shader < a.shader) { return false; }

  if (a.model_mesh < b.model_mesh) { return true; }
  if (b.model_mesh < a.model_mesh) { return false; }

  return false;
}

void start_render_group(RenderGroup *group) {
  group->commands.erase(group->commands.begin(), group->commands.end());

  group->last_model = 0;
  group->last_shader = 0;
  group->draw_calls = 0;
}

void end_render_group(App *app, RenderGroup *group) {
  std::sort(group->commands.begin(), group->commands.end(), sort_function);

  group->model_change = 0;
  group->shader_change = 0;

  for (auto it = group->commands.begin(); it != group->commands.end(); it++) {
    if (group->last_shader != it->shader) {
      use_program(app, it->shader);
      group->last_shader = it->shader;
      group->shader_change += 1;
    }

    send_shader_uniform(app->current_program, "uNMatrix", it->normal);
    send_shader_uniform(app->current_program, "uMVMatrix", it->model_view);
    send_shader_uniform(app->current_program, "in_color", it->color);

    if (group->last_model != it->model_mesh) {
      use_model_mesh(app, it->model_mesh);
      group->last_model = it->model_mesh;
      group->model_change += 1;
    }

    group->draw_calls += 1;

    glDrawElements(GL_TRIANGLES, it->model_mesh->data.indices_count, GL_UNSIGNED_INT, 0);
  }
}

void add_command_to_render_group(RenderGroup *group, RenderCommand command) {
  group->commands.push_back(command);
}
