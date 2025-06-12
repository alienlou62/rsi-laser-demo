#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <cstdint> // For uint8_t

#include <opencv2/opencv.hpp>

class ImageProcessing
{
public:
  // Constants for image processing
  static constexpr double MIN_CIRCLE_RADIUS = 1.0;
  static constexpr double MIN_CONTOUR_AREA = 700.0;

  static constexpr int lowH = 105, highH = 126, lowS = 0, highS = 255, lowV = 35, highV = 190;

  // If the target is within this pixel tolerance of the center, then we consider the offset to be zero
  static constexpr unsigned int PIXEL_TOLERANCE = 10;

  static bool TryDetectBall(const cv::Mat& bayerFrame, double& offsetX, double& offsetY);

  // -- Utility functions to wrap image buffers into cv::Mat objects --
  static inline cv::Mat WrapBayerBuffer(const uint8_t *pImageBuffer, int width, int height)
  {
    return cv::Mat(height, width, CV_8UC1, (void *)pImageBuffer);
  }

  static inline cv::Mat WrapYUYVBuffer(const uint8_t *pImageBuffer, int width, int height)
  {
    return cv::Mat(height, width, CV_8UC2, (void *)pImageBuffer);
  }
};

#endif // IMAGE_PROCESSING_H