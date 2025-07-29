#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>

// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

// --- RMP/RSI Headers ---
#include "rsi.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "misc_helpers.h"
#include "motion_control.h"
#include "rmp_helpers.h"
#include "timing_helpers.h"

using namespace RSI::RapidCode;

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, setting shutdown flag..." << std::endl;
  g_shutdown = true;
}

// --- Main Function ---
int main()
{
  const std::chrono::milliseconds loopInterval(5); // 5ms loop interval
  const std::string EXECUTABLE_NAME = "Pylon_RSI_Tracking_BayerOnly";
  MiscHelpers::PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  std::signal(SIGINT, sigint_handler);

  // --- Pylon & Camera Initialization ---
  Pylon::PylonAutoInitTerm pylonAutoInitTerm = Pylon::PylonAutoInitTerm();
  Pylon::CInstantCamera camera;
  Pylon::CGrabResultPtr ptrGrabResult;

  CameraHelpers::ConfigureCamera(camera);
  CameraHelpers::PrimeCamera(camera, ptrGrabResult);

  // --- RMP Initialization ---
  MotionController* controller = RMPHelpers::GetController();
  MultiAxis* multiAxis = controller->LoadExistingMultiAxis(RMPHelpers::NUM_AXES);
  Axis* axisX = controller->AxisGet(0);
  Axis* axisY = controller->AxisGet(1);

  multiAxis->Abort();
  multiAxis->ClearFaults();
  multiAxis->MotionAttributeMaskOffSet(RSIMotionAttrMask::RSIMotionAttrMaskAPPEND);
  multiAxis->MotionAttributeMaskOffSet(RSIMotionAttrMask::RSIMotionAttrMaskNO_WAIT);
  multiAxis->AmpEnableSet(true);

  double targetX = 0.0, targetY = 0.0;

  int grabFailures = 0, processFailures = 0;
  TimingStats loopTiming, retrieveTiming, processingTiming, motionTiming;
  try
  {
    // --- Main Loop ---
    while (!g_shutdown && camera.IsGrabbing())
    {
      RateLimiter rateLimiter(loopInterval);
      auto loopStopwatch = Stopwatch(loopTiming);

      // --- Frame Retrieval ---
      auto retrieveStopwatch = Stopwatch(retrieveTiming);
      bool frameGrabbed = false;
      try
      {
        frameGrabbed = CameraHelpers::TryGrabFrame(camera, ptrGrabResult, 0);
      }
      catch (...)
      {
        // If an exception occurs during frame grab increment failure count
        ++grabFailures;
        continue;
      }

      // If the frame grab failed due to a timeout, exit early but do not increment failure count
      if (!frameGrabbed)
      {
        continue;
      }
      retrieveStopwatch.Stop();

      // Get the axis positions at the time the frame was grabbed
      double initialX = axisX->ActualPositionGet();
      double initialY = axisY->ActualPositionGet();

      // --- Image Processing ---
      auto processingStopwatch = Stopwatch(processingTiming);
      cv::Mat yuyvFrame = ImageProcessing::WrapYUYVBuffer(
            static_cast<uint8_t *>(ptrGrabResult->GetBuffer()),
            CameraHelpers::IMAGE_WIDTH, CameraHelpers::IMAGE_HEIGHT);
      
      cv::Vec3f ball(0.0, 0.0, 0.0);
      bool ballDetected = ImageProcessing::TryDetectBall(yuyvFrame, ball);
      if (!ballDetected)
      {
        ++processFailures;
        continue;
      }
      processingStopwatch.Stop();

      // Calculate the target positions based on the offsets and the position at the time of frame grab
      double offsetX = 0.0, offsetY = 0.0;
      ImageProcessing::CalculateTargetPosition(ball, offsetX, offsetY);
      targetX = initialX + offsetX;
      targetY = initialY + offsetY;

      // --- Motion Control ---
      auto motionStopwatch = Stopwatch(motionTiming);
      try
      {
        std::cout << "Initial Position: (" << initialX << ", " << initialY << "), "
                  << "Target Position: (" << targetX << ", " << targetY << ")" << std::endl;
        MotionControl::MoveMotorsWithLimits(multiAxis, targetX, targetY);
      }
      catch (const RsiError &e)
      {
        std::cerr << "RMP exception during motion control: " << e.what() << std::endl;
        g_shutdown = true;
      }
      motionStopwatch.Stop();
      // g_shutdown = true;
    }
  }
  catch (const cv::Exception &e)
  {
    std::cerr << "OpenCV exception: " << e.what() << std::endl;
    exitCode = 1;
  }
  catch (const Pylon::GenericException &e)
  {
    std::cerr << "Pylon exception: " << e.what() << std::endl;
    exitCode = 1;
  }
  catch (const RsiError &e)
  {
    std::cerr << "RMP exception: " << e.what() << std::endl;
    exitCode = 1;
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << std::endl;
    exitCode = 1;
  }
  catch (...)
  {
    std::cerr << "Unknown exception occurred." << std::endl;
    exitCode = 1;
  }

  // Cleanup
  multiAxis->Abort();
  multiAxis->ClearFaults();

  // Print loop statistics
  printStats("Loop", loopTiming);
  printStats("Retrieve", retrieveTiming);
  printStats("Processing", processingTiming);
  printStats("Motion", motionTiming);

  std::cout << "--------------------------------------------" << std::endl;
  std::cout << "Grab Failures:     " << grabFailures << std::endl;
  std::cout << "Process Failures:  " << processFailures << std::endl;

  cv::destroyAllWindows();

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);

  return exitCode;
}