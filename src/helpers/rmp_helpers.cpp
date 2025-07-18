#include "rmp_helpers.h"
#include "rsi.h"
#include "rttask.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;


namespace RMPHelpers {

  void CheckErrors(RapidCodeObject *rsiObject, const std::source_location &location)
  {
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
      message << "Error! In " << location.file_name() << '(' << location.line() << ':' << location.column() << ") `" << location.function_name()
              << "`:\n"
              << errorStrings;
      throw std::runtime_error(message.str().c_str());
    }
  }

  MotionController *GetController()
  {
    MotionController *controller(MotionController::Get());
    CheckErrors(controller);
    return controller;
  }

  MultiAxis *CreateMultiAxis(MotionController *controller)
  {
    MultiAxis *multiAxis(controller->MultiAxisGet(NUM_AXES));
    multiAxis->AxisRemoveAll();
    CheckErrors(multiAxis);
    for (int i = 0; i < NUM_AXES; i++)
    {
      Axis *axis = controller->AxisGet(i);
      CheckErrors(axis);
      axis->Abort();
      axis->ClearFaults();
      multiAxis->AxisAdd(axis);
    }
    multiAxis->MotionAttributeMaskOffSet(RSIMotionAttrMask::RSIMotionAttrMaskAPPEND);
    multiAxis->Abort();
    multiAxis->ClearFaults();
    return multiAxis;
  }

  std::string RSIStateToString(const RSIState& state)
  {
    switch (state)
    {
      case RSIState::RSIStateIDLE:
        return "IDLE";
      case RSIState::RSIStateMOVING:
        return "MOVING";
      case RSIState::RSIStateSTOPPING:
        return "STOPPING";
      case RSIState::RSIStateSTOPPED:
        return "STOPPED";
      case RSIState::RSIStateSTOPPING_ERROR:
        return "STOPPING_ERROR";
      case RSIState::RSIStateERROR:
        return "ERROR";
    }
    return "UNKNOWN";
  }

  RTTaskManager CreateRTTaskManager(const std::string &userLabel)
  {
    RTTaskManagerCreationParameters params;
    std::strncpy(params.RTTaskDirectory, RMP_PATH, sizeof(params.RTTaskDirectory));
    std::strncpy(params.UserLabel, userLabel.c_str(), sizeof(params.UserLabel));
    params.CpuCore = CPU_CORE;

    RTTaskManager manager(RTTaskManager::Create(params));
    CheckErrors(&manager);
    return manager;
  }

  RTTask SubmitRTTask(RTTaskManager &manager, RTTaskCreationParameters &params)
  {
    RTTask task(manager.TaskSubmit(params));
    CheckErrors(&task);
    return task;
  }
} // namespace RMPHelpers
