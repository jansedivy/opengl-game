#pragma once

#include <time.h>

struct Random {
  int start;
  int max_random;
};

Random create_random_sequence() {
  Random result;

  result.start = time(NULL);
  result.max_random = 2147483647;

  return result;
}

inline int get_next_integer(Random *random) {
  int result = (16807 * random->start) % random->max_random;
  random->start = result;
  return result;
}

inline float get_next_float(Random *random) {
  int result = get_next_integer(random);
  return (float)result / (float)random->max_random;
}

inline float get_random_float_between(float min, float max) {
  float random = ((float)rand()) / (float)RAND_MAX;
  float diff = max - min;
  float r = random * diff;
  return min + r;
}
