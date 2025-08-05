#!/bin/bash

# Simulate RT task writing actual camera frame data for testing
echo "Simulating RT task with camera frame data..."

# Create sample camera frame data JSON with base64 image
# This simulates what the DetectBall RT task would write
cat > /tmp/rsi_camera_data.json << 'EOF'
{
  "timestamp": 1754408800000,
  "frameNumber": 12345,
  "width": 640,
  "height": 480,
  "format": "jpeg",
  "imageData": "data:image/jpeg;base64,/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/2wBDAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAv/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFQEBAQAAAAAAAAAAAAAAAAAAAAX/xAAUEQEAAAAAAAAAAAAAAAAAAAAA/9oADAMBAAIRAxEAPwA/wA",
  "imageSize": 631,
  "ballDetected": true,
  "centerX": 320.5,
  "centerY": 240.8,
  "confidence": 0.872,
  "targetX": 15.2,
  "targetY": -8.4,
  "rtTaskRunning": true
}
EOF

# Create running flag
echo "1" > /tmp/rsi_rt_task_running

echo "RT task camera frame simulation data created."
echo "The JSON now contains actual base64-encoded JPEG image data that the UI can render."
echo ""
echo "Testing C# server response..."

# Test the C# server
curl -s http://localhost:8080/camera/frame | head -10

echo -e "\n\nRT task camera frame simulation complete."
echo "The imageData field contains a data URI that can be directly used in HTML <img> tags or Canvas."
