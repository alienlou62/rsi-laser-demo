#include "rmp_helpers.h"
#include "rsi.h"
#include "rttask.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

RSI::RapidCode::MotionController *RMPHelpers::GetController()
{
  using namespace RSI::RapidCode;
  MotionController *controller(MotionController::Get());
  CheckErrors(controller);
  return controller;
}

RSI::RapidCode::MultiAxis *RMPHelpers::CreateMultiAxis(RSI::RapidCode::MotionController *controller)
{
  using namespace RSI::RapidCode;
  MultiAxis *multiAxis(controller->MultiAxisGet(NUM_AXES));
  multiAxis->AxisRemoveAll();
  CheckErrors(multiAxis);
  for (int i = 0; i < NUM_AXES; i++)
  {
    Axis *axis = controller->AxisGet(i);
    CheckErrors(axis);
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

RSI::RapidCode::RealTimeTasks::RTTaskManager *RMPHelpers::CreateRTTaskManager(const std::string &userLabel)
{
  using namespace RSI::RapidCode;
  using namespace RSI::RapidCode::RealTimeTasks;

  RTTaskManagerCreationParameters params;
  std::strncpy(params.RTTaskDirectory, RMP_PATH, sizeof(params.RTTaskDirectory));
  std::strncpy(params.UserLabel, userLabel.c_str(), sizeof(params.UserLabel));
  params.CpuCore = CPU_CORE;

  RTTaskManager *manager(RTTaskManager::Create(params));
  CheckErrors(manager);
  return manager;
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
    message << "Error! In " << location.file_name() << '(' << location.line() << ':' << location.column() << ") `" << location.function_name()
            << "`:\n"
            << errorStrings;
    throw std::runtime_error(message.str().c_str());
  }
}
