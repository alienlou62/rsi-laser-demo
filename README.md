# RSI Laser Demo

A mechanical demonstration showcasing the precision of the RMP system at Robotics Systems Integration (RSI).
This two-axis gimbal mechanism uses Basler's Pylon SDK, OpenCV, and RMP motion control to track a colored ball
and dynamically position a laser to center it in real time.

## Features

- Real-time tracking using OpenCV & Pylon SDK
- Two-axis gimbal system
- Dynamic motor control using RSI's RMP

### Current Timing Metrics

| Category   | Min (ms) | Max (ms) | Average (ms) |
|------------|----------|----------|--------------|
| Loop       | 20       | 443      | 61.7381      |
| Retrieve   | 0        | 96       | 0.875969     |
| Processing | 14       | 245      | 48.2302      |
| Motion     | 2        | 29       | 5.09524      |
