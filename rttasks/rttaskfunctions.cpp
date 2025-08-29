//camera
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

//src
#include "rttaskglobals.h"
#include "camera_helpers.h"
#include "image_processing.h"
#include "shared_data_helpers.h"

//system
#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>

using namespace Pylon;
using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

// Global variable (different from RTTASK_GLOBAL)
PylonAutoInitTerm g_PylonAutoInitTerm;
CInstantCamera g_camera;
CGrabResultPtr g_ptrGrabResult;

// Shared storage for camera frames
struct Frame
{
  CameraHelpers::YUYVFrame yuyvData;
  int frameNumber;
  double timestamp;
  bool ballDetected;
  double centerX;
  double centerY;
  double radius;
  double targetX;
  double targetY;
};

inline std::shared_ptr g_frameStorage = std::make_shared<SharedDataHelpers::SPSCStorage<Frame>>();

// Initializes the global data structure and sets up the camera and multi-axis.
RSI_TASK(Initialize)
{
  // Initialize the global data
  data->initialized = false;

  data->cameraReady = false;
  data->cameraGrabbing = false;
  data->frameGrabFailures = 0;

  data->ballDetected = false;
  data->ballDetectionFailures = 0;
  data->ballCenterX = 0.0;
  data->ballCenterY = 0.0;
  data->ballRadius = 0.0;

  data->newImageAvailable = false;
  data->frameTimestamp = 0;
  data->imageWidth = CameraHelpers::IMAGE_WIDTH;
  data->imageHeight = CameraHelpers::IMAGE_HEIGHT;
  data->imageSequenceNumber = 0;
  data->imageDataSize = sizeof(CameraHelpers::YUYVFrame);

  data->multiAxisReady = false;
  data->motionEnabled = false;
  data->newTarget = false;
  data->targetX = 0.0;
  data->targetY = 0.0;

  data->firmwareTimingDeltaMax = 0;
  data->firmwareTimingDeltaMaxSampleCount = 0;
  data->networkTimingDeltaMax = 0;
  data->networkTimingDeltaMaxSampleCount = 0;
  data->networkTimingReceiveDeltaMax = 0;
  data->networkTimingReceiveDeltaMaxSampleCount = 0;

  // Enable network timing
  RTMotionControllerGet()->NetworkTimingEnableSet(true);

  // Setup the camera
  CameraHelpers::ConfigureCamera(g_camera);
  CameraHelpers::PrimeCamera(g_camera, g_ptrGrabResult);
  data->cameraReady = true;

  // Setup the multi-axis
  RTMultiAxisGet(0)->Abort();
  RTMultiAxisGet(0)->ClearFaults();
  RTMultiAxisGet(0)->MotionAttributeMaskOffSet(RSIMotionAttrMask::RSIMotionAttrMaskAPPEND);
  RTMultiAxisGet(0)->MotionAttributeMaskOnSet(RSIMotionAttrMask::RSIMotionAttrMaskNO_WAIT);
  RTMultiAxisGet(0)->AmpEnableSet(true);

  // Set the initial target positions to the current positions
  data->targetX = RTAxisGet(0)->ActualPositionGet();
  data->targetY = RTAxisGet(1)->ActualPositionGet();

  data->multiAxisReady = true;
  data->initialized = true;
  data->motionEnabled = true;
}

// Moves the motors based on the target positions.
RSI_TASK(MoveMotors)
{
  // Define limits for the target positions
  static constexpr double NEG_X_LIMIT = -0.19;
  static constexpr double POS_X_LIMIT = 0.19;
  static constexpr double NEG_Y_LIMIT = -0.14;
  static constexpr double POS_Y_LIMIT = 0.14;

  // Check if the system is initialized and motion is enabled
  if (!data->initialized) return;
  if (!data->motionEnabled) return;
  if (!data->multiAxisReady) return;

  // Only execute if a new target is set
  if (!data->newTarget.exchange(false)) return;

  // Move the motors to the target positions respecting the limits
  try
    {
      double clampedX = std::clamp(data->targetX.load(), NEG_X_LIMIT, POS_X_LIMIT);
      double clampedY = std::clamp(data->targetY.load(), NEG_Y_LIMIT, POS_Y_LIMIT);
      RTMultiAxisGet(0)->MoveSCurve(std::array{clampedX, clampedY}.data());
    }
    catch (const RsiError &e)
    {
      if (RTMultiAxisGet(0))
        RTMultiAxisGet(0)->Abort();
      throw std::runtime_error(std::string("RMP exception during velocity control: ") + e.what());
    }
    catch (const std::exception &ex)
    {
      if (RTMultiAxisGet(0))
        RTMultiAxisGet(0)->Abort();
      throw std::runtime_error(std::string("Error during velocity control: ") + ex.what());
    }
}

// Processes the image captured by the camera.
RSI_TASK(DetectBall)
{
  static SharedDataHelpers::SPSCStorageManager frameWriter(g_frameStorage, true);

  if (!data->initialized) return;
  if (!data->cameraReady) return;

  bool frameGrabbed = false;

  try
  {
    frameGrabbed = CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult, 0);
  }
  catch (...)
  {
    // If an exception occurs during frame grab increment failure count
    data->frameGrabFailures++;
    return;
  }
  data->cameraGrabbing = true;

  // If frame grab failed due to a timeout, exit early but do not increment failure count
  if (!frameGrabbed) return;

  // Store image data for JSON file - for C# server communication
  static uint32_t sequenceNumber = 0;
  sequenceNumber++;
  
  // Get current frame timestamp
  auto frameTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::high_resolution_clock::now().time_since_epoch()).count();
  
  // Update image streaming globals
  data->newImageAvailable = true;
  data->frameTimestamp = frameTimestamp;
  data->imageWidth = CameraHelpers::IMAGE_WIDTH;
  data->imageHeight = CameraHelpers::IMAGE_HEIGHT;
  data->imageSequenceNumber = sequenceNumber;

  // Record the axis positions at the time of frame grab
  double initialX(RTAxisGet(0)->ActualPositionGet());
  double initialY(RTAxisGet(1)->ActualPositionGet());

  // Convert the grabbed frame to a CV mat format for processing
  cv::Mat yuyvFrame = ImageProcessing::WrapYUYVBuffer(static_cast<uint8_t *>(g_ptrGrabResult->GetBuffer()), 
                                                      CameraHelpers::IMAGE_WIDTH, 
                                                      CameraHelpers::IMAGE_HEIGHT);

  // Detect the ball in the YUYV frame
  cv::Vec3f ball(0.0, 0.0, 0.0);
  bool ballDetected = ImageProcessing::TryDetectBall(yuyvFrame, ball);

  // Update global data with the detection results
  data->ballCenterX = ball[0];
  data->ballCenterY = ball[1];
  data->ballRadius = ball[2];
  data->ballDetected = ballDetected;

  // Calculate confidence based on ball radius (larger = more confident)
  double confidence = ballDetected ? std::min(ball[2] / 50.0, 1.0) : 0.0;

  // Calculate the target positions based on the offsets and the position at the time of frame grab
  double offsetX(0.0), offsetY(0.0);
  if (ballDetected) {
    ImageProcessing::CalculateTargetPosition(ball, offsetX, offsetY);
    data->targetX = initialX + offsetX;
    data->targetY = initialY + offsetY;
  }
  
  // Store the YUYV frame and metadata in the shared memory
  memcpy(frameWriter.data().yuyvData, yuyvFrame.data, sizeof(CameraHelpers::YUYVFrame));
  frameWriter.data().frameNumber = data->imageSequenceNumber;
  frameWriter.data().timestamp = static_cast<double>(data->frameTimestamp);
  frameWriter.data().ballDetected = ballDetected;
  frameWriter.data().centerX = ball[0];
  frameWriter.data().centerY = ball[1];
  frameWriter.data().radius = ball[2];
  frameWriter.data().targetX = data->targetX;
  frameWriter.data().targetY = data->targetY;
  frameWriter.flags() = 1; // indicate new data is available
  frameWriter.exchange();

  // If no ball was detected, increment the failure count
  if (!ballDetected)
  {
    data->ballDetectionFailures++;
  }
  data->newTarget = true;
}

// Function to convert image data to base64 for JSON embedding
std::string EncodeBase64(const std::vector<uint8_t>& data) {
  static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  int val = 0, valb = -6;
  for (uint8_t c : data) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      result.push_back(chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
  while (result.size() % 4) result.push_back('=');
  return result;
}

// Writes the camera frame data to a JSON file and creates a running flag file
RSI_TASK(OutputImage)
{
  static SharedDataHelpers::SPSCStorageManager frameReader(g_frameStorage, false);

  if (!data->initialized) return;

  // Check if new image data is available
  frameReader.exchange();
  if (frameReader.flags() == 0) return;

  try {
    // Construct cv::Mat from YUYVFrame data in the shared buffer
    cv::Mat yuyvMat(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC2, (void*)frameReader.data().yuyvData);
    cv::Mat rgbFrame;
    cv::cvtColor(yuyvMat, rgbFrame, cv::COLOR_YUV2RGB_YUYV);
    // Encode as JPEG with quality 80
    std::vector<uint8_t> jpegBuffer;
    std::vector<int> jpegParams = {cv::IMWRITE_JPEG_QUALITY, 80};
    cv::imencode(".jpg", rgbFrame, jpegBuffer, jpegParams);
    
    // Convert to base64
    std::string base64Image = EncodeBase64(jpegBuffer);
    
    // Write JSON with frame data
    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": " << std::fixed << std::setprecision(0) << frameReader.data().timestamp << ",\n";
    json << "  \"frameNumber\": " << frameReader.data().frameNumber << ",\n";
    json << "  \"width\": " << CameraHelpers::IMAGE_WIDTH << ",\n";
    json << "  \"height\": " << CameraHelpers::IMAGE_HEIGHT << ",\n";
    json << "  \"format\": \"jpeg\",\n";
    json << "  \"imageData\": \"data:image/jpeg;base64," << base64Image << "\",\n";
    json << "  \"imageSize\": " << jpegBuffer.size() << ",\n";
    json << "  \"ballDetected\": " << (frameReader.data().ballDetected ? "true" : "false") << ",\n";
    json << "  \"centerX\": " << std::fixed << std::setprecision(2) << frameReader.data().centerX << ",\n";
    json << "  \"centerY\": " << std::fixed << std::setprecision(2) << frameReader.data().centerY << ",\n";
    json << "  \"radius\": " << std::fixed << std::setprecision(2) << frameReader.data().radius << ",\n";
    json << "  \"targetX\": " << std::fixed << std::setprecision(2) << frameReader.data().targetX << ",\n";
    json << "  \"targetY\": " << std::fixed << std::setprecision(2) << frameReader.data().targetY << ",\n";
    json << "  \"rtTaskRunning\": true\n";
    json << "}";
    
    // Write to file atomically
    std::ofstream dataFile("/tmp/rsi_camera_data.json.tmp");
    if (dataFile.is_open()) {
      dataFile << json.str();
      dataFile.close();
      // Atomic rename to prevent partial reads
      std::rename("/tmp/rsi_camera_data.json.tmp", "/tmp/rsi_camera_data.json");
    }
    
    // Create running flag file
    std::ofstream flagFile("/tmp/rsi_rt_task_running");
    if (flagFile.is_open()) {
      flagFile << "1";
      flagFile.close();
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error writing camera frame JSON: " << e.what() << std::endl;
  }

  // Reset flags after writing
  frameReader.flags() = 0;
}

template<typename T>
bool atomic_max(std::atomic<T>& target, T value) {
  T old = target.load(std::memory_order_relaxed);
  while (old < value &&
         !target.compare_exchange_weak(old, value,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
  }

  // Return true if we updated the max
  return old < value;
}

RSI_TASK(RecordTimingMetrics)
{
  static const uint64_t FIRMWARE_TIMING_DELTA_ADDR = 
    RTMotionControllerGet()->AddressGet(RSIControllerAddressType::RSIControllerAddressTypeFIRMWARE_TIMING_DELTA);

  static const uint64_t NETWORK_TIMING_DELTA_ADDR = 
    RTMotionControllerGet()->AddressGet(RSIControllerAddressType::RSIControllerAddressTypeNETWORK_TIMING_DELTA);

  static const uint64_t NETWORK_TIMING_RECEIVE_DELTA_ADDR = 
    RTMotionControllerGet()->AddressGet(RSIControllerAddressType::RSIControllerAddressTypeNETWORK_TIMING_RECEIVE_DELTA);

  int32_t sampleCount = RTMotionControllerGet()->SampleCounterGet();
  int32_t networkCount = RTMotionControllerGet()->NetworkCounterGet();

  int32_t firmwareTimingDelta = RTMotionControllerGet()->MemoryGet(FIRMWARE_TIMING_DELTA_ADDR);
  int32_t networkTimingDelta = RTMotionControllerGet()->MemoryGet(NETWORK_TIMING_DELTA_ADDR);
  int32_t networkTimingReceiveDelta = RTMotionControllerGet()->MemoryGet(NETWORK_TIMING_RECEIVE_DELTA_ADDR);

  // Update max values and their corresponding sample counts
  if (atomic_max(data->firmwareTimingDeltaMax, firmwareTimingDelta)) {
    atomic_max(data->firmwareTimingDeltaMaxSampleCount, sampleCount);
  }

  if (atomic_max(data->networkTimingDeltaMax, networkTimingDelta)) {
    atomic_max(data->networkTimingDeltaMaxSampleCount, sampleCount);
  }

  if (atomic_max(data->networkTimingReceiveDeltaMax, networkTimingReceiveDelta)) {
    atomic_max(data->networkTimingReceiveDeltaMaxSampleCount, sampleCount);
  }
}