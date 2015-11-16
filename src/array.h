template<typename T> class Array {
  public:
    Array() : capacity(0), size(0), data(0) {}
    ~Array() {
      free(data);
    }

    T &operator[](u32 index);
    const T &operator[](u32 index) const;

    u32 capacity;
    u32 size;
    T *data;
};

template<typename T> T* begin(Array<T> &array) {
  return array.data;
}

template<typename T> const T* begin(const Array<T> &array) {
  return array.data;
}

template<typename T> T* end(Array<T> &array) {
  return array.data + array.size;
}

template<typename T> const T* end(const Array<T> &array) {
  return array.data + array.size;
}

template<typename T> T &Array<T>::operator[](u32 index) {
  return data[index];
};

template<typename T> const T &Array<T>::operator[](u32 index) const {
  return data[index];
};

template<typename T> void grow_array(Array<T> &array) {
  u32 new_capacity = array.capacity * 2 + 8;

  T *data = (T *)malloc(sizeof(T) * new_capacity);
  memcpy(data, array.data, sizeof(T) * array.size);
  free(array.data);

  array.data = data;
  array.capacity = new_capacity;
}

template<typename T> void push_back(Array<T> &array, T value) {
  if (array.size + 1 > array.capacity) {
    grow_array(array);
  }

  array.data[array.size++] = value;
}
