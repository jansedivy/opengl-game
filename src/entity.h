#pragma once

#define MAX_GRASS_GROUP_COUNT 500

typedef u32 Pid;

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

namespace EntityType {
  enum EntityType {
    EntityPlayer = 0,
    EntityBlock = 1,
    EntityParticleEmitter = 2,
    EntityGrass = 3,
    EntityWater = 4
  };
}

struct WorldPosition {
  u32 chunk_x;
  u32 chunk_y;

  vec3 offset_;
};

struct EntityHeader {
  Pid id;
  u32 flags = 0;
  EntityType::EntityType type;

  WorldPosition position;

  Model *model = NULL;
  Texture *texture = 0;

  quat orientation;
  vec3 velocity;
  vec3 scale;
  vec4 color;
};

struct EntityParticleEmitter {
  EntityHeader header;

  vec4 initial_color = vec4(1.0);
  float particle_size = 10.0f;
  float gravity = 500.0f;
};

struct EntityGrass {
  EntityHeader header;
  Texture *texture;
  float min_radius;
  float max_radius;

  float min_scale;
  float max_scale;

  u32 grass_count;
  Model *grass_model;

  void *data;
  vec4 *positions;
  vec3 *rotations;
  vec3 *tints;

  GLuint position_data_id;

  bool initialized;
  bool reload_data;
  bool render;
};

// TODO(sedivy): don't put different types in the same struct
struct Entity {
  EntityHeader header;

  EntityParticleEmitter particle_emitter;
  EntityGrass grass;
};
