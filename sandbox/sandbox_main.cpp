#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>
#include <cstddef>
#include <cstdint>

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

bool DetectBall(const Mat& in, Vec3f& out)
{
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(in, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  // Find the most circular contour, that is of a minimum size
  double bestScore = 0.5; // Minimum score for a valid circle
  int bestIndex = -1;
  for (int i = 0; i < contours.size(); ++i)
  {
    const std::vector<cv::Point>& contour = contours[i];
    if (contour.size() < 5) continue; // Need at least 5 points for fitting an ellipse

    // Fit an ellipse to the contour
    cv::RotatedRect ellipse = cv::fitEllipse(contour);
    if (ellipse.size.width < 10 || ellipse.size.height < 10) continue; // Filter out small ellipses

    // Calculate the score based on the circularity of the ellipse
    double score = std::min(ellipse.size.width, ellipse.size.height) / std::max(ellipse.size.width, ellipse.size.height);
    if (score > bestScore)
    {
      bestScore = score;
      bestIndex = i;
    }
  }
  if (bestIndex < 0)
    return false; // No valid circle found

  Point2f center;
  float radius;
  minEnclosingCircle(contours[bestIndex], center, radius);
  out = Vec3f(center.x, center.y, radius);
  return true; // Circle found
}

void ExtractV(const Mat& in, Mat& out)
{
  // Extract the V channel from a YUV image
  for (int i = 0; i < in.rows; ++i)
  {
    const uchar* const inRow = in.ptr<uchar>(i);    // 2 channels per pixel
    uchar* outRow = out.ptr<uchar>(i / 2);

    for (int j = 0; j < CameraHelpers::IMAGE_WIDTH; j += 2)
    {
      outRow[j / 2] = inRow[2 * j + 3];      // channel 1 of pixel j+1 (V)
    }
  }
}

int ProcessYUYVImages()
{
  int exitCode = 0;
  
  // Setup the image reader/writer which will automatically read image into in and write processed image to out each loop
  Mat in = ImageProcessing::CreateYUYVMat(CameraHelpers::IMAGE_WIDTH, CameraHelpers::IMAGE_HEIGHT);
  Mat out(CameraHelpers::IMAGE_HEIGHT / 2, CameraHelpers::IMAGE_WIDTH / 2, CV_8UC3); // Subsampled 3 channel output
  ImageHelpers::ImageReaderWriter readerWriter(ImageHelpers::ImageType::YUYV, in, out);

  TimingStats timing;

  Mat yuv(out.size(), CV_8UC3);
  Mat v(out.size(), CV_8UC1);
  Mat mask(out.size(), CV_8UC1);
  Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(7, 7));
  Vec3f ball;
  for (int index : readerWriter)
  {
    Stopwatch stopwatch(timing);
    ExtractV(in, v); // Extract the V channel from the YUYV image
    threshold(v, mask, 144, 255, THRESH_BINARY); // Threshold the V channel to create a mask

    // Clean up the mask
    morphologyEx(mask, mask, MORPH_CLOSE, kernel); // Close small holes in the mask
    morphologyEx(mask, mask, MORPH_OPEN, kernel); // Remove noise with morphological opening

    bool foundBall = DetectBall(mask, ball);
    stopwatch.Stop(); // Stop the stopwatch for timing

    // Draw the detected ball if it exists
    cvtColor(mask, out, COLOR_GRAY2BGR); // Convert mask to 3-channel BGR for output
    if (foundBall) // If radius is valid
    {
      Point2f center(ball[0], ball[1]);
      float radius = ball[2];
      circle(out, center, static_cast<int>(radius), Scalar(0, 255, 0), 1); // Draw circle around detected ball
    }
  }

  printStats<std::chrono::microseconds>("YUYV Image Processing", timing);

  return exitCode;
}

void SubsampleBayer(const Mat& in, Mat& out)
{
  // Subsample the Bayer image by taking every second 2x2 pixel block
  for (int y = 0; y < CameraHelpers::IMAGE_HEIGHT; y += 4) {
    const uchar* const row0 = in.ptr<uchar>(y);
    const uchar* const row1 = in.ptr<uchar>(y + 1);

    uchar* outRow0 = out.ptr<uchar>(y / 2);
    uchar* outRow1 = out.ptr<uchar>(y / 2 + 1);

    for (int x = 0; x < CameraHelpers::IMAGE_WIDTH; x += 4)
    {
      outRow0[x / 2    ] = row0[x    ];
      outRow0[x / 2 + 1] = row0[x + 1];
      outRow1[x / 2    ] = row1[x    ];
      outRow1[x / 2 + 1] = row1[x + 1];
    }
  }
}

void SubsampleYUYV(const Mat& in, Mat& out)
{
  for (int i = 0; i < CameraHelpers::IMAGE_HEIGHT; i += 2)
  {
    const uchar* const inRow = in.ptr<uchar>(i);    // 2 channels per pixel
    uchar* outRow = out.ptr<uchar>(i / 2);          // 3 channels per pixel

    for (int j = 0; j < CameraHelpers::IMAGE_WIDTH; j += 2)
    {
      outRow[3 * (j / 2)    ] = inRow[2 * j    ];      // channel 0 of pixel j (Y)
      outRow[3 * (j / 2) + 1] = inRow[2 * j + 1];      // channel 1 of pixel j (U)
      outRow[3 * (j / 2) + 2] = inRow[2 * j + 3];      // channel 1 of pixel j+1 (V)
    }
  }
}

int ProcessBayerImages()
{
  int exitCode = 0;
  
  // Setup the image reader/writer which will automatically read image into in and write processed image to out each loop
  Mat in = ImageProcessing::CreateBayerMat(CameraHelpers::IMAGE_WIDTH, CameraHelpers::IMAGE_HEIGHT);
  Mat out(CameraHelpers::IMAGE_HEIGHT / 2, CameraHelpers::IMAGE_WIDTH / 2, CV_8UC3); // Subsampled 3 channel output
  ImageHelpers::ImageReaderWriter readerWriter(ImageHelpers::ImageType::BAYER, in, out);

  TimingStats timing;

  Mat subsampled(out.size(), CV_8UC1);
  Mat rgb(out.size(), CV_8UC3);
  Mat hsv(out.size(), CV_8UC3);
  Mat mask1(out.size(), CV_8UC1);
  Mat mask2(out.size(), CV_8UC1);
  Mat mask(out.size(), CV_8UC1);
  Vec3f ball;
  for (int index : readerWriter)
  {
    Stopwatch stopwatch(timing);
    SubsampleBayer(in, subsampled);
    cvtColor(subsampled, rgb, COLOR_BayerBG2BGR);
    cvtColor(rgb, hsv, COLOR_RGB2HSV);

    inRange(hsv, Scalar(0, 30, 30), Scalar(15, 255, 255), mask1);
    inRange(hsv, Scalar(160, 30, 30), Scalar(180, 255, 255), mask2);
    bitwise_or(mask1, mask2, mask);

    cvtColor(mask, out, COLOR_GRAY2BGR); // Convert mask to 3-channel BGR for output

    bool foundBall = DetectBall(mask, ball);

    // Draw the detected ball if it exists
    if (foundBall) // If radius is valid
    {
      Point2f center(ball[0], ball[1]);
      float radius = ball[2];
      circle(out, center, static_cast<int>(radius), Scalar(0, 255, 0), 2); // Draw circle around detected ball
    }
  }

  printStats<std::chrono::microseconds>("Bayer Image Processing", timing);

  return exitCode;
}

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, toggling shutdown flag..." << std::endl;
  g_shutdown = true;
}

// --- Main Function ---
int main()
{
  const std::string EXECUTABLE_NAME = "Sandbox";
  MiscHelpers::PrintHeader(EXECUTABLE_NAME);
  std::signal(SIGINT, sigint_handler);
  cv::setNumThreads(0); // Turn off OpenCV threading

  int exitCode = 0;
  // exitCode += ProcessBayerImages();
  exitCode += ProcessYUYVImages();

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
