#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>

// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <pylon/PylonIncludes.h>

// --- RMP/RSI Headers ---
#include "rsi.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "misc_helpers.h"
#include "motion_control.h"
#include "rmp_helpers.h"
#include "timing_helpers.h"
#include "image_helpers.h"

#ifndef SANDBOX_DIR
#define SANDBOX_DIR ""
#endif // SANDBOX_DIR

using namespace RSI::RapidCode;
using namespace cv;
using namespace Pylon;
using namespace GenApi;

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, toggling shutdown flag..." << std::endl;
  g_shutdown = true;
}

void OldCalculateTargetPosition(const Vec3f& ball, double &offsetX, double &offsetY)
{
  constexpr unsigned int CENTER_X = CameraHelpers::IMAGE_WIDTH / 4;
  constexpr unsigned int CENTER_Y = CameraHelpers::IMAGE_HEIGHT / 4;
  constexpr unsigned int MIN_PIXEL_OFFSET = ImageProcessing::PIXEL_THRESHOLD / 2;
  constexpr double MOTOR_UNITS_PER_PIXEL = -CameraHelpers::RADIANS_PER_PIXEL / std::numbers::pi;

  // Calculate the offset from the center of the image
  int pixelOffsetX = static_cast<int>(ball[0]) - CENTER_X;
  int pixelOffsetY = static_cast<int>(ball[1]) - CENTER_Y;

  if (std::abs(pixelOffsetX) > MIN_PIXEL_OFFSET)
    offsetX = MOTOR_UNITS_PER_PIXEL * pixelOffsetX;
  else
    offsetX = 0.0;

  if (std::abs(pixelOffsetY) > MIN_PIXEL_OFFSET)
    offsetY = MOTOR_UNITS_PER_PIXEL * pixelOffsetY;
  else
    offsetY = 0.0;
}

void NewCalculateTargetPosition(const Vec3f& ball, double &offsetX, double &offsetY)
{
  constexpr unsigned int CENTER_X = CameraHelpers::IMAGE_WIDTH / 2;
  constexpr unsigned int CENTER_Y = CameraHelpers::IMAGE_HEIGHT / 2;
  constexpr unsigned int MIN_PIXEL_OFFSET = ImageProcessing::PIXEL_THRESHOLD;
  constexpr double MOTOR_UNITS_PER_PIXEL = -CameraHelpers::RADIANS_PER_PIXEL / (2.0 * std::numbers::pi);

  // Calculate the offset from the center of the image
  int pixelOffsetX = static_cast<int>(ball[0]) - CENTER_X;
  int pixelOffsetY = static_cast<int>(ball[1]) - CENTER_Y;

  offsetX = (std::abs(pixelOffsetX) > MIN_PIXEL_OFFSET) ? MOTOR_UNITS_PER_PIXEL * pixelOffsetX : 0.0;
  offsetY = (std::abs(pixelOffsetY) > MIN_PIXEL_OFFSET) ? MOTOR_UNITS_PER_PIXEL * pixelOffsetY : 0.0;
}

// --- Main Function ---
int main()
{
  const std::string EXECUTABLE_NAME = "Sandbox";
  MiscHelpers::PrintHeader(EXECUTABLE_NAME);
  std::signal(SIGINT, sigint_handler);
  cv::setNumThreads(0); // Turn off OpenCV threading

  int exitCode = 0;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> rand_x_dist(0, 319);
  std::uniform_int_distribution<> rand_y_dist(0, 239);
  int radius = 20;
  auto randBall = [&]() -> cv::Vec3f {
    return cv::Vec3f(static_cast<float>(rand_x_dist(gen)), static_cast<float>(rand_y_dist(gen)), static_cast<float>(radius));
  };

  for (int i = 0; i < 10; ++i)
  {
    cv::Vec3f ball = randBall();
    double oldOffsetX = 0.0, oldOffsetY = 0.0;
    OldCalculateTargetPosition(ball, oldOffsetX, oldOffsetY);
    std::cout << "Old Version - Ball: " << ball << ", Offset: (" << oldOffsetX << ", " << oldOffsetY << ")" << std::endl;

    ball *= 2.0f;
    double newOffsetX = 0.0, newOffsetY = 0.0;
    NewCalculateTargetPosition(ball, newOffsetX, newOffsetY);
    std::cout << "New Version - Ball: " << ball << ", Offset: (" << newOffsetX << ", " << newOffsetY << ")" << std::endl;

    // Print the difference in offsets
    std::cout << "Difference - Offset: (" << (newOffsetX - oldOffsetX) << ", " << (newOffsetY - oldOffsetY) << ")" << std::endl;
  }

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
