#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>

// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
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
    CEnumerationPtr pixelFormatEnum(nodeMap.GetNode("PixelFormat"));
    if (IsAvailable(pixelFormatEnum) && IsReadable(pixelFormatEnum))
    {
      // Get all available entries
      StringList_t entries;
      pixelFormatEnum->GetSymbolics(entries);
      std::cout << "Available Pixel Formats:" << std::endl;
      for (const auto& entry : entries)
      {
        CEnumEntryPtr enumEntry(pixelFormatEnum->GetEntryByName(entry));
        if (IsAvailable(enumEntry) && IsReadable(enumEntry))
        {
          std::cout << " - " << entry << std::endl;
        }
      }
    }
    else
    {
      std::cout << "PixelFormat enumeration not available!" << std::endl;
    }
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