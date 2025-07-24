#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>

#include <pylon/PylonIncludes.h>

#include "rsi.h"
#include "rttask.h"

#include "timing_helpers.h"
#include "misc_helpers.h"
#include "rmp_helpers.h"
#include "camera_helpers.h"

using namespace Pylon;

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

constexpr std::chrono::milliseconds LOOP_INTERVAL(50); // milliseconds

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, setting shutdown flag..." << std::endl;
  g_shutdown = true;
}

void PrintTaskTiming(RTTask &task, const std::string &taskName)
{
  RTTaskStatus status = task.StatusGet();
  // Lambda to convert nanoseconds to milliseconds
  auto nsToMs = [](uint64_t ns)
  { return static_cast<double>(ns) / 1e6; };
  std::cout << "Task: " << taskName << std::endl;
  std::cout << "Execution count: " << status.ExecutionCount << std::endl;
  std::cout << "Last execution time: " << nsToMs(status.ExecutionTimeLast) << " ms" << std::endl;
  std::cout << "Maximum execution time: " << nsToMs(status.ExecutionTimeMax) << " ms" << std::endl;
  std::cout << "Average execution time: " << nsToMs(status.ExecutionTimeMean) << " ms" << std::endl;
  std::cout << "Last start time delta: " << nsToMs(status.StartTimeDeltaLast) << " ms" << std::endl;
  std::cout << "Maximum start time delta: " << nsToMs(status.StartTimeDeltaMax) << " ms" << std::endl;
  std::cout << "Average start time delta: " << nsToMs(status.StartTimeDeltaMean) << " ms" << std::endl
            << std::endl;
}

bool CheckRTTaskStatus(RTTask &task, const std::string &taskName)
{
  try
  {
    RTTaskStatus status = task.StatusGet();
    if (status.ErrorMessage[0] != '\0')
    {
      std::cerr << "Error in " << taskName << ": " << status.ErrorMessage << std::endl;
      return false;
    }

    if (status.State == RTTaskState::Dead)
    {
      std::cerr << "Task " << taskName << " is dead." << std::endl;
      return false;
    }
  }
  catch (const RsiError &e)
  {
    std::cerr << "Failed to get status of " << taskName << ": " << e.what() << std::endl;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Unexpected error while getting " << taskName << " status: " << e.what() << std::endl;
  }
  // If we failed to get the status, don't treat that as a fatal error.
  return true;
}

int main()
{
  const std::string EXECUTABLE_NAME = "Real-Time Tasks: Laser Tracking";
  MiscHelpers::PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  std::signal(SIGINT, sigint_handler);

  // SetupCamera();

  // --- RMP Initialization ---
  MotionController *controller = RMPHelpers::GetController();
  MultiAxis *multiAxis = RMPHelpers::CreateMultiAxis(controller);
  RTTaskManager manager(RMPHelpers::CreateRTTaskManager("LaserTracking"));

  try
  {
    FirmwareValue cameraReady = manager.GlobalValueGet("cameraReady");
    if (!cameraReady.Bool)
    {
      std::cerr << "Error: Camera is not ready." << std::endl;
      return -1;
    }

    FirmwareValue multiAxisReady = manager.GlobalValueGet("multiAxisReady");
    if (!multiAxisReady.Bool)
    {
      std::cerr << "Error: MultiAxis is not ready." << std::endl;
      return -1;
    }

    FirmwareValue motionEnabled = {.Bool = true};
    manager.GlobalValueSet(motionEnabled, "motionEnabled");

    // --- Main Loop ---
    while (!g_shutdown)
    {
      RateLimiter rateLimiter(LOOP_INTERVAL);

      FirmwareValue targetX = manager.GlobalValueGet("targetX");
      std::cout << "Target X: " << targetX.Double << std::endl;

      FirmwareValue targetY = manager.GlobalValueGet("targetY");
      std::cout << "Target Y: " << targetY.Double << std::endl;

      if (!CheckRTTaskStatus(ballDetectionTask, "Ball Detection Task"))
      {
        g_shutdown = true;
        exitCode = 1;
      }

      if (!CheckRTTaskStatus(motionTask, "Motion Task"))
      {
        g_shutdown = true;
        exitCode = 1;
      }

      // Check if the MultiAxis is in an error state
      try
      {
        RMPHelpers::CheckErrors(multiAxis);
      }
      catch (const std::exception &e)
      {
        std::cerr << e.what() << '\n';
      }

      // Check if the MultiAxis is in an error state and print the source of the error
      RSIState state = multiAxis->StateGet();
      if (state == RSIState::RSIStateERROR ||
          state == RSIState::RSIStateSTOPPING_ERROR)
      {
        std::cout << "MultiAxis is in state: " << RMPHelpers::RSIStateToString(state) << std::endl;
        RSISource source = multiAxis->SourceGet();
        std::cerr << "Error Source: " << multiAxis->SourceNameGet(source) << std::endl;
      }
    }

    // Print task timing information
    PrintTaskTiming(motionTask, "Motion Task");
    PrintTaskTiming(ballDetectionTask, "Ball Detection Task");
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

  // --- Cleanup ---
  FirmwareValue motionEnabled = {.Bool = false};
  manager.GlobalValueSet(motionEnabled, "motionEnabled");
  multiAxis->Abort();
  multiAxis->ClearFaults();

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
