#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <cstdint> // For uint8_t

#include <opencv2/opencv.hpp>

namespace ImageProcessing
{
  // Constants for image processing
  inline static constexpr double MIN_CIRCLE_RADIUS = 1.0;
  inline static constexpr double MIN_CONTOUR_AREA = 700.0;

  inline static constexpr int lowH = 105, highH = 126, lowS = 0, highS = 255, lowV = 35, highV = 190;

  // If the target is within this pixel tolerance of the center, then we consider the offset to be zero
  inline static constexpr unsigned int PIXEL_TOLERANCE = 10;

  bool TryDetectBall(const cv::Mat& bayerFrame, double& offsetX, double& offsetY);

  // -- Utility functions to create OpenCV Mat objects --
  inline cv::Mat CreateBayerMat(int width, int height)
  {
    return cv::Mat(height, width, CV_8UC1);
  }

  inline cv::Mat WrapBayerBuffer(const uint8_t *pImageBuffer, int width, int height)
  {
    return cv::Mat(height, width, CV_8UC1, (void *)pImageBuffer);
  }

  inline cv::Mat CreateYUYVMat(int width, int height)
  {
    return cv::Mat(height, width, CV_8UC2);
  }

  inline cv::Mat WrapYUYVBuffer(const uint8_t *pImageBuffer, int width, int height)
  {
    return cv::Mat(height, width, CV_8UC2, (void *)pImageBuffer);
  }
};

#endif // IMAGE_PROCESSING_H