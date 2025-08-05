# RapidLaser (RealTimeTasks + Vision Demo)

RapidLaser is a mechanical demonstration designed to showcase the precision of RSI's RMP motion control system, specifically its [**RealTimeTasks**](https://support.roboticsys.com/rmp/rttasks.html) feature which allows you to execute deterministic user logic, motion control, and I/O operations without specialized real-time programming expertise.  

It features a two-axis gimbal mechanism that uses Basler's Pylon SDK and OpenCV to track a colored ball in real time, dynamically positioning a laser to keep the ball centered.

## 🚀 Features

- Real-time tracking using OpenCV & Pylon SDK
- Two-axis gimbal system
- Dynamic motor control using RSI's [RMP EtherCAT Motion Controller](https://www.roboticsys.com/rmp-ethercat-motion-controller)

## 📁 Project Structure

- `src/` - Core source code and libraries
- `ui/` - Main desktop demo UI/app (RapidLaser.Desktop)
- `scripts/` - Utility scripts for running the UI, and more

## 📦 Prerequisites

- Windows or Linux PC
- RMP SDK
- .NET 9.0 SDK
- Basler camera (with Pylon SDK)
- OpenCV

## 🏁 Quick Start

### 🖥️ Run the UI

Navigate to the `scripts/` folder and run the appropriate script for your platform:

**Windows**  

1. Open a Unix-style shell like (Git Bash)
2. Run command: `./ui_run.sh`  

**Linux**  

1. Open terminal
2. Make file executable: `chmod +x ui_run.sh`
3. Run command: `./ui_run.sh` 

**Note**: the `ui_run.sh` script will call dotnet publish only if it does not locate an executable in the temp/ folder

## ⏱️ Performance Metrics

### 1️⃣ Timing Metrics Initial Run

| Category   | Min (ms) | Max (ms) | Average (ms) |
|------------|----------|----------|--------------|
| Loop       | 20       | 443      | 61.7381      |
| Retrieve   | 0        | 96       | 0.875969     |
| Processing | 14       | 245      | 48.2302      |
| Motion     | 2        | 29       | 5.09524      |

### 2️⃣ Timing Metrics Second Run

| Category   | Min (ms) | Max (ms) | Last (ms) | Average (ms) |
|------------|----------|----------|-----------|---------------|
| Loop       | 5        | 77       | 14        | 10.2427       |
| Retrieve   | 0        | 3        | 0         | 0.0589298     |
| Processing | 4        | 71       | 9         | 7.72252       |
| Motion     | 0        | 12       | 4         | 2.45086       |

## 📄 License

TO DETERMINE
