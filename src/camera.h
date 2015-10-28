#pragma once

struct Camera {
  mat4 view_matrix;

  vec3 rotation;
  vec3 position;
  Frustum frustum;

  bool ortho = false;
  vec2 size;

  float far;
  float near;
};
