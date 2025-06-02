#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

// Forward declarations for cv::Mat and Pylon::CGrabResultPtr
namespace cv { class Mat; }
namespace Pylon { class CGrabResultPtr; }

class ImageProcessing
{
public:
    // Constants for image processing
    static constexpr double CENTER_X = 320;
    static constexpr double CENTER_Y = 240;
    static constexpr double MIN_CIRCLE_RADIUS = 1.0;
    static constexpr int lowH = 105, highH = 126, lowS = 0, highS = 255, lowV = 35, highV = 190;
    static constexpr double MIN_CONTOUR_AREA = 700.0;

    static bool TryProcessImage(const Pylon::CGrabResultPtr& grabResult, double& targetX, double& targetY);

    static void ConvertToRGB(const Pylon::CGrabResultPtr& grabResult, cv::Mat& rgbFrame);
};

#endif // IMAGE_PROCESSING_H