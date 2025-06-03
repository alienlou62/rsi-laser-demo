#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>

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

#ifndef SOURCE_DIR
#define SOURCE_DIR ""
#endif // SOURCE_DIR

using namespace RSI::RapidCode;
using namespace cv;

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, toggling shutdown flag..." << std::endl;
  g_shutdown = true;
}

TimingStats total, rgbToHsv, masking, contourFinding, boundingBoxFinding;

bool DetectBall(Mat& rgbFrame, Rect& boundingBox)
{
  constexpr unsigned int CENTER_X = CameraHelpers::IMAGE_WIDTH / 2;
  constexpr unsigned int CENTER_Y = CameraHelpers::IMAGE_HEIGHT / 2;

  static Mat hsvFrame, mask, kernel;
  
  {
    Stopwatch rgbToHsvWatch(rgbToHsv);
    cvtColor(rgbFrame, hsvFrame, COLOR_RGB2HSV);
  }

  {
    Stopwatch maskingWatch(masking);
    Scalar lower_hsv(ImageProcessing::lowH, ImageProcessing::lowS, ImageProcessing::lowV);
    Scalar upper_hsv(ImageProcessing::highH, ImageProcessing::highS, ImageProcessing::highV);
    inRange(hsvFrame, lower_hsv, upper_hsv, mask);
  }

  std::vector<std::vector<Point>> contours;
  int largestContourIndex = -1;
  double largestContourArea = 0;
  {
    Stopwatch contourFindingWatch(contourFinding);
    kernel = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
    morphologyEx(mask, mask, MORPH_CLOSE, kernel);
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    contours.erase(
      std::remove_if(contours.begin(), contours.end(), [](const std::vector<Point> &c) { return contourArea(c) < ImageProcessing::MIN_CONTOUR_AREA; }),
      contours.end()
    );
    for (int i = 0; i < static_cast<int>(contours.size()); ++i)
    {
      double area = contourArea(contours[i]);
      if (area > largestContourArea)
      {
        largestContourArea = area;
        largestContourIndex = i;
      }
    }
    if (largestContourIndex == -1)
    {
      return false; // Skip to the next frame if no contours are found
    }
  }

  {
    Stopwatch boundingBoxWatch(boundingBoxFinding);
    boundingBox = boundingRect(contours[largestContourIndex]);
  }
  return true;
}

// --- Main Function ---
int main()
{
  const std::string EXECUTABLE_NAME = "Sandbox";
  PrintHeader(EXECUTABLE_NAME);
  int exitCode = 0;

  std::signal(SIGINT, sigint_handler);

  std::string videoFilePath = std::string(SOURCE_DIR) + "/test_video.mp4";
  VideoCapture video(videoFilePath);
  if (!video.isOpened())
  {
    std::cerr << "Error: Could not open video file: " << videoFilePath << std::endl;
    PrintFooter(EXECUTABLE_NAME, -1);
    return -1;
  }

  Mat rgbFrame, hsvFrame, mask, kernel;
  Rect ballBox;

  constexpr unsigned int CENTER_X = CameraHelpers::IMAGE_WIDTH / 2;
  constexpr unsigned int CENTER_Y = CameraHelpers::IMAGE_HEIGHT / 2;

  while (!g_shutdown)
  {
    bool success = video.read(rgbFrame);
    if (!success || rgbFrame.empty())
    {
      break;
    }

    Stopwatch totalWatch(total);

    if(!DetectBall(rgbFrame, ballBox)) continue;

    double targetX = ballBox.x + ballBox.width / 2 - CENTER_X;
    double targetY = ballBox.y + ballBox.height / 2 - CENTER_Y;
    targetX = CameraHelpers::RADIANS_PER_PIXEL * targetX / (2.0 * std::numbers::pi);
    targetY = CameraHelpers::RADIANS_PER_PIXEL * targetY / (2.0 * std::numbers::pi);
  }

  // Print timing statistics
  printStats("Total", total);
  printStats("RGB to HSV", rgbToHsv);
  printStats("Masking", masking);
  printStats("Contour Finding", contourFinding);
  printStats("Bounding Box Finding", boundingBoxFinding);

  cv::destroyAllWindows();

  PrintFooter(EXECUTABLE_NAME, exitCode);

  return exitCode;
}