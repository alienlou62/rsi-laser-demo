#include "image_processing.h"
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>
#include <sstream>

bool ImageProcessing::TryConvertToRGB(const Pylon::CGrabResultPtr &grabResult, cv::Mat &rgbFrame, std::string *errorMsg)
{
  if (!grabResult)
  {
    if (errorMsg)
      *errorMsg = "[ImageProcessing] Grab result is null!";
    return false;
  }
  int width = grabResult->GetWidth();
  int height = grabResult->GetHeight();
  const uint8_t *pImageBuffer = static_cast<uint8_t *>(grabResult->GetBuffer());
  if (!pImageBuffer)
  {
    if (errorMsg)
      *errorMsg = "[ImageProcessing] Image buffer is null!";
    return false;
  }
  if (width <= 0 || height <= 0)
  {
    if (errorMsg)
      *errorMsg = "[ImageProcessing] Invalid image dimensions! Width: " + std::to_string(width) + ", Height: " + std::to_string(height);
    return false;
  }
  cv::Mat bayerFrame(height, width, CV_8UC1, (void *)pImageBuffer);
  if (bayerFrame.empty())
  {
    if (errorMsg)
      *errorMsg = "[ImageProcessing] Retrieved empty bayer frame!";
    return false;
  }
  try
  {
    cv::cvtColor(bayerFrame, rgbFrame, cv::COLOR_BayerBG2RGB);
    if (rgbFrame.empty())
    {
      if (errorMsg)
        *errorMsg = "[ImageProcessing] Bayer-to-RGB conversion produced empty frame!";
      return false;
    }
    return true;
  }
  catch (const cv::Exception &e)
  {
    if (errorMsg)
      *errorMsg = std::string("[ImageProcessing] OpenCV exception during Bayer-to-RGB conversion: ") + e.what();
    return false;
  }
  catch (const std::exception &e)
  {
    if (errorMsg)
      *errorMsg = std::string("[ImageProcessing] Standard exception during Bayer-to-RGB conversion: ") + e.what();
    return false;
  }
  catch (...)
  {
    if (errorMsg)
      *errorMsg = "[ImageProcessing] Unknown error during Bayer-to-RGB conversion.";
    return false;
  }
}

void ImageProcessing::ConvertToRGB(const Pylon::CGrabResultPtr &grabResult, cv::Mat &rgbFrame)
{
  std::string errorMsg;
  if (!TryConvertToRGB(grabResult, rgbFrame, &errorMsg))
  {
    throw std::runtime_error(errorMsg);
  }
}

bool ImageProcessing::TryProcessImage(const Pylon::CGrabResultPtr &grabResult, double &targetX, double &targetY, std::string *errorMsg)
{
  // Constants for image processing
  constexpr double CENTER_X = 320;
  constexpr double CENTER_Y = 240;
  constexpr double MIN_CIRCLE_RADIUS = 1.0;
  constexpr int lowH = 105, highH = 126, lowS = 0, highS = 255, lowV = 35, highV = 190;

  // Static variables to avoid reallocation
  static cv::Mat rgbFrame, hsvFrame, mask, kernel;

  if (!TryConvertToRGB(grabResult, rgbFrame, errorMsg))
  {
    return false;
  }

  cv::cvtColor(rgbFrame, hsvFrame, cv::COLOR_RGB2HSV);
  cv::Scalar lower_ball(lowH, lowS, lowV);
  cv::Scalar upper_ball(highH, highS, highV);
  cv::inRange(hsvFrame, lower_ball, upper_ball, mask);
  kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
  cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  contours.erase(
      std::remove_if(contours.begin(), contours.end(), [](const std::vector<cv::Point> &c) { return cv::contourArea(c) < 700.0; }), contours.end()
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
  if (errorMsg)
    *errorMsg = "[ImageProcessing] No valid target found.";
  return false;
}

void ImageProcessing::ProcessImage(const Pylon::CGrabResultPtr &grabResult, double &targetX, double &targetY)
{
  std::string errorMsg;
  if (!TryProcessImage(grabResult, targetX, targetY, &errorMsg))
  {
    throw std::runtime_error(errorMsg);
  }
}
