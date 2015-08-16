#include "app.h"

float clamp(float value, float min, float max) {
  return std::max(min, std::min(max, value));
}

void normalize_plane(Plane *plane) {
  float mag = glm::sqrt(plane->normal.x * plane->normal.x + plane->normal.y * plane->normal.y + plane->normal.z * plane->normal.z);
  plane->normal = plane->normal / mag;
  plane->distance = plane->distance / mag;
}

inline Entity *get_entity_by_id(App *app, u32 id) {
  for (u32 i=0; i<app->entity_count; i++) {
    Entity *entity = app->entities + i;
    if (entity->id == id) {
      return entity;
    }
  }

  return NULL;
}

void load_texture(Texture *texture, int type=0) {
  if (!texture->initialized && !texture->has_data) {
    int channels;

    int width, height;
    u8* image = stbi_load(texture->path, &width, &height, &channels, type);

    texture->width = width;
    texture->height = height;

    texture->data = image;
    texture->has_data = true;
  }
}

float distance_from_plane(Plane plane, glm::vec3 position) {
  return glm::dot(plane.normal, position) + plane.distance;
}

const char *get_filename_ext(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if(!dot || dot == filename) return "";
  return dot + 1;
}

glm::vec3 read_vector(char *start) {
  glm::vec3 result;
  sscanf(start, "%f,%f,%f", &result.x, &result.y, &result.z);
  return result;
}

Model *get_model_by_name(App *app, char *name) {
  if (strcmp(name, "cube") == 0) {
    return &app->cube_model;
  } else if (strcmp(name, "sphere") == 0) {
    return &app->sphere_model;
  } else if (strcmp(name, "rock") == 0) {
    return &app->rock_model;
  } else if (strcmp(name, "quad") == 0) {
    return &app->quad_model;
  } else if (strcmp(name, "tree_001") == 0) {
    return app->trees + 0;
  } else if (strcmp(name, "tree_002") == 0) {
    return app->trees + 1;
  }

  return NULL;
}

float get_terrain_height_at(float x, float y) {
  return (
    scaled_octave_noise_2d(20.0f, 0.3f, 10.0f, 0.0f, 40.0f, y / 4000.0f, x / 4000.0f) +
    scaled_octave_noise_2d(20.0f, 0.3f, 10.0f, 0.0f, 40.0f, x / 4000.0f, y / 4000.0f) +
    scaled_octave_noise_2d(1.0f, 1.0f, 10.0f, -100.0f, 500.0f, x / 40000.0f, y / 40000.0f) +
    scaled_octave_noise_2d(1.0f, 1.0f, 10.0f, 00.0f, 1000.0f, x / 400000.0f, y / 400000.0f) +

    0.0f
  );
}

void mount_entity_to_terrain(Entity *entity) {
  entity->position.y = get_terrain_height_at(entity->position.x, entity->position.z);
}

void save_binary_level_file(LoadedLevel loaded_level) {
  LoadedLevelHeader header;
  header.entity_count = loaded_level.entities.size();

  PlatformFile file = platform.open_file((char *)"level.level", "w");
  fwrite(&header, 1, sizeof(header), (FILE *)file.platform); // TODO(sedivy): remove fwrite
  for (auto it = loaded_level.entities.begin(); it != loaded_level.entities.end(); it++) {
    fwrite(&(*it), 1, sizeof(EntitySave), (FILE *)file.platform);
  }
  platform.close_file(file);
}

void save_text_level_file(LoadedLevel level) {
  const char *base_path = "../../../level";

  platform.create_directory((char *)base_path);

  for (auto it = level.entities.begin(); it != level.entities.end(); it++) {
    char full_path[256];
    sprintf(full_path, "%s/%d.entity", base_path, it->id);

    PlatformFile file = platform.open_file(full_path, "w");

    // TODO(sedivy): replace fprintf
    fprintf((FILE *)file.platform, "%d\n", it->id);
    fprintf((FILE *)file.platform, "p: %f, %f, %f\n", it->position.x, it->position.y, it->position.z);
    fprintf((FILE *)file.platform, "s: %f, %f, %f\n", it->scale.x, it->scale.y, it->scale.z);
    fprintf((FILE *)file.platform, "r: %f, %f, %f\n", it->rotation.x, it->rotation.y, it->rotation.z);
    fprintf((FILE *)file.platform, "c: %f, %f, %f\n", it->color.x, it->color.y, it->color.z);
    fprintf((FILE *)file.platform, "f: %d\n", it->flags);
    fprintf((FILE *)file.platform, "m: %s\n", it->model_name);

    platform.close_file(file);
  }
}

LoadedLevel load_binary_level_file() {
  LoadedLevel result;

  DebugReadFileResult file = platform.debug_read_entire_file("level.level");

  LoadedLevelHeader *header = static_cast<LoadedLevelHeader *>((void *)file.contents);

  for (u32 i=0; i<header->entity_count; i++) {
    EntitySave *entity = (EntitySave *)((char *)header + sizeof(LoadedLevelHeader) + sizeof(EntitySave) * i);

    result.entities.push_back(*entity);
  }

  platform.debug_free_file(file);

  return result;
}

void deserialize_entity(App *app, EntitySave *src, Entity *dest) {
  dest->id = src->id;
  dest->position = src->position;
  dest->scale = src->scale;
  dest->rotation = src->rotation;
  dest->color = src->color;
  dest->flags = src->flags;
  dest->model = get_model_by_name(app, src->model_name);

  if (dest->flags & EntityFlags::MOUNT_TO_TERRAIN) {
    mount_entity_to_terrain(dest);
  }
}

void load_debug_level(App *app) {
  const char *base_path = "../../../level";

  PlatformDirectory directory = platform.open_directory(base_path);
  if (directory.platform != NULL) {
    LoadedLevel loaded_level;

    while (true) {
      PlatformDirectoryEntry entry = platform.read_next_directory_entry(directory);
      if (entry.empty) { break; }

      if (platform.is_directory_entry_file(entry)) {
        char *full_path = (char *)alloca(sizeof(base_path) + sizeof(entry.name));
        sprintf(full_path, "%s/%s", base_path, entry.name);

        if (strcmp(get_filename_ext(full_path), "entity") == 0) {
          PlatformFile file = platform.open_file(full_path, "r");
          if (!file.error) {

            EntitySave entity = {};

            PlatformFileLine line = platform.read_file_line(file);
            if (!line.empty) {
              sscanf(line.contents, "%d", &entity.id);
            }

            while (true) {
              PlatformFileLine line = platform.read_file_line(file);
              if (line.empty) { break; }

              char *start = (char *)((u8*)line.contents + 2);

              switch (line.contents[0]) {
                case 'p': {
                  entity.position = read_vector(start);
                } break;
                case 'm': {
                  sscanf(start, "%s", entity.model_name);
                } break;
                case 'c': {
                  entity.color = glm::vec4(read_vector(start), 1.0f);
                } break;
                case 's': {
                  entity.scale = read_vector(start);
                } break;
                case 'r': {
                  entity.rotation = read_vector(start);
                } break;
                case 'f': {
                  sscanf(start, "%d", &entity.flags);
                } break;
              }
            }

            loaded_level.entities.push_back(entity);

            platform.close_file(file);
          }
        }
      }
    }

    platform.close_directory(directory);

    save_binary_level_file(loaded_level);
  }

  LoadedLevel bin_loaded_level = load_binary_level_file();

  for (auto it = bin_loaded_level.entities.begin(); it != bin_loaded_level.entities.end(); it++) {
    Entity *entity = app->entities + app->entity_count++;

    deserialize_entity(app, &(*it), entity);

    if (entity->id > app->last_id) {
      app->last_id = entity->id;
    }
  }
}

RayMatchResult ray_match_sphere(Ray ray, glm::vec3 position, float radius) {
  RayMatchResult result;
  result.hit = false;

  glm::vec3 offset = ray.start - position;
  float a = glm::dot(ray.direction, ray.direction);
  float b = 2 * glm::dot(ray.direction, offset);

  float c = glm::dot(offset, offset) - radius * radius;
  float discriminant = b * b - 4 * a * c;

  if (discriminant > 0.0f) {
    float t = (-b - sqrt(discriminant)) / (2 * a);
    if (t >= 0.0f) {

      result.hit = true;
      result.distance = t;
      result.hit_position = ray.start + (ray.direction * t);
    }
  }

  return result;
}

#if 0
bool ray_match_entity(Ray ray, Entity *entity) {
  /* if (!entity->model) { */
  /*   return false; */
  /* } */

  /* if (!ray_match_sphere(ray, entity->position, entity->model->radius * glm::compMax(entity->scale))) { */
  /*   return false; */
  /* } */

  glm::mat4 model_view;
  model_view = glm::translate(model_view, entity->position);
  /* model_view = glm::rotate(model_view, entity->rotation.x, glm::vec3(1.0, 0.0, 0.0)); */
  /* model_view = glm::rotate(model_view, entity->rotation.y, glm::vec3(0.0, 1.0, 0.0)); */
  /* model_view = glm::rotate(model_view, entity->rotation.z, glm::vec3(0.0, 0.0, 1.0)); */
  model_view = glm::scale(model_view, entity->scale);


  glm::mat4 res = glm::inverse(model_view);

  glm::vec3 start = glm::vec3(glm::vec4(ray.start, 0.0f) * res);
  glm::vec3 direction = glm::normalize(glm::vec3(res * glm::vec4(ray.start + ray.direction, 0.0f)) - start);

  ModelData mesh = entity->model->mesh.data;

  for (int i=0; i<mesh.indices_count; i += 3) {
    int indices_a = mesh.indices[i + 0] * 3;
    int indices_b = mesh.indices[i + 1] * 3;
    int indices_c = mesh.indices[i + 2] * 3;

    glm::vec3 a = glm::vec3(mesh.vertices[indices_a + 0],
                             mesh.vertices[indices_a + 1],
                             mesh.vertices[indices_a + 2]);

    glm::vec3 b = glm::vec3(mesh.vertices[indices_b + 0],
                             mesh.vertices[indices_b + 1],
                             mesh.vertices[indices_b + 2]);

    glm::vec3 c = glm::vec3(mesh.vertices[indices_c + 0],
                             mesh.vertices[indices_c + 1],
                             mesh.vertices[indices_c + 2]);

    glm::vec3 result_position;
    if (glm::intersectRayTriangle(start, direction, a, b, c, result_position)) {
      return true;
    }
  }


  return false;
}
#endif

bool is_sphere_in_frustum(Frustum *frustum, glm::vec3 position, float radius) {
  for (int i=0; i<6; i++) {
    float distance = distance_from_plane(frustum->planes[i], position);
    if (distance + radius < 0) {
      return false;
    }
  }

  return true;
}

void fill_frustum_with_matrix(Frustum *frustum, glm::mat4 matrix) {
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

void unload_texture(Texture *texture) {
  if (texture->initialized) {
    glDeleteTextures(1, &texture->id);
  }

  if (texture->has_data) {
    stbi_image_free(texture->data);
    texture->initialized = false;
    texture->has_data = false;
    texture->is_being_loaded = false;
  }
};

void unload_model(Model *model) {
  if (model->initialized) {
    glDeleteBuffers(1, &model->mesh.vertices_id);
    glDeleteBuffers(1, &model->mesh.indices_id);
    glDeleteBuffers(1, &model->mesh.normals_id);
    glDeleteBuffers(1, &model->mesh.uv_id);
  }

  free(model->mesh.data.data);

  model->initialized = false;
  model->has_data = false;
  model->is_being_loaded = false;
}

float calculate_radius(Mesh *mesh) {
  float max_radius = 0.0f;

  for (u32 i=0; i<mesh->data.vertices_count; i += 3) {
    float radius = glm::length(glm::vec3(mesh->data.vertices[i], mesh->data.vertices[i + 1], mesh->data.vertices[i + 2]));
    if (radius > max_radius) {
      max_radius = radius;
    }
  }

  return max_radius;
}

char *allocate_string(const char *string) {
  char *new_location = static_cast<char *>(malloc(strlen(string)));

  strcpy(new_location, string);

  return new_location;
}

void initialize_texture(Texture *texture, GLenum type=GL_RGB, bool mipmap=true, GLenum wrap_type=GL_REPEAT) {
  if (!texture->initialized) {
    glGenTextures(1, &texture->id);

    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexImage2D(GL_TEXTURE_2D, 0, type, texture->width, texture->height, 0, type, GL_UNSIGNED_BYTE, texture->data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_type);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_type);

    float aniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);

    if (mipmap) {
      glGenerateMipmap(GL_TEXTURE_2D);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    texture->initialized = true;
  }
}

