#include "rmp_helpers.h"
#include <cstring>
#include <stdexcept>
#include <sstream>

RSI::RapidCode::MotionController* RMPHelpers::CreateController()
{
    using namespace RSI::RapidCode;
    MotionController::CreationParameters createParams;
    std::strncpy(createParams.RmpPath, RMP_PATH, createParams.PathLengthMaximum);
    std::strncpy(createParams.NicPrimary, NIC_PRIMARY, createParams.PathLengthMaximum);
    createParams.CpuAffinity = CPU_AFFINITY;
    MotionController* controller(MotionController::Create(&createParams));
    CheckErrors(controller);
    return controller;
}

RSI::RapidCode::MultiAxis* RMPHelpers::ConfigureAxes(RSI::RapidCode::MotionController* controller)
{
    using namespace RSI::RapidCode;
    controller->AxisCountSet(NUM_AXES + 1);
    MultiAxis* multiAxis(controller->MultiAxisGet(NUM_AXES));
    CheckErrors(multiAxis);
    for (int i = 0; i < NUM_AXES; i++) {
        Axis *axis = controller->AxisGet(i);
        CheckErrors(axis);
        axis->UserUnitsSet(USER_UNITS);
        axis->ErrorLimitActionSet(RSIAction::RSIActionNONE);
        axis->HardwarePosLimitActionSet(RSIAction::RSIActionNONE);
        axis->HardwareNegLimitActionSet(RSIAction::RSIActionNONE);
        axis->PositionSet(0);
        axis->Abort();
        axis->ClearFaults();
        multiAxis->AxisAdd(axis);
    }
    multiAxis->Abort();
    multiAxis->ClearFaults();
    CheckErrors(multiAxis);
    return multiAxis;
}

RSI::RapidCode::RealTimeTasks::RTTaskManager* RMPHelpers::CreateRTTaskManager(std::string userLabel)
{
    using namespace RSI::RapidCode;
    using namespace RSI::RapidCode::RealTimeTasks;

    RTTaskManagerCreationParameters params;
    std::strncpy(params.RTTaskDirectory, RMP_PATH, sizeof(params.RTTaskDirectory));
    std::strncpy(params.UserLabel, userLabel.c_str(), sizeof(params.UserLabel));
    params.CpuCore = CPU_AFFINITY;

    RTTaskManager* manager(RTTaskManager::Create(params));
    CheckErrors(manager);
    return manager;
}

void RMPHelpers::StartTheNetwork(RSI::RapidCode::MotionController *controller)
{
    using namespace RSI::RapidCode;
    if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateOPERATIONAL)
    {
        std::cout << "Starting Network.." << std::endl;
        controller->NetworkStart();
    }
    if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateOPERATIONAL)
    {
        int messagesToRead = controller->NetworkLogMessageCountGet();
        for (int i = 0; i < messagesToRead; i++)
        {
            std::cout << controller->NetworkLogMessageGet(i) << std::endl;
        }
        throw std::runtime_error("Expected OPERATIONAL state but the network did not get there.");
    }
    else
    {
        std::cout << "Network Started" << std::endl << std::endl;
    }
}

void RMPHelpers::ShutdownTheNetwork(RSI::RapidCode::MotionController *controller)
{
    using namespace RSI::RapidCode;
    if (controller->NetworkStateGet() == RSINetworkState::RSINetworkStateUNINITIALIZED ||
        controller->NetworkStateGet() == RSINetworkState::RSINetworkStateSHUTDOWN)
    {
        return;
    }
    std::cout << "Shutting down the network.." << std::endl;
    controller->NetworkShutdown();
    if (controller->NetworkStateGet() != RSINetworkState::RSINetworkStateUNINITIALIZED &&
        controller->NetworkStateGet() != RSINetworkState::RSINetworkStateSHUTDOWN)
    {
        int messagesToRead = controller->NetworkLogMessageCountGet();
        for (int i = 0; i < messagesToRead; i++)
        {
            std::cout << controller->NetworkLogMessageGet(i) << std::endl;
        }
        throw std::runtime_error("Expected SHUTDOWN state but the network did not get there.");
    }
    else
    {
        std::cout << "Network Shutdown" << std::endl << std::endl;
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
                << location.file_name() << '(' << location.line() << ':' << location.column() << ") `"
                << location.function_name() << "`:\n"
                << errorStrings;
        throw std::runtime_error(message.str().c_str());
    }
}
