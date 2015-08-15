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
#include <glm/gtx/intersect.hpp>

#include <cstdio>

#include <vector>
#include <unordered_map>

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
#include "array.h"
#include "random.h"
#include <cstdio>


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
  GLuint indices_id;

  GLuint vertices_id;
  GLuint normals_id;
  GLuint uv_id;

  ModelData data;
};

struct Model {
  const char *path = NULL;
  const char *id_name;

  Mesh mesh;

  float radius = 0.0f;

  bool is_being_loaded = false;
  bool has_data = false;
  bool initialized = false;
};

struct CubeMap {
  GLuint id;
  Model *model;
};

struct TerrainChunk {
  u32 x;
  u32 y;

  Model models[3];

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

namespace EntityFlags {
  enum EntityFlags {
    PERMANENT_FLAG = (1 << 0),
    MOUNT_TO_TERRAIN = (1 << 1),
    CASTS_SHADOW = (1 << 2),

    RENDER_HIDDEN = (1 << 3),
    RENDER_WIREFRAME = (1 << 4),
    RENDER_IGNORE_DEPTH = (1 << 5)
  };
};

struct RayMatchResult {
  bool hit;
  glm::vec3 hit_position;
  float distance;
};

struct Entity {
  u32 id = 0;

  u32 render_flags = 0;
  u32 flags = 0;

  Model *model = NULL;

  glm::vec3 position;

  glm::vec3 rotation;

  glm::vec3 velocity;
  glm::vec3 scale;
  glm::vec4 color;

  EntityType type;
  Texture *texture = 0;
};

#include "render_group.h"

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
  glm::vec2 size;

  float far;
  float near;
};

struct EditorHandleRenderCommand {
  float distance_from_camera;
  glm::mat4 model_view;
};

namespace UICommandType {
  enum UICommandType {
    NONE = 0,
    RECT,
    TEXT
  };
}

struct UICommand {
  glm::vec4 color;
  glm::vec4 image_color;
  u32 vertices_count;
  UICommandType::UICommandType type;
};

struct UICommandBuffer {
  std::vector<GLfloat> vertices;
  std::vector<UICommand> commands;
};

struct Editor {
  bool holding_entity;
  bool inspect_entity;
  u32 entity_id;
  float distance_from_entity_offset;
  glm::vec3 hold_offset;
  float handle_size;

  bool show_left = true;
  bool show_right = true;

  bool show_performance = false;
  bool show_state_changes = false;

  UICommandBuffer command_buffer;
};

struct FrameBuffer {
  GLuint id;
  GLuint texture;
  GLuint depth;
  u32 width;
  u32 height;
};

struct DebugDrawState {
  float offset_top = 0;
};

#pragma pack(1)
struct LoadedLevelHeader {
  u32 entity_count;
};

struct EntitySave {
  u32 id;
  glm::vec3 position;
  glm::vec3 scale;
  glm::vec3 rotation;
  glm::vec4 color;
  u32 flags;
  char model_name[128];
};
#pragma options align=reset

struct LoadedLevel {
  std::vector<EntitySave> entities;
};

struct App {
  u32 last_id;

  Shader program;
  Shader another_program;
  Shader debug_program;
  Shader ui_program;
  Shader solid_program;
  Shader fullscreen_program;
  Shader fullscreen_fog_program;
  Shader fullscreen_color_program;
  Shader fullscreen_fxaa_program;
  Shader fullscreen_bloom_program;
  Shader fullscreen_depth_program;
  Shader terrain_program;
  Shader skybox_program;
  Shader textured_program;

  Shader *current_program;

  GLuint font_quad;
  GLuint fullscreen_quad;

  GLuint vao;

  GLuint debug_buffer;
  std::vector<glm::vec3> debug_lines;

  FrameBuffer frame_1;
  FrameBuffer frame_2;

  GLuint shadow_buffer;
  GLuint shadow_depth_texture;
  u32 shadow_width;
  u32 shadow_height;

  Texture gradient_texture;
  Texture color_correction_texture;
  Texture circle_texture;
  Texture editor_texture;

  Font font;

  Model cube_model;
  Model sphere_model;
  Model rock_model;
  Model quad_model;

  Model trees[2];

  Entity entities[10000];
  u32 entity_count = 0;

  Camera shadow_camera;

  Camera camera;
  u32 camera_follow;

  GLuint *last_shader;

  TerrainChunk *chunk_cache;
  u32 chunk_cache_count;

  CubeMap cubemap;

  RenderGroup render_group;

  bool editing_mode = false;

  Editor editor;

  u32 fps;
  u32 framecount;
  u32 frametimelast;
};

Memory *debug_global_memory;

#define PROFILE(ID) u64 profile_cycle_count##ID = platform.get_performance_counter();
#define PROFILE_END(ID) debug_global_memory->counters[DebugCycleCounter_##ID].cycle_count += (platform.get_performance_counter()) - profile_cycle_count##ID; ++debug_global_memory->counters[DebugCycleCounter_##ID].hit_count;
#define PROFILE_END_COUNTED(ID, COUNT) debug_global_memory->counters[DebugCycleCounter_##ID].cycle_count += platform.get_performance_counter() - profile_cycle_count##ID; debug_global_memory->counters[DebugCycleCounter_##ID].hit_count += (COUNT);
