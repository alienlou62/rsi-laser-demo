# gRPC Camera Streaming Implementation

This implementation provides real-time camera image streaming from C++ real-time tasks to the C# UI using gRPC.

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   RT Camera     │    │  Shared Memory  │    │   gRPC Server   │
│   Task (C++)    │───▶│   Image Buffer  │───▶│    (C++)        │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                                        │
                                                        ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   UI Display    │◀───│  gRPC Client    │◀───│  Network Stream │
│    (C#)         │    │     (C#)        │    │     (gRPC)      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Components

### C++ Side (Real-Time)

1. **RTTask DetectBall** (`RTTaskFunctions/src/rttaskfunctions.cpp`)
   - Captures YUYV camera frames using Pylon SDK
   - Performs real-time ball detection
   - Stores image data in shared memory
   - Updates RT task globals with detection results

2. **ImageBuffer** (`src/camera_grpc_server.cpp`)
   - Windows shared memory management
   - Thread-safe image storage and retrieval
   - 1MB buffer for camera frames

3. **CameraStreamServer** (`src/camera_grpc_server.cpp`)
   - gRPC server implementation
   - Serves `CameraStreamService` from camera_streaming.proto
   - Supports both streaming and single-frame requests
   - Converts YUYV to RGB/JPEG on demand

### C# Side (UI)

1. **GrpcCameraService** (`Services/GrpcCameraService.cs`)
   - Implements `ICameraService` using gRPC
   - Connects to C++ gRPC server on port 50051
   - Supports frame streaming and single frame requests

2. **GrpcImageProcessingService** (`Services/GrpcImageProcessingService.cs`)
   - Processes gRPC camera frames
   - Extracts ball detection data from frames
   - Generates binary masks for visualization

## Data Flow

1. **Image Capture**: RT DetectBall task captures YUYV frames from camera
2. **Detection**: OpenCV processes frame for ball detection
3. **Storage**: Image data stored in Windows shared memory
4. **Globals Update**: RT task globals updated with detection results
5. **gRPC Request**: C# UI requests frames via gRPC
6. **Format Conversion**: Server converts YUYV to requested format (RGB/JPEG)
7. **Frame Assembly**: Server combines image data with detection results
8. **Streaming**: Frames streamed to UI with embedded ball detection data
9. **Display**: UI displays live video with ball tracking overlay

## Supported Image Formats

- `FORMAT_YUYV`: Raw camera data (640x480x2 bytes)
- `FORMAT_RGB`: RGB24 format (640x480x3 bytes)
- `FORMAT_JPEG`: JPEG compressed (variable size)
- `FORMAT_GRAYSCALE`: 8-bit grayscale (640x480x1 bytes)

## Key Features

- **Real-Time Performance**: Uses RT tasks for deterministic image capture
- **Efficient Memory Usage**: Shared memory avoids copying large image buffers
- **Format Flexibility**: On-demand image format conversion
- **Ball Detection Overlay**: Detection results embedded in frame metadata
- **Streaming Support**: Server-side streaming for smooth video playback
- **Error Handling**: Graceful degradation when components fail

## Configuration

### C++ Server
- **Port**: 50051 (configurable in `rttasks_demo_main.cpp`)
- **Buffer Size**: 1MB shared memory buffer
- **Image Size**: 640x480 YUYV (configurable in `camera_helpers.h`)

### C# Client
- **Server Address**: localhost:50051 (configurable in `GrpcCameraService.cs`)
- **Default Format**: RGB24
- **Timeout**: 5 seconds for connection, 1 second for frames

## Building

### Prerequisites
- gRPC C++ library
- Protocol Buffers C++ library
- OpenCV
- Pylon SDK
- .NET 9.0 SDK

### C++ Build
```bash
mkdir build && cd build
cmake ..
make LaserDemoRTTasks
```

### C# Build
```bash
cd ui
dotnet build
```

## Running

1. Start C++ RT tasks application:
   ```bash
   ./build/LaserDemoRTTasks
   ```

2. Start C# UI:
   ```bash
   cd ui && dotnet run --project RapidLaser.Desktop
   ```

## Performance Characteristics

- **Latency**: ~1-2 frames (33-66ms at 30 FPS)
- **Throughput**: Up to 30 FPS streaming
- **Memory Usage**: 1MB shared buffer + frame conversion buffers
- **CPU Usage**: Minimal overhead for gRPC streaming

## Troubleshooting

1. **gRPC Connection Failed**: Check that C++ server is running and port 50051 is available
2. **No Image Data**: Verify camera is connected and RT tasks are running
3. **Format Conversion Errors**: Check OpenCV installation and image dimensions
4. **Shared Memory Issues**: Ensure proper Windows permissions for shared memory access

## Future Enhancements

- Configurable image compression levels
- Multiple camera support
- Dynamic frame rate adjustment
- Bandwidth optimization for network streaming
- H.264 hardware encoding support
