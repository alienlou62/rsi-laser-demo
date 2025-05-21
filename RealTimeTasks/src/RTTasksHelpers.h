/*!
  @example RTTasksHelpers.h

  @attention See the following Concept pages for a detailed explanation of this
  sample: @ref rttasks.

  @copydoc sample-app-warning
*/

#ifndef RT_TASKS_HELPERS
#define RT_TASKS_HELPERS

#include "rttask.h"

#include <iostream>
#include <memory>

using namespace RSI::RapidCode; // Import our RapidCode namespace
using namespace RSI::RapidCode::RealTimeTasks; // Import the RealTimeTasks namespace

namespace RTTaskHelper
{

/// @internal @[RTTaskManager-Creation-Parameters-cpp]
  // Get the creation parameters for the task manager
#if defined(WIN32) && defined(NDEBUG)

  // If we are on Windows and not Debug, then we need to create an INtime task manager
  RTTaskManagerCreationParameters GetTaskManagerCreationParameters()
  {
    RTTaskManagerCreationParameters parameters;
    std::snprintf(
      parameters.RTTaskDirectory,
      RTTaskManagerCreationParameters::DirectoryLengthMaximum,
      RMP_DEFAULT_PATH    // The RMP install directory
    );
    parameters.Platform = PlatformType::INtime;
    std::snprintf(
      parameters.NodeName,
      RTTaskManagerCreationParameters::NameLengthMaximum,
      "NodeA"
    );
    return parameters;
  }
#else

  // Otherwise, we are on Linux or Debug, so we can create a native task manager
  RTTaskManagerCreationParameters GetTaskManagerCreationParameters()
  {
    RTTaskManagerCreationParameters parameters;
    std::snprintf(
      parameters.RTTaskDirectory,
      RTTaskManagerCreationParameters::DirectoryLengthMaximum,
      RMP_DEFAULT_PATH    // The RMP install directory
    );

    // For Linux real-time set this to a isolated core
    // parameters.CpuCore = -1;
    return parameters;
  }
#endif  // defined(WIN32) && defined(NDEBUG)
/// @internal @[RTTaskManager-Creation-Parameters-cpp]

  /// @internal @[RTTaskManager-InitializeRTTaskObjects-cpp]
  // This calls a task to initialize the global variables and get pointers to RapidCode objects
  // in the RTTaskFunctions library. It needs to be called before any task
  // that uses RapidCode objects.
  void InitializeRTTaskObjects(std::unique_ptr<RTTaskManager>& manager)
  {
    std::cout << "Running initialization task..." << std::endl;
    RTTaskCreationParameters initParams("Initialize");
    initParams.Repeats = RTTaskCreationParameters::RepeatNone;
    std::unique_ptr<RTTask> task(manager->TaskSubmit(initParams));
    constexpr int timeoutMs = 5000;
    task->ExecutionCountAbsoluteWait(1, timeoutMs); // Wait for the task to execute once.
  }
  /// @internal @[RTTaskManager-InitializeRTTaskObjects-cpp]
}

#endif // RT_TASKS_HELPERS
