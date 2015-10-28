#pragma once

struct Texture {
  const char *path = NULL;
  const char *short_name = NULL;

  GLuint id = 0;
  u8 *data = NULL;

  u32 width = 0;
  u32 height = 0;

  u32 state = AssetState::EMPTY;
};

