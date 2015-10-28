#pragma once

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

struct Model {
  const char *path = NULL;
  const char *id_name;

  Mesh mesh;

  float radius = 0.0f;

  u32 state = AssetState::EMPTY;
};

struct LoadModelWork {
  Model *model;
};
