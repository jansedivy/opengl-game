#include "app.h"

#include "string.cpp"

void update_world_position(WorldPosition *position) {
  int x = glm::floor(position->offset_.x / (float)CHUNK_SIZE_X);
  int z = glm::floor(position->offset_.z / (float)CHUNK_SIZE_Y);

  position->offset_.x -= x * CHUNK_SIZE_X;
  position->offset_.z -= z * CHUNK_SIZE_Y;

  position->chunk_x += x;
  position->chunk_y += z;
}

vec3 get_forward(quat value) {
  return glm::inverse(value) * vec3(0.0, 0.0, -1.0);
}

vec3 get_up(quat value) {
  return glm::inverse(value) * vec3(0.0, -1.0, 0.0);
}

vec3 get_world_position(WorldPosition position) {
  return position.offset_ + vec3(CHUNK_SIZE_X * position.chunk_x, 0.0, CHUNK_SIZE_Y * position.chunk_y);
}

vec3 position_direction(WorldPosition &from, WorldPosition &to) {
  return glm::normalize(get_world_position(to) - get_world_position(from));
}

WorldPosition add_offset(WorldPosition position, vec3 offset) {
  position.offset_ += offset;

  update_world_position(&position);

  return position;
};

WorldPosition make_position(vec3 value) {
  WorldPosition result;

  result.chunk_x = 0;
  result.chunk_y = 0;
  result.offset_ = value;

  update_world_position(&result);

  return result;
}

float position_distance2(WorldPosition &a, WorldPosition &b) {
  return glm::distance2(get_world_position(a), get_world_position(b));
}

mat4 make_billboard_matrix(WorldPosition &position, WorldPosition &camera_position, vec3 camera_up) {
  vec3 look = position_direction(position, camera_position);
  vec3 right = glm::cross(camera_up, look);
  vec3 up = glm::cross(look, right);
  mat4 transform;
  transform[0] = vec4(right, 0.0f);
  transform[1] = vec4(up, 0.0f);
  transform[2] = vec4(look, 0.0f);
  /* transform[3] = vec4(position, 1.0f); */
  return transform;
}

inline mat4 get_model_view(Entity *entity, Camera *camera) {
  mat4 model_view;

  model_view = glm::translate(model_view, get_world_position(entity->header.position));
  model_view *= glm::toMat4(entity->header.orientation);
  if (entity->header.flags & EntityFlags::LOOK_AT_CAMERA) {
    model_view *= make_billboard_matrix(entity->header.position, camera->position, vec3(camera->view_matrix[0][1], camera->view_matrix[1][1], camera->view_matrix[2][1]));
  }
  model_view = glm::scale(model_view, entity->header.scale);

  return model_view;
}


#include "assets.cpp"
#include "raytrace.cpp"
#include "plane.cpp"
#include "camera.cpp"

#include "texture.cpp"
#include "shader.cpp"
#include "model.cpp"
#include "chunk.cpp"
#include "primitives.cpp"

template<typename T>
void mount_entity_to_terrain(T *entity) {
  vec3 position = get_world_position(entity->header.position);
  entity->header.position.offset_.y = get_terrain_height_at(position.x, position.z);
}

Model *get_model_by_name(App *app, char *name) {
  return app->models.at(name);
}

#include "level.cpp"

inline Entity *get_entity_by_id(App *app, u32 id) {
  PROFILE_BLOCK("Finding entity");
  for (auto it = array::begin(app->entities); it != array::end(app->entities); it++) {
    if (it->header.id == id) {
      return it;
    }
  }

  return NULL;
}

u32 next_entity_id(App *app) {
  return ++app->last_id;
}

void generate_random_grass_positions(vec2 start, vec4 *positions, u32 position_count, float min_radius, float radius, float min_grass_scale, float max_grass_scale, u32 *count) {
  *count = 0;

  positions[0] = vec4(start.x, get_terrain_height_at(start.x, start.y), start.y, get_random_float_between(min_grass_scale, max_grass_scale));
  *count += 1;

  bool full = false;
  for (u32 i=0; i<position_count; i++) {
    if (full) {
      break;
    }

    vec2 record = vec2(positions[i].x, positions[i].z);

    for (u32 k=0; k<10; k++) {
      float angle = get_random_float_between(0.0f, tau);
      float random_distance = get_random_float_between(min_radius + radius, min_radius + radius);

      vec2 new_position = record + vec2(glm::cos(angle) * random_distance, glm::sin(angle) * random_distance);

      if (new_position.x < 0.0f || new_position.y < 0.0f || new_position.x > 40000.0f || new_position.y > 40000.0f) {
        continue;
      }

      bool hit = false;

      for (u32 l=0; l<(*count); l++) {
        vec2 check_record = vec2(positions[l].x, positions[l].z);

        float distance = glm::distance(new_position, check_record);
        if (distance < min_radius) {
          hit = true;
          break;
        }
      }

      if (!hit) {
        if ((*count) >= position_count) {
          full = true;
          break;
        }
        positions[(*count)] = vec4(new_position.x, get_terrain_height_at(new_position.x, new_position.y), new_position.y, get_random_float_between(min_grass_scale, max_grass_scale));
        *count += 1;
      }
    }
  }
}

void quit(Memory *memory) {
}

void setup_all_shaders(App *app) {
  create_shader(&app->main_object_program, "assets/shaders/object.vert", "assets/shaders/object.frag");
  create_shader(&app->water_program, "assets/shaders/water.vert", "assets/shaders/water.frag");
  create_shader(&app->solid_program, "assets/shaders/solid.vert", "assets/shaders/solid.frag");
  create_shader(&app->record_depth_program, "assets/shaders/record_depth.vert", "assets/shaders/record_depth.frag");
  create_shader(&app->phong_program, "assets/shaders/phong.vert", "assets/shaders/phong.frag");
  create_shader(&app->terrain_program, "assets/shaders/terrain.vert", "assets/shaders/terrain.frag");
  create_shader(&app->textured_program, "assets/shaders/textured.vert", "assets/shaders/textured.frag");
  create_shader(&app->grass_program, "assets/shaders/grass.vert", "assets/shaders/grass.frag");
  create_shader(&app->controls_program, "assets/shaders/controls.vert", "assets/shaders/controls.frag");

  create_shader(&app->debug_program, "assets/shaders/debug.vert", "assets/shaders/debug.frag");
  create_shader(&app->particle_program, "assets/shaders/particle.vert", "assets/shaders/particle.frag");

  create_shader(&app->skybox_program, "assets/shaders/skybox.vert", "assets/shaders/skybox.frag");

  create_shader(&app->ui_program, "assets/shaders/ui.vert", "assets/shaders/ui.frag");

  create_shader(&app->fullscreen_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen.frag");
  create_shader(&app->fullscreen_merge_alpha, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_merge_alpha.frag");
  create_shader(&app->fullscreen_fog_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_fog.frag");
  create_shader(&app->fullscreen_color_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_color.frag");
  create_shader(&app->fullscreen_fxaa_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_fxaa.frag");
  create_shader(&app->fullscreen_bloom_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_bloom.frag");
  create_shader(&app->fullscreen_hdr_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_hdr.frag");
  create_shader(&app->fullscreen_SSAO_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_ssao.frag");
  create_shader(&app->fullscreen_depth_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_depth.frag");
  create_shader(&app->fullscreen_lens_program, "assets/shaders/fullscreen.vert", "assets/shaders/fullscreen_lens_flare.frag");
}

void init(Memory *memory) {
  glewExperimental = GL_TRUE;
  glewInit();

  App *app = memory->app;

  app->antialiasing = true;
  app->color_correction = true;
  app->lens_flare = false;
  app->bloom = false;
  app->hdr = false;

  app->time = 0;

  app->current_program = NULL;

  app->editor.handle_size = 40.0f;
  app->editor.holding_entity = false;
  app->editor.inspect_entity = false;
  app->editor.show_left = true;
  app->editor.show_right = true;
  app->editor.left_state = EditorLeftState::MODELING;
  app->editor.speed = 10000.0f;

  debug_global_memory = memory;

  platform = memory->platform;

  app->last_id = 0;

  app->camera.ortho = false;
  app->camera.near = 0.5f;
  app->camera.far = 50000.0f;
  app->camera.size = vec2((float)memory->width, (float)memory->height);

  app->shadow_camera.ortho = true;
  app->shadow_camera.near = 1.0f;
  app->shadow_camera.far = 100000.0f;
  app->shadow_camera.size = vec2(4096.0f, 4096.0f) / 2.0f;
  app->shadow_camera.orientation = quat(0.82f, 0.55f, 0.0f, 0.0f);

  glGenVertexArrays(1, &app->vao);
  glBindVertexArray(app->vao);

  /* glEnable(GL_MULTISAMPLE); */
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  glCullFace(GL_BACK);

  {
    {
      generate_sphere_mesh(&app->sphere_model.mesh, &app->sphere_model.radius, 40);
      app->sphere_model.id_name = allocate_string("sphere");
      app->sphere_model.path = NULL;
      app->sphere_model.state = AssetState::HAS_DATA;
      app->models[app->sphere_model.id_name] = &app->sphere_model;
    }

    {
      GLfloat vertices[] = {
        -1.0, -1.0,  1.0,
        1.0, -1.0,  1.0,
        1.0,  1.0,  1.0,
        -1.0,  1.0,  1.0,
        -1.0, -1.0, -1.0,
        -1.0,  1.0, -1.0,
        1.0,  1.0, -1.0,
        1.0, -1.0, -1.0,
        -1.0,  1.0, -1.0,
        -1.0,  1.0,  1.0,
        1.0,  1.0,  1.0,
        1.0,  1.0, -1.0,
        -1.0, -1.0, -1.0,
        1.0, -1.0, -1.0,
        1.0, -1.0,  1.0,
        -1.0, -1.0,  1.0,
        1.0, -1.0, -1.0,
        1.0,  1.0, -1.0,
        1.0,  1.0,  1.0,
        1.0, -1.0,  1.0,
        -1.0, -1.0, -1.0,
        -1.0, -1.0,  1.0,
        -1.0,  1.0,  1.0,
        -1.0,  1.0, -1.0
      };

      GLfloat normals[] = {
        0.0,  0.0,  1.0,
        0.0,  0.0,  1.0,
        0.0,  0.0,  1.0,
        0.0,  0.0,  1.0,
        0.0,  0.0, -1.0,
        0.0,  0.0, -1.0,
        0.0,  0.0, -1.0,
        0.0,  0.0, -1.0,
        0.0,  1.0,  0.0,
        0.0,  1.0,  0.0,
        0.0,  1.0,  0.0,
        0.0,  1.0,  0.0,
        0.0, -1.0,  0.0,
        0.0, -1.0,  0.0,
        0.0, -1.0,  0.0,
        0.0, -1.0,  0.0,
        1.0,  0.0,  0.0,
        1.0,  0.0,  0.0,
        1.0,  0.0,  0.0,
        1.0,  0.0,  0.0,
        -1.0,  0.0,  0.0,
        -1.0,  0.0,  0.0,
        -1.0,  0.0,  0.0,
        -1.0,  0.0,  0.0
      };

      GLint indices[] = {
        0, 1, 2,      0, 2, 3,
        4, 5, 6,      4, 6, 7,
        8, 9, 10,     8, 10, 11,
        12, 13, 14,   12, 14, 15,
        16, 17, 18,   16, 18, 19,
        20, 21, 22,   20, 22, 23
      };

      GLfloat uvs[] = {
        0, -3,
        1, -3,
        0, -2,
        1, -2,
        0, -2,
        1, -2,
        0, -1,
        1, -1,
        0, -1,
        1, -1,
        0, 0,
        1, 0,
        0, 0,
        1, 0,
        0, 1,
        1, 1,
        1, -3,
        2, -3,
        1, -2,
        2, -2,
        -1, -3,
        0, -3,
        -1, -2,
        0, -2
      };

      Mesh mesh;
      allocate_mesh(&mesh, array_count(vertices), array_count(normals), array_count(indices), array_count(uvs));
      memcpy(mesh.data.vertices, &vertices, sizeof(vertices));
      memcpy(mesh.data.normals, &normals, sizeof(normals));
      memcpy(mesh.data.indices, &indices, sizeof(indices));
      memcpy(mesh.data.uv, &uvs, sizeof(uvs));

      app->cube_model.id_name = allocate_string("cube");
      app->cube_model.path = NULL;
      app->cube_model.radius = calculate_radius(&mesh);
      app->cube_model.mesh = mesh;
      app->cube_model.state = AssetState::HAS_DATA;
      initialize_model(&app->cube_model);
      app->models[app->cube_model.id_name] = &app->cube_model;
    }

    {
      GLfloat vertices[] = {
        -1.0, -1.0, 0.0, 1.0, -1.0, 0.0, 1.0, 1.0, 0.0, -1.0, 1.0, 0.0
      };

      GLfloat normals[] = {
        -1.0, -1.0, 0.0, 1.0, -1.0, 0.0, 1.0, 1.0, 0.0, -1.0, 1.0, 0.0
      };

      GLint indices[] = {
        0, 1, 2, 0, 2, 3
      };

      GLfloat uvs[] = {
        0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0
      };

      Mesh mesh;
      allocate_mesh(&mesh, array_count(vertices), array_count(normals), array_count(indices), array_count(uvs));
      memcpy(mesh.data.vertices, &vertices, sizeof(vertices));
      memcpy(mesh.data.normals, &normals, sizeof(normals));
      memcpy(mesh.data.indices, &indices, sizeof(indices));
      memcpy(mesh.data.uv, &uvs, sizeof(uvs));

      app->quad_model.id_name = allocate_string("quad");
      app->quad_model.path = NULL;
      app->quad_model.radius = calculate_radius(&mesh);
      app->quad_model.mesh = mesh;
      app->quad_model.state = AssetState::HAS_DATA;
      initialize_model(&app->quad_model);
      app->models[app->quad_model.id_name] = &app->quad_model;
    }

    Model *model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/rock.obj");
    model->id_name = allocate_string("rock");
    app->models[model->id_name] = model;

    model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/plant.lwo");
    model->id_name = allocate_string("plant");
    app->models[model->id_name] = model;

    model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/plant_01.lwo");
    model->id_name = allocate_string("plant_001");
    app->models[model->id_name] = model;

    model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/tree_01.obj");
    model->id_name = allocate_string("tree_001");
    app->models[model->id_name] = model;

    model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/tree_02.obj");
    model->id_name = allocate_string("tree_002");
    app->models[model->id_name] = model;

    model = static_cast<Model *>(malloc(sizeof(Model)));
    model->path = allocate_string("assets/models/tree_03.obj");
    model->id_name = allocate_string("tree_003");
    app->models[model->id_name] = model;
  }

  setup_all_shaders(app);

  app->chunk_cache_count = 4096;
  app->chunk_cache = static_cast<TerrainChunk *>(malloc(sizeof(TerrainChunk) * app->chunk_cache_count));

  for (u32 i=0; i<app->chunk_cache_count; i++) {
    TerrainChunk *chunk = app->chunk_cache + i;
    chunk->models[0].state = AssetState::EMPTY;
    chunk->models[1].state = AssetState::EMPTY;
    chunk->models[2].state = AssetState::EMPTY;
    chunk->initialized = false;
  }

  app->color_correction_texture.path = allocate_string("assets/textures/color_correction.png");
  app->gradient_texture.path = allocate_string("assets/textures/gradient.png");

  {
    load_texture(&app->gradient_texture, STBI_rgb_alpha);
    initialize_texture(&app->gradient_texture, GL_RGBA, GL_RGBA, false);
    stbi_image_free(app->gradient_texture.data);
    app->gradient_texture.data = NULL;
  }

  {
    load_texture(&app->color_correction_texture, STBI_rgb_alpha);
    initialize_texture(&app->color_correction_texture, GL_RGB8, GL_RGBA, false);
    stbi_image_free(app->color_correction_texture.data);
    app->color_correction_texture.data = NULL;
  }

  {
    acquire_asset_file((char *)"assets/font.ttf");
    DebugReadFileResult font_file = platform.debug_read_entire_file("assets/font.ttf");
    app->font = create_font(font_file.contents, 16.0f);
    platform.debug_free_file(font_file);
  }

  {
    acquire_asset_file((char *)"assets/mono_font.ttf");
    DebugReadFileResult font_file = platform.debug_read_entire_file("assets/mono_font.ttf");
    app->mono_font = create_font(font_file.contents, 16.0f);
    platform.debug_free_file(font_file);
  }

  {
    Array<char *> faces;
    array::reserve(faces, 6);
    array::push_back(faces, (char *)"assets/textures/right.png");
    array::push_back(faces, (char *)"assets/textures/left.png");
    array::push_back(faces, (char *)"assets/textures/top.png");
    array::push_back(faces, (char *)"assets/textures/bottom.png");
    array::push_back(faces, (char *)"assets/textures/back.png");
    array::push_back(faces, (char *)"assets/textures/front.png");

    load_and_initialize_cubemap_texture(&app->cubemap, &faces);
  }

  {
    float vertices[] = {
      -1.0f, -1.0f,
      1.0f, -1.0f,
      -1.0f, 1.0f,

      1.0f, -1.0f,
      1.1f, 1.0f,
      -1.0f, 1.0f
    };

    glGenBuffers(1, &app->fullscreen_quad);
    glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // NOTE(sedivy): frame buffer
  for (u32 i=0; i<array_count(app->frames); i++) {
    FrameBuffer *frame = app->frames + i;

    frame->width = 1280;
    frame->height = 720;

    // NOTE(sedivy): texture
    {
      glGenTextures(1, &frame->texture);
      glBindTexture(GL_TEXTURE_2D, frame->texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // NOTE(sedivy): depth
    {
      glGenTextures(1, &frame->depth);
      glBindTexture(GL_TEXTURE_2D, frame->depth);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, frame->width, frame->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // NOTE(sedivy): Framebuffer
    {
      glGenFramebuffers(1, &frame->id);
      glBindFramebuffer(GL_FRAMEBUFFER, frame->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame->texture, 0);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frame->depth, 0);
    }
  }

  {
    app->shadow_width = 4096;
    app->shadow_height = 4096;

    // NOTE(sedivy): depth
    {
      glGenTextures(1, &app->shadow_depth_texture);
      glBindTexture(GL_TEXTURE_2D, app->shadow_depth_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, app->shadow_width, app->shadow_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }

    // NOTE(sedivy): Framebuffer
    {
      glGenFramebuffers(1, &app->shadow_buffer);
      glBindFramebuffer(GL_FRAMEBUFFER, app->shadow_buffer);
      glDrawBuffer(GL_NONE);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, app->shadow_depth_texture, 0);
    }
  }

  glGenBuffers(1, &app->debug_buffer);

  load_debug_level(memory, app);

  Entity follow_entity = {};
  follow_entity.header.id = next_entity_id(app);
  follow_entity.header.type = EntityType::EntityPlayer;
  follow_entity.header.flags = EntityFlags::RENDER_HIDDEN | EntityFlags::HIDE_IN_EDITOR;
  follow_entity.header.position = make_position(vec3(20000.0f, 20000.0f, 20000.0f));
  follow_entity.header.orientation = quat(1.0f, 0.0f, 0.0f, 0.0f);
  follow_entity.header.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  follow_entity.header.model = &app->sphere_model;
  array::push_back(app->entities, follow_entity);

  app->camera_follow = follow_entity.header.id;

  {
    /* generate_trees(app); */

#if 0
    Random random = create_random_sequence();

    vec2 rock_noise[200];
    u32 size = generate_blue_noise(rock_noise, array_count(rock_noise), 100.0f, 5000.0f);

    for (u32 i=0; i<size; i++) {
      Entity *entity = app->entities + app->entity_count;

      entity->id = next_entity_id(app);
      entity->type = EntityBlock;
      entity->position.x = rock_noise[i].x;
      entity->position.z = rock_noise[i].y;
      mount_entity_to_terrain(entity);
      entity->flags = EntityFlags::PERMANENT_FLAG;
      entity->scale = 100.0f * vec3(0.8f + get_next_float(&random) / 2.0f, 0.8f + get_next_float(&random) / 2.0f, 0.8f + get_next_float(&random) / 2.0f);
      entity->model = &app->rock_model;
      entity->color = vec4(get_random_float_between(0.2f, 0.5f), 0.45f, 0.5f, 1.0f);
      entity->orientation = vec3(get_next_float(&random) * 0.5f - 0.25f, get_next_float(&random) * glm::pi<float>(), get_next_float(&random) * 0.5f - 0.25f);
      app->entity_count += 1;
    }
#endif
  }

  app->framecount = 0;
  app->frametimelast = platform.get_time();

  float vertices[] = {
    -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
    -0.5f, 0.5f, 0.0f,

    0.5f, -0.5f, 0.0f,
    0.5f, 0.5f, 0.0f,
    -0.5f, 0.5f, 0.0f
  };

  glGenBuffers(1, &app->particle_model);
  glBindBuffer(GL_ARRAY_BUFFER, app->particle_model);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  app->next_particle = 0;

  glGenBuffers(1, &app->particle_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, app->particle_buffer);
  glBufferData(GL_ARRAY_BUFFER, array_count(app->particles) * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

  glGenBuffers(1, &app->particle_color_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, app->particle_color_buffer);
  glBufferData(GL_ARRAY_BUFFER, array_count(app->particles) * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
}

vec4 shade_color(vec4 color, float percent) {
  return vec4(vec3(color) * (1 + percent), color.a);
}

#include "render_group.cpp"
#include "ui.cpp"

bool sort_by_distance(const EditorHandleRenderCommand &a, const EditorHandleRenderCommand &b) {
  return a.distance_from_camera > b.distance_from_camera;
}

bool sort_particles_by_distance(const Particle &a, const Particle &b) {
  return a.distance_from_camera > b.distance_from_camera;
}

void render_debug_circle(Array<EditorHandleRenderCommand> *commands, Camera *camera, WorldPosition &position, float size, vec4 color) {
  vec3 world_position = get_world_position(position);

  if (!is_sphere_in_frustum(&camera->frustum, world_position, size)) { return; }

  mat4 model_view;
  model_view = glm::translate(model_view, world_position);
  model_view = glm::scale(model_view, vec3(size));
  model_view *= make_billboard_matrix(position, camera->position, vec3(camera->view_matrix[0][1], camera->view_matrix[1][1], camera->view_matrix[2][1]));

  EditorHandleRenderCommand command;
  command.distance_from_camera = position_distance2(camera->position, position);
  command.model_view = model_view;
  command.color = color;

  array::push_back(*commands, command);
}

void draw_3d_debug_info(Input &input, App *app) {
  Texture *texture = get_texture(app, (char *)"circle.png");
  if (!texture && texture->state == AssetState::INITIALIZED) { return; }

  PROFILE_BLOCK("Draw 3D Debug");
  use_program(app, &app->controls_program);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthFunc(GL_ALWAYS);

  set_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);

  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_2D, texture->id);
  set_uniformi(app->current_program, "textureImage", 0);

  use_model_mesh(app, &app->quad_model.mesh);

  Array<EditorHandleRenderCommand> render_commands;

  if (app->editor.show_handles && !app->editor.holding_entity) {
    for (auto it = array::begin(app->entities); it != array::end(app->entities); it++) {
      if (it->header.flags & EntityFlags::HIDE_IN_EDITOR) { continue; }

      if (it->header.model && it->header.model->state == AssetState::INITIALIZED) {
        continue;
      }

      vec4 color = vec4(0.0, 1.0, 0.0, 0.7);
      if (it->header.type == EntityType::EntityParticleEmitter) {
        color = vec4(0.0, 1.0, 1.0, 0.7);
      }

      if (!input.is_mouse_locked && app->editor.hovering_entity && app->editor.hover_entity == it->header.id) {
        color *= vec4(0.6, 0.6, 0.6, 1.0);
      }

      if (it->header.type == EntityType::EntityGrass) {
        EntityGrass *grass = (EntityGrass *)it;
        if (app->editor.inspect_entity && app->editor.entity_id == grass->header.id) {
          for (u32 grass_index=0; grass_index<grass->grass_count; grass_index++) {
            vec4 data = grass->positions[grass_index];
            WorldPosition position = make_position(vec3(data));
            render_debug_circle(&render_commands, &app->camera, position, data.w / 7.0f, vec4(0.4, 1.0, 0.4, 0.4));
          }
        }
      }

      render_debug_circle(&render_commands, &app->camera, it->header.position, app->editor.handle_size, color);
    }
  }

  std::sort(array::begin(render_commands), array::end(render_commands), sort_by_distance);

  for (auto it = array::begin(render_commands); it != array::end(render_commands); it++) {
    set_uniform(app->current_program, "uMVMatrix", it->model_view);
    set_uniform(app->current_program, "in_color", it->color);
    glDrawElements(GL_TRIANGLES, app->quad_model.mesh.data.indices_count, GL_UNSIGNED_INT, 0);
  }

  glDisable(GL_BLEND);
  glDepthFunc(GL_LESS);
}

void flush_2d_render(App *app, Memory *memory) {
  PROFILE_BLOCK("Draw UI Flush");

  mat4 projection = glm::ortho(0.0f, float(memory->width), float(memory->height), 0.0f);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glActiveTexture(GL_TEXTURE0);

  use_program(app, &app->ui_program);

  set_uniform(app->current_program, "uPMatrix", projection);
  set_uniformi(app->current_program, "textureImage", 0);

  u32 vertices_index = 0;

  UICommandBuffer *command_buffer = &app->editor.command_buffer;

  glBindBuffer(GL_ARRAY_BUFFER, app->debug_buffer);
  glBufferData(GL_ARRAY_BUFFER, command_buffer->vertices.size * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, command_buffer->vertices.size * sizeof(GLfloat), &command_buffer->vertices[0]);

  glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
  glVertexAttribPointer(shader_get_attribute_location(app->current_program, "uv"), 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *)(2 * sizeof(GLfloat)));

  for (auto it = array::begin(command_buffer->commands); it != array::end(command_buffer->commands); it++) {
    glBindTexture(GL_TEXTURE_2D, it->texture_id);
    set_uniform(app->current_program, "background_color", it->color);
    set_uniform(app->current_program, "image_color", it->image_color);
    glDrawArrays(GL_TRIANGLES, vertices_index, it->vertices_count);
    vertices_index += it->vertices_count;
  }

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

void generate_grass(EntityGrass *grass) {
  grass->grass_count = 0;
  grass->render = false;

  vec3 position = get_world_position(grass->header.position);

  generate_random_grass_positions(vec2(position.x, position.z), grass->positions, MAX_GRASS_GROUP_COUNT, grass->min_radius, grass->max_radius, grass->min_scale, grass->max_scale, &grass->grass_count);

  for (u32 i=0; i<grass->grass_count; i++) {
    grass->rotations[i] = vec3(0.0f, get_random_float_between(0.0f, tau), 0.0f);
    float tint = get_random_float_between(-0.14f, 0.04f);
    grass->tints[i] = vec3(0.4353f + tint, 0.5922f + tint, 0.2235f + tint);
  }

  grass->reload_data = true;
  grass->render = true;
}

void generate_grass_work(void *data) {
  EntityGrass *grass = (EntityGrass *)data;
  generate_grass(grass);
}

void draw_2d_debug_info(App *app, Memory *memory, Input &input) {
  PROFILE_BLOCK("Settup UI");
  UICommandBuffer *command_buffer = &app->editor.command_buffer;
  array::clear(command_buffer->vertices);
  array::clear(command_buffer->commands);

  DebugDrawState draw_state;
  draw_state.offset_top = 25.0f;
  draw_state.width = 175.0f;

  vec4 button_background_color = vec4(0.5f, 0.6f, 0.9f, 0.9f);
  vec4 default_background_color = vec4(0.2f, 0.4f, 0.9f, 0.9f);

  char text[256];

  push_toggle_button(app, input, &draw_state, command_buffer, 10.0f, &app->editor.show_left, vec4(1.0f, 0.16f, 0.15f, 0.9f));

  if (app->editor.show_left) {
    sprintf(text, "performance: %d\n", app->editor.show_performance);
    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
      app->editor.show_performance = !app->editor.show_performance;
    }

    if (app->editor.show_performance) {
      for (u32 i=0; i<array_count(global_last_frame_counters); i++) {
        DebugCounter *counter = global_last_frame_counters + i;

        if (counter->hit_count) {
          float time = (float)(counter->cycle_count * 1000) / (float)platform.get_performance_frequency();

          sprintf(text, "%17s: %6.3fms %4llu %6.3fms\n", counter->name, time, counter->hit_count, time / (float)counter->hit_count);

          float original_width = draw_state.width;
          draw_state.width = font_get_string_size_in_px(&app->mono_font, text) + 5.0f;
          push_debug_text(&app->mono_font, &draw_state, command_buffer, 10.0f, text, vec3(1.0f, 1.0f, 1.0f), vec4(0.0f, 0.1f, 0.6f, 0.9f));
          draw_state.width = original_width;
        }
      }
    }

    sprintf(text, "fps: %d\n", (u32)app->fps);
    push_debug_text(&app->font, &draw_state, command_buffer, 10.0f, text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

    draw_state.offset_top += 10.0f;

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 40.0f, (char *)"save!", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
      save_level(memory, app);
    }

    draw_state.offset_top += 10.0f;

    vec4 selected_button_background_color = vec4(0.5f, 0.8f, 1.0f, 0.9f);

    debug_layout_set(&draw_state, 4);

    if (debug_button_image(app, input, &draw_state, command_buffer, 10.0f, app->editor.left_state == EditorLeftState::MODELING ? selected_button_background_color : button_background_color, get_texture(app, (char *)"model.png"))) {
      app->editor.left_state = EditorLeftState::MODELING;
    }

    if (debug_button_image(app, input, &draw_state, command_buffer, 10.0f, app->editor.left_state == EditorLeftState::LIGHT ? selected_button_background_color : button_background_color, get_texture(app, (char *)"light.png"))) {
      app->editor.left_state = EditorLeftState::LIGHT;
    }

    draw_state.offset_top += draw_state.next_width;

    debug_layout_reset(&draw_state);

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Terrain", vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::TERRAIN ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::TERRAIN;
    }

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Post Processing", vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::POST_PROCESSING ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::POST_PROCESSING;
    }

    if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"Editor settings", vec3(1.0f, 1.0f, 1.0f), app->editor.left_state == EditorLeftState::EDITOR_SETTINGS ? selected_button_background_color : button_background_color)) {
      app->editor.left_state = EditorLeftState::EDITOR_SETTINGS;
    }

    draw_state.offset_top += 10.0f;

    switch (app->editor.left_state) {
      {
        case EditorLeftState::MODELING:
          struct EntityButtonSelection {
            const char *id_name;
            const char *display_name;
          };

          EntityButtonSelection types[] = {
            {"tree_001", "Tree 1"},
            {"tree_002", "Tree 2"},
            {"tree_003", "Tree 3"},
            {"rock", "Rock"},
            {"cube", "Cube"},
            {"sphere", "Sphere"},
            {"quad", "Quad"},
          };

          for (u32 i=0; i<array_count(types); i++) {
            if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)types[i].display_name, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
              Entity entity;

              vec3 forward = get_forward(app->camera.orientation);

              entity.header.id = next_entity_id(app);
              entity.header.type = EntityType::EntityBlock;
              entity.header.position = add_offset(app->camera.position, forward * 400.0f);
              entity.header.orientation = quat();
              entity.header.scale = vec3(100.f);
              entity.header.model = get_model_by_name(app, (char *)types[i].id_name);
              entity.header.color = vec4(0.31f, 0.18f, 0.02f, 1.0f);
              entity.header.flags = EntityFlags::CASTS_SHADOW | EntityFlags::PERMANENT_FLAG;

              if (strcmp(types[i].id_name, "quad") == 0) {
                entity.header.flags |= EntityFlags::LOOK_AT_CAMERA;
              }

              app->editor.entity_id = entity.header.id;
              app->editor.inspect_entity = true;
              array::push_back(app->entities, entity);
            }
          }

          draw_state.offset_top += 10.0f;

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)"particle emitter", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            Entity entity;

            vec3 forward = get_forward(app->camera.orientation);

            entity.header.id = next_entity_id(app);
            entity.header.type = EntityType::EntityParticleEmitter;
            entity.header.position = add_offset(app->camera.position, forward * 400.0f);
            entity.header.orientation = quat();
            entity.header.model = NULL;
            entity.header.flags = EntityFlags::RENDER_HIDDEN | EntityFlags::PERMANENT_FLAG;
            ((EntityParticleEmitter *)&entity)->particle_size = 40.0f;
            ((EntityParticleEmitter *)&entity)->initial_color = vec4(1.0f);

            app->editor.entity_id = entity.header.id;
            app->editor.inspect_entity = true;
            array::push_back(app->entities, entity);
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)"Grass", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            Entity new_entity;
            EntityGrass *entity = (EntityGrass *)&new_entity;

            vec3 forward = get_forward(app->camera.orientation);

            entity->header.id = next_entity_id(app);
            entity->header.type = EntityType::EntityGrass;
            entity->header.position = add_offset(app->camera.position, forward * 400.0f);
            entity->header.orientation = quat();
            entity->header.model = NULL;
            entity->header.flags = EntityFlags::MOUNT_TO_TERRAIN | EntityFlags::RENDER_HIDDEN | EntityFlags::PERMANENT_FLAG;

            if (entity->header.flags & EntityFlags::MOUNT_TO_TERRAIN) {
              mount_entity_to_terrain(&new_entity);
            }

            entity->grass_count = 0;
            entity->min_radius = 25.0f;
            entity->max_radius = 51.0f;

            entity->min_scale = 30.0f;
            entity->max_scale = 40.0f;

            entity->data = malloc((sizeof(vec4) + sizeof(vec3) + sizeof(vec3)) * MAX_GRASS_GROUP_COUNT);
            entity->positions = (vec4 *)entity->data;
            entity->rotations = (vec3 *)(entity->positions + MAX_GRASS_GROUP_COUNT);
            entity->tints = (vec3 *)(entity->rotations + MAX_GRASS_GROUP_COUNT);

            entity->initialized = false;
            entity->reload_data = true;
            entity->render = false;
            entity->grass_model = get_model_by_name(app, (char *)"plant");
            entity->texture = get_texture(app, (char *)"plant_01.png");

            app->editor.entity_id = entity->header.id;
            app->editor.inspect_entity = true;
            array::push_back(app->entities, new_entity);
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)"Water", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            Entity entity;

            vec3 forward = get_forward(app->camera.orientation);

            entity.header.id = next_entity_id(app);
            entity.header.type = EntityType::EntityWater;
            entity.header.position = add_offset(app->camera.position, forward * 400.0f);
            entity.header.orientation = quat();
            entity.header.scale = vec3(100.0f);
            entity.header.model = get_model_by_name(app, (char *)"quad");
            entity.header.color = vec4(0.2f, 0.45f, 0.5f, 0.5f);
            entity.header.flags = EntityFlags::PERMANENT_FLAG;

            app->editor.entity_id = entity.header.id;
            app->editor.inspect_entity = true;
            array::push_back(app->entities, entity);
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 20.0f, (char *)"Phong", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            Entity entity;

            vec3 forward = get_forward(app->camera.orientation);

            entity.header.id = next_entity_id(app);
            entity.header.type = EntityType::EntityBlock;
            entity.header.position = add_offset(app->camera.position, forward * 400.0f);
            entity.header.orientation = quat();
            entity.header.scale = vec3(100.0f);
            entity.header.model = get_model_by_name(app, (char *)"sphere");
            entity.header.color = vec4(0.2f, 0.45f, 0.5f, 1.0f);
            entity.header.flags = EntityFlags::PERMANENT_FLAG;

            app->editor.entity_id = entity.header.id;
            app->editor.inspect_entity = true;
            array::push_back(app->entities, entity);
          }
          break;
      }
      {
        case EditorLeftState::TERRAIN:
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"rebuild chunks", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            rebuild_chunks(app);
          }
          break;
      }
      {
        case EditorLeftState::LIGHT:
          push_debug_editable_quat(input, &draw_state, &app->font, command_buffer, 10.0f, "Sun orientation", &app->shadow_camera.orientation, default_background_color);
          push_debug_range((char *)"near", input, &app->font, command_buffer, &draw_state, 10.0f, default_background_color, &app->shadow_camera.near, 0.0f, 100.0f);
          push_debug_range((char *)"far", input, &app->font, command_buffer, &draw_state, 10.0f, default_background_color, &app->shadow_camera.far, 0.0f, 100000.0f);
          break;
      }
      {
        case EditorLeftState::EDITOR_SETTINGS:
          sprintf(text, "Show handles %d\n", app->editor.show_handles);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.show_handles = !app->editor.show_handles;
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"slow speed", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.speed = 10000.0f;
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"medium speed", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.speed = 40000.0f;
          }

          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, (char *)"fast speed", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.speed = 60000.0f;
          }

          sprintf(text, "Experimenal editing: %d\n", app->editor.experimental_terrain_entity_movement);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.experimental_terrain_entity_movement = !app->editor.experimental_terrain_entity_movement;
          }

          sprintf(text, "Show camera frustum: %d\n", app->editor.show_camera_frustum);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->editor.show_camera_frustum = !app->editor.show_camera_frustum;
          }
          break;
      }
      {
        case EditorLeftState::POST_PROCESSING:
          sprintf(text, "AA: %d\n", app->antialiasing);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->antialiasing = !app->antialiasing;
          }

          sprintf(text, "Color correction: %d\n", app->color_correction);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->color_correction = !app->color_correction;
          }

          sprintf(text, "HDR (wip): %d\n", app->hdr);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->hdr = !app->hdr;
          }

          sprintf(text, "Bloom (wip): %d\n", app->bloom);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->bloom = !app->bloom;
          }

          sprintf(text, "Lens flare (wip): %d\n", app->lens_flare);
          if (push_debug_button(input, app, &draw_state, command_buffer, 10.0f, 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
            app->lens_flare = !app->lens_flare;
          }
          break;
      }
    }
  }

  if (app->editor.inspect_entity) {
    draw_state.offset_top = 25.0f;
    draw_state.width = 275.0f;

    push_toggle_button(app, input, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), &app->editor.show_right, vec4(1.0f, 0.16f, 0.15f, 0.9f));

    if (app->editor.show_right) {
      Entity *entity = get_entity_by_id(app, app->editor.entity_id);

      vec4 selected_button_background_color = vec4(0.5f, 0.8f, 1.0f, 0.9f);

      debug_layout_set(&draw_state, 2);

      if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, (char *)"Common", vec3(1.0f, 1.0f, 1.0f), app->editor.right_state == EditorRightState::COMMON ? selected_button_background_color : button_background_color)) {
        app->editor.right_state = EditorRightState::COMMON;
      }

      if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, (char *)"Specific", vec3(1.0f, 1.0f, 1.0f), app->editor.right_state == EditorRightState::SPECIFIC ? selected_button_background_color : button_background_color)) {
        app->editor.right_state = EditorRightState::SPECIFIC;
      }

      debug_layout_reset(&draw_state);

      draw_state.offset_top += 25.0f;

      if (entity) {
        switch (app->editor.right_state) {
          case EditorRightState::COMMON:
          {
            sprintf(text, "id: %d\n", entity->header.id);
            push_debug_text(&app->font, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

            if (entity->header.model) {
              sprintf(text, "model: %s\n", entity->header.model->id_name);
              push_debug_text(&app->font, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), text, vec3(1.0f, 1.0f, 1.0f), default_background_color);
            }

            sprintf(text, "type: %d\n", entity->header.type);
            push_debug_text(&app->font, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

            sprintf(text, "chunk_x: %d\n", entity->header.position.chunk_x);
            push_debug_text(&app->font, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

            sprintf(text, "chunk_y: %d\n", entity->header.position.chunk_y);
            push_debug_text(&app->font, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), text, vec3(1.0f, 1.0f, 1.0f), default_background_color);

            push_debug_vector(&draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "position", entity->header.position.offset_, default_background_color);
            push_debug_editable_quat(input, &draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "orientation", &entity->header.orientation, default_background_color);
            push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "scale", &entity->header.scale, default_background_color, 0.0f, 200.0f);

            float start = draw_state.offset_top;
            push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "color", &entity->header.color, default_background_color, 0.0f, 1.0f);
            debug_render_rect(command_buffer, memory->width - 151.0f, start + 1.0f, 22.0f, 22.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f));
            debug_render_rect(command_buffer, memory->width - 150.0f, start + 2.0f, 20.0f, 20.0f, entity->header.color);

            sprintf(text, "ground mounted: %d\n", (entity->header.flags & EntityFlags::MOUNT_TO_TERRAIN) != 0);
            if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
              entity->header.flags = entity->header.flags ^ EntityFlags::MOUNT_TO_TERRAIN;

              if (entity->header.flags & EntityFlags::MOUNT_TO_TERRAIN) {
                mount_entity_to_terrain(entity);
              }
            }

            sprintf(text, "casts shadow: %d\n", (entity->header.flags & EntityFlags::CASTS_SHADOW) != 0);
            if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
              entity->header.flags = entity->header.flags ^ EntityFlags::CASTS_SHADOW;
            }

            sprintf(text, "look at camera: %d\n", (entity->header.flags & EntityFlags::LOOK_AT_CAMERA) != 0);
            if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
              entity->header.flags = entity->header.flags ^ EntityFlags::LOOK_AT_CAMERA;
            }

            sprintf(text, "save to file: %d\n", (entity->header.flags & EntityFlags::PERMANENT_FLAG) != 0);
            if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 25.0f, text, vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
              entity->header.flags = entity->header.flags ^ EntityFlags::PERMANENT_FLAG;
            }
          } break;
          case EditorRightState::SPECIFIC:
          {

            if (entity->header.type == EntityType::EntityParticleEmitter) {
              EntityParticleEmitter *emitter = (EntityParticleEmitter *)entity;

              float start = draw_state.offset_top;
              push_debug_editable_vector(input, &draw_state, &app->font, command_buffer, memory->width - (draw_state.width + 25.0f), "initial_color", &emitter->initial_color, default_background_color, 0.0f, 1.0f);
              debug_render_rect(command_buffer, memory->width - 151.0f, start + 1.0f, 22.0f, 22.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f));
              debug_render_rect(command_buffer, memory->width - 150.0f, start + 2.0f, 20.0f, 20.0f, emitter->initial_color);

              float x = memory->width - (draw_state.width + 25.0f);

              push_debug_text(&app->font, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), (char *)"particle_size", vec3(1.0f, 1.0f, 1.0f), default_background_color);

              push_debug_range(NULL, input, &app->font, command_buffer, &draw_state, x, default_background_color, &emitter->particle_size, 0.0f, 100.0f);
              push_debug_text(&app->font, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), (char *)"gravity", vec3(1.0f, 1.0f, 1.0f), default_background_color);
              push_debug_range(NULL, input, &app->font, command_buffer, &draw_state, x, default_background_color, &emitter->gravity, -1000.0f, 1000.0f);
            } else if (entity->header.type == EntityType::EntityGrass) {
              EntityGrass *grass = (EntityGrass *)entity;

              draw_state.offset_top += 10.0f;

              push_debug_range((char *)"min_radius", input, &app->font, command_buffer, &draw_state, memory->width - (draw_state.width + 25.0f), default_background_color, &grass->min_radius, 10.0f, 400.0f);
              push_debug_range((char *)"max_radius", input, &app->font, command_buffer, &draw_state, memory->width - (draw_state.width + 25.0f), default_background_color, &grass->max_radius, 10.0f, 500.0f);
              push_debug_range((char *)"min_scale", input, &app->font, command_buffer, &draw_state, memory->width - (draw_state.width + 25.0f), default_background_color, &grass->min_scale, 5.0f, 100.0f);
              push_debug_range((char *)"max_scale", input, &app->font, command_buffer, &draw_state, memory->width - (draw_state.width + 25.0f), default_background_color, &grass->max_scale, 5.0f, 100.0f);

              if (push_debug_button(input, app, &draw_state, command_buffer, memory->width - (draw_state.width + 25.0f), 35.0f, (char *)"Rebuild grass", vec3(1.0f, 1.0f, 1.0f), button_background_color)) {
                platform.add_work(memory->low_queue, generate_grass_work, grass);
              }
            }

          } break;
        }
      }
    }
  }
}

void render_skybox(App *app) {
  PROFILE_BLOCK("Draw Skybox");
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  use_program(app, &app->skybox_program);

  mat4 projection = get_camera_projection(&app->camera);
  projection *= glm::toMat4(app->camera.orientation);

  set_uniform(app->current_program, "projection", projection);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, app->cubemap.id);
  set_uniformi(app->current_program, "uSampler", 0);

  use_model_mesh(app, &app->cube_model.mesh);
  glDrawElements(GL_TRIANGLES, app->cube_model.mesh.data.indices_count, GL_UNSIGNED_INT, 0);

  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

void render_scene(Memory *memory, App *app, Camera *camera, Shader *forced_shader=NULL, bool shadow_pass=false) {
  PROFILE_BLOCK("Draw Scene");
  start_render_group(&app->render_group);
  app->render_group.camera = camera;
  app->render_group.force_shader = forced_shader;
  app->render_group.shadow_pass = shadow_pass;

  RenderGroup second_render_group;
  start_render_group(&second_render_group);
  second_render_group.camera = camera;
  second_render_group.force_shader = NULL;
  second_render_group.shadow_pass = false;

  for (auto it = array::begin(app->entities); it != array::end(app->entities); it++) {
    if (!shadow_pass) {
      if ((shadow_pass && (it->header.flags & EntityFlags::CASTS_SHADOW)) || !shadow_pass) {
        EntityGrass *grass = (EntityGrass *)it;
        if (grass->render) {
          bool texture_wait = process_texture(memory, grass->texture);
          bool wait_model = process_model(memory, grass->grass_model);

          if (!texture_wait && !wait_model) {
            PROFILE_BLOCK("Render Grass");

            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthFunc(GL_LESS);

            use_program(app, &app->grass_program);

            set_uniform(app->current_program, "uPMatrix", camera->view_matrix);
            set_uniformi(app->current_program, "uShadow", 0);
            set_uniform(app->current_program, "texmapscale", vec2(1.0f / app->shadow_width, 1.0f / app->shadow_height));
            set_uniform(app->current_program, "shadow_light_position", get_world_position(app->shadow_camera.position));

            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, grass->texture->id);
            set_uniformi(app->current_program, "textureImage", 1);
            set_uniform(app->current_program, "shadow_matrix", app->shadow_camera.view_matrix);
            set_uniformf(app->current_program, "time", app->time);

            Mesh *mesh = &grass->grass_model->mesh;

            use_model_mesh(app, mesh);

            if (!grass->initialized) {
              grass->initialized = true;

              glGenBuffers(1, &grass->position_data_id);
            }

            if (grass->reload_data) {
              grass->reload_data = false;

              glBindBuffer(GL_ARRAY_BUFFER, grass->position_data_id);
              glBufferData(GL_ARRAY_BUFFER, grass->grass_count * (sizeof(vec4) + sizeof(vec3) + sizeof(vec4)), NULL, GL_STATIC_DRAW);

              glBufferSubData(GL_ARRAY_BUFFER, 0, grass->grass_count * sizeof(vec4), grass->positions);
              glBufferSubData(GL_ARRAY_BUFFER, grass->grass_count * sizeof(vec4), grass->grass_count * sizeof(vec3), grass->rotations);
              glBufferSubData(GL_ARRAY_BUFFER, grass->grass_count * (sizeof(vec4) + sizeof(vec3)), grass->grass_count * sizeof(vec4), grass->tints);

              glBindBuffer(GL_ARRAY_BUFFER, 0);
            }

            GLuint position_id = shader_get_attribute_location(app->current_program, "position_data");
            GLuint rotation_id = shader_get_attribute_location(app->current_program, "orientation");
            GLuint tint_id = shader_get_attribute_location(app->current_program, "tint");

            glBindBuffer(GL_ARRAY_BUFFER, grass->position_data_id);

            glVertexAttribPointer(position_id, 4, GL_FLOAT, GL_FALSE, 0, 0);
            glVertexAttribPointer(rotation_id, 3, GL_FLOAT, GL_FALSE, 0, (void *)(grass->grass_count * sizeof(vec4)));
            glVertexAttribPointer(tint_id, 3, GL_FLOAT, GL_FALSE, 0, (void *)(grass->grass_count * (sizeof(vec4) + sizeof(vec3))));

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_id);

            glVertexAttribDivisor(0, 0);
            glVertexAttribDivisor(1, 0);
            glVertexAttribDivisor(2, 0);
            glVertexAttribDivisor(3, 1);
            glVertexAttribDivisor(4, 1);
            glVertexAttribDivisor(5, 1);

            glDrawElementsInstanced(GL_TRIANGLES, mesh->data.indices_count, GL_UNSIGNED_INT, 0, grass->grass_count);

            glVertexAttribDivisor(0, 0);
            glVertexAttribDivisor(1, 0);
            glVertexAttribDivisor(2, 0);
            glVertexAttribDivisor(3, 0);
            glVertexAttribDivisor(4, 0);
            glVertexAttribDivisor(5, 0);
          }
        }
      }
    }

    if (it->header.model == NULL || it->header.flags & EntityFlags::RENDER_HIDDEN) { continue; }

    if (shadow_pass && !(it->header.flags & EntityFlags::CASTS_SHADOW)) {
      continue;
    }

    bool model_wait = process_model(memory, it->header.model);
    bool texture_wait = false;

    if (it->header.texture) {
      texture_wait = process_texture(memory, it->header.texture);
    }

    if (model_wait || texture_wait) { continue; }

    if (!is_sphere_in_frustum(&camera->frustum, get_world_position(it->header.position), it->header.model->radius * glm::compMax(it->header.scale))) {
      continue;
    }

    mat4 model_view = get_model_view(it, &app->camera);

    mat3 normal = glm::inverseTranspose(mat3(model_view));

    RenderCommand command;
    command.shader = &app->main_object_program;
    command.model_view = model_view;
    command.flags = it->header.flags;
    command.normal = normal;
    command.color = it->header.color;
    command.cull_type = GL_BACK;
    command.model_mesh = &it->header.model->mesh;

    if (app->editing_mode && app->editor.inspect_entity && app->editor.entity_id == it->header.id)  {
      RenderCommand wireframe_command = command;
      wireframe_command.shader = &app->solid_program;
      wireframe_command.color = vec4(1.0, 0.0, 1.0, 1.0);
      wireframe_command.flags |= EntityFlags::RENDER_WIREFRAME;
      add_command_to_render_group(&second_render_group, wireframe_command);
    }

    if (it->header.type == EntityType::EntityWater) {
      command.shader = &app->water_program;
    }

    add_command_to_render_group(&app->render_group, command);
  }

  end_render_group(app, &app->render_group);
  end_render_group(app, &second_render_group);
}

void render_terrain(Memory *memory, App *app) {
  PROFILE_BLOCK("Draw Terrain");
  use_program(app, &app->terrain_program);

  glCullFace(GL_BACK);

  set_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);

  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_2D, app->shadow_depth_texture);
  set_uniformi(app->current_program, "uShadow", 0);
  set_uniform(app->current_program, "shadow_matrix", app->shadow_camera.view_matrix);
  set_uniform(app->current_program, "texmapscale", vec2(1.0f / app->shadow_width, 1.0f / app->shadow_height));

  int x_coord = app->camera.position.chunk_x;
  int y_coord = app->camera.position.chunk_y;

  {
    PROFILE_BLOCK("Terrain render", 17*17);
    for (int y=-8 + y_coord; y<=8 + y_coord; y++) {
      for (int x=-8 + x_coord; x<=8 + x_coord; x++) {
        if (x < 0 || y < 0 ) { continue; }

        TerrainChunk *chunk = get_chunk_at(app->chunk_cache, app->chunk_cache_count, x, y);

        int dx = chunk->x - x_coord;
        int dy = chunk->y - y_coord;

        float distance = glm::length2(vec2(dx, dy));

        int detail_level = 2;
        if (distance < 16.0f) { detail_level = 1; }
        if (distance < 4.0f) { detail_level = 0; }

        Model *model = chunk_get_model(memory, chunk, detail_level);

        if (model) {
          if (!is_sphere_in_frustum(&app->camera.frustum, vec3(chunk->x * CHUNK_SIZE_X, 0.0f, chunk->y * CHUNK_SIZE_Y), model->radius)) {
            continue;
          }

          render_terrain_chunk(app, chunk, model);
        }
      }
    }
  }
}

void tick(Memory *memory, Input input) {
  debug_global_memory = memory;
  platform = memory->platform;
  App *app = memory->app;

  app->time += input.delta_time;

  {
    PROFILE_BLOCK("Tick");

    if (memory->should_reload) {
      memory->should_reload = false;

      glewExperimental = GL_TRUE;
      glewInit();
    }

    if (app->editing_mode) {
      draw_2d_debug_info(app, memory, input);
    }

    // NOTE(sedivy): update
    {
      PROFILE_BLOCK("Update");

      app->camera.size = vec2((float)memory->width, (float)memory->height);

      Entity *follow_entity = get_entity_by_id(app, app->camera_follow);
      {
        if (input.once.key_r) {
          setup_all_shaders(app);

          unload_texture(&app->gradient_texture);
          load_texture(&app->gradient_texture, STBI_rgb_alpha);
          initialize_texture(&app->gradient_texture, GL_RGBA, GL_RGBA, false);
          if (app->gradient_texture.data) {
            stbi_image_free(app->gradient_texture.data);
            app->gradient_texture.data = NULL;
          }

          unload_texture(&app->color_correction_texture);
          load_texture(&app->color_correction_texture, STBI_rgb_alpha);
          initialize_texture(&app->color_correction_texture, GL_RGB8, GL_RGBA, false);
          if (app->color_correction_texture.data) {
            stbi_image_free(app->color_correction_texture.data);
            app->color_correction_texture.data = NULL;
          }

          for (auto it = app->textures.begin(); it != app->textures.end(); it++) {
            unload_texture(it->second);
          }

          for (auto it = app->models.begin(); it != app->models.end(); it++) {
            if (it->second->path != NULL) { // NOTE(sedivy): If it has path it should be reloadable
              unload_model(it->second);
            }
          }
        }

        if (input.once.key_p) {
          app->editing_mode = !app->editing_mode;
          if (!app->editing_mode) {
            platform.lock_mouse();
          }
        }

        if (input.is_mouse_locked) {
          vec3 forward = get_forward(app->camera.orientation);
          vec3 right = glm::normalize(glm::cross(forward, vec3(0.0, 1.0, 0.0)));

          follow_entity->header.orientation *= glm::angleAxis((float)input.rel_mouse_y/200.0f, right);
          follow_entity->header.orientation *= glm::angleAxis((float)input.rel_mouse_x/200.0f, vec3(0.0, 1.0, 0.0));
          follow_entity->header.orientation = glm::normalize(follow_entity->header.orientation);
        }

        app->camera.orientation = glm::normalize(app->camera.orientation);
        app->shadow_camera.orientation = glm::normalize(app->shadow_camera.orientation);

        quat orientation = follow_entity->header.orientation;

        vec3 forward = get_forward(app->camera.orientation);
        vec3 right = glm::normalize(glm::cross(forward, vec3(0.0, 1.0, 0.0)));

        vec3 movement;

        if (input.up) {
          movement += forward;
        }

        if (input.down) {
          movement -= forward;
        }

        if (input.left) {
          movement -= right;
        }

        if (input.right) {
          movement += right;
        }

        float speed = 900.0f;

        if (input.alt && input.once.enter) {
          platform.toggle_fullscreen();
        }

        if (app->editing_mode) {
          speed = app->editor.speed;
        } else {
          if (input.shift) {
            speed = 10000.0f;
          }
          movement.y = 0;
        }

        if (glm::length(movement) > 0.0f) {
          movement = glm::normalize(movement);
        }

        for (auto it = array::begin(app->entities); it != array::end(app->entities); it++) {
          if (it->header.type == EntityType::EntityParticleEmitter) {
            EntityParticleEmitter *emitter = (EntityParticleEmitter *)it;

            Particle *particle = app->particles + app->next_particle++;
            if (app->next_particle >= array_count(app->particles)) {
              app->next_particle = 0;
            }

            particle->position = get_world_position(emitter->header.position);
            particle->color = emitter->initial_color;
            particle->size = emitter->particle_size;
            particle->velocity = vec3(get_random_float_between(-500.0f, 500.0f), get_random_float_between(0.0f, 1000.0f), get_random_float_between(-500.0f, 500.0f));
            particle->gravity = emitter->gravity;

          } else if (it->header.type == EntityType::EntityPlayer) {
            it->header.velocity += movement * speed * input.delta_time;
            it->header.velocity = glm::mix(it->header.velocity, vec3(0.0f), input.delta_time * 10.0f);

            it->header.position = add_offset(it->header.position, it->header.velocity * input.delta_time);
          }

          update_world_position(&it->header.position);
        }

        {
          PROFILE_BLOCK("Update particles");
          for (u32 i=0; i<array_count(app->particles); i++) {
            Particle *particle = app->particles + i;

            particle->velocity.y += particle->gravity * input.delta_time;
            particle->position += particle->velocity * input.delta_time;
            particle->size = std::max(particle->size - 60.0f * input.delta_time, 0.0f);
            particle->velocity = glm::mix(particle->velocity, vec3(0.0f), input.delta_time);
            particle->color.a = glm::mix(particle->color.a, 0.0f, input.delta_time * 4.0f);
          }

          std::sort(app->particles, app->particles + array_count(app->particles), sort_particles_by_distance);

          for (u32 i=0; i<array_count(app->particles); i++) {
            Particle *particle = app->particles + i;

            app->particle_positions[i] = vec4(vec3(particle->position), particle->size);
            app->particle_colors[i] = particle->color;
          }
        }

        vec3 player_position = get_world_position(follow_entity->header.position);
        if (!app->editing_mode) {
          follow_entity->header.position.offset_.y = get_terrain_height_at(player_position.x, player_position.z);
        } else {
          float terrain = get_terrain_height_at(player_position.x, player_position.z);
          if (player_position.y < terrain) {
            follow_entity->header.position.offset_.y = terrain;
          }
        }

        app->camera.orientation = follow_entity->header.orientation;
        app->camera.position = add_offset(follow_entity->header.position, vec3(0.0f, 70.0f, 0.0f));

        app->camera.view_matrix = get_camera_projection(&app->camera);
        app->camera.view_matrix *= glm::toMat4(app->camera.orientation);
        app->camera.view_matrix = glm::translate(app->camera.view_matrix, (get_world_position(app->camera.position) * -1.0f));

        fill_frustum_with_matrix(&app->camera.frustum, app->camera.view_matrix);

        Ray ray = get_mouse_ray(app, input, memory);

        Entity *closest_entity = NULL;
        float closest_distance = FLT_MAX;
        vec3 hit_position;

        for (auto it = array::begin(app->entities); it != array::end(app->entities); it++) {
          if (!(it->header.flags & EntityFlags::HIDE_IN_EDITOR)) {
            RayMatchResult hit;
            hit.hit = false;

            if (it->header.model && it->header.model->state == AssetState::INITIALIZED) {
              hit = ray_match_entity(app, ray, it);
            } else {
              hit = ray_match_sphere(ray, get_world_position(it->header.position), app->editor.handle_size);
            }

            if (hit.hit) {
              float distance = glm::distance(hit.hit_position, ray.start);
              if (distance < closest_distance) {
                closest_distance = distance;
                closest_entity = it;
                hit_position = hit.hit_position;
              }
            }
          }
        }

        if (closest_entity) {
          app->editor.hover_entity = closest_entity->header.id;
          app->editor.hovering_entity = true;
        } else {
          app->editor.hovering_entity = false;
        }

        if (app->editing_mode && input.mouse_click) {
          if (closest_entity) {
            if (input.shift) {
              Entity new_entity;
              memcpy(&new_entity, closest_entity, sizeof(Entity));

              // TODO(sedivy): Don't hard code unique properties
              if (new_entity.header.type == EntityType::EntityGrass) {
                EntityGrass *grass = (EntityGrass *)&new_entity;
                grass->positions = (vec4 *)malloc(sizeof(vec4) * MAX_GRASS_GROUP_COUNT);
                grass->rotations = (vec3 *)malloc(sizeof(vec3) * MAX_GRASS_GROUP_COUNT);
                grass->tints = (vec3 *)malloc(sizeof(vec3) * MAX_GRASS_GROUP_COUNT);
                grass->grass_count = 0;
                grass->initialized = false;
                grass->reload_data = true;
              }

              new_entity.header.id = next_entity_id(app);
              array::push_back(app->entities, new_entity);
              closest_entity = &app->entities[app->entities.size - 1];
            }

            app->editor.entity_id = closest_entity->header.id;
            app->editor.holding_entity = true;
            app->editor.distance_from_entity_offset = closest_distance;
            app->editor.hold_offset = hit_position - get_world_position(closest_entity->header.position);
            app->editor.inspect_entity = true;
          } else {
            app->editor.inspect_entity = false;
          }
        }

        {
          vec3 forward = get_forward(app->shadow_camera.orientation);

          app->shadow_camera.position = add_offset(app->camera.position, glm::normalize(forward) * ((app->shadow_camera.far - app->shadow_camera.near) / 2.0f * -1.0f));
          app->shadow_camera.view_matrix = get_camera_projection(&app->shadow_camera);
          app->shadow_camera.view_matrix *= glm::toMat4(app->shadow_camera.orientation);
          app->shadow_camera.view_matrix = glm::translate(app->shadow_camera.view_matrix, (get_world_position(app->shadow_camera.position) * -1.0f));

          fill_frustum_with_matrix(&app->shadow_camera.frustum, app->shadow_camera.view_matrix);

          if (app->editing_mode && app->editor.show_camera_frustum) {
            debug_render_frustum(app, &app->shadow_camera);
          }
        }

        /* debug_render_frustum(app, &app->camera); */

        if (app->editing_mode) {
          if (input.right_mouse_down) {
            if (!input.is_mouse_locked) {
              platform.lock_mouse();
            }
          } else {
            if (input.is_mouse_locked) {
              platform.unlock_mouse();
            }
          }

          if (!input.left_mouse_down) {
            app->editor.holding_entity = false;
          }

          if (app->editor.holding_entity) {
            Entity *entity = get_entity_by_id(app, app->editor.entity_id);

            Ray ray = get_mouse_ray(app, input, memory);

            if ((entity->header.flags & EntityFlags::MOUNT_TO_TERRAIN) != 0) {
              if (app->editor.experimental_terrain_entity_movement) {
                RayMatchResult hit = ray_match_terrain(app, ray);
                if (hit.hit) {
                  entity->header.position = make_position(hit.hit_position);
                }
              } else {
                entity->header.position = make_position((ray.start + ray.direction * app->editor.distance_from_entity_offset) - app->editor.hold_offset);
              }
              mount_entity_to_terrain(entity);
            } else {
              entity->header.position = add_offset(add_offset(app->camera.position, ray.direction * app->editor.distance_from_entity_offset), -app->editor.hold_offset);
            }
          }
        } else {
          if (input.mouse_click) {
            platform.lock_mouse();
          }

          if (input.escape) {
            platform.unlock_mouse();
          }
        }
      }
    }

    // NOTE(sedivy): render
    {
      {
        PROFILE_BLOCK("Draw");

        {
          PROFILE_BLOCK("Draw Shadows");
          glBindFramebuffer(GL_FRAMEBUFFER, app->shadow_buffer);
          glViewport(0, 0, app->shadow_width, app->shadow_height);
          glClear(GL_DEPTH_BUFFER_BIT);

          glEnable(GL_DEPTH_TEST);

          render_scene(memory, app, &app->shadow_camera, &app->record_depth_program, true);

          glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, app->frames[0].id);
        glViewport(0, 0, app->frames[0].width, app->frames[0].height);
        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render_skybox(app);

        {
          PROFILE_BLOCK("Draw Main");

          render_terrain(memory, app);
          render_scene(memory, app, &app->camera);
          glBindFramebuffer(GL_FRAMEBUFFER, app->frames[0].id);

          // NOTE(sedivy): particles
          {
            PROFILE_BLOCK("Draw Particles");
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glDepthMask(GL_FALSE);

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);

            use_program(app, &app->particle_program);

            set_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);
            set_uniform(app->current_program, "camera_up", vec3(app->camera.view_matrix[0][1], app->camera.view_matrix[1][1], app->camera.view_matrix[2][1]));
            set_uniform(app->current_program, "camera_right", vec3(app->camera.view_matrix[0][0], app->camera.view_matrix[1][0], app->camera.view_matrix[2][0]));

            glBindBuffer(GL_ARRAY_BUFFER, app->particle_buffer);
            glBufferData(GL_ARRAY_BUFFER, array_count(app->particles) * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, array_count(app->particles) * 4 * sizeof(GLfloat), &app->particle_positions);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 4, GL_FLOAT, GL_FALSE, 0, 0);

            glBindBuffer(GL_ARRAY_BUFFER, app->particle_color_buffer);
            glBufferData(GL_ARRAY_BUFFER, array_count(app->particles) * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, array_count(app->particles) * 4 * sizeof(GLfloat), &app->particle_colors);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "color"), 4, GL_FLOAT, GL_FALSE, 0, 0);

            glBindBuffer(GL_ARRAY_BUFFER, app->particle_model);
            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "data"), 3, GL_FLOAT, GL_FALSE, 0, 0);

            glVertexAttribDivisor(0, 0);
            glVertexAttribDivisor(1, 1);
            glVertexAttribDivisor(2, 1);

            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, array_count(app->particles));

            glVertexAttribDivisor(0, 0);
            glVertexAttribDivisor(1, 0);
            glVertexAttribDivisor(2, 0);

            glDisable(GL_BLEND);
            glDisable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
          }

          {
            if (app->debug_lines.size > 0) {
              PROFILE_BLOCK("Draw Debug Lines");
              use_program(app, &app->debug_program);
              glBindBuffer(GL_ARRAY_BUFFER, app->debug_buffer);
              glBufferData(GL_ARRAY_BUFFER, app->debug_lines.size * sizeof(vec3)*2, NULL, GL_STREAM_DRAW);
              glBufferSubData(GL_ARRAY_BUFFER, 0, app->debug_lines.size * sizeof(vec3)*2, &app->debug_lines[0]);
              set_uniform(app->current_program, "uPMatrix", app->camera.view_matrix);
              glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
              glDrawArrays(GL_LINES, 0, app->debug_lines.size);
              array::clear(app->debug_lines);
            }
          }

          if (app->editing_mode) {
            draw_3d_debug_info(input, app);
          }
        }

        {
          PROFILE_BLOCK("Draw Final");
          glViewport(0, 0, app->frames[0].width, app->frames[0].height);
          glDisable(GL_DEPTH_TEST);
          glDisable(GL_CULL_FACE);

          app->read_frame = 0;
          app->write_frame = 1;

          glActiveTexture(GL_TEXTURE0 + 0);
          glBindTexture(GL_TEXTURE_2D, app->frames[app->read_frame].texture);

          glActiveTexture(GL_TEXTURE0 + 1);
          glBindTexture(GL_TEXTURE_2D, app->frames[app->write_frame].texture);

          glBindBuffer(GL_ARRAY_BUFFER, app->fullscreen_quad);

          {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_program);

            glActiveTexture(GL_TEXTURE0 + 2);
            glBindTexture(GL_TEXTURE_2D, app->color_correction_texture.id);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          if (app->color_correction) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_color_program);

            glActiveTexture(GL_TEXTURE0 + 2);
            glBindTexture(GL_TEXTURE_2D, app->color_correction_texture.id);

            set_uniformi(app->current_program, "uSampler", app->read_frame);
            set_uniformi(app->current_program, "color_correction_texture", 2);
            set_uniformf(app->current_program, "lut_size", 16.0f);

            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          if (app->antialiasing) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_fxaa_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);
            set_uniform(app->current_program, "texture_size", vec2(app->frames[0].width, app->frames[0].height));

            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          if (app->bloom) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_bloom_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          if (app->hdr) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_hdr_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          if (app->lens_flare) {
            glBindFramebuffer(GL_FRAMEBUFFER, app->frames[app->write_frame].id);
            use_program(app, &app->fullscreen_lens_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::swap(app->write_frame, app->read_frame);
          }

          {
            glViewport(0, 0, memory->width, memory->height);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            use_program(app, &app->fullscreen_program);

            set_uniformi(app->current_program, "uSampler", app->read_frame);

            glVertexAttribPointer(shader_get_attribute_location(app->current_program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
          }
        }
      }

      if (app->editing_mode) {
        flush_2d_render(app, memory);
      }
    }
  }

  memcpy(global_last_frame_counters, global_counters, sizeof(global_counters));
  for (u32 i=0; i<array_count(global_counters); i++) {
    DebugCounter *counter = global_counters + i;

    if (counter->hit_count) {
      counter->cycle_count = 0;
      counter->hit_count = 0;
    }
  }

  u64 diff = platform.get_time();
  if (diff >= app->frametimelast + 1000) {
    app->fps = app->framecount;
    app->framecount = 0;
    app->frametimelast = diff;
  } else {
    app->framecount += 1;
  }
}
