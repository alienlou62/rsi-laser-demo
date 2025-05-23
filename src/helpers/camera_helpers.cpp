#include "camera_helpers.h"
#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <sstream>
#include <stdexcept>
#include <string>

// ----------- Implementation -----------

void CameraHelpers::ConfigureCamera(Pylon::CInstantCamera &camera)
{
    try
    {
        camera.Attach(Pylon::CTlFactory::GetInstance().CreateFirstDevice());
        std::cout << "Using device: " << camera.GetDeviceInfo().GetModelName() << std::endl;
        camera.Open();

        Pylon::CFeaturePersistence::Load(CONFIG_FILE, &camera.GetNodeMap());
        GenApi::INodeMap &nodemap = camera.GetNodeMap();
    }
    catch (const Pylon::GenericException &e)
    {
        std::cerr << "[CameraHelpers] Exception during camera configuration: " << e.GetDescription() << '\n';
        throw;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[CameraHelpers] Standard exception during camera configuration: " << e.what() << '\n';
        throw;
    }
    catch (...)
    {
        std::cerr << "[CameraHelpers] Unknown exception during camera configuration.\n";
        throw;
    }
}

void CameraHelpers::GrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs)
{
    std::string errorMsg;
    if (!TryGrabFrame(camera, grabResult, timeoutMs, &errorMsg))
        throw std::runtime_error(errorMsg);
}

bool CameraHelpers::TryGrabFrame(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint timeoutMs, std::string *errorMsg)
{
    try
    {
        if (!camera.RetrieveResult(timeoutMs, grabResult, Pylon::TimeoutHandling_Return)) {
            if (errorMsg) *errorMsg = "[CameraHelpers] Timeout: No frame received within " + std::to_string(timeoutMs) + " ms.";
            return false;
        }
    }
    catch (const Pylon::GenericException &e)
    {
        if (errorMsg) *errorMsg = std::string("[CameraHelpers] Exception during frame grab: ") + e.GetDescription();
        return false;
    }

    if (!grabResult)
    {
        if (errorMsg) *errorMsg = "[CameraHelpers] Grab failed: Result pointer is null after RetrieveResult.";
        return false;
    }

    if (!grabResult->GrabSucceeded())
    {
        if (errorMsg)
        {
            std::ostringstream oss;
            oss << "[CameraHelpers] Grab failed: Code " << grabResult->GetErrorCode()
                << ", Desc: " << grabResult->GetErrorDescription();
            *errorMsg = oss.str();
        }
        return false;
    }
    return true;
}

void CameraHelpers::PrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries)
{
    std::string errorMsg;
    if (!TryPrimeCamera(camera, grabResult, maxRetries, &errorMsg))
        throw std::runtime_error(errorMsg);
}

bool CameraHelpers::TryPrimeCamera(Pylon::CInstantCamera &camera, Pylon::CGrabResultPtr &grabResult, uint maxRetries, std::string *errorMsg)
{
    camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);
    std::ostringstream errorStream;
    std::string grabError;
    uint retries = 0;
    while (retries < maxRetries)
    {
        if (TryGrabFrame(camera, grabResult, TIMEOUT_MS, errorMsg ? &grabError : nullptr))
            return true;
        if (errorMsg) errorStream << "[PrimeCamera attempt " << (retries+1) << "/" << maxRetries << "] " << grabError << std::endl;
        retries++;
    }
    if (errorMsg) *errorMsg = "[CameraHelpers] Failed to grab a frame during priming after " +
               std::to_string(maxRetries) + " retries.\n" + errorStream.str();
    return false;
}
