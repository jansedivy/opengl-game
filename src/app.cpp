#include "app.h"

void normalize_plane(Plane *plane) {
  float mag = glm::sqrt(plane->normal.x * plane->normal.x + plane->normal.y * plane->normal.y + plane->normal.z * plane->normal.z);
  plane->normal = plane->normal / mag;
  plane->distance = plane->distance / mag;
}

inline Entity *get_entity_by_id(App *app, u32 id) {
  PROFILE_BLOCK("Finding entity");
  for (u32 i=0; i<app->entity_count; i++) {
    Entity *entity = app->entities + i;
    if (entity->id == id) {
      return entity;
    }
  }

  return NULL;
}

char *join_string(char *first, char *second) {
  char *result = static_cast<char *>(malloc(strlen(first) + strlen(second) + 1));

  strcpy(result, first);
  strcat(result, second);

  return result;
}

void acquire_asset_file(char *path) {
#if INTERNAL
  PROFILE_BLOCK("Acquiring Asset");
  char *original_file_path = join_string(debug_global_memory->debug_assets_path, path);
  u64 original_time = platform.get_file_time(original_file_path);
  u64 used_time = platform.get_file_time(path);

  if (original_time > used_time) {
    DebugReadFileResult result = platform.debug_read_entire_file(original_file_path);

    PlatformFile file = platform.open_file(path, "w+");
    platform.write_to_file(file, result.fileSize, result.contents);
    platform.close_file(file);

    platform.debug_free_file(result);
  }

  free(original_file_path);
#endif
}

void load_texture(Texture *texture, int type=0) {
  if (platform.atomic_exchange(&texture->state, AssetState::EMPTY, AssetState::PROCESSING)) {
    PROFILE_BLOCK("Loading Model");
    acquire_asset_file((char *)texture->path);
    int channels;

    int width, height;
    u8* image = stbi_load(texture->path, &width, &height, &channels, type);

    texture->width = width;
    texture->height = height;

    texture->data = image;
    texture->state = AssetState::HAS_DATA;
  }
}

float distance_from_plane(Plane plane, vec3 position) {
  return glm::dot(plane.normal, position) + plane.distance;
}

const char *get_filename_ext(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename) return "";
  return dot + 1;
}

vec3 read_vector(char *start) {
  vec3 result;
  sscanf(start, "%f,%f,%f", &result.x, &result.y, &result.z);
  return result;
}

vec4 read_vector4(char *start) {
  vec4 result;
  sscanf(start, "%f,%f,%f,%f", &result.x, &result.y, &result.z, &result.w);
  return result;
}

Model *get_model_by_name(App *app, char *name) {
  return app->models[name];
}

