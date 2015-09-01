#pragma once

void unload_chunk(TerrainChunk *chunk) {
  for (u32 i=0; i<array_count(chunk->models); i++) {
    Model *model = chunk->models + i;
    unload_model(model);
  }
  chunk->has_data = false;
  chunk->is_being_loaded = false;
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
        chunk->next = static_cast<TerrainChunk *>(malloc(sizeof(TerrainChunk)));
        chunk->next->initialized = false;
        chunk->next->prev = chunk;
      }
    } else {
      chunk->x = x;
      chunk->y = y;
      chunk->initialized = true;
      chunk->next = 0;
      chunk->models[0].initialized = false;
      chunk->has_data = false;
      chunk->is_being_loaded = false;
      break;
    }

    chunk = chunk->next;
  } while (chunk);

  return chunk;
}

Model generate_ground(int chunk_x, int chunk_y, float detail) {
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
      float x_coord = static_cast<float>(x) / detail;
      float y_coord = static_cast<float>(y) / detail;

      float value = get_terrain_height_at(x_coord + offset_x, y_coord + offset_y);

      mesh.data.vertices[vertices_index++] = x_coord;
      mesh.data.vertices[vertices_index++] = value;
      mesh.data.vertices[vertices_index++] = y_coord;

      // TODO(sedivy): calculate center
      float distance = glm::length(glm::vec3(x_coord, value, y_coord));
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

    glm::vec3 v0 = glm::vec3(mesh.data.vertices[indices_a + 0],
                             mesh.data.vertices[indices_a + 1],
                             mesh.data.vertices[indices_a + 2]);

    glm::vec3 v1 = glm::vec3(mesh.data.vertices[indices_b + 0],
                             mesh.data.vertices[indices_b + 1],
                             mesh.data.vertices[indices_b + 2]);

    glm::vec3 v2 = glm::vec3(mesh.data.vertices[indices_c + 0],
                             mesh.data.vertices[indices_c + 1],
                             mesh.data.vertices[indices_c + 2]);

    glm::vec3 normal = glm::normalize(glm::cross(v2 - v0, v1 - v0));

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

    glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));

    mesh.data.normals[i + 0] = normal.x;
    mesh.data.normals[i + 1] = normal.y;
    mesh.data.normals[i + 2] = normal.z;
  }

  Model model;
  model.id_name = allocate_string("chunk");
  model.path = allocate_string("chunk");
  model.mesh = mesh;
  model.initialized = false;
  model.has_data = true;
  model.is_being_loaded = false;
  model.radius = radius;

  return model;
}

void generate_ground_work(void *data) {
  TerrainChunk *chunk = static_cast<TerrainChunk*>(data);
  chunk->models[0] = generate_ground(chunk->x, chunk->y, 0.03f);
  chunk->models[1] = generate_ground(chunk->x, chunk->y, 0.01f);
  chunk->models[2] = generate_ground(chunk->x, chunk->y, 0.005f);

  optimize_model(chunk->models + 0);
  optimize_model(chunk->models + 1);
  optimize_model(chunk->models + 2);

  chunk->has_data = true;
  chunk->is_being_loaded = true;
}

