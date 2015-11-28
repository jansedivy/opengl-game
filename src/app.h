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
#include <base/array.h>
#include "random.h"

#include "debug.h"

static float tau = glm::pi<float>() * 2.0f;
static float pi = glm::pi<float>();

#include "shader.h"

struct Box {
  vec3 min;
  vec3 max;
};

#include "assets.h"
#include "model.h"

#include "texture.h"
#include "chunk.h"
#include "raytrace.h"
#include "entity.h"
#include "plane.h"
#include "camera.h"

#include "render_group.h"
#include "level.h"

#include "ui.h"
#include "editor.h"

struct FrameBuffer {
  GLuint id;
  GLuint texture;
  GLuint depth;
  u32 width;
  u32 height;
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
  Array<vec3> debug_lines;

  u32 read_frame;
  u32 write_frame;
  FrameBuffer frames[2];

  GLuint shadow_buffer;
  GLuint shadow_depth_texture;
  u32 shadow_width;
  u32 shadow_height;

  Texture gradient_texture;
  Texture color_correction_texture;
  Texture cubemap;

  std::unordered_map<std::string, Texture*> textures;

  Font font;
  Font mono_font;

  Model cube_model;
  Model sphere_model;
  Model quad_model;

  std::unordered_map<std::string, Model*> models;

  Array<Entity> entities;

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
