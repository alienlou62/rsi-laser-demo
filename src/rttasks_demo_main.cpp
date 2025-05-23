#include <iostream>

#include <pylon/PylonIncludes.h>

#include "rsi.h"
#include "rttask.h"

#include "rmp_helpers.h"
#include "camera_helpers.h"
#include "timing_helpers.h"
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
  SetupCamera();

  MotionController* controller = RMPHelpers::CreateController();
  RMPHelpers::StartTheNetwork(controller);
  MultiAxis* multiAxis = RMPHelpers::ConfigureAxes(controller);
  multiAxis->AmpEnableSet(true);
  std::shared_ptr<RTTaskManager> manager(RMPHelpers::CreateRTTaskManager("LaserTracking"));

  try
  {
    RTTaskCreationParameters initializeParams("Initialize");
    initializeParams.Repeats = RTTaskCreationParameters::RepeatNone;
    std::shared_ptr<RTTask> initializeTask(manager->TaskSubmit(initializeParams));
    initializeTask->ExecutionCountAbsoluteWait(1);
  }
  catch(const RSI::RapidCode::RsiError& e)
  {
    std::cerr << "RMP exception: " << e.what() << std::endl;
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
  }
  catch(...)
  {
    std::cerr << "Unknown exception occurred." << '\n';
  }

  manager->Shutdown();
  multiAxis->Abort();
  multiAxis->ClearFaults();
}