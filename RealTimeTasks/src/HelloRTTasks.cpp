/*!
  @example HelloRTTasks.cpp

  @attention See the following Concept pages for a detailed explanation of this
  sample: @ref rttasks.

  @copydoc sample-app-warning
*/

#include "SampleAppsHelper.h" // Import our helper functions.
#include "rsi.h"              // Import our RapidCode Library.

#include "RTTasksHelpers.h"   // Import our helper functions for RTTasks
#include "rttask.h"           // Import the RTTask library

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace RSI::RapidCode; // Import the RapidCode namespace
using namespace RSI::RapidCode::RealTimeTasks; // Import the RealTimeTasks namespace

int main()
{
  const std::string SAMPLE_APP_NAME = "Real-Time Tasks: Hello RTTasks";

  // Print a start message to indicate that the sample app has started
  SampleAppsHelper::PrintHeader(SAMPLE_APP_NAME);

  /// @[HelloRTTasks-cpp]
  // Create the controller
  MotionController::CreationParameters params = SampleAppsHelper::GetCreationParameters();
  MotionController *controller = MotionController::Create(&params);

  RTTaskManagerCreationParameters parameters = RTTaskHelper::GetTaskManagerCreationParameters();
  std::cout << "Creating task manager..." << std::endl;
  std::unique_ptr<RTTaskManager> manager(RTTaskManager::Create(parameters));

  int exitCode = -1; // Set the exit code to an error value.
  try
  {
    // Prepare the controller as defined in SampleAppsHelper.h depending on the configuration
    SampleAppsHelper::CheckErrors(controller);
    SampleAppsHelper::SetupController(controller);

    // Tell the task manager the name of the function to run as a task, the name
    // of the library that contains the function, and the directory where the 
    // library is located. The Increment function is defined in the RTTaskFunctions library.
    // It increments a global counter variable.
    std::cout << "Submitting task..." << std::endl;
    RTTaskCreationParameters params("Increment");
    params.Repeats = RTTaskCreationParameters::RepeatForever;
    std::unique_ptr<RTTask> task(manager->TaskSubmit(params));

    // Wait for the task to run for a bit
    task->ExecutionCountAbsoluteWait(50, 500);

    // Get the counter global tag to see if the task ran correctly
    std::cout << "Getting counter global tag..." << std::endl;
    RSI::RapidCode::FirmwareValue counter = manager->GlobalValueGet("counter");

    if (counter.Int64 <= 0)
    {
      // The task did not run correctly
      exitCode = -1;
      std::cout << "Counter is not greater than 0. The task did not run correctly." << std::endl;
    }
    else
    {
      // The task ran correctly
      exitCode = 0;
      std::cout << "Counter: " << counter.Int64 << std::endl;
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }
  // Shutdown the task manager firmware process
  if (manager != nullptr)
  {
    manager->Shutdown();
  }

  // Clean up the controller and any other objects as needed
  SampleAppsHelper::Cleanup(controller);

  // Delete the controller as the program exits to ensure memory is deallocated in the correct order
  controller->Delete();
  /// @[HelloRTTasks-cpp]

  // Print a message to indicate the sample app has finished and if it was successful or not
  SampleAppsHelper::PrintFooter(SAMPLE_APP_NAME, exitCode);

  return exitCode;
}
