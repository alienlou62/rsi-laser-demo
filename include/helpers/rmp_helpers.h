#ifndef RMP_HELPERS_H
#define RMP_HELPERS_H

#include <source_location>
#include <memory>
#include <string>

#include "rsi.h"
#include "rttask.h"

class RMPHelpers {
public:
  // MultiAxis configuration parameters
  static constexpr int NUM_AXES = 2;

  // RTTaskManager configuration parameters
  static constexpr char RMP_PATH[] = "/rsi";
  static constexpr int CPU_CORE = 2;

  // Check for errors in the RapidCodeObject and throw an exception if any non-warning errors are found
  static void CheckErrors(RSI::RapidCode::RapidCodeObject *rsiObject, const std::source_location &location = std::source_location::current());

  // Get a pointer to the the already created MotionController
  static RSI::RapidCode::MotionController* GetController();

  // Create a MultiAxis object and add axes to it
  static RSI::RapidCode::MultiAxis* CreateMultiAxis(RSI::RapidCode::MotionController* controller);

  // Convert the RSIState to a string representation
  static std::string RSIStateToString(const RSI::RapidCode::RSIState& state);

  // Create a RTTaskManager with a safe shared pointer that will automatically shutdown and delete the manager
  static std::shared_ptr<RSI::RapidCode::RealTimeTasks::RTTaskManager> CreateRTTaskManager (const std::string& userLabel = "");

  // Submit an RTTask with a safe shared pointer that will automatically stop and delete the task
  static std::shared_ptr<RSI::RapidCode::RealTimeTasks::RTTask> SubmitRTTask(
    std::shared_ptr<RSI::RapidCode::RealTimeTasks::RTTaskManager>& manager, 
    RSI::RapidCode::RealTimeTasks::RTTaskCreationParameters& params);
};

#endif // RMP_HELPERS_H
