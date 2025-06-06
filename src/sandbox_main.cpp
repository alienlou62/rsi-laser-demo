#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>

// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

// --- RMP/RSI Headers ---
#include "rsi.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "misc_helpers.h"
#include "motion_control.h"
#include "rmp_helpers.h"
#include "timing_helpers.h"

#ifndef SOURCE_DIR
#define SOURCE_DIR ""
#endif // SOURCE_DIR

#ifndef CONFIG_FILE
#define CONFIG_FILE ""
#endif // CONFIG_FILE

using namespace RSI::RapidCode;
using namespace cv;
using namespace Pylon;
using namespace GenApi;

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, toggling shutdown flag..." << std::endl;
  g_shutdown = true;
}

// --- Main Function ---
int main()
{
  const std::string EXECUTABLE_NAME = "Sandbox";
  PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  std::signal(SIGINT, sigint_handler);

  // --- Pylon & Camera Initialization ---
  PylonAutoInitTerm pylonAutoInitTerm = PylonAutoInitTerm();
  CInstantCamera camera;
  CGrabResultPtr ptrGrabResult;

  try
  {
    camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
    std::cout << "Using device: " << camera.GetDeviceInfo().GetModelName() << std::endl;
    camera.Open();

    CFeaturePersistence::Load(CONFIG_FILE, &camera.GetNodeMap());
    INodeMap &nodeMap = camera.GetNodeMap();

    CEnumerationPtr pixelFormat(nodeMap.GetNode("PixelFormat"));
    if (IsAvailable(pixelFormat) && IsReadable(pixelFormat))
    {
      pixelFormat->FromString("YUV422_YUYV_Packed", true);
    }
    else
    {
      std::cout << "PixelFormat not available!" << std::endl;
    }

    CameraHelpers::PrimeCamera(camera, ptrGrabResult);

    std::cout << "PayloadType: " << ptrGrabResult->GetPayloadType() << std::endl;
    std::cout << "PixelType: " << ptrGrabResult->GetPixelType() << std::endl;
    std::cout << "Height: " << ptrGrabResult->GetHeight() << std::endl;
    std::cout << "Width: " << ptrGrabResult->GetWidth() << std::endl;
    std::cout << "OffsetX: " << ptrGrabResult->GetOffsetX() << std::endl;
    std::cout << "OffsetY: " << ptrGrabResult->GetOffsetY() << std::endl;
    std::cout << "PaddingX: " << ptrGrabResult->GetPaddingX() << std::endl;
    std::cout << "PaddingY: " << ptrGrabResult->GetPaddingY() << std::endl;
    std::cout << "Payload size: " << ptrGrabResult->GetPayloadSize() << std::endl;
    std::cout << "Buffer size: " << ptrGrabResult->GetBufferSize() << std::endl;
    std::size_t stride; ptrGrabResult->GetStride(stride);
    std::cout << "Stride: " << stride << std::endl;
    std::cout << "Image size: " << ptrGrabResult->GetImageSize() << std::endl;

    // Print error code/message just in case
    std::cout << "Error code: " << ptrGrabResult->GetErrorCode() << std::endl;
    std::cout << "Error description: " << ptrGrabResult->GetErrorDescription() << std::endl;

    /*
    PayloadType: 0
    PixelType: 34603058
    Height: 480
    Width: 640
    OffsetX: 16
    OffsetY: 16
    PaddingX: 0
    PaddingY: 0
    Payload size: 614400
    Buffer size: 614400
    Stride: 1280
    Image size: 614400
    */
  }
  catch(const GenericException& e)
  {
    std::cerr << "Pylon exception during camera configuration: " << e.GetDescription() << std::endl;
    return -1;
  }

  // while (!g_shutdown)
  // {

  // }

  cv::destroyAllWindows();

  PrintFooter(EXECUTABLE_NAME, exitCode);

  return exitCode;
}