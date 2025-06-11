#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <numbers>

#include "camera_helpers.h"

// Forward declaration for RSI::RapidCode::MultiAxis
namespace RSI { namespace RapidCode { class MultiAxis; } }

class MotionControl 
{
public:
    static constexpr double NEG_X_LIMIT = -0.19;
    static constexpr double POS_X_LIMIT = 0.19;
    static constexpr double NEG_Y_LIMIT = -0.14;
    static constexpr double POS_Y_LIMIT = 0.14;

    static void MoveMotorsWithLimits(RSI::RapidCode::MultiAxis* multiAxis, double x, double y);
};

#endif // MOTION_CONTROL_H
