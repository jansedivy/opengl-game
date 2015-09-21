void save_binary_level_file(LoadedLevel loaded_level) {
  LoadedLevelHeader header;
  header.entity_count = loaded_level.entities.size();

  PlatformFile file = platform.open_file((char *)"assets/level.level", "w");
  platform.write_to_file(file, sizeof(header), &header);
  for (auto it = loaded_level.entities.begin(); it != loaded_level.entities.end(); it++) {
    platform.write_to_file(file, sizeof(EntitySave), &(*it));
  }
  platform.close_file(file);
}

void save_text_level_file(Memory *memory, LoadedLevel level) {
#if INTERNAL
  platform.create_directory(memory->debug_level_path);

  for (auto it = level.entities.begin(); it != level.entities.end(); it++) {
    char full_path[256];
    sprintf(full_path, "%s/%d.entity", memory->debug_level_path, it->id);

    PlatformFile file = platform.open_file(full_path, "w");

    platform.print_to_file(file, "%d\n", it->id);
    platform.print_to_file(file, "type: %d\n", it->type);
    platform.print_to_file(file, "flags: %d\n", it->flags);
    platform.print_to_file(file, "position: %f, %f, %f\n", it->position.x, it->position.y, it->position.z);

    if (it->has_model) {
      platform.print_to_file(file, "model_name: %s\n", it->model_name);
    }
    platform.print_to_file(file, "scale: %f, %f, %f\n", it->scale.x, it->scale.y, it->scale.z);
    platform.print_to_file(file, "rotation: %f, %f, %f\n", it->rotation.x, it->rotation.y, it->rotation.z);
    platform.print_to_file(file, "color: %f, %f, %f, %f\n", it->color.x, it->color.y, it->color.z, it->color.w);

    platform.close_file(file);
  }
#endif
}

LoadedLevel load_binary_level_file() {
  LoadedLevel result;

  DebugReadFileResult file = platform.debug_read_entire_file("assets/level.level");
  SCOPE_EXIT(platform.debug_free_file(file));

  u8 *contents = (u8 *)file.contents;

  u32 read_position = 0;

  LoadedLevelHeader *header = (LoadedLevelHeader *)(contents + read_position);
  read_position += sizeof(LoadedLevelHeader);

  for (u32 i=0; i<header->entity_count; i++) {
    EntitySave *entity = (EntitySave *)(contents + read_position);
    read_position += sizeof(EntitySave);

    result.entities.push_back(*entity);
  }

  return result;
}

void save_level(Memory *memory, App *app) {
  LoadedLevel level;

  for (u32 i=0; i<app->entity_count; i++) {
    Entity *entity = app->entities + i;

    if (entity->header.flags & EntityFlags::PERMANENT_FLAG) {
      EntitySave save_entity = {};
      save_entity.id = entity->header.id;
      save_entity.type = entity->header.type;
      save_entity.position = entity->header.position;
      save_entity.scale = entity->header.scale;
      save_entity.rotation = entity->header.rotation;
      save_entity.color = entity->header.color;
      save_entity.flags = entity->header.flags;
      if (entity->header.model) {
        save_entity.has_model = true;
        strcpy(save_entity.model_name, entity->header.model->id_name);
      } else {
        save_entity.has_model = false;
      }
      level.entities.push_back(save_entity);
    }
  }

  save_binary_level_file(level);
  save_text_level_file(memory, level);
}

void load_debug_level(Memory *memory, App *app) {
#if INTERNAL
  bool changed_entity_files = false;

  {
    u64 level_time = platform.get_file_time((char *)"assets/level.level");;

    PlatformDirectory dir = platform.open_directory(memory->debug_level_path);

    while (dir.platform != NULL) {
      PlatformDirectoryEntry entry = platform.read_next_directory_entry(dir);
      if (entry.empty) { break; }

      if (platform.is_directory_entry_file(entry)) {
        char *full_path = mprintf("%s/%s", memory->debug_level_path, entry.name);
        SCOPE_EXIT(free(full_path));

        u64 entity_time = platform.get_file_time(full_path);
        if (entity_time > level_time) {
          changed_entity_files = true;
          break;
        }
      }
    }
  }

  if (changed_entity_files) {
    PlatformDirectory dir = platform.open_directory(memory->debug_level_path);

    LoadedLevel loaded_level;

    while (dir.platform != NULL) {
      PlatformDirectoryEntry entry = platform.read_next_directory_entry(dir);
      if (entry.empty) { break; }

      if (platform.is_directory_entry_file(entry)) {
        char *full_path = mprintf("%s/%s", memory->debug_level_path, entry.name);
        SCOPE_EXIT(free(full_path));

        if (strcmp(get_filename_ext(full_path), "entity") == 0) {
          PlatformFile file = platform.open_file(full_path, "r");
          if (!file.error) {

            EntitySave entity = {};
            entity.has_model = false;

            PlatformFileLine line = platform.read_file_line(file);
            if (!line.empty) {
              sscanf(line.contents, "%d", &entity.id);
            }

            while (true) {
              PlatformFileLine line = platform.read_file_line(file);
              if (line.empty) { break; }

              char property_name[256];
              u32 offset;

              sscanf(line.contents, " %[^ :] :%n", property_name, &offset);

              char *start = (char *)line.contents + offset;

              if (strcmp(property_name, "type") == 0) {
                sscanf(start, "%d", &entity.type);
              }

              if (strcmp(property_name, "flags") == 0) {
                sscanf(start, "%d", &entity.flags);
              } else if (strcmp(property_name, "position") == 0) {
                entity.position = read_vector(start);
              } else if (strcmp(property_name, "model_name") == 0) {
                entity.has_model = true;
                sscanf(start, "%s", entity.model_name);
              } else if (strcmp(property_name, "scale") == 0) {
                entity.scale = read_vector(start);
              } else if (strcmp(property_name, "rotation") == 0) {
                entity.rotation = read_vector(start);
              } else if (strcmp(property_name, "color") == 0) {
                entity.color = read_vector4(start);
              }
            }

            loaded_level.entities.push_back(entity);

            platform.close_file(file);
          }
        }
      }
    }

    save_binary_level_file(loaded_level);

    platform.close_directory(dir);
  }
#endif

  {
    LoadedLevel bin_loaded_level = load_binary_level_file();

    for (auto it = bin_loaded_level.entities.begin(); it != bin_loaded_level.entities.end(); it++) {
      Entity *entity = app->entities + app->entity_count++;

      deserialize_entity(app, &(*it), entity);

      if (entity->header.id > app->last_id) {
        app->last_id = entity->header.id;
      }
    }
  }
}