#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <memory>

#include "rsi.h"
#include "rttask.h"

#include "timing_helpers.h"
#include "misc_helpers.h"
#include "rmp_helpers.h"

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

constexpr std::chrono::milliseconds LOOP_INTERVAL(50); // milliseconds
constexpr int32_t TASK_WAIT_TIMEOUT = 1000;
constexpr int32_t PROCESS_TASK_PERIOD = 15;
constexpr int32_t MOVE_TASK_PERIOD = 15;

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

std::shared_ptr<RTTask> SubmitRepeatingTask(
  std::shared_ptr<RTTaskManager>& manager, const std::string& taskName,
  int32_t period = RTTaskCreationParameters::PeriodDefault,
  int32_t phase = RTTaskCreationParameters::PhaseDefault
  )
{
  RTTaskCreationParameters repeatingParams(taskName.c_str());
  repeatingParams.Repeats = RTTaskCreationParameters::RepeatForever;
  repeatingParams.Period = period;
  repeatingParams.Phase = phase;
  repeatingParams.EnableTiming = true;
  std::shared_ptr<RTTask> repeatingTask(manager->TaskSubmit(repeatingParams), RTTaskDeleter);
  return repeatingTask;
}

void printTaskTiming(std::shared_ptr<RTTask> task, const std::string& taskName)
{
  if (!task) return;

  RTTaskStatus status = task->StatusGet();
  std::cout << "Task: " << taskName << std::endl;
  std::cout << "Execution count: " << status.ExecutionCount << std::endl;
  std::cout << "Last execution time: " << status.ExecutionTimeLast << " ns" << std::endl;
  std::cout << "Maximum execution time: " << status.ExecutionTimeMax << " ns" << std::endl;
  std::cout << "Average execution time: " << status.ExecutionTimeMean << " ns" << std::endl << std::endl;
}

int main()
{
  const std::string EXECUTABLE_NAME = "Real-Time Tasks: Laser Tracking";
  PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  std::signal(SIGINT, sigint_handler);

  // --- RMP Initialization ---
  MotionController* controller = RMPHelpers::GetController();
  MultiAxis* multiAxis = RMPHelpers::CreateMultiAxis(controller);
  multiAxis->AmpEnableSet(true);

  std::shared_ptr<RTTaskManager> manager(RMPHelpers::CreateRTTaskManager("LaserTracking"));
  try
  {
    std::shared_ptr<RTTaskManager> manager(RMPHelpers::CreateRTTaskManager("LaserTracking"), RTTaskManagerDeleter);
    SubmitSingleShotTask(manager, "Initialize");

    std::shared_ptr<RTTask> processImageTask = SubmitRepeatingTask(manager, "ProcessImage", PROCESS_TASK_PERIOD);
    std::shared_ptr<RTTask> moveMotorsTask = SubmitRepeatingTask(manager, "MoveMotors", MOVE_TASK_PERIOD, 1);

    // --- Main Loop ---
    while (!g_shutdown)
    {
      RateLimiter rateLimiter(LOOP_INTERVAL);

      // Print the value of all global variables
      FirmwareValue cameraReady = manager->GlobalValueGet("cameraReady");
      if (!cameraReady.Bool)
      {
        std::cerr << "Error: Camera is not ready." << std::endl;
        break;
      }

      FirmwareValue targetX = manager->GlobalValueGet("targetX");
      std::cout << "Target X: " << targetX.Double << std::endl;

      FirmwareValue targetY = manager->GlobalValueGet("targetY");
      std::cout << "Target Y: " << targetY.Double << std::endl;
    }

    // Print task timing information
    printTaskTiming(moveMotorsTask, "MoveMotors");
    printTaskTiming(processImageTask, "ProcessImage");
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

  PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
