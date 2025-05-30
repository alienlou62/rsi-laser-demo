// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

#include "rttaskglobals.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "rmp_helpers.h"
#include "motion_control.h"

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
  data->targetX = 0.0;
  data->targetY = 0.0;

  // Setup the multi-axis
  RTMultiAxisGet(0)->AxisAdd(RTAxisGet(0));
  RTMultiAxisGet(0)->AxisAdd(RTAxisGet(1));
  RTMultiAxisGet(0)->ClearFaults();
  RTMultiAxisGet(0)->AmpEnableSet(true);

  data->cameraInitialized = false;
  data->cameraPrimed = false;
  try
  {
    g_camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
    g_camera.Open();
    data->cameraInitialized = true;

    CameraHelpers::PrimeCamera(g_camera, g_ptrGrabResult);
    data->cameraPrimed = true;
  }
  catch (const GenericException &e)
  {
    std::cerr << "Pylon exception during initialization: " << e.GetDescription() << std::endl;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Standard exception during initialization: " << e.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "Unknown exception during initialization." << std::endl;
  }
}

// Moves the motors based on the target positions.
RSI_TASK(MoveMotors)
{
  MotionControl::MoveMotorsWithLimits(RTMultiAxisGet(0), data->targetX, data->targetY);
}

// Processes the image captured by the camera.
RSI_TASK(ProcessImage)
{
  if (!data->cameraInitialized || !data->cameraPrimed)
  {
    std::cerr << "Camera not initialized or primed." << std::endl;
    return;
  }

  try
  {
    double targetX = data->targetX, targetY = data->targetY;
    if (CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult, 0))
      if (ImageProcessing::TryProcessImage(g_ptrGrabResult, targetX, targetY))
        data->targetX = targetX, data->targetY = targetY;
      else
        std::cerr << "Failed to process image." << std::endl;
    else
      std::cerr << "Failed to grab frame from camera." << std::endl;
  }
  catch (const GenericException &e)
  {
    std::cerr << "Pylon exception during image processing: " << e.GetDescription() << std::endl;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Standard exception during image processing: " << e.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "Unknown exception during image processing." << std::endl;
  }
}