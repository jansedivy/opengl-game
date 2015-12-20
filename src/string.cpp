char *join_string(char *first, char *second) {
  char *result = (char *)malloc(strlen(first) + strlen(second) + 1);

  strcpy(result, first);
  strcat(result, second);

  return result;
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

char *allocate_string(const char *string) {
  return mprintf("%s", string);
}
