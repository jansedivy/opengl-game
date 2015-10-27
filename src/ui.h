#pragma once

namespace UICommandType {
  enum UICommandType {
    NONE = 0,
    RECT,
    TEXT
  };
}

struct UICommand {
  vec4 color;
  vec4 image_color;
  u32 vertices_count;
  UICommandType::UICommandType type;
  GLuint texture_id;
};

struct UICommandBuffer {
  std::vector<GLfloat> vertices;
  std::vector<UICommand> commands;
};

struct DebugDrawState {
  float offset_top = 0.0f;
  float width = 0.0f;
  float next_width = 0.0f;
  UICommandBuffer *command_buffer;

  bool set_pushing = false;
  bool push_down = true;
  u32 set_count = 1;
  u32 pushed_count = 0;
};
