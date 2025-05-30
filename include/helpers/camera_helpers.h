#ifndef CAMERA_HELPERS_H
#define CAMERA_HELPERS_H

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

    // Try to grab a frame. Returns true on success, false on timeout or incomplete grab. Throws only for fatal/unrecoverable errors.
    static bool TryGrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs = TIMEOUT_MS);

    // Throws std::runtime_error if fails after maxRetries
    static void PrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries = MAX_RETRIES);
};

#endif // CAMERA_HELPERS_H
