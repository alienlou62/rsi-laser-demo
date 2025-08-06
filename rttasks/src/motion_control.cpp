#include "motion_control.h"
#include "rsi.h"

#include <algorithm>
#include <array>
#include <stdexcept>

using namespace RSI::RapidCode;

namespace MotionControl
{
  void MoveMotorsWithLimits(MultiAxis *multiAxis, double x, double y)
  {
    try
    {
      double clampedX = std::clamp(x, NEG_X_LIMIT, POS_X_LIMIT);
      double clampedY = std::clamp(y, NEG_Y_LIMIT, POS_Y_LIMIT);
      multiAxis->MoveSCurve(std::array{clampedX, clampedY}.data());
    }
    catch (const RsiError &e)
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
}
