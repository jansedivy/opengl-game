#pragma once

struct Camera {
  mat4 view_matrix;

  quat orientation;
  WorldPosition position;
  Frustum frustum;

  bool ortho = false;
  vec2 size;

  float far;
  float near;
};
