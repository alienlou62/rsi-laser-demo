/*!
  @example RandomWalk.cpp

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
  const std::string SAMPLE_APP_NAME = "Real-Time Tasks: Random Walk";

  // Print a start message to indicate that the sample app has started
  SampleAppsHelper::PrintHeader(SAMPLE_APP_NAME);

  /* CONSTANTS */
  // *NOTICE* The following constants must be configured before attempting to run with hardware.
  const int NUM_AXES = 1;   // The number of axes to configure, default is 0 for this template
  const int TIMEOUT_MS = 5000; // Timeout for waiting for the task to execute once

  /* RAPIDCODE INITIALIZATION */

  // Create the controller
  MotionController::CreationParameters params = SampleAppsHelper::GetCreationParameters();
  MotionController *controller = MotionController::Create(&params);

  // Create the task manager
  std::unique_ptr<RTTaskManager> manager = nullptr;

  int exitCode = -1; // Set the exit code to an error value.
  try
  {
    // Prepare the controller as defined in SampleAppsHelper.h depending on the configuration
    SampleAppsHelper::CheckErrors(controller);
    SampleAppsHelper::SetupController(controller, NUM_AXES);

    /* SET CONTROLLER OBJECT COUNTS HERE*/
    // Normally you would set the number of axes here, but for samples that is handled in the SampleAppsHelper::SetupController function
    // controller->AxisCountSet(NUM_AXES); 

    /// @internal @[RandomWalk-cpp]
    // Configure the axis
    Axis *axis = controller->AxisGet(0);
    SampleAppsHelper::CheckErrors(axis);
    axis->PositionSet(0);
    axis->AmpEnableSet(true);

    // Create the task manager
    RTTaskManagerCreationParameters parameters = RTTaskHelper::GetTaskManagerCreationParameters();
    std::cout << "Creating task manager..." << std::endl;
    manager.reset(RTTaskManager::Create(parameters));

    // Call the initialization task to initialize the global variables and get pointers to RapidCode objects
    // in the RTTaskFunctions library. This needs to be called before any task that uses RapidCode objects.
    RTTaskHelper::InitializeRTTaskObjects(manager);

    // Tell the task manager the name of the function to run as a task, the name
    // of the library that contains the function, and the directory where the 
    // library is located. The RandomWalk function is defined in the RTTaskFunctions library.
    // It moves the axis back and forth based on std::rand().
    std::cout << "Submitting task..." << std::endl;
    RTTaskCreationParameters params("RandomWalk");
    params.Repeats = RTTaskCreationParameters::RepeatForever;
    params.Period = 5;
    std::unique_ptr<RTTask> task(manager->TaskSubmit(params));

    // Wait for the task to run for a bit
    task->ExecutionCountAbsoluteWait(50, 500);

    // Get the counter global tag to see if the task ran correctly
    std::cout << "Getting counter global tag..." << std::endl;
    FirmwareValue counter = manager->GlobalValueGet("counter");
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

    // The average global tag is the position the axis should be at if the RandomWalk task is working correctly
    FirmwareValue average = manager->GlobalValueGet("average");
    std::cout << "Average: " << average.Double << std::endl;

    // Print the final axis position
    std::cout << "Axis position: " << axis->CommandPositionGet() << std::endl;
    
    task->Stop();
    axis->MotionDoneWait(TIMEOUT_MS); // The axis might still be moving from the task, so wait for it to finish
    axis->AmpEnableSet(false);
    /// @internal @[RandomWalk-cpp]
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

  // Print a message to indicate the sample app has finished and if it was successful or not
  SampleAppsHelper::PrintFooter(SAMPLE_APP_NAME, exitCode);

  return exitCode;
}
