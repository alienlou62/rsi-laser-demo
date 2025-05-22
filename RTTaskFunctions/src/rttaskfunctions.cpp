#include "rttaskglobals.h"

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

// This task initializes the global data structure.
RSI_TASK(Initialize)
{
  data->targetX = 0.0;
  data->targetY = 0.0;
}