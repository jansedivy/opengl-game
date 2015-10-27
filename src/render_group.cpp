#include "render_group.h"

bool sort_function(const RenderCommand &a, const RenderCommand &b) {
  int depth_a = (a.flags & EntityFlags::RENDER_IGNORE_DEPTH) != 0 ? 1 : 0;
  int depth_b = (b.flags & EntityFlags::RENDER_IGNORE_DEPTH) != 0 ? 1 : 0;

  if (depth_a < depth_b) { return true; }
  if (depth_b < depth_a) { return false; }

  if (a.shader < b.shader) { return true; }
  if (b.shader < a.shader) { return false; }

  if (a.texture < b.texture) { return true; }
  if (b.texture < a.texture) { return false; }

  if (a.model_mesh < b.model_mesh) { return true; }
  if (b.model_mesh < a.model_mesh) { return false; }

  return false;
}

bool depth_record_sort_function(const RenderCommand &a, const RenderCommand &b) {
  if (a.texture < b.texture) { return true; }
  if (b.texture < a.texture) { return false; }

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

void change_shader(RenderGroup *group, App *app, Shader *shader) {
  use_program(app, shader);

  if (shader_has_uniform(app->current_program, "eye_position")) {
    set_uniform(app->current_program, "eye_position", app->camera.position);
  }

  if (shader_has_uniform(app->current_program, "uPMatrix")) {
    set_uniform(app->current_program, "uPMatrix", group->camera->view_matrix);
  }

  if (shader_has_uniform(app->current_program, "uShadow")) {
    set_uniformi(app->current_program, "uShadow", 0);
  }

  if (shader_has_uniform(app->current_program, "shadow_matrix")) {
    set_uniform(app->current_program, "shadow_matrix", app->shadow_camera.view_matrix);
  }

  if (shader_has_uniform(app->current_program, "texmapscale")) {
    set_uniform(app->current_program, "texmapscale", vec2(1.0f / app->shadow_width, 1.0f / app->shadow_height));
  }

  if (shader_has_uniform(app->current_program, "shadow_light_position")) {
    set_uniform(app->current_program, "shadow_light_position", app->shadow_camera.position);
  }
}

void end_render_group(App *app, RenderGroup *group, bool sort=true) {
  PROFILE_BLOCK("Render Group Blit");
  if (sort) {
    if (group->force_shader) {
      std::sort(group->commands.begin(), group->commands.end(), depth_record_sort_function);
    } else {
      std::sort(group->commands.begin(), group->commands.end(), sort_function);
    }
  }

  glEnable(GL_CULL_FACE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  set_depth_mode(group, GL_LESS, true);

  group->last_model = NULL;
  group->last_shader = NULL;

  if (group->shadow_pass) {
    glCullFace(GL_FRONT);
    group->cull_face = GL_FRONT;
  } else {
    glCullFace(GL_BACK);
    group->cull_face = GL_BACK;
  }

  if (group->force_shader) {
    change_shader(group, app, group->force_shader);
    group->last_shader = group->force_shader;
  }

  {
    PROFILE_BLOCK("Render Entity", group->commands.size());
    for (auto it = group->commands.begin(); it != group->commands.end(); it++) {
      if (group->force_shader == NULL) {
        if (group->last_shader != it->shader) {
          change_shader(group, app, it->shader);
          group->last_shader = it->shader;
        }
      }

      if (!group->shadow_pass && it->cull_type != group->cull_face) {
        glCullFace(it->cull_type);
        group->cull_face = it->cull_type;
      }

      if ((it->flags & EntityFlags::RENDER_WIREFRAME) != 0) {
        glEnable(GL_LINE_SMOOTH);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      }

      set_depth_mode(group, GL_LESS);

      if ((it->flags & EntityFlags::RENDER_IGNORE_DEPTH) != 0) {
        set_depth_mode(group, GL_ALWAYS);
      }

      if (shader_has_uniform(app->current_program, "uNMatrix")) {
        set_uniform(app->current_program, "uNMatrix", it->normal);
      }

      if (shader_has_uniform(app->current_program, "uMVMatrix")) {
        set_uniform(app->current_program, "uMVMatrix", it->model_view);
      }

      if (shader_has_uniform(app->current_program, "in_color")) {
        set_uniform(app->current_program, "in_color", it->color);
      }

      if (shader_has_uniform(app->current_program, "tint")) {
        set_uniform(app->current_program, "tint", it->tint);
      }

      if (shader_has_uniform(app->current_program, "znear")) {
        set_uniformf(app->current_program, "znear", app->camera.near);
      }

      if (shader_has_uniform(app->current_program, "zfar")) {
        set_uniformf(app->current_program, "zfar", app->camera.far);
      }

      if (shader_has_uniform(app->current_program, "camera_depth_texture")) {
        glActiveTexture(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_2D, app->frames[0].depth);
        set_uniformi(app->current_program, "camera_depth_texture", 2);
      }

      if (shader_has_uniform(app->current_program, "textureImage")) {
        if (it->texture && it->texture != group->last_texture) {
          glActiveTexture(GL_TEXTURE0 + 1);
          glBindTexture(GL_TEXTURE_2D, it->texture->id);
        }
        set_uniformi(app->current_program, "textureImage", 1);
      }

      if (group->last_model != it->model_mesh) {
        group->last_model = it->model_mesh;
      }
      use_model_mesh(app, it->model_mesh);

      u32 count = it->model_mesh->data.indices_count;

      glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    }
  }

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  set_depth_mode(group, GL_LESS, true);
  glPolygonOffset(0, 0);
}

void add_command_to_render_group(RenderGroup *group, RenderCommand command) {
  group->commands.push_back(command);
}
