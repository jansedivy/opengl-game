
void acquire_asset_file(char *path) {
#if INTERNAL
  PROFILE_BLOCK("Acquiring Asset");
  char *original_file_path = join_string(debug_global_memory->debug_assets_path, path);

  u64 original_time = platform.get_file_time(original_file_path);
  u64 used_time = platform.get_file_time(path);

  if (original_time > used_time) {
    DebugReadFileResult result = platform.debug_read_entire_file(original_file_path);

    PlatformFile file = platform.open_file(path, "w+");
    platform.write_to_file(file, result.fileSize, result.contents);
    platform.close_file(file);

    platform.debug_free_file(result);
  }

  free(original_file_path);
#endif
}

