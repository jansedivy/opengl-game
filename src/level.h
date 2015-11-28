#pragma once

#pragma pack(1)
struct LoadedLevelHeader {
  u32 entity_count;
};

struct EntitySave {
  u32 id;
  EntityType::EntityType type;
  vec3 position;
  vec3 scale;
  quat rotation;
  vec4 color;
  u32 flags;

  bool has_model;
  char model_name[128];
};
#pragma options align=reset

struct LoadedLevel {
  Array<EntitySave> entities;
};
