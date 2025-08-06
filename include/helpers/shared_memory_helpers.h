#ifndef SHARED_MEMORY_HELPERS_H
#define SHARED_MEMORY_HELPERS_H

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <type_traits>

#ifndef SHARED_MEMORY_NAME
#define SHARED_MEMORY_NAME "/laser_demo_shared_memory"
#endif // SHARED_MEMORY_NAME

struct Frame
{
  CameraHelpers::YUYVFrame yuyvData;
  int frameNumber;
  double timestamp;
  bool ballDetected;
  double centerX;
  double centerY;
  double confidence;
  double targetX;
  double targetY;
};

template<typename T>
struct TripleBuffer {
  static_assert(std::is_default_constructible<T>::value, "T must be default-constructible");
  static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");

  std::mutex writer_mutex;
  std::mutex reader_mutex;
  std::atomic<int> spare_index{0};
  uint32_t flags[3] = {0, 0, 0};
  T buffers[3];
};

template<typename T>
class TripleBufferManager {
public:
  TripleBufferManager(TripleBuffer<T>* triple, bool is_writer = false)
    : triple_(triple), 
      index_(is_writer ? 1 : 2),
      is_writer_(is_writer)
  {
    lock_ = is_writer ? std::unique_lock<std::mutex>(triple_->writer_mutex, std::try_to_lock) 
                      : std::unique_lock<std::mutex>(triple_->reader_mutex, std::try_to_lock);
    if (!lock_.owns_lock()) {
      throw std::runtime_error("Failed to acquire lock on TripleBuffer");
    }
  }

  TripleBufferManager() = default;
  TripleBufferManager(const TripleBufferManager&) = delete;
  TripleBufferManager& operator=(const TripleBufferManager&) = delete;

  TripleBufferManager(TripleBufferManager&& other) noexcept
    : triple_(std::move(other.triple_)),
      lock_(std::move(other.lock_)),
      index_(other.index_),
      is_writer_(other.is_writer_)
  {
    other.index_ = -1;
  }

  ~TripleBufferManager() = default;

  void Initialize(TripleBuffer<T>* triple, bool is_writer = false) {
    triple_ = triple;
    index_ = is_writer ? 1 : 2;
    is_writer_ = is_writer;

    if (is_writer_) {
      lock_ = std::unique_lock<std::mutex>(triple_->writer_mutex, std::try_to_lock);
    } else {
      lock_ = std::unique_lock<std::mutex>(triple_->reader_mutex, std::try_to_lock);
    }

    if (!lock_.owns_lock()) {
      throw std::runtime_error("Failed to acquire lock on TripleBuffer");
    }
  }

  T* operator->() { return &triple_->buffers[index_]; }
  T& operator*()  { return triple_->buffers[index_]; }

  T* get() { return &triple_->buffers[index_]; }

  uint32_t& flags() { return triple_->flags[index_]; }

  void swap_buffers() {
    int old_spare = triple_->spare_index.exchange(index_, std::memory_order_acq_rel);
    index_ = old_spare;
  }

protected:
  TripleBuffer<T>* triple_;
  std::unique_lock<std::mutex> lock_;
  int index_;
  bool is_writer_ = false;
};

template<typename T>
class SharedMemoryTripleBuffer
{
public:
  SharedMemoryTripleBuffer(const std::string& name, bool is_writer = false)
    : name_(name), is_writer_(is_writer)
  {
    int flags = is_writer_ ? (O_CREAT | O_EXCL | O_RDWR) : O_RDWR;
    fd_ = shm_open(name_.c_str(), flags, 0666);
    if (fd_ == -1) {
      int err = errno;
      throw std::runtime_error("Failed to open shared memory segment " + name_ + ": " + std::string(strerror(err)));
    }

    size_t size = sizeof(TripleBuffer<T>);
    if (is_writer_) {
      if (ftruncate(fd_, size) == -1) {
        close(fd_);
        throw std::runtime_error("Failed to set size of shared memory segment");
      }
    }

    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    close(fd_);
    if (ptr == MAP_FAILED) {
      throw std::runtime_error("Failed to map shared memory segment");
    }

    triple_ = static_cast<TripleBuffer<T>*>(ptr);
    if (is_writer_) {
      new (triple_) TripleBuffer<T>();
    }
  }

  ~SharedMemoryTripleBuffer()
  {
    size_t size = sizeof(TripleBuffer<T>);
    munmap(triple_, size);
    if (is_writer_) {
      shm_unlink(name_.c_str());
    }
  }

  SharedMemoryTripleBuffer() = default;

  SharedMemoryTripleBuffer(const SharedMemoryTripleBuffer&) = delete;
  SharedMemoryTripleBuffer& operator=(const SharedMemoryTripleBuffer&) = delete;
  SharedMemoryTripleBuffer(SharedMemoryTripleBuffer&& other) noexcept
    : name_(std::move(other.name_)), 
      is_writer_(other.is_writer_), 
      fd_(other.fd_), 
      triple_(other.triple_)
  {
    other.fd_ = -1;
    other.triple_ = nullptr;
  }

  void Initialize(const std::string& name, bool is_writer = false)
  {
    name_ = name;
    is_writer_ = is_writer;

    int flags = is_writer_ ? (O_CREAT | O_EXCL | O_RDWR) : O_RDWR;
    fd_ = shm_open(name_.c_str(), flags, 0666);
    if (fd_ == -1) {
      int err = errno;
      throw std::runtime_error("Failed to open shared memory segment " + name_ + ": " + std::string(strerror(err)));
    }

    size_t size = sizeof(TripleBuffer<T>);
    if (is_writer_) {
      if (ftruncate(fd_, size) == -1) {
        close(fd_);
        throw std::runtime_error("Failed to set size of shared memory segment");
      }
    }

    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    close(fd_);
    if (ptr == MAP_FAILED) {
      throw std::runtime_error("Failed to map shared memory segment");
    }

    triple_ = static_cast<TripleBuffer<T>*>(ptr);
    if (is_writer_) {
      new (triple_) TripleBuffer<T>();
    }
  }

  TripleBuffer<T>* get() { return triple_; }

  TripleBuffer<T>* operator->() { return triple_; }
  TripleBuffer<T>& operator*()  { return *triple_; }

  private:
    std::string name_;
    bool is_writer_;
    int fd_;
    TripleBuffer<T>* triple_;
};

#endif // SHARED_MEMORY_HELPERS_H
