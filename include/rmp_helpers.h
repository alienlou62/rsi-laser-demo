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
    // MotionController creation parameters
    static constexpr char RMP_PATH[] = "/rsi/";
    static constexpr char NIC_PRIMARY[] = "enp3s0";
    static constexpr int CPU_AFFINITY = 3;

    // MultiAxis configuration parameters
    static constexpr int NUM_AXES = 2;
    static constexpr int USER_UNITS = 67108864;

    // Create and return a shared_ptr to a MotionController
    static RSI::RapidCode::MotionController* CreateController();

    // Configure axes and return a shared_ptr to MultiAxis
    static RSI::RapidCode::MultiAxis* ConfigureAxes(RSI::RapidCode::MotionController* controller);

    static RSI::RapidCode::RealTimeTasks::RTTaskManager* CreateRTTaskManager(std::string userLabel);

    // Start the network and check for errors
    static void StartTheNetwork(RSI::RapidCode::MotionController *controller);

    // Shutdown the network and check for errors
    static void ShutdownTheNetwork(RSI::RapidCode::MotionController *controller);

    // Check for errors in the RapidCodeObject and throw an exception if any non-warning errors are found
    static void CheckErrors(RSI::RapidCode::RapidCodeObject *rsiObject, const std::source_location &location = std::source_location::current());
};

#endif // RMP_HELPERS_H
