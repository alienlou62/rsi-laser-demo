#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <string>

// Forward declarations for cv::Mat and Pylon::CGrabResultPtr
namespace cv { class Mat; }
namespace Pylon { class CGrabResultPtr; }

class ImageProcessing
{
public:
    static void ProcessImage(const Pylon::CGrabResultPtr& grabResult, double& targetX, double& targetY);

    static bool TryProcessImage(const Pylon::CGrabResultPtr& grabResult, double& targetX, double& targetY, std::string* errorMsg = nullptr);

    static void ConvertToRGB(const Pylon::CGrabResultPtr& grabResult, cv::Mat& rgbFrame);

    static bool TryConvertToRGB(const Pylon::CGrabResultPtr& grabResult, cv::Mat& rgbFrame, std::string* errorMsg = nullptr);
};

#endif // IMAGE_PROCESSING_H