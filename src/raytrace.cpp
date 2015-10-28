RayMatchResult ray_match_sphere(Ray ray, vec3 position, float radius) {
  PROFILE_BLOCK("Ray Sphere");
  vec3 result_position;
  vec3 result_normal;

  RayMatchResult result;
  result.hit = intersectRaySphere(ray.start, ray.direction, position, radius, result_position, result_normal);
  if (result.hit) {
    result.hit_position = result_position;
  }

  return result;
}

RayMatchResult ray_match_terrain(App *app, Ray ray) {
  PROFILE_BLOCK("Ray Terrain");
  RayMatchResult result;

  for (u32 i=0; i<app->chunk_cache_count; i++) {
    TerrainChunk *chunk = app->chunk_cache + i;
    if (chunk->initialized) {
      Model *model = NULL;

      for (u32 model_index=0; model_index<array_count(chunk->models); model_index++) {
        Model *item = chunk->models + model_index;
        if (item->state == AssetState::INITIALIZED) {
          model = item;
          break;
        }
      }

      if (model && model->state == AssetState::INITIALIZED) {
        RayMatchResult sphere = ray_match_sphere(ray, vec3(chunk->x * CHUNK_SIZE_X, 0.0f, chunk->y * CHUNK_SIZE_Y), model->radius);
        if (!sphere.hit) {
          continue;
        }

        mat4 model_view;
        model_view = glm::translate(model_view, vec3(chunk->x * CHUNK_SIZE_X, 0.0f, chunk->y * CHUNK_SIZE_Y));
        mat4 res = glm::inverse(model_view);

        vec3 start = vec3(res * vec4(ray.start, 1.0f));
        vec3 direction = vec3(res * vec4(ray.direction, 0.0f));

        ModelData mesh = model->mesh.data;

        for (u32 i=0; i<mesh.indices_count; i += 3) {
          int indices_a = mesh.indices[i + 0] * 3;
          int indices_b = mesh.indices[i + 1] * 3;
          int indices_c = mesh.indices[i + 2] * 3;

          vec3 a = vec3(mesh.vertices[indices_a + 0],
              mesh.vertices[indices_a + 1],
              mesh.vertices[indices_a + 2]);

          vec3 b = vec3(mesh.vertices[indices_b + 0],
              mesh.vertices[indices_b + 1],
              mesh.vertices[indices_b + 2]);

          vec3 c = vec3(mesh.vertices[indices_c + 0],
              mesh.vertices[indices_c + 1],
              mesh.vertices[indices_c + 2]);

          vec3 result_position;

          if (glm::intersectRayTriangle(start, direction, a, b, c, result_position)) {
            result.hit_position = ray.start + ray.direction * result_position.z;
            result.hit = true;
            return result;
          }
        }

        continue;
      }
    }
  }

  result.hit = false;
  return result;
}

RayMatchResult ray_match_entity(App *app, Ray ray, Entity *entity) {
  RayMatchResult result;

  if (entity->header.model == NULL || entity->header.model->state != AssetState::INITIALIZED) {
    result.hit = false;
    return result;
  }

  RayMatchResult sphere = ray_match_sphere(ray, entity->header.position, entity->header.model->radius * glm::compMax(entity->header.scale));
  if (!sphere.hit) {
    result.hit = false;
    return result;
  }
  PROFILE_BLOCK("Ray Entity");

  mat4 model_view;
  model_view = glm::translate(model_view, entity->header.position);

  if (entity->header.flags & EntityFlags::LOOK_AT_CAMERA) {
    model_view *= make_billboard_matrix(entity->header.position, app->camera.position, vec3(app->camera.view_matrix[0][1], app->camera.view_matrix[1][1], app->camera.view_matrix[2][1]));
  }

  model_view = glm::rotate(model_view, entity->header.rotation.x, vec3(1.0, 0.0, 0.0));
  model_view = glm::rotate(model_view, entity->header.rotation.y, vec3(0.0, 1.0, 0.0));
  model_view = glm::rotate(model_view, entity->header.rotation.z, vec3(0.0, 0.0, 1.0));

  model_view = glm::scale(model_view, entity->header.scale);

  mat4 res = glm::inverse(model_view);

  vec3 start = vec3(res * vec4(ray.start, 1.0f));
  vec3 direction = vec3(res * vec4(ray.direction, 0.0f));

  ModelData mesh = entity->header.model->mesh.data;

  for (u32 i=0; i<mesh.indices_count; i += 3) {
    int indices_a = mesh.indices[i + 0] * 3;
    int indices_b = mesh.indices[i + 1] * 3;
    int indices_c = mesh.indices[i + 2] * 3;

    vec3 a = vec3(mesh.vertices[indices_a + 0],
                             mesh.vertices[indices_a + 1],
                             mesh.vertices[indices_a + 2]);

    vec3 b = vec3(mesh.vertices[indices_b + 0],
                             mesh.vertices[indices_b + 1],
                             mesh.vertices[indices_b + 2]);

    vec3 c = vec3(mesh.vertices[indices_c + 0],
                             mesh.vertices[indices_c + 1],
                             mesh.vertices[indices_c + 2]);

    vec3 result_position;

    if (glm::intersectRayTriangle(start, direction, a, b, c, result_position)) {
      result.hit_position = ray.start + ray.direction * result_position.z;
      result.hit = true;
      return result;
    }
  }

  result.hit = false;
  return result;
}

