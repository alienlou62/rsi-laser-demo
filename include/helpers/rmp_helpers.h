#ifndef RMP_HELPERS_H
#define RMP_HELPERS_H

#include <source_location>
#include <string>
#include <chrono>

#include "rsi.h"
#include "rttask.h"

namespace RMPHelpers {
  // MultiAxis configuration parameters
  inline constexpr int NUM_AXES = 2;

  // RTTaskManager configuration parameters
  inline constexpr char RMP_PATH[] = "/rsi";
  inline constexpr int CPU_CORE = 6;

  // Check for errors in the RapidCodeObject and throw an exception if any non-warning errors are found
  void CheckErrors(RSI::RapidCode::RapidCodeObject *rsiObject, const std::source_location &location = std::source_location::current());

  // Get a pointer to the the already created MotionController
  RSI::RapidCode::MotionController* GetController();

  // Create a MultiAxis object and add axes to it
  RSI::RapidCode::MultiAxis* CreateMultiAxis(RSI::RapidCode::MotionController* controller);

  // Convert the RSIState to a string representation
  std::string RSIStateToString(const RSI::RapidCode::RSIState& state);

  // Create a RTTaskManager with a safe shared pointer that will automatically shutdown and delete the manager
  RSI::RapidCode::RealTimeTasks::RTTaskManager CreateRTTaskManager (const std::string& userLabel = "");

  // Submit an RTTask with a safe shared pointer that will automatically stop and delete the task
  RSI::RapidCode::RealTimeTasks::RTTask SubmitRTTask(
    RSI::RapidCode::RealTimeTasks::RTTaskManager& manager,
    RSI::RapidCode::RealTimeTasks::RTTaskCreationParameters& params);
}

#endif // RMP_HELPERS_H
