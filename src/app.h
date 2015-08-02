#pragma once


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define array_count(arr) (sizeof(arr) / sizeof(arr[0]))

#include "platform.h"

#define GLEW_STATIC
#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>

#include <vector>
#include <unordered_map>

#include "array.h"

#include "random.h"
#include "perlin.cpp"

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include "stb_truetype.h"
#include "stb_image_write.h"
#include "stb_image.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "font.h"

#define CHUNK_SIZE_X 5000
#define CHUNK_SIZE_Y 5000

PlatformAPI platform;

class Shader {
  public:
    GLuint id;
    std::unordered_map<std::string, u32> uniforms;
    std::unordered_map<std::string, GLuint> attributes;
    bool initialized = false;
};

struct Box {
  glm::vec3 min;
  glm::vec3 max;
};

struct ModelData {
  void *data = NULL;

  float *vertices = NULL;
  u32 vertices_count = 0;

  float *normals = NULL;
  u32 normals_count = 0;

  float *uv = NULL;
  u32 uv_count = 0;

  int *indices = NULL;
  u32 indices_count = 0;
};

struct Mesh {
  GLuint vertices_id;
  GLuint indices_id;
  GLuint normals_id;
  GLuint uv_id;

  ModelData data;
};

struct Model {
  const char *path = NULL;

  Mesh mesh;
  u32 mesh_count = 0;

  float radius = 300.0f;

  bool is_being_loaded = false;
  bool has_data = false;
  bool initialized = false;
};

#include "render_group.h"

struct CubeMap {
  GLuint id;
  Model *model;
};

struct TerrainChunk {
  u32 x;
  u32 y;

  float min;
  float max;

  Model model;
  Model model_mid;
  Model model_low;

  bool initialized;

  bool has_data;
  bool is_being_loaded;

  TerrainChunk *prev;
  TerrainChunk *next;
};

enum EntityType {
  EntityPlayer,
  EntityBlock,

  EntityCount
};

enum RenderFlags {
  RenderHidden = 1,
  RenderWireframe = 2
};

struct Texture {
  const char *path = NULL;
  GLuint id = 0;
  u8 *data = NULL;

  u32 width = 0;
  u32 height = 0;

  bool initialized = false;
  bool is_being_loaded = false;
  bool has_data = false;
};

struct Entity {
  u32 id = 0;

  u32 render_flags = 0;

  Model *model = NULL;

  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 velocity;
  glm::vec3 scale;
  glm::vec3 color;

  EntityType type;
  Texture *texture = 0;
};

struct Ray {
  glm::vec3 start;
  glm::vec3 direction;
};

struct Plane {
  glm::vec3 normal;
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

struct Camera {
  glm::mat4 view_matrix;

  glm::vec3 rotation;
  glm::vec3 position;
  Frustum frustum;

  bool ortho = false;

  float far;
  float near;

  float aspect_ratio;
};

struct App {
  u32 last_id;

  Shader program;
  Shader another_program;
  Shader debug_program;
  Shader solid_program;
  Shader fullscreen_program;
  Shader terrain_program;
  Shader skybox_program;
  Shader textured_program;

  Shader *current_program;

  GLuint font_quad;
  GLuint fullscreen_quad;

  GLuint vao;

  GLuint debug_buffer;
  std::vector<glm::vec3> debug_lines;

  GLuint frame_buffer;
  GLuint frame_texture;
  GLuint frame_depth_texture;
  u32 frame_width;
  u32 frame_height;

  Texture grass_texture;
  Texture gradient_texture;

  Font font;

  Model cube_model;
  Model sphere_model;
  Model rock_model;
  Model grass_model;

  Model trees[2];

  Entity entities[10000];
  u32 entity_count = 0;

  Camera camera;
  u32 camera_follow;

  GLuint *last_shader;

  TerrainChunk *chunk_cache;
  u32 chunk_cache_count;

  CubeMap cubemap;

  RenderGroup render_group;

  bool editing_mode = false;
};

Memory *debug_global_memory;

#define PROFILE(ID) u64 profile_cycle_count##ID = platform.get_performance_counter();
#define PROFILE_END(ID) debug_global_memory->counters[DebugCycleCounter_##ID].cycle_count += platform.get_performance_counter() - profile_cycle_count##ID; ++debug_global_memory->counters[DebugCycleCounter_##ID].hit_count;
#define PROFILE_END_COUNTED(ID, COUNT) debug_global_memory->counters[DebugCycleCounter_##ID].cycle_count += platform.get_performance_counter() - profile_cycle_count##ID; debug_global_memory->counters[DebugCycleCounter_##ID].hit_count += (COUNT);
