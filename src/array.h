#pragma once

template<typename T>
class Array {
  public:
    void init() {
      count = 0;
      max_size = 1;
      buffer = static_cast<T*>(malloc(sizeof(T)));
    }

    ~Array() {
      free(buffer);
    }

    inline void add(T value) {
      printf("OMG\n");
      if (count >= max_size) {
        max_size *= 2;
        buffer = static_cast<T*>(realloc(buffer, max_size));
      }

      *(buffer + count) = value;
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
