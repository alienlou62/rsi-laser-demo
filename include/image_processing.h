#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <cstdint> // For uint8_t

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

    static bool TryProcessImage(const uint8_t* pImageBuffer, int width, int height, double& targetX, double& targetY);
};

#endif // IMAGE_PROCESSING_H