#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

// Forward declaration for RSI::RapidCode::MultiAxis
namespace RSI { namespace RapidCode { class MultiAxis; } }

namespace MotionControl {
    void MoveMotorsWithLimits(RSI::RapidCode::MultiAxis* multiAxis, double x, double y);
}

#endif // MOTION_CONTROL_H
