#include <chrono>
#include <cmath>
#include <csignal>
#include <fstream>
#include <iostream>
#include <numbers>
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <functional>
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

#ifndef SOURCE_DIR
#define SOURCE_DIR ""
#endif // SOURCE_DIR

#ifndef CONFIG_FILE
#define CONFIG_FILE ""
#endif // CONFIG_FILE

using namespace RSI::RapidCode;
using namespace cv;
using namespace Pylon;
using namespace GenApi;

constexpr const char* const BAYER_FOLDER = SOURCE_DIR "test_images/bayer/";
constexpr const unsigned int NUM_BAYER_IMAGES = 20;

constexpr const char* const YUYV_FOLDER = SOURCE_DIR "test_images/yuyv/";
constexpr const unsigned int NUM_YUYV_IMAGES = 18;

enum class ImageType
{
  BAYER,
  YUYV
};

std::string ImageFileName(unsigned int index, ImageType type)
{
  std::string folder;
  switch (type)
  {
    case ImageType::BAYER:
      folder = BAYER_FOLDER;
      if (index >= NUM_BAYER_IMAGES)
      {
        throw std::out_of_range("Index out of range for Bayer images");
      }
      break;
    case ImageType::YUYV:
      folder = YUYV_FOLDER;
      if (index >= NUM_YUYV_IMAGES)
      {
        throw std::out_of_range("Index out of range for YUYV images");
      }
      break;
    default:
      throw std::invalid_argument("Invalid image type");
  }
  return folder + std::string("Image") + std::to_string(index) + std::string(".raw");
}

bool ReadImage(Mat& img, unsigned int index, ImageType type)
{
  if (type != ImageType::BAYER && type != ImageType::YUYV)
  {
    std::cerr << "Unsupported image type." << std::endl;
    return false;
  }
  if (index >= (type == ImageType::BAYER ? NUM_BAYER_IMAGES : NUM_YUYV_IMAGES))
  {
    std::cerr << "Index out of range for the specified image type." << std::endl;
    return false;
  }
  
  std::string fileName = ImageFileName(index, type);
  std::ifstream file(fileName, std::ios::binary);
  if (!file.is_open())
  {
    std::cerr << "Failed to open image file: " << fileName << std::endl;
    return false;
  }

  // Read the raw image data
  if (type == ImageType::BAYER)
  {
    img.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC1);
  }
  else if (type == ImageType::YUYV)
  {
    img.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC2);
  }
  else
  {
    std::cerr << "Unsupported image type" << std::endl;
    file.close();
    return false;
  }

  file.read(reinterpret_cast<char*>(img.data), img.total() * img.elemSize());
  if (!file)
  {
    std::cerr << "Failed to read image data from file: " << fileName << std::endl;
    file.close();
    return false;
  }

  file.close();
  return true;
}

void ProcessFrame(const Mat& inFrame, Mat& outFrame)
{
  // Convert YUYV to a 3-channel YUV image
  outFrame.create(inFrame.rows, inFrame.cols, CV_8UC3);
  for (int i = 0; i < inFrame.rows; ++i)
  {
    for (int j = 0; j < inFrame.cols; j += 2)
    {
      // Extract Y, U, and V components
      uchar y0 = inFrame.at<Vec2b>(i, j)[0];
      uchar u = inFrame.at<Vec2b>(i, j)[1];
      uchar y1 = inFrame.at<Vec2b>(i, j + 1)[0];
      uchar v = inFrame.at<Vec2b>(i, j + 1)[1];

      // Store in the YUV image
      outFrame.at<Vec3b>(i, j) = Vec3b(y0, u, v);
      if (j + 1 < inFrame.cols)
      {
        outFrame.at<Vec3b>(i, j + 1) = Vec3b(y1, u, v);
      }
    }
  }
}

int Sandbox()
{
  int exitCode = 0;

  TimingStats timing;
  Mat inFrame, bgrFrame, outFrame;
  inFrame.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC1); // For YUYV images
  bgrFrame.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC3); // For RGB output
  outFrame.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC3); // For HSV output
  for (int i = 0; i < NUM_YUYV_IMAGES; ++i)
  {
    if (!ReadImage(inFrame, i, ImageType::BAYER))
    {
      std::cerr << "Failed to read image " << i << std::endl;
      exitCode = 1;
      break;
    }

    Stopwatch stopwatch(timing);
    // ProcessFrame(inFrame, outFrame);
    cvtColor(inFrame, bgrFrame, COLOR_BayerBG2BGR); // Convert Bayer to BGR
    cvtColor(bgrFrame, outFrame, COLOR_BGR2HSV); // Convert BGR to HSV
    stopwatch.Stop();
    
    // Save the RGB frame to a file
    std::string outputFileName = SOURCE_DIR "build/out_" + std::to_string(i) + ".png";
    if (!imwrite(outputFileName, outFrame))
    {
      std::cerr << "Failed to save output image to file: " << outputFileName << std::endl;
      exitCode = 1;
      break;
    }
  }

  printStats<std::chrono::microseconds>("YUYV Frame Processing", timing);

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
  PrintHeader(EXECUTABLE_NAME);
  std::signal(SIGINT, sigint_handler);
  cv::setNumThreads(0); // Set OpenCV to use a single thread for consistency

  int exitCode = Sandbox();

  PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
