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

  TimingStats convertTiming;
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
      pixelFormat->FromString("BayerBG8", true);
    }
    else
    {
      std::cout << "PixelFormat not available!" << std::endl;
    }

    CameraHelpers::PrimeCamera(camera, ptrGrabResult);

    while(!g_shutdown)
    {
      bool frameGrabbed = CameraHelpers::TryGrabFrame(camera, ptrGrabResult);
      if (!frameGrabbed)
      {
        std::cerr << "Failed to grab frame" << std::endl;
        continue;
      }
      if (!ptrGrabResult)
      {
        std::cerr << "Fatal: Grab failed, result pointer is null after RetrieveResult (unexpected state)." << std::endl;
        return -1;
      }
      if (!ptrGrabResult->GrabSucceeded())
      {
        const int64_t errorCode = ptrGrabResult->GetErrorCode();
        std::ostringstream oss;
        oss << "Grab failed: Code " << errorCode << ", Desc: " << ptrGrabResult->GetErrorDescription();
        std::cerr << oss.str() << std::endl;
        return -1;
      }

      Stopwatch convertStopwatch(convertTiming);
      int width = ptrGrabResult->GetWidth();
      int height = ptrGrabResult->GetHeight();
      std::size_t stride; ptrGrabResult->GetStride(stride);
      uint8_t* buffer = (uint8_t*)ptrGrabResult->GetBuffer();

      // For BayerBG8: single channel, use CV_8UC1
      cv::Mat bayerImg(height, width, CV_8UC1, buffer, stride);

      if (bayerImg.empty())
      {
        std::cerr << "Failed to create cv::Mat from Bayer buffer." << std::endl;
        return -1;
      }

      // Convert BayerBG8 to BGR for display
      cv::Mat bgrImg;
      cv::cvtColor(bayerImg, bgrImg, cv::COLOR_BayerBG2BGR);
      convertStopwatch.Stop();

      if (bgrImg.empty())
      {
        std::cerr << "Failed to convert Bayer to BGR." << std::endl;
        return -1;
      }

      // Show the result
      cv::imshow("Basler Camera - BayerBG8->BGR", bgrImg);
      cv::waitKey(1);
    }
  }
  catch(const GenericException& e)
  {
    std::cerr << "Pylon exception during camera configuration: " << e.GetDescription() << std::endl;
    return -1;
  }
  catch(const Exception& e)
  {
    std::cerr << "OpenCV exception during camera configuration: " << e.what() << std::endl;
    return -1;
  }
  catch(const std::exception& e)
  {
    std::cerr << "std::exception during camera configuration: " << e.what() << std::endl;
    return -1;
  }
  catch(...)
  {
    std::cerr << "Unknown exception during camera configuration." << std::endl;
    return -1;
  }

  printStats("Conversion Timing:", convertTiming);

  /*
  Conversion Timing::YUYV
  Iterations: 742
  Last:       3 ms
  Min:        0 ms
  Max:        39 ms
  Average:    2.02965 ms
  */

  cv::destroyAllWindows();

  PrintFooter(EXECUTABLE_NAME, exitCode);

  return exitCode;
}