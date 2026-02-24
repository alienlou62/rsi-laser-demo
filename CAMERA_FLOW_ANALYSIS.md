# Camera Data Flow & Packet Analysis - Basler acA640-300gc

## Overview
The system uses the **Pylon SDK** (Basler's API) to interface with the Basler GigE camera. Images are received in **YUYV format (YUV422, 4:2:2)** and then processed through OpenCV pipelines.

---

## 1. IMAGE RECEIVE & PACKET TYPE

### Camera Configuration (.pfs file)
**Location:** `config/camera.pfs`

**Key Settings:**
```
PixelFormat       YUV422_YUYV_Packed    ← Output format (2 bytes per pixel)
Width             640
Height            480
ExposureTimeRaw   5000                  ← Exposure time in microseconds
GainRaw           136                   ← Analog gain (0-512 typically)
AcquisitionFrameRateAbs  50             ← Target frame rate (Hz)
```

### Packet Details
- **Pixel Format:** `YUV422_YUYV_Packed` (also written as `YUV2_YUYV` in literature)
- **Bytes per Pixel:** 2 bytes (packed format)
- **Frame Size:** 640 × 480 × 2 = 614,400 bytes
- **GigE Packet Type:** Standard GigE Vision protocol (GVSP - GigE Vision Streaming Protocol)
- **Max Payload:** ~9000 bytes per MTU (Ethernet packet usually ~9KB for GigE)
- **Total Packets per Frame:** ~68 packets (614,400 / 9000)

### YUYV (YUV422) Format Explained
```
Raw Data Layout (4 pixels shown):
[Y0][U0V0][Y1][Y2][U2V2][Y3]

Chroma (U/V) is shared between 2 adjacent horizontal pixels (4:2:2 subsampling)
- Every other U and V is explicit; others are interpolated
- This is why color conversion must account for pixel pairs
```

---

## 2. IMAGE RECEPTION FLOW (Code Path)

### A. Camera Initialization & Frame Grab

**File:** `rttasks/src/camera_helpers.cpp`

```cpp
void ConfigureCamera(CInstantCamera &camera)
{
    camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
    camera.Open();
    
    INodeMap &nodeMap = camera.GetNodeMap();
    CFeaturePersistence::Load(CONFIG_FILE, &nodeMap);
    // → Loads camera.pfs settings into Pylon SDK
}

void PrimeCamera(CInstantCamera &camera, CGrabResultPtr &grabResult)
{
    camera.Open();
    camera.StartGrabbing(GrabStrategy_LatestImageOnly);
    // → Begin continuous frame acquisition from camera
    // → "LatestImageOnly" drops frames if processing is slow
    
    if (TryGrabFrame(camera, grabResult))
        return;  // Success: frame buffer ready
}

bool TryGrabFrame(CInstantCamera &camera, CGrabResultPtr &grabResult, unsigned timeoutMs)
{
    if (!camera.RetrieveResult(timeoutMs, grabResult, TimeoutHandling_Return))
        return false;  // Timeout: no frame available
    
    if (grabResult->GrabSucceeded())
        return true;   // Frame successfully grabbed
    
    // Error handling: GigE Vision errors (underruns, etc)
}
```

**What happens:**
1. Pylon SDK opens connection to camera over GigE
2. `.pfs` file settings are loaded into camera (exposure, gain, format, etc.)
3. Camera starts streaming YUYV packets over GigE Vision protocol
4. Pylon SDK reassembles fragmented packets into complete frames
5. `RetrieveResult()` returns a frame buffer when available

---

### B. Frame Grab & YUYV Buffer

**File:** `rttasks/rttaskfunctions.cpp` - `DetectBall` task (line ~162)

```cpp
bool frameGrabbed = CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult, 0);

if (!frameGrabbed)
    return;  // Timeout or incomplete frame

// Get raw YUYV buffer directly from Pylon's result
uint8_t *yuyv_buffer = static_cast<uint8_t *>(g_ptrGrabResult->GetBuffer());

// Wrap buffer as OpenCV Mat (no memory copy!)
cv::Mat yuyvFrame = ImageProcessing::WrapYUYVBuffer(
    yuyv_buffer,
    CameraHelpers::IMAGE_WIDTH,   // 640
    CameraHelpers::IMAGE_HEIGHT   // 480
);
// → Mat type: CV_8UC2 (8-bit 2-channel = YUYV format)
```

**Buffer Layout in Memory:**
```
320 x 480 pixels, each pixel pair is 4 bytes:
[Y0][U][Y1][V][Y2][U][Y3][V]...

OpenCV treats as:
640 x 480 x CV_8UC2
where each element is 2 bytes (one Y-pixel paired with one chroma sample)
```

---

### C. Color Space Conversion (YUYV → RGB)

**File:** `rttasks/rttaskfunctions.cpp` - `OutputImage` task (line ~337)

```cpp
// Convert from YUYV to RGB for JPEG encoding
cv::Mat yuyvMat(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, 
                 CV_8UC2, (void *)buffer);

cv::Mat rgbFrame;
cv::cvtColor(yuyvMat, rgbFrame, cv::COLOR_YUV2RGB_YUYV);
// OpenCV's optimized YUYV→RGB conversion
// Handles 4:2:2 chroma subsampling automatically
```

**Conversion Details:**
- **Input:** YUYV (YUV420, 4:2:2 chroma subsampling)
- **Output:** RGB (8-bit per channel, 3 channels)
- **Algorithm:** 
  - Y is used directly
  - U/V are interpolated horizontally (shared between pixel pairs)
  - Full YUV→RGB matrix applied: `[R,G,B] = M × [Y,U,V]`

---

### D. Ball Detection (Thresholding on V channel)

**File:** `rttasks/src/image_processing.cpp`

```cpp
bool TryDetectBall(const cv::Mat& yuyvFrame, cv::Vec3f& ball)
{
    // Extract V (red chrominance) channel from YUYV
    // Red objects have high V values
    
    cv::Mat v_channel = ExtractV(yuyvFrame);  // Line ~52
    
    // Binary threshold: pixels with V > RED_THRESHOLD (150) become white
    cv::Mat mask;
    cv::threshold(v_channel, mask, RED_THRESHOLD, 255, cv::THRESH_BINARY);
    
    // Morphological cleanup (remove noise)
    // Dilation → Erosion (close holes)
    // Erosion → Dilation (remove small objects)
    
    // Find contours of bright regions
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // Fit circle to largest contour
    if (FindBall(mask, ball))  // Returns [centerX, centerY, radius]
        return true;
    return false;
}
```

**Why V-channel Detection?**
- V encodes **red-yellow** chrominance
- A **red/orange ball** has high V values (>150)
- This avoids expensive full-image processing
- Works reliably under consistent lighting

---

### E. Frame Output as JSON + Base64 JPEG

**File:** `rttasks/rttaskfunctions.cpp` - `OutputImage` task (line ~339)

```cpp
// Encode RGB frame as JPEG
std::vector<uint8_t> jpegBuffer;
std::vector<int> jpegParams = {cv::IMWRITE_JPEG_QUALITY, 80};
cv::imencode(".jpg", rgbFrame, jpegBuffer, jpegParams);
// → OpenCV's libjpeg integration, quality 80

// Base64 encode JPEG buffer
std::string base64Image = EncodeBase64(jpegBuffer);

// Create JSON output
json << "{\n";
json << "  \"imageData\": \"data:image/jpeg;base64," << base64Image << "\",\n";
json << "  \"ballDetected\": " << (ballDetected ? "true" : "false") << ",\n";
json << "  \"centerX\": " << centerX << ",\n";
json << "  \"centerY\": " << centerY << ",\n";
json << "  \"radius\": " << radius << "\n";
json << "}\n";

// Atomic write to /tmp/rsi_camera_data.json
std::ofstream dataFile("/tmp/rsi_camera_data.json.tmp");
dataFile << json.str();
dataFile.close();
std::rename("/tmp/rsi_camera_data.json.tmp", "/tmp/rsi_camera_data.json");
// Atomic rename prevents partial reads
```

**Data Format Flow:**
```
GigE Vision Packets (YUYV)
    ↓ (Pylon SDK reassambles)
YUYV Buffer [640×480×2 bytes]
    ↓ (cvtColor)
RGB Mat [640×480×3 bytes]
    ↓ (imencode)
JPEG Buffer [~40-80 KB @ quality 80]
    ↓ (base64)
Base64 String [~53KB-107KB]
    ↓ (JSON embed)
/tmp/rsi_camera_data.json [~100KB file]
    ↓ (HTTP GET)
UI receives data:image/jpeg;base64,... in JSON
    ↓ (browser decode)
Image displayed in UI
```

---

## 3. VERIFYING YOUR .PFS FILE

### Checklist for Basler acA640-300gc

**1. Pixel Format**
```
Current: PixelFormat	YUV422_YUYV_Packed
✓ CORRECT for this camera model
Alternative options: Mono8, Mono10, RGB8, RGB10, regex2Check datasheet for your model
```

**2. Image Dimensions**
```
Current: Width 640, Height 480
✓ Native sensor is 640×480 for acA640-300gc
(The "640" in the model name is width)
```

**3. Exposure Settings**
```
Current: ExposureTimeRaw 5000  (5000 microseconds = 5ms)
Reference line: camera.pfs line 62
→ Adjust for lighting conditions:
   - Bright environment: 1000-2000 µs
   - Normal room: 3000-5000 µs (current)
   - Dark environment: 8000-10000 µs
   - Max safe: 20000 µs
```

**4. Gain**
```
Current: GainRaw 136
Range: 0-512 for GigE cameras
→ Increase if dark; decrease if overexposed
→ Gain + Exposure = exposure compensation
```

**5. Frame Rate**
```
Current: AcquisitionFrameRateAbs 50  (50 FPS target)
Max sustainable: Limited by exposure + GigE bandwidth
640×480×2 bytes × 50 FPS = 30.7 Mbps (fits in GigE 1000 Mbps)
✓ Feasible
```

**6. Trigger Settings**
```
Current:
TriggerMode     Off
→ Free-running mode (no external trigger)
✓ Correct for continuous streaming
If you need hardware sync, set:
TriggerSource   Line1
TriggerActivation  RisingEdge
```

**7. Color Balance (for Red Ball Detection)**
```
Current settings tune white balance for outdoor/daylight
BalanceRatioRaw: Red=73, Green=64, Blue=101
→ These set the gain per color channel in Bayer→RGB
→ For red ball, these values help boost red channel sensitivity
→ If detection is weak, try increasing Red ratio
```

---

## 4. PACKET LOSS DEBUGGING

### Symptoms of Issues

| Issue | Cause | Fix |
|-------|-------|-----|
| Frames timeout (0 FPS) | Network/GigE issues or camera not streaming | Check GigE cable, IP configuration, camera power |
| Dropped frames | Insufficient bandwidth or USB congestion | Lower frame rate or reduce image format complexity |
| Partial/corrupted frames | GigE Vision GVSP reassembly failure | Increase MTU size, check network congestion |
| Color shift (detection fails) | White balance misconfigured | Recalibrate with `BalanceRatioRaw` values |
| Detection fails in low light | Underexposed | Increase `ExposureTimeRaw` or `GainRaw` |

### To Monitor Packet Health

```cpp
// In camera_helpers.cpp, after frame grab:
int64_t blockedFrames = grabResult->GetBlockedPixels();
int64_t errorCode = grabResult->GetErrorCode();

std::cout << "Blocked pixels: " << blockedFrames << "\n";
std::cout << "Error code: " << std::hex << errorCode << std::dec << "\n";
// → Non-zero = incomplete frame due to GigE issues
```

---

## 5. CUSTOM ADJUSTMENTS FOR YOUR SETUP

### If Detection Fails

**Option 1: Adjust V-threshold (line 46, image_processing.cpp)**
```cpp
inline static constexpr double RED_THRESHOLD = 150;  // Current
// Lower to 120-130 to catch darker reds
// Raise to 170-200 to detect only bright reds
```

**Option 2: Adjust Exposure + Gain in .pfs**
```
ExposureTimeRaw  8000    ← Increase for darker scenes
GainRaw          200     ← Boost if still underexposed
```

**Option 3: Verify Color Space**
Currently assumes YUYV→RGB. If detection still fails:
```cpp
// Try alternative Bayer decoding if camera actually outputs Bayer
cv::cvtColor(bayerMat, rgbFrame, cv::COLOR_BayerBG2RGB);
```

### If Frame Rate is Low

Check that `AcquisitionFrameRateEnable` is set and `AcquisitionFrameRateAbs` is configured:
```
AcquisitionFrameRateEnable    0      ← Might be disabled
AcquisitionFrameRateAbs       50     ← Set target rate
```

If GigE is saturated:
- Lower frame rate: `AcquisitionFrameRateAbs  30`
- Use JPEG compression at camera (if available)
- Reduce resolution (if flexible)

---

## Summary

Your system correctly:
1. **Receives** YUYV packets via GigE Vision protocol
2. **Reassembles** fragmented GVSP packets (Pylon SDK)
3. **Stores** raw YUYV buffers in memory
4. **Detects** red objects by thresholding V-channel
5. **Converts** YUYV→RGB and encodes to JPEG
6. **Outputs** base64-JPEG in JSON for HTTP distribution

**Your `.pfs` file is correctly configured** for the Basler acA640-300gc. Adjustments should focus on exposure/gain tuning for lighting conditions, not packet type changes.
