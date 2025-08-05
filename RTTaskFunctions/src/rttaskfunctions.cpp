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
SharedMemoryTripleBuffer<Frame> g_sharedMemory(SHARED_MEMORY_NAME, true);
TripleBufferManager<Frame> g_tripleBufferManager(g_sharedMemory.get(), true);

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
  memcpy(g_tripleBufferManager->yuyvData, yuyvFrame.data, sizeof(CameraHelpers::YUYVFrame));
  g_tripleBufferManager->frameNumber = data->imageSequenceNumber;
  g_tripleBufferManager->timestamp = static_cast<double>(data->frameTimestamp);
  g_tripleBufferManager->ballDetected = ballDetected;
  g_tripleBufferManager->centerX = ball[0];
  g_tripleBufferManager->centerY = ball[1];
  g_tripleBufferManager->confidence = confidence;
  g_tripleBufferManager->targetX = data->targetX;
  g_tripleBufferManager->targetY = data->targetY;
  g_tripleBufferManager.flags() = 1; // indicate new data is available
  g_tripleBufferManager.swap_buffers();

  // If no ball was detected, increment the failure count
  if (!ballDetected)
  {
    data->ballDetectionFailures++;
  }
  data->newTarget = true;
}