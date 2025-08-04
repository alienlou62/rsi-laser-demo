#ifndef CAMERA_GRPC_SERVER_H
#define CAMERA_GRPC_SERVER_H

#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <grpcpp/grpcpp.h>

// Forward declarations
namespace RSI::RapidCode::RealTimeTasks {
    class RTTaskManager;
}

namespace CameraGrpcServer {

class CameraStreamServer {
public:
    CameraStreamServer();
    ~CameraStreamServer();

    // Start the gRPC server on specified port
    bool Start(const std::string& server_address, RSI::RapidCode::RealTimeTasks::RTTaskManager* taskManager);
    
    // Stop the gRPC server
    void Stop();
    
    // Check if server is running
    bool IsRunning() const { return m_isRunning.load(); }

private:
    std::unique_ptr<grpc::Server> m_server;
    std::atomic<bool> m_isRunning{false};
    RSI::RapidCode::RealTimeTasks::RTTaskManager* m_taskManager;
    
    void RunServer();
};

// Shared memory management for image data
class ImageBuffer {
public:
    static ImageBuffer& GetInstance();
    
    bool Initialize(size_t bufferSize);
    void Cleanup();
    
    // Thread-safe image operations
    bool StoreImage(const void* imageData, size_t size, uint32_t sequenceNumber);
    bool GetLatestImage(void* buffer, size_t& size, uint32_t& sequenceNumber) const;
    
    size_t GetBufferSize() const { return m_bufferSize; }
    bool IsInitialized() const { return m_buffer != nullptr; }

private:
    ImageBuffer() = default;
    ~ImageBuffer() { Cleanup(); }
    
    mutable std::mutex m_mutex;
    void* m_buffer = nullptr;
    size_t m_bufferSize = 0;
    size_t m_currentSize = 0;
    uint32_t m_sequenceNumber = 0;
    
    // Platform-specific shared memory handle
    void* m_memoryHandle = nullptr;
};

} // namespace CameraGrpcServer

#endif // CAMERA_GRPC_SERVER_H
