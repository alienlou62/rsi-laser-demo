#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <atomic>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>
#include <string>
#include <sys/wait.h>

// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <pylon/PylonIncludes.h>

// --- RMP/RSI Headers ---
#include "rsi.h"
#include "rttask.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "misc_helpers.h"
#include "motion_control.h"
#include "rmp_helpers.h"
#include "timing_helpers.h"
#include "image_helpers.h"
#include "shared_memory_helpers.h"

#ifndef SANDBOX_DIR
#define SANDBOX_DIR ""
#endif // SANDBOX_DIR

using namespace RSI::RapidCode;
using namespace RSI::RapidCode::RealTimeTasks;
using namespace cv;
using namespace Pylon;
using namespace GenApi;

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, toggling shutdown flag..." << std::endl;
  g_shutdown = true;
}

struct Frame { int value; };

int SharedMemoryTest()
{
  const std::string SHM_NAME = "/triple_buffer_test";
  SharedMemoryTripleBuffer<Frame> shm(SHM_NAME, true);
  pid_t pid = fork();
  if (pid != 0)
  { // reader
    SharedMemoryTripleBuffer<Frame> shm_reader(SHM_NAME);
    TripleBufferManager<Frame> reader(shm_reader.get());
    int index = 0;
    while (index < 100 && !g_shutdown) {
      reader.swap_buffer();
      int value = reader->value;

      std::cout << "Read " << value << std::endl;
      usleep(16667);
      ++index;
    }
    return 0;
  } 
  else
  { // writer
    TripleBufferManager<Frame> writer(shm.get(), true);
    int index = 0;
    while (index < 500 && !g_shutdown)
    {
      writer->value = index;
      writer.swap_buffer();
      // std::cout << "Wrote " << index << std::endl;

      usleep(5000);
      ++index;
    }
    
    wait(nullptr);
  }
  return 0;
}

// --- Main Function ---
int main()
{
  const std::string EXECUTABLE_NAME = "Sandbox";
  MiscHelpers::PrintHeader(EXECUTABLE_NAME);
  std::signal(SIGINT, sigint_handler);
  cv::setNumThreads(0); // Turn off OpenCV threading

  int exitCode = 0;

  exitCode = SharedMemoryTest();

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
