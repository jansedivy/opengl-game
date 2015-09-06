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
  float offset_top = 0;
  float width = 0;
  UICommandBuffer *command_buffer;
};
