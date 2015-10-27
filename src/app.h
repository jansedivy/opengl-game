#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <algorithm>

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
#include <glm/gtx/norm.hpp>
#include <glm/gtc/noise.hpp>

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4;
using glm::mat3;
using glm::quat;

#include <vcacheopt.h>

#include <cstdio>

#include <vector>
#include <unordered_map>

#include "perlin.h"

#include "stb_truetype.cpp"
#include "stb_image_write.cpp"
#include "stb_image.cpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct Memory *debug_global_memory;
PlatformAPI platform;

#include "scope_exit.h"

#include "font.h"
#include "array.h"
#include "random.h"

#include "debug.h"

#define CHUNK_SIZE_X 5000
#define CHUNK_SIZE_Y 5000

static float tau = glm::pi<float>() * 2.0f;
static float pi = glm::pi<float>();

struct Shader {
  GLuint id;
  std::unordered_map<std::string, GLuint> uniforms;
  std::unordered_map<std::string, GLuint> attributes;
  bool initialized = false;
};

struct Box {
  vec3 min;
  vec3 max;
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

namespace AssetState {
  enum AssetState {
    EMPTY,
    INITIALIZED,
    HAS_DATA,
    PROCESSING
  };
}

struct Model {
  const char *path = NULL;
  const char *id_name;

  Mesh mesh;

  float radius = 0.0f;

  u32 state = AssetState::EMPTY;
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

  TerrainChunk *prev;
  TerrainChunk *next;
};

struct Texture {
  const char *path = NULL;
  const char *short_name = NULL;

  GLuint id = 0;
  u8 *data = NULL;

  u32 width = 0;
  u32 height = 0;

  u32 state = AssetState::EMPTY;
};

struct RayMatchResult {
  bool hit;
  vec3 hit_position;
};

#include "entity.h"

struct Ray {
  vec3 start;
  vec3 direction;
};

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

struct Camera {
  mat4 view_matrix;

  vec3 rotation;
  vec3 position;
  Frustum frustum;

  bool ortho = false;
  vec2 size;

  float far;
  float near;
};

#include "render_group.h"

struct EditorHandleRenderCommand {
  float distance_from_camera;
  mat4 model_view;
  vec4 color;
};

namespace EditorLeftState {
  enum EditorLeftState {
    MODELING,
    TERRAIN,
    EDITOR_SETTINGS,
    POST_PROCESSING,
    LIGHT,
  };
}

namespace EditorRightState {
  enum EditorRightState {
    COMMON,
    SPECIFIC
  };
}

#include "ui.h"

struct Editor {
  bool holding_entity = false;
  bool hovering_entity = false;
  bool inspect_entity = false;
  u32 entity_id;
  u32 hover_entity;
  float distance_from_entity_offset;
  vec3 hold_offset;
  float handle_size;

  bool show_left = true;
  bool show_right = true;

  bool show_handles = true;
  bool show_performance = false;

  bool experimental_terrain_entity_movement = false;

  float speed;

  EditorLeftState::EditorLeftState left_state;
  EditorRightState::EditorRightState right_state;

  UICommandBuffer command_buffer;
};

struct FrameBuffer {
  GLuint id;
  GLuint texture;
  GLuint depth;
  u32 width;
  u32 height;
};

#pragma pack(1)
struct LoadedLevelHeader {
  u32 entity_count;
};

struct EntitySave {
  u32 id;
  EntityType::EntityType type;
  vec3 position;
  vec3 scale;
  vec3 rotation;
  vec4 color;
  u32 flags;

  bool has_model;
  char model_name[128];
};
#pragma options align=reset

struct LoadedLevel {
  std::vector<EntitySave> entities;
};

struct Particle {
  vec3 position;
  vec4 color;
  vec3 velocity;

  float size;
  float gravity;

  float distance_from_camera;
};

struct App {
  u32 last_id;

  Shader main_object_program;
  Shader transparent_program;
  Shader water_program;
  Shader debug_program;
  Shader ui_program;
  Shader solid_program;
  Shader record_depth_program;
  Shader phong_program;
  Shader fullscreen_program;
  Shader fullscreen_merge_alpha;
  Shader fullscreen_fog_program;
  Shader fullscreen_color_program;
  Shader fullscreen_fxaa_program;
  Shader fullscreen_bloom_program;
  Shader fullscreen_hdr_program;
  Shader fullscreen_SSAO_program;
  Shader fullscreen_depth_program;
  Shader fullscreen_lens_program;
  Shader terrain_program;
  Shader particle_program;
  Shader skybox_program;
  Shader textured_program;
  Shader grass_program;
  Shader controls_program;

  Shader *current_program;

  GLuint fullscreen_quad;

  GLuint vao;

  GLuint debug_buffer;
  GLuint debug_index_buffer;
  std::vector<vec3> debug_lines;

  u32 read_frame;
  u32 write_frame;
  FrameBuffer frames[2];

  GLuint shadow_buffer;
  GLuint shadow_depth_texture;
  u32 shadow_width;
  u32 shadow_height;

  Texture gradient_texture;
  Texture color_correction_texture;
  CubeMap cubemap;

  std::unordered_map<std::string, Texture*> textures;

  Font font;
  Font mono_font;

  Model cube_model;
  Model sphere_model;
  Model quad_model;

  std::unordered_map<std::string, Model*> models;

  Entity *entities;
  u32 entity_count = 0;

  Camera shadow_camera;

  Camera camera;
  u32 camera_follow;

  GLuint *last_shader;

  TerrainChunk *chunk_cache;
  u32 chunk_cache_count;

  RenderGroup render_group;
  RenderGroup transparent_render_group;

  bool editing_mode = false;

  Editor editor;

  u32 fps;
  u32 framecount;
  u32 frametimelast;

  float time;

  bool antialiasing;
  bool color_correction;
  bool bloom;
  bool hdr;
  bool lens_flare;

  Particle particles[4096];
  u32 next_particle;

  vec4 particle_positions[4096];
  vec4 particle_colors[4096];

  GLuint particle_buffer;
  GLuint particle_color_buffer;

  GLuint particle_model;
  Memory *memory;
};
