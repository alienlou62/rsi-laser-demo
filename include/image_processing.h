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
    static constexpr double MIN_CIRCLE_RADIUS = 1.0;
    static constexpr double MIN_CONTOUR_AREA = 700.0;

    static constexpr int lowH = 105, highH = 126, lowS = 0, highS = 255, lowV = 35, highV = 190;

    // If the target is within this pixel tolerance of the center, then we consider the offset to be zero
    static constexpr unsigned int PIXEL_TOLERANCE = 10;

    static bool TryDetectBall(const uint8_t* pImageBuffer, int width, int height, double& offsetX, double& offsetY);
};

#endif // IMAGE_PROCESSING_H