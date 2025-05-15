# RSI Laser Demo

A mechanical demonstration showcasing the precision of the RMP system at Robotics Systems Integration (RSI). 
This two-axis gimbal mechanism uses Basler's Pylon SDK, OpenCV, and RMP motion control to track a colored ball 
and dynamically position a laser to center it in real time.

## Features
- Real-time tracking using OpenCV & Pylon SDK
- Two-axis gimbal system
- Dynamic motor control using RSI's RMP

### Build Instructions
```bash
cd build
cmake ..
make
./Basics_Grab

| Metric                 | Value     | Notes                                                          |
| ---------------------- | --------- | -------------------------------------------------------------- |
| **Frame Rate**         | \~200 FPS | Measured during normal operation                               |
| **Cycle Time**         | \~40 ms   | Includes capture, processing, and actuation                    |
| **Detection Accuracy** | \~95%     | Occasionally detects smaller background objects (can be tuned) |
