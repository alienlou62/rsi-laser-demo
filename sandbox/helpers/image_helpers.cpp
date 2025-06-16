#include <fstream>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <cstddef>
#include "camera_helpers.h"
#include "helpers/image_helpers.h"

namespace ImageHelpers {

  ImageReaderWriter::ImageReaderWriter(const ImageType type, cv::Mat& inFrame, cv::Mat& outFrame)
    : type(type), inFrame(inFrame), outFrame(outFrame)
  {
    const std::string imageFolder = (type == ImageType::BAYER) ? BAYER_FOLDER : YUYV_FOLDER;
    const std::filesystem::path inputFolder = std::filesystem::path(INPUT_FOLDER) / imageFolder;
    for (const auto& entry : std::filesystem::directory_iterator(inputFolder)) {
      if (entry.is_regular_file() && entry.path().extension() == INPUT_IMAGE_EXTENSION) {
        inputFiles.push_back(entry.path());
      }
    }
    numImages = static_cast<unsigned int>(inputFiles.size());
    std::sort(inputFiles.begin(), inputFiles.end());
    const std::string outputFolder = std::filesystem::path(OUTPUT_FOLDER) / imageFolder;
    std::filesystem::create_directories(outputFolder);
    outputFiles.reserve(numImages);
    for (const auto& p : inputFiles) {
      outputFiles.push_back(std::filesystem::path(outputFolder) / p.filename().replace_extension(OUTPUT_IMAGE_EXTENSION));
    }
  }

  bool ImageReaderWriter::ReadImage(unsigned int index)
  {
    if (index >= numImages) {
      std::cerr << "Index out of range for the specified image type." << std::endl;
      return false;
    }
    std::filesystem::path filePath = inputFiles[index];
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "Failed to open image file: " << filePath.filename() << std::endl;
      return false;
    }
    if (type == ImageType::BAYER) {
      inFrame.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC1);
    } else if (type == ImageType::YUYV) {
      inFrame.create(CameraHelpers::IMAGE_HEIGHT, CameraHelpers::IMAGE_WIDTH, CV_8UC2);
    } else {
      std::cerr << "Unsupported image type" << std::endl;
      file.close();
      return false;
    }
    file.read(reinterpret_cast<char*>(inFrame.data), inFrame.total() * inFrame.elemSize());
    if (!file) {
      std::cerr << "Failed to read image data from file: " << filePath.filename() << std::endl;
      file.close();
      return false;
    }
    file.close();
    loadedIndex = index;
    return true;
  }

  bool ImageReaderWriter::WriteImage(unsigned int index)
  {
    if (index >= numImages) {
      std::cerr << "Index out of range for the specified image type." << std::endl;
      return false;
    }
    std::filesystem::path filePath = outputFiles[index];
    if (!cv::imwrite(filePath, outFrame)) {
      std::cerr << "Failed to write image to file: " << filePath.filename() << std::endl;
      return false;
    }
    return true;
  }

  ImageReaderWriter::Iterator::Iterator(ImageReaderWriter& rw, unsigned int index)
    : readerWriter(rw), currentIndex(index)
  {
    if (currentIndex < readerWriter.numImages) {
      if (!readerWriter.ReadImage(currentIndex)) {
        std::cerr << "Failed to read image at index " << currentIndex << std::endl;
        currentIndex = readerWriter.numImages;
      }
    }
  }

  ImageReaderWriter::Iterator& ImageReaderWriter::Iterator::operator++()
  {
    if (!readerWriter.WriteImage(currentIndex)) {
      std::cerr << "Failed to write image at index " << (currentIndex) << std::endl;
    }
    ++currentIndex;
    if (currentIndex < readerWriter.numImages) {
      if (!readerWriter.ReadImage(currentIndex)) {
        std::cerr << "Failed to read image at index " << currentIndex << std::endl;
        currentIndex = readerWriter.numImages;
      }
    }
    return *this;
  }

  bool ImageReaderWriter::Iterator::operator==(const Iterator& other) const { return currentIndex == other.currentIndex; }
  bool ImageReaderWriter::Iterator::operator!=(const Iterator& other) const { return !(*this == other); }
  unsigned int ImageReaderWriter::Iterator::operator*() const { return currentIndex; }

  ImageReaderWriter::Iterator ImageReaderWriter::begin() { return Iterator(*this, 0); }
  ImageReaderWriter::Iterator ImageReaderWriter::end() { return Iterator(*this, numImages); }
} // namespace ImageHelpers