
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <SDL2/SDL.h>

#include <dirent.h>
#include <cstdio>
#include <sys/stat.h>

#include "app.h"

SDL_Window *window;
int x, y;

struct AppCode {
  TickType* tick;
  InitType* init;
  QuitType* quit;

  u64 last_time_write;

  const char *path;
  void* library;
};

u64 get_file_time(char *path) {
  u64 result = 0;

  struct stat file_stat;

  if (stat(path, &file_stat) == 0) {
    result = file_stat.st_mtime;
  }

  return result;
}

AppCode load_app_code() {
  AppCode result;

  // TODO(sedivy): move string into a constant
  const char *path = "app.dylib";

  void *lib = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
  if (!lib) {
    fprintf(stderr, "%s\n", dlerror());
  }

  result.library = lib;
  result.last_time_write = get_file_time((char *)path);
  result.path = path;

  result.init = (InitType*)dlsym(lib, "init");
  result.tick = (TickType*)dlsym(lib, "tick");
  result.quit = (InitType*)dlsym(lib, "quit");

  return result;
}

void unload_app_code(AppCode *code) {
  dlclose(code->library);
}

void debug_free_file(DebugReadFileResult file) {
  if (file.contents) {
    free(file.contents);
  }
}

DebugReadFileResult debug_read_entire_file(const char *path) {
  DebugReadFileResult result = {0};

  FILE *file = fopen(path, "rb");

  if (file != NULL) {
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(size);

    if (buffer != NULL) {
      size_t bytes_read = fread(buffer, 1, size, file);
      if (bytes_read == size) {
        fclose(file);
        result.fileSize = size;
        result.contents = buffer;
        return result;
      } else {
        free(buffer);
        fclose(file);
        printf("Error reading file %s\n", path);
        return result;
      }
    } else {
      printf("Error reading file %s\n", path);
      fclose(file);
      return result;
    }
  } else {
    printf("Error reading file %s\n", path);
    fclose(file);
    return result;
  }

  return result;
}

struct WorkEntry {
  PlatformWorkQueueCallback *callback;
  void *data;
};

struct Queue {
  u32 volatile next_entry_to_write;
  u32 volatile next_entry_to_read;

  u32 volatile completion_count;
  u32 volatile completion_goal;

  WorkEntry entries[512];

  SDL_sem *semaphore;
};

bool do_queue_work(Queue *queue) {
  bool sleep = false;

  int original_next_index = queue->next_entry_to_read;
  int new_next_index = (original_next_index + 1) % array_count(queue->entries);

  if (original_next_index != queue->next_entry_to_write) {
    SDL_bool value = SDL_AtomicCAS((SDL_atomic_t *)&queue->next_entry_to_read, original_next_index, new_next_index);

    if (value) {
      WorkEntry *entry = queue->entries + original_next_index;
      entry->callback(entry->data);

      SDL_AtomicIncRef((SDL_atomic_t *)&queue->completion_count);
    }
  } else {
    sleep = true;
  }

  return sleep;
}

static int thread_function(void *data) {
  Queue *queue = (Queue *)data;

  while (true) {
    if (do_queue_work(queue)) {
      SDL_SemWait(queue->semaphore);
    }
  }
}

void add_work(Queue *queue, PlatformWorkQueueCallback *callback, void *data) {
  u32 new_next_entry_to_write = (queue->next_entry_to_write + 1) % array_count(queue->entries);

  assert(new_next_entry_to_write != queue->next_entry_to_read);

  WorkEntry *entry = queue->entries + queue->next_entry_to_write;

  entry->callback = callback;
  entry->data = data;

  queue->completion_goal += 1;

  SDL_CompilerBarrier();

  queue->next_entry_to_write = new_next_entry_to_write;
  SDL_SemPost(queue->semaphore);
}

void complete_all_work(Queue *queue) {
/*   while (queue->completed != queue->count) { */
/*     do_queue_work(queue); */
/*   } */

/*   queue->count = 0; */
/*   queue->completed = 0; */
/*   queue->next_entry_to_read = 0; */
}

bool queue_has_free_spot(Queue *queue) {
  return (array_count(queue->entries) - (queue->completion_goal - queue->completion_count)) > 1;
}

u32 get_time() {
  return SDL_GetTicks();
}

