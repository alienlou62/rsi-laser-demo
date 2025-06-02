#ifndef CAMERA_HELPERS_H
#define CAMERA_HELPERS_H

#include <cmath>

// Forward declarations for Pylon types
namespace Pylon {
    class CInstantCamera;
    class CGrabResultPtr;
}

#ifndef CONFIG_FILE
#define CONFIG_FILE ""
#endif

class CameraHelpers
{
public:
    // Camera constants
    static constexpr unsigned int IMAGE_WIDTH = 640;
    static constexpr unsigned int IMAGE_HEIGHT = 480;

    static constexpr double PIXEL_SIZE = 4.8e-3; //mm
    static constexpr double FOCAL_LENGTH = 4.0; //mm
    static constexpr double RadiansPerPixel() // radians per pixel
    {
        return 2.0 * std::atan(PIXEL_SIZE / (2.0 * FOCAL_LENGTH));
    }

    // Image capture constants
    static constexpr unsigned int TIMEOUT_MS = 1000;
    static constexpr unsigned int MAX_RETRIES = 10;

    // Configure the camera with predefined settings
    static void ConfigureCamera(Pylon::CInstantCamera &camera);

    // Try to grab a frame. Returns true on success, false on timeout or incomplete grab. Throws only for fatal/unrecoverable errors.
    static bool TryGrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, unsigned int timeoutMs = TIMEOUT_MS);

    // Throws std::runtime_error if fails after maxRetries
    static void PrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, unsigned int maxRetries = MAX_RETRIES);
};

#endif // CAMERA_HELPERS_H
