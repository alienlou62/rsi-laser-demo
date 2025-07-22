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
constexpr int32_t TASK_WAIT_TIMEOUT = 1000;            // 1 seconds, for task execution wait
constexpr int32_t INIT_TIMEOUT = 15000;                // 15 seconds, initialization can take a while
constexpr int32_t DETECTION_TASK_PERIOD = 3;
constexpr int32_t MOVE_TASK_PERIOD = 3;

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, setting shutdown flag..." << std::endl;
  g_shutdown = true;
}

void SubmitSingleShotTask(RTTaskManager &manager, const std::string &taskName, int32_t timeoutMs = TASK_WAIT_TIMEOUT)
{
  RTTaskCreationParameters singleShotParams(taskName.c_str());
  singleShotParams.Repeats = RTTaskCreationParameters::RepeatNone;
  singleShotParams.EnableTiming = true;
  RTTask singleShotTask(RMPHelpers::SubmitRTTask(manager, singleShotParams));
  singleShotTask.ExecutionCountAbsoluteWait(1, timeoutMs);
}

RTTask SubmitRepeatingTask(
    RTTaskManager &manager, const std::string &taskName,
    int32_t period = RTTaskCreationParameters::PeriodDefault,
    int32_t phase = RTTaskCreationParameters::PhaseDefault,
    TaskPriority priority = RTTaskCreationParameters::PriorityDefault,
    int32_t timeoutMs = TASK_WAIT_TIMEOUT)
{
  RTTaskCreationParameters repeatingParams(taskName.c_str());
  repeatingParams.Repeats = RTTaskCreationParameters::RepeatForever;
  repeatingParams.Period = period;
  repeatingParams.Phase = phase;
  repeatingParams.Priority = priority;
  repeatingParams.EnableTiming = true;
  RTTask repeatingTask(RMPHelpers::SubmitRTTask(manager, repeatingParams));
  repeatingTask.ExecutionCountAbsoluteWait(1, timeoutMs);
  repeatingTask.TimingReset(); // Reset timing stats for the task after the first run
  return repeatingTask;
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

void SetupCamera()
{
  PylonAutoInitTerm pylonAutoInitTerm;
  CInstantCamera camera;
  CameraHelpers::ConfigureCamera(camera);
  CGrabResultPtr ptr;
  CameraHelpers::PrimeCamera(camera, ptr);
  camera.Close();
  camera.DestroyDevice();
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

  try
  {
    RTTaskManager manager(RMPHelpers::CreateRTTaskManager("LaserTracking"));
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Adjust to 300–500ms if needed
    SubmitSingleShotTask(manager, "Initialize", INIT_TIMEOUT);

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

    RTTask ballDetectionTask = SubmitRepeatingTask(manager, "DetectBall", DETECTION_TASK_PERIOD, 0, TaskPriority::Low);
    RTTask motionTask = SubmitRepeatingTask(manager, "MoveMotors", MOVE_TASK_PERIOD, 1, TaskPriority::High);
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
  multiAxis->Abort();
  multiAxis->ClearFaults();

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
