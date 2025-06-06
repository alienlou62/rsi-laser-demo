#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <numbers>

#include "camera_helpers.h"

// Forward declaration for RSI::RapidCode::MultiAxis
namespace RSI { namespace RapidCode { class MultiAxis; } }

class MotionControl 
{
public:
    static constexpr double NEG_X_LIMIT = -0.245;
    static constexpr double POS_X_LIMIT = 0.120;
    static constexpr double NEG_Y_LIMIT = -0.125;
    static constexpr double POS_Y_LIMIT = 0.135;

    static void MoveMotorsWithLimits(RSI::RapidCode::MultiAxis* multiAxis, double x, double y);
};

#endif // MOTION_CONTROL_H
