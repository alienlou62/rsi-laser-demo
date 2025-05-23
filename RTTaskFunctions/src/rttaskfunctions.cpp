// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

#include "rttaskglobals.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "motion_control.h"

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

Pylon::PylonAutoInitTerm g_pylonAutoInitTerm = Pylon::PylonAutoInitTerm();
Pylon::CInstantCamera g_camera;
Pylon::CGrabResultPtr g_ptrGrabResult;

// This task initializes the global data structure.
RSI_TASK(Initialize)
{
  data->cameraPrimed = false;
  data->targetX = 0.0;
  data->targetY = 0.0;

  // Initialize the camera and prime it
  g_camera.Attach(Pylon::CTlFactory::GetInstance().CreateFirstDevice());
  g_camera.Open();
  data->cameraPrimed = CameraHelpers::TryPrimeCamera(g_camera, g_ptrGrabResult);
}

// This task retrieves the latest frame from the camera and processes it.
RSI_TASK(ProcessFrame)
{
  double x = 0.0, y = 0.0;

  // Image processing pipeline, if any step fails it will set the targets to 0
  if (data->cameraPrimed)
    if (CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult))
      ImageProcessing::TryProcessImage(g_ptrGrabResult, x, y);

  data->targetX = x;
  data->targetY = y;
}

// This task moves the motors based on the target positions.
RSI_TASK(MoveMotors)
{
  MotionControl::MoveMotorsWithLimits(RTMultiAxisGet(2), data->targetX, data->targetY);
}