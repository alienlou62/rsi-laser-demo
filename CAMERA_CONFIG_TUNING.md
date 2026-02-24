# Quick Reference: Camera .pfs Configuration & Testing

## Current Configuration Summary

**Camera:** Basler acA640-300gc
**Format:** YUYV (YUV422_YUYV_Packed)
**Output:** 640×480, 2 bytes/pixel, ~614 KB/frame
**Frame Rate:** 50 FPS (target)

---

## Critical Parameters in camera.pfs

```
# ACQUISITION
PixelFormat                YUV422_YUYV_Packed
Width                      640
Height                     480

# EXPOSURE & GAIN (most common tuning points)
ExposureMode              Timed
ExposureTimeRaw           5000         ← Exposure in microseconds
GainRaw                   136          ← Analog gain (0-512)
ExposureAuto              Off          ← Manual control

# FRAME RATE
AcquisitionFrameRateEnable            0
AcquisitionFrameRateAbs               50    ← FPS target

# COLOR TUNING (for red ball detection)
BalanceRatioRaw (Red)                 73
BalanceRatioRaw (Green)               64
BalanceRatioRaw (Blue)                101
GammaEnable                           1
GammaSelector                         sRGB

# TRIGGERS (currently disabled)
TriggerMode                Off
TriggerSelector            AcquisitionStart
```

---

## Recommended Settings by Lighting

### Bright Outdoor
```pfs
ExposureTimeRaw      2000
GainRaw              100
BalanceRatioRaw Red  80      ← Slightly boost red for saturation
```

### Normal Indoor (current)
```pfs
ExposureTimeRaw      5000
GainRaw              136
BalanceRatioRaw Red  73
```

### Dim/Dark
```pfs
ExposureTimeRaw      10000
GainRaw              220
BalanceRatioRaw Red  65     ← May need to decrease if too saturated
```

### High-speed tracking (reduce motion blur)
```pfs
ExposureTimeRaw      1000
GainRaw              180
AcquisitionFrameRateAbs   100    ← Increase frame rate
```

---

## Code Parameters to Tune

### 1. Red Ball Detection Threshold

**File:** `rttasks/src/image_processing.cpp`, line 46
```cpp
inline static constexpr double RED_THRESHOLD = 150;
```

**Tuning Guide:**
- **Too high (200+):** Only bright red balls detected, misses dim ones
- **Current (150):** Balance for typical red/orange balls
- **Too low (100):** Picks up skin tones, orange clothing (false positives)

**Recommended range:** 130-170 depending on ball color

### 2. Minimum Contour Area

**File:** `rttasks/include/image_processing.h`, line 14
```cpp
inline static constexpr double MIN_CONTOUR_AREA = 100;
```

**Effect:**
- Ignores objects smaller than N pixels²
- Prevents noise from being detected as ball
- Current: 100 pixels² ≈ 10 pixel radius circle

**Adjust if:**
- Too many false positives: increase to 150-200
- Missing distant balls: decrease to 50-75

### 3. Maximum Circle Fit Error

**File:** `rttasks/include/image_processing.h`, line 15
```cpp
inline static constexpr double MAX_CIRCLE_FIT_ERROR = 200;
```

**Effect:**
- Rejects non-circular objects detected as contours
- Higher = more lenient (accepts irregular shapes)
- Lower = only perfect circles

**Typical range:** 100-300

---

## Testing & Debugging Commands

### 1. Verify Camera Connection

```bash
# Check if camera is visible to Pylon SDK
# (Run as user with libpylon permissions or use sudo)
geoiplookup  # If system has GeoIP genicam tools

# Or directly test Pylon:
cd /home/eec/rsi-laser-demo
python3 << 'EOF'
from Pylon import PylonInstantCamera
camera = PylonInstantCamera()
camera.Open()
print(f"Camera: {camera.GetDeviceInfo().GetFullName()}")
camera.Close()
EOF
```

### 2. Verify Frame Capture (once RT task manager is running)

```bash
# Check if /tmp/rsi_camera_data.json is being updated
watch -n 0.1 'stat /tmp/rsi_camera_data.json'
# Should show incrementing "Modify" timestamps

# Or:
for i in {1..10}; do
  stat /tmp/rsi_camera_data.json | grep Modify
  sleep 0.5
done
```

### 3. Extract and Inspect Captured JPEG

```bash
# Extract base64 JPEG from JSON and decode
python3 << 'EOF'
import json
import base64

with open('/tmp/rsi_camera_data.json', 'r') as f:
    data = json.load(f)

# Extract base64 part (after "data:image/jpeg;base64,")
b64_str = data['imageData'].split(',')[1]
jpeg_data = base64.b64decode(b64_str)

# Write to file
with open('/tmp/last_frame.jpg', 'wb') as f:
    f.write(jpeg_data)

print(f"Extracted {len(jpeg_data)} bytes to /tmp/last_frame.jpg")

# Optional: verify it's valid JPEG
try:
    from PIL import Image
    img = Image.open('/tmp/last_frame.jpg')
    print(f"Valid JPEG: {img.size} {img.format}")
except ImportError:
    print("(PIL not installed, but file written)")
EOF

# View with:
# feh /tmp/last_frame.jpg
# or download via SCP
```

### 4. Monitor Detection Performance

```bash
# Check detection stats
python3 << 'EOF'
import json
import time

for _ in range(20):
    with open('/tmp/rsi_camera_data.json', 'r') as f:
        data = json.load(f)
    
    detected = data.get('ballDetected', False)
    x = data.get('centerX', 0)
    y = data.get('centerY', 0)
    r = data.get('radius', 0)
    frame = data.get('frameNumber', 0)
    
    status = "✓ DETECTED" if detected else "✗ NOT FOUND"
    print(f"Frame {frame}: {status} | Center: ({x:.1f}, {y:.1f}) Radius: {r:.1f}")
    time.sleep(0.1)
EOF
```

### 5. Test HTTP Camera Server

```bash
# Check if server is serving frames
curl -s http://localhost:50080/camera/frame | python3 -m json.tool | head -20

# Monitor frame freshness
for i in {1..5}; do
  curl -s http://localhost:50080/camera/frame | python3 -c "import sys, json; d=json.load(sys.stdin); print(f'Frame {d[\"frameNumber\"]}: Detected {d[\"ballDetected\"]}')"
  sleep 1
done
```

### 6. Check for GigE Packet Loss (advanced)

```bash
# Monitor network interface for dropped packets
watch -n 1 'ethtool -S enp2s0 | grep -i drop'

# Or:
ifconfig enp2s0 | grep "RX dropped\|TX dropped"

# Record traffic to inspect frames
sudo tcpdump -i enp2s0 -w /tmp/gige_traffic.pcap &
# ... let it run for a few seconds ...
# To analyze: wireshark /tmp/gige_traffic.pcap (filter by GigE Vision GVSP)
```

---

## Troubleshooting Checklist

### Camera not connected
- [ ] Check GigE cable to camera (separate from network)
- [ ] Camera has power
- [ ] `ethtool enp2s0` shows link status
- [ ] `/etc/laser_demo/camera.pfs` file exists and is readable

### Frames not being captured
- [ ] RT task manager running: `ps aux | grep rttaskmanager`
- [ ] `/tmp/rsi_camera_data.json` being updated
- [ ] No permission errors in `/rsi/rttaskapi.1.log`

### Ball not detected in good lighting
- [ ] Inspect captured JPEG (command #3 above)
- [ ] Is the ball actually red/orange in color?
- [ ] Lower `RED_THRESHOLD` to 130 and rebuild:
  ```bash
  cd /home/eec/rsi-laser-demo/build
  cmake ..
  make
  cd /home/eec/rsi-laser-demo
  sudo /rsi/rttaskmanager &  # Restart to load new library
  ```

### High latency / slow frame rate
- [ ] Check `AcquisitionFrameRateAbs` in .pfs (lower = slower capture)
- [ ] Check GigE bandwidth: `watch -n 1 'ip -s link show enp2s0'`
- [ ] Reduce resolution or increase exposure to lower JPEG size

---

## Modifying .pfs and Rebuilding

### 1. Edit camera.pfs
```bash
nano /etc/laser_demo/camera.pfs
# Modify any parameter, save, and exit
```

### 2. Restart RT task manager
```bash
# Kill existing process
pkill -f rttaskmanager
sleep 1

# Start new instance (loads updated .pfs)
sudo /rsi/rttaskmanager &
```

### 3. Verify change took effect
```bash
# Check JSON output for new JPEG quality or resolution
curl -s http://localhost:50080/camera/frame | python3 -c "import sys, json; d=json.load(sys.stdin); print(f'Height: {d[\"height\"]}, Width: {d[\"width\"]}, Size: {d[\"imageSize\"]} bytes')"
```

---

## Packet Type Verification

Your current setup uses the **correct packet type** for GigE cameras:

| Layer | Protocol | Details |
|-------|----------|---------|
| Physical | 1000BASE-T GigE | Gigabit Ethernet |
| Link | Ethernet II | Standard IEEE 802.3 frames |
| Transport | GigE Vision (GVSP) | Basler's streaming protocol |
| Payload | YUYV (YUV4:2:2) | 2 bytes per pixel |
| Application | HTTP (Camera server) | JSON + base64 JPEG |

**No changes needed** unless you want to switch to:
- **Mono8** (single-channel 8-bit) - faster, no color
- **RGB8** (full RGB, 3× bandwidth) - slower to transmit
- **Bayer patterns** (CFA-encoded) - requires debayering

---

## Performance Benchmark

**Expected performance** with current settings:
```
Frame rate:        ~45-50 FPS actual (50 FPS target)
JPEG size:         ~50-70 KB per frame
Network bandwidth: ~2-4 Mbps (well below 1 Gbps limit)
Latency:           ~30-50 ms (cap grab → JSON write)
```

If seeing sub-30 FPS, check:
1. CPU usage on RT task manager (`top -p <PID>`)
2. GigE bandwidth saturation
3. Disk I/O to `/tmp/rsi_camera_data.json`
