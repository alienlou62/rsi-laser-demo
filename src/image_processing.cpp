#include "image_processing.h"

#include <numbers>
#include <opencv2/opencv.hpp>

#include "camera_helpers.h" // For image constants

using namespace cv;

bool ImageProcessing::TryDetectBall(const uint8_t *pImageBuffer, int width, int height, double &offsetX, double &offsetY)
{
  constexpr unsigned int CENTER_X = CameraHelpers::IMAGE_WIDTH / 2;
  constexpr unsigned int CENTER_Y = CameraHelpers::IMAGE_HEIGHT / 2;

  // Static variables to avoid reallocation
  static Mat rgbFrame, hsvFrame, mask, kernel;

  Mat bayerFrame(height, width, CV_8UC1, (void *)pImageBuffer);
  cvtColor(bayerFrame, rgbFrame, COLOR_BayerBG2RGB);
  cvtColor(rgbFrame, hsvFrame, COLOR_RGB2HSV);
  Scalar lower_ball(lowH, lowS, lowV);
  Scalar upper_ball(highH, highS, highV);
  inRange(hsvFrame, lower_ball, upper_ball, mask);
  kernel = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
  morphologyEx(mask, mask, MORPH_CLOSE, kernel);
  std::vector<std::vector<Point>> contours;
  findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
  contours.erase(
      std::remove_if(contours.begin(), contours.end(), [](const std::vector<Point> &c) { return contourArea(c) < MIN_CONTOUR_AREA; }),
      contours.end()
  );
  int largestContourIndex = -1;
  double largestContourArea = 0;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    double area = contourArea(contours[i]);
    if (area > largestContourArea)
    {
      largestContourArea = area;
      largestContourIndex = i;
    }
  }
  bool foundBall = false;
  if (largestContourIndex != -1)
  {
    Point2f center;
    float radius;
    minEnclosingCircle(contours[largestContourIndex], center, radius);
    if (radius > MIN_CIRCLE_RADIUS)
    {
      int pixelOffsetX = center.x - CENTER_X;
      int pixelOffsetY = center.y - CENTER_Y;

      // Convert pixel offsets to revolutions if they exceed the tolerance
      // Offsets are inverted to match the axes coordinate system compared to the camera's coordinate system
      offsetX = 0.0;
      if (std::abs(pixelOffsetX) > PIXEL_TOLERANCE)
      {
        offsetX = -CameraHelpers::RADIANS_PER_PIXEL * pixelOffsetX / (2.0 * std::numbers::pi);
      }

      offsetY = 0.0;
      if (std::abs(pixelOffsetY) > PIXEL_TOLERANCE)
      {
        offsetY = -CameraHelpers::RADIANS_PER_PIXEL * pixelOffsetY / (2.0 * std::numbers::pi);
      }
      foundBall = true;
    }
  }

  //imshow("RGB Frame", rgbFrame);
  //waitKey(1); // Display the RGB frame for 1 ms
  return foundBall;
}
