#include <stdio.h>

#include <SDL.h>

#include <cstdio>

#include "app.h"
#include <dirent.h>
#include <direct.h>

#include <windows.h>
#include <string>

#include "app.h"

struct AppCode {
  TickType* tick;
  InitType* init;
  QuitType* quit;

  const char *path;
  HMODULE library;
};


AppCode load_app_code() {
  AppCode result;

  // TODO(sedivy): move string into a constant
  const char *path = "app.dll";

  printf("%s\n", path);

  HMODULE lib = LoadLibraryA(path);

  result.library = lib;
  result.path = path;

  result.init = (InitType*)GetProcAddress(lib, "init");
  result.tick = (TickType*)GetProcAddress(lib, "tick");
  result.quit = (InitType*)GetProcAddress(lib, "quit");

  return result;
}

void UnloadAppCode(AppCode *code) {
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

    char *buffer = (char *)malloc(size + 1);

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

SDL_Window *window;
int x, y;

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

  result.platform = opendir(path);

  return result;
}

PlatformDirectoryEntry read_next_directory_entry(PlatformDirectory directory) {
  PlatformDirectoryEntry result;

  dirent *entry = readdir(static_cast<DIR *>(directory.platform));
  result.platform = entry;
  if (entry) {
    result.name = entry->d_name;
    result.empty = false;
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
  _mkdir(path);
}

PlatformFileLine read_file_line(PlatformFile file) {
  PlatformFileLine result;
  result.contents = NULL;

  /*
  if (std::getline(&result.contents, &result.length, static_cast<FILE *>(file.platform)) == -1) {
    result.empty = true;
  } else {
    result.empty = false;
  }
  */

  return result;
}

void close_directory(PlatformDirectory directory) {
  closedir(static_cast<DIR *>(directory.platform));
}

int WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nCmdShow
) {
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

  memory.platform = platform;
  memory.low_queue = &low_queue;
  memory.main_queue = &main_queue;

#if INTERNAL
  memory.debug_assets_path = (char *)"../../";
  memory.debug_level_path = (char *)"../../assets/level";
#endif

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

  glClear(GL_COLOR_BUFFER_BIT);

  SDL_Event event;

  bool running = true;

  code.init(&memory);

  debug_global_memory = &memory;

  int original_mouse_down_x = 0;
  int original_mouse_down_y = 0;

  u32 last_time = get_time();

  while (running) {
    PROFILE(tick);

    u32 now = get_time();
    float delta = (now - last_time) / 1000.0f;
    last_time = now;

    SDL_GetWindowSize(window, &memory.width, &memory.height);

    Input input = {};

    input.delta_time = delta;

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
    PROFILE_END(tick);
  }

  code.quit(&memory);

  return 1;
}
