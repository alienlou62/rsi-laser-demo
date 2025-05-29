#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <memory>

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

constexpr int32_t TASK_WAIT_TIMEOUT = 1000;
constexpr int32_t TASK_PERIOD = 100;

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, setting shutdown flag..." << std::endl;
  g_shutdown = true;
}

template <typename T, typename F>
void SafeDeleter(T* ptr, F&& shutdown, const char* context) {
  if (ptr) {
    try {
      shutdown(ptr);
    } catch (const RsiError& e) {
      std::cerr << "Exception in " << context << " (RsiError): " << e.what() << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "Exception in " << context << " (std::exception): " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "Unknown exception in " << context << std::endl;
    }
    delete ptr;
  }
}

void RTTaskDeleter(RTTask* ptr) {
  SafeDeleter(ptr, [](RTTask* t){ t->Stop(); }, "RTTaskDeleter");
}

void RTTaskManagerDeleter(RTTaskManager* ptr) {
  SafeDeleter(ptr, [](RTTaskManager* m){ m->Shutdown(); }, "RTTaskManagerDeleter");
}

void SubmitSingleShotTask(std::shared_ptr<RTTaskManager>& manager, const std::string& taskName, int32_t timeoutMs = TASK_WAIT_TIMEOUT)
{
  RTTaskCreationParameters singleShotParams(taskName.c_str());
  singleShotParams.Repeats = RTTaskCreationParameters::RepeatNone;
  singleShotParams.EnableTiming = true;
  std::shared_ptr<RTTask> singleShotTask(manager->TaskSubmit(singleShotParams), RTTaskDeleter);
  singleShotTask->ExecutionCountAbsoluteWait(1, timeoutMs);
}

std::shared_ptr<RTTask> SubmitRepeatingTask(std::shared_ptr<RTTaskManager>& manager, const std::string& taskName, int32_t periodMs = TASK_PERIOD)
{
  RTTaskCreationParameters repeatingParams(taskName.c_str());
  repeatingParams.Repeats = RTTaskCreationParameters::RepeatForever;
  repeatingParams.Period = periodMs;
  repeatingParams.EnableTiming = true;
  std::shared_ptr<RTTask> repeatingTask(manager->TaskSubmit(repeatingParams), RTTaskDeleter);
  return repeatingTask;
}

int main()
{
  const std::chrono::milliseconds loopInterval(100);
  const std::string EXECUTABLE_NAME = "Real-Time Tasks: Laser Tracking";
  PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

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

  std::shared_ptr<RTTaskManager> manager(RMPHelpers::CreateRTTaskManager("LaserTracking"));
  FirmwareValue targetX = {.Double = 0.0}, targetY = {.Double = 0.0};
  
  int grabFailures = 0, processFailures = 0;
  TimingStats loopTiming, retrieveTiming, processingTiming, motionTiming;
  try
  {
    std::shared_ptr<RTTaskManager> manager(RMPHelpers::CreateRTTaskManager("LaserTracking"), RTTaskManagerDeleter);
    SubmitSingleShotTask(manager, "Initialize");

    std::shared_ptr<RTTask> moveMotorsTask = SubmitRepeatingTask(manager, "MoveMotors");

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
      if (!ImageProcessing::TryProcessImage(ptrGrabResult, targetX.Double, targetY.Double, &processError))
      {
        std::cerr << processError << std::endl;
        ++processFailures;
        continue;
      }
      processingStopwatch.Stop();

      // --- Motion Control ---
      manager->GlobalValueSet(targetX, "targetX");
      manager->GlobalValueSet(targetY, "targetY");
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

  printStats("Loop", loopTiming);
  printStats("Retrieval", retrieveTiming);
  printStats("Processing", processingTiming);

  // --- Cleanup ---
  multiAxis->Abort();
  multiAxis->ClearFaults();

  PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
