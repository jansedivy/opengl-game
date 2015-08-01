#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>

#include "app.h"
#include <OpenGl/gl.h>

#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "SDL2/SDL.h"
#include "platform.h"

#include "app.h"

struct AppCode {
  TickType* tick;
  InitType* init;

  time_t last_time_write;

  const char *path;
  void* library;
};

time_t get_last_write_time(const char *path) {
  time_t result = 0;

  struct stat FileStat;

  if (stat(path, &FileStat) == 0) {
    result = FileStat.st_mtimespec.tv_sec;
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
  result.last_time_write = get_last_write_time(path);
  result.path = path;

  result.init = (InitType*)dlsym(lib, "init");
  result.tick = (TickType*)dlsym(lib, "tick");

  return result;
}

void UnloadAppCode(AppCode *code) {
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

    char *buffer = (char *)malloc(size + 1);

    if (buffer != NULL) {
      size_t bytes_read = fread(buffer, sizeof(*buffer), size, file);
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

void delay(u32 time) {
  SDL_Delay(time);
}

void lock_mouse() {
  SDL_SetRelativeMouseMode(SDL_TRUE);

  int x, y;
  SDL_GetRelativeMouseState(&x, &y);
}

void unlock_mouse() {
  SDL_SetRelativeMouseMode(SDL_FALSE);
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
  memory.permanent_storage = malloc(sizeof(App));

  memory.platform.debug_read_entire_file = debug_read_entire_file;
  memory.platform.debug_free_file = debug_free_file;
  memory.platform.get_time = get_time;
  memory.platform.get_performance_counter = get_performance_counter;
  memory.platform.delay = delay;
  memory.platform.lock_mouse = lock_mouse;
  memory.platform.unlock_mouse = unlock_mouse;
  memory.platform.add_work = add_work;
  memory.platform.complete_all_work = complete_all_work;
  memory.platform.queue_has_free_spot = queue_has_free_spot;

  memory.low_queue = &low_queue;
  memory.main_queue = &main_queue;

  chdir(SDL_GetBasePath());

  AppCode code = load_app_code();

  SDL_Init(SDL_INIT_EVERYTHING);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  SDL_Window *window = SDL_CreateWindow("Explore",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      memory.width, memory.height,
      SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

  SDL_GL_CreateContext(window);

  SDL_GL_SetSwapInterval(-1);

  glClear(GL_COLOR_BUFFER_BIT);

  SDL_Event event;

  bool running = true;

  code.init(&memory);

  while (running) {
    Input input = {};

    if (get_last_write_time(code.path) > code.last_time_write) {
      memory.should_reload = true;
      UnloadAppCode(&code);
      code = load_app_code();
    }

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          running = false;
          break;
      case SDL_MOUSEBUTTONUP:
          break;
      case SDL_MOUSEBUTTONDOWN:
          input.mouse_click = true;
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
    input.escape = state[SDL_SCANCODE_ESCAPE];
    input.is_mouse_locked = SDL_GetRelativeMouseMode();

    SDL_GetRelativeMouseState(&input.mouseX, &input.mouseY);

    code.tick(&memory, input);

    SDL_GL_SwapWindow(window);
  }

  return 1;
}
