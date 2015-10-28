char *join_string(char *first, char *second) {
  char *result = static_cast<char *>(malloc(strlen(first) + strlen(second) + 1));

  strcpy(result, first);
  strcat(result, second);

  return result;
}

char *allocate_string(const char *string) {
  char *new_location = static_cast<char *>(malloc(strlen(string)));

  strcpy(new_location, string);

  return new_location;
}

const char *get_filename_ext(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename) return "";
  return dot + 1;
}

char *mprintf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char *data;
  vasprintf(&data, format, args);
  va_end(args);

  return data;
}