u64 get_performance_counter() {
  return SDL_GetPerformanceCounter();
}

u64 get_performance_frequency() {
  return SDL_GetPerformanceFrequency();
}

void delay(u32 time) {
  SDL_Delay(time);
}

void lock_mouse() {
  SDL_GetMouseState(&x, &y);

  SDL_SetRelativeMouseMode(SDL_TRUE);

  int x, y;
  SDL_GetRelativeMouseState(&x, &y);
}

void unlock_mouse() {
  if (SDL_GetRelativeMouseMode()) {
    SDL_SetRelativeMouseMode(SDL_FALSE);

    SDL_WarpMouseInWindow(window, x, y);
  }
}

PlatformDirectory open_directory(const char *path) {
  PlatformDirectory result;

  result.platform = (void *)opendir(path);

  return result;
}

PlatformDirectoryEntry read_next_directory_entry(PlatformDirectory directory) {
  PlatformDirectoryEntry result;

  DIR *handle = (DIR *)directory.platform;
  if (handle) {
    dirent *entry = readdir(handle);
    result.platform = entry;
    if (entry) {
      result.name = entry->d_name;
      result.empty = false;
    } else {
      result.empty = true;
    }
  } else {
    result.empty = true;
  }

  return result;
}

bool is_directory_entry_file(PlatformDirectoryEntry directory) {
  return static_cast<dirent *>(directory.platform)->d_type == 0x8;
}

PlatformFile open_file(char *path, const char *flags) {
  PlatformFile result;

  result.platform = fopen(path, flags);
  result.error = result.platform == NULL;

  return result;
}

void close_file(PlatformFile file) {
  fclose(static_cast<FILE *>(file.platform));
}

void write_to_file(PlatformFile file, u64 len, void *value) {
  fwrite(value, 1, len, static_cast<FILE *>(file.platform));
}

void create_directory(char *path) {
  mkdir(path, 0777);
}

inline void format_string(char* buf, int buf_size, const char* fmt, va_list args) {
  int val = vsnprintf(buf, buf_size, fmt, args);
  if (val == -1 || val >= buf_size) {
    buf[buf_size-1] = '\0';
  }
}

bool atomic_exchange(u32 volatile *atomic, u32 old_value, u32 new_value) {
  return (bool)SDL_AtomicCAS((SDL_atomic_t *)atomic, old_value, new_value);
}

void toggle_fullscreen() {
  u32 flags = SDL_GetWindowFlags(window);

  if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
    SDL_SetWindowFullscreen(window, 0);
  } else {
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
  }
}

void message_box(const char *title, const char *format, ...) {
  static const int size = 512;
  char message[size];

  va_list args;
  va_start(args, format);
  format_string(message, size, format, args);
  va_end(args);

  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
}

PlatformFileLine read_file_line(PlatformFile file) {
  PlatformFileLine result;
  result.contents = NULL;

  if (getline(&result.contents, &result.length, static_cast<FILE *>(file.platform)) == -1) {
    result.empty = true;
  } else {
    result.empty = false;
  }

  return result;
}

void close_directory(PlatformDirectory directory) {
  closedir(static_cast<DIR *>(directory.platform));
}

int main() {
  Queue main_queue = {};
  main_queue.semaphore = SDL_CreateSemaphore(0);

  for (int i=0; i<SDL_GetCPUCount(); i++) {
    SDL_CreateThread(thread_function, "main_worker_thread", &main_queue);
  }

  Queue low_queue = {};
  low_queue.semaphore = SDL_CreateSemaphore(0);

  for (int i=0; i<3; i++) {
    SDL_CreateThread(thread_function, "low_worker_thread", &low_queue);
  }

  Memory memory;
  memory.width = 1280;
  memory.height = 720;
  memory.should_reload = false;
  memory.app = new App();

  PlatformAPI platform;
  platform.debug_read_entire_file = debug_read_entire_file;
  platform.debug_free_file = debug_free_file;
  platform.get_time = get_time;
  platform.get_performance_counter = get_performance_counter;
  platform.get_performance_frequency = get_performance_frequency;
  platform.delay = delay;
  platform.lock_mouse = lock_mouse;
  platform.unlock_mouse = unlock_mouse;
  platform.add_work = add_work;
  platform.complete_all_work = complete_all_work;
  platform.queue_has_free_spot = queue_has_free_spot;

  platform.open_directory = open_directory;
  platform.read_next_directory_entry = read_next_directory_entry;
  platform.is_directory_entry_file = is_directory_entry_file;
  platform.open_file = open_file;
  platform.close_file = close_file;
  platform.read_file_line = read_file_line;
  platform.close_directory = close_directory;
  platform.write_to_file = write_to_file;
  platform.create_directory = create_directory;
  platform.get_file_time = get_file_time;
  platform.message_box = message_box;
  platform.toggle_fullscreen = toggle_fullscreen;
  platform.atomic_exchange = atomic_exchange;

  memory.platform = platform;
  memory.low_queue = &low_queue;
  memory.main_queue = &main_queue;

#if INTERNAL
  memory.debug_assets_path = (char *)"../../../../";
  memory.debug_level_path = (char *)"../../../../assets/level";
#endif

  chdir(SDL_GetBasePath());

  AppCode code = load_app_code();

  SDL_Init(SDL_INIT_EVERYTHING);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  /* SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); */
  /* SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); */

  window = SDL_CreateWindow("Explore",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      memory.width, memory.height,
      SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

  SDL_GL_CreateContext(window);

  SDL_GL_SetSwapInterval(-1);
  /* SDL_GL_SetSwapInterval(0); */

  glClear(GL_COLOR_BUFFER_BIT);

  SDL_Event event;

  bool running = true;

  code.init(&memory);

  debug_global_memory = &memory;

  int original_mouse_down_x = 0;
  int original_mouse_down_y = 0;

  u32 last_time = get_time();

  while (running) {
    u32 now = get_time();
    float delta = (now - last_time) / 1000.0f;
    last_time = now;

    if (delta > (1.0f/60.0f)) {
      delta = (1.0f/60.0f);
    }

    SDL_GetWindowSize(window, &memory.width, &memory.height);

    Input input = {};

    input.delta_time = delta;

    if (get_file_time((char *)code.path) > code.last_time_write) {
      memory.should_reload = true;
      unload_app_code(&code);
      code = load_app_code();
    }

    SDL_GetMouseState(&input.mouse_x, &input.mouse_y);

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          running = false;
          break;
        case SDL_KEYDOWN:
          if (event.key.keysym.scancode == SDL_SCANCODE_P) {
            input.once.key_p = true;
          }
          if (event.key.keysym.scancode == SDL_SCANCODE_RETURN) {
            input.once.enter = true;
          }
          if (event.key.keysym.scancode == SDL_SCANCODE_R) {
            input.once.key_r = true;
          }
          if (event.key.keysym.scancode == SDL_SCANCODE_O) {
            input.once.key_o = true;
          }
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (event.button.button == SDL_BUTTON_LEFT) {
            input.mouse_click = true;
            original_mouse_down_x = input.mouse_x;
            original_mouse_down_y = input.mouse_y;
          }
          break;
        case SDL_MOUSEBUTTONUP:
          if (event.button.button == SDL_BUTTON_LEFT) {
            input.mouse_up = true;
          }
          break;
        case SDL_WINDOWEVENT:
          if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            unlock_mouse();
          }
          break;
      }
    }

    const u8 *state = SDL_GetKeyboardState(NULL);

    input.up = state[SDL_SCANCODE_W];
    input.left = state[SDL_SCANCODE_A];
    input.right = state[SDL_SCANCODE_D];
    input.down = state[SDL_SCANCODE_S];

    input.space = state[SDL_SCANCODE_SPACE];
    input.shift = state[SDL_SCANCODE_LSHIFT];
    input.alt = state[SDL_SCANCODE_LALT];
    input.key_r = state[SDL_SCANCODE_R];
    input.key_o = state[SDL_SCANCODE_O];
    input.escape = state[SDL_SCANCODE_ESCAPE];
    input.is_mouse_locked = SDL_GetRelativeMouseMode();

    input.original_mouse_down_x = original_mouse_down_x;
    input.original_mouse_down_y = original_mouse_down_y;

    u32 mouse_state = SDL_GetRelativeMouseState(&input.rel_mouse_x, &input.rel_mouse_y);

    input.right_mouse_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
    input.left_mouse_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);

    code.tick(&memory, input);

    SDL_GL_SwapWindow(window);
  }

  code.quit(&memory);

  return 1;
}
