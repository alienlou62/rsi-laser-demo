#ifndef RMP_HELPERS_H
#define RMP_HELPERS_H

#include <memory>
#include <iostream>
#include <string>
#include <sstream>
#include <source_location>

#include "rsi.h"

class RMPHelpers {
public:
    static constexpr char RMP_PATH[] = "/rsi/";
    static constexpr char NIC_PRIMARY[] = "enp3s0";
    static constexpr int CPU_AFFINITY = 3;

    static constexpr int NUM_AXES = 2;
    static constexpr int USER_UNITS = 67108864;

    // Create and return a shared_ptr to a MotionController
    static std::shared_ptr<RSI::RapidCode::MotionController> CreateController();

    // Configure axes and return a shared_ptr to MultiAxis
    static std::shared_ptr<RSI::RapidCode::MultiAxis> ConfigureAxes(std::shared_ptr<RSI::RapidCode::MotionController> controller);

    // Start the network and check for errors
    static void StartTheNetwork(RSI::RapidCode::MotionController *controller);
    inline static void StartTheNetwork(std::shared_ptr<RSI::RapidCode::MotionController> controller) 
    { 
        StartTheNetwork(controller.get());
    }

    // Check for errors in the RapidCodeObject and throw an exception if any non-warning errors are found
    static void CheckErrors(RSI::RapidCode::RapidCodeObject *rsiObject, const std::source_location &location = std::source_location::current());
    inline static void CheckErrors(std::shared_ptr<RSI::RapidCode::RapidCodeObject> rsiObject, const std::source_location &location = std::source_location::current()) 
    {
        CheckErrors(rsiObject.get(), location);
    }
};

std::shared_ptr<RSI::RapidCode::MotionController> RMPHelpers::CreateController()
{
    using namespace RSI::RapidCode;
    MotionController::CreationParameters createParams;
    std::strncpy(createParams.RmpPath, RMP_PATH, createParams.PathLengthMaximum);
    std::strncpy(createParams.NicPrimary, NIC_PRIMARY, createParams.PathLengthMaximum);
    createParams.CpuAffinity = CPU_AFFINITY;
    std::shared_ptr<MotionController> controller(MotionController::Create(&createParams),
        [](MotionController *controller) {
            if (controller) controller->Delete();
        });
    CheckErrors(controller);
    return controller;
}

std::shared_ptr<RSI::RapidCode::MultiAxis> RMPHelpers::ConfigureAxes(std::shared_ptr<RSI::RapidCode::MotionController> controller)
{
    using namespace RSI::RapidCode;
    controller->AxisCountSet(NUM_AXES + 1);
    std::shared_ptr<MultiAxis> multiAxis(controller->MultiAxisGet(NUM_AXES),
        [](MultiAxis *multiAxis) {
            if (multiAxis) {
                multiAxis->Abort();
                multiAxis->ClearFaults();
            }
        });
    CheckErrors(multiAxis);
    for (int i = 0; i < NUM_AXES; i++) {
        Axis *axis = controller->AxisGet(i);
        CheckErrors(axis);
        axis->UserUnitsSet(USER_UNITS);
        axis->ErrorLimitActionSet(RSIAction::RSIActionNONE);
        axis->HardwarePosLimitActionSet(RSIAction::RSIActionNONE);
        axis->HardwareNegLimitActionSet(RSIAction::RSIActionNONE);
        axis->PositionSet(0);
        multiAxis->AxisAdd(axis);
    }
    multiAxis->Abort();
    multiAxis->ClearFaults();
    return multiAxis;
}

void RMPHelpers::StartTheNetwork(RSI::RapidCode::MotionController *controller)
{
    using namespace RSI::RapidCode;
    // Initialize the Network
    if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateOPERATIONAL) // Check if network is started already.
    {
        std::cout << "Starting Network.." << std::endl;
        controller->NetworkStart(); // If not. Initialize The Network. (This can also be done from RapidSetup Tool)
    }

    if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateOPERATIONAL) // Check if network is started again.
    {
        int messagesToRead = controller->NetworkLogMessageCountGet(); // Some kind of error starting the network, read the network log messages

        for (int i = 0; i < messagesToRead; i++)
        {
        std::cout << controller->NetworkLogMessageGet(i) << std::endl; // Print all the messages to help figure out the problem
        }
        throw std::runtime_error("Expected OPERATIONAL state but the network did not get there.");
    }
    else // Else, of network is operational.
    {
        std::cout << "Network Started" << std::endl << std::endl;
    }
}

void RMPHelpers::CheckErrors(RSI::RapidCode::RapidCodeObject *rsiObject, const std::source_location &location)
{
    using namespace RSI::RapidCode;
    bool hasErrors = false;
    std::string errorStrings("");
    while (rsiObject->ErrorLogCountGet() > 0)
    {
        const RsiError *err = rsiObject->ErrorLogGet();

        errorStrings += err->what();
        errorStrings += "\n";

        if (!err->isWarning)
        {
        hasErrors = true;
        }
    }

    if (hasErrors)
    {
        std::ostringstream message;
        message << "Error! In "
                << location.file_name() << '('
                << location.line() << ':'
                << location.column() << ") `"
                << location.function_name() << "`:\n"
                << errorStrings;

        throw std::runtime_error(message.str().c_str());
    }
}

#endif // RMP_HELPERS_H
