#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <thread>
#include <fstream>
#include <sstream>

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
constexpr int32_t DETECTION_TASK_PERIOD = 1;
constexpr int32_t MOVE_TASK_PERIOD = 1;

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
  // std::cout << "Last execution time: " << nsToMs(status.ExecutionTimeLast) << " ms" << std::endl;
  std::cout << "Maximum execution time: " << nsToMs(status.ExecutionTimeMax) << " ms" << std::endl;
  std::cout << "Average execution time: " << nsToMs(status.ExecutionTimeMean) << " ms" << std::endl;
  // std::cout << "Last start time delta: " << nsToMs(status.StartTimeDeltaLast) << " ms" << std::endl;
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

  /*** RMP INIT ***/
  MotionController *controller = RMPHelpers::GetController();
  MultiAxis* multiAxis = controller->LoadExistingMultiAxis(RMPHelpers::NUM_AXES);
  RTTaskManager manager(RMPHelpers::CreateRTTaskManager("LaserTracking", 6));

  try
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Adjust to 300–500ms if needed
    
    /* LEAVE THIS COMMENTED OUT FOR NOW (we are doing this from rsiconfig command) */
    //SubmitSingleShotTask(manager, "Initialize", INIT_TIMEOUT);

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

    // Simple shared data structure for C# camera server
    // We'll use global firmware values that C# can read via file I/O or shared memory
    struct CameraFrameData {
      double timestamp;
      int frameNumber;
      int width;
      int height;
      bool ballDetected;
      double centerX;
      double centerY;
      double confidence;
      double targetX;
      double targetY;
    };
    
    // Initialize frame data values in firmware global storage
    FirmwareValue frameTimestamp = {.Double = 0.0};
    manager.GlobalValueSet(frameTimestamp, "frameTimestamp");
    
    FirmwareValue frameNumber = {.Int32 = 0};
    manager.GlobalValueSet(frameNumber, "frameNumber");
    
    FirmwareValue frameWidth = {.Int32 = CameraHelpers::IMAGE_WIDTH};
    manager.GlobalValueSet(frameWidth, "frameWidth");
    
    FirmwareValue frameHeight = {.Int32 = CameraHelpers::IMAGE_HEIGHT};
    manager.GlobalValueSet(frameHeight, "frameHeight");
    
    FirmwareValue ballDetected = {.Bool = false};
    manager.GlobalValueSet(ballDetected, "ballDetected");
    
    FirmwareValue ballCenterX = {.Double = 0.0};
    manager.GlobalValueSet(ballCenterX, "ballCenterX");
    
    FirmwareValue ballCenterY = {.Double = 0.0};
    manager.GlobalValueSet(ballCenterY, "ballCenterY");
    
    FirmwareValue ballConfidence = {.Double = 0.0};
    manager.GlobalValueSet(ballConfidence, "ballConfidence");

    std::cout << "Camera data interface initialized for C# server communication" << std::endl;

    FirmwareValue motionEnabled = {.Bool = true};
    manager.GlobalValueSet(motionEnabled, "motionEnabled");

    /* LEAVE THIS COMMENTED OUT FOR NOW (we are doing this from rsiconfig command) */
    // RTTask ballDetectionTask = SubmitRepeatingTask(manager, "DetectBall", DETECTION_TASK_PERIOD, 0);
    // motionTask = SubmitRepeatingTask(manager, "MoveMotors", MOVE_TASK_PERIOD, 0, TaskPriority::High);
    
    // Reset timing after they have run a few times
    // ballDetectionTask.ExecutionCountAbsoluteWait(5, TASK_WAIT_TIMEOUT);
    // ballDetectionTask.TimingReset();
    // motionTask.ExecutionCountAbsoluteWait(5, TASK_WAIT_TIMEOUT);
    // motionTask.TimingReset();

    /*** MAIN LOOP ***/
    int frameCounter = 0;
    while (!g_shutdown)
    {
      RateLimiter rateLimiter(LOOP_INTERVAL);

      FirmwareValue targetX = manager.GlobalValueGet("targetX");
      FirmwareValue targetY = manager.GlobalValueGet("targetY");
      
      // Update camera frame data for C# server
      auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
      
      FirmwareValue timestamp = {.Double = static_cast<double>(now)};
      manager.GlobalValueSet(timestamp, "frameTimestamp");
      
      FirmwareValue frameNum = {.Int32 = ++frameCounter};
      manager.GlobalValueSet(frameNum, "frameNumber");
      
      // Simulate ball detection (this would come from actual camera processing)
      // For demo purposes, create a simple moving ball simulation
      double simulatedBallX = 320.0 + 100.0 * std::sin(frameCounter * 0.1);
      double simulatedBallY = 240.0 + 50.0 * std::cos(frameCounter * 0.15);
      bool hasBall = (frameCounter % 60) < 45; // Ball visible 75% of the time
      
      FirmwareValue ballDetected = {.Bool = hasBall};
      manager.GlobalValueSet(ballDetected, "ballDetected");
      
      FirmwareValue ballCenterX = {.Double = simulatedBallX};
      manager.GlobalValueSet(ballCenterX, "ballCenterX");
      
      FirmwareValue ballCenterY = {.Double = simulatedBallY};
      manager.GlobalValueSet(ballCenterY, "ballCenterY");
      
      FirmwareValue ballConfidence = {.Double = hasBall ? 0.85 : 0.0};
      manager.GlobalValueSet(ballConfidence, "ballConfidence");
      
      // Note: Camera frame JSON writing is handled by the DetectBall RT task
      // This main loop only updates the simulation data for globals
      
      std::cout << "Frame " << frameCounter << " - Target X: " << targetX.Double 
                << ", Target Y: " << targetY.Double;
      
      if (hasBall) {
        std::cout << " | Ball detected at (" << simulatedBallX << ", " << simulatedBallY << ")";
      }
      std::cout << std::endl;

      /* LEAVE THIS COMMENTED OUT FOR NOW (we are doing this from rsiconfig command) */
      // if (!CheckRTTaskStatus(ballDetectionTask, "Ball Detection Task"))
      // {
      //   g_shutdown = true;
      //   exitCode = 1;
      // }
      // if (!CheckRTTaskStatus(motionTask, "Motion Task"))
      // {
      //   g_shutdown = true;
      //   exitCode = 1;
      // }

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
      if (state == RSIState::RSIStateERROR || state == RSIState::RSIStateSTOPPING_ERROR)
      {
        std::cout << "MultiAxis is in state: " << RMPHelpers::RSIStateToString(state) << std::endl;
        RSISource source = multiAxis->SourceGet();
        std::cerr << "Error Source: " << multiAxis->SourceNameGet(source) << std::endl;
      }
    }

    /* LEAVE THIS COMMENTED OUT FOR NOW (we are doing this from rsiconfig command) */
    // Print task timing information
    //PrintTaskTiming(motionTask, "Motion Task");
    //PrintTaskTiming(ballDetectionTask, "Ball Detection Task");
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

  /*** CLEANUP ***/
  // Remove RT task running flag
  std::remove("/tmp/rsi_rt_task_running");
  std::remove("/tmp/rsi_camera_data.json");
  
  // manager.Shutdown();
  multiAxis->Abort();
  multiAxis->ClearFaults();

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
