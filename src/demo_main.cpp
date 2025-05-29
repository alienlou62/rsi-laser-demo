#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>

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

using namespace RSI::RapidCode;

volatile sig_atomic_t g_shutdown = false;
void sigquit_handler(int signal)
{
  std::cout << "SIGQUIT handler ran, setting shutdown flag..." << std::endl;
  g_shutdown = true;
}

volatile sig_atomic_t g_paused = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, toggling paused flag..." << std::endl;
  g_paused = !g_paused;
}

// --- Main Function ---
int main()
{
  const std::chrono::milliseconds loopInterval(5); // 5ms loop interval
  const std::string EXECUTABLE_NAME = "Pylon_RSI_Tracking_BayerOnly";
  PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  std::signal(SIGQUIT, sigquit_handler);
  std::signal(SIGINT, sigint_handler);

  // --- Pylon & Camera Initialization ---
  Pylon::PylonAutoInitTerm pylonAutoInitTerm = Pylon::PylonAutoInitTerm();
  Pylon::CInstantCamera camera;
  Pylon::CGrabResultPtr ptrGrabResult;

  CameraHelpers::ConfigureCamera(camera);
  CameraHelpers::PrimeCamera(camera, ptrGrabResult);

  // --- RMP Initialization ---
  MotionController* controller = RMPHelpers::GetController();
  MultiAxis* multiAxis = RMPHelpers::CreateMultiAxis(controller);
  multiAxis->AmpEnableSet(true);
  double targetX = 0.0, targetY = 0.0;

  int grabFailures = 0, processFailures = 0;
  TimingStats loopTiming, retrieveTiming, processingTiming, motionTiming;
  try
  {
    // --- Main Loop ---
    while (!g_shutdown && camera.IsGrabbing())
    {
      RateLimiter rateLimiter(loopInterval);
      auto loopStopwatch = Stopwatch(loopTiming);

      // --- Frame Retrieval ---
      auto retrieveStopwatch = Stopwatch(retrieveTiming);
      std::string grabError;
      if (!CameraHelpers::TryGrabFrame(camera, ptrGrabResult, CameraHelpers::TIMEOUT_MS, &grabError))
      {
        std::cerr << grabError << std::endl;
        ++grabFailures;
        continue;
      }
      retrieveStopwatch.Stop();

      // --- Image Processing ---
      auto processingStopwatch = Stopwatch(processingTiming);
      std::string processError;
      if (!ImageProcessing::TryProcessImage(ptrGrabResult, targetX, targetY, &processError))
      {
        std::cerr << processError << std::endl;
        ++processFailures;
        continue;
      }
      processingStopwatch.Stop();

      // --- Motion Control ---
      auto motionStopwatch = Stopwatch(motionTiming);
      if (g_paused)
      {
        MotionControl::MoveMotorsWithLimits(multiAxis, 0.0, 0.0);
      }
      else
      {
        try
        {
          MotionControl::MoveMotorsWithLimits(multiAxis, targetX, targetY);
        }
        catch (const RsiError &e)
        {
          std::cerr << "RMP exception during motion control: " << e.what() << std::endl;
          g_shutdown = true;
        }
      }
      motionStopwatch.Stop();
    }
  }
  catch (const cv::Exception &e)
  {
    std::cerr << "OpenCV exception: " << e.what() << std::endl;
    exitCode = 1;
  }
  catch (const Pylon::GenericException &e)
  {
    std::cerr << "Pylon exception: " << e.GetDescription() << std::endl;
    exitCode = 1;
  }
  catch (const RsiError &e)
  {
    std::cerr << "RMP exception: " << e.what() << std::endl;
    exitCode = 1;
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << std::endl;
    exitCode = 1;
  }
  catch (...)
  {
    std::cerr << "Unknown exception occurred." << std::endl;
    exitCode = 1;
  }

  // Cleanup
  multiAxis->Abort();
  multiAxis->ClearFaults();

  // Print loop statistics
  printStats("Loop", loopTiming);
  printStats("Retrieve", retrieveTiming);
  printStats("Processing", processingTiming);
  printStats("Motion", motionTiming);

  std::cout << "--------------------------------------------" << std::endl;
  std::cout << "Grab Failures:     " << grabFailures << std::endl;
  std::cout << "Process Failures:  " << processFailures << std::endl;

  cv::destroyAllWindows();

  PrintFooter(EXECUTABLE_NAME, exitCode);

  return exitCode;
}