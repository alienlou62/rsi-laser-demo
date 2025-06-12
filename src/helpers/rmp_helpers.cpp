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

void RMPHelpers::CheckErrors(RapidCodeObject *rsiObject, const std::source_location &location)
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

MotionController *RMPHelpers::GetController()
{
  MotionController *controller(MotionController::Get());
  CheckErrors(controller);
  return controller;
}

MultiAxis *RMPHelpers::CreateMultiAxis(MotionController *controller)
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

std::string RMPHelpers::RSIStateToString(const RSIState& state)
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

// --- Safe handling of RTTask and RTTaskManager deletion ---
template <typename T, typename F>
static void SafeDeleter(T* ptr, F&& shutdown, const char* context) {
  if (ptr) {
    try {
      shutdown(ptr);
    } catch (const RsiError& e) {
      std::cerr << "Exception in " << context << " (RsiError): " << e.what() << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "Exception in " << context << " (std::exception): " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "Unknown exception in " << context << std::endl;
    }
    delete ptr;
  }
}

inline void RTTaskDeleter(RTTask* ptr) {
  SafeDeleter(ptr, [](RTTask* t){ t->Stop(); }, "RTTaskDeleter");
}

inline void RTTaskManagerDeleter(RTTaskManager* ptr) {
  SafeDeleter(ptr, [](RTTaskManager* m){ m->Shutdown(); }, "RTTaskManagerDeleter");
}

std::shared_ptr<RTTaskManager> RMPHelpers::CreateRTTaskManager(const std::string &userLabel)
{
  RTTaskManagerCreationParameters params;
  std::strncpy(params.RTTaskDirectory, RMP_PATH, sizeof(params.RTTaskDirectory));
  std::strncpy(params.UserLabel, userLabel.c_str(), sizeof(params.UserLabel));
  params.CpuCore = CPU_CORE;

  std::shared_ptr<RTTaskManager> manager(RTTaskManager::Create(params), RTTaskManagerDeleter);
  CheckErrors(manager.get());
  return manager;
}

std::shared_ptr<RTTask> RMPHelpers::SubmitRTTask(std::shared_ptr<RTTaskManager> &manager, RTTaskCreationParameters &params)
{
  std::shared_ptr<RTTask> task(manager->TaskSubmit(params), RTTaskDeleter);
  CheckErrors(task.get());
  return task;
}