float get_terrain_height_at(float x, float y) {
  return (
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

  PlatformFile file = platform.open_file((char *)"assets/level.level", "w");
  platform.write_to_file(file, sizeof(header), &header);
  for (auto it = loaded_level.entities.begin(); it != loaded_level.entities.end(); it++) {
    platform.write_to_file(file, sizeof(EntitySave), &(*it));
  }
  platform.close_file(file);
}

void save_text_level_file(Memory *memory, LoadedLevel level) {
#if INTERNAL
  platform.create_directory(memory->debug_level_path);

  for (auto it = level.entities.begin(); it != level.entities.end(); it++) {
    char full_path[256];
    sprintf(full_path, "%s/%d.entity", memory->debug_level_path, it->id);

    PlatformFile file = platform.open_file(full_path, "w");

    // TODO(sedivy): replace fprintf
    fprintf((FILE *)file.platform, "%d\n", it->id);
    fprintf((FILE *)file.platform, "t: %d\n", it->type);
    fprintf((FILE *)file.platform, "p: %f, %f, %f\n", it->position.x, it->position.y, it->position.z);
    fprintf((FILE *)file.platform, "s: %f, %f, %f\n", it->scale.x, it->scale.y, it->scale.z);
    fprintf((FILE *)file.platform, "r: %f, %f, %f\n", it->rotation.x, it->rotation.y, it->rotation.z);
    fprintf((FILE *)file.platform, "c: %f, %f, %f, %f\n", it->color.x, it->color.y, it->color.z, it->color.w);
    fprintf((FILE *)file.platform, "f: %d\n", it->flags);
    fprintf((FILE *)file.platform, "m: %s\n", it->model_name);

    platform.close_file(file);
  }
#endif
}

LoadedLevel load_binary_level_file() {
  LoadedLevel result;

  DebugReadFileResult file = platform.debug_read_entire_file("assets/level.level");

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
  dest->type = src->type;
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

#include <dirent.h>
void load_debug_level(Memory *memory, App *app) {
#if INTERNAL
  {
    PlatformDirectory dir = platform.open_directory(memory->debug_level_path);

    LoadedLevel loaded_level;

    while (dir.platform != NULL) {
      PlatformDirectoryEntry entry = platform.read_next_directory_entry(dir);
      if (entry.empty) { break; }

      if (platform.is_directory_entry_file(entry)) {
        char *full_path = (char *)alloca(sizeof(memory->debug_level_path) + sizeof(entry.name));
        sprintf(full_path, "%s/%s", memory->debug_level_path, entry.name);

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
                case 't': {
                            sscanf(start, "%d", &entity.type);
                          } break;
                case 'm': {
                            sscanf(start, "%s", entity.model_name);
                          } break;
                case 'c': {
                            entity.color = read_vector4(start);
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

    save_binary_level_file(loaded_level);

    platform.close_directory(dir);
  }
#endif

  {
    LoadedLevel bin_loaded_level = load_binary_level_file();

    for (auto it = bin_loaded_level.entities.begin(); it != bin_loaded_level.entities.end(); it++) {
      Entity *entity = app->entities + app->entity_count++;

      deserialize_entity(app, &(*it), entity);

      if (entity->id > app->last_id) {
        app->last_id = entity->id;
      }
    }
  }
}

RayMatchResult ray_match_sphere(Ray ray, vec3 position, float radius) {
  vec3 result_position;
  vec3 result_normal;

  RayMatchResult result;
  result.hit = intersectRaySphere(ray.start, ray.direction, position, radius, result_position, result_normal);
  if (result.hit) {
    result.hit_position = result_position;
  }

  return result;
}

mat4 make_billboard_matrix(vec3 position, vec3 camera_position, vec3 camera_up) {
  vec3 look = glm::normalize(camera_position - position);
  vec3 right = glm::cross(camera_up, look);
  vec3 up = glm::cross(look, right);
  mat4 transform;
  transform[0] = vec4(right, 0.0f);
  transform[1] = vec4(up, 0.0f);
  transform[2] = vec4(look, 0.0f);
  /* transform[3] = vec4(position, 1.0f); */
  return transform;
}

RayMatchResult ray_match_terrain(App *app, Ray ray) {
  RayMatchResult result;

  for (u32 i=0; i<app->chunk_cache_count; i++) {
    TerrainChunk *chunk = app->chunk_cache + i;
    if (chunk->initialized) {
      Model *model = NULL;
      for (u32 model_index=0; model_index<array_count(app->models); model_index++) {
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

  if (entity->model->state != AssetState::INITIALIZED) {
    result.hit = false;
    return result;
  }

  RayMatchResult sphere = ray_match_sphere(ray, entity->position, entity->model->radius * glm::compMax(entity->scale));
  if (!sphere.hit) {
    result.hit = false;
    return result;
  }

  mat4 model_view;
  model_view = glm::translate(model_view, entity->position);

  if (entity->flags & EntityFlags::LOOK_AT_CAMERA) {
    model_view *= make_billboard_matrix(entity->position, app->camera.position, vec3(app->camera.view_matrix[0][1], app->camera.view_matrix[1][1], app->camera.view_matrix[2][1]));
  }

  model_view = glm::rotate(model_view, entity->rotation.x, vec3(1.0, 0.0, 0.0));
  model_view = glm::rotate(model_view, entity->rotation.y, vec3(0.0, 1.0, 0.0));
  model_view = glm::rotate(model_view, entity->rotation.z, vec3(0.0, 0.0, 1.0));

  model_view = glm::scale(model_view, entity->scale);

  mat4 res = glm::inverse(model_view);

  vec3 start = vec3(res * vec4(ray.start, 1.0f));
  vec3 direction = vec3(res * vec4(ray.direction, 0.0f));

  ModelData mesh = entity->model->mesh.data;

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

bool is_sphere_in_frustum(Frustum *frustum, vec3 position, float radius) {
  for (int i=0; i<6; i++) {
    float distance = distance_from_plane(frustum->planes[i], position);
    if (distance + radius < 0) {
      return false;
    }
  }

  return true;
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

void unload_texture(Texture *texture) {
  if (platform.atomic_exchange(&texture->state, AssetState::INITIALIZED, AssetState::PROCESSING)) {
    glDeleteTextures(1, &texture->id);

    stbi_image_free(texture->data);

    texture->state = AssetState::EMPTY;
  }
};

void unload_model(Model *model) {
  if (platform.atomic_exchange(&model->state, AssetState::INITIALIZED, AssetState::PROCESSING)) {
    glDeleteBuffers(1, &model->mesh.vertices_id);
    glDeleteBuffers(1, &model->mesh.indices_id);
    glDeleteBuffers(1, &model->mesh.normals_id);
    glDeleteBuffers(1, &model->mesh.uv_id);

    free(model->mesh.data.data);

    model->state = AssetState::EMPTY;
  }
}

float calculate_radius(Mesh *mesh) {
  float max_radius = 0.0f;

  for (u32 i=0; i<mesh->data.vertices_count; i += 3) {
    float radius = glm::length(vec3(mesh->data.vertices[i], mesh->data.vertices[i + 1], mesh->data.vertices[i + 2]));
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

void initialize_texture(Texture *texture, GLenum interal_type=GL_RGB, GLenum type=GL_RGB, bool mipmap=true, GLenum wrap_type=GL_REPEAT) {
  if (platform.atomic_exchange(&texture->state, AssetState::HAS_DATA, AssetState::PROCESSING)) {
    glGenTextures(1, &texture->id);

    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexImage2D(GL_TEXTURE_2D, 0, interal_type, texture->width, texture->height, 0, type, GL_UNSIGNED_BYTE, texture->data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_type);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_type);

    if (mipmap) {
      float aniso = 0.0f;
      glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);

      glGenerateMipmap(GL_TEXTURE_2D);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    texture->state = AssetState::INITIALIZED;
  }
}

void optimize_model(Model *model) {
  VertexCacheOptimizer vco;
  vco.Optimize(model->mesh.data.indices, model->mesh.data.indices_count / 3);
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

  model->state = AssetState::INITIALIZED; // TODO(sedivy): atomic
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

#include "shader.h"

u32 next_entity_id(App *app) {
  return ++app->last_id;
}

u32 generate_blue_noise(vec2 *positions, u32 position_count, float min_radius, float radius) {
  u32 noise_count = 0;

  positions[0] = vec2(20000.0, 20000.0);
  noise_count += 1;

  bool full = false;
  for (u32 i=0; i<position_count; i++) {
    if (full) {
      break;
    }

    vec2 record = positions[i];

    for (u32 k=0; k<10; k++) {
      float angle = get_random_float_between(0.0f, tau);
      float random_distance = get_random_float_between(min_radius + radius, min_radius + radius);

      vec2 new_position = record + vec2(glm::cos(angle) * random_distance, glm::sin(angle) * random_distance);

      if (new_position.x < 0.0f || new_position.y < 0.0f || new_position.x > 40000.0f || new_position.y > 40000.0f) {
        continue;
      }

      bool hit = false;

      for (u32 l=0; l<noise_count; l++) {
        vec2 check_record = positions[l];

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
  vec2 noise_records[200];
  u32 size = generate_blue_noise(noise_records, array_count(noise_records), 500.0f, 400.0f);

  Random random = create_random_sequence();

  for (u32 i=0; i<size; i++) {
    Entity *entity = app->entities + app->entity_count++;

    entity->id = next_entity_id(app);
    entity->type = EntityBlock;
    entity->position.x = noise_records[i].x;
    entity->position.z = noise_records[i].y;
    mount_entity_to_terrain(entity);
    entity->scale = vec3(1.0f);
    entity->model = get_model_by_name(app, (char *)"tree_001");
    entity->color = vec4(0.7f, 0.3f, 0.2f, 1.0f);
    entity->rotation = vec3(0.0f, get_next_float(&random) * glm::pi<float>(), 0.0f);
    entity->flags = EntityFlags::PERMANENT_FLAG | EntityFlags::MOUNT_TO_TERRAIN;
  }
}

void save_level(Memory *memory, App *app) {
  LoadedLevel level;

  for (u32 i=0; i<app->entity_count; i++) {
    Entity *entity = app->entities + i;

    if (entity->flags & EntityFlags::PERMANENT_FLAG) {
      EntitySave save_entity = {};
      save_entity.id = entity->id;
      save_entity.type = entity->type;
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
  save_text_level_file(memory, level);
}

void quit(Memory *memory) {
}

void setup_all_shaders(App *app) {
  create_shader(&app->main_object_program, "assets/shaders/object.vert", "assets/shaders/object.frag");
  create_shader(&app->transparent_program, "assets/shaders/transparent.vert", "assets/shaders/transparent.frag");
  create_shader(&app->water_program, "assets/shaders/water.vert", "assets/shaders/water.frag");
  create_shader(&app->solid_program, "assets/shaders/solid.vert", "assets/shaders/solid.frag");
  create_shader(&app->phong_program, "assets/shaders/phong.vert", "assets/shaders/phong.frag");
  create_shader(&app->terrain_program, "assets/shaders/terrain.vert", "assets/shaders/terrain.frag");
  create_shader(&app->textured_program, "assets/shaders/textured.vert", "assets/shaders/textured.frag");
  create_shader(&app->controls_program, "assets/shaders/controls.vert", "assets/shaders/controls.frag");

  create_shader(&app->debug_program, "assets/shaders/debug.vert", "assets/shaders/debug.frag");
  create_shader(&app->particle_program, "assets/shaders/particle.vert", "assets/shaders/particle.frag");

  create_shader(&app->skybox_program, "assets/shaders/skybox.vert", "assets/shaders/skybox.frag");

  create_shader(&app->ui_program, "assets/shaders/ui.vert", "assets/shaders/ui.frag");

  create_shader(&app->fullscreen_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen.frag");
  create_shader(&app->fullscreen_merge_alpha, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_merge_alpha.frag");
  create_shader(&app->fullscreen_fog_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_fog.frag");
  create_shader(&app->fullscreen_color_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_color.frag");
  create_shader(&app->fullscreen_fxaa_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_fxaa.frag");
  create_shader(&app->fullscreen_bloom_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_bloom.frag");
  create_shader(&app->fullscreen_hdr_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_hdr.frag");
  create_shader(&app->fullscreen_SSAO_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_ssao.frag");
  create_shader(&app->fullscreen_depth_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_depth.frag");
  create_shader(&app->fullscreen_lens_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_lens_flare.frag");
}

void init(Memory *memory) {
  glewExperimental = GL_TRUE;
  glewInit();

  App *app = memory->app;

  app->antialiasing = true;
  app->color_correction = true;
  app->lens_flare = false;
  app->bloom = false;
  app->hdr = false;

  app->current_program = NULL;

  app->editor.handle_size = 40.0f;
  app->editor.holding_entity = false;
  app->editor.inspect_entity = false;
  app->editor.show_left = true;
  app->editor.show_right = true;
  app->editor.left_state = EditorLeftState::MODELING;
  app->editor.speed = 10000.0f;

  debug_global_memory = memory;

  platform = memory->platform;

  app->entity_count = 0;
  app->last_id = 0;

  app->camera.ortho = false;
  app->camera.near = 0.5f;
  app->camera.far = 50000.0f;
  app->camera.size = vec2((float)memory->width, (float)memory->height);

  app->shadow_camera.ortho = true;
  app->shadow_camera.near = 100.0f;
  app->shadow_camera.far = 6000.0f;
  app->shadow_camera.size = vec2(4096.0f, 4096.0f) / 2.0f;

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
      u32 uv_count = vertices_count;
      u32 indices_count = latitude_bands * longitude_bands * 6;

      Mesh mesh;
      allocate_mesh(&mesh, vertices_count, normals_count, indices_count, uv_count);

      u32 vertices_index = 0;
      u32 normals_index = 0;
      u32 uv_index = 0;
      u32 indices_index = 0;

      for (float lat_number = 0; lat_number <= latitude_bands; lat_number++) {
        float theta = lat_number * pi / latitude_bands;
        float sinTheta = sin(theta);
        float cos_theta = cos(theta);

        for (float long_number = 0; long_number <= longitude_bands; long_number++) {
          float phi = long_number * 2 * pi / longitude_bands;
          float sin_phi = sin(phi);
          float cos_phi = cos(phi);

          float x = cos_phi * sinTheta;
          float y = cos_theta;
          float z = sin_phi * sinTheta;

          mesh.data.uv[uv_index++] = 1 - (long_number / longitude_bands);
          mesh.data.uv[uv_index++] = 1 - (lat_number / latitude_bands);

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
      app->sphere_model.id_name = allocate_string("sphere");
      app->sphere_model.radius = radius;
      app->sphere_model.state = AssetState::HAS_DATA;
      app->models[app->sphere_model.id_name] = &app->sphere_model;
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

      app->cube_model.id_name = allocate_string("cube");
      app->cube_model.radius = calculate_radius(&mesh);
      app->cube_model.mesh = mesh;
      app->cube_model.state = AssetState::HAS_DATA;
      initialize_model(&app->cube_model);
      app->models[app->cube_model.id_name] = &app->cube_model;
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

      app->quad_model.id_name = allocate_string("quad");
      app->quad_model.radius = calculate_radius(&mesh);
      app->quad_model.mesh = mesh;
      app->quad_model.state = AssetState::HAS_DATA;
      initialize_model(&app->quad_model);
      app->models[app->quad_model.id_name] = &app->quad_model;
    }

    Model *model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/rock.obj");
    model->id_name = allocate_string("rock");
    app->models[model->id_name] = model;

    model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/tree_01.obj");
    model->id_name = allocate_string("tree_001");
    app->models[model->id_name] = model;

    model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/tree_02.obj");
    model->id_name = allocate_string("tree_002");
    app->models[model->id_name] = model;

    model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/tree_03.obj");
    model->id_name = allocate_string("tree_003");
    app->models[model->id_name] = model;
  }

  setup_all_shaders(app);

  app->chunk_cache_count = 4096;
  app->chunk_cache = static_cast<TerrainChunk *>(malloc(sizeof(TerrainChunk) * app->chunk_cache_count));

  for (u32 i=0; i<app->chunk_cache_count; i++) {
    TerrainChunk *chunk = app->chunk_cache + i;
    chunk->models[0].state = AssetState::EMPTY;
    chunk->models[1].state = AssetState::EMPTY;
    chunk->models[2].state = AssetState::EMPTY;
    chunk->initialized = false;
  }

  app->color_correction_texture.path = allocate_string("assets/textures/color_correction.png");
  app->planet_texture.path = allocate_string("assets/textures/planet.jpg");
  app->gradient_texture.path = allocate_string("assets/textures/gradient.png");
  app->circle_texture.path = allocate_string("assets/textures/circle.png");
  app->debug_texture.path = allocate_string("assets/textures/debug.png");
  app->debug_transparent_texture.path = allocate_string("assets/textures/debug_transparent.png");
  app->editor_texture.path = allocate_string("assets/textures/editor_images.png");

  {
    load_texture(&app->gradient_texture, STBI_rgb_alpha);
    initialize_texture(&app->gradient_texture, GL_RGBA, GL_RGBA, false);
    stbi_image_free(app->gradient_texture.data);
    app->gradient_texture.data = NULL;
  }

  {
    load_texture(&app->editor_texture, STBI_rgb_alpha);
    initialize_texture(&app->editor_texture, GL_RGBA, GL_RGBA, false);
    stbi_image_free(app->editor_texture.data);
    app->editor_texture.data = NULL;
  }

  {
    load_texture(&app->color_correction_texture, STBI_rgb_alpha);
    initialize_texture(&app->color_correction_texture, GL_RGB8, GL_RGBA, false);
    stbi_image_free(app->color_correction_texture.data);
    app->color_correction_texture.data = NULL;
  }

  {
    load_texture(&app->circle_texture, STBI_rgb_alpha);
    initialize_texture(&app->circle_texture, GL_RGBA, GL_RGBA, true);
    stbi_image_free(app->circle_texture.data);
    app->circle_texture.data = NULL;
  }

  {
    load_texture(&app->debug_texture, STBI_rgb_alpha);
    initialize_texture(&app->debug_texture, GL_RGBA, GL_RGBA, true);
    stbi_image_free(app->debug_texture.data);
    app->debug_texture.data = NULL;
  }

  {
    load_texture(&app->debug_transparent_texture, STBI_rgb_alpha);
    initialize_texture(&app->debug_transparent_texture, GL_RGBA, GL_RGBA, true);
    stbi_image_free(app->debug_transparent_texture.data);
    app->debug_transparent_texture.data = NULL;
  }

  {
    acquire_asset_file((char *)"assets/font.ttf");
    DebugReadFileResult font_file = platform.debug_read_entire_file("assets/font.ttf");
    app->font = create_font(font_file.contents, 16.0f);
    platform.debug_free_file(font_file);
  }

  {
    acquire_asset_file((char *)"assets/mono_font.ttf");
    DebugReadFileResult font_file = platform.debug_read_entire_file("assets/mono_font.ttf");
    app->mono_font = create_font(font_file.contents, 16.0f);
    platform.debug_free_file(font_file);
  }

  const char* faces[] = {
    "assets/textures/right.png", "assets/textures/left.png",
    "assets/textures/top.png", "assets/textures/bottom.png",
    "assets/textures/back.png", "assets/textures/front.png"
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
    acquire_asset_file((char *)faces[i]);
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

    frame->width = 1280;
    frame->height = 720;

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
    glGenFramebuffers(1, &app->transparent_buffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, app->transparent_buffer);

    glGenTextures(1, &app->transparent_color_texture);
    glBindTexture(GL_TEXTURE_2D, app->transparent_color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->frames[0].width, app->frames[0].height, 0, GL_RGBA, GL_FLOAT, NULL);
    /* glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, app->frames[0].width, app->frames[0].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); */
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, app->transparent_color_texture, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &app->transparent_alpha_texture);
    glBindTexture(GL_TEXTURE_2D, app->transparent_alpha_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, app->frames[0].width, app->frames[0].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    /* glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, app->frames[0].width, app->frames[0].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); */
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, app->transparent_alpha_texture, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    app->transparent_buffers[0] = GL_COLOR_ATTACHMENT0;
    app->transparent_buffers[1] = GL_COLOR_ATTACHMENT1;
  }


  {
    app->shadow_width = 4096;
    app->shadow_height = 4096;

    // NOTE(sedivy): depth
    {
      glGenTextures(1, &app->shadow_depth_texture);
      glBindTexture(GL_TEXTURE_2D, app->shadow_depth_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, app->shadow_width, app->shadow_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

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

  load_debug_level(memory, app);

  {
    Entity *entity = app->entities + app->entity_count;
    entity->id = next_entity_id(app);
    entity->type = EntityPlayer;
    entity->flags = EntityFlags::RENDER_HIDDEN | EntityFlags::HIDE_IN_EDITOR;
    entity->position.x = 0.0f;
    entity->position.y = 0.0f;
    entity->position.z = 0.0f;
    entity->color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
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

    vec2 rock_noise[200];
    u32 size = generate_blue_noise(rock_noise, array_count(rock_noise), 100.0f, 5000.0f);

    for (u32 i=0; i<size; i++) {
      Entity *entity = app->entities + app->entity_count;

      entity->id = next_entity_id(app);
      entity->type = EntityBlock;
      entity->position.x = rock_noise[i].x;
      entity->position.z = rock_noise[i].y;
      mount_entity_to_terrain(entity);
      entity->flags = EntityFlags::PERMANENT_FLAG;
      entity->scale = 100.0f * vec3(0.8f + get_next_float(&random) / 2.0f, 0.8f + get_next_float(&random) / 2.0f, 0.8f + get_next_float(&random) / 2.0f);
      entity->model = &app->rock_model;
      entity->color = vec4(get_random_float_between(0.2f, 0.5f), 0.45f, 0.5f, 1.0f);
      entity->rotation = vec3(get_next_float(&random) * 0.5f - 0.25f, get_next_float(&random) * glm::pi<float>(), get_next_float(&random) * 0.5f - 0.25f);
      app->entity_count += 1;
    }
#endif
  }

  app->framecount = 0;
  app->frametimelast = platform.get_time();

  float vertices[] = {
    -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
    -0.5f, 0.5f, 0.0f,

    0.5f, -0.5f, 0.0f,
    0.5f, 0.5f, 0.0f,
    -0.5f, 0.5f, 0.0f
  };

  glGenBuffers(1, &app->particle_model);
  glBindBuffer(GL_ARRAY_BUFFER, app->particle_model);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  app->next_particle = 0;

  glGenBuffers(1, &app->particle_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, app->particle_buffer);
  glBufferData(GL_ARRAY_BUFFER, array_count(app->particles) * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

  glGenBuffers(1, &app->particle_color_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, app->particle_color_buffer);
  glBufferData(GL_ARRAY_BUFFER, array_count(app->particles) * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
}

struct LoadModelWork {
  Model *model;
};

void load_model_work(void *data) {
  PROFILE_BLOCK("Loading Model");
  LoadModelWork *work = static_cast<LoadModelWork *>(data);

  if (platform.atomic_exchange(&work->model->state, AssetState::EMPTY, AssetState::PROCESSING)) {
    acquire_asset_file((char *)work->model->path);

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

          float new_distance = glm::length(vec3(mesh_data->mVertices[l].x, mesh_data->mVertices[l].y, mesh_data->mVertices[l].z));
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

    optimize_model(model);

    model->radius = max_distance;
    model->state = AssetState::HAS_DATA; // TODO(sedivy): atomic
  }

  free(work);
}

void load_texture_work(void *data) {
  load_texture(static_cast<Texture*>(data));
}

inline bool process_texture(Memory *memory, Texture *texture) {
  if (texture->state == AssetState::INITIALIZED) {
    return false;
  }

  if (platform.queue_has_free_spot(memory->low_queue)) {
    if (texture->state == AssetState::EMPTY) {
      platform.add_work(memory->low_queue, load_texture_work, texture);

      return true;
    }
  }

  if (texture->state == AssetState::HAS_DATA) {
    initialize_texture(texture);
    return false;
  }

  return true;
}

inline bool process_model(Memory *memory, Model *model) {
  if (model->state == AssetState::INITIALIZED) {
    return false;
  }

  if (platform.queue_has_free_spot(memory->low_queue)) {
    if (model->state == AssetState::EMPTY) {
      LoadModelWork *work = static_cast<LoadModelWork *>(malloc(sizeof(LoadModelWork)));
      work->model = model;

      platform.add_work(memory->low_queue, load_model_work, work);

      return true;
    }
  }

  if (model->state == AssetState::HAS_DATA) {
    initialize_model(model);
    return false;
  }

  return true;
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

#include "chunk.h"

vec4 shade_color(vec4 color, float percent) {
  return vec4(vec3(color) * (1 + percent), color.a);
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

#include "render_group.cpp"
#include "ui.cpp"

bool sort_by_distance(const EditorHandleRenderCommand &a, const EditorHandleRenderCommand &b) {
  return a.distance_from_camera > b.distance_from_camera;
}

bool sort_particles_by_distance(const Particle &a, const Particle &b) {
  return a.distance_from_camera > b.distance_from_camera;
}

void draw_3d_debug_info(Input &input, App *app) {
  PROFILE_BLOCK("Draw 3D Debug");
  use_program(app, &app->controls_program);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthFunc(GL_ALWAYS);

  set_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);

  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_2D, app->circle_texture.id);
  set_uniformi(app->current_program, "textureImage", 0);

  use_model_mesh(app, &app->quad_model.mesh);

  std::vector<EditorHandleRenderCommand> render_commands;

  if (app->editor.show_handles && !app->editor.holding_entity) {
    for (u32 i=0; i<app->entity_count; i++) {
      Entity *entity = app->entities + i;

      if (entity->type == EntityBlock) { continue; }

      if (entity->flags & EntityFlags::HIDE_IN_EDITOR) { continue; }

      if (!is_sphere_in_frustum(&app->camera.frustum, entity->position, app->editor.handle_size)) {
        continue;
      }

      mat4 model_view;
      model_view = glm::translate(model_view, entity->position);
      model_view = glm::scale(model_view, vec3(app->editor.handle_size));
      model_view *= make_billboard_matrix(entity->position, app->camera.position, vec3(app->camera.view_matrix[0][1], app->camera.view_matrix[1][1], app->camera.view_matrix[2][1]));

      EditorHandleRenderCommand command;
      command.distance_from_camera = glm::distance2(app->camera.position, entity->position);
      command.model_view = model_view;
      if (entity->type == EntityParticleEmitter) {
        command.color = vec4(0.0, 1.0, 1.0, 0.7);
      } else if (entity->type == EntityPlanet) {
        command.color = vec4(1.0, 1.0, 0.0, 0.7);
      } else {
        command.color = vec4(0.0, 1.0, 0.0, 0.7);
      }

      if (!input.is_mouse_locked && app->editor.hovering_entity && app->editor.hover_entity == entity->id) {
        command.color *= vec4(0.6, 0.6, 0.6, 1.0);
      }

      render_commands.push_back(command);
    }
  }

  std::sort(render_commands.begin(), render_commands.end(), sort_by_distance);

  for (auto it = render_commands.begin(); it != render_commands.end(); it++) {
    set_uniform(app->current_program, "uMVMatrix", it->model_view);
    set_uniform(app->current_program, "in_color", it->color);
    glDrawElements(GL_TRIANGLES, app->quad_model.mesh.data.indices_count, GL_UNSIGNED_INT, 0);
  }

  glDisable(GL_BLEND);
  glDepthFunc(GL_LESS);
}

void flush_2d_render(App *app, Memory *memory) {
  PROFILE_BLOCK("Draw UI Flush");
  mat4 projection = glm::ortho(0.0f, float(memory->width), float(memory->height), 0.0f);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glActiveTexture(GL_TEXTURE0);

  use_program(app, &app->ui_program);

  set_uniform(app->current_program, "uPMatrix", projection);
  set_uniformi(app->current_program, "textureImage", 0);

  u32 vertices_index = 0;

  UICommandBuffer *command_buffer = &app->editor.command_buffer;

  glBindBuffer(GL_ARRAY_BUFFER, app->debug_buffer);
  glBufferData(GL_ARRAY_BUFFER, command_buffer->vertices.size() * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, command_buffer->vertices.size() * sizeof(GLfloat), &command_buffer->vertices[0]);

  glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
  glVertexAttribPointer(shader_get_attribute_location(app->current_program, "uv"), 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *)(2 * sizeof(GLfloat)));

  for (auto it = command_buffer->commands.begin(); it != command_buffer->commands.end(); it++) {
    glBindTexture(GL_TEXTURE_2D, it->texture_id);
    set_uniform(app->current_program, "background_color", it->color);
    set_uniform(app->current_program, "image_color", it->image_color);
    glDrawArrays(GL_TRIANGLES, vertices_index, it->vertices_count);
    vertices_index += it->vertices_count;
  }

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

void draw_2d_debug_info(App *app, Memory *memory, Input &input) {
  PROFILE_BLOCK("Settup UI");
  UICommandBuffer *command_buffer = &app->editor.command_buffer;
  command_buffer->vertices.clear();
  command_buffer->commands.clear();

  DebugDrawState draw_state;
  draw_state.offset_top = 25.0f;
  draw_state.width = 175.0f;

  vec4 button_background_color = vec4(0.5f, 0.6f, 0.9f, 0.9f);
  vec4 default_background_color = vec4(0.2f, 0.4f, 0.9f, 0.9f);

  char text[256];

  push_toggle_button(app, input, &draw_state, command_buffer, 10.0f, &app->editor.show_left, vec4(1.0f, 0.16f, 0.15f, 0.9f));

  if (app->editor.show_left) {
    sprintf(text, "performance: %d\n", app->editor.show_performance);
    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
      app->editor.show_performance = !app->editor.show_performance;
    }

    if (app->editor.show_performance) {
      for (u32 i=0; i<array_count(global_last_frame_counters); i++) {
        DebugCounter *counter = global_last_frame_counters + i;

        if (counter->hit_count) {
          float time = (float)(counter->cycle_count * 1000) / (float)platform.get_performance_frequency();

          sprintf(text, "%17s: %6.3fms %4llu %6.3fms\n", counter->name, time, counter->hit_count, time / (float)counter->hit_count);

          float original_width = draw_state.width;
          draw_state.width = 350.0f;
          push_debug_text(app, &app->mono_font, &draw_state, command_buffer, 10.0f, text, vec3(1.0f, 1.0f, 1.0f), vec4(0.0f, 0.1f, 0.6f, 0.9f));
          draw_state.width = original_width;
        }
      }
    }

    sprintf(text, "fps: %d\n", (u32)app->fps);
    push_debug_text(app, &draw_state, command_buffer, 10.0f, text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

    draw_state.offset_top += 10.0f;

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 40.0f, (char *)"save!", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
      save_level(memory, app);
    }

    draw_state.offset_top += 10.0f;

    vec4 selected_button_background_color = vec4(0.5f, 0.8f, 1.0f, 0.9f);

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Modeling", vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::MODELING ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::MODELING;
    }

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Terrain", vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::TERRAIN ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::TERRAIN;
    }

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Light", vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::LIGHT ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::LIGHT;
    }

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Post Processing", vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::POST_PROCESSING ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::POST_PROCESSING;
    }

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Editor settings", vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::EDITOR_SETTINGS ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::EDITOR_SETTINGS;
    }

    draw_state.offset_top += 10.0f;

    switch (app->editor.left_state) {
      {
        case EditorLeftState::MODELING:
          struct Something {
            const char *id_name;
            const char *display_name;
          };

          Something types[] = {
            {"tree_001", "Tree 1"},
            {"tree_002", "Tree 2"},
            {"tree_003", "Tree 3"},
            {"rock", "Rock"},
            {"cube", "Cube"},
            {"sphere", "Sphere"},
            {"quad", "Quad"},
          };

          for (u32 i=0; i<array_count(types); i++) {
            if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)types[i].display_name, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {

              Entity *entity = app->entities + app->entity_count++;

              vec3 forward;
              forward.x = glm::sin(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);
              forward.y = -glm::sin(app->camera.rotation[0]);
              forward.z = -glm::cos(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);

              entity->id = next_entity_id(app);
              entity->type = EntityBlock;
              entity->position = app->camera.position + forward * 400.0f;
              entity->scale = vec3(100.f);
              entity->model = get_model_by_name(app, (char *)types[i].id_name);
              entity->color = vec4(get_random_float_between(0.2f, 0.5f), 0.45f, 0.5f, 1.0f);
              entity->flags = EntityFlags::CASTS_SHADOW | EntityFlags::PERMANENT_FLAG;

              if (strcmp(types[i].id_name, "quad") == 0) {
                entity->flags |= EntityFlags::LOOK_AT_CAMERA;
              }
            }
          }

          draw_state.offset_top += 10.0f;

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)"particle emitter", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            Entity *entity = app->entities + app->entity_count++;

            vec3 forward;
            forward.x = glm::sin(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);
            forward.y = -glm::sin(app->camera.rotation[0]);
            forward.z = -glm::cos(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);

            entity->id = next_entity_id(app);
            entity->type = EntityParticleEmitter;
            entity->position = app->camera.position + forward * 400.0f;
            entity->scale = vec3(40.0f, 0.0f, 0.0f);
            entity->model = get_model_by_name(app, (char *)"sphere");
            entity->color = vec4(1.0f);
            entity->flags = EntityFlags::RENDER_HIDDEN | EntityFlags::PERMANENT_FLAG;
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)"Planet", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            Entity *entity = app->entities + app->entity_count++;

            vec3 forward;
            forward.x = glm::sin(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);
            forward.y = -glm::sin(app->camera.rotation[0]);
            forward.z = -glm::cos(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);


            entity->id = next_entity_id(app);
            entity->type = EntityPlanet;
            entity->position = app->camera.position + forward * 400.0f;
            entity->scale = vec3(100.0f);
            entity->model = get_model_by_name(app, (char *)"sphere");
            entity->color = vec4(get_random_float_between(0.2f, 0.5f), 0.45f, 0.5f, 1.0f);
            entity->flags = EntityFlags::CASTS_SHADOW | EntityFlags::PERMANENT_FLAG;
            entity->positions.push_back(vec2(50.0833f, 14.4167f));
            entity->positions.push_back(vec2(37.7833f, 122.4167f));
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)"Water", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            Entity *entity = app->entities + app->entity_count++;

            vec3 forward;
            forward.x = glm::sin(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);
            forward.y = -glm::sin(app->camera.rotation[0]);
            forward.z = -glm::cos(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);

            entity->id = next_entity_id(app);
            entity->type = EntityWater;
            entity->position = app->camera.position + forward * 400.0f;
            entity->scale = vec3(100.0f);
            entity->model = get_model_by_name(app, (char *)"quad");
            entity->color = vec4(0.2f, 0.45f, 0.5f, 0.5f);
            entity->flags = EntityFlags::PERMANENT_FLAG;
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)"Phong", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            Entity *entity = app->entities + app->entity_count++;

            vec3 forward;
            forward.x = glm::sin(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);
            forward.y = -glm::sin(app->camera.rotation[0]);
            forward.z = -glm::cos(app->camera.rotation[1]) * glm::cos(app->camera.rotation[0]);

            entity->id = next_entity_id(app);
            entity->type = EntityBlock;
            entity->position = app->camera.position + forward * 400.0f;
            entity->scale = vec3(100.0f);
            entity->model = get_model_by_name(app, (char *)"sphere");
            entity->color = vec4(0.2f, 0.45f, 0.5f, 1.0f);
            entity->flags = EntityFlags::PERMANENT_FLAG;
          }
          break;
      }
      {
        case EditorLeftState::TERRAIN:
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"rebuild chunks", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            rebuild_chunks(app);
          }
          break;
      }
      {
        case EditorLeftState::LIGHT:
          push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, 10.0f, "rotation", &app->shadow_camera.rotation, default_background_color, 0.0f, tau);
          break;
      }
      {
        case EditorLeftState::EDITOR_SETTINGS:
          sprintf(text, "Show handles %d\n", app->editor.show_handles);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.show_handles = !app->editor.show_handles;
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"slow speed", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.speed = 10000.0f;
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"medium speed", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.speed = 40000.0f;
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"fast speed", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.speed = 60000.0f;
          }

          sprintf(text, "Experimenal editing: %d\n", app->editor.experimental_terrain_entity_movement);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.experimental_terrain_entity_movement = !app->editor.experimental_terrain_entity_movement;
          }
          break;
      }
      {
        case EditorLeftState::POST_PROCESSING:
          sprintf(text, "AA: %d\n", app->antialiasing);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->antialiasing = !app->antialiasing;
          }

          sprintf(text, "Color correction: %d\n", app->color_correction);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->color_correction = !app->color_correction;
          }

          sprintf(text, "HDR (wip): %d\n", app->hdr);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->hdr = !app->hdr;
          }

          sprintf(text, "Bloom (wip): %d\n", app->bloom);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->bloom = !app->bloom;
          }

          sprintf(text, "Lens flare (wip): %d\n", app->lens_flare);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->lens_flare = !app->lens_flare;
          }
          break;
      }
    }
  }

  if (app->editor.inspect_entity) {
    draw_state.offset_top = 25.0f;
    draw_state.width = 275.0f;

    push_toggle_button(app, input, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), &app->editor.show_right, vec4(1.0f, 0.16f, 0.15f, 0.9f));

    if (app->editor.show_right) {
      Entity *entity = get_entity_by_id(app, app->editor.entity_id);
      if (entity) {

        sprintf(text, "id: %d\n", entity->id);
        push_debug_text(app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

        sprintf(text, "model: %s\n", entity->model->id_name);
        push_debug_text(app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

        sprintf(text, "type: %d\n", entity->type);
        push_debug_text(app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

        push_debug_vector(&draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "position", entity->position, default_background_color);
        push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "rotation", &entity->rotation, default_background_color, 0.0f, pi * 2);
        push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "scale", &entity->scale, default_background_color, 0.0f, 200.0f);

        float start = draw_state.offset_top;
        push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "color", &entity->color, default_background_color, 0.0f, 1.0f);
        debug_render_rect(command_buffer, memory->width - 151.0f, start + 1.0f, 22.0f, 22.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f));
        debug_render_rect(command_buffer, memory->width - 150.0f, start + 2.0f, 20.0f, 20.0f, entity->color);

        sprintf(text, "ground mounted: %d\n", (entity->flags & EntityFlags::MOUNT_TO_TERRAIN) != 0);
        if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
          entity->flags = entity->flags ^ EntityFlags::MOUNT_TO_TERRAIN;

          if (entity->flags & EntityFlags::MOUNT_TO_TERRAIN) {
            mount_entity_to_terrain(entity);
          }
        }

        sprintf(text, "casts shadow: %d\n", (entity->flags & EntityFlags::CASTS_SHADOW) != 0);
        if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
          entity->flags = entity->flags ^ EntityFlags::CASTS_SHADOW;
        }

        sprintf(text, "look at camera: %d\n", (entity->flags & EntityFlags::LOOK_AT_CAMERA) != 0);
        if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
          entity->flags = entity->flags ^ EntityFlags::LOOK_AT_CAMERA;
        }

        sprintf(text, "save to file: %d\n", (entity->flags & EntityFlags::PERMANENT_FLAG) != 0);
        if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
          entity->flags = entity->flags ^ EntityFlags::PERMANENT_FLAG;
        }
      }
    }
  }
}

void render_skybox(App *app) {
  PROFILE_BLOCK("Draw Skybox");
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  use_program(app, &app->skybox_program);

  mat4 projection = get_camera_projection(&app->camera);

  projection = glm::rotate(projection, app->camera.rotation.x, vec3(1.0, 0.0, 0.0));
  projection = glm::rotate(projection, app->camera.rotation.y, vec3(0.0, 1.0, 0.0));
  projection = glm::rotate(projection, app->camera.rotation.z, vec3(0.0, 0.0, 1.0));

  set_uniform(app->current_program, "projection", projection);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, app->cubemap.id);
  set_uniformi(app->current_program, "uSampler", 0);

  use_model_mesh(app, &app->cube_model.mesh);
  glDrawElements(GL_TRIANGLES, app->cube_model.mesh.data.indices_count, GL_UNSIGNED_INT, 0);

  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

void render_scene(Memory *memory, App *app, Camera *camera, Shader *forced_shader=NULL, bool shadow_pass=false) {
  PROFILE_BLOCK("Draw Scene");
  start_render_group(&app->transparent_render_group);
  app->transparent_render_group.camera = camera;
  app->transparent_render_group.transparent_pass = true;
  app->transparent_render_group.force_shader = forced_shader;
  app->transparent_render_group.shadow_pass = shadow_pass;

  start_render_group(&app->render_group);
  app->render_group.camera = camera;
  app->render_group.transparent_pass = false;
  app->render_group.force_shader = forced_shader;
  app->render_group.shadow_pass = shadow_pass;

  for (u32 i=0; i<app->entity_count; i++) {
    Entity *entity = app->entities + i;

    if (entity->flags & EntityFlags::RENDER_HIDDEN) { continue; }

    bool model_wait = process_model(memory, entity->model);
    bool texture_wait = false;

    if (entity->texture) {
      texture_wait = process_texture(memory, entity->texture);
    }

    if (model_wait || texture_wait) { continue; }

    if (!is_sphere_in_frustum(&camera->frustum, entity->position, entity->model->radius * glm::compMax(entity->scale))) {
      continue;
    }

    mat4 model_view;

    model_view = glm::translate(model_view, entity->position);

    if (entity->flags & EntityFlags::LOOK_AT_CAMERA) {
      model_view *= make_billboard_matrix(entity->position, app->camera.position, vec3(app->camera.view_matrix[0][1], app->camera.view_matrix[1][1], app->camera.view_matrix[2][1]));
    }

    model_view = glm::rotate(model_view, entity->rotation.x, vec3(1.0, 0.0, 0.0));
    model_view = glm::rotate(model_view, entity->rotation.y, vec3(0.0, 1.0, 0.0));
    model_view = glm::rotate(model_view, entity->rotation.z, vec3(0.0, 0.0, 1.0));

    model_view = glm::scale(model_view, entity->scale);

    mat3 normal = glm::inverseTranspose(mat3(model_view));

    RenderCommand command;

    command.shader = &app->main_object_program;
    command.model_view = model_view;
    command.flags = entity->flags;
    command.normal = normal;
    command.color = entity->color;
    command.cull_type = GL_BACK;
    command.model_mesh = &entity->model->mesh;

    if (entity->type == EntityWater) {
      command.shader = &app->water_program;
    } else if (entity->type == EntityPlanet) {
      if (process_texture(memory, &app->planet_texture)) { continue; }

      command.texture = &app->planet_texture;
      command.shader = &app->textured_program;

      for (auto it = entity->positions.begin(); it != entity->positions.end(); it++) {
        mat4 child_view;
        child_view = glm::translate(child_view, entity->position);
        float radius = entity->scale.x;

#if 0
        float lat = glm::radians(180.0f - (90.0f + it->x));
        float lon = glm::radians(it->y);
#else
        float lat = glm::radians(it->x);
        float lon = glm::radians(it->y);

        /* float lat = it->x; */
        /* float lon = it->y; */
#endif

        float f = 0.0f;
        float ls = glm::pow(glm::atan((1.0f - f)), 2) * glm::tan(lat);

        float x = radius * glm::cos(ls) * glm::cos(lon) + glm::cos(lon);
        float y = radius * glm::cos(ls) * glm::sin(lon) + glm::sin(lon);
        float z = radius * glm::sin(ls);

        vec3 position = vec3(x, z, y);

        child_view = glm::translate(child_view, position);
        child_view = glm::scale(child_view, vec3(1.0f));
        mat3 normal = glm::inverseTranspose(mat3(child_view));

        RenderCommand child_command;
        child_command.model_view = child_view;
        child_command.flags = entity->flags;
        child_command.normal = normal;
        child_command.color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
        child_command.cull_type = GL_BACK;
        child_command.model_mesh = &entity->model->mesh;
        add_command_to_render_group(&app->render_group, child_command);
      }
    }

    bool transparent = command.texture != NULL || command.color.a < 1.0f; // TODO(sedivy): remove when there is way to tell if the object is transparent
    if (false) { // TODO(sedivy): replace with transparent variable
      command.distance_from_camera = glm::distance2(app->camera.position, entity->position);
      command.shader = &app->transparent_program;
      add_command_to_render_group(&app->transparent_render_group, command);
    } else {
      add_command_to_render_group(&app->render_group, command);
    }
  }

  end_render_group(app, &app->render_group);

  if (!shadow_pass) {
#if 0
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, app->shadow_depth_texture);

    glBindFramebuffer(GL_FRAMEBUFFER, app->transparent_buffer);

    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);

    glDrawBuffers(2, app->transparent_buffers);
    end_render_group(app, &app->transparent_render_group);

    glDepthMask(GL_TRUE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
  }
}

void render_terrain(Memory *memory, App *app) {
  PROFILE_BLOCK("Draw Terrain");
  use_program(app, &app->terrain_program);

  glCullFace(GL_BACK);

  set_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);

  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_2D, app->shadow_depth_texture);
  set_uniformi(app->current_program, "uShadow", 0);
  set_uniform(app->current_program, "shadow_matrix", app->shadow_camera.view_matrix);
  set_uniform(app->current_program, "texmapscale", vec2(1.0f / app->shadow_width, 1.0f / app->shadow_height));

  int x_coord = static_cast<int>(app->camera.position.x / CHUNK_SIZE_X);
  int y_coord = static_cast<int>(app->camera.position.z / CHUNK_SIZE_Y);

  {
    PROFILE_BLOCK("Terrain render", 17*17);
    for (int y=-8 + y_coord; y<=8 + y_coord; y++) {
      for (int x=-8 + x_coord; x<=8 + x_coord; x++) {
        if (x < 0 || y < 0 ) { continue; }

        TerrainChunk *chunk = get_chunk_at(app->chunk_cache, app->chunk_cache_count, x, y);

        int dx = chunk->x - x_coord;
        int dy = chunk->y - y_coord;

        float distance = glm::length2(vec2(dx, dy));

        int detail_level = 2;
        if (distance < 16.0f) { detail_level = 1; }
        if (distance < 4.0f) { detail_level = 0; }

        Model *model = chunk_get_model(memory, chunk, detail_level);

        if (model) {
          if (!is_sphere_in_frustum(&app->camera.frustum, vec3(chunk->x * CHUNK_SIZE_X, 0.0f, chunk->y * CHUNK_SIZE_Y), model->radius)) {
            continue;
          }

          render_terrain_chunk(app, chunk, model);
        }
      }
    }
  }
}

void tick(Memory *memory, Input input) {
  debug_global_memory = memory;
  platform = memory->platform;
  App *app = memory->app;

  {
    PROFILE_BLOCK("Tick");

    if (memory->should_reload) {
      memory->should_reload = false;

      glewExperimental = GL_TRUE;
      glewInit();
    }

    if (app->editing_mode) {
      draw_2d_debug_info(app, memory, input);
    }

    // NOTE(sedivy): update
    {
      PROFILE_BLOCK("Update");

      app->camera.size = vec2((float)memory->width, (float)memory->height);

      Entity *follow_entity = get_entity_by_id(app, app->camera_follow);
      {
        if (input.once.key_r) {
          setup_all_shaders(app);

          unload_texture(&app->gradient_texture);
          load_texture(&app->gradient_texture, STBI_rgb_alpha);
          initialize_texture(&app->gradient_texture, GL_RGBA, GL_RGBA, false);
          stbi_image_free(app->gradient_texture.data);
          app->gradient_texture.data = NULL;

          unload_texture(&app->color_correction_texture);
          load_texture(&app->color_correction_texture, STBI_rgb_alpha);
          initialize_texture(&app->color_correction_texture, GL_RGB8, GL_RGBA, false);
          stbi_image_free(app->color_correction_texture.data);
          app->color_correction_texture.data = NULL;

          unload_texture(&app->circle_texture);
          load_texture(&app->circle_texture, STBI_rgb_alpha);
          initialize_texture(&app->circle_texture, GL_RGBA, GL_RGBA, true);

          for (auto it = app->models.begin(); it != app->models.end(); it++) {
            if (it->second->path != NULL) { // NOTE(sedivy): If it has path it should be reloadable
              unload_model(it->second);
            }
          }

          {
            unload_texture(&app->debug_transparent_texture);
            load_texture(&app->debug_transparent_texture, STBI_rgb_alpha);
            initialize_texture(&app->debug_transparent_texture, GL_RGBA, GL_RGBA, true);
            stbi_image_free(app->debug_transparent_texture.data);
            app->debug_transparent_texture.data = NULL;
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

        vec3 rotation = follow_entity->rotation;

        vec3 forward;
        forward.x = glm::sin(rotation[1]) * glm::cos(rotation[0]);
        forward.y = -glm::sin(rotation[0]);
        forward.z = -glm::cos(rotation[1]) * glm::cos(rotation[0]);

        vec3 right = glm::normalize(glm::cross(forward, vec3(0.0, 1.0, 0.0)));

        vec3 movement;

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

        float speed = 1000.3f;

        if (input.alt && input.once.enter) {
          platform.toggle_fullscreen();
        }

        if (app->editing_mode) {
          speed = app->editor.speed;
        } else {
          if (input.shift) {
            speed = 10000.0f;
          }
          movement.y = 0;
        }

        if (glm::length(movement) > 0.0f) {
          movement = glm::normalize(movement);
        }

        follow_entity->velocity += movement * speed * input.delta_time;

        for (u32 i=0; i<app->entity_count; i++) {
          Entity *entity = app->entities + i;

          if (entity->type == EntityParticleEmitter) {
            Particle *particle = app->particles + app->next_particle++;
            if (app->next_particle >= array_count(app->particles)) {
              app->next_particle = 0;
            }

            particle->position = entity->position;
            particle->color = entity->color;
            particle->size = entity->scale.x;
            particle->velocity = vec3(get_random_float_between(-500.0f, 500.0f), get_random_float_between(0.0f, 1000.0f), get_random_float_between(-500.0f, 500.0f));
            particle->gravity = 500.0f;
          } else if (entity->type == EntityPlayer) {
            entity->position += entity->velocity * input.delta_time;
            entity->velocity = glm::mix(entity->velocity, vec3(0.0f), input.delta_time * 10.0f);
          }
        }

        for (u32 i=0; i<array_count(app->particles); i++) {
          Particle *particle = app->particles + i;

          particle->velocity.y += particle->gravity * input.delta_time;
          particle->position += particle->velocity * input.delta_time;
          particle->size = std::max(particle->size - 60.0f * input.delta_time, 0.0f);
          particle->velocity = glm::mix(particle->velocity, vec3(0.0f), input.delta_time);
          particle->color.a = glm::mix(particle->color.a, 0.0f, input.delta_time * 4.0f);

          particle->distance_from_camera = glm::distance2(app->camera.position, particle->position);
        }

        std::sort(app->particles, app->particles + array_count(app->particles), sort_particles_by_distance);

        for (u32 i=0; i<array_count(app->particles); i++) {
          Particle *particle = app->particles + i;

          app->particle_positions[4 * i + 0] = particle->position.x;
          app->particle_positions[4 * i + 1] = particle->position.y;
          app->particle_positions[4 * i + 2] = particle->position.z;
          app->particle_positions[4 * i + 3] = particle->size;

          app->particle_colors[4 * i + 0] = particle->color.x;
          app->particle_colors[4 * i + 1] = particle->color.y;
          app->particle_colors[4 * i + 2] = particle->color.z;
          app->particle_colors[4 * i + 3] = particle->color.w;
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
        app->camera.position = follow_entity->position + vec3(0.0f, 70.0f, 0.0f);
        app->camera.view_matrix = get_camera_projection(&app->camera);
        app->camera.view_matrix = glm::rotate(app->camera.view_matrix, app->camera.rotation.x, vec3(1.0, 0.0, 0.0));
        app->camera.view_matrix = glm::rotate(app->camera.view_matrix, app->camera.rotation.y, vec3(0.0, 1.0, 0.0));
        app->camera.view_matrix = glm::rotate(app->camera.view_matrix, app->camera.rotation.z, vec3(0.0, 0.0, 1.0));
        app->camera.view_matrix = glm::translate(app->camera.view_matrix, (app->camera.position * -1.0f));
        fill_frustum_with_matrix(&app->camera.frustum, app->camera.view_matrix);

        Ray ray = get_mouse_ray(app, input, memory);

        Entity *closest_entity = NULL;
        float closest_distance = FLT_MAX;
        vec3 hit_position;

        for (u32 i=0; i<app->entity_count; i++) {
          Entity *entity = app->entities + i;

          if (!(entity->flags & EntityFlags::HIDE_IN_EDITOR)) {
            RayMatchResult hit;
            hit.hit = false;

            if (entity->type == EntityBlock) {
              hit = ray_match_entity(app, ray, entity);
            } else {
              hit = ray_match_sphere(ray, entity->position, app->editor.handle_size);
            }

            if (hit.hit) {
              float distance = glm::distance(hit.hit_position, ray.start);
              if (distance < closest_distance) {
                closest_distance = distance;
                closest_entity = entity;
                hit_position = hit.hit_position;
              }
            }
          }
        }

        if (closest_entity) {
          app->editor.hover_entity = closest_entity->id;
          app->editor.hovering_entity = true;
        } else {
          app->editor.hovering_entity = false;
        }

        if (app->editing_mode && input.mouse_click) {
          if (closest_entity) {
            if (input.shift) {
              Entity *new_entity = app->entities + app->entity_count++;
              *new_entity = *closest_entity;
              new_entity->id = next_entity_id(app);
              closest_entity = new_entity;
            }

            app->editor.entity_id = closest_entity->id;
            app->editor.holding_entity = true;
            app->editor.distance_from_entity_offset = closest_distance;
            app->editor.hold_offset = hit_position - closest_entity->position;
            app->editor.inspect_entity = true;
          } else {
            app->editor.inspect_entity = false;
          }
        }


        {
          /* app->shadow_camera.position = vec3(20000.0, 4000.0, 20000.0); */
          app->shadow_camera.position = app->camera.position + vec3(0.0, 4000.0, 0.0);
          /* app->shadow_camera.rotation = vec3(1.229797, -1.170025, 0.0f); */
          app->shadow_camera.rotation = vec3(pi / 2.0f, 0.0f, 0.0f);
          app->shadow_camera.view_matrix = get_camera_projection(&app->shadow_camera);
          app->shadow_camera.view_matrix = glm::rotate(app->shadow_camera.view_matrix, app->shadow_camera.rotation.x, vec3(1.0, 0.0, 0.0));
          app->shadow_camera.view_matrix = glm::rotate(app->shadow_camera.view_matrix, app->shadow_camera.rotation.y, vec3(0.0, 1.0, 0.0));
          app->shadow_camera.view_matrix = glm::rotate(app->shadow_camera.view_matrix, app->shadow_camera.rotation.z, vec3(0.0, 0.0, 1.0));
          app->shadow_camera.view_matrix = glm::translate(app->shadow_camera.view_matrix, (app->shadow_camera.position * -1.0f));

          fill_frustum_with_matrix(&app->shadow_camera.frustum, app->shadow_camera.view_matrix);

#if 0
          vec3 forward;
          forward.x = glm::sin(app->shadow_camera.rotation[1]) * glm::cos(app->shadow_camera.rotation[0]);
          forward.y = -glm::sin(app->shadow_camera.rotation[0]);
          forward.z = -glm::cos(app->shadow_camera.rotation[1]) * glm::cos(app->shadow_camera.rotation[0]);
          forward = glm::normalize(forward);

          vec3 up = vec3(0.0, 1.0, 0.0);

          vec3 fc = app->shadow_camera.position + forward * app->shadow_camera.far;

          vec3 right = glm::normalize(glm::cross(forward, vec3(0.0, 1.0, 0.0)));

          vec3 ftl = fc + (up * app->shadow_camera.size.x/2.0f) - (right * app->shadow_camera.size.y * app->shadow_camera.far/2.0f);
          vec3 ftr = fc + (up * app->shadow_camera.size.x/2.0f) + (right * app->shadow_camera.size.y * app->shadow_camera.far/2.0f);
          vec3 fbl = fc - (up * app->shadow_camera.size.x/2.0f) - (right * app->shadow_camera.size.y * app->shadow_camera.far/2.0f);
          vec3 fbr = fc - (up * app->shadow_camera.size.x/2.0f) + (right * app->shadow_camera.size.y * app->shadow_camera.far/2.0f);

          vec3 nc = app->shadow_camera.position + forward * app->shadow_camera.near;

          vec3 ntl = nc + (up * app->shadow_camera.size.x/2.0f) - (right * app->shadow_camera.size.y * app->shadow_camera.near/2.0f);
          vec3 ntr = nc + (up * app->shadow_camera.size.x/2.0f) + (right * app->shadow_camera.size.y * app->shadow_camera.near/2.0f);
          vec3 nbl = nc - (up * app->shadow_camera.size.x/2.0f) - (right * app->shadow_camera.size.y * app->shadow_camera.near/2.0f);
          vec3 nbr = nc - (up * app->shadow_camera.size.x/2.0f) + (right * app->shadow_camera.size.y * app->shadow_camera.near/2.0f);

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

            if ((entity->flags & EntityFlags::MOUNT_TO_TERRAIN) != 0) {
              if (app->editor.experimental_terrain_entity_movement) {
                RayMatchResult hit = ray_match_terrain(app, ray);
                if (hit.hit) {
                  entity->position = hit.hit_position;
                }
              } else {
                entity->position = (ray.start + ray.direction * app->editor.distance_from_entity_offset) - app->editor.hold_offset;
              }
              mount_entity_to_terrain(entity);
            } else {
              entity->position = (ray.start + ray.direction * app->editor.distance_from_entity_offset) - app->editor.hold_offset;
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
    }

    // NOTE(sedivy): render
    {
      {
        PROFILE_BLOCK("Draw");

        {
          PROFILE_BLOCK("Draw Shadows");
          glBindFramebuffer(GL_FRAMEBUFFER, app->shadow_buffer);
          glViewport(0, 0, app->shadow_width, app->shadow_height);
          glClear(GL_DEPTH_BUFFER_BIT);

          glEnable(GL_DEPTH_TEST);

          render_scene(memory, app, &app->shadow_camera, &app->solid_program, true);

          glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, app->frames[0].id);
        glViewport(0, 0, app->frames[0].width, app->frames[0].height);
        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render_skybox(app);

        {
          PROFILE_BLOCK("Draw Main");

          render_terrain(memory, app);
          render_scene(memory, app, &app->camera);
          glBindFramebuffer(GL_FRAMEBUFFER, app->frames[0].id);

          // NOTE(sedivy): particles
          {
            PROFILE_BLOCK("Draw Particles");
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glDepthMask(GL_FALSE);

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);

            use_program(app, &app->particle_program);

            set_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);
            set_uniform(app->current_program, "camera_up", vec3(app->camera.view_matrix[0][1], app->camera.view_matrix[1][1], app->camera.view_matrix[2][1]));
            set_uniform(app->current_program, "camera_right", vec3(app->camera.view_matrix[0][0], app->camera.view_matrix[1][0], app->camera.view_matrix[2][0]));

            glBindBuffer(GL_ARRAY_BUFFER, app->particle_buffer);
            glBufferData(GL_ARRAY_BUFFER, array_count(app->particles) * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, array_count(app->particles) * 4 * sizeof(GLfloat), &app->particle_positions);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 4, GL_FLOAT, GL_FALSE, 0, 0);

            glBindBuffer(GL_ARRAY_BUFFER, app->particle_color_buffer);
            glBufferData(GL_ARRAY_BUFFER, array_count(app->particles) * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, array_count(app->particles) * 4 * sizeof(GLfloat), &app->particle_colors);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "color"), 4, GL_FLOAT, GL_FALSE, 0, 0);

            glBindBuffer(GL_ARRAY_BUFFER, app->particle_model);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "data"), 3, GL_FLOAT, GL_FALSE, 0, 0);

            glVertexAttribDivisor(0, 0);
            glVertexAttribDivisor(1, 1);
            glVertexAttribDivisor(2, 1);

            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, array_count(app->particles));

            glVertexAttribDivisor(0, 0);
            glVertexAttribDivisor(1, 0);
            glVertexAttribDivisor(2, 0);

            glDisable(GL_BLEND);
            glDisable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
          }

          {
            if (app->debug_lines.size() > 0) {
              PROFILE_BLOCK("Draw Debug Lines");
              use_program(app, &app->debug_program);
              glBindBuffer(GL_ARRAY_BUFFER, app->debug_buffer);
              glBufferData(GL_ARRAY_BUFFER, app->debug_lines.size() * sizeof(GLfloat)*3*2, NULL, GL_STREAM_DRAW);
              glBufferSubData(GL_ARRAY_BUFFER, 0, app->debug_lines.size() * sizeof(GLfloat)*3*2, &app->debug_lines[0]);
              set_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);
              glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
              glDrawArrays(GL_LINES, 0, app->debug_lines.size()*2);
              app->debug_lines.clear();
            }
          }

          if (app->editing_mode) {
            draw_3d_debug_info(input, app);
          }
        }

        {
          PROFILE_BLOCK("Draw Final");
          glViewport(0, 0, app->frames[0].width, app->frames[0].height);
          glDisable(GL_DEPTH_TEST);
          glDisable(GL_CULL_FACE);

          app->read_frame = 0;
          app->write_frame = 1;

          glActiveTexture(GL_TEXTURE0 + 0);
          glBindTexture(GL_TEXTURE_2D, app->frames[app->read_frame].texture);

          glActiveTexture(GL_TEXTURE0 + 1);
          glBindTexture(GL_TEXTURE_2D, app->frames[app->write_frame].texture);

          {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_program);

            glActiveTexture(GL_TEXTURE0 + 2);
            glBindTexture(GL_TEXTURE_2D, app->color_correction_texture.id);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          if (0)
          {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, app->frames[app->write_frame].id);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, app->transparent_buffer);
            glReadBuffer(GL_COLOR_ATTACHMENT0);

            use_program(app, &app->fullscreen_merge_alpha);

            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);

            glActiveTexture(GL_TEXTURE0 + 3);
            glBindTexture(GL_TEXTURE_2D, app->transparent_color_texture);
            set_uniformi(app->current_program, "color_texture", 3);

            glActiveTexture(GL_TEXTURE0 + 4);
            glBindTexture(GL_TEXTURE_2D, app->transparent_alpha_texture);
            set_uniformi(app->current_program, "transparent_color_texture", 4);

            glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glDisable(GL_BLEND);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

            std::swap(app->write_frame, app->read_frame);
          }

          if (app->color_correction) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_color_program);

            glActiveTexture(GL_TEXTURE0 + 2);
            glBindTexture(GL_TEXTURE_2D, app->color_correction_texture.id);

            set_uniformi(app->current_program, "uSampler", app->read_frame);
            set_uniformi(app->current_program, "color_correction_texture", 2);
            set_uniformf(app->current_program, "lut_size", 16.0f);

            glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          if (app->antialiasing) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_fxaa_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);
            set_uniform(app->current_program, "texture_size", vec2(app->frames[0].width, app->frames[0].height));

            glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          if (app->bloom) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_bloom_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            std::swap(app->write_frame, app->read_frame);
          }

          if (app->hdr) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_hdr_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            std::swap(app->write_frame, app->read_frame);
          }

          if (app->lens_flare) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_lens_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            std::swap(app->write_frame, app->read_frame);
          }

          {
            glViewport(0, 0, memory->width, memory->height);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            use_program(app, &app->fullscreen_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
          }
        }
      }

      if (app->editing_mode) {
        flush_2d_render(app, memory);
      }
    }
  }

  memcpy(global_last_frame_counters, global_counters, sizeof(global_counters));
  for (u32 i=0; i<array_count(global_counters); i++) {
    DebugCounter *counter = global_counters + i;

    if (counter->hit_count) {
      counter->cycle_count = 0;
      counter->hit_count = 0;
    }
  }
  global_debug_counter = 0;

  u64 diff = platform.get_time();
  if (diff >= app->frametimelast + 1000) {
    app->fps = app->framecount;
    app->framecount = 0;
    app->frametimelast = diff;
  } else {
    app->framecount += 1;
  }
}
