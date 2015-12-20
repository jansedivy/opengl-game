#pragma once

struct Shader {
  GLuint id;
  std::unordered_map<std::string, GLint> uniforms;
  std::unordered_map<std::string, GLint> attributes;
  bool initialized = false;
};
