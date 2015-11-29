float get_terrain_height_at(float x, float y) {
  return (
    scaled_octave_noise_2d(1.0f, 1.0f, 10.0f, -100.0f, 500.0f, x / 40000.0f, y / 40000.0f) +
    scaled_octave_noise_2d(1.0f, 1.0f, 10.0f, 00.0f, 1000.0f, x / 400000.0f, y / 400000.0f) +
    scaled_octave_noise_2d(1.0f, 1.0f, 100.0f, 00.0f, 1000.0f, x / 800000.0f, y / 800000.0f) +

    0.0f
  );
}

void unload_chunk(TerrainChunk *chunk) {
  for (u32 i=0; i<array_count(chunk->models); i++) {
    Model *model = chunk->models + i;
    unload_model(model);
  }
}

TerrainChunk *get_chunk_at(TerrainChunk *chunks, u32 count, u32 x, u32 y) {
  u32 hash = 6269 * x + 8059 * y;
  u32 slot = hash % (count - 1);
  assert(slot < count);

  TerrainChunk *chunk = chunks + slot;

  do {
    if (chunk->initialized) {
      if (chunk->x == x && chunk->y == y) {
        break;
      }

      if (!chunk->next) {
        chunk->next = (TerrainChunk *)(malloc(sizeof(TerrainChunk)));
        chunk->next->initialized = false;
        chunk->next->prev = chunk;
      }
    } else {
      chunk->x = x;
      chunk->y = y;
      chunk->initialized = true;
      chunk->next = 0;
      chunk->models[0].state = AssetState::EMPTY;
      chunk->models[1].state = AssetState::EMPTY;
      chunk->models[2].state = AssetState::EMPTY;
      break;
    }

    chunk = chunk->next;
  } while (chunk);

  return chunk;
}

void generate_ground(Model *model, int chunk_x, int chunk_y, float detail) {
 PROFILE_BLOCK("Generate Ground");
  int size_x = CHUNK_SIZE_X;
  int size_y = CHUNK_SIZE_Y;

  int offset_x = chunk_x * size_x;
  int offset_y = chunk_y * size_y;

  int width = size_x * detail + 1;
  int height = size_y * detail + 1;

  u32 vertices_count = width * height * 3;
  u32 normals_count = vertices_count;
  u32 indices_count = (width - 1) * (height - 1) * 6;

  Mesh mesh;
  allocate_mesh(&mesh, vertices_count, normals_count, indices_count, 0);

  u32 vertices_index = 0;
  u32 normals_index = 0;
  u32 indices_index = 0;

  float radius = 0.0f;

  for (int x=0; x<width; x++) {
    for (int y=0; y<height; y++) {
      float x_coord = (float)(x) / detail;
      float y_coord = (float)(y) / detail;

      float value = get_terrain_height_at(x_coord + offset_x, y_coord + offset_y);

      mesh.data.vertices[vertices_index++] = x_coord;
      mesh.data.vertices[vertices_index++] = value;
      mesh.data.vertices[vertices_index++] = y_coord;

      // TODO(sedivy): calculate center
      float distance = glm::length(vec3(x_coord, value, y_coord));
      if (distance > radius) {
        radius = distance;
      }

      mesh.data.normals[normals_index++] = 0.0f;
      mesh.data.normals[normals_index++] = 0.0f;
      mesh.data.normals[normals_index++] = 0.0f;
    }
  }

  for (int i=0; i<height - 1; i++) {
    for (int l=0; l<width - 1; l++) {
      mesh.data.indices[indices_index++] = (height * l + i + 0);
      mesh.data.indices[indices_index++] = (height * l + i + 1);
      mesh.data.indices[indices_index++] = (height * l + i + height);

      mesh.data.indices[indices_index++] = (height * l + i + height);
      mesh.data.indices[indices_index++] = (height * l + i + 1);
      mesh.data.indices[indices_index++] = (height * l + i + height + 1);
    }
  }

  for (u32 i=0; i<indices_count; i += 3) {
    int indices_a = mesh.data.indices[i + 0] * 3;
    int indices_b = mesh.data.indices[i + 1] * 3;
    int indices_c = mesh.data.indices[i + 2] * 3;

    vec3 v0 = vec3(mesh.data.vertices[indices_a + 0],
                             mesh.data.vertices[indices_a + 1],
                             mesh.data.vertices[indices_a + 2]);

    vec3 v1 = vec3(mesh.data.vertices[indices_b + 0],
                             mesh.data.vertices[indices_b + 1],
                             mesh.data.vertices[indices_b + 2]);

    vec3 v2 = vec3(mesh.data.vertices[indices_c + 0],
                             mesh.data.vertices[indices_c + 1],
                             mesh.data.vertices[indices_c + 2]);

    vec3 normal = glm::normalize(glm::cross(v2 - v0, v1 - v0));

    mesh.data.normals[indices_a + 0] = -normal.x;
    mesh.data.normals[indices_a + 1] = -normal.y;
    mesh.data.normals[indices_a + 2] = -normal.z;

    mesh.data.normals[indices_b + 0] = -normal.x;
    mesh.data.normals[indices_b + 1] = -normal.y;
    mesh.data.normals[indices_b + 2] = -normal.z;

    mesh.data.normals[indices_c + 0] = -normal.x;
    mesh.data.normals[indices_c + 1] = -normal.y;
    mesh.data.normals[indices_c + 2] = -normal.z;
  }

  for (u32 i=0; i<normals_count / 3; i += 3) {
    float x = mesh.data.normals[i + 0];
    float y = mesh.data.normals[i + 1];
    float z = mesh.data.normals[i + 2];

    vec3 normal = glm::normalize(vec3(x, y, z));

    mesh.data.normals[i + 0] = normal.x;
    mesh.data.normals[i + 1] = normal.y;
    mesh.data.normals[i + 2] = normal.z;
  }

  model->id_name = allocate_string("chunk");
  model->mesh = mesh;
  model->radius = radius;
}

