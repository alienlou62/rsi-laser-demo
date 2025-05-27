#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>

// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

#include "rsi.h"
#include "rttask.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "misc_helpers.h"
#include "motion_control.h"
#include "rmp_helpers.h"
#include "timing_helpers.h"

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

// --- Global Variables ---
Pylon::PylonAutoInitTerm g_pylonAutoInitTerm = Pylon::PylonAutoInitTerm();
Pylon::CInstantCamera g_camera;
Pylon::CGrabResultPtr g_ptrGrabResult;

MotionController *g_controller(nullptr);
MultiAxis *g_multiAxis(nullptr);
FirmwareValue g_targetX = {.Double = 0.0};
FirmwareValue g_targetY = {.Double = 0.0};

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, setting shutdown flag..." << std::endl;
  g_shutdown = true;
}

int main()
{
  const std::chrono::milliseconds loopInterval(100);
  const std::string EXECUTABLE_NAME = "Real-Time Tasks: Laser Tracking";
  PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  std::signal(SIGINT, sigint_handler);

  // --- Pylon Initialization & Camera Loop ---
  CameraHelpers::ConfigureCamera(g_camera);
  CameraHelpers::PrimeCamera(g_camera, g_ptrGrabResult);

  // --- RMP Initialization ---
  g_controller = RMPHelpers::GetController();
  g_multiAxis = RMPHelpers::CreateMultiAxis(g_controller);
  g_multiAxis->AmpEnableSet(true);
  std::shared_ptr<RTTaskManager> manager(RMPHelpers::CreateRTTaskManager("LaserTracking"));

  int grabFailures = 0, processFailures = 0;
  TimingStats loopTiming, retrieveTiming, processingTiming, motionTiming;
  try
  {
    RTTaskCreationParameters initializeParams("Initialize");
    initializeParams.Repeats = RTTaskCreationParameters::RepeatNone;
    initializeParams.EnableTiming = true;
    std::shared_ptr<RTTask> initializeTask(manager->TaskSubmit(initializeParams));
    initializeTask->ExecutionCountAbsoluteWait(1, 1000);

    RTTaskCreationParameters moveMotorsParams("MoveMotors");
    moveMotorsParams.Repeats = RTTaskCreationParameters::RepeatForever;
    moveMotorsParams.Period = 50;
    moveMotorsParams.EnableTiming = true;
    std::shared_ptr<RTTask> moveMotorsTask(manager->TaskSubmit(moveMotorsParams));

    // --- Main Loop ---
    while (!g_shutdown && g_camera.IsGrabbing())
    {
      ScopedRateLimiter rateLimiter(loopInterval);
      auto loopStopwatch = ScopedStopwatch(loopTiming);

      // --- Frame Retrieval ---
      auto retrieveStopwatch = ScopedStopwatch(retrieveTiming);
      std::string grabError;
      if (!CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult, CameraHelpers::TIMEOUT_MS, &grabError))
      {
        std::cerr << grabError << std::endl;
        ++grabFailures;
        continue;
      }
      retrieveStopwatch.Stop();

      // --- Image Processing ---
      auto processingStopwatch = ScopedStopwatch(processingTiming);
      std::string processError;
      if (!ImageProcessing::TryProcessImage(g_ptrGrabResult, g_targetX.Double, g_targetY.Double, &processError))
      {
        std::cerr << processError << std::endl;
        ++processFailures;
        continue;
      }
      processingStopwatch.Stop();

      // --- Motion Control ---
      auto motionStopwatch = ScopedStopwatch(motionTiming);
      manager->GlobalValueSet(g_targetX, "targetX");
      manager->GlobalValueSet(g_targetY, "targetY");
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

  manager->Shutdown();
  g_multiAxis->Abort();
  g_multiAxis->ClearFaults();

  PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
