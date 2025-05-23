#include <iostream>

#include <pylon/PylonIncludes.h>

#include "rsi.h"
#include "rttask.h"

#include "rmp_helpers.h"
#include "camera_helpers.h"
#include "misc_helpers.h"

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

void SetupCamera()
{
  Pylon::PylonAutoInitTerm pylonAutoInitTerm = Pylon::PylonAutoInitTerm();
  Pylon::CInstantCamera camera;
  CameraHelpers::ConfigureCamera(camera);
}

int main()
{
  const std::string EXECUTABLE_NAME = "Real-Time Tasks: Laser Tracking";
  PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  SetupCamera();

  MotionController* controller = RMPHelpers::GetController();
  MultiAxis* multiAxis = RMPHelpers::CreateMultiAxis(controller);
  multiAxis->AmpEnableSet(true);
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
    processFrameParams.Period = 50; // 50ms
    processFrameParams.EnableTiming = true;
    std::shared_ptr<RTTask> processFrameTask(manager->TaskSubmit(processFrameParams));
    processFrameTask->ExecutionCountAbsoluteWait(1);
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

  multiAxis->Abort();
  multiAxis->ClearFaults();

  PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}