//camera
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>
//src
#include "rttaskglobals.h"
#include "camera_helpers.h"
#include "image_processing.h"
#include "motion_control.h"
#include "rmp_helpers.h"
#include "shared_memory_helpers.h"

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
// SharedMemoryTripleBuffer<Frame> g_sharedMemory;
TripleBuffer<Frame> g_frameBuffer;
TripleBufferManager<Frame> g_frameBufferWriter(&g_frameBuffer, true);
TripleBufferManager<Frame> g_frameBufferReader(&g_frameBuffer, false);

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
  if (!data->initialized) return;
  if (!data->motionEnabled) return;
  if (!data->multiAxisReady) return;

  // Only execute if a new target is set
  if (!data->newTarget.exchange(false)) return;

  MotionControl::MoveMotorsWithLimits(RTMultiAxisGet(0), data->targetX, data->targetY);
}

// Processes the image captured by the camera.
RSI_TASK(DetectBall)
{
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
  memcpy(g_frameBufferWriter->yuyvData, yuyvFrame.data, sizeof(CameraHelpers::YUYVFrame));
  g_frameBufferWriter->frameNumber = data->imageSequenceNumber;
  g_frameBufferWriter->timestamp = static_cast<double>(data->frameTimestamp);
  g_frameBufferWriter->ballDetected = ballDetected;
  g_frameBufferWriter->centerX = ball[0];
  g_frameBufferWriter->centerY = ball[1];
  g_frameBufferWriter->confidence = confidence;
  g_frameBufferWriter->targetX = data->targetX;
  g_frameBufferWriter->targetY = data->targetY;
  g_frameBufferWriter.flags() = 1; // indicate new data is available
  g_frameBufferWriter.swap_buffers();

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

// Function to write camera frame data as JSON for C# UI
void WriteCameraFrameJSON(const Frame& frameData) {
  try {
    // Construct cv::Mat from YUYVFrame data in frameData
    cv::Mat yuyvMat(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC2, (void*)frameData.yuyvData);
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
    json << "  \"timestamp\": " << std::fixed << std::setprecision(0) << frameData.timestamp << ",\n";
    json << "  \"frameNumber\": " << frameData.frameNumber << ",\n";
    json << "  \"width\": " << CameraHelpers::IMAGE_WIDTH << ",\n";
    json << "  \"height\": " << CameraHelpers::IMAGE_HEIGHT << ",\n";
    json << "  \"format\": \"jpeg\",\n";
    json << "  \"imageData\": \"data:image/jpeg;base64," << base64Image << "\",\n";
    json << "  \"imageSize\": " << jpegBuffer.size() << ",\n";
    json << "  \"ballDetected\": " << (frameData.ballDetected ? "true" : "false") << ",\n";
    json << "  \"centerX\": " << std::fixed << std::setprecision(2) << frameData.centerX << ",\n";
    json << "  \"centerY\": " << std::fixed << std::setprecision(2) << frameData.centerY << ",\n";
    json << "  \"confidence\": " << std::fixed << std::setprecision(3) << frameData.confidence << ",\n";
    json << "  \"targetX\": " << std::fixed << std::setprecision(2) << frameData.targetX << ",\n";
    json << "  \"targetY\": " << std::fixed << std::setprecision(2) << frameData.targetY << ",\n";
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
}

RSI_TASK(OutputImage)
{
  if (!data->initialized) return;

  // Check if new image data is available
  g_frameBufferReader.swap_buffers();
  if (g_frameBufferReader.flags() == 0) return;

  WriteCameraFrameJSON(*g_frameBufferReader);

  // Reset flags after writing
  g_frameBufferReader.flags() = 0;
}
