#include <iostream>
#include <csignal>
#include <cmath>
#include <chrono>

// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <pylon/PylonIncludes.h>

// --- RMP/RSI Headers ---
#include "rsi.h"

#include "rmp_helpers.h"
#include "camera_helpers.h"
#include "image_processing.h"
#include "timing_helpers.h"
#include "misc_helpers.h"

using namespace RSI::RapidCode;

int grabFailures = 0;
int processFailures = 0;

// --- Global Variables ---
Pylon::PylonAutoInitTerm g_pylonAutoInitTerm = Pylon::PylonAutoInitTerm();
Pylon::CInstantCamera g_camera;
Pylon::CGrabResultPtr g_ptrGrabResult;

MotionController* g_controller(nullptr);
MultiAxis* g_multiAxis(nullptr);
double g_offsetX = 0.0;
double g_offsetY = 0.0;

volatile sig_atomic_t g_shutdown = false;
void sigquit_handler(int signal)
{
    std::cout << "SIGQUIT handler ran, setting shutdown flag..." << std::endl;
    g_shutdown = true;
}

volatile sig_atomic_t g_paused = false;
void sigint_handler(int signal)
{
    std::cout << "SIGINT handler ran, toggling paused flag..." << std::endl;
    g_paused = !g_paused;
}

bool HandlePaused() {
    bool axisStopped = g_multiAxis->StateGet() == RSIState::RSIStateSTOPPED ||
                      g_multiAxis->StateGet() == RSIState::RSIStateSTOPPING;
    if (g_paused && !axisStopped)
    {
        g_multiAxis->Stop();
    }
    else if (!g_paused && axisStopped)
    {
        g_multiAxis->ClearFaults();
    }
    return g_paused;
}

void MoveMotorsWithLimits()
{
    static const double POSITION_DEAD_ZONE = 0.0005;
    static const double PIXEL_DEAD_ZONE = 40.0;
    static const double MAX_OFFSET = 320.0; // max pixel offset
    static const double MIN_VELOCITY = 0.0001;
    static const double MAX_VELOCITY = 0.2; // max velocity
    static const double jerkPct = 30.0; // Smoother than 50%

    // --- Dead zone logic ---
    if (std::abs(g_offsetX) < PIXEL_DEAD_ZONE && std::abs(g_offsetY) < PIXEL_DEAD_ZONE)
    {
        g_multiAxis->MoveVelocity(std::array{0.0, 0.0}.data());
        return;
    }

    try
    {
        // --- Normalize pixel offsets to [-1, 1] ---
        double normX = std::clamp(g_offsetX / MAX_OFFSET, -1.0, 1.0);
        double normY = std::clamp(g_offsetY / MAX_OFFSET, -1.0, 1.0);

        // --- Exponential scaling to reduce velocity when near center ---
        double velX = -std::copysign(1.0, normX) * std::pow(std::abs(normX), 1.5) * MAX_VELOCITY;
        double velY = -std::copysign(1.0, normY) * std::pow(std::abs(normY), 1.5) * MAX_VELOCITY;

        // --- Avoid tiny jitter motions ---
        if (std::abs(velX) < MIN_VELOCITY)
            velX = 0.0;
        if (std::abs(velY) < MIN_VELOCITY)
            velY = 0.0;

        // --- Velocity mode only (no MoveSCurve to position) ---
        g_multiAxis->MoveVelocity(std::array{velX, velY}.data());
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error during velocity control: " << ex.what() << std::endl;
        if (g_multiAxis)
            g_multiAxis->Abort();
    }
}

// --- Main Function ---
int main()
{
    const std::chrono::milliseconds loopInterval(5); // 5ms loop interval
    const std::string EXECUTABLE_NAME = "Pylon_RSI_Tracking_BayerOnly";
    PrintHeader(EXECUTABLE_NAME);
    int exitCode = 0;

    std::signal(SIGQUIT, sigquit_handler);
    std::signal(SIGINT, sigint_handler);

    // --- Pylon Initialization & Camera Loop ---
    CameraHelpers::ConfigureCamera(g_camera);
    CameraHelpers::PrimeCamera(g_camera, g_ptrGrabResult);

    // --- RMP Initialization ---
    g_controller = RMPHelpers::GetController();
    g_multiAxis = RMPHelpers::CreateMultiAxis(g_controller);
    g_multiAxis->AmpEnableSet(true);

    TimingStats loopTiming, retrieveTiming, processingTiming, motionTiming;
    try
    {
        // --- Main Loop ---
        while (!g_shutdown && g_camera.IsGrabbing())
        {
            ScopedRateLimiter rateLimiter(loopInterval);
            auto loopStopwatch = ScopedStopwatch(loopTiming);

            {
                auto retrieveStopwatch = ScopedStopwatch(retrieveTiming);
                std::string grabError;
                if (!CameraHelpers::TryGrabFrame(g_camera, g_ptrGrabResult, CameraHelpers::TIMEOUT_MS, &grabError))
                {
                    std::cerr << grabError << std::endl;
                    ++grabFailures;
                    continue;
                }
            }

            {
                auto processingStopwatch = ScopedStopwatch(processingTiming);
                std::string processError;
                if (!ImageProcessing::TryProcessImage(g_ptrGrabResult, g_offsetX, g_offsetY, &processError))
                {
                    std::cerr << processError << std::endl;
                    ++processFailures;
                    continue;
                }
            }

            // Handle paused state and continue if paused
            if (HandlePaused()) continue;

            {
                auto motionStopwatch = ScopedStopwatch(motionTiming);
                try
                {
                    MoveMotorsWithLimits();
                }
                catch (const RsiError& e)
                {
                    std::cerr << "RMP exception during motion control: " << e.what() << std::endl;
                    g_shutdown = true;
                }
            }
        }
    }
    catch(const cv::Exception& e)
    {
        std::cerr << "OpenCV exception: " << e.what() << std::endl;
        exitCode = 1;
    }
    catch(const Pylon::GenericException& e)
    {
        std::cerr << "Pylon exception: " << e.GetDescription() << std::endl;
        exitCode = 1;
    }
    catch(const RsiError& e)
    {
        std::cerr << "RMP exception: " << e.what() << std::endl;
        exitCode = 1;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        exitCode = 1;
    }
    catch(...)
    {
        std::cerr << "Unknown exception occurred." << std::endl;
        exitCode = 1;
    }

    // Cleanup
    g_multiAxis->Abort();
    g_multiAxis->ClearFaults();

    // Print loop statistics
    printStats("Loop", loopTiming);
    printStats("Retrieve", retrieveTiming);
    printStats("Processing", processingTiming);
    printStats("Motion", motionTiming);

    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "Grab Failures:     " << grabFailures << std::endl;
    std::cout << "Process Failures:  " << processFailures << std::endl;

    cv::destroyAllWindows();

    PrintFooter(EXECUTABLE_NAME, exitCode);

    return exitCode;
}