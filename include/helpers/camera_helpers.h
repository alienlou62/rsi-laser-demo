#ifndef CAMERA_HELPERS_H
#define CAMERA_HELPERS_H

#include <string>

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
    static constexpr uint TIMEOUT_MS = 1000;
    static constexpr uint MAX_RETRIES = 10;

    // Configure the camera with predefined settings
    static void ConfigureCamera(Pylon::CInstantCamera &camera);

    // Throws std::runtime_error on error
    static void GrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs = TIMEOUT_MS);

    // Returns false and sets errorOut on failure, does NOT throw for camera/grab errors
    static bool TryGrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs = TIMEOUT_MS, std::string *errorOut = nullptr);

    // Throws std::runtime_error if fails after maxRetries
    static void PrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries = MAX_RETRIES);

    // Returns false and sets errorOut on failure, does NOT throw for camera/grab errors
    static bool TryPrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries = MAX_RETRIES, std::string *errorOut = nullptr);
};


#endif // CAMERA_HELPERS_H
