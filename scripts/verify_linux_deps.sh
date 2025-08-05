#!/bin/bash

# Script to verify Linux dependencies for the camera gRPC streaming

echo "=== Verifying Linux Dependencies for Camera gRPC Streaming ==="
echo

# Check for shared memory filesystem
echo "1. Checking shared memory filesystem..."
if [ -d "/dev/shm" ]; then
    echo "   ✓ /dev/shm is available"
    ls -la /dev/shm/ | head -5
    echo "   Available space in /dev/shm:"
    df -h /dev/shm/
else
    echo "   ✗ /dev/shm is not available - shared memory may not work"
fi
echo

# Check for rt library
echo "2. Checking for POSIX real-time library..."
if ldconfig -p | grep -q "librt"; then
    echo "   ✓ librt (POSIX real-time library) is available"
    ldconfig -p | grep librt
else
    echo "   ✗ librt not found - install with: sudo apt-get install libc6-dev"
fi
echo

# Check for gRPC libraries
echo "3. Checking for gRPC libraries..."
if pkg-config --exists grpc++; then
    echo "   ✓ gRPC++ is available"
    echo "   Version: $(pkg-config --modversion grpc++)"
else
    echo "   ✗ gRPC++ not found - install with: sudo apt-get install libgrpc++-dev"
fi
echo

# Check for protobuf
echo "4. Checking for Protocol Buffers..."
if pkg-config --exists protobuf; then
    echo "   ✓ Protocol Buffers is available"
    echo "   Version: $(pkg-config --modversion protobuf)"
else
    echo "   ✗ Protocol Buffers not found - install with: sudo apt-get install libprotobuf-dev"
fi
echo

# Check for OpenCV
echo "5. Checking for OpenCV..."
if pkg-config --exists opencv4; then
    echo "   ✓ OpenCV4 is available"
    echo "   Version: $(pkg-config --modversion opencv4)"
elif pkg-config --exists opencv; then
    echo "   ✓ OpenCV is available"
    echo "   Version: $(pkg-config --modversion opencv)"
else
    echo "   ✗ OpenCV not found - install with: sudo apt-get install libopencv-dev"
fi
echo

# Test shared memory creation (basic test)
echo "6. Testing shared memory functionality..."
cat > /tmp/test_shm.cpp << 'EOF'
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

int main() {
    const char* name = "/test_rsi_shm";
    size_t size = 1024;
    
    // Create shared memory
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "Failed to create shared memory" << std::endl;
        return 1;
    }
    
    // Set size
    if (ftruncate(fd, size) == -1) {
        std::cerr << "Failed to set size" << std::endl;
        close(fd);
        shm_unlink(name);
        return 1;
    }
    
    // Map memory
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Failed to map memory" << std::endl;
        close(fd);
        shm_unlink(name);
        return 1;
    }
    
    // Test write/read
    strcpy((char*)ptr, "Hello RSI!");
    std::cout << "Shared memory test: " << (char*)ptr << std::endl;
    
    // Cleanup
    munmap(ptr, size);
    close(fd);
    shm_unlink(name);
    
    std::cout << "✓ Shared memory functionality works correctly" << std::endl;
    return 0;
}
EOF

if g++ -o /tmp/test_shm /tmp/test_shm.cpp -lrt 2>/dev/null; then
    if /tmp/test_shm; then
        echo "   ✓ Shared memory test passed"
    else
        echo "   ✗ Shared memory test failed"
    fi
    rm -f /tmp/test_shm
else
    echo "   ✗ Failed to compile shared memory test (missing compiler or rt library)"
fi
rm -f /tmp/test_shm.cpp

echo
echo "=== Dependency Check Complete ==="
echo "If all checks passed, the camera gRPC streaming should work on Linux."
echo "If any checks failed, install the missing dependencies before building."
