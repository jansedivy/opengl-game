#pragma once

struct Shader {
  GLuint id;
  std::unordered_map<std::string, GLuint> uniforms;
  std::unordered_map<std::string, GLuint> attributes;
  bool initialized = false;
};
