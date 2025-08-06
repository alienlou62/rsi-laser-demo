#ifndef SHARED_DATA_HELPERS_H
#define SHARED_DATA_HELPERS_H

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <type_traits>

namespace SharedDataHelpers
{
  // SPSC (Single Producer Single Consumer) storage for shared data
  template<typename ElementType>
  struct SPSCStorage {
    static_assert(std::is_default_constructible<ElementType>::value, "ElementType must be default-constructible");
    static_assert(std::is_trivially_copyable<ElementType>::value, "ElementType must be trivially copyable");

    using element_type = ElementType;

    std::mutex writer_mutex;
    std::mutex reader_mutex;
    std::atomic<int> spare_index{0};
    uint32_t flags[3] = {0, 0, 0};
    element_type elements[3];
  };

  // SPSC storage manager for accessing shared data in a concurrency-safe manner
  template<typename StoragePtr>
  class SPSCStorageManager {
  public:
    using storage_type = typename std::pointer_traits<StoragePtr>::element_type;
    using value_type = typename storage_type::element_type;

    SPSCStorageManager(StoragePtr storage, bool is_writer = false)
      : storage_(storage), 
        index_(is_writer ? 1 : 2),
        is_writer_(is_writer)
    {
      lock_ = is_writer ? std::unique_lock<std::mutex>(storage_->writer_mutex, std::try_to_lock) 
                        : std::unique_lock<std::mutex>(storage_->reader_mutex, std::try_to_lock);
      if (!lock_.owns_lock()) {
        throw std::runtime_error("Failed to acquire lock on SPSCStorage");
      }
    }

    SPSCStorageManager() = delete;
    SPSCStorageManager(const SPSCStorageManager&) = delete;
    SPSCStorageManager& operator=(const SPSCStorageManager&) = delete;

    SPSCStorageManager(SPSCStorageManager&& other) noexcept
      : storage_(std::move(other.storage_)),
        lock_(std::move(other.lock_)),
        index_(other.index_),
        is_writer_(other.is_writer_)
    {
      other.index_ = -1;
    }

    ~SPSCStorageManager() = default;

    value_type& data() { return storage_->elements[index_]; }
    uint32_t& flags() { return storage_->flags[index_]; }

    void exchange() {
      int old_spare = storage_->spare_index.exchange(index_, std::memory_order_acq_rel);
      index_ = old_spare;
    }

  protected:
    StoragePtr storage_;
    std::unique_lock<std::mutex> lock_;
    int index_;
    bool is_writer_ = false;
  };

  // Shared memory SPSC storage for cross-process communication
  template<typename T>
  class SharedMemorySPSCStorage
  {
  public:
    SharedMemorySPSCStorage(const std::string& name, bool is_writer = false)
      : name_(name), is_writer_(is_writer)
    {
      int flags = is_writer_ ? (O_CREAT | O_EXCL | O_RDWR) : O_RDWR;
      fd_ = shm_open(name_.c_str(), flags, 0666);
      if (fd_ == -1) {
        int err = errno;
        throw std::runtime_error("Failed to open shared memory segment " + name_ + ": " + std::string(strerror(err)));
      }

      size_t size = sizeof(SPSCStorage<T>);
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

      storage_ = static_cast<SPSCStorage<T>*>(ptr);
      if (is_writer_) {
        new (storage_) SPSCStorage<T>();
      }
    }

    ~SharedMemorySPSCStorage()
    {
      size_t size = sizeof(SPSCStorage<T>);
      munmap(storage_, size);
      if (is_writer_) {
        shm_unlink(name_.c_str());
      }
    }

    SharedMemorySPSCStorage() = delete;

    SharedMemorySPSCStorage(const SharedMemorySPSCStorage&) = delete;
    SharedMemorySPSCStorage& operator=(const SharedMemorySPSCStorage&) = delete;
    SharedMemorySPSCStorage(SharedMemorySPSCStorage&& other) noexcept
      : name_(std::move(other.name_)), 
        is_writer_(other.is_writer_), 
        fd_(other.fd_), 
        storage_(other.storage_)
    {
      other.fd_ = -1;
      other.storage_ = nullptr;
    }

    SPSCStorage<T>* get() { return storage_; }

    SPSCStorage<T>* operator->() { return storage_; }
    SPSCStorage<T>& operator*()  { return *storage_; }

    private:
      std::string name_;
      bool is_writer_;
      int fd_;
      SPSCStorage<T>* storage_;
  };
}

#endif // SHARED_DATA_HELPERS_H
