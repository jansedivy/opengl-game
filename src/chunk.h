#pragma once

#define CHUNK_SIZE_X 50
#define CHUNK_SIZE_Y 50

struct TerrainChunk {
  u32 x;
  u32 y;

  Model models[3];

  bool initialized;

  TerrainChunk *prev;
  TerrainChunk *next;
};

