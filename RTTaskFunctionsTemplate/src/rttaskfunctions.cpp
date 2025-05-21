#include "rttaskglobals.h"

#include <cstdlib>
#include <ctime>

#include <iostream>

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;

// This task initializes the global data, the random number generator, and gets pointers RapidCode objects
RSI_TASK(Initialize)
{
  data->counter = 0;
  data->average = 0;
}

// This task increments the counter in the global data
RSI_TASK(Increment)
{
  data->counter += 1;
}
