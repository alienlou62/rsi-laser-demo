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

    // Tolerance to avoid unnecessary small movements
    static constexpr double TOLERANCE = 10.0 * CameraHelpers::RADIANS_PER_PIXEL / (2.0 * std::numbers::pi);

    static void MoveMotorsWithLimits(RSI::RapidCode::MultiAxis* multiAxis, double x, double y);
};

#endif // MOTION_CONTROL_H
