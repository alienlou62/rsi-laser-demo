#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>
#include <cstddef>
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
#include "image_helpers.h"

#ifndef SANDBOX_DIR
#define SANDBOX_DIR ""
#endif // SANDBOX_DIR

using namespace RSI::RapidCode;
using namespace cv;
using namespace Pylon;
using namespace GenApi;

void SubsampleBayer(const Mat& inFrame, Mat& outFrame)
{
  // Subsample the Bayer image by taking every second 2x2 pixel block
  outFrame.create(inFrame.rows / 2, inFrame.cols / 2, CV_8UC1);

  for (int y = 0; y < inFrame.rows; y += 4) {
    const uchar* row0 = inFrame.ptr<uchar>(y);
    const uchar* row1 = inFrame.ptr<uchar>(y + 1);

    uchar* outRow0 = outFrame.ptr<uchar>(y / 2);
    uchar* outRow1 = outFrame.ptr<uchar>(y / 2 + 1);

    for (int x = 0; x < inFrame.cols; x += 4)
    {
      outRow0[x / 2    ] = row0[x    ];
      outRow0[x / 2 + 1] = row0[x + 1];
      outRow1[x / 2    ] = row1[x    ];
      outRow1[x / 2 + 1] = row1[x + 1];
    }
  }
}

void SubsampleYUYV(const Mat& inFrame, Mat& outFrame)
{
  for (int i = 0; i < inFrame.rows; i += 2)
  {
    const uchar* inRow = inFrame.ptr<uchar>(i);          // 2 channels per pixel
    uchar* outRow = outFrame.ptr<uchar>(i / 2);          // 3 channels per pixel

    for (int j = 0; j < inFrame.cols - 1; j += 2)
    {
      outRow[3 * (j / 2)    ] = inRow[2 * j    ];      // channel 0 of pixel j (Y)
      outRow[3 * (j / 2) + 1] = inRow[2 * j + 1];      // channel 1 of pixel j (U)
      outRow[3 * (j / 2) + 2] = inRow[2 * j + 3];      // channel 1 of pixel j+1 (V)
    }
  }
}

int ProcessBayerImages()
{
  int exitCode = 0;
  
  // Setup the image reader/writer which will automatically read image into inFrame and write processed image to outFrame each loop
  Mat inFrame = ImageProcessing::CreateBayerMat(CameraHelpers::IMAGE_WIDTH, CameraHelpers::IMAGE_HEIGHT);
  Mat outFrame(CameraHelpers::IMAGE_HEIGHT / 2, CameraHelpers::IMAGE_WIDTH / 2, CV_8UC3); // Subsampled 3 channel output
  ImageHelpers::ImageReaderWriter readerWriter(ImageHelpers::ImageType::BAYER, inFrame, outFrame);

  TimingStats timing;

  Mat subsampledFrame(outFrame.size(), CV_8UC1);
  Mat rgbFrame(outFrame.size(), CV_8UC3);
  for (int index : readerWriter)
  {
    Stopwatch stopwatch(timing);
    SubsampleBayer(inFrame, subsampledFrame);
    cvtColor(subsampledFrame, rgbFrame, COLOR_BayerBG2RGB);
    cvtColor(rgbFrame, outFrame, COLOR_RGB2HSV);
  }

  printStats<std::chrono::microseconds>("Bayer Image Processing", timing);

  return exitCode;
}

int ProcessYUYVImages()
{
  int exitCode = 0;
  
  // Setup the image reader/writer which will automatically read image into inFrame and write processed image to outFrame each loop
  Mat inFrame = ImageProcessing::CreateYUYVMat(CameraHelpers::IMAGE_WIDTH, CameraHelpers::IMAGE_HEIGHT);
  Mat outFrame(CameraHelpers::IMAGE_HEIGHT / 2, CameraHelpers::IMAGE_WIDTH / 2, CV_8UC3); // Subsampled 3 channel output
  ImageHelpers::ImageReaderWriter readerWriter(ImageHelpers::ImageType::YUYV, inFrame, outFrame);

  TimingStats timing;

  for (int index : readerWriter)
  {
    Stopwatch stopwatch(timing);
    SubsampleYUYV(inFrame, outFrame);
  }

  printStats<std::chrono::microseconds>("YUYV Image Processing", timing);

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
  MiscHelpers::PrintHeader(EXECUTABLE_NAME);
  std::signal(SIGINT, sigint_handler);
  cv::setNumThreads(0); // Turn off OpenCV threading

  int exitCode = 0;
  exitCode += ProcessBayerImages();
  exitCode += ProcessYUYVImages();

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
