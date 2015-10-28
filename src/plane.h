#pragma once

struct Plane {
  vec3 normal;
  float distance;
};

struct Frustum {
  Plane planes[6];
};

enum FrustumPlaneType {
  LeftPlane = 0,
  RightPlane = 1,
  TopPlane = 2,
  BottomPlane = 3,
  NearPlane = 4,
  FarPlane = 5
};
