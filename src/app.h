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

#include <vcacheopt.h>

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

static float tau = glm::pi<float>() * 2.0f;
static float pi = glm::pi<float>();

struct Shader {
  GLuint id;
  std::unordered_map<std::string, GLuint> uniforms;
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

namespace ModelDataState {
  enum ModelDataState {
    EMPTY,
    INITIALIZED,
    HAS_DATA,
    PROCESSING,
    REGISTERED_TO_LOAD
  };
}

struct Model {
  const char *path = NULL;
  const char *id_name;

  Mesh mesh;

  float radius = 0.0f;

  u32 state = ModelDataState::EMPTY;
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

enum EntityType {
  EntityPlayer = 0,
  EntityBlock = 1,
  EntityParticleEmitter = 2,
  EntityPlanet = 3,
  EntityWater = 4
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
    RENDER_IGNORE_DEPTH = (1 << 5),
    HIDE_IN_EDITOR = (1 << 6),
    LOOK_AT_CAMERA = (1 << 7)
  };
};

struct RayMatchResult {
  bool hit;
  glm::vec3 hit_position;
};

struct Entity {
  u32 id = 0;

  u32 flags = 0;

  Model *model = NULL;

  glm::vec3 position;

  glm::vec3 rotation;

  glm::vec3 velocity;
  glm::vec3 scale;
  glm::vec4 color;

  EntityType type;
  Texture *texture = 0;

  // TODO(sedivy): Move planet specific stuff into union
  std::vector<glm::vec2> positions;
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
  glm::vec2 size;

  float far;
  float near;
};

#include "render_group.h"

struct EditorHandleRenderCommand {
  float distance_from_camera;
  glm::mat4 model_view;
  glm::vec4 color;
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

namespace EditorLeftState {
  enum EditorLeftState {
    MODELING,
    TERRAIN,
    EDITOR_SETTINGS,
    POST_PROCESSING,
    LIGHT,
  };
}

struct Editor {
  bool holding_entity = false;
  bool hovering_entity = false;
  bool inspect_entity = false;
  u32 entity_id;
  u32 hover_entity;
  float distance_from_entity_offset;
  glm::vec3 hold_offset;
  float handle_size;

  bool show_left = true;
  bool show_right = true;

  bool show_handles = true;
  bool show_performance = false;
  bool show_state_changes = false;

  float speed;

  EditorLeftState::EditorLeftState left_state;

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
  EntityType type;
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

struct Particle {
  glm::vec3 position;
  glm::vec4 color;
  glm::vec3 velocity;

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
  Shader controls_program;

  Shader *current_program;

  GLuint fullscreen_quad;

  GLuint vao;

  GLuint debug_buffer;
  std::vector<glm::vec3> debug_lines;

  u32 read_frame;
  u32 write_frame;
  FrameBuffer frames[2];

  GLuint transparent_buffer;
  GLuint transparent_color_texture;
  GLuint transparent_alpha_texture;
  GLenum transparent_buffers[2];

  GLuint shadow_buffer;
  GLuint shadow_depth_texture;
  u32 shadow_width;
  u32 shadow_height;

  Texture gradient_texture;
  Texture color_correction_texture;
  Texture planet_texture;
  Texture circle_texture;
  Texture debug_texture;
  Texture debug_transparent_texture;
  Texture editor_texture;
  CubeMap cubemap;

  Font font;

  Model cube_model;
  Model sphere_model;
  Model quad_model;

  std::unordered_map<std::string, Model*> models;

  Entity entities[10000];
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

  bool antialiasing;
  bool color_correction;
  bool bloom;
  bool hdr;
  bool lens_flare;

  Particle particles[4096];
  u32 next_particle;

  GLfloat particle_positions[4096 * 4];
  GLfloat particle_colors[4096 * 4];

  GLuint particle_buffer;
  GLuint particle_color_buffer;

  GLuint particle_model;
};

Memory *debug_global_memory;

#include "debug.h"
