# MarbleLoop (Speed-Based Prediction Demo)

**MarbleLoop** is a mechanical demonstration designed to showcase predictive motion coordination using real-time sensing and actuator control. It combines Sick WFE fork sensors, a Festo linear actuator, and a Mitsubishi MR-J motor to measure marble speed and adjust a catcher’s position and pipe angle for continuous looping motion.

The demo highlights RSI’s motion system capabilities by using live I/O and motion control to anticipate and intercept free-moving marbles with sub-millisecond response precision.

## 🚀 Features

* Dual fork-sensor setup for high-speed marble velocity measurement
* Predictive catcher positioning using a vertical Festo actuator
* Automated pipe angle control via Mitsubishi MR-J servo motor
* Modular design for continuous marble loop operation
* Built using RSI’s [RMP EtherCAT Motion Controller](https://www.roboticsys.com/rmp-ethercat-motion-controller)

## 🏗️ Mechanical Overview

* **Sensors**: 2× Sick WFE fork sensors (velocity measurement)
* **Vertical Actuator**: Festo linear actuator with precision positioning
* **Angle Adjustment**: Mitsubishi MR-J series servo motor
* **Marble Path**: 5 ft curved tube with adjustable slope
* **Frame**: Mobile cart-mounted aluminum extrusion structure

## 📁 Project Structure

* `src/` – Core control logic (sensor reading, motion prediction, actuator commands)
* `scripts/` – Utility launch scripts
* `hardware/` – Configuration files for devices and IO mapping
* `models/` – Optional: Predictive velocity → landing position mappings or physics helpers

## 📦 Prerequisites

* Windows or Linux PC
* [RMP SDK](https://support.roboticsys.com)
* CMake + C++17
* Fork sensor wiring to digital inputs
* Mitsubishi MR-J driver configured via RSI
* Festo actuator connected via EtherCAT or discrete IO
* Optional: OpenCV or logging tools for visualization

## 🏙️ Quick Start

### 1️⃣ Sensor Testing

Make sure both WFE Sick fork sensors are wired to digital inputs and confirm edge detection using the I/O Monitor.

### 2️⃣ Launch MarbleLoop

```bash
cd scripts/
./run_marbleloop.sh
```

Modify `run_marbleloop.sh` to point to your build output or add `dotnet publish` if using a .NET wrapper.

## 🔢 Example: Speed → Catch Position

| Distance Between Sensors (in) | Time Between Triggers (ms) | Velocity (in/s) | Predicted Catcher Y (in) |
| ----------------------------- | -------------------------- | --------------- | ------------------------ |
| 3.00                          | 75                         | 40.00           | 8.1                      |
| 3.00                          | 50                         | 60.00           | 11.2                     |
| 3.00                          | 25                         | 120.00          | 16.5                     |

*Values based on preliminary testing; adjust for ramp angle and friction.*

## ⏱️ Performance Benchmarks

| Component                      | Response Time (ms) |
| ------------------------------ | ------------------ |
| Sensor Trigger → Velocity Calc | < 1 ms             |
| Prediction → Actuator Move     | < 5 ms             |
| Full Catch Loop                | \~300–600 ms       |

## 🔧 TODO / Future Improvements

* Add OpenCV tracking as a secondary validation layer
* Tune angle adjustment model using marble mass/friction calibration
* Add visual indicator (LED or screen) to show system state (catch success/fail)
* Integrate auto-reset mechanism for reloading marbles

## 📄 License

TO DETERMINE
