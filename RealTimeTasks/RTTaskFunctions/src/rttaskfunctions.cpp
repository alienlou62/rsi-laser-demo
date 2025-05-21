/*!
  @example rttaskfunctions.cpp

  @attention See the following Concept pages for a detailed explanation of this
  sample: @ref rttasks.

  @copydoc sample-app-warning
*/

#include "rttaskglobals.h"

#include <cstdlib>
#include <ctime>

#include <iostream>

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

/// @internal @[RTTaskFunctions-Initialize]
// This task initializes the global data and the random number generator
RSI_TASK(Initialize)
{
  data->counter = 0;
  data->average = 0;

  std::srand(std::time(nullptr));
}
/// @internal @[RTTaskFunctions-Initialize]

/// @internal @[RTTaskFunctions-Increment]
// This task increments the counter in the global data
RSI_TASK(Increment)
{
  data->counter += 1;
}
/// @internal @[RTTaskFunctions-Increment]

/// @internal @[RTTaskFunctions-RandomWalk]
// This task randomly moves the axis back and forth
RSI_TASK(RandomWalk)
{
  int random = std::rand() % 2;
  double step = random ? 0.05 : -0.025; // Randomly increment or decrement the average
  data->average += step;
  data->counter += 1;

  RTAxisGet(0)->MoveSCurve(data->average);
}
/// @internal @[RTTaskFunctions-RandomWalk]

/// @internal @[RTTaskFunctions-CalculateTarget]
// This task reads the analog input value from the network node, scales it to 
// a value between 0 and 1, and stores it in the targetPosition variable
RSI_TASK(CalculateTarget)
{
  constexpr int NODE_INDEX = 0;         // The network node with the analog input
  constexpr int ANALOG_INDEX = 0;       // The index of the analog input to use
  constexpr int ANALOG_MAX = 65536;     // Max value of the analog input
  constexpr int ANALOG_ORIGIN = 42800;  // The value to treat as the "origin" of the analog input

  auto networkNode = RTNetworkNodeGet(NODE_INDEX);
  
  // Read the raw analog input value
  int32_t analogInVal = networkNode->AnalogInGet(ANALOG_INDEX);

  // Shift the value by the origin
  int32_t shiftedVal = analogInVal - ANALOG_ORIGIN;

  // Make sure the value is between 0 and ANALOG_MAX using modulo
  int32_t modVal = (shiftedVal + ANALOG_MAX) % ANALOG_MAX;

  // Scale the value to be between 0 and 1 and store it in targetPosition
  data->targetPosition = double(modVal) / ANALOG_MAX;
}
/// @internal @[RTTaskFunctions-CalculateTarget]

/// @internal @[RTTaskFunctions-FollowTarget]
// This task moves the axis to the target position if it is not within the tolerance
// of the target position already.
RSI_TASK(FollowTarget)
{
  constexpr int AXIS_INDEX = 1;      // The index of the axis to move
  constexpr double TOLERANCE = 0.02; // The tolerance for the position difference

  auto axis = RTAxisGet(AXIS_INDEX);
  // Check if the axis is within the tolerance of the target position
  if (abs(axis->ActualPositionGet() - data->targetPosition) > TOLERANCE)
  {
    // Move the axis to the target position
    axis->MoveSCurve(data->targetPosition);
  }
}
/// @internal @[RTTaskFunctions-FollowTarget]
