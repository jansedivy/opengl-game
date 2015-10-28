#pragma once

struct RayMatchResult {
  bool hit;
  vec3 hit_position;
};

struct Ray {
  vec3 start;
  vec3 direction;
};