struct GenerateGrountWorkData {
  TerrainChunk *chunk;
  int detail_level;
};

void generate_ground_work(void *data) {
  auto *work = (GenerateGrountWorkData*)(data);

  TerrainChunk *chunk = work->chunk;

  Model *model = chunk->models + work->detail_level;

  if (platform.atomic_exchange(&model->state, AssetState::EMPTY, AssetState::PROCESSING)) {
    float resolution = 0.0f;
    if (work->detail_level == 0) {
      resolution = 0.03f;
    } else if (work->detail_level == 1) {
      resolution = 0.01f;
    } else if (work->detail_level == 2) {
      resolution = 0.005f;
    }

    generate_ground(model, chunk->x, chunk->y, resolution);

    optimize_model(model);

    model->state = AssetState::HAS_DATA;
  }

  free(work);
}

inline bool process_terrain(Memory *memory, TerrainChunk *chunk, int detail_level) {
  Model *model = chunk->models + detail_level;

  if (model->state == AssetState::INITIALIZED) {
    return false;
  }

  if (model->state == AssetState::HAS_DATA) {
    initialize_model(model);
    return true;
  }

  if (platform.queue_has_free_spot(memory->main_queue)) {
    if (model->state == AssetState::EMPTY) {
      auto *work = (GenerateGrountWorkData*)(malloc(sizeof(GenerateGrountWorkData)));
      work->chunk = chunk;
      work->detail_level = detail_level;

      platform.add_work(memory->main_queue, generate_ground_work, work);

      return true;
    }
  }

  return true;
}


Model *chunk_get_model(Memory *memory, TerrainChunk *chunk, int detail_level) {
  if (process_terrain(memory, chunk, detail_level)) {
    for (u32 i=0; i<array_count(chunk->models); i++) {
      Model *model = chunk->models + i;
      if (model->state == AssetState::INITIALIZED) {
        return model;
      }
    }
    return NULL;
  }

  return chunk->models + detail_level;
}

void render_terrain_chunk(App *app, TerrainChunk *chunk, Model *model) {
  mat4 model_view;
  model_view = glm::translate(model_view, vec3(chunk->x * CHUNK_SIZE_X, 0.0f, chunk->y * CHUNK_SIZE_Y));

  mat3 normal = glm::inverseTranspose(mat3(model_view));

  if (shader_has_uniform(app->current_program, "uNMatrix")) {
    set_uniform(app->current_program, "uNMatrix", normal);
  }

  if (shader_has_uniform(app->current_program, "uMVMatrix")) {
    set_uniform(app->current_program, "uMVMatrix", model_view);
  }

  if (shader_has_uniform(app->current_program, "in_color")) {
    set_uniform(app->current_program, "in_color", vec4(0.0f, 0.3f, 0.1f, 1.0f));
  }

  use_model_mesh(app, &model->mesh);
  glDrawElements(GL_TRIANGLES, model->mesh.data.indices_count, GL_UNSIGNED_INT, 0);
}

void rebuild_chunks(App *app) {
  PROFILE_BLOCK("Unload Chunk", app->chunk_cache_count);
  for (u32 i=0; i<app->chunk_cache_count; i++) {
    TerrainChunk *chunk = app->chunk_cache + i;
    unload_chunk(chunk);
  }
}
