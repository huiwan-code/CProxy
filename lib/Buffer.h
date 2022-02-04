#pragma once
#include <iostream>
#include <memory>
class Buffer {
  public:
    // write_index_是可写的，所以长度为size的buffer，最多只能存储size-1的数据
    Buffer(int capacity = 8, int max_capacity = 1024)
    : capacity_(capacity),
      data_size_(capacity + 1) {
      max_capacity_ = max_capacity > capacity ? max_capacity : capacity;
      data_ = (char *)malloc(data_size_);
    }
    ~Buffer() {
      free(data_);
    }
    size_t read(char *data, int expect_len);
    size_t write_to_buffer(char *data, int expect_len);
    size_t write_to_sock(int fd);
    int get_unread_size();
  private:

    void ensure_insert(int insert_len);
    int get_free_size();
    void resize(int new_size);
    int capacity_;
    int data_size_;
    int max_capacity_;
    char *data_;
    int read_index_ = 0;
    int write_index_ = 0; // write_index_处可写
};

typedef std::shared_ptr<Buffer> SP_Buffer;