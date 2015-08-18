#pragma once

template<typename T>
class Array {
  public:
    Array() {
      count = 0;
      max_size = 0;
      buffer = NULL;
    }

    ~Array() {
      free(buffer);
    }

    inline void add(T value) {
      if (buffer == NULL) {
        max_size = 1;
        buffer = static_cast<T *>(malloc(sizeof(T) * max_size));
      }

      if (count >= max_size) {
        u32 new_size = max_size * 2;
        buffer = static_cast<T *>(realloc(buffer, new_size));
        max_size = new_size;
      }

      buffer[count] = value;
      count += 1;
    }

    inline T &get(u32 index) {
      assert(index < count);
      return buffer[index];
    }

    u32 count;
    u32 max_size;

    T *buffer;
};
