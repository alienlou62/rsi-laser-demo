#include "rttaskglobals.h"

#include "rmp_helpers.h"
#include "motion_control.h"

#include <iostream>
#include <string>

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

// This task initializes the global data structure.
RSI_TASK(Initialize)
{
  data->targetX = 0.0;
  data->targetY = 0.0;
}

// This task moves the motors based on the target positions.
RSI_TASK(MoveMotors)
{
  MotionControl::MoveMotorsWithLimits(RTMultiAxisGet(RMPHelpers::NUM_AXES), data->targetX, data->targetY);
}