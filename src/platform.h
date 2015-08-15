#pragma once

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int32_t s32;
typedef int64_t s64;

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

extern "C" {
  struct InputOnce {
    bool key_p;
    bool key_r;
  };

  struct Input {
    int rel_mouse_x;
    int rel_mouse_y;

    int mouse_x;
    int mouse_y;

    bool up;
    bool left;
    bool right;
    bool down;

    bool key_r;
    bool key_o;

    InputOnce once;

    bool shift;
    bool escape;

    bool space;
    bool mouse_click;
    bool right_mouse_down;
    bool left_mouse_down;

    bool is_mouse_locked;
  };

  struct PlatformDirectory {
    void *platform;
  };

  struct PlatformDirectoryEntry {
    bool empty;
    char *name;

    void *platform;
  };

  struct PlatformFileLine {
    char *contents;
    size_t length;
    bool empty;
  };

  struct PlatformFile {
    void *platform;
    bool error;
  };

  struct DebugReadFileResult {
    u32 fileSize;
    char *contents;
  };

  struct LoadedBitmap {
    int width;
    int height;
    void *memory;
  };

  typedef DebugReadFileResult debugReadEntireFileType(const char *name);
  typedef void debugFreeFileType(DebugReadFileResult file);
  typedef void PlatformWorkQueueCallback(void *data);
  typedef void add_work_type(struct Queue *queue, PlatformWorkQueueCallback *callback, void *data);
  typedef void complete_all_work_type(struct Queue *queue);
  typedef bool queue_has_free_spot_type(struct Queue *queue);
  typedef u32 get_time_type();
  typedef u64 get_performance_counter_type();
  typedef u64 get_performance_frequency_type();
  typedef void delay_type(u32 time);
  typedef void lock_mouse_type();
  typedef void unlock_mouse_type();

  typedef PlatformDirectory open_directory_type(const char *path);
  typedef PlatformDirectoryEntry read_next_directory_entry_type(PlatformDirectory directory);
  typedef bool is_directory_entry_file_type(PlatformDirectoryEntry entry);
  typedef PlatformFile open_file_type(char *path, const char *flags);
  typedef void close_file_type(PlatformFile file);
  typedef PlatformFileLine read_file_line_type(PlatformFile file);
  typedef void close_directory_type(PlatformDirectory directory);
  typedef void write_to_file_type(PlatformFile file, char *text);
  typedef void create_directory_type(char *path);

  enum {
    /* 0  */DebugCycleCounter_update,
    /* 1  */DebugCycleCounter_render,
    /* 2  */DebugCycleCounter_render_shadows,
    /* 3  */DebugCycleCounter_render_skybox,
    /* 4  */DebugCycleCounter_render_debug,
    /* 5  */DebugCycleCounter_render_final,
    /* 6  */DebugCycleCounter_render_main,
    /* 7  */DebugCycleCounter_render_entities,
    /* 8  */DebugCycleCounter_render_chunks,
    /* 9  */DebugCycleCounter_render_ui,
    /* 10 */DebugCycleCounter_render_ui_flush,
    /* 12 */DebugCycleCounter_frame,
    /* 11 */DebugCycleCounter_count
  };

  struct DebugCounter {
    u64 cycle_count;
    u64 hit_count;
  };

  struct PlatformAPI {
    add_work_type *add_work;
    complete_all_work_type *complete_all_work;
    queue_has_free_spot_type *queue_has_free_spot;
    debugReadEntireFileType *debug_read_entire_file;
    debugFreeFileType *debug_free_file;
    get_time_type *get_time;
    get_performance_counter_type *get_performance_counter;
    get_performance_frequency_type *get_performance_frequency;
    delay_type *delay;
    lock_mouse_type *lock_mouse;
    unlock_mouse_type *unlock_mouse;

    open_directory_type *open_directory;
    read_next_directory_entry_type *read_next_directory_entry;
    is_directory_entry_file_type *is_directory_entry_file;
    open_file_type *open_file;
    write_to_file_type *write_to_file;
    create_directory_type *create_directory;
    close_file_type *close_file;
    read_file_line_type *read_file_line;
    close_directory_type *close_directory;
  };

  struct Memory {
    bool should_reload;

    int width;
    int height;

    void *permanent_storage;

    Queue *main_queue;
    Queue *low_queue;

    PlatformAPI platform;

    DebugCounter last_frame_counters[DebugCycleCounter_count];
    DebugCounter counters[DebugCycleCounter_count];
  };

  void tick(Memory *memory, Input input);
  typedef void TickType(Memory *memory, Input input);

  void init(Memory *memory);
  typedef void InitType(Memory *memory);

  void quit(Memory *memory);
  typedef void QuitType(Memory *memory);
}
