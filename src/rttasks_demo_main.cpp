#include <iostream>
#include <csignal>
#include <cmath>

#include <pylon/PylonIncludes.h>

#include "rsi.h"
#include "rttask.h"

#include "rmp_helpers.h"
#include "camera_helpers.h"
#include "timing_helpers.h"
#include "misc_helpers.h"

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

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

void SetupCamera()
{
  Pylon::PylonAutoInitTerm pylonAutoInitTerm = Pylon::PylonAutoInitTerm();
  Pylon::CInstantCamera camera;
  CameraHelpers::ConfigureCamera(camera);
}

int main()
{
  const std::chrono::milliseconds loopInterval(100);
  const std::string EXECUTABLE_NAME = "Real-Time Tasks: Laser Tracking";
  PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  std::signal(SIGQUIT, sigquit_handler);
  std::signal(SIGINT, sigint_handler);

  SetupCamera();

  MotionController* controller = RMPHelpers::GetController();
  MultiAxis* multiAxis = RMPHelpers::CreateMultiAxis(controller);
  // multiAxis->AmpEnableSet(true);
  std::shared_ptr<RTTaskManager> manager(RMPHelpers::CreateRTTaskManager("LaserTracking"));

  try
  {
    RTTaskCreationParameters initializeParams("Initialize");
    initializeParams.Repeats = RTTaskCreationParameters::RepeatNone;
    initializeParams.EnableTiming = true;
    std::shared_ptr<RTTask> initializeTask(manager->TaskSubmit(initializeParams));
    initializeTask->ExecutionCountAbsoluteWait(1);

    RTTaskCreationParameters processFrameParams("ProcessFrame");
    processFrameParams.Repeats = RTTaskCreationParameters::RepeatForever;
    processFrameParams.Period = 100;
    processFrameParams.EnableTiming = true;
    std::shared_ptr<RTTask> processFrameTask(manager->TaskSubmit(processFrameParams));
    processFrameTask->ExecutionCountAbsoluteWait(1);

    // RTTaskCreationParameters moveMotorsParams("MoveMotors");
    // moveMotorsParams.Repeats = RTTaskCreationParameters::RepeatForever;
    // moveMotorsParams.Period = 50;
    // moveMotorsParams.EnableTiming = true;
    // std::shared_ptr<RTTask> moveMotorsTask(manager->TaskSubmit(moveMotorsParams));

    while (!g_shutdown)
    {
      ScopedRateLimiter rateLimiter(loopInterval);

      FirmwareValue cameraPrimed = manager->GlobalValueGet("cameraPrimed");
      FirmwareValue targetX = manager->GlobalValueGet("targetX");
      FirmwareValue targetY = manager->GlobalValueGet("targetY");

      std::cout << "Camera Primed: " << (cameraPrimed.Bool ? "Yes" : "No") << std::endl;
      std::cout << "Target X: " << targetX.Double << ", Target Y: " << targetY.Double << std::endl;
    }
  }
  catch(const RSI::RapidCode::RsiError& e)
  {
    std::cerr << "RMP exception: " << e.what() << std::endl;
    exitCode = 1;
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
    exitCode = 1;
  }
  catch(...)
  {
    std::cerr << "Unknown exception occurred." << '\n';
    exitCode = 1;
  }

  manager->Shutdown();
  multiAxis->Abort();
  multiAxis->ClearFaults();

  PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
