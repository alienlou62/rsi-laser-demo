#!/bin/bash

# Simulate RT task writing camera data for testing
echo "Simulating RT task camera data..."

# Create sample camera data JSON
cat > /tmp/rsi_camera_data.json << 'EOF'
{
  "timestamp": 1754408800000,
  "frameNumber": 12345,
  "width": 640,
  "height": 480,
  "format": "jpeg",
  "ballDetected": true,
  "centerX": 320.5,
  "centerY": 240.8,
  "confidence": 0.87,
  "targetX": 15.2,
  "targetY": -8.4,
  "rtTaskRunning": true
}
EOF

# Create running flag
echo "1" > /tmp/rsi_rt_task_running

echo "RT task simulation data created. Testing C# server response..."

# Test the C# server
curl -s http://localhost:8080/camera/frame

echo -e "\n\nRT task simulation complete."
