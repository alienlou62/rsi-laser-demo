#include "motion_control.h"
#include "rsi.h"

#include <algorithm>
#include <array>
#include <stdexcept>

void MotionControl::MoveMotorsWithLimits(RSI::RapidCode::MultiAxis *multiAxis, double x, double y)
{
  try
  {
    double clampedX(0.0), clampedY(0.0);
    if (abs(x) > TOLERANCE) clampedX = std::clamp(x, NEG_X_LIMIT, POS_X_LIMIT);
    if (abs(y) > TOLERANCE) clampedY = std::clamp(y, NEG_Y_LIMIT, POS_Y_LIMIT);
    multiAxis->MoveRelative(std::array{clampedX, clampedY}.data());
  }
  catch (const RSI::RapidCode::RsiError &e)
  {
    if (multiAxis)
      multiAxis->Abort();
    throw std::runtime_error(std::string("RMP exception during velocity control: ") + e.what());
  }
  catch (const std::exception &ex)
  {
    if (multiAxis)
      multiAxis->Abort();
    throw std::runtime_error(std::string("Error during velocity control: ") + ex.what());
  }
}
