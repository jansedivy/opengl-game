#pragma once

struct Camera {
  mat4 view_matrix;

  quat rotation;
  WorldPosition position;
  Frustum frustum;

  bool ortho = false;
  vec2 size;

  float far;
  float near;
};
