#include "image_processing.h"
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>
#include <sstream>

void ImageProcessing::ConvertToRGB(const Pylon::CGrabResultPtr &grabResult, cv::Mat &rgbFrame)
{
  if (!grabResult)
  {
    throw std::runtime_error("[ImageProcessing] Grab result is null!");
  }
  int width = grabResult->GetWidth();
  int height = grabResult->GetHeight();
  const uint8_t *pImageBuffer = static_cast<uint8_t *>(grabResult->GetBuffer());
  if (!pImageBuffer)
  {
    throw std::runtime_error("[ImageProcessing] Image buffer is null!");
  }
  if (width <= 0 || height <= 0)
  {
    throw std::runtime_error("[ImageProcessing] Invalid image dimensions! Width: " + std::to_string(width) + ", Height: " + std::to_string(height));
  }
  cv::Mat bayerFrame(height, width, CV_8UC1, (void *)pImageBuffer);
  if (bayerFrame.empty())
  {
    throw std::runtime_error("[ImageProcessing] Retrieved empty bayer frame!");
  }
  cv::cvtColor(bayerFrame, rgbFrame, cv::COLOR_BayerBG2RGB);
  if (rgbFrame.empty())
  {
    throw std::runtime_error("[ImageProcessing] Bayer-to-RGB conversion produced empty frame!");
  }
}

bool ImageProcessing::TryProcessImage(const Pylon::CGrabResultPtr &grabResult, double &targetX, double &targetY)
{
  // Static variables to avoid reallocation
  static cv::Mat rgbFrame, hsvFrame, mask, kernel;

  ConvertToRGB(grabResult, rgbFrame);

  cv::cvtColor(rgbFrame, hsvFrame, cv::COLOR_RGB2HSV);
  cv::Scalar lower_ball(lowH, lowS, lowV);
  cv::Scalar upper_ball(highH, highS, highV);
  cv::inRange(hsvFrame, lower_ball, upper_ball, mask);
  kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
  cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  contours.erase( std::remove_if(contours.begin(), contours.end(), 
    [](const std::vector<cv::Point> &c) { return cv::contourArea(c) < MIN_CONTOUR_AREA; }
    ), contours.end()
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
      targetX = center.x - CENTER_X;
      targetY = center.y - CENTER_Y;
      return true;
    }
  }
  return false;
}
