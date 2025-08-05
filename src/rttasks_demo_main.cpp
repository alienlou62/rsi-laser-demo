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
#include "image_processing.h"
#include "shared_memory_helpers.h"

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

// Function to convert image data to base64 for JSON embedding
std::string EncodeBase64(const std::vector<uint8_t>& data) {
  static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  int val = 0, valb = -6;
  for (uint8_t c : data) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      result.push_back(chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
  while (result.size() % 4) result.push_back('=');
  return result;
}

// Function to write camera frame data as JSON for C# UI
void WriteCameraFrameJSON(const Frame& frameData) {
  try {
    // Construct cv::Mat from YUYVFrame data in frameData
    cv::Mat yuyvMat(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC2, (void*)frameData.yuyvData);
    cv::Mat rgbFrame;
    cv::cvtColor(yuyvMat, rgbFrame, cv::COLOR_YUV2BGR_YUYV);
    // Encode as JPEG with quality 80
    std::vector<uint8_t> jpegBuffer;
    std::vector<int> jpegParams = {cv::IMWRITE_JPEG_QUALITY, 80};
    cv::imencode(".jpg", rgbFrame, jpegBuffer, jpegParams);
    
    // Convert to base64
    std::string base64Image = EncodeBase64(jpegBuffer);
    
    // Write JSON with frame data
    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": " << std::fixed << std::setprecision(0) << frameData.timestamp << ",\n";
    json << "  \"frameNumber\": " << frameData.frameNumber << ",\n";
    json << "  \"width\": " << CameraHelpers::IMAGE_WIDTH << ",\n";
    json << "  \"height\": " << CameraHelpers::IMAGE_HEIGHT << ",\n";
    json << "  \"format\": \"jpeg\",\n";
    json << "  \"imageData\": \"data:image/jpeg;base64," << base64Image << "\",\n";
    json << "  \"imageSize\": " << jpegBuffer.size() << ",\n";
    json << "  \"ballDetected\": " << (frameData.ballDetected ? "true" : "false") << ",\n";
    json << "  \"centerX\": " << std::fixed << std::setprecision(2) << frameData.centerX << ",\n";
    json << "  \"centerY\": " << std::fixed << std::setprecision(2) << frameData.centerY << ",\n";
    json << "  \"confidence\": " << std::fixed << std::setprecision(3) << frameData.confidence << ",\n";
    json << "  \"targetX\": " << std::fixed << std::setprecision(2) << frameData.targetX << ",\n";
    json << "  \"targetY\": " << std::fixed << std::setprecision(2) << frameData.targetY << ",\n";
    json << "  \"rtTaskRunning\": true\n";
    json << "}";
    
    // Write to file atomically
    std::ofstream dataFile("/tmp/rsi_camera_data.json.tmp");
    if (dataFile.is_open()) {
      dataFile << json.str();
      dataFile.close();
      // Atomic rename to prevent partial reads
      std::rename("/tmp/rsi_camera_data.json.tmp", "/tmp/rsi_camera_data.json");
    }
    
    // Create running flag file
    std::ofstream flagFile("/tmp/rsi_rt_task_running");
    if (flagFile.is_open()) {
      flagFile << "1";
      flagFile.close();
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error writing camera frame JSON: " << e.what() << std::endl;
  }
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
    // Open the shared memory for triple buffering
    SharedMemoryTripleBuffer<Frame> sharedMemory(SHARED_MEMORY_NAME, false);
    TripleBufferManager<Frame> tripleBufferManager(sharedMemory.get(), false);
    Frame frame;

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

      tripleBufferManager.swap_buffers();

      // Check if new data is available in the triple buffer
      if (tripleBufferManager.flags() == 1)
      {
        memcpy(&frame, tripleBufferManager.get(), sizeof(Frame));
        tripleBufferManager.flags() = 0; // Reset flags after reading
        WriteCameraFrameJSON(frame);
      }


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
