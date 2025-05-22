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
#include "timing_helpers.h"
#include "misc_helpers.h"

using namespace RSI::RapidCode;

// --- Constants ---
// Motor Control Constants from "older code"
const double MAX_VELOCITY = 0.2;
const double ACCELERATION = 2.0;
const double DECELERATION = 2.0;
const double CENTER_X = 320; // HARDCODED Center X for motor offset calculation
const double CENTER_Y = 240; // HARDCODED Center Y for motor offset calculation
const double PROPORTIONAL_GAIN = 1.0;
const double VELOCITY_SCALING = 0.002;
const double MIN_POSITION = -0.05;
const double MAX_POSITION = 0.05;
const double DEAD_ZONE = 40.0; // Dead zone in PIXELS for motor offset
// OpenCV processing constants
const double MIN_CONTOUR_AREA = 10.0;
const double MIN_CIRCLE_RADIUS = 1.0;

// HSV Thresholds (initial guesses)
const int lowH = 105;  // Lower Hue
const int highH = 126; // Upper Hue
const int lowS = 0;    // Lower Saturation
const int highS = 255; // Upper Saturation
const int lowV = 35;   // Lower Value
const int highV = 190; // Upper Value

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
    static const double PIXEL_DEAD_ZONE = DEAD_ZONE;
    static const double MAX_OFFSET = 320.0; // max pixel offset
    static const double MIN_VELOCITY = 0.0001;
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

