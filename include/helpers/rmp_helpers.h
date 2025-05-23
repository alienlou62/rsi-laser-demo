#ifndef RMP_HELPERS_H
#define RMP_HELPERS_H

#include <iostream>
#include <string>
#include <sstream>
#include <source_location>

#include "rsi.h"
#include "rttask.h"

class RMPHelpers {
public:
    // MultiAxis configuration parameters
    static constexpr int NUM_AXES = 2;

    // RTTaskManager configuration parameters
    static constexpr char RMP_PATH[] = "/rsi";
    static constexpr int CPU_CORE = 3;

    // Get a pointer to the the already created MotionController
    static RSI::RapidCode::MotionController* GetController();

    // Create a MultiAxis object and add axes to it
    static RSI::RapidCode::MultiAxis* CreateMultiAxis(RSI::RapidCode::MotionController* controller);

    static RSI::RapidCode::RealTimeTasks::RTTaskManager* CreateRTTaskManager(std::string userLabel);

    // Check for errors in the RapidCodeObject and throw an exception if any non-warning errors are found
    static void CheckErrors(RSI::RapidCode::RapidCodeObject *rsiObject, const std::source_location &location = std::source_location::current());
};

#endif // RMP_HELPERS_H
