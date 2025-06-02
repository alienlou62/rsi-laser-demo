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

// Initializes the global data structure.
RSI_TASK(Initialize)
{
  data->cameraReady = false;
  data->targetX = 0.0;
  data->targetY = 0.0;

  // Setup the multi-axis
  RTMultiAxisGet(0)->AxisAdd(RTAxisGet(0));
  RTMultiAxisGet(0)->AxisAdd(RTAxisGet(1));
  RTMultiAxisGet(0)->ClearFaults();
  RTMultiAxisGet(0)->AmpEnableSet(true);

  g_camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
  CameraHelpers::PrimeCamera(g_camera, g_ptrGrabResult);
  data->cameraReady = true;
}

// Moves the motors based on the target positions.
RSI_TASK(MoveMotors) { MotionControl::MoveMotorsWithLimits(RTMultiAxisGet(0), data->targetX, data->targetY); }

// Processes the image captured by the camera.
RSI_TASK(ProcessImage)
{
  if (!data->cameraReady)
  {
    std::cerr << "Camera is not ready. Skipping image processing." << std::endl;
    return;
  }

  double targetX = data->targetX, targetY = data->targetY;
  if (CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult, 0))
    if (g_ptrGrabResult)
      if (ImageProcessing::TryProcessImage(
            static_cast<uint8_t *>(g_ptrGrabResult->GetBuffer()),
            g_ptrGrabResult->GetWidth(), g_ptrGrabResult->GetHeight(),
            targetX, targetY))
        data->targetX = targetX, data->targetY = targetY;
}