#pragma once

struct ModelData {
  void *data = NULL;
  float *vertices = NULL;
  float *normals = NULL;
  float *uv = NULL;
  int *indices = NULL;
  float *colors = NULL;

  u32 vertices_count = 0;
  u32 normals_count = 0;
  u32 uv_count = 0;
  u32 indices_count = 0;
  u32 colors_count = 0;
};

struct Mesh {
  GLuint buffer;
  GLuint indices_id;

  ModelData data;
};

struct Model {
  const char *path;
  const char *id_name;

  Mesh mesh;

  float radius;

  u32 state;
};

struct LoadModelWork {
  Model *model;
};
