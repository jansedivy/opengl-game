void unload_model(Model *model) {
  if (platform.atomic_exchange(&model->state, AssetState::INITIALIZED, AssetState::PROCESSING)) {
    glDeleteBuffers(1, &model->mesh.buffer);

    free(model->mesh.data.data);

    model->state = AssetState::EMPTY;
  }
}

float calculate_radius(Mesh *mesh) {
  float max_radius = 0.0f;

  for (u32 i=0; i<mesh->data.vertices_count; i += 3) {
    float radius = glm::length2(vec3(mesh->data.vertices[i], mesh->data.vertices[i + 1], mesh->data.vertices[i + 2]));
    if (radius > max_radius) {
      max_radius = radius;
    }
  }

  return glm::sqrt(max_radius);
}

void optimize_model(Model *model) {
  VertexCacheOptimizer vco;
  vco.Optimize(model->mesh.data.indices, model->mesh.data.indices_count / 3); // TODO(sedivy): why divide by three
}

void initialize_model(Model *model) {
  u32 vertices_size = model->mesh.data.vertices_count * sizeof(float);
  u32 normals_size = model->mesh.data.normals_count * sizeof(float);
  u32 uv_size = model->mesh.data.uv_count * sizeof(float);
  u32 colors_size = model->mesh.data.colors_count + sizeof(float);

  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, vertices_size + normals_size + uv_size + colors_size, NULL, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices_size, model->mesh.data.vertices);
  glBufferSubData(GL_ARRAY_BUFFER, vertices_size, normals_size, model->mesh.data.normals);
  glBufferSubData(GL_ARRAY_BUFFER, vertices_size + normals_size, uv_size, model->mesh.data.uv);
  glBufferSubData(GL_ARRAY_BUFFER, vertices_size + normals_size + uv_size, colors_size, model->mesh.data.colors);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  GLuint indices_id;
  glGenBuffers(1, &indices_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->mesh.data.indices_count * sizeof(GLint), model->mesh.data.indices, GL_STATIC_DRAW);

  model->mesh.buffer = buffer;
  model->mesh.indices_id = indices_id;

  model->state = AssetState::INITIALIZED; // TODO(sedivy): atomic
}

void allocate_mesh(Mesh *mesh, u32 vertices_count, u32 normals_count, u32 indices_count, u32 uv_count, u32 colors_count) {
  u32 vertices_size = vertices_count * sizeof(float);
  u32 normals_size = normals_count * sizeof(float);
  u32 indices_size = indices_count * sizeof(GLint);
  u32 uv_size = uv_count * sizeof(float);
  u32 colors_size = colors_count * sizeof(float);

  u8 *data = (u8 *)malloc(vertices_size + normals_size + indices_size + uv_size + colors_size);

  float *vertices = (float*)data;
  float *normals = (float*)(data + vertices_size);
  int *indices = (int*)(data + vertices_size + normals_size);
  float *uv = (float*)(data + vertices_size + normals_size + indices_size);
  float *colors = (float*)(data + vertices_size + normals_size + indices_size + uv_size);

  mesh->data.data = data;

  mesh->data.vertices = vertices;
  mesh->data.vertices_count = vertices_count;

  mesh->data.normals = normals;
  mesh->data.normals_count = normals_count;

  mesh->data.indices = indices;
  mesh->data.indices_count = indices_count;

  mesh->data.uv = uv;
  mesh->data.uv_count = uv_count;

  mesh->data.colors = colors;
  mesh->data.colors_count = colors_count;
}

void load_model_work(void *data) {
  PROFILE_BLOCK("Loading Model");
  LoadModelWork *work = (LoadModelWork *)data;

  if (platform.atomic_exchange(&work->model->state, AssetState::EMPTY, AssetState::PROCESSING)) {
    acquire_asset_file((char *)work->model->path);

    DebugReadFileResult result = platform.debug_read_entire_file(work->model->path);

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFileFromMemory(result.contents, result.fileSize, aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeGraph |
        aiProcess_SortByPType);

    Model *model = work->model;

    float max_distance = 0.0f;

    if (scene->HasMeshes()) {
      u32 indices_offset = 0;

      u32 count = 0;
      u32 index_count = 0;

      for (u32 i=0; i<scene->mNumMeshes; i++) {
        aiMesh *mesh_data = scene->mMeshes[i];
        count += mesh_data->mNumVertices;

        for (u32 l=0; l<mesh_data->mNumFaces; l++) {
          aiFace face = mesh_data->mFaces[l];

          index_count += face.mNumIndices;
        }
      }

      u32 vertices_count = count * 3;
      u32 normals_count = count * 3;
      u32 uv_count = count * 2;
      u32 indices_count = index_count;

      u32 vertices_index = 0;
      u32 normals_index = 0;
      u32 uv_index = 0;
      u32 indices_index = 0;

      Mesh mesh = {};
      allocate_mesh(&mesh, vertices_count, normals_count, indices_count, uv_count, 0);

      for (u32 i=0; i<scene->mNumMeshes; i++) {
        aiMesh *mesh_data = scene->mMeshes[i];

        for (u32 l=0; l<mesh_data->mNumVertices; l++) {
          mesh.data.vertices[vertices_index++] = mesh_data->mVertices[l].x;
          mesh.data.vertices[vertices_index++] = mesh_data->mVertices[l].y;
          mesh.data.vertices[vertices_index++] = mesh_data->mVertices[l].z;

          float new_distance = glm::length(vec3(mesh_data->mVertices[l].x, mesh_data->mVertices[l].y, mesh_data->mVertices[l].z));
          if (new_distance > max_distance) {
            max_distance = new_distance;
          }

          mesh.data.normals[normals_index++] = mesh_data->mNormals[l].x;
          mesh.data.normals[normals_index++] = mesh_data->mNormals[l].y;
          mesh.data.normals[normals_index++] = mesh_data->mNormals[l].z;

          if (mesh_data->mTextureCoords[0]) {
            mesh.data.uv[uv_index++] = mesh_data->mTextureCoords[0][l].x;
            mesh.data.uv[uv_index++] = mesh_data->mTextureCoords[0][l].y;
          }
        }

        for (u32 l=0; l<mesh_data->mNumFaces; l++) {
          aiFace face = mesh_data->mFaces[l];

          for (u32 j=0; j<face.mNumIndices; j++) {
            mesh.data.indices[indices_index++] = face.mIndices[j] + indices_offset;
          }
        }

        indices_offset += mesh_data->mNumVertices;
      }

      model->mesh = mesh;
    }

    platform.debug_free_file(result);

    optimize_model(model);

    model->radius = max_distance;
    model->state = AssetState::HAS_DATA; // TODO(sedivy): atomic
  }

  free(work);
}

inline bool process_model(Memory *memory, Model *model) {
  if (model->state == AssetState::INITIALIZED) {
    return false;
  }

  if (platform.queue_has_free_spot(memory->low_queue)) {
    if (model->state == AssetState::EMPTY) {
      LoadModelWork *work = (LoadModelWork *)malloc(sizeof(LoadModelWork));
      work->model = model;

      platform.add_work(memory->low_queue, load_model_work, work);

      return true;
    }
  }

  if (model->state == AssetState::HAS_DATA) {
    initialize_model(model);
    return false;
  }

  return true;
}

inline void use_model_mesh(App *app, Mesh *mesh) {
  glBindBuffer(GL_ARRAY_BUFFER, mesh->buffer);

  u32 offset = 0;

  if (shader_has_attribute(app->current_program, "position")) {
    GLuint id = shader_get_attribute_location(app->current_program, "position");
    glVertexAttribPointer(id, 3, GL_FLOAT, GL_FALSE, 0, (void *)offset);
  }

  offset += mesh->data.vertices_count * sizeof(float);

  if (shader_has_attribute(app->current_program, "normals")) {
    GLuint id = shader_get_attribute_location(app->current_program, "normals");
    glVertexAttribPointer(id, 3, GL_FLOAT, GL_FALSE, 0, (void *)offset);
  }

  offset += mesh->data.normals_count * sizeof(float);

  if (shader_has_attribute(app->current_program, "uv")) {
    GLuint id = shader_get_attribute_location(app->current_program, "uv");
    glVertexAttribPointer(id, 2, GL_FLOAT, GL_FALSE, 0, (void *)offset);
  }

  offset += mesh->data.uv_count * sizeof(float);

  if (shader_has_attribute(app->current_program, "colors")) {
    GLuint id = shader_get_attribute_location(app->current_program, "colors");
    glVertexAttribPointer(id, 3, GL_FLOAT, GL_FALSE, 0, (void *)offset);
  }

  offset += mesh->data.colors_count * sizeof(float);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_id);
}
