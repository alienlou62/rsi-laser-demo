// --- Camera and Image Processing Headers ---
// #include <opencv2/opencv.hpp>
// #include <pylon/PylonIncludes.h>

#include "rttaskglobals.h"

// #include "camera_helpers.h"
// #include "image_processing.h"
#include "rmp_helpers.h"
#include "motion_control.h"

#include <iostream>
#include <string>

// using namespace Pylon;

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

// Global variable (different from RTTASK_GLOBAL)
// std::atomic<CInstantCamera*> g_camera(nullptr);
// std::atomic<CGrabResultPtr*> g_ptrGrabResult(nullptr);

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

  // data->cameraInitialized = false;
  // data->cameraPrimed = false;
  // PylonInitialize();
  // try
  // {
  //   std::unique_ptr<CInstantCamera> camera = std::make_unique<CInstantCamera>();
  //   camera->Attach(CTlFactory::GetInstance().CreateFirstDevice());
  //   camera->Open();
  //   g_camera.store(camera.release());
  //   g_ptrGrabResult.store(new CGrabResultPtr());
  //   data->cameraInitialized = true;

  //   CameraHelpers::PrimeCamera(*g_camera, *g_ptrGrabResult);
  //   data->cameraPrimed = true;
  // }
  // catch (const GenericException &e)
  // {
  //   std::cerr << "Pylon exception during initialization: " << e.GetDescription() << std::endl;
  // }
  // catch (const std::exception &e)
  // {
  //   std::cerr << "Standard exception during initialization: " << e.what() << std::endl;
  // }
  // catch (...)
  // {
  //   std::cerr << "Unknown exception during initialization." << std::endl;
  // }
}

// Cleanup any resources or perform finalization tasks.
RSI_TASK(Cleanup)
{
  // try
  // {
  //   CGrabResultPtr* grabResult = g_ptrGrabResult.exchange(nullptr);
  //   if (grabResult) delete grabResult;
    
  //   CInstantCamera* camera = g_camera.exchange(nullptr);
  //   if (camera) delete camera;

  //   data->cameraInitialized = false;
  //   data->cameraPrimed = false;
  // }
  // catch (const GenericException &e)
  // {
  //   std::cerr << "Pylon exception during cleanup: " << e.GetDescription() << std::endl;
  // }
  // catch (const std::exception &e)
  // {
  //   std::cerr << "Standard exception during cleanup: " << e.what() << std::endl;
  // }
  // catch (...)
  // {
  //   std::cerr << "Unknown exception during cleanup." << std::endl;
  // }
  // PylonTerminate();
}

// Moves the motors based on the target positions.
RSI_TASK(MoveMotors)
{
  MotionControl::MoveMotorsWithLimits(RTMultiAxisGet(0), data->targetX, data->targetY);
}