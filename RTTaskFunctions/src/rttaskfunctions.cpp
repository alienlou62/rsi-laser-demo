// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

#include "rttaskglobals.h"

#include "camera_helpers.h"

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
  // Grab a frame from the camera
  if (!data->cameraPrimed) return;
  if (!CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult)) return;

  int width = g_ptrGrabResult->GetWidth();
  int height = g_ptrGrabResult->GetHeight();
  const uint8_t* pImageBuffer = static_cast<uint8_t*>(g_ptrGrabResult->GetBuffer());
}