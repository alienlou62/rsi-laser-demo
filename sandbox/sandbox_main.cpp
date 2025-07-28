#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>
#include <cstddef>
#include <cstdint>

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
#include "image_helpers.h"

#ifndef SANDBOX_DIR
#define SANDBOX_DIR ""
#endif // SANDBOX_DIR

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
  MiscHelpers::PrintHeader(EXECUTABLE_NAME);
  std::signal(SIGINT, sigint_handler);
  cv::setNumThreads(0); // Turn off OpenCV threading

  int exitCode = 0;
  

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
