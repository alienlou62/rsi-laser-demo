#ifndef CAMERA_HELPERS_H
#define CAMERA_HELPERS_H

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <sstream>
#include <stdexcept>
#include <string>

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

private:
    // Internal implementation: returns true on success, false on failure, sets errorMsg if not null
    static bool GrabFrameCore(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs, std::string *errorMsg = nullptr);
    // Internal implementation: returns true on success, false on failure, sets errorMsg if not null
    static bool PrimeCameraCore(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries, std::string *errorMsg = nullptr);
};


// ----------- Implementation -----------

void CameraHelpers::ConfigureCamera(Pylon::CInstantCamera &camera)
{
  camera.Attach(Pylon::CTlFactory::GetInstance().CreateFirstDevice());
  std::cout << "Using device: " << camera.GetDeviceInfo().GetModelName() << std::endl;
  camera.Open();

  Pylon::CFeaturePersistence::Load(CONFIG_FILE, &camera.GetNodeMap());
  GenApi::INodeMap &nodemap = camera.GetNodeMap();
}

bool CameraHelpers::GrabFrameCore(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs, std::string *errorMsg)
{
    bool retrieveSucceeded = false;
    try
    {
        retrieveSucceeded = camera.RetrieveResult(timeoutMs, grabResult, Pylon::TimeoutHandling_Return);
    }
    catch (const Pylon::GenericException &e)
    {
        if (errorMsg) *errorMsg = "Exception during frame grab: " + std::string(e.GetDescription());
        return false;
    }
    
    if (!retrieveSucceeded)
    {
        if (errorMsg) *errorMsg = "Failed to retrieve result within timeout.";
        return false;
    }

    // If retrieveSucceeded, then grabResult shouldn't be null, but let's check to be safe
    if (!grabResult || !grabResult->GrabSucceeded())
    {
        if (errorMsg)
        {
            std::ostringstream oss;
            oss << "Camera grab failed: ";
            if (!grabResult)
                // This should never be reached since we checked retrieveSucceeded
                oss << "Result pointer is null.";
            else
                // If we have a grabResult but it failed for some other reason (not a timeout)
                oss << "Code " << grabResult->GetErrorCode()
                    << ", Desc: " << grabResult->GetErrorDescription();
            *errorMsg = oss.str();
        }
        return false;
    }
    return true;
}

void CameraHelpers::GrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs)
{
    std::string errorMsg;
    if (!GrabFrameCore(camera, grabResult, timeoutMs, &errorMsg))
        throw std::runtime_error(errorMsg);
}

bool CameraHelpers::TryGrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs, std::string *errorOut)
{
    return GrabFrameCore(camera, grabResult, timeoutMs, errorOut);
}

bool CameraHelpers::PrimeCameraCore(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries, std::string *errorMsg)
{
    camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);
    std::ostringstream errorStream;
    std::string grabError;
    uint retries = 0;
    while (retries < maxRetries)
    {
        if (GrabFrameCore(camera, grabResult, TIMEOUT_MS, errorMsg ? &grabError : nullptr))
            return true;

        if (errorMsg) errorStream << grabError << std::endl;
        retries++;
    }
    if (errorMsg) *errorMsg = "Failed to grab a frame during priming after " +
               std::to_string(maxRetries) + " retries. " + errorStream.str();
    return false;
}

void CameraHelpers::PrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries)
{
    std::string errorMsg;
    if (!PrimeCameraCore(camera, grabResult, maxRetries, &errorMsg))
        throw std::runtime_error(errorMsg);
}

bool CameraHelpers::TryPrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries, std::string *errorOut)
{
    return PrimeCameraCore(camera, grabResult, maxRetries, errorOut);
}

#endif // CAMERA_HELPERS_H
