#include "image_processing.h"

#include <numbers>
#include <opencv2/opencv.hpp>

#include "camera_helpers.h" // For image constants

using namespace cv;

namespace ImageProcessing
{
  void ExtractV(const Mat& in, Mat& out)
  {
    // Extract the V channel from a YUV image
    for (int i = 0; i < CameraHelpers::IMAGE_HEIGHT; ++i)
    {
      const uchar* const inRow = in.ptr<uchar>(i);    // 2 channels per pixel
      uchar* outRow = out.ptr<uchar>(i / 2);

      for (int j = 0; j < CameraHelpers::IMAGE_WIDTH; j += 2)
      {
        outRow[j / 2] = inRow[2 * j + 3];      // channel 1 of pixel j+1 (V)
      }
    }
  }

  void MaskV(const Mat& in, Mat& out)
  {
    static Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(7, 7));

    threshold(in, out, RED_THRESHOLD, 255, THRESH_BINARY);
    morphologyEx(out, out, MORPH_CLOSE, kernel);
    morphologyEx(out, out, MORPH_OPEN, kernel);
  }

  double ScoreContour(const std::vector<Point>& contour)
  {
    if (contour.size() < MIN_CONTOUR_AREA) return 0.0; // Not enough points to form a valid contour

    RotatedRect ellipse = fitEllipse(contour);
    return std::min(ellipse.size.width, ellipse.size.height) / std::max(ellipse.size.width, ellipse.size.height);
  }

  bool FindBall(const Mat& mask, Vec3f &ball)
  {
    std::vector<std::vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty())
      return false; // No contours found

    // Find the largest contour
    size_t bestIndex = -1;
    double bestScore = MIN_CIRCULARITY;
    for (size_t i = 0; i < contours.size(); ++i)
    {
      double score = ScoreContour(contours[i]);
      if (score > bestScore)
      {
        bestScore = score;
        bestIndex = i;
      }
    }

    if (bestIndex == -1)
      return false; // No valid contour found

    Point2f center;
    float radius;
    minEnclosingCircle(contours[bestIndex], center, radius);
    ball = Vec3f(center.x, center.y, radius);
    return true; // Circle found
  }

  void CalculateTargetPosition(const Vec3f& ball, double &offsetX, double &offsetY)
  {
    // Image is downsampled to 1/2 size, so all constants need to be adjusted accordingly
    constexpr unsigned int CENTER_X = CameraHelpers::IMAGE_WIDTH / 4;
    constexpr unsigned int CENTER_Y = CameraHelpers::IMAGE_HEIGHT / 4;
    constexpr unsigned int MIN_PIXEL_OFFSET = PIXEL_THRESHOLD / 2;
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

  bool TryDetectBall(const Mat& yuyvFrame, double &offsetX, double &offsetY)
  {
    // Static variables to avoid reallocation
    static Mat v(CameraHelpers::IMAGE_HEIGHT / 2, CameraHelpers::IMAGE_WIDTH / 2, CV_8UC1);
    static Vec3f ball;

    bool foundBall = false;

    ExtractV(yuyvFrame, v);
    MaskV(v, v);
    foundBall = FindBall(v, ball);
    if(!foundBall) return false;

    CalculateTargetPosition(ball, offsetX, offsetY);

    return true;
  }
} // namespace ImageProcessing
