#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <numbers>

#include "camera_helpers.h"

// Forward declaration for RSI::RapidCode::MultiAxis
namespace RSI { namespace RapidCode { class MultiAxis; } }

namespace MotionControl 
{
  inline constexpr double NEG_X_LIMIT = -0.19;
  inline constexpr double POS_X_LIMIT = 0.19;
  inline constexpr double NEG_Y_LIMIT = -0.14;
  inline constexpr double POS_Y_LIMIT = 0.14;

  void MoveMotorsWithLimits(RSI::RapidCode::MultiAxis* multiAxis, double x, double y);
};

#endif // MOTION_CONTROL_H