void initialize_model(Model *model) {
  GLuint vertices_id;
  glGenBuffers(1, &vertices_id);
  glBindBuffer(GL_ARRAY_BUFFER, vertices_id);
  glBufferData(GL_ARRAY_BUFFER, model->mesh.data.vertices_count * sizeof(GLfloat), model->mesh.data.vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  GLuint normals_id;
  glGenBuffers(1, &normals_id);
  glBindBuffer(GL_ARRAY_BUFFER, normals_id);
  glBufferData(GL_ARRAY_BUFFER, model->mesh.data.normals_count * sizeof(GLfloat), model->mesh.data.normals, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  GLuint uv_id;
  glGenBuffers(1, &uv_id);
  glBindBuffer(GL_ARRAY_BUFFER, uv_id);
  glBufferData(GL_ARRAY_BUFFER, model->mesh.data.uv_count * sizeof(GLfloat), model->mesh.data.uv, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  GLuint indices_id;
  glGenBuffers(1, &indices_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->mesh.data.indices_count * sizeof(GLint), model->mesh.data.indices, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  model->mesh.indices_id = indices_id;
  model->mesh.vertices_id = vertices_id;
  model->mesh.normals_id = normals_id;
  model->mesh.uv_id = uv_id;

  model->initialized = true;
}

inline bool shader_has_attribute(Shader *shader, const char *name) {
  return shader->attributes.count(name);
}

inline GLuint shader_get_attribute_location(Shader *shader, const char *name) {
  assert(shader_has_attribute(shader, name));
  return shader->attributes[name];
}

void use_program(App* app, Shader *program) {
  if (program == app->current_program) { return; }
  if (app->current_program != NULL) {
    for (auto it = app->current_program->attributes.begin(); it != app->current_program->attributes.end(); it++) {
      glDisableVertexAttribArray(it->second);
    }
  }

  app->current_program = program;
  glUseProgram(program->id);

  for (auto it = program->attributes.begin(); it != program->attributes.end(); it++) {
    glEnableVertexAttribArray(it->second);
  }
}

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

void allocate_mesh(Mesh *mesh, u32 vertices_count, u32 normals_count, u32 indices_count, u32 uv_count) {
  u32 vertices_size = vertices_count * sizeof(float);
  u32 normals_size = normals_count * sizeof(float);
  u32 indices_size = indices_count * sizeof(int);
  u32 uv_size = uv_count * sizeof(float);

  u8 *data = static_cast<u8*>(malloc(vertices_size + normals_size + indices_size + uv_size));

  float *vertices = (float*)data;
  float *normals = (float*)(data + vertices_size);
  int *indices = (int*)(data + vertices_size + normals_size);
  float *uv = (float*)(data + vertices_size + normals_size + indices_size);

  mesh->data.data = data;
  mesh->data.vertices = vertices;
  mesh->data.vertices_count = vertices_count;

  mesh->data.normals = normals;
  mesh->data.normals_count = normals_count;

  mesh->data.indices = indices;
  mesh->data.indices_count = indices_count;

  mesh->data.uv = uv;
  mesh->data.uv_count = uv_count;
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
  model.path = allocate_string("chunk");
  model.mesh = mesh;
  model.initialized = false;
  model.has_data = true;
  model.is_being_loaded = false;
  model.radius = radius;

  return model;
}

void delete_shader(Shader *shader) {
  glDeleteProgram(shader->id);
  shader->initialized = false;
  shader->uniforms.clear();
  shader->attributes.clear();
}

Shader *create_shader(Shader *shader, const char *vert_filename, const char *frag_filename) {
  shader->uniforms = std::unordered_map<std::string, u32>();
  shader->attributes = std::unordered_map<std::string, GLuint>();

  if (shader->initialized) {
    delete_shader(shader);
  }

  GLuint vertexShader;
  GLuint fragmentShader;

  {
    DebugReadFileResult vertex = platform.debug_read_entire_file(vert_filename);
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex.contents, (const GLint*)&vertex.fileSize);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

    if (!success) {
      glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
      printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n %s\n", infoLog);
    }

    platform.debug_free_file(vertex);
  }

  {
    DebugReadFileResult fragment = platform.debug_read_entire_file(frag_filename);

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragment.contents, (const GLint*)&fragment.fileSize);
    glCompileShader(fragmentShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

    if (!success) {
      glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
      printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n %s\n", infoLog);
    }
    platform.debug_free_file(fragment);
  }

  GLuint shaderProgram;
  shaderProgram = glCreateProgram();

  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  shader->id = shaderProgram;
  shader->initialized = true;

  glUseProgram(shaderProgram);

  int length, size, count;
  char name[256];
  GLenum type;
  glGetProgramiv(shaderProgram, GL_ACTIVE_UNIFORMS, &count);
  for (int i=0; i<count; i++) {
    glGetActiveUniform(shaderProgram, static_cast<GLuint>(i), sizeof(name), &length, &size, &type, name);
    shader->uniforms[name] = glGetUniformLocation(shaderProgram, name);
  }

  glGetProgramiv(shaderProgram, GL_ACTIVE_ATTRIBUTES, &count);
  for (int i=0; i<count; i++) {
    glGetActiveAttrib(shaderProgram, GLuint(i), sizeof(name), &length, &size, &type, name);
    shader->attributes[name] = glGetAttribLocation(shaderProgram, name);
  }

  glUseProgram(0);

  return shader;
}

u32 next_entity_id(App *app) {
  return ++app->last_id;
}

static float tau = glm::pi<float>() * 2.0f;
static float pi = glm::pi<float>();

u32 generate_blue_noise(glm::vec2 *positions, u32 position_count, float min_radius, float radius) {
  u32 noise_count = 0;

  positions[0] = glm::vec2(20000.0, 20000.0);
  noise_count += 1;

  bool full = false;
  for (u32 i=0; i<position_count; i++) {
    if (full) {
      break;
    }

    glm::vec2 record = positions[i];

    for (u32 k=0; k<10; k++) {
      float angle = get_random_float_between(0.0f, tau);
      float random_distance = get_random_float_between(min_radius + radius, min_radius + radius);

      glm::vec2 new_position = record + glm::vec2(glm::cos(angle) * random_distance, glm::sin(angle) * random_distance);

      if (new_position.x < 0.0f || new_position.y < 0.0f || new_position.x > 40000.0f || new_position.y > 40000.0f) {
        continue;
      }

      bool hit = false;

      for (u32 l=0; l<noise_count; l++) {
        glm::vec2 check_record = positions[l];

        float distance = glm::distance(new_position, check_record);
        if (distance < min_radius) {
          hit = true;
          break;
        }
      }

      if (!hit) {
        if (noise_count >= position_count) {
          full = true;
          break;
        }
        positions[noise_count] = new_position;
        noise_count += 1;
      }
    }
  }

  return noise_count;
}

void generate_trees(App *app) {
  glm::vec2 noise_records[200];
  u32 size = generate_blue_noise(noise_records, array_count(noise_records), 500.0f, 400.0f);

  Random random = create_random_sequence();

  for (u32 i=0; i<size; i++) {
    Entity *entity = app->entities + app->entity_count++;

    entity->id = next_entity_id(app);
    entity->type = EntityBlock;
    entity->position.x = noise_records[i].x;
    entity->position.z = noise_records[i].y;
    mount_entity_to_terrain(entity);
    entity->scale = glm::vec3(1.0f);
    entity->model = app->trees + (i % array_count(app->trees));
    entity->color = glm::vec4(0.7f, 0.3f, 0.2f, 1.0f);
    entity->rotation = glm::vec3(0.0f, get_next_float(&random) * glm::pi<float>(), 0.0f);
    entity->flags = EntityFlags::PERMANENT_FLAG | EntityFlags::MOUNT_TO_TERRAIN;
  }
}

void save_level(App *app) {
  LoadedLevel level;

  for (u32 i=0; i<app->entity_count; i++) {
    Entity *entity = app->entities + i;

    if (entity->flags & EntityFlags::PERMANENT_FLAG) {
      EntitySave save_entity = {};
      save_entity.id = entity->id;
      save_entity.position = entity->position;
      save_entity.scale = entity->scale;
      save_entity.rotation = entity->rotation;
      save_entity.color = entity->color;
      save_entity.flags = entity->flags;
      strcpy(save_entity.model_name, entity->model->id_name);
      level.entities.push_back(save_entity);
    }
  }

  save_binary_level_file(level);
  save_text_level_file(level);
}

void quit(Memory *memory) {
}

void load_color_grading_texture(Texture *texture) {
  load_texture(texture, STBI_rgb_alpha);
  glGenTextures(1, &texture->id);

  glBindTexture(GL_TEXTURE_2D, texture->id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->data);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  texture->initialized = true;
  texture->data = NULL;
}

void init(Memory *memory) {
  glewExperimental = GL_TRUE;
  glewInit();

  App *app = static_cast<App*>(memory->permanent_storage);

  app->antialiasing = true;
  app->color_correction = true;
  app->bloom = false;

  app->current_program = NULL;

  app->editor.handle_size = 40.0f;
  app->editor.holding_entity = false;
  app->editor.inspect_entity = false;
  app->editor.show_left = true;
  app->editor.show_right = true;
  app->editor.left_state = EditorLeftState::MODELING;

  debug_global_memory = memory;

  platform = memory->platform;

  app->entity_count = 0;
  app->last_id = 0;

  app->camera.ortho = false;
  app->camera.near = 0.5f;
  app->camera.far = 50000.0f;
  app->camera.size = glm::vec2((float)memory->width, (float)memory->height);

  app->shadow_camera.ortho = true;
  app->shadow_camera.near = 100.0f;
  app->shadow_camera.far = 6000.0f;
  app->shadow_camera.size = glm::vec2(4096.0f, 4096.0f) / 2.0f;

  glGenVertexArrays(1, &app->vao);
  glBindVertexArray(app->vao);

  /* glEnable(GL_MULTISAMPLE); */
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  glCullFace(GL_BACK);

  {
    {
      float latitude_bands = 30;
      float longitude_bands = 30;
      float radius = 1.0;

      u32 vertices_count = (latitude_bands + 1) * (longitude_bands + 1) * 3;
      u32 normals_count = vertices_count;
      /* u32 uv_count = 0; */
      u32 indices_count = latitude_bands * longitude_bands * 6;

      Mesh mesh;
      allocate_mesh(&mesh, vertices_count, normals_count, indices_count, 0);

      u32 vertices_index = 0;
      u32 normals_index = 0;
      /* u32 uv_index = 0; */
      u32 indices_index = 0;

      for (float lat_number = 0; lat_number <= latitude_bands; lat_number++) {
        float theta = lat_number * M_PI / latitude_bands;
        float sinTheta = sin(theta);
        float cos_theta = cos(theta);

        for (float long_number = 0; long_number <= longitude_bands; long_number++) {
          float phi = long_number * 2 * M_PI / longitude_bands;
          float sin_phi = sin(phi);
          float cos_phi = cos(phi);

          float x = cos_phi * sinTheta;
          float y = cos_theta;
          float z = sin_phi * sinTheta;

          /* uv.push_back(1 - (long_number / longitude_bands)); */
          /* uv.push_back(1 - (lat_number / latitude_bands)); */

          mesh.data.vertices[vertices_index++] = radius * x;
          mesh.data.vertices[vertices_index++] = radius * y;
          mesh.data.vertices[vertices_index++] = radius * z;

          mesh.data.normals[normals_index++] = x;
          mesh.data.normals[normals_index++] = y;
          mesh.data.normals[normals_index++] = z;
        }
      }

      for (int lat_number = 0; lat_number < latitude_bands; lat_number++) {
        for (int long_number = 0; long_number < longitude_bands; long_number++) {
          int first = (lat_number * (longitude_bands + 1)) + long_number;
          int second = first + longitude_bands + 1;

          mesh.data.indices[indices_index++] = first + 1;
          mesh.data.indices[indices_index++] = second;
          mesh.data.indices[indices_index++] = first;

          mesh.data.indices[indices_index++] = first + 1;
          mesh.data.indices[indices_index++] = second + 1;
          mesh.data.indices[indices_index++] = second;
        }
      }

      app->sphere_model.mesh = mesh;
      app->sphere_model.path = allocate_string("sphere");
      app->sphere_model.id_name = allocate_string("sphere");
      app->sphere_model.radius = radius;

      app->sphere_model.has_data = true;
      app->sphere_model.initialized = false;
      app->sphere_model.is_being_loaded = false;
    }

    {
      GLfloat vertices[] = {
        -1.0, -1.0,  1.0,
        1.0, -1.0,  1.0,
        1.0,  1.0,  1.0,
        -1.0,  1.0,  1.0,
        -1.0, -1.0, -1.0,
        -1.0,  1.0, -1.0,
        1.0,  1.0, -1.0,
        1.0, -1.0, -1.0,
        -1.0,  1.0, -1.0,
        -1.0,  1.0,  1.0,
        1.0,  1.0,  1.0,
        1.0,  1.0, -1.0,
        -1.0, -1.0, -1.0,
        1.0, -1.0, -1.0,
        1.0, -1.0,  1.0,
        -1.0, -1.0,  1.0,
        1.0, -1.0, -1.0,
        1.0,  1.0, -1.0,
        1.0,  1.0,  1.0,
        1.0, -1.0,  1.0,
        -1.0, -1.0, -1.0,
        -1.0, -1.0,  1.0,
        -1.0,  1.0,  1.0,
        -1.0,  1.0, -1.0
      };

      GLfloat normals[] = {
        0.0,  0.0,  1.0,
        0.0,  0.0,  1.0,
        0.0,  0.0,  1.0,
        0.0,  0.0,  1.0,
        0.0,  0.0, -1.0,
        0.0,  0.0, -1.0,
        0.0,  0.0, -1.0,
        0.0,  0.0, -1.0,
        0.0,  1.0,  0.0,
        0.0,  1.0,  0.0,
        0.0,  1.0,  0.0,
        0.0,  1.0,  0.0,
        0.0, -1.0,  0.0,
        0.0, -1.0,  0.0,
        0.0, -1.0,  0.0,
        0.0, -1.0,  0.0,
        1.0,  0.0,  0.0,
        1.0,  0.0,  0.0,
        1.0,  0.0,  0.0,
        1.0,  0.0,  0.0,
        -1.0,  0.0,  0.0,
        -1.0,  0.0,  0.0,
        -1.0,  0.0,  0.0,
        -1.0,  0.0,  0.0
      };

      GLint indices[] = {
        0, 1, 2,      0, 2, 3,
        4, 5, 6,      4, 6, 7,
        8, 9, 10,     8, 10, 11,
        12, 13, 14,   12, 14, 15,
        16, 17, 18,   16, 18, 19,
        20, 21, 22,   20, 22, 23
      };

      Mesh mesh;
      allocate_mesh(&mesh, array_count(vertices), array_count(normals), array_count(indices), 0);
      memcpy(mesh.data.vertices, &vertices, sizeof(vertices));
      memcpy(mesh.data.normals, &normals, sizeof(normals));
      memcpy(mesh.data.indices, &indices, sizeof(indices));

      app->cube_model.path = allocate_string("cube");
      app->cube_model.id_name = allocate_string("cube");
      app->cube_model.radius = calculate_radius(&mesh);
      app->cube_model.mesh = mesh;
      app->cube_model.has_data = true;
      app->cube_model.is_being_loaded = false;
      app->cube_model.initialized = false;
      initialize_model(&app->cube_model);
    }

    {
      GLfloat vertices[] = {
        -1.0, -1.0, 0.0, 1.0, -1.0, 0.0, 1.0, 1.0, 0.0, -1.0, 1.0, 0.0
      };

      GLfloat normals[] = {
        -1.0, -1.0, 0.0, 1.0, -1.0, 0.0, 1.0, 1.0, 0.0, -1.0, 1.0, 0.0
      };

      GLint indices[] = {
        0, 1, 2, 0, 2, 3
      };

      GLfloat uvs[] = {
        0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0
      };

      Mesh mesh;
      allocate_mesh(&mesh, array_count(vertices), array_count(normals), array_count(indices), array_count(uvs));
      memcpy(mesh.data.vertices, &vertices, sizeof(vertices));
      memcpy(mesh.data.normals, &normals, sizeof(normals));
      memcpy(mesh.data.indices, &indices, sizeof(indices));
      memcpy(mesh.data.uv, &uvs, sizeof(uvs));

      app->quad_model.path = allocate_string("quad");
      app->quad_model.id_name = allocate_string("quad");
      app->quad_model.radius = calculate_radius(&mesh);
      app->quad_model.mesh = mesh;
      app->quad_model.has_data = true;
      app->quad_model.is_being_loaded = false;
      app->quad_model.initialized = false;
      initialize_model(&app->quad_model);
    }

    app->rock_model.path = allocate_string("rock.obj");
    app->rock_model.id_name = allocate_string("rock");
    app->trees[0].path = allocate_string("trees/tree_01.obj");
    app->trees[0].id_name = allocate_string("tree_001");
    app->trees[1].path = allocate_string("trees/tree_02.obj");
    app->trees[1].id_name = allocate_string("tree_002");
  }

  create_shader(&app->solid_program, "solid_vert.glsl", "solid_frag.glsl");
  create_shader(&app->another_program, "another_vert.glsl", "another_frag.glsl");
  create_shader(&app->debug_program, "debug_vert.glsl", "debug_frag.glsl");
  create_shader(&app->ui_program, "ui_vert.glsl", "ui_frag.glsl");
  create_shader(&app->fullscreen_fog_program, "fullscreen.vert", "fullscreen_fog.frag");
  create_shader(&app->fullscreen_color_program, "fullscreen.vert", "fullscreen_color.frag");
  create_shader(&app->fullscreen_fxaa_program, "fullscreen.vert", "fullscreen_fxaa.frag");
  create_shader(&app->fullscreen_bloom_program, "fullscreen.vert", "fullscreen_bloom.frag");
  create_shader(&app->fullscreen_SSAO_program, "fullscreen.vert", "fullscreen_ssao.frag");
  create_shader(&app->fullscreen_program, "fullscreen.vert", "fullscreen.frag");
  create_shader(&app->fullscreen_depth_program, "fullscreen.vert", "fullscreen_depth.frag");
  create_shader(&app->terrain_program, "terrain.vert", "terrain.frag");
  create_shader(&app->skybox_program, "skybox.vert", "skybox.frag");
  create_shader(&app->program, "vert.glsl", "frag.glsl");
  create_shader(&app->textured_program, "textured.vert", "textured.frag");

  app->chunk_cache_count = 4096;
  app->chunk_cache = static_cast<TerrainChunk *>(malloc(sizeof(TerrainChunk) * app->chunk_cache_count));

  for (u32 i=0; i<app->chunk_cache_count; i++) {
    TerrainChunk *chunk = app->chunk_cache + i;
    chunk->models[0].initialized = false;
    chunk->has_data = false;
    chunk->is_being_loaded = false;
    chunk->initialized = false;
  }

  app->color_correction_texture.path = allocate_string("color_correction.png");
  app->gradient_texture.path = allocate_string("gradient.png");
  app->circle_texture.path = allocate_string("circle.png");
  app->editor_texture.path = allocate_string("editor_images.png");

  {
    load_texture(&app->gradient_texture, STBI_rgb_alpha);
    initialize_texture(&app->gradient_texture, GL_RGBA, false);
    stbi_image_free(app->gradient_texture.data);
    app->gradient_texture.data = NULL;
  }

  {
    load_texture(&app->editor_texture, STBI_rgb_alpha);
    initialize_texture(&app->editor_texture, GL_RGBA, false);
    stbi_image_free(app->editor_texture.data);
    app->editor_texture.data = NULL;
  }

  load_color_grading_texture(&app->color_correction_texture);

  {
    load_texture(&app->circle_texture, STBI_rgb_alpha);
    initialize_texture(&app->circle_texture, GL_RGBA, true);
    stbi_image_free(app->circle_texture.data);
    app->circle_texture.data = NULL;
  }

  DebugReadFileResult font_file = platform.debug_read_entire_file("font.ttf");
  app->font = create_font(font_file.contents, 16.0f);
  platform.debug_free_file(font_file);

  const char* faces[] = {
    "right.png", "left.png",
    "top.png", "bottom.png",
    "back.png", "front.png"
  };

  GLenum types[] = {
    GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
  };

  glGenTextures(1, &app->cubemap.id);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, app->cubemap.id);

  for (u32 i=0; i<array_count(faces); i++) {
    int width, height, channels;
    u8 *image = stbi_load(faces[i], &width, &height, &channels, STBI_rgb_alpha);
    glTexImage2D(types[i], 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    stbi_image_free(image);
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

  {
    float vertices[] = {
      -1.0f, -1.0f,
      1.0f, -1.0f,
      -1.0f, 1.0f,

      1.0f, -1.0f,
      1.1f, 1.0f,
      -1.0f, 1.0f
    };

    glGenBuffers(1, &app->fullscreen_quad);
    glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // NOTE(sedivy): frame buffer
  for (u32 i=0; i<array_count(app->frames); i++) {
    FrameBuffer *frame = app->frames + i;

    frame->width = memory->width;
    frame->height = memory->height;

    // NOTE(sedivy): texture
    {
      glGenTextures(1, &frame->texture);
      glBindTexture(GL_TEXTURE_2D, frame->texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame->width, frame->height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // NOTE(sedivy): depth
    {
      glGenTextures(1, &frame->depth);
      glBindTexture(GL_TEXTURE_2D, frame->depth);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, frame->width, frame->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // NOTE(sedivy): Framebuffer
    {
      glGenFramebuffers(1, &frame->id);
      glBindFramebuffer(GL_FRAMEBUFFER, frame->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame->texture, 0);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frame->depth, 0);
    }
  }

  {
    app->shadow_width = 4096;
    app->shadow_height = 4096;

    // NOTE(sedivy): depth
    {
      glGenTextures(1, &app->shadow_depth_texture);
      glBindTexture(GL_TEXTURE_2D, app->shadow_depth_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, app->shadow_width, app->shadow_height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }

    // NOTE(sedivy): Framebuffer
    {
      glGenFramebuffers(1, &app->shadow_buffer);
      glBindFramebuffer(GL_FRAMEBUFFER, app->shadow_buffer);
      glDrawBuffer(GL_NONE);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, app->shadow_depth_texture, 0);
    }
  }

  glGenBuffers(1, &app->debug_buffer);

  load_debug_level(app);

  {
    Entity *entity = app->entities + app->entity_count;
    entity->id = next_entity_id(app);
    entity->type = EntityPlayer;
    entity->flags = EntityFlags::RENDER_HIDDEN;
    entity->position.x = 0.0f;
    entity->position.y = 0.0f;
    entity->position.z = 0.0f;
    entity->color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    entity->model = &app->sphere_model;

    app->entity_count += 1;

    app->camera_follow = entity->id;
  }

  Entity *follow_entity = get_entity_by_id(app, app->camera_follow);
  follow_entity->rotation.y = 2.5f;
  follow_entity->position.x = 20000.0f;
  follow_entity->position.z = 20000.0f;

  {
    /* generate_trees(app); */

#if 0
    Random random = create_random_sequence();

    glm::vec2 rock_noise[200];
    u32 size = generate_blue_noise(rock_noise, array_count(rock_noise), 100.0f, 5000.0f);

    for (u32 i=0; i<size; i++) {
      Entity *entity = app->entities + app->entity_count;

      entity->id = next_entity_id(app);
      entity->type = EntityBlock;
      entity->position.x = rock_noise[i].x;
      entity->position.z = rock_noise[i].y;
      mount_entity_to_terrain(entity);
      entity->flags = EntityFlags::PERMANENT_FLAG;
      entity->scale = 100.0f * glm::vec3(0.8f + get_next_float(&random) / 2.0f, 0.8f + get_next_float(&random) / 2.0f, 0.8f + get_next_float(&random) / 2.0f);
      entity->model = &app->rock_model;
      entity->color = glm::vec4(get_random_float_between(0.2f, 0.5f), 0.45f, 0.5f, 1.0f);
      entity->rotation = glm::vec3(get_next_float(&random) * 0.5f - 0.25f, get_next_float(&random) * glm::pi<float>(), get_next_float(&random) * 0.5f - 0.25f);
      app->entity_count += 1;
    }
#endif
  }

  app->framecount = 0;
  app->frametimelast = platform.get_time();
}

inline bool shader_has_uniform(Shader *shader, const char *name) {
  return shader->uniforms.count(name);
}

inline GLuint shader_get_uniform_location(Shader *shader, const char *name) {
  assert(shader_has_uniform(shader, name));
  return shader->uniforms[name];
}

inline void send_shader_uniform(Shader *shader, const char *name, glm::mat4 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniformMatrix4fv(location, 1, false, glm::value_ptr(value));
}

inline void send_shader_uniform(Shader *shader, const char *name, glm::mat3 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniformMatrix3fv(location, 1, false, glm::value_ptr(value));
}

inline void send_shader_uniformi(Shader *shader, const char *name, int value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform1i(location, value);
}

inline void send_shader_uniformf(Shader *shader, const char *name, float value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform1f(location, value);
}

inline void send_shader_uniform(Shader *shader, const char *name, glm::vec3 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform3fv(location, 1, glm::value_ptr(value));
}

inline void send_shader_uniform(Shader *shader, const char *name, glm::vec4 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform4fv(location, 1, glm::value_ptr(value));
}

inline void send_shader_uniform(Shader *shader, const char *name, glm::vec2 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform2fv(location, 1, glm::value_ptr(value));
}

struct LoadModelWork {
  Model *model;
};

void load_model_work(void *data) {
  LoadModelWork *work = static_cast<LoadModelWork *>(data);

  if (!work->model->initialized && !work->model->has_data) {

    DebugReadFileResult result = platform.debug_read_entire_file(work->model->path);

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFileFromMemory(result.contents, result.fileSize, aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeGraph |
        aiProcess_SortByPType);

    Model *model = work->model;

    float max_distance = 0.0f;

    if (scene->HasMeshes()) {
      u32 indices_offset = 0;

      u32 count = 0;
      u32 index_count = 0;

      for (u32 i=0; i<scene->mNumMeshes; i++) {
        aiMesh *mesh_data = scene->mMeshes[i];
        count += mesh_data->mNumVertices;

        for (u32 l=0; l<mesh_data->mNumFaces; l++) {
          aiFace face = mesh_data->mFaces[l];

          index_count += face.mNumIndices;
        }
      }

      u32 vertices_count = count * 3;
      u32 normals_count = vertices_count;
      u32 uv_count = count * 2;
      u32 indices_count = index_count;

      u32 vertices_index = 0;
      u32 normals_index = 0;
      u32 uv_index = 0;
      u32 indices_index = 0;

      Mesh mesh;
      allocate_mesh(&mesh, vertices_count, normals_count, indices_count, uv_count);

      for (u32 i=0; i<scene->mNumMeshes; i++) {
        aiMesh *mesh_data = scene->mMeshes[i];

        for (u32 l=0; l<mesh_data->mNumVertices; l++) {
          mesh.data.vertices[vertices_index++] = mesh_data->mVertices[l].x;
          mesh.data.vertices[vertices_index++] = mesh_data->mVertices[l].y;
          mesh.data.vertices[vertices_index++] = mesh_data->mVertices[l].z;

          float new_distance = glm::length(glm::vec3(mesh_data->mVertices[l].x, mesh_data->mVertices[l].y, mesh_data->mVertices[l].z));
          if (new_distance > max_distance) {
            max_distance = new_distance;
          }

          mesh.data.normals[normals_index++] = mesh_data->mNormals[l].x;
          mesh.data.normals[normals_index++] = mesh_data->mNormals[l].y;
          mesh.data.normals[normals_index++] = mesh_data->mNormals[l].z;

          if (mesh_data->mTextureCoords[0]) {
            mesh.data.uv[uv_index++] = mesh_data->mTextureCoords[0][l].x;
            mesh.data.uv[uv_index++] = mesh_data->mTextureCoords[0][l].y;
          }
        }

        for (u32 l=0; l<mesh_data->mNumFaces; l++) {
          aiFace face = mesh_data->mFaces[l];

          for (u32 j=0; j<face.mNumIndices; j++) {
            mesh.data.indices[indices_index++] = face.mIndices[j] + indices_offset;
          }
        }

        indices_offset += mesh_data->mNumVertices;
      }

      model->mesh = mesh;
    }

    platform.debug_free_file(result);

    model->has_data = true;
    model->radius = max_distance;
  }

  free(work);
};

void load_texture_work(void *data) {
  load_texture(static_cast<Texture*>(data));
};

void generate_ground_work(void *data) {
  TerrainChunk *chunk = static_cast<TerrainChunk*>(data);
  chunk->models[0] = generate_ground(chunk->x, chunk->y, 0.03f);
  chunk->models[1] = generate_ground(chunk->x, chunk->y, 0.01f);
  chunk->models[2] = generate_ground(chunk->x, chunk->y, 0.005f);
  chunk->has_data = true;
  chunk->is_being_loaded = true;
}

inline bool process_texture(Memory *memory, Entity *entity) {
  if (!entity->texture->initialized && !entity->texture->has_data && !entity->texture->is_being_loaded) {
    if (platform.queue_has_free_spot(memory->low_queue)) {
      platform.add_work(memory->low_queue, load_texture_work, entity->texture);

      entity->texture->is_being_loaded = true;
    }
    return true;
  }

  if (!entity->texture->initialized && entity->texture->has_data) {
    initialize_texture(entity->texture);
    return true;
  }

  return false;
}

inline bool process_model(Memory *memory, Model *model) {
  if (!model->initialized && !model->has_data && !model->is_being_loaded) {
    if (platform.queue_has_free_spot(memory->low_queue)) {
      LoadModelWork *work = static_cast<LoadModelWork *>(malloc(sizeof(LoadModelWork)));
      work->model = model;

      platform.add_work(memory->low_queue, load_model_work, work);

      model->is_being_loaded = true;
    }
    return true;
  }

  if (!model->initialized && model->has_data) {
    initialize_model(model);
    return true;
  }

  return false;
}

inline void use_model_mesh(App *app, Mesh *mesh) {
  if (shader_has_attribute(app->current_program, "position")) {
    GLuint id = shader_get_attribute_location(app->current_program, "position");
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertices_id);
    glVertexAttribPointer(id, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  if (shader_has_attribute(app->current_program, "normals")) {
    GLuint id = shader_get_attribute_location(app->current_program, "normals");
    glBindBuffer(GL_ARRAY_BUFFER, mesh->normals_id);
    glVertexAttribPointer(id, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  if (shader_has_attribute(app->current_program, "uv")) {
    GLuint id = shader_get_attribute_location(app->current_program, "uv");
    glBindBuffer(GL_ARRAY_BUFFER, mesh->uv_id);
    glVertexAttribPointer(id, 2, GL_FLOAT, GL_FALSE, 0, 0);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_id);
}

inline bool process_terrain(Memory *memory, TerrainChunk *chunk) {
  if (!chunk->has_data) {
    if (!chunk->is_being_loaded && platform.queue_has_free_spot(memory->main_queue)) {
      chunk->is_being_loaded = true;

      platform.add_work(memory->main_queue, generate_ground_work, chunk);
    }

    return true;
  }

  bool generated = false;

  for (u32 i=0; i<array_count(chunk->models); i++) {
    Model *model = chunk->models + i;
    if (!model->initialized) {
      initialize_model(model);
      generated = true;
    }
  }

  return generated;
}

bool render_terrain_chunk(App *app, TerrainChunk *chunk, Model *model) {
  glm::mat4 model_view;
  model_view = glm::translate(model_view, glm::vec3(chunk->x * CHUNK_SIZE_X, 0.0f, chunk->y * CHUNK_SIZE_Y));

  glm::mat3 normal = glm::inverseTranspose(glm::mat3(model_view));

  if (shader_has_uniform(app->current_program, "uNMatrix")) {
    send_shader_uniform(app->current_program, "uNMatrix", normal);
  }

  if (shader_has_uniform(app->current_program, "uMVMatrix")) {
    send_shader_uniform(app->current_program, "uMVMatrix", model_view);
  }

  if (shader_has_uniform(app->current_program, "in_color")) {
    send_shader_uniform(app->current_program, "in_color", glm::vec4(1.0f, 0.4f, 0.1f, 1.0f));
  }

  use_model_mesh(app, &model->mesh);
  glDrawElements(GL_TRIANGLES, model->mesh.data.indices_count, GL_UNSIGNED_INT, 0);

  return true;
}

void debug_render_rect(UICommandBuffer *command_buffer, float x, float y, float width, float height, glm::vec4 color, glm::vec4 image_color=glm::vec4(0.0f), u32 x_index=0, u32 y_index=0) {
  float scale = 1.0f / 4.0f;

  command_buffer->vertices.push_back(x);
  command_buffer->vertices.push_back(y);

  command_buffer->vertices.push_back(scale * x_index + 0.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 0.0f * scale);

  command_buffer->vertices.push_back(x + width);
  command_buffer->vertices.push_back(y);

  command_buffer->vertices.push_back(scale * x_index + 1.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 0.0f * scale);

  command_buffer->vertices.push_back(x);
  command_buffer->vertices.push_back(y + height);

  command_buffer->vertices.push_back(scale * x_index + 0.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 1.0f * scale);

  command_buffer->vertices.push_back(x + width);
  command_buffer->vertices.push_back(y);

  command_buffer->vertices.push_back(scale * x_index + 1.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 0.0f * scale);

  command_buffer->vertices.push_back(x + width);
  command_buffer->vertices.push_back(y + height);

  command_buffer->vertices.push_back(scale * x_index + 1.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 1.0f * scale);

  command_buffer->vertices.push_back(x);
  command_buffer->vertices.push_back(y + height);

  command_buffer->vertices.push_back(scale * x_index + 0.0f * scale);
  command_buffer->vertices.push_back(scale * y_index + 1.0f * scale);

  UICommand command;
  command.vertices_count = 6;
  command.color = color;
  command.image_color = image_color;
  command.type = UICommandType::RECT;

  command_buffer->commands.push_back(command);
}

void draw_string(UICommandBuffer *command_buffer, Font *font, float x, float y, char *text, glm::vec3 color=glm::vec3(1.0f, 1.0f, 1.0f)) {
  float font_x = 0, font_y = font->size;
  stbtt_aligned_quad q;

  UICommand command;
  command.type = UICommandType::TEXT;
  command.vertices_count = 0;
  command.color = glm::vec4(color, 1.0f);

  while (*text != '\0') {
    font_get_quad(font, *text++, &font_x, &font_y, &q);

    GLfloat vertices_data[] = {
      x + q.x0, y + q.y0, // top left
      q.s0, q.t0,

      x + q.x0, y + q.y1, // bottom left
      q.s0, q.t1,

      x + q.x1, y + q.y0, // top right
      q.s1, q.t0,

      x + q.x1, y + q.y0, // top right
      q.s1, q.t0,

      x + q.x0, y + q.y1, // bottom left
      q.s0, q.t1,

      x + q.x1, y + q.y1, // bottom right
      q.s1, q.t1
    };

    for (u32 i=0; i<array_count(vertices_data); i++) {
      command_buffer->vertices.push_back(vertices_data[i]);
    }
    command.vertices_count += 6;
  }

  command_buffer->commands.push_back(command);
}

bool push_debug_button(Input &input,
                       App *app,
                       DebugDrawState *state,
                       UICommandBuffer *command_buffer,
                       float x, float height,
                       char *text,
                       glm::vec3 color,
                       glm::vec4 background_color,
                       glm::vec4 hover_color=glm::vec4(0.5f, 0.6f, 1.0f, 1.0f),
                       glm::vec4 active_color=glm::vec4(0.2f, 0.3f, 0.7f, 1.0f)) {
  float min_x = x;
  float min_y = state->offset_top;;
  float max_x = min_x + 175.0f;
  float max_y = min_y + height;

  bool clicked = false;

  if (input.mouse_x > min_x && input.mouse_y > min_y && input.mouse_x < max_x && input.mouse_y < max_y) {
    if (input.mouse_click) {
      background_color = active_color;
      clicked = true;
      input.mouse_click = false;
    } else {
      background_color = hover_color;
    }
  }

  debug_render_rect(command_buffer, x, state->offset_top, 175.0f, height, background_color);
  draw_string(command_buffer, &app->font, x + 5, state->offset_top + (height - 23.0f) / 2.0f, text, color);
  state->offset_top += height;

  return clicked;
}

void push_debug_text(App *app, DebugDrawState *state, UICommandBuffer *command_buffer, float x, char *text, glm::vec3 color, glm::vec4 background_color) {
  debug_render_rect(command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color);
  draw_string(command_buffer, &app->font, x + 5, state->offset_top, text, color);
  state->offset_top += 25.0f;
}

glm::mat4 get_camera_projection(Camera *camera) {
  glm::mat4 projection;

  if (camera->ortho) {
    projection = glm::ortho(camera->size.x / -2.0f, camera->size.y / 2.0f, camera->size.x / 2.0f, camera->size.y / -2.0f, camera->near, camera->far);
  } else {
    projection = glm::perspective(glm::radians(75.0f), camera->size.x / camera->size.y, camera->near, camera->far);
  }

  return projection;
}

Ray get_mouse_ray(App *app, Input input, Memory *memory) {
  glm::vec3 from = glm::unProject(
      glm::vec3(input.mouse_x, memory->height - input.mouse_y, 0.0),
      glm::mat4(),
      app->camera.view_matrix,
      glm::vec4(0.0, 0.0, memory->width, memory->height));

  glm::vec3 to = glm::unProject(
      glm::vec3(input.mouse_x, memory->height - input.mouse_y, 1.0),
      glm::mat4(),
      app->camera.view_matrix,
      glm::vec4(0.0, 0.0, memory->width, memory->height));

  glm::vec3 direction = glm::normalize(to - from);

  Ray ray;
  ray.start = from;
  ray.direction = direction;

  return ray;
}

glm::mat4 make_billboard_matrix(glm::vec3 position, glm::vec3 camera_position, glm::vec3 camera_up) {
  glm::vec3 look = glm::normalize(camera_position - position);
  glm::vec3 right = glm::cross(camera_up, look);
  glm::vec3 up = glm::cross(look, right);
  glm::mat4 transform;
  transform[0] = glm::vec4(right, 0.0f);
  transform[1] = glm::vec4(up, 0.0f);
  transform[2] = glm::vec4(look, 0.0f);
  /* transform[3] = glm::vec4(position, 1.0f); */
  return transform;
}

#include "render_group.cpp"

void debug_render_range(Input &input, UICommandBuffer *command_buffer, float x, float y, float width, float height, glm::vec4 background_color, float *value, float min, float max) {
  debug_render_rect(command_buffer, x, y, width, height, background_color);

  float scale = *value / (max - min);

  debug_render_rect(command_buffer, x, y, width * scale, height, glm::vec4(0.5f, 1.0f, 0.2f, 1.0f));

  float min_x = x;
  float min_y = y;
  float max_x = min_x + width;
  float max_y = min_y + height;

  if (input.left_mouse_down && input.original_mouse_down_x > min_x && input.original_mouse_down_y > min_y && input.original_mouse_down_x < max_x && input.original_mouse_down_y < max_y) {
    *value = clamp((input.mouse_x - min_x) / width * (max - min), min, max);
    input.mouse_click = false;
  }
}

void push_debug_editable_vector(Input &input, DebugDrawState *state, Font *font, UICommandBuffer *command_buffer, float x, const char *name, glm::vec3 *vector, glm::vec4 background_color, float min, float max) {
  char text[256];

  debug_render_rect(command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color);

  sprintf(text, "%s", name);
  draw_string(command_buffer, font, x + 5.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->x);
  debug_render_range(input, command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color, &vector->x, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->y);
  debug_render_range(input, command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color, &vector->y, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->z);
  debug_render_range(input, command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color, &vector->z, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;
}

void push_debug_editable_vector(Input &input, DebugDrawState *state, Font *font, UICommandBuffer *command_buffer, float x, const char *name, glm::vec4 *vector, glm::vec4 background_color, float min, float max) {
  char text[256];

  debug_render_rect(command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color);

  sprintf(text, "%s", name);
  draw_string(command_buffer, font, x + 5.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->x);
  debug_render_range(input, command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color, &vector->x, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->y);
  debug_render_range(input, command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color, &vector->y, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector->z);
  debug_render_range(input, command_buffer, x, state->offset_top, 175.0f, 25.0f, background_color, &vector->z, min, max);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;
}

void push_debug_vector(DebugDrawState *state, Font *font, UICommandBuffer *command_buffer, float x, const char *name, glm::vec3 vector, glm::vec4 background_color) {
  char text[256];

  debug_render_rect(command_buffer, x, state->offset_top, 175.0f, 25.0f * 4.0f, background_color);

  sprintf(text, "%s", name);
  draw_string(command_buffer, font, x + 5.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector.x);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector.y);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;

  sprintf(text, "%f", vector.z);
  draw_string(command_buffer, font, x + 25.0f, state->offset_top, text, glm::vec3(1.0f, 1.0f, 1.0f));
  state->offset_top += 25.0f;
}

bool sort_by_distance(const EditorHandleRenderCommand &a, const EditorHandleRenderCommand &b) {
  return a.distance_from_camera > b.distance_from_camera;
}

void draw_3d_debug_info(App *app) {
  use_program(app, &app->textured_program);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthFunc(GL_ALWAYS);

  send_shader_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);

  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_2D, app->circle_texture.id);
  send_shader_uniformi(app->current_program, "textureImage", 0);

  use_model_mesh(app, &app->quad_model.mesh);

  std::vector<EditorHandleRenderCommand> render_commands;

  if (!app->editor.holding_entity) {
    for (u32 i=0; i<app->entity_count; i++) {
      Entity *entity = app->entities + i;

      if (entity->flags & EntityFlags::RENDER_HIDDEN) { continue; }

      if (!is_sphere_in_frustum(&app->camera.frustum, entity->position, app->editor.handle_size)) {
        continue;
      }

      glm::mat4 model_view;
      model_view = glm::translate(model_view, entity->position);
      model_view = glm::scale(model_view, glm::vec3(app->editor.handle_size));
      model_view *= make_billboard_matrix(entity->position, app->camera.position, glm::vec3(app->camera.view_matrix[0][1], app->camera.view_matrix[1][1], app->camera.view_matrix[2][1]));

      EditorHandleRenderCommand command;
      command.distance_from_camera = glm::distance(app->camera.position, entity->position);
      command.model_view = model_view;
      render_commands.push_back(command);
    }
  }

  std::sort(render_commands.begin(), render_commands.end(), sort_by_distance);

  for (auto it = render_commands.begin(); it != render_commands.end(); it++) {
    send_shader_uniform(app->current_program, "uMVMatrix", it->model_view);
    glDrawElements(GL_TRIANGLES, app->quad_model.mesh.data.indices_count, GL_UNSIGNED_INT, 0);
  }

  glDisable(GL_BLEND);
  glDepthFunc(GL_LESS);
}

void flush_2d_render(App *app, Memory *memory) {
  glm::mat4 projection = glm::ortho(0.0f, float(memory->width), float(memory->height), 0.0f);

  UICommandBuffer *command_buffer = &app->editor.command_buffer;

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  use_program(app, &app->program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, app->font.texture);

  glActiveTexture(GL_TEXTURE0 + 1);
  glBindTexture(GL_TEXTURE_2D, app->editor_texture.id);

  send_shader_uniform(app->current_program, "uPMatrix", projection);

  u32 vertices_index = 0;

  glBindBuffer(GL_ARRAY_BUFFER, app->debug_buffer);
  glBufferData(GL_ARRAY_BUFFER, command_buffer->vertices.size() * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, command_buffer->vertices.size() * sizeof(GLfloat), &command_buffer->vertices[0]);

  glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
  glVertexAttribPointer(shader_get_attribute_location(app->current_program, "uv"), 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *)(2 * sizeof(GLfloat)));

  for (auto it = command_buffer->commands.begin(); it != command_buffer->commands.end(); it++) {
    switch (it->type) {
      case UICommandType::NONE:
        break;
      case UICommandType::RECT:
        send_shader_uniformi(app->current_program, "textureImage", 1);
          send_shader_uniform(app->current_program, "background_color", it->color);
          send_shader_uniform(app->current_program, "image_color", it->image_color);
        break;
      case UICommandType::TEXT:
        send_shader_uniformi(app->current_program, "textureImage", 0);
        send_shader_uniform(app->current_program, "background_color", glm::vec4(0.0f));
          send_shader_uniform(app->current_program, "image_color", it->color);
        break;
    }

    glDrawArrays(GL_TRIANGLES, vertices_index, it->vertices_count);
    vertices_index += it->vertices_count;
  }

  glEnable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

void push_toggle_button(App *app, Input &input, DebugDrawState *draw_state, UICommandBuffer *command_buffer, float x, bool *value,
                        glm::vec4 background_color,
                        glm::vec4 hover_color=glm::vec4(0.5f, 0.6f, 1.0f, 1.0f),
                        glm::vec4 active_color=glm::vec4(0.2f, 0.3f, 0.7f, 1.0f)) {

  u32 toggl_button_x_index;
  u32 toggl_button_y_index;

  if (*value) {
    toggl_button_x_index = 0;
    toggl_button_y_index = 0;
  } else {
    toggl_button_x_index = 1;
    toggl_button_y_index = 0;
  }

  float image_height = 40.0f;
  if (push_debug_button(input, app, draw_state, command_buffer, x, image_height, (char *)"", glm::vec3(0.0f), background_color, hover_color, active_color)) {
    *value = !(*value);
  }

  debug_render_rect(command_buffer, x, draw_state->offset_top - image_height, image_height, image_height, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.4f, 0.0f, 0.0f, 1.0f), toggl_button_x_index, toggl_button_y_index);
}

void rebuild_chunks(App *app) {
  for (u32 i=0; i<app->chunk_cache_count; i++) {
    TerrainChunk *chunk = app->chunk_cache + i;
    unload_chunk(chunk);
  }
}

void draw_2d_debug_info(App *app, Memory *memory, Input &input) {
  UICommandBuffer *command_buffer = &app->editor.command_buffer;
  command_buffer->vertices.clear();
  command_buffer->commands.clear();

  DebugDrawState draw_state;
  draw_state.offset_top = 25.0f;

  glm::vec4 button_background_color = glm::vec4(0.5f, 0.6f, 0.9f, 0.9f);
  glm::vec4 default_background_color = glm::vec4(0.2f, 0.4f, 0.9f, 0.9f);

  char text[256];

  push_toggle_button(app, input, &draw_state, command_buffer, 10.0f, &app->editor.show_left, glm::vec4(1.0f, 0.16f, 0.15f, 0.9f), glm::vec4(0.8f, 0.1f, 0.1f, 0.9f), glm::vec4(0.7f, 0.1f, 0.1f, 0.9f));

  if (app->editor.show_left) {
    sprintf(text, "performance: %d\n", app->editor.show_performance);
    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
      app->editor.show_performance = !app->editor.show_performance;
    }

    if (app->editor.show_performance) {
      for (u32 i=0; i<array_count(memory->last_frame_counters); i++) {
        DebugCounter *counter = memory->last_frame_counters + i;

        if (counter->hit_count) {
          float time = (float)(counter->cycle_count * 1000) / (float)platform.get_performance_frequency();

          if (counter->hit_count > 1) {
            sprintf(text, "%d: %.2fms %llu %.2fms\n", i, time, counter->hit_count, time / (float)counter->hit_count);
          } else {
            sprintf(text, "%d: %.2fms", i, time);
          }

          push_debug_text(app, &draw_state, command_buffer, 10.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.1f, 0.6f, 0.9f));
        }
      }
    }

    sprintf(text, "state changes: %d\n", app->editor.show_state_changes);
    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
      app->editor.show_state_changes = !app->editor.show_state_changes;
    }

    if (app->editor.show_state_changes) {
      sprintf(text, "model_change: %d\n", app->render_group.model_change);
      push_debug_text(app, &draw_state, command_buffer, 10.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.1f, 0.6f, 0.9f));

      sprintf(text, "shader_change: %d\n", app->render_group.shader_change);
      push_debug_text(app, &draw_state, command_buffer, 10.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.1f, 0.6f, 0.9f));

      sprintf(text, "draw_calls: %d\n", app->render_group.draw_calls);
      push_debug_text(app, &draw_state, command_buffer, 10.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.1f, 0.6f, 0.9f));
    }

    sprintf(text, "fps: %d\n", (u32)app->fps);
    push_debug_text(app, &draw_state, command_buffer, 10.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), default_background_color);

    draw_state.offset_top += 10.0f;

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 40.0f, (char *)"save!", glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
      save_level(app);
    }

    draw_state.offset_top += 10.0f;

    glm::vec4 selected_button_background_color = glm::vec4(0.5f, 0.8f, 1.0f, 0.9f);

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Modeling", glm::vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::MODELING ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::MODELING;
    }

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Terrain", glm::vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::TERRAIN ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::TERRAIN;
    }

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Post Processing", glm::vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::POST_PROCESSING ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::POST_PROCESSING;
    }

    draw_state.offset_top += 10.0f;

    switch (app->editor.left_state) {
      {
        case EditorLeftState::MODELING:
          const char *types[] = {
            "tree_001",
            "tree_002",
            "rock",
            "cube",
            "sphere"
          };

          for (u32 i=0; i<array_count(types); i++) {
            if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)types[i], glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {

              Entity *entity = app->entities + app->entity_count++;

              glm::vec3 forward;
              forward.x = glm::sin(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);
              forward.y = -glm::sin(app->camera.rotation[0]);
              forward.z = -glm::cos(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);

              entity->id = next_entity_id(app);
              entity->type = EntityBlock;
              entity->position = app->camera.position + forward * 400.0f;
              entity->scale = glm::vec3(1.0f);
              entity->model = get_model_by_name(app, (char *)types[i]);
              entity->color = glm::vec4(get_random_float_between(0.2f, 0.5f), 0.45f, 0.5f, 1.0f);
              entity->flags = EntityFlags::CASTS_SHADOW | EntityFlags::PERMANENT_FLAG;

              if (
                  strcmp(types[i], "cube") == 0 ||
                  strcmp(types[i], "sphere") == 0 ||
                  strcmp(types[i], "rock") == 0) {
                entity->scale = glm::vec3(100.f);
              }
            }
          }
          break;
      }
      {
        case EditorLeftState::TERRAIN:
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"rebuild chunks", glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            rebuild_chunks(app);
          }
          break;
      }
      {
        case EditorLeftState::POST_PROCESSING:
          sprintf(text, "AA: %d\n", app->antialiasing);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->antialiasing = !app->antialiasing;
          }

          sprintf(text, "Color correction: %d\n", app->color_correction);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->color_correction = !app->color_correction;
          }

          sprintf(text, "Bloom (not working): %d\n", app->bloom);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->bloom = !app->bloom;
          }
          break;
      }
    }
  }

  if (app->editor.inspect_entity) {
    draw_state.offset_top = 25.0f;

    push_toggle_button(app, input, &draw_state, command_buffer, memory->width - 200.0f, &app->editor.show_right, glm::vec4(1.0f, 0.16f, 0.15f, 0.9f), glm::vec4(0.8f, 0.1f, 0.1f, 0.9f), glm::vec4(0.7f, 0.1f, 0.1f, 0.9f));

    if (app->editor.show_right) {
      Entity *entity = get_entity_by_id(app, app->editor.entity_id);
      if (entity) {

        sprintf(text, "id: %d\n", entity->id);
        push_debug_text(app, &draw_state, command_buffer, memory->width - 200, text, glm::vec3(1.0f, 1.0f, 1.0f), default_background_color);

        sprintf(text, "model: %s\n", entity->model->id_name);
        push_debug_text(app, &draw_state, command_buffer, memory->width - 200, text, glm::vec3(1.0f, 1.0f, 1.0f), default_background_color);

        push_debug_vector(&draw_state, &app->font, command_buffer, memory->width - 200, "position", entity->position, default_background_color);
        push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - 200, "rotation", &entity->rotation, default_background_color, 0.0f, pi * 2);
        push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - 200, "scale", &entity->scale, default_background_color, 0.0f, 200.0f);

        float start = draw_state.offset_top;
        push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - 200, "color", &entity->color, default_background_color, 0.0f, 1.0f);
        debug_render_rect(command_buffer, memory->width - 151.0f, start + 1.0f, 22.0f, 22.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
        debug_render_rect(command_buffer, memory->width - 150.0f, start + 2.0f, 20.0f, 20.0f, entity->color);

        sprintf(text, "ground mounted: %d\n", (entity->flags & EntityFlags::MOUNT_TO_TERRAIN) != 0);
        if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - 200.0f, 25.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
          entity->flags = entity->flags ^ EntityFlags::MOUNT_TO_TERRAIN;

          if (entity->flags & EntityFlags::MOUNT_TO_TERRAIN) {
            mount_entity_to_terrain(entity);
          }
        }

        sprintf(text, "casts shadow: %d\n", (entity->flags & EntityFlags::CASTS_SHADOW) != 0);
        if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - 200.0f, 25.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
          entity->flags = entity->flags ^ EntityFlags::CASTS_SHADOW;
        }

        sprintf(text, "save to file: %d\n", (entity->flags & EntityFlags::PERMANENT_FLAG) != 0);
        if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - 200.0f, 25.0f, text, glm::vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
          entity->flags = entity->flags ^ EntityFlags::PERMANENT_FLAG;
        }
      }
    }
  }
}

void tick(Memory *memory, Input input) {
  debug_global_memory = memory;
  platform = memory->platform;

  PROFILE(frame);

  App *app = static_cast<App*>(memory->permanent_storage);

  if (memory->should_reload) {
    memory->should_reload = false;

    glewExperimental = GL_TRUE;
    glewInit();
  }

  if (app->editing_mode) {
    PROFILE(render_ui);
    draw_2d_debug_info(app, memory, input);
    PROFILE_END(render_ui);
  }

  // NOTE(sedivy): update
  PROFILE(update);

  app->camera.size = glm::vec2((float)memory->width, (float)memory->height);

  Entity *follow_entity = get_entity_by_id(app, app->camera_follow);
  {
    if (input.once.key_r) {
      create_shader(&app->another_program, "another_vert.glsl", "another_frag.glsl");
      create_shader(&app->debug_program, "debug_vert.glsl", "debug_frag.glsl");
      create_shader(&app->ui_program, "ui_vert.glsl", "ui_frag.glsl");
      create_shader(&app->solid_program, "solid_vert.glsl", "solid_frag.glsl");
      create_shader(&app->fullscreen_fog_program, "fullscreen.vert", "fullscreen_fog.frag");
      create_shader(&app->fullscreen_color_program, "fullscreen.vert", "fullscreen_color.frag");
      create_shader(&app->fullscreen_fxaa_program, "fullscreen.vert", "fullscreen_fxaa.frag");
      create_shader(&app->fullscreen_bloom_program, "fullscreen.vert", "fullscreen_bloom.frag");
      create_shader(&app->fullscreen_SSAO_program, "fullscreen.vert", "fullscreen_ssao.frag");
      create_shader(&app->fullscreen_program, "fullscreen.vert", "fullscreen.frag");
      create_shader(&app->fullscreen_depth_program, "fullscreen.vert", "fullscreen_depth.frag");
      create_shader(&app->terrain_program, "terrain.vert", "terrain.frag");
      create_shader(&app->skybox_program, "skybox.vert", "skybox.frag");
      create_shader(&app->program, "vert.glsl", "frag.glsl");
      create_shader(&app->textured_program, "textured.vert", "textured.frag");

      unload_texture(&app->gradient_texture);
      load_texture(&app->gradient_texture, STBI_rgb_alpha);
      initialize_texture(&app->gradient_texture, GL_RGBA, false);
      stbi_image_free(app->gradient_texture.data);
      app->gradient_texture.data = NULL;

      unload_texture(&app->color_correction_texture);
      load_color_grading_texture(&app->color_correction_texture);

      unload_texture(&app->circle_texture);
      load_texture(&app->circle_texture, STBI_rgb_alpha);
      initialize_texture(&app->circle_texture, GL_RGBA, true);

      unload_model(&app->rock_model);

      for (u32 i=0; i<array_count(app->trees); i++) {
        unload_model(app->trees + i);
      }
    }

    if (input.once.key_p) {
      app->editing_mode = !app->editing_mode;
      if (!app->editing_mode) {
        platform.lock_mouse();
      }
    }

    if (input.is_mouse_locked) {
      follow_entity->rotation.y += static_cast<float>(input.rel_mouse_x)/200.0f;
      follow_entity->rotation.x += static_cast<float>(input.rel_mouse_y)/200.0f;
    }

    static float halfpi = glm::half_pi<float>();

    if (follow_entity->rotation[0] > halfpi - 0.001f) {
      follow_entity->rotation[0] = halfpi - 0.001f;
    }

    if (follow_entity->rotation[0] < -halfpi + 0.001f) {
      follow_entity->rotation[0] = -halfpi + 0.001f;
    }

    glm::vec3 rotation = follow_entity->rotation;

    glm::vec3 forward;
    forward.x = glm::sin(rotation[1]) * glm::cos(rotation[0]);
    forward.y = -glm::sin(rotation[0]);
    forward.z = -glm::cos(rotation[1]) * glm::cos(rotation[0]);

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0, 1.0, 0.0)));

    glm::vec3 movement;

    if (input.up) {
      movement += forward;
    }

    if (input.down) {
      movement -= forward;
    }

    if (input.left) {
      movement -= right;
    }

    if (input.right) {
      movement += right;
    }

    float speed = 1.3f;

    if (app->editing_mode) {
      speed = 10.0f;
      if (input.space) {
        movement.y += 1;
      }
      if (input.shift) {
        movement.y += -1;
      }
    } else {
      if (input.shift) {
        speed = 10.0f;
      }
      movement.y = 0;
    }

    if (glm::length(movement) > 0.0f) {
      movement = glm::normalize(movement);
    }

    follow_entity->velocity += movement * speed;

    for (u32 i=0; i<app->entity_count; i++) {
      Entity *entity = app->entities + i;

      if (entity->type == EntityBlock) {
        float distance = glm::distance(entity->position, follow_entity->position);
        if (distance < 1000.0f) {
          entity->flags |= EntityFlags::RENDER_WIREFRAME;
        } else {
          entity->flags = entity->flags & ~EntityFlags::RENDER_WIREFRAME;
        }
      } else if (entity->type == EntityPlayer) {
        entity->position += entity->velocity;
        entity->velocity *= 0.6f;
      }
    }

    if (follow_entity->position.x < 0) { follow_entity->position.x = 0; }
    if (follow_entity->position.z < 0) { follow_entity->position.z = 0; }

    if (!app->editing_mode) {
      follow_entity->position.y = get_terrain_height_at(follow_entity->position.x, follow_entity->position.z);
    } else {
      float terrain = get_terrain_height_at(follow_entity->position.x, follow_entity->position.z);
      if (follow_entity->position.y < terrain) {
        follow_entity->position.y = terrain;
      }
    }
    /* printf("position: %f %f %f\n", follow_entity->position.x, follow_entity->position.y, follow_entity->position.z); */
    /* printf("rotation: %f %f %f\n\n", follow_entity->rotation.x, follow_entity->rotation.y, follow_entity->rotation.z); */

    app->camera.rotation = follow_entity->rotation;
    app->camera.position = follow_entity->position + glm::vec3(0.0f, 70.0f, 0.0f);
    app->camera.view_matrix = get_camera_projection(&app->camera);
    app->camera.view_matrix = glm::rotate(app->camera.view_matrix, app->camera.rotation.x, glm::vec3(1.0, 0.0, 0.0));
    app->camera.view_matrix = glm::rotate(app->camera.view_matrix, app->camera.rotation.y, glm::vec3(0.0, 1.0, 0.0));
    app->camera.view_matrix = glm::rotate(app->camera.view_matrix, app->camera.rotation.z, glm::vec3(0.0, 0.0, 1.0));
    app->camera.view_matrix = glm::translate(app->camera.view_matrix, (app->camera.position * -1.0f));
    fill_frustum_with_matrix(&app->camera.frustum, app->camera.view_matrix);

    if (app->editing_mode && input.mouse_click) {
      Ray ray = get_mouse_ray(app, input, memory);

      Entity *closest_entity = NULL;
      float closest_distance = FLT_MAX;
      glm::vec3 hit_position;

      for (u32 i=0; i<app->entity_count; i++) {
        Entity *entity = app->entities + i;
        if (!(entity->flags & EntityFlags::RENDER_HIDDEN)) {
          RayMatchResult hit = ray_match_sphere(ray, entity->position, app->editor.handle_size);

          if (hit.hit) {
            if (hit.distance < closest_distance) {
              closest_distance = hit.distance;
              closest_entity = entity;
              hit_position = hit.hit_position;
            }
          }
        }
      }

      if (closest_entity) {
        app->editor.entity_id = closest_entity->id;
        app->editor.holding_entity = true;
        app->editor.distance_from_entity_offset = glm::distance(app->camera.position, hit_position);
        app->editor.hold_offset = hit_position - closest_entity->position;
        app->editor.inspect_entity = true;
      } else {
        app->editor.inspect_entity = false;
      }
    }


    {
      /* app->shadow_camera.position = glm::vec3(20000.0, 4000.0, 20000.0); */
      app->shadow_camera.position = app->camera.position + glm::vec3(0.0, 4000.0, 0.0);
      /* app->shadow_camera.rotation = glm::vec3(1.229797, -1.170025, 0.0f); */
      app->shadow_camera.rotation = glm::vec3(pi / 2.0f, 0.0f, 0.0f);
      app->shadow_camera.view_matrix = get_camera_projection(&app->shadow_camera);
      app->shadow_camera.view_matrix = glm::rotate(app->shadow_camera.view_matrix, app->shadow_camera.rotation.x, glm::vec3(1.0, 0.0, 0.0));
      app->shadow_camera.view_matrix = glm::rotate(app->shadow_camera.view_matrix, app->shadow_camera.rotation.y, glm::vec3(0.0, 1.0, 0.0));
      app->shadow_camera.view_matrix = glm::rotate(app->shadow_camera.view_matrix, app->shadow_camera.rotation.z, glm::vec3(0.0, 0.0, 1.0));
      app->shadow_camera.view_matrix = glm::translate(app->shadow_camera.view_matrix, (app->shadow_camera.position * -1.0f));

      fill_frustum_with_matrix(&app->shadow_camera.frustum, app->shadow_camera.view_matrix);

    #if 0
      glm::vec3 forward;
      forward.x = glm::sin(app->shadow_camera.rotation[1]) * glm::cos(app->shadow_camera.rotation[0]);
      forward.y = -glm::sin(app->shadow_camera.rotation[0]);
      forward.z = -glm::cos(app->shadow_camera.rotation[1]) * glm::cos(app->shadow_camera.rotation[0]);
      forward = glm::normalize(forward);

      glm::vec3 up = glm::vec3(0.0, 1.0, 0.0);

      glm::vec3 fc = app->shadow_camera.position + forward * app->shadow_camera.far;

      glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0, 1.0, 0.0)));

      glm::vec3 ftl = fc + (up * app->shadow_camera.size.x/2.0f) - (right * app->shadow_camera.size.y * app->shadow_camera.far/2.0f);
      glm::vec3 ftr = fc + (up * app->shadow_camera.size.x/2.0f) + (right * app->shadow_camera.size.y * app->shadow_camera.far/2.0f);
      glm::vec3 fbl = fc - (up * app->shadow_camera.size.x/2.0f) - (right * app->shadow_camera.size.y * app->shadow_camera.far/2.0f);
      glm::vec3 fbr = fc - (up * app->shadow_camera.size.x/2.0f) + (right * app->shadow_camera.size.y * app->shadow_camera.far/2.0f);

      glm::vec3 nc = app->shadow_camera.position + forward * app->shadow_camera.near;

      glm::vec3 ntl = nc + (up * app->shadow_camera.size.x/2.0f) - (right * app->shadow_camera.size.y * app->shadow_camera.near/2.0f);
      glm::vec3 ntr = nc + (up * app->shadow_camera.size.x/2.0f) + (right * app->shadow_camera.size.y * app->shadow_camera.near/2.0f);
      glm::vec3 nbl = nc - (up * app->shadow_camera.size.x/2.0f) - (right * app->shadow_camera.size.y * app->shadow_camera.near/2.0f);
      glm::vec3 nbr = nc - (up * app->shadow_camera.size.x/2.0f) + (right * app->shadow_camera.size.y * app->shadow_camera.near/2.0f);

      {
        app->debug_lines.push_back(ftl);
        app->debug_lines.push_back(ftr);

        app->debug_lines.push_back(ftl);
        app->debug_lines.push_back(fbl);

        app->debug_lines.push_back(fbl);
        app->debug_lines.push_back(fbr);

        app->debug_lines.push_back(fbr);
        app->debug_lines.push_back(fbl);
      }

      {
        app->debug_lines.push_back(ntl);
        app->debug_lines.push_back(ntr);

        app->debug_lines.push_back(ntl);
        app->debug_lines.push_back(nbl);

        app->debug_lines.push_back(nbl);
        app->debug_lines.push_back(nbr);

        app->debug_lines.push_back(nbr);
        app->debug_lines.push_back(nbl);
      }
#endif
    }

    if (app->editing_mode) {

#if 0
      if (input.once.key_o) {
        Entity *entity = app->entities + app->entity_count++;

        entity->id = next_entity_id(app);
        entity->type = EntityBlock;
        entity->position = follow_entity->position;
        entity->scale = glm::vec3(100.0f);
        entity->model = get_model_by_name(app, (char *)"rock");
        entity->color = glm::vec4(get_random_float_between(0.2f, 0.5f), 0.45f, 0.5f, 1.0f);
        entity->rotation = glm::vec3(get_random_float_between(0.0, pi * 2.0f), get_random_float_between(0.0, pi * 2.0f), get_random_float_between(0.0, pi * 2.0f));
        entity->flags = EntityFlags::CASTS_SHADOW | EntityFlags::PERMANENT_FLAG;
      }
#endif

      if (input.right_mouse_down) {
        if (!input.is_mouse_locked) {
          platform.lock_mouse();
        }
      } else {
        if (input.is_mouse_locked) {
          platform.unlock_mouse();
        }
      }

      if (!input.left_mouse_down) {
        app->editor.holding_entity = false;
      }

      if (app->editor.holding_entity) {
        Entity *entity = get_entity_by_id(app, app->editor.entity_id);

        Ray ray = get_mouse_ray(app, input, memory);
        entity->position = (ray.start + ray.direction * app->editor.distance_from_entity_offset) - app->editor.hold_offset;

        if ((entity->flags & EntityFlags::MOUNT_TO_TERRAIN) != 0) {
          mount_entity_to_terrain(entity);
        }
      }
    } else {
      if (input.mouse_click) {
        platform.lock_mouse();
      }

      if (input.escape) {
        platform.unlock_mouse();
      }
    }
  }
  PROFILE_END(update);

  // NOTE(sedivy): render
  {
    PROFILE(render);

    {
      PROFILE(render_shadows);
      glBindFramebuffer(GL_FRAMEBUFFER, app->shadow_buffer);
      glViewport(0, 0, app->shadow_width, app->shadow_height);
      glClear(GL_DEPTH_BUFFER_BIT);

      use_program(app, &app->solid_program);

      send_shader_uniform(app->current_program, "uPMatrix", app->shadow_camera.view_matrix);
      send_shader_uniform(app->current_program, "in_color", glm::vec4(0.0f));

      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
      glCullFace(GL_FRONT);

      {
        int x_coord = static_cast<int>(app->shadow_camera.position.x / CHUNK_SIZE_X);
        int y_coord = static_cast<int>(app->shadow_camera.position.z / CHUNK_SIZE_Y);

        for (int y=-4 + y_coord; y<=4 + y_coord; y++) {
          for (int x=-4 + x_coord; x<=4 + x_coord; x++) {
            if (x < 0 || y < 0 ) { continue; }

            TerrainChunk *chunk = get_chunk_at(app->chunk_cache, app->chunk_cache_count, x, y);

            if (process_terrain(memory, chunk)) {
              continue;
            }

            if (!is_sphere_in_frustum(&app->camera.frustum, glm::vec3(chunk->x * CHUNK_SIZE_X, 0.0f, chunk->y * CHUNK_SIZE_Y), chunk->models[0].radius)) {
              continue;
            }

            int dx = chunk->x - x_coord;
            int dy = chunk->y - y_coord;

            float distance = glm::pow(dx, 2) + glm::pow(dy, 2);

            int detail_level = 2;
            if (distance < 16.0f) { detail_level = 1; }
            if (distance < 4.0f) { detail_level = 0; }

            Model *model = &chunk->models[detail_level];

            render_terrain_chunk(app, chunk, model);
          }
        }
      }

      start_render_group(&app->render_group);
      for (u32 i=0; i<app->entity_count; i++) {
        Entity *entity = app->entities + i;

        if ((entity->flags & EntityFlags::CASTS_SHADOW) == 0 || (entity->flags & EntityFlags::RENDER_HIDDEN) != 0) { continue; }

        bool model_wait = process_model(memory, entity->model);
        bool texture_wait = false;

        if (entity->texture) {
          texture_wait = process_texture(memory, entity);
        }

        if (model_wait || texture_wait) { continue; }

        if (!is_sphere_in_frustum(&app->shadow_camera.frustum, entity->position, entity->model->radius * glm::compMax(entity->scale))) {
          continue;
        }

        glm::mat4 model_view;
        model_view = glm::translate(model_view, entity->position);
        model_view = glm::rotate(model_view, entity->rotation.x, glm::vec3(1.0, 0.0, 0.0));
        model_view = glm::rotate(model_view, entity->rotation.y, glm::vec3(0.0, 1.0, 0.0));
        model_view = glm::rotate(model_view, entity->rotation.z, glm::vec3(0.0, 0.0, 1.0));
        model_view = glm::scale(model_view, entity->scale);

        RenderCommand command;
        command.shader = &app->solid_program;
        command.model_view = model_view;
        command.flags = 0;
        command.color = glm::vec4(1.0f);
        command.cull_type = GL_FRONT;

        command.model_mesh = &entity->model->mesh;
        add_command_to_render_group(&app->render_group, command);
      }

      end_render_group(app, &app->render_group);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      PROFILE_END(render_shadows);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, app->frames[0].id);
    glViewport(0, 0, app->frames[0].width, app->frames[0].height);
    glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // NOTE(sedivy): skybox
    {
      PROFILE(render_skybox);
      glDisable(GL_CULL_FACE);
      glDisable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE);

      use_program(app, &app->skybox_program);

      glm::mat4 projection = get_camera_projection(&app->camera);

      projection = glm::rotate(projection, app->camera.rotation.x, glm::vec3(1.0, 0.0, 0.0));
      projection = glm::rotate(projection, app->camera.rotation.y, glm::vec3(0.0, 1.0, 0.0));
      projection = glm::rotate(projection, app->camera.rotation.z, glm::vec3(0.0, 0.0, 1.0));

      send_shader_uniform(app->current_program, "projection", projection);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_CUBE_MAP, app->cubemap.id);
      send_shader_uniformi(app->current_program, "uSampler", 0);

      use_model_mesh(app, &app->cube_model.mesh);
      glDrawElements(GL_TRIANGLES, app->cube_model.mesh.data.indices_count, GL_UNSIGNED_INT, 0);

      glDepthMask(GL_TRUE);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
      PROFILE_END(render_skybox);
    }

    {
      PROFILE(render_main);
      // NOTE(sedivy): Terrain
      {
        use_program(app, &app->terrain_program);

        glCullFace(GL_BACK);

        send_shader_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, app->shadow_depth_texture);
        send_shader_uniformi(app->current_program, "uShadow", 0);
        send_shader_uniform(app->current_program, "shadow_matrix", app->shadow_camera.view_matrix);
        send_shader_uniform(app->current_program, "texmapscale", glm::vec2(1.0f / app->shadow_width, 1.0f / app->shadow_height));

#if 1
        int x_coord = static_cast<int>(app->camera.position.x / CHUNK_SIZE_X);
        int y_coord = static_cast<int>(app->camera.position.z / CHUNK_SIZE_Y);
#else
        int x_coord = static_cast<int>(20000.0f / CHUNK_SIZE_X);
        int y_coord = static_cast<int>(20000.0f / CHUNK_SIZE_Y);
#endif

        u32 chunk_count = 0;

        start_render_group(&app->render_group);

        PROFILE(render_chunks);
        for (int y=-4 + y_coord; y<=4 + y_coord; y++) {
          for (int x=-4 + x_coord; x<=4 + x_coord; x++) {
            if (x < 0 || y < 0 ) { continue; }

            TerrainChunk *chunk = get_chunk_at(app->chunk_cache, app->chunk_cache_count, x, y);

            if (process_terrain(memory, chunk)) {
              continue;
            }

            if (!is_sphere_in_frustum(&app->camera.frustum, glm::vec3(chunk->x * CHUNK_SIZE_X, 0.0f, chunk->y * CHUNK_SIZE_Y), chunk->models[0].radius)) {
              continue;
            }

            int dx = chunk->x - x_coord;
            int dy = chunk->y - y_coord;

            float distance = glm::pow(dx, 2) + glm::pow(dy, 2);

            int detail_level = 2;
            if (distance < 16.0f) { detail_level = 1; }
            if (distance < 4.0f) { detail_level = 0; }

            Model *model = &chunk->models[detail_level];

            if (render_terrain_chunk(app, chunk, model)) {
              chunk_count += 1;
            }
          }
        }

        end_render_group(app, &app->render_group);
        PROFILE_END_COUNTED(render_chunks, chunk_count);
      }

      {
        start_render_group(&app->render_group);

        use_program(app, &app->another_program);

        send_shader_uniform(app->current_program, "eye_position", app->camera.position);
        send_shader_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, app->shadow_depth_texture);
        send_shader_uniformi(app->current_program, "uShadow", 0);
        send_shader_uniform(app->current_program, "shadow_matrix", app->shadow_camera.view_matrix);
        send_shader_uniform(app->current_program, "texmapscale", glm::vec2(1.0f / app->shadow_width, 1.0f / app->shadow_height));
        send_shader_uniform(app->current_program, "shadow_light_position", app->shadow_camera.position);

        PROFILE(render_entities);
        for (u32 i=0; i<app->entity_count; i++) {
          Entity *entity = app->entities + i;

          if (entity->flags & EntityFlags::RENDER_HIDDEN) { continue; }

          bool model_wait = process_model(memory, entity->model);
          bool texture_wait = false;

          if (entity->texture) {
            texture_wait = process_texture(memory, entity);
          }

          if (model_wait || texture_wait) { continue; }

          if (!is_sphere_in_frustum(&app->camera.frustum, entity->position, entity->model->radius * glm::compMax(entity->scale))) {
            continue;
          }

          glm::mat4 model_view;

          model_view = glm::translate(model_view, entity->position);

          model_view = glm::rotate(model_view, entity->rotation.x, glm::vec3(1.0, 0.0, 0.0));
          model_view = glm::rotate(model_view, entity->rotation.y, glm::vec3(0.0, 1.0, 0.0));
          model_view = glm::rotate(model_view, entity->rotation.z, glm::vec3(0.0, 0.0, 1.0));

          model_view = glm::scale(model_view, entity->scale);

          glm::mat3 normal = glm::inverseTranspose(glm::mat3(model_view));

#if 0
          {
            if (!process_model(memory, &app->sphere_model)) {
              glm::mat4 sphere_view;
              sphere_view = glm::translate(sphere_view, entity->position);
              sphere_view = glm::scale(sphere_view, glm::vec3(entity->model->radius) * glm::compMax(entity->scale));

              RenderCommand command;
              command.shader = &app->another_program;
              command.model_view = sphere_view;
              command.flags = 0;

              command.normal = glm::inverseTranspose(glm::mat3(sphere_view));
              command.color = glm::vec3(1.0f);

              command.model_mesh = &app->sphere_model.mesh;
              add_command_to_render_group(&app->render_group, command);
            }
          }
#endif

          RenderCommand command;
          command.shader = &app->another_program;
          command.model_view = model_view;
          command.flags = entity->flags;
          command.normal = normal;
          command.color = entity->color;
          command.cull_type = GL_BACK;

          command.model_mesh = &entity->model->mesh;
          add_command_to_render_group(&app->render_group, command);
        }
        end_render_group(app, &app->render_group);
        PROFILE_END_COUNTED(render_entities, app->entity_count);
      }

      {
        PROFILE(render_debug);
        use_program(app, &app->debug_program);
        glBindBuffer(GL_ARRAY_BUFFER, app->debug_buffer);
        glBufferData(GL_ARRAY_BUFFER, app->debug_lines.size() * sizeof(GLfloat)*3*2, &app->debug_lines[0], GL_STREAM_DRAW);
        send_shader_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawArrays(GL_LINES, 0, app->debug_lines.size()*2);
        app->debug_lines.clear();
        PROFILE_END(render_debug);
      }

      PROFILE_END(render_main);
    }

    {
      PROFILE(render_final);
      glViewport(0, 0, app->frames[0].width, app->frames[1].height);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);

      app->write_frame = 1;
      app->read_frame = 0;

#if 0
      {
        glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
        use_program(app, &app->fullscreen_SSAO_program);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, app->frames[app->read_frame].texture);

        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, app->frames[app->read_frame].depth);

        send_shader_uniformi(app->current_program, "uSampler", 0);
        send_shader_uniformi(app->current_program, "uDepth", 1);

        glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
        glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        std::swap(app->write_frame, app->read_frame);
      }
#endif

      if (app->color_correction) {
        glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
        use_program(app, &app->fullscreen_color_program);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, app->frames[app->read_frame].texture);

        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, app->color_correction_texture.id);

        send_shader_uniformi(app->current_program, "uSampler", 0);
        send_shader_uniformi(app->current_program, "color_correction_texture", 1);
        send_shader_uniformf(app->current_program, "lut_size", 16.0f);

        glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
        glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        std::swap(app->write_frame, app->read_frame);
      }

      if (app->editing_mode) {
        draw_3d_debug_info(app);
      }

      if (app->antialiasing) {
        glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
        use_program(app, &app->fullscreen_fxaa_program);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, app->frames[app->read_frame].texture);

        send_shader_uniformi(app->current_program, "uSampler", 0);
        send_shader_uniform(app->current_program, "texture_size", glm::vec2(app->frames[0].width, app->frames[0].height));

        glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
        glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        std::swap(app->write_frame, app->read_frame);
      }

      if (app->bloom) {
        glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
        use_program(app, &app->fullscreen_bloom_program);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, app->frames[app->read_frame].texture);

        send_shader_uniformi(app->current_program, "uSampler", 0);

        glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
        glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        std::swap(app->write_frame, app->read_frame);
      }

      {
        glViewport(0, 0, memory->width, memory->height);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        use_program(app, &app->fullscreen_program);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, app->frames[app->read_frame].texture);

        send_shader_uniformi(app->current_program, "uSampler", 0);

        glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
        glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
      }

      PROFILE_END(render_final);
    }

    PROFILE_END(render);

    if (app->editing_mode) {
      PROFILE(render_ui_flush);
      flush_2d_render(app, memory);
      PROFILE_END(render_ui_flush);
    }
  }

  memcpy(memory->last_frame_counters, memory->counters, sizeof(memory->counters));
  for (u32 i=0; i<array_count(memory->counters); i++) {
    DebugCounter *counter = memory->counters + i;

    if (counter->hit_count) {
      counter->cycle_count = 0;
      counter->hit_count = 0;
    }
  }

  u64 diff = platform.get_time();
  if (diff >= app->frametimelast + 1000) {
    app->fps = app->framecount;
    app->framecount = 0;
    app->frametimelast = diff;
  } else {
    app->framecount += 1;
  }

  PROFILE_END(frame);
}

