#include "camera_helpers.h"
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/PylonIncludes.h>
#include <stdexcept>
#include <string>

using namespace Pylon;
using namespace GenApi;

// ----------- Implementation -----------
namespace CameraHelpers {

  void ConfigureCamera(CInstantCamera &camera)
  {
    try
    {
      camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
      std::cout << "Using device: " << camera.GetDeviceInfo().GetModelName() << std::endl;
      camera.Open();

      CFeaturePersistence::Load(CONFIG_FILE, &camera.GetNodeMap());
      INodeMap &nodeMap = camera.GetNodeMap();
    }
    catch (const GenericException &e)
    {
      throw std::runtime_error(std::string("[CameraHelpers] Pylon exception during camera configuration: ") + e.GetDescription());
    }
    catch (const std::exception &e)
    {
      throw std::runtime_error(std::string("[CameraHelpers] std::exception during camera configuration: ") + e.what());
    }
    catch (...)
    {
      throw std::runtime_error("[CameraHelpers] Unknown exception during camera configuration.");
    }
  }

  bool TryGrabFrame(CInstantCamera &camera, CGrabResultPtr &grabResult, unsigned int timeoutMs)
  {
    try
    {
      if (!camera.RetrieveResult(timeoutMs, grabResult, TimeoutHandling_Return))
      {
        return false; // Timeout
      }
    }
    catch (const GenericException &e)
    {
      throw std::runtime_error(std::string("[CameraHelpers] Exception during frame grab: ") + e.what());
    }
    catch (const std::exception &e)
    {
      throw std::runtime_error(std::string("[CameraHelpers] std::exception during frame grab: ") + e.what());
    }
    catch (...)
    {
      throw std::runtime_error("[CameraHelpers] Unknown exception during frame grab.");
    }

    if (!grabResult)
    {
      throw std::runtime_error("[CameraHelpers] Fatal: Grab failed, result pointer is null after RetrieveResult (unexpected state).");
    }

    if (!grabResult->GrabSucceeded())
    {
      const int64_t errorCode = grabResult->GetErrorCode();
      if (errorCode == 0xe1000014) // Incomplete buffer (buffer underrun). Not considered a fatal error.
      {
        return false;
      }
      std::ostringstream oss;
      oss << "[CameraHelpers] Grab failed: Code " << errorCode << ", Desc: " << grabResult->GetErrorDescription();
      throw std::runtime_error(oss.str());
    }
    return true;
  }

  void PrimeCamera(CInstantCamera &camera, CGrabResultPtr &grabResult, unsigned int maxRetries)
  {

    unsigned int retries = 0;
    while (retries < maxRetries)
    {
      try {
        camera.Open();
        camera.StartGrabbing(GrabStrategy_LatestImageOnly);
        if (TryGrabFrame(camera, grabResult))
          return;

        camera.Close();
      } catch (const std::exception& e) {
        throw std::runtime_error(std::string("[CameraHelpers] Fatal error during camera priming: ") + e.what());
      }
      retries++;
    }
    throw std::runtime_error("[CameraHelpers] Failed to grab a frame during priming after " + std::to_string(maxRetries) + " retries.");
  }
} // namespace CameraHelpers