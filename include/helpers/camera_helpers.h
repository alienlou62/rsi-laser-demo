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

namespace CameraHelpers {
  // Camera constants
  inline constexpr unsigned int IMAGE_WIDTH = 640;
  inline constexpr unsigned int IMAGE_HEIGHT = 480;

  inline constexpr double PIXEL_SIZE = 4.8e-3; //mm
  inline constexpr double FOCAL_LENGTH = 4.09; //mm
  inline constexpr double RADIANS_PER_PIXEL = 2.0 * std::atan(PIXEL_SIZE / (2.0 * FOCAL_LENGTH));

  // Image capture constants
  inline constexpr unsigned int TIMEOUT_MS = 1000;
  inline constexpr unsigned int MAX_RETRIES = 10;

  // Configure the camera with predefined settings
  void ConfigureCamera(Pylon::CInstantCamera &camera);

  // Try to grab a frame. Returns true on success, false on timeout or incomplete grab. Throws only for fatal/unrecoverable errors.
  bool TryGrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, unsigned int timeoutMs = TIMEOUT_MS);

  // Throws std::runtime_error if fails after maxRetries
  void PrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, unsigned int maxRetries = MAX_RETRIES);
}

#endif // CAMERA_HELPERS_H
