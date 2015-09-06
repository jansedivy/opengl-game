#pragma once

struct DebugProfileBlock {
  u32 id;
  u64 hit_count;

  DebugProfileBlock(u32 init_id, char *name, u64 init_hit_count=1) {
    id = init_id;
    hit_count = init_hit_count;
    global_counters[id].name = name;
    global_counters[id].cycle_count -= platform.get_performance_counter();
  }

  ~DebugProfileBlock() {
    global_counters[id].cycle_count += platform.get_performance_counter();
    global_counters[id].hit_count += hit_count;
  }
};

static u64 global_debug_counter = 0; // TODO(sedivy): remove

#define PROFILE_BLOCK(NAME, ...) DebugProfileBlock debug_profile_block_##__LINE__(__COUNTER__, (char *)NAME, ## __VA_ARGS__)
