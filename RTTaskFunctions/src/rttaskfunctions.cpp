// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

#include "rttaskglobals.h"

#include "rmp_helpers.h"
#include "camera_helpers.h"
#include "image_processing.h"
#include "motion_control.h"

#include <iostream>
#include <string>

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
  std::string errorMsg;
  data->cameraPrimed = CameraHelpers::TryPrimeCamera(g_camera, g_ptrGrabResult, CameraHelpers::MAX_RETRIES, &errorMsg);
  std::cout << "Camera primed: " << (data->cameraPrimed ? "Yes" : "No") << std::endl;
  std::cerr << errorMsg << std::endl;
}

// This task retrieves the latest frame from the camera and processes it.
RSI_TASK(ProcessFrame)
{
  double x = 0.0, y = 0.0;
  std::string errorMsg;

  // Image processing pipeline, if any step fails it will set the targets to 0
  if (data->cameraPrimed)
    if (CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult, CameraHelpers::TIMEOUT_MS, &errorMsg))
      ImageProcessing::TryProcessImage(g_ptrGrabResult, x, y);
    else
      std::cerr << errorMsg << std::endl;
  
  data->targetX = x;
  data->targetY = y;
}

// This task moves the motors based on the target positions.
RSI_TASK(MoveMotors)
{
  MotionControl::MoveMotorsWithLimits(RTMultiAxisGet(RMPHelpers::NUM_AXES), data->targetX, data->targetY);
}