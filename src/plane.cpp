void normalize_plane(Plane *plane) {
  float mag = glm::sqrt(plane->normal.x * plane->normal.x + plane->normal.y * plane->normal.y + plane->normal.z * plane->normal.z);
  plane->normal = plane->normal / mag;
  plane->distance = plane->distance / mag;
}

float distance_from_plane(Plane plane, vec3 position) {
  return glm::dot(plane.normal, position) + plane.distance;
}

void fill_frustum_with_matrix(Frustum *frustum, mat4 matrix) {
  frustum->planes[LeftPlane].normal.x = matrix[0][3] + matrix[0][0];
  frustum->planes[LeftPlane].normal.y = matrix[1][3] + matrix[1][0];
  frustum->planes[LeftPlane].normal.z = matrix[2][3] + matrix[2][0];
  frustum->planes[LeftPlane].distance = matrix[3][3] + matrix[3][0];

  frustum->planes[RightPlane].normal.x = matrix[0][3] - matrix[0][0];
  frustum->planes[RightPlane].normal.y = matrix[1][3] - matrix[1][0];
  frustum->planes[RightPlane].normal.z = matrix[2][3] - matrix[2][0];
  frustum->planes[RightPlane].distance = matrix[3][3] - matrix[3][0];

  frustum->planes[TopPlane].normal.x = matrix[0][3] - matrix[0][1];
  frustum->planes[TopPlane].normal.y = matrix[1][3] - matrix[1][1];
  frustum->planes[TopPlane].normal.z = matrix[2][3] - matrix[2][1];
  frustum->planes[TopPlane].distance = matrix[3][3] - matrix[3][1];

  frustum->planes[BottomPlane].normal.x = matrix[0][3] + matrix[0][1];
  frustum->planes[BottomPlane].normal.y = matrix[1][3] + matrix[1][1];
  frustum->planes[BottomPlane].normal.z = matrix[2][3] + matrix[2][1];
  frustum->planes[BottomPlane].distance = matrix[3][3] + matrix[3][1];

  frustum->planes[NearPlane].normal.x = matrix[0][3] + matrix[0][2];
  frustum->planes[NearPlane].normal.y = matrix[1][3] + matrix[1][2];
  frustum->planes[NearPlane].normal.z = matrix[2][3] + matrix[2][2];
  frustum->planes[NearPlane].distance = matrix[3][3] + matrix[3][2];

  frustum->planes[FarPlane].normal.x = matrix[0][3] - matrix[0][2];
  frustum->planes[FarPlane].normal.y = matrix[1][3] - matrix[1][2];
  frustum->planes[FarPlane].normal.z = matrix[2][3] - matrix[2][2];
  frustum->planes[FarPlane].distance = matrix[3][3] - matrix[3][2];

  for(int i=0; i<6; i++) {
    normalize_plane(frustum->planes +i);
  }
}

