void load_texture(Texture *texture, int type=0) {
  if (platform.atomic_exchange(&texture->state, AssetState::EMPTY, AssetState::PROCESSING)) {
    PROFILE_BLOCK("Loading Model");
    acquire_asset_file((char *)texture->path);
    int channels;

    int width, height;
    u8* image = stbi_load(texture->path, &width, &height, &channels, type);

    texture->width = width;
    texture->height = height;

    texture->data = image;
    texture->state = AssetState::HAS_DATA;
  }
}

void load_texture_work(void *data) {
  load_texture(static_cast<Texture*>(data));
}

void initialize_texture(Texture *texture, GLenum interal_type=GL_RGB, GLenum type=GL_RGB, bool mipmap=true, GLenum wrap_type=GL_REPEAT) {
  if (platform.atomic_exchange(&texture->state, AssetState::HAS_DATA, AssetState::PROCESSING)) {
    glGenTextures(1, &texture->id);

    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexImage2D(GL_TEXTURE_2D, 0, interal_type, texture->width, texture->height, 0, type, GL_UNSIGNED_BYTE, texture->data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_type);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_type);

    if (mipmap) {
      float aniso = 0.0f;
      glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);

      glGenerateMipmap(GL_TEXTURE_2D);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    texture->state = AssetState::INITIALIZED;
  }
}

inline bool process_texture(Memory *memory, Texture *texture) {
  if (texture->state == AssetState::INITIALIZED) {
    return false;
  }

  if (platform.queue_has_free_spot(memory->low_queue)) {
    if (texture->state == AssetState::EMPTY) {
      platform.add_work(memory->low_queue, load_texture_work, texture);

      return true;
    }
  }

  if (texture->state == AssetState::HAS_DATA) {
    initialize_texture(texture, GL_RGBA, GL_RGBA);
    free(texture->data);
    texture->data = NULL;
    return false;
  }

  return true;
}

Texture* get_texture(App *app, char *name) {
  Texture *texture;
  if (app->textures.count(name)) {
    texture = app->textures.at(name);
    if (texture->state == AssetState::INITIALIZED) {
      return texture;
    }
  } else {
    texture = new Texture();
    texture->path = mprintf("assets/textures/%s", name);
    texture->short_name = allocate_string(name);
    app->textures[name] = texture;
  }

  process_texture(app->memory, texture);

  return texture;
}

void unload_texture(Texture *texture) {
  if (platform.atomic_exchange(&texture->state, AssetState::INITIALIZED, AssetState::PROCESSING)) {
    glDeleteTextures(1, &texture->id);

    if (texture->data) {
      stbi_image_free(texture->data);
      texture->data = NULL;
    }

    texture->state = AssetState::EMPTY;
  }
};
