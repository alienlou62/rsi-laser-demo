# RSI Laser Demo

A mechanical demonstration showcasing the precision of the RMP system at Robotics Systems Integration (RSI).
This two-axis gimbal mechanism uses Basler's Pylon SDK, OpenCV, and RMP motion control to track a colored ball
and dynamically position a laser to center it in real time.

## Features

- Real-time tracking using OpenCV & Pylon SDK
- Two-axis gimbal system
- Dynamic motor control using RSI's RMP

### Timing Metrics Initial Run

| Category   | Min (ms) | Max (ms) | Average (ms) |
|------------|----------|----------|--------------|
| Loop       | 20       | 443      | 61.7381      |
| Retrieve   | 0        | 96       | 0.875969     |
| Processing | 14       | 245      | 48.2302      |
| Motion     | 2        | 29       | 5.09524      |

### Timing Metrics Second Run

| Category   | Min (ms) | Max (ms) | Last (ms) | Average (ms) |
|------------|----------|----------|-----------|---------------|
| Loop       | 5        | 77       | 14        | 10.2427       |
| Retrieve   | 0        | 3        | 0         | 0.0589298     |
| Processing | 4        | 71       | 9         | 7.72252       |
| Motion     | 0        | 12       | 4         | 2.45086       |

