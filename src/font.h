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
  font.size = font_size;

  u8 *alpha = static_cast<u8 *>(malloc(font.width * font.height));

  stbtt_PackBegin(&font.pc, alpha, font.width, font.height, 0, 1, NULL);
    stbtt_PackSetOversampling(&font.pc, 2, 2);
    stbtt_PackFontRange(&font.pc, static_cast<u8 *>(font_data), 0, font_size, 32, 95, font.chardata[0] + 32);
  stbtt_PackEnd(&font.pc);

  glGenTextures(1, &font.texture);
  glBindTexture(GL_TEXTURE_2D, font.texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font.width, font.height, 0, GL_RED, GL_UNSIGNED_BYTE, alpha);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  free(alpha);

  return font;
}

void font_get_quad(Font *font, char text, float *x, float *y, stbtt_aligned_quad *q) {
  stbtt_GetPackedQuad(font->chardata[0], font->width, font->width, text, x, y, q, 1);
}