void RMPCleanup()
{
    try
    {
        if (g_multiAxis)
        {
            g_multiAxis->Abort();
            g_multiAxis->ClearFaults();
        }

        if (g_controller)
        {
            try
            {
                RMPHelpers::ShutdownTheNetwork(g_controller);
            }
            catch(const RSI::RapidCode::RsiError& e)
            {
                std::cerr << "RMP exception during network shutdown: " << e.what() << std::endl;
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            g_controller->Shutdown();
            g_controller->Delete();
        }
    }
    catch(const RsiError& e)
    {
        std::cerr << "RMP exception during shutdown: " << e.what() << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

// --- Image Processing Function ---
bool ConvertToRGB(cv::Mat &outRgbFrame, std::string *errorMsg = nullptr)
{
    if (!g_ptrGrabResult)
    {
        if (errorMsg) *errorMsg = "Error: Grab result is null!";
        return false;
    }

    int width = g_ptrGrabResult->GetWidth();
    int height = g_ptrGrabResult->GetHeight();
    const uint8_t *pImageBuffer = static_cast<uint8_t *>(g_ptrGrabResult->GetBuffer());

    if (!pImageBuffer)
    {
        if (errorMsg) *errorMsg = "Error: Image buffer is null!";
        return false;
    }
    if (width <= 0 || height <= 0)
    {
        if (errorMsg) *errorMsg = "Error: Invalid image dimensions! Width: " + std::to_string(width) + ", Height: " + std::to_string(height);
        return false;
    }

    cv::Mat bayerFrame(height, width, CV_8UC1, (void *)pImageBuffer);
    if (bayerFrame.empty())
    {
        if (errorMsg) *errorMsg = "Error: Retrieved empty bayer frame!";
        return false;
    }

    try
    {
        // Convert Bayer to RGB
        cv::cvtColor(bayerFrame, outRgbFrame, cv::COLOR_BayerBG2RGB);

        if (outRgbFrame.empty())
        {
            if (errorMsg) *errorMsg = "Error: Bayer-to-RGB conversion produced empty frame!";
            return false;
        }

        return true; // Successfully converted
    }
    catch (const cv::Exception &e)
    {
        if (errorMsg) *errorMsg = std::string("OpenCV exception during Bayer-to-RGB conversion: ") + e.what();
        return false;
    }
    catch (const std::exception &e)
    {
        if (errorMsg) *errorMsg = std::string("Standard exception during Bayer-to-RGB conversion: ") + e.what();
        return false;
    }
    catch (...)
    {
        if (errorMsg) *errorMsg = "Unknown error during Bayer-to-RGB conversion.";
        return false;
    }
    return false; // Fallback in case of failure
}

bool ProcessFrame(std::string *errorMsg = nullptr)
{
    cv::Mat rgbFrame;
    if (!ConvertToRGB(rgbFrame, errorMsg))
    {
        if (errorMsg && errorMsg->empty()) *errorMsg = "Error: Failed to convert frame to RGB!";
        return false;
    }

    static bool target_found_last_frame = false;
    bool target_currently_found = false;
    bool result = false;

    // OpenCV Processing
    cv::Mat hsvFrame;
    cv::cvtColor(rgbFrame, hsvFrame, cv::COLOR_RGB2HSV);

    cv::Scalar lower_ball(lowH, lowS, lowV);
    cv::Scalar upper_ball(highH, highS, highV);

    cv::Mat mask;
    cv::inRange(hsvFrame, lower_ball, upper_ball, mask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    contours.erase(
        std::remove_if(contours.begin(), contours.end(),
                       [](const std::vector<cv::Point> &c)
                       {
                           return cv::contourArea(c) < 700.0;
                       }),
        contours.end());

    int largestContourIndex = -1;
    double largestContourArea = 0;

    if (contours.empty())
    {
        if (errorMsg) *errorMsg = "Error: No contours found.";
        return false;
    }
    else
    {
        for (int i = 0; i < contours.size(); i++)
        {
            double area = cv::contourArea(contours[i]);
            // Optionally add debug info to errorMsg if needed
            if (area > largestContourArea)
            {
                largestContourArea = area;
                largestContourIndex = i;
            }
        }
    }

    if (largestContourIndex != -1)
    {
        cv::Point2f center;
        float radius;
        cv::minEnclosingCircle(contours[largestContourIndex], center, radius);
        if (radius > MIN_CIRCLE_RADIUS)
        {
            target_currently_found = true;
            target_found_last_frame = true;
            cv::circle(rgbFrame, center, (int)radius, cv::Scalar(0, 255, 0), 2);
            cv::circle(rgbFrame, center, 5, cv::Scalar(0, 255, 0), -1);
            cv::circle(rgbFrame, cv::Point(CENTER_X, CENTER_Y), DEAD_ZONE, cv::Scalar(0, 0, 255), 1);

            g_offsetX = center.x - CENTER_X;
            g_offsetY = center.y - CENTER_Y;
        }
        else
        {
            if (errorMsg) *errorMsg = "Error: Detected contour radius below minimum.";
            return false;
        }
    }
    else
    {
        if (errorMsg) *errorMsg = "Error: No valid contour found.";
        return false;
    }

    if (!target_currently_found && target_found_last_frame)
    {
        g_offsetX = 0;
        g_offsetY = 0;
        target_found_last_frame = false;
    }
    result = true;

    cv::imshow("RGB Frame", rgbFrame);
    cv::imshow("Red Mask", mask);
    cv::waitKey(1);
    return result;
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
    g_controller = RMPHelpers::CreateController();
    g_multiAxis = RMPHelpers::ConfigureAxes(g_controller);

    TimingStats loopTiming, retrieveTiming, processingTiming, motionTiming;
    try
    {
        RMPHelpers::StartTheNetwork(g_controller);
        g_multiAxis->AmpEnableSet(true);

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
                if (!ProcessFrame(&processError))
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
                MoveMotorsWithLimits();
            }
        }
    }
    catch(const cv::Exception& e)
    {
        std::cerr << "OpenCV exception: " << e.what() << std::endl;
    }
    catch(const Pylon::GenericException& e)
    {
        std::cerr << "Pylon exception: " << e.GetDescription() << std::endl;
    }
    catch(const RsiError& e)
    {
        std::cerr << "RMP exception: " << e.what() << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    catch(...)
    {
        std::cerr << "Unknown exception occurred." << std::endl;
    }

    // Cleanup
    RMPCleanup();

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