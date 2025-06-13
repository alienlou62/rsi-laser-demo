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

enum class ImageType
{
  BAYER,
  YUYV
};

constexpr const char* const BAYER_FOLDER = SOURCE_DIR "test_images/bayer/";
constexpr const char* const BAYER_OUTPUT_FOLDER = SOURCE_DIR "test_images/output/bayer/";
constexpr const unsigned int NUM_BAYER_IMAGES = 21;

constexpr const char* const YUYV_FOLDER = SOURCE_DIR "test_images/yuyv/";
constexpr const char* const YUYV_OUTPUT_FOLDER = SOURCE_DIR "test_images/output/yuyv/";
constexpr const unsigned int NUM_YUYV_IMAGES = 18;

std::string ImageFileName(unsigned int index)
{
  return std::string("Image") + std::to_string(index) + std::string(".raw");
}

class ImageReaderWriter
{
  using NameGenerator = std::function<std::string(unsigned int)>;

  const NameGenerator inputNameGenerator;
  const std::string inputFolder;
  Mat& inFrame;

  const NameGenerator outputNameGenerator;
  const std::string outputFolder;
  Mat& outFrame;

  const ImageType type;
  const unsigned int numImages;
  int loadedIndex = -1; // Track the index of the currently loaded image
public:
  ImageReaderWriter(const ImageType type, Mat& inFrame, Mat& outFrame)
    : inputNameGenerator(ImageFileName),
      inputFolder((type == ImageType::BAYER) ? BAYER_FOLDER : YUYV_FOLDER),
      inFrame(inFrame),
      outputNameGenerator(ImageFileName),
      outputFolder((type == ImageType::BAYER) ? BAYER_OUTPUT_FOLDER : YUYV_OUTPUT_FOLDER),
      outFrame(outFrame),
      type(type),
      numImages((type == ImageType::BAYER) ? NUM_BAYER_IMAGES : NUM_YUYV_IMAGES)
  {}

  bool ReadImage(unsigned int index)
  {
    if (index >= numImages)
    {
      std::cerr << "Index out of range for the specified image type." << std::endl;
      return false;
    }

    std::string fileName = inputFolder + inputNameGenerator(index);
    std::ifstream file(fileName, std::ios::binary);
    if (!file.is_open())
    {
      std::cerr << "Failed to open image file: " << fileName << std::endl;
      return false;
    }

    // Read the raw image data
    if (type == ImageType::BAYER)
    {
      inFrame.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC1);
    }
    else if (type == ImageType::YUYV)
    {
      inFrame.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC2);
    }
    else
    {
      std::cerr << "Unsupported image type" << std::endl;
      file.close();
      return false;
    }

    file.read(reinterpret_cast<char*>(inFrame.data), inFrame.total() * inFrame.elemSize());
    if (!file)
    {
      std::cerr << "Failed to read image data from file: " << fileName << std::endl;
      file.close();
      return false;
    }

    file.close();
    loadedIndex = index; // Update the loaded index
    return true;
  }

  bool WriteImage(unsigned int index)
  {
    if (index >= numImages)
    {
      std::cerr << "Index out of range for the specified image type." << std::endl;
      return false;
    }

    std::string fileName = outputFolder + outputNameGenerator(index);
    if (!imwrite(fileName, outFrame))
    {
      std::cerr << "Failed to write image to file: " << fileName << std::endl;
      return false;
    }

    return true;
  }

  class Iterator
  {
    ImageReaderWriter& readerWriter;
    unsigned int currentIndex;
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = unsigned int;
    using difference_type = std::ptrdiff_t;
    using pointer = unsigned int*;
    using reference = unsigned int&;

    Iterator(ImageReaderWriter& rw, unsigned int index)
      : readerWriter(rw), currentIndex(index)
    {
      // Attempt to read the first image
      if (currentIndex < readerWriter.numImages)
      {
        if (!readerWriter.ReadImage(currentIndex))
        {
          std::cerr << "Failed to read image at index " << currentIndex << std::endl;
          currentIndex = readerWriter.numImages; // Move to end
        }
      }
    }
    Iterator& operator++()
    { 
      // Write the current image to the output folder
      if (!readerWriter.WriteImage(currentIndex))
      {
        std::cerr << "Failed to write image at index " << (currentIndex) << std::endl;
      }

      // Move to the next index
      ++currentIndex;

      // If we have not reached the end, read the next image
      if (currentIndex < readerWriter.numImages)
      {
        if (!readerWriter.ReadImage(currentIndex))
        {
          std::cerr << "Failed to read image at index " << currentIndex << std::endl;
          currentIndex = readerWriter.numImages; // Move to end
        }
      }
      return *this;
    }
    bool operator==(const Iterator& other) const { return currentIndex == other.currentIndex; }
    bool operator!=(const Iterator& other) const { return !(*this == other); }
    unsigned int operator*() const { return currentIndex; }
  };

  Iterator begin() { return Iterator(*this, 0); }
  Iterator end() { return Iterator(*this, numImages); }
};

void SubsambleBayer(const Mat& inFrame, Mat& outFrame)
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

int Sandbox()
{
  int exitCode = 0;
  
  // Setup the image reader/writer
  Mat inFrame(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC1); // For Bayer input
  Mat outFrame(CameraHelpers::IMAGE_HEIGHT / 2, CameraHelpers::IMAGE_WIDTH / 2, CV_8UC3); // Subsampled 3 channel output
  ImageReaderWriter readerWriter(ImageType::BAYER, inFrame, outFrame);

  TimingStats timing;

  // For each loop, inFrame will automatically be loaded with the next image
  // and outFrame will automatically be written to the output folder
  for (int index : readerWriter)
  {
    Stopwatch stopwatch(timing);
  }

  printStats<std::chrono::microseconds>("Image Processing", timing);

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
  cv::setNumThreads(0); // Turn off OpenCV threading

  int exitCode = Sandbox();

  PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
