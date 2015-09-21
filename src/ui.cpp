void draw_string(UICommandBuffer *command_buffer, Font *font, float x, float y, char *text, vec3 color=vec3(1.0f, 1.0f, 1.0f)) {
  PROFILE_BLOCK("Draw String");
  y = glm::round(y);
  x = glm::round(x);

  float font_x = 0, font_y = font->size;
  stbtt_aligned_quad q;

  UICommand command;
  command.type = UICommandType::TEXT;
  command.vertices_count = 0;
  command.color = vec4(color, 1.0f);
  command.image_color = vec4(color, 1.0f);
  command.color = vec4(0.0f);
  command.texture_id = font->texture;

  while (*text != '\0') {
    font_get_quad(font, *text++, &font_x, &font_y, &q);

    GLfloat vertices_data[] = {
      x + q.x0, y + q.y0, // top left
      q.s0, q.t0,

      x + q.x0, y + q.y1, // bottom left
      q.s0, q.t1,

      x + q.x1, y + q.y0, // top right
      q.s1, q.t0,

      x + q.x1, y + q.y0, // top right
      q.s1, q.t0,

      x + q.x0, y + q.y1, // bottom left
      q.s0, q.t1,

      x + q.x1, y + q.y1, // bottom right
      q.s1, q.t1
    };

    for (u32 i=0; i<array_count(vertices_data); i++) {
      command_buffer->vertices.push_back(vertices_data[i]);
    }
    command.vertices_count += 6;
  }

  command_buffer->commands.push_back(command);
}

void debug_render_rect(UICommandBuffer *command_buffer, float x, float y, float width, float height, vec4 color, vec4 image_color=vec4(0.0f), u32 x_index=0, u32 y_index=0, Texture* texture=NULL) {
  float scale = 1.0f / 4.0f;

  command_buffer->vertices.push_back(x);
  command_buffer->vertices.push_back(y);

  command_buffer->vertices.push_back(scale * x_index + 0.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 0.0f * scale);

  command_buffer->vertices.push_back(x + width);
  command_buffer->vertices.push_back(y);

  command_buffer->vertices.push_back(scale * x_index + 1.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 0.0f * scale);

  command_buffer->vertices.push_back(x);
  command_buffer->vertices.push_back(y + height);

  command_buffer->vertices.push_back(scale * x_index + 0.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 1.0f * scale);

  command_buffer->vertices.push_back(x + width);
  command_buffer->vertices.push_back(y);

  command_buffer->vertices.push_back(scale * x_index + 1.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 0.0f * scale);

  command_buffer->vertices.push_back(x + width);
  command_buffer->vertices.push_back(y + height);

  command_buffer->vertices.push_back(scale * x_index + 1.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 1.0f * scale);

  command_buffer->vertices.push_back(x);
  command_buffer->vertices.push_back(y + height);

  command_buffer->vertices.push_back(scale * x_index + 0.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 1.0f * scale);

  UICommand command;
  command.vertices_count = 6;
  command.color = color;
  command.image_color = image_color;
  command.type = UICommandType::RECT;
  if (texture) {
    command.texture_id = texture->id;
  }

  command_buffer->commands.push_back(command);
}

void debug_layout_set(DebugDrawState *state, u32 count) {
  state->push_down = false;
  state->set_count = count;
  state->set_pushing = true;
}

void debug_layout_reset(DebugDrawState *state) {
  state->pushed_count = 0;
  state->push_down = true;
  state->set_count = 1;
  state->set_pushing = false;
}

bool push_debug_button(Input &input,
                       App *app,
                       DebugDrawState *state,
                       UICommandBuffer *command_buffer,
                       float x, float height,
                       char *text,
                       vec3 color,
                       vec4 background_color) {
  PROFILE_BLOCK("Push Debug Button");

  float min_x = (state->width / state->set_count) * state->pushed_count + x;
  float min_y = state->offset_top;
  float max_x = min_x + state->width / state->set_count;
  float max_y = min_y + height;

  bool clicked = false;

  vec4 button_background = background_color;

  if (!input.is_mouse_locked) {
    if (!input.left_mouse_down && input.mouse_x > min_x && input.mouse_y > min_y && input.mouse_x < max_x && input.mouse_y < max_y) {
      button_background = shade_color(background_color, 0.3f);
    }

    if (input.original_mouse_down_x > min_x && input.original_mouse_down_y > min_y && input.original_mouse_down_x < max_x && input.original_mouse_down_y < max_y &&
        input.mouse_x > min_x && input.mouse_y > min_y && input.mouse_x < max_x && input.mouse_y < max_y) {
      if (input.left_mouse_down) {
        button_background = shade_color(background_color, -0.3f);
        input.mouse_click = false;
      }

      if (input.mouse_up) {
        clicked = true;
        input.mouse_click = false;
        input.mouse_up = false;
      }
    }
  }

  debug_render_rect(command_buffer, min_x, min_y, max_x - min_x, max_y - min_y, button_background);
  draw_string(command_buffer, &app->font, min_x + 5, state->offset_top + ((max_y - min_y) - 23.0f) / 2.0f, text, color);

  if (state->set_pushing) {
    state->pushed_count += 1;
  }

  if (state->push_down) {
    state->offset_top += height;
  }

  return clicked;
}

void push_debug_text(App *app, Font *font, DebugDrawState *state, UICommandBuffer *command_buffer, float x, char *text, vec3 color, vec4 background_color) {
  PROFILE_BLOCK("Push Debug Text");
  debug_render_rect(command_buffer, x, state->offset_top, state->width, 25.0f, background_color);
  draw_string(command_buffer, font, x + 5, state->offset_top, text, color);
  state->offset_top += 25.0f;
}

void push_debug_text(App *app, DebugDrawState *state, UICommandBuffer *command_buffer, float x, char *text, vec3 color, vec4 background_color) {
  push_debug_text(app, &app->font, state, command_buffer, x, text, color, background_color);
}

void debug_render_range(Input &input, UICommandBuffer *command_buffer, float x, float y, float width, float height, vec4 background_color, float *value, float min, float max) {
  PROFILE_BLOCK("Push Debug Range");
  float scale = *value / (max - min);

  vec4 bg_color = vec4(0.5f, 1.0f, 0.2f, 1.0f);

  float min_x = x;
  float min_y = y;
  float max_x = min_x + width;
  float max_y = min_y + height;

  if (!input.is_mouse_locked) {
    if (input.mouse_x > min_x && input.mouse_y > min_y && input.mouse_x < max_x && input.mouse_y < max_y) {
      bg_color = shade_color(bg_color, -0.2f);
      background_color = shade_color(background_color, -0.4f);
    }
  }

  debug_render_rect(command_buffer, x, y, width, height, background_color);
  debug_render_rect(command_buffer, x, y, width * scale, height, bg_color);

  if (input.left_mouse_down && input.original_mouse_down_x > min_x && input.original_mouse_down_y > min_y && input.original_mouse_down_x < max_x && input.original_mouse_down_y < max_y) {
    *value = glm::clamp((input.mouse_x - min_x) / width * (max - min), min, max);
    input.mouse_click = false;
  }
}

void push_debug_editable_vector(Input &input, DebugDrawState *state, Font *font, UICommandBuffer *command_buffer, float x, const char *name, vec3 *vector, vec4 background_color, float min, float max) {
  char text[256];

  debug_render_rect(command_buffer, x, state->offset_top, state->width, 25.0f, background_color);

  sprintf(text, "%s", name);
  draw_string(command_buffer, font, x + 5.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->x);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->x, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->y);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->y, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->z);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->z, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;
}

void push_debug_editable_vector(Input &input, DebugDrawState *state, Font *font, UICommandBuffer *command_buffer, float x, const char *name, vec4 *vector, vec4 background_color, float min, float max) {
  PROFILE_BLOCK("Push Debug Vector");
  char text[256];

  debug_render_rect(command_buffer, x, state->offset_top, state->width, 25.0f, background_color);

  sprintf(text, "%s", name);
  draw_string(command_buffer, font, x + 5.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->x);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->x, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->y);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->y, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->z);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->z, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->w);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->w, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;
}

void push_debug_editable_quat(Input &input, DebugDrawState *state, Font *font, UICommandBuffer *command_buffer, float x, const char *name, quat *vector, vec4 background_color, float min, float max) {
  PROFILE_BLOCK("Push Debug Quat");
  char text[256];

  debug_render_rect(command_buffer, x, state->offset_top, state->width, 25.0f, background_color);

  sprintf(text, "%s", name);
  draw_string(command_buffer, font, x + 5.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->x);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->x, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->y);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->y, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->z);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->z, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->w);
  debug_render_range(input, command_buffer, x, state->offset_top, state->width, 25.0f, background_color, &vector->w, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;
}

void push_debug_vector(DebugDrawState *state, Font *font, UICommandBuffer *command_buffer, float x, const char *name, vec3 vector, vec4 background_color) {
  PROFILE_BLOCK("Push Debug Vector");
  char text[256];

  debug_render_rect(command_buffer, x, state->offset_top, state->width, 25.0f * 4.0f, background_color);

  sprintf(text, "%s", name);
  draw_string(command_buffer, font, x + 5.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector.x);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector.y);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector.z);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;
}



void push_toggle_button(App *app, Input &input, DebugDrawState *draw_state, UICommandBuffer *command_buffer, float x, bool *value, vec4 background_color) {
  PROFILE_BLOCK("Push Debug Toggle");
  u32 toggl_button_x_index;
  u32 toggl_button_y_index;

  if (*value) {
    toggl_button_x_index = 0;
    toggl_button_y_index = 0;
  } else {
    toggl_button_x_index = 1;
    toggl_button_y_index = 0;
  }

  float image_height = 30.0f;
  if (push_debug_button(input, app, draw_state, command_buffer, x, image_height, (char *)"", vec3(0.0f), background_color)) {
    *value = !(*value);
  }

  debug_render_rect(command_buffer, x, draw_state->offset_top - image_height, image_height, image_height, vec4(0.0f, 0.0f, 0.0f, 0.0f), vec4(0.4f, 0.0f, 0.0f, 1.0f), toggl_button_x_index, toggl_button_y_index, &app->editor_texture);
}