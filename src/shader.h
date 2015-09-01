#pragma once

inline bool shader_has_attribute(Shader *shader, const char *name) {
  return shader->attributes.count(name);
}

inline GLuint shader_get_attribute_location(Shader *shader, const char *name) {
  assert(shader_has_attribute(shader, name));
  return shader->attributes[name];
}

void use_program(App* app, Shader *program) {
  if (program == app->current_program) { return; }
  if (app->current_program != NULL) {
    for (auto it = app->current_program->attributes.begin(); it != app->current_program->attributes.end(); it++) {
      glDisableVertexAttribArray(it->second);
    }
  }

  app->current_program = program;
  glUseProgram(program->id);

  for (auto it = program->attributes.begin(); it != program->attributes.end(); it++) {
    glEnableVertexAttribArray(it->second);
  }
}

inline bool shader_has_uniform(Shader *shader, const char *name) {
  return shader->uniforms.count(name);
}

inline GLuint shader_get_uniform_location(Shader *shader, const char *name) {
  assert(shader_has_uniform(shader, name));
  return shader->uniforms[name];
}

inline void set_uniform(Shader *shader, const char *name, glm::mat4 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniformMatrix4fv(location, 1, false, glm::value_ptr(value));
}

inline void set_uniform(Shader *shader, const char *name, glm::mat3 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniformMatrix3fv(location, 1, false, glm::value_ptr(value));
}

inline void set_uniformi(Shader *shader, const char *name, int value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform1i(location, value);
}

inline void set_uniformf(Shader *shader, const char *name, float value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform1f(location, value);
}

inline void set_uniform(Shader *shader, const char *name, glm::vec3 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform3fv(location, 1, glm::value_ptr(value));
}

inline void set_uniform(Shader *shader, const char *name, glm::vec4 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform4fv(location, 1, glm::value_ptr(value));
}

inline void set_uniform(Shader *shader, const char *name, glm::vec2 value) {
  GLint location = shader_get_uniform_location(shader, name);
  glUniform2fv(location, 1, glm::value_ptr(value));
}

void delete_shader(Shader *shader) {
  glDeleteProgram(shader->id);
  shader->initialized = false;
  shader->uniforms.clear();
  shader->attributes.clear();
}

Shader *create_shader(Shader *shader, const char *vert_filename, const char *frag_filename) {
  acquire_asset_file((char *)vert_filename);
  acquire_asset_file((char *)frag_filename);

  if (shader->initialized) {
    delete_shader(shader);
  }

  GLuint vertexShader;
  GLuint fragmentShader;

  {
    DebugReadFileResult vertex = platform.debug_read_entire_file(vert_filename);
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex.contents, (const GLint*)&vertex.fileSize);
    glCompileShader(vertexShader);

    GLint success;
    GLchar info_log[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

    if (!success) {
      glGetShaderInfoLog(vertexShader, sizeof(info_log), NULL, info_log);
      platform.message_box("Vertex Shader error", "\"%s\" %s", vert_filename, info_log);
      exit(0);
    }

    platform.debug_free_file(vertex);
  }

  {
    DebugReadFileResult fragment = platform.debug_read_entire_file(frag_filename);

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragment.contents, (const GLint*)&fragment.fileSize);
    glCompileShader(fragmentShader);

    GLint success;
    GLchar info_log[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

    if (!success) {
      glGetShaderInfoLog(fragmentShader, sizeof(info_log), NULL, info_log);
      platform.message_box("Fragment Shader error", "\"%s\" %s", frag_filename, info_log);
      exit(0);
    }
    platform.debug_free_file(fragment);
  }

  GLuint shaderProgram;
  shaderProgram = glCreateProgram();

  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  shader->id = shaderProgram;
  shader->initialized = true;

  glUseProgram(shaderProgram);

  int length, size, count;
  char name[256];
  GLenum type;
  glGetProgramiv(shaderProgram, GL_ACTIVE_UNIFORMS, &count);
  for (int i=0; i<count; i++) {
    glGetActiveUniform(shaderProgram, static_cast<GLuint>(i), sizeof(name), &length, &size, &type, name);
    shader->uniforms[name] = glGetUniformLocation(shaderProgram, name);
  }

  glGetProgramiv(shaderProgram, GL_ACTIVE_ATTRIBUTES, &count);
  for (int i=0; i<count; i++) {
    glGetActiveAttrib(shaderProgram, GLuint(i), sizeof(name), &length, &size, &type, name);
    shader->attributes[name] = glGetAttribLocation(shaderProgram, name);
  }

  glUseProgram(0);

  return shader;
}

