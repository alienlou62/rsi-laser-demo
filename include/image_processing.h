#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <cstdint> // For uint8_t
#include <numbers>

#include <opencv2/opencv.hpp>

namespace ImageProcessing
{
  // Offsets under this threshold are considered negligible and are ignored
  inline constexpr unsigned int PIXEL_THRESHOLD = 10; 

  // Constants for image processing
  inline static constexpr double RED_THRESHOLD = 145; // Threshold for red channel (v) in YUYV format
  inline static constexpr double MAX_CIRCLE_FIT_ERROR = 200; // Maximum error allowed for circle fitting to consider a contour as a valid ball
  inline static constexpr double MIN_CONTOUR_AREA = 100; // Minimum area for a contour to be considered valid

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