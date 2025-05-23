#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

class ImageProcessing
{
public:
    static void ProcessImage(const Pylon::CGrabResultPtr& grabResult, double& targetX, double& targetY);

    static bool TryProcessImage(const Pylon::CGrabResultPtr& grabResult, double& targetX, double& targetY, std::string* errorMsg = nullptr);

    static void ConvertToRGB(const Pylon::CGrabResultPtr& grabResult, cv::Mat& rgbFrame);

    static bool TryConvertToRGB(const Pylon::CGrabResultPtr& grabResult, cv::Mat& rgbFrame, std::string* errorMsg = nullptr);
};

#endif // IMAGE_PROCESSING_H