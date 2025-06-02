#include "image_processing.h"

#include <numbers>

#include <opencv2/opencv.hpp>

#include "camera_helpers.h" // For image constants

bool ImageProcessing::TryProcessImage(const uint8_t *pImageBuffer, int width, int height, double &targetX, double &targetY)
{
  constexpr unsigned int CENTER_X = CameraHelpers::IMAGE_WIDTH / 2;
  constexpr unsigned int CENTER_Y = CameraHelpers::IMAGE_HEIGHT / 2;

  // Static variables to avoid reallocation
  static cv::Mat rgbFrame, hsvFrame, mask, kernel;

  cv::Mat bayerFrame(height, width, CV_8UC1, (void *)pImageBuffer);
  cv::cvtColor(bayerFrame, rgbFrame, cv::COLOR_BayerBG2RGB);
  cv::cvtColor(rgbFrame, hsvFrame, cv::COLOR_RGB2HSV);
  cv::Scalar lower_ball(lowH, lowS, lowV);
  cv::Scalar upper_ball(highH, highS, highV);
  cv::inRange(hsvFrame, lower_ball, upper_ball, mask);
  kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
  cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  contours.erase(
      std::remove_if(contours.begin(), contours.end(), [](const std::vector<cv::Point> &c) { return cv::contourArea(c) < MIN_CONTOUR_AREA; }),
      contours.end()
  );
  int largestContourIndex = -1;
  double largestContourArea = 0;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    double area = cv::contourArea(contours[i]);
    if (area > largestContourArea)
    {
      largestContourArea = area;
      largestContourIndex = i;
    }
  }
  if (largestContourIndex != -1)
  {
    cv::Point2f center;
    float radius;
    cv::minEnclosingCircle(contours[largestContourIndex], center, radius);
    if (radius > MIN_CIRCLE_RADIUS)
    {
      int pixelOffsetX = center.x - CENTER_X;
      int pixelOffsetY = center.y - CENTER_Y;

      // Calculate the targets in revolutions based on the pixel offsets
      targetX = CameraHelpers::RADIANS_PER_PIXEL * pixelOffsetX / (2.0 * std::numbers::pi);
      targetY = CameraHelpers::RADIANS_PER_PIXEL * pixelOffsetY / (2.0 * std::numbers::pi);
      return true;
    }
  }
  return false;
}
