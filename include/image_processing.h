#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <cstdint> // For uint8_t

#include <opencv2/opencv.hpp>

namespace ImageProcessing
{
  // Constants for image processing
  inline static constexpr double RED_THRESHOLD = 143.5; // Threshold for red channel (v) in YUYV format
  inline static constexpr double MIN_CIRCULARITY = 0.50; // Minimum circularity score for a valid circle
  inline static constexpr double MIN_CONTOUR_AREA = 100.0; // Minimum contour area to consider for ball detection
  
  // Offsets under this threshold are considered negligible and are ignored
  inline constexpr unsigned int PIXEL_THRESHOLD = 10; 

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