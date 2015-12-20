#pragma once

struct EditorHandleRenderCommand {
  float distance_from_camera;
  mat4 model_view;
  vec4 color;
};

namespace EditorLeftState {
  enum EditorLeftState {
    MODELING,
    TERRAIN,
    EDITOR_SETTINGS,
    POST_PROCESSING,
    LIGHT,
  };
}

namespace EditorRightState {
  enum EditorRightState {
    COMMON,
    SPECIFIC
  };
}

struct Editor {
  bool holding_entity = false;
  bool hovering_entity = false;
  bool inspect_entity = false;
  Pid entity_id;
  Pid hover_entity;
  float distance_from_entity_offset;
  vec3 hold_offset;
  float handle_size;

  bool show_left = true;
  bool show_right = true;

  bool show_handles = true;
  bool show_performance = false;

  bool show_camera_frustum = false;

  bool experimental_terrain_entity_movement = false;

  float speed;

  EditorLeftState::EditorLeftState left_state;
  EditorRightState::EditorRightState right_state;

  UICommandBuffer command_buffer;
};

