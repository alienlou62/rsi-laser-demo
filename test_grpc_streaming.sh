#!/bin/bash

# Test script to demonstrate the gRPC camera streaming solution
echo "=== RSI Laser Demo - gRPC Camera Streaming Test ==="
echo

echo "Platform: This solution works on both Windows and Linux"
echo "Current dependencies for Linux:"
echo "  - gRPC C++ library (libgrpc++-dev)"
echo "  - Protocol Buffers (libprotobuf-dev)" 
echo "  - OpenCV (libopencv-dev)"
echo "  - POSIX RT library (included in glibc)"
echo "  - Pylon SDK for Linux"
echo "Run './scripts/verify_linux_deps.sh' to check dependencies on Linux"
echo

echo "1. Building C++ project with gRPC support..."
echo "   - Make sure you have gRPC and protobuf installed"
echo "   - Run: mkdir build && cd build"
echo "   - Run: cmake .."
echo "   - Run: make LaserDemoRTTasks"
echo

echo "2. Building C# UI project..."
echo "   - Run: cd ui"
echo "   - Run: dotnet build"
echo

echo "3. Running the system:"
echo "   - Terminal 1: ./build/LaserDemoRTTasks (starts C++ RT tasks + gRPC server)"
echo "   - Terminal 2: cd ui && dotnet run --project RapidLaser.Desktop (starts UI)"
echo

echo "4. Expected behavior:"
echo "   - C++ program starts real-time camera capture and ball detection"
echo "   - gRPC server starts on port 50051"
echo "   - Image data is stored in shared memory and streamed via gRPC"
echo "   - C# UI connects to gRPC server and displays live camera feed"
echo "   - Ball detection data is overlaid on the video stream"
echo

echo "5. Architecture Overview:"
echo "   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐"
echo "   │   RT Camera     │    │  Shared Memory  │    │   gRPC Server   │"
echo "   │   Task (C++)    │───▶│   Image Buffer  │───▶│    (C++)        │"
echo "   └─────────────────┘    └─────────────────┘    └─────────────────┘"
echo "                                                           │"
echo "                                                           ▼"
echo "   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐"
echo "   │   UI Display    │◀───│  gRPC Client    │◀───│  Network Stream │"
echo "   │    (C#)         │    │     (C#)        │    │     (gRPC)      │"
echo "   └─────────────────┘    └─────────────────┘    └─────────────────┘"
echo

echo "6. Key Components Created:"
echo "   - camera_grpc_server.h/cpp: gRPC server implementation"
echo "   - ImageBuffer: Shared memory management for images"
echo "   - GrpcCameraService.cs: C# gRPC client for camera"
echo "   - GrpcImageProcessingService.cs: C# service for processing gRPC frames"
echo "   - Updated RT tasks to store images in shared memory"
echo "   - Updated CMakeLists.txt with gRPC dependencies"
echo

echo "7. Data Flow:"
echo "   a) RT DetectBall task captures YUYV camera frames"
echo "   b) Image data stored in Windows shared memory"
echo "   c) Ball detection results stored in RT task globals"
echo "   d) gRPC server reads from shared memory and globals"
echo "   e) Server converts YUYV to RGB/JPEG as requested"
echo "   f) C# UI receives streaming frames with detection overlay"
echo "   g) UI displays live video with ball tracking visualization"
echo

echo "8. Supported Image Formats:"
echo "   - FORMAT_YUYV: Raw camera data"
echo "   - FORMAT_RGB: Converted RGB24"
echo "   - FORMAT_JPEG: Compressed JPEG"
echo "   - FORMAT_GRAYSCALE: Grayscale conversion"
echo

echo "Note: This is a complete end-to-end solution for streaming camera images"
echo "      from real-time C++ threads to the C# UI via gRPC with ball detection overlay."
