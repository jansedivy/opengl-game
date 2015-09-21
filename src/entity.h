#pragma once

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
    EntityWater = 4
  };
}

struct EntityHeader {
  u32 id = 0;
  u32 flags = 0;
  EntityType::EntityType type;
  vec3 position;

  Model *model = NULL;
  Texture *texture = 0;

  vec3 rotation;
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

struct EntityWater {
  EntityHeader header;
};

struct EntityBlock {
  EntityHeader header;
};

struct EntityPlayer {
  EntityHeader header;
};

union Entity {
  EntityHeader header;

  EntityPlayer player;
  EntityBlock block;
  EntityWater water;
  EntityParticleEmitter particle_emitter;
};

