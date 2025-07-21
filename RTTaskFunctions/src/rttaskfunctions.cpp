// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

#include "rttaskglobals.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "motion_control.h"
#include "rmp_helpers.h"

#include <iostream>
#include <string>

using namespace Pylon;

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

// Global variable (different from RTTASK_GLOBAL)
PylonAutoInitTerm g_PylonAutoInitTerm;
CInstantCamera g_camera;
CGrabResultPtr g_ptrGrabResult;

// Initializes the global data structure and sets up the camera and multi-axis.
RSI_TASK(Initialize)
{
  // Initialize the global data
  data->cameraReady = false;
  data->multiAxisReady = false;
  data->motionEnabled = false;
  data->newTarget = false;
  data->targetX = 0.0;
  data->targetY = 0.0;

  try
  {
    CameraHelpers::ConfigureCamera(g_camera);
    CameraHelpers::PrimeCamera(g_camera, g_ptrGrabResult);
    data->cameraReady = true;
  }
  catch (const GenericException& e)
  {
    throw std::runtime_error(e.what());
  }
  catch (const std::exception& e)
  {
    throw std::runtime_error(e.what());
  }
  catch (...)
  {
    throw std::runtime_error("Unknown exception during camera initialization.");
  }

  // Setup the multi-axis
  RTMultiAxisGet(0)->Abort();
  RTMultiAxisGet(0)->ClearFaults();
  RTMultiAxisGet(0)->MotionAttributeMaskOffSet(RSIMotionAttrMask::RSIMotionAttrMaskAPPEND);
  RTMultiAxisGet(0)->AmpEnableSet(true);

  data->multiAxisReady = true;
}

// Moves the motors based on the target positions.
RSI_TASK(MoveMotors)
{
  if (!data->motionEnabled) return;
  if (!data->multiAxisReady) return;

  // Check if there is a new target, if not, return early
  if (!data->newTarget.exchange(false)) return;

  MotionControl::MoveMotorsWithLimits(RTMultiAxisGet(0), data->targetX, data->targetY);
}

// Processes the image captured by the camera.
RSI_TASK(DetectBall)
{
  if (!data->cameraReady) return;

  bool frameGrabbed = CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult, 0);
  if (!frameGrabbed) return;

  // Record the axis positions at the time of frame grab
  double initialX(RTAxisGet(0)->ActualPositionGet());
  double initialY(RTAxisGet(1)->ActualPositionGet());

  cv::Mat yuyvFrame = ImageProcessing::WrapYUYVBuffer(
      static_cast<uint8_t *>(g_ptrGrabResult->GetBuffer()),
      CameraHelpers::IMAGE_WIDTH, CameraHelpers::IMAGE_HEIGHT);
  double offsetX(0.0), offsetY(0.0);
  bool targetFound = ImageProcessing::TryDetectBall(yuyvFrame, offsetX, offsetY);
  if (!targetFound) return;

  // Calculate the target positions based on the offsets and the position at the time of frame grab
  data->targetX = initialX + offsetX;
  data->targetY = initialY + offsetY;
  data->newTarget = true;
}