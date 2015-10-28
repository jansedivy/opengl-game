bool is_sphere_in_frustum(Frustum *frustum, vec3 position, float radius) {
  for (int i=0; i<6; i++) {
    float distance = distance_from_plane(frustum->planes[i], position);
    if (distance + radius < 0) {
      return false;
    }
  }

  return true;
}

void debug_render_frustum(App *app, Camera *camera) {
  vec4 hcorners[8];
  hcorners[0] = glm::vec4(-1, 1, 1, 1);
  hcorners[1] = glm::vec4(1, 1, 1, 1);
  hcorners[2] = glm::vec4(1, -1, 1, 1);
  hcorners[3] = glm::vec4(-1, -1, 1, 1);

  hcorners[4] = glm::vec4(-1, 1, -1, 1);
  hcorners[5] = glm::vec4(1, 1, -1, 1);
  hcorners[6] = glm::vec4(1, -1, -1, 1);
  hcorners[7] = glm::vec4(-1, -1, -1, 1);

  vec3 corners[8];
  glm::mat4 inverseProj = glm::inverse(camera->view_matrix);
  for (int i = 0; i < 8; i++) {
    hcorners[i] = inverseProj * hcorners[i];
    hcorners[i] /= hcorners[i].w;

    corners[i] = glm::vec3(hcorners[i]);
  }

  app->debug_lines.reserve(24);
  app->debug_lines.push_back(corners[0]);
  app->debug_lines.push_back(corners[1]);
  app->debug_lines.push_back(corners[1]);
  app->debug_lines.push_back(corners[2]);
  app->debug_lines.push_back(corners[2]);
  app->debug_lines.push_back(corners[3]);
  app->debug_lines.push_back(corners[3]);
  app->debug_lines.push_back(corners[0]);

  app->debug_lines.push_back(corners[0]);
  app->debug_lines.push_back(corners[4]);
  app->debug_lines.push_back(corners[1]);
  app->debug_lines.push_back(corners[5]);
  app->debug_lines.push_back(corners[2]);
  app->debug_lines.push_back(corners[6]);
  app->debug_lines.push_back(corners[3]);
  app->debug_lines.push_back(corners[7]);

  app->debug_lines.push_back(corners[4]);
  app->debug_lines.push_back(corners[5]);
  app->debug_lines.push_back(corners[5]);
  app->debug_lines.push_back(corners[6]);
  app->debug_lines.push_back(corners[6]);
  app->debug_lines.push_back(corners[7]);
  app->debug_lines.push_back(corners[7]);
  app->debug_lines.push_back(corners[4]);
}

mat4 get_camera_projection(Camera *camera) {
  PROFILE_BLOCK("Camera proj");
  mat4 projection;

  if (camera->ortho) {
    projection = glm::ortho(camera->size.x / -2.0f, camera->size.y / 2.0f, camera->size.x / 2.0f, camera->size.y / -2.0f, camera->near, camera->far);
  } else {
    projection = glm::perspective(glm::radians(75.0f), camera->size.x / camera->size.y, camera->near, camera->far);
  }

  return projection;
}

Ray get_mouse_ray(App *app, Input input, Memory *memory) {
  PROFILE_BLOCK("Mouse Ray");
  vec3 from = glm::unProject(
      vec3(input.mouse_x, memory->height - input.mouse_y, 0.0f),
      mat4(),
      app->camera.view_matrix,
      vec4(0.0f, 0.0f, memory->width, memory->height));

  vec3 to = glm::unProject(
      vec3(input.mouse_x, memory->height - input.mouse_y, 1.0f),
      mat4(),
      app->camera.view_matrix,
      vec4(0.0f, 0.0f, memory->width, memory->height));

  vec3 direction = glm::normalize(to - from);

  Ray ray;
  ray.start = from;
  ray.direction = direction;

  return ray;
}
