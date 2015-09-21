#pragma once

#include "app.h"

struct Font {
  stbtt_pack_context pc;
  stbtt_packedchar chardata[3][128];

  GLuint texture;
  int width;
  int height;

  float size;
};

Font create_font(void *font_data, float font_size) {
  Font font;
  font.width = 512;
  font.height = 512;
  font.size = STBTT_POINT_SIZE(font_size);

  u8 *alpha_buffer = static_cast<u8 *>(malloc(font.width * font.height));
  u8 *color_buffer = static_cast<u8 *>(malloc(font.width * font.height * 4));

  stbtt_PackBegin(&font.pc, alpha_buffer, font.width, font.height, 0, 1, NULL);
    stbtt_PackSetOversampling(&font.pc, 1, 1);
    stbtt_PackFontRange(&font.pc, static_cast<u8 *>(font_data), 0, font.size, 32, 95, font.chardata[0] + 32);
  stbtt_PackEnd(&font.pc);

  u32 alpha_index = 0;

  for (s32 i=0; i<(font.width * font.height); i += 4) {
    u8 alpha = alpha_buffer[alpha_index];

    color_buffer[i] = 255;
    color_buffer[i+1] = 255;
    color_buffer[i+2] = 255;
    color_buffer[i+3] = alpha;

    alpha_index += 1;
  }

  glGenTextures(1, &font.texture);
  glBindTexture(GL_TEXTURE_2D, font.texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, font.width, font.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, color_buffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  free(alpha_buffer);
  free(color_buffer);

  return font;
}

void font_get_quad(Font *font, char text, float *x, float *y, stbtt_aligned_quad *q) {
  stbtt_GetPackedQuad(font->chardata[0], font->width, font->width, text, x, y, q, 1);
}
