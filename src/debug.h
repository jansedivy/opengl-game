#pragma once

#define PROFILE(ID) u64 profile_cycle_count##ID = platform.get_performance_counter();
#define PROFILE_END(ID) debug_global_memory->counters[DebugCycleCounter_##ID].cycle_count += (platform.get_performance_counter()) - profile_cycle_count##ID; ++debug_global_memory->counters[DebugCycleCounter_##ID].hit_count;
#define PROFILE_END_COUNTED(ID, COUNT) debug_global_memory->counters[DebugCycleCounter_##ID].cycle_count += platform.get_performance_counter() - profile_cycle_count##ID; debug_global_memory->counters[DebugCycleCounter_##ID].hit_count += (COUNT);
