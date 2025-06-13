#ifndef IMAGE_HELPERS_H
#define IMAGE_HELPERS_H

#include <opencv2/opencv.hpp>
#include <filesystem>
#include <vector>
#include <string>

namespace ImageHelpers {
  // Enum for image type
  enum class ImageType {
    BAYER,
    YUYV
  };

  inline constexpr const char* INPUT_FOLDER = SOURCE_DIR "test_img_input/";
  inline constexpr const char* OUTPUT_FOLDER = SOURCE_DIR "test_img_output/";
  inline constexpr const char* BAYER_FOLDER = "bayer/";
  inline constexpr const char* YUYV_FOLDER = "yuyv/";
  inline constexpr const char* INPUT_IMAGE_EXTENSION = ".raw";
  inline constexpr const char* OUTPUT_IMAGE_EXTENSION = ".png";

  class ImageReaderWriter {
  public:
    ImageReaderWriter(const ImageType type, cv::Mat& inFrame, cv::Mat& outFrame);
    bool ReadImage(unsigned int index);
    bool WriteImage(unsigned int index);

    class Iterator {
      ImageReaderWriter& readerWriter;
      unsigned int currentIndex;
    public:
      using iterator_category = std::input_iterator_tag;
      using value_type = unsigned int;
      using difference_type = std::ptrdiff_t;
      using pointer = unsigned int*;
      using reference = unsigned int&;
      Iterator(ImageReaderWriter& rw, unsigned int index);
      Iterator& operator++();
      bool operator==(const Iterator& other) const;
      bool operator!=(const Iterator& other) const;
      unsigned int operator*() const;
    };
    Iterator begin();
    Iterator end();

  private:
    const ImageType type;
    std::vector<std::filesystem::path> inputFiles;
    cv::Mat& inFrame;
    std::vector<std::filesystem::path> outputFiles;
    cv::Mat& outFrame;
    unsigned int numImages;
    int loadedIndex = -1;
  };
}

#endif // IMAGE_HELPERS_H
