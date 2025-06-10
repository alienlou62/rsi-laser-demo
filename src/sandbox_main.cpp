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

#ifndef CONFIG_FILE
#define CONFIG_FILE ""
#endif // CONFIG_FILE

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

// Helper class for automatic VideoWriter management
class VideoWriterHelper {
public:
  VideoWriter writer;
  std::string filename;
  VideoWriterHelper(const std::string& title, int width, int height, double fps)
  {
    filename = title + ".mp4";
    // Hardcoded: avc1 codec, 30 fps fallback, .mp4 extension
    writer.open(
      filename,
      VideoWriter::fourcc('a', 'v', 'c', '1'),
      (fps > 0) ? fps : 30,
      Size(width, height)
    );
    if (!writer.isOpened()) {
      throw std::runtime_error("Failed to open video writer: " + filename);
    }
  }
  ~VideoWriterHelper() {
    writer.release();
  }
  void write(const Mat& frame) { writer.write(frame); }
};

std::vector<Vec3b> CollectRedBallPixels(VideoCapture& video)
{
  // --- Collect red ball pixels from first 30 frames ---
  std::vector<Vec3b> ballPixels;
  Mat frame;
  for (int i = 0; i < 30; ++i) {
    if (!video.read(frame) || frame.empty()) break;

    Mat hsv;
    cvtColor(frame, hsv, COLOR_BGR2HSV);

    // Looser bounds!
    Mat mask1, mask2, mask;
    inRange(hsv, Scalar(0, 30, 30), Scalar(15, 255, 255), mask1);
    inRange(hsv, Scalar(165, 30, 30), Scalar(180, 255, 255), mask2);
    mask = mask1 | mask2;

    morphologyEx(mask, mask, MORPH_OPEN, getStructuringElement(MORPH_ELLIPSE, Size(5,5)));

    // Find largest contour (presume it's the ball)
    std::vector<std::vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    if (!contours.empty()) {
    size_t largest = 0;
      double maxArea = 0;
      for (size_t c = 0; c < contours.size(); ++c) {
        double area = contourArea(contours[c]);
        if (area > maxArea) { maxArea = area; largest = c; }
      }
      
      // Fit a circle to the largest contour and draw it
      Point2f center;
      float radius;
      minEnclosingCircle(contours[largest], center, radius);
      circle(frame, center, static_cast<int>(radius), Scalar(255, 0, 0), 2); // blue circle

      // Collect pixels inside the circle
      int x0 = std::max(0, static_cast<int>(center.x - radius));
      int x1 = std::min(frame.cols - 1, static_cast<int>(center.x + radius));
      int y0 = std::max(0, static_cast<int>(center.y - radius));
      int y1 = std::min(frame.rows - 1, static_cast<int>(center.y + radius));
      float r2 = radius * radius;
      for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
          float dx = x - center.x;
          float dy = y - center.y;
          if ((dx*dx + dy*dy) <= r2)
            ballPixels.push_back(hsv.at<Vec3b>(y, x));
        }
      }
    }
  }
  return ballPixels;
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
  if (!video.isOpened()) {
    std::cerr << "Error: Could not open video file: " << videoFilePath << std::endl;
    PrintFooter(EXECUTABLE_NAME, -1);
    return -1;
  }

  int frame_width = static_cast<int>(video.get(CAP_PROP_FRAME_WIDTH));
  int frame_height = static_cast<int>(video.get(CAP_PROP_FRAME_HEIGHT));
  double fps = video.get(CAP_PROP_FPS);

  VideoWriterHelper outputVideo("output_annotated", frame_width, frame_height, fps);
  VideoWriterHelper maskVideo("output_mask", frame_width, frame_height, fps);

  std::vector<Vec3b> redBallPixels = CollectRedBallPixels(video);

  Mat frame, hsv, mask, maskBGR;
  while(video.read(frame) && !frame.empty())
  {
    cvtColor(frame, hsv, COLOR_BGR2HSV);
    
    Mat mask1, mask2, mask;
    inRange(hsv, Scalar(0, 30, 30), Scalar(15, 255, 255), mask1);
    inRange(hsv, Scalar(165, 30, 30), Scalar(180, 255, 255), mask2);
    mask = mask1 | mask2;

    cvtColor(mask, maskBGR, COLOR_GRAY2BGR);
    maskVideo.write(maskBGR);
  }

  PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
