#include "motion_control.h"
#include "rsi.h"

#include <array>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace MotionControl {
void MoveMotorsWithLimits(RSI::RapidCode::MultiAxis* multiAxis, double x, double y)
{
    static const double POSITION_DEAD_ZONE = 0.0005;
    static const double PIXEL_DEAD_ZONE = 40.0;
    static const double MAX_OFFSET = 320.0; // max pixel offset
    static const double MIN_VELOCITY = 0.0001;
    static const double MAX_VELOCITY = 0.2; // max velocity
    static const double jerkPct = 30.0; // Smoother than 50%

    // --- Dead zone logic ---
    if (std::abs(x) < PIXEL_DEAD_ZONE && std::abs(y) < PIXEL_DEAD_ZONE)
    {
        multiAxis->MoveVelocity(std::array{0.0, 0.0}.data());
        return;
    }

    try
    {
        // --- Normalize pixel offsets to [-1, 1] ---
        double normX = std::clamp(x / MAX_OFFSET, -1.0, 1.0);
        double normY = std::clamp(y / MAX_OFFSET, -1.0, 1.0);

        // --- Exponential scaling to reduce velocity when near center ---
        double velX = -std::copysign(1.0, normX) * std::pow(std::abs(normX), 1.5) * MAX_VELOCITY;
        double velY = -std::copysign(1.0, normY) * std::pow(std::abs(normY), 1.5) * MAX_VELOCITY;

        // --- Avoid tiny jitter motions ---
        if (std::abs(velX) < MIN_VELOCITY)
            velX = 0.0;
        if (std::abs(velY) < MIN_VELOCITY)
            velY = 0.0;

        // --- Velocity mode only (no MoveSCurve to position) ---
        multiAxis->MoveVelocity(std::array{velX, velY}.data());
    }
    catch (const RSI::RapidCode::RsiError &e)
    {
        std::cerr << "RMP exception during velocity control: " << e.what() << std::endl;
        if (multiAxis)
            multiAxis->Abort();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error during velocity control: " << ex.what() << std::endl;
        if (multiAxis)
            multiAxis->Abort();
    }
}
} // namespace MotionControl
