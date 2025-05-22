#include <opencv2/opencv.hpp>
#include <iostream>
#include <pylon/PylonIncludes.h>                // Pylon Core
#include <pylon/PixelType.h>                    // Pylon Pixel Types (Defines EPixelType etc)
#include <pylon/BaslerUniversalInstantCamera.h> // Needed for nodemap access via CInstantCamera
#include <csignal>                              // For signal handling
#include <cmath>                                // For std::abs, std::clamp, std::min, std::max
#include <thread>                               // For std::this_thread::sleep_for
#include <chrono>                               // For std::chrono::milliseconds

#ifndef CONFIG_FILE
#define CONFIG_FILE ""
#endif

// --- RMP/RSI Headers ---
// !! REMINDER: Compile using the RMP SDK's build system (CMake) to resolve RMP_DEFAULT_PATH !!
#include "SampleAppsHelper.h"
#include "rsi.h"

#include "Timing.h" // For Stopwatch class

// Namespaces
using namespace RSI::RapidCode;
using namespace Pylon;
using namespace GenApi; // Needed for Node Map access
using namespace std;
using namespace cv;

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

// Priming Constants
const int TIMEOUT_MS = 500;
const int MAX_RETRIES = 10;

int grabFailures = 0;
int processFailures = 0;

// --- Global Variables ---
PylonAutoInitTerm g_pylonAutoInitTerm = PylonAutoInitTerm();
CInstantCamera g_camera;
CGrabResultPtr g_ptrGrabResult;

shared_ptr<MotionController> g_controller(nullptr);
shared_ptr<MultiAxis> g_multiAxis(nullptr);
double g_offsetX = 0.0;
double g_offsetY = 0.0;

// --- Create the motion controller ---
shared_ptr<MotionController> CreateController()
{
    MotionController::CreationParameters createParams;
    createParams.CpuAffinity = 3;
    strncpy(createParams.RmpPath, "/rsi/", createParams.PathLengthMaximum);
    strncpy(createParams.NicPrimary, "enp3s0", createParams.PathLengthMaximum); // *** VERIFY ***

    shared_ptr<MotionController> controller(MotionController::Create(&createParams),
                                            [](MotionController *controller)
                                            {
                                                controller->Delete();
                                                controller = nullptr;
                                            });

    SampleAppsHelper::CheckErrors(controller.get());
    printf("RSI Controller Created!\n");

    return controller;
}

shared_ptr<MultiAxis> ConfigureAxes(shared_ptr<MotionController> controller)
{
    constexpr int NUM_AXES = 2; // Number of axes to configure
    constexpr int USER_UNITS = 67108864;

    // Add a motion supervisor for the multiaxis
    controller->AxisCountSet(NUM_AXES + 1);
    shared_ptr<MultiAxis> multiAxis(controller->MultiAxisGet(NUM_AXES),
                                    [](MultiAxis *multiAxis)
                                    {
                                        multiAxis->Abort();
                                        multiAxis->ClearFaults();
                                        multiAxis = nullptr;
                                    });
    SampleAppsHelper::CheckErrors(multiAxis.get());

    for (int i = 0; i < NUM_AXES; i++)
    {
        Axis *axis = controller->AxisGet(i);
        SampleAppsHelper::CheckErrors(axis);
        axis->UserUnitsSet(USER_UNITS);
        axis->ErrorLimitActionSet(RSIAction::RSIActionNONE);
        axis->HardwarePosLimitActionSet(RSIAction::RSIActionNONE);
        axis->HardwareNegLimitActionSet(RSIAction::RSIActionNONE);
        axis->PositionSet(0);
        multiAxis->AxisAdd(axis);
    }

    multiAxis->Abort();
    multiAxis->ClearFaults();
    return multiAxis;
}

volatile sig_atomic_t g_shutdown = false;
void sigquit_handler(int signal)
{
    cout << "SIGQUIT handler ran, setting shutdown flag..." << endl;
    g_shutdown = true;
}

volatile sig_atomic_t g_paused = false;
void sigint_handler(int signal)
{
    cout << "SIGINT handler ran, toggling paused flag..." << endl;
    g_paused = !g_paused;
}

void InitializeRMP()
{
    g_controller = CreateController();
    SampleAppsHelper::StartTheNetwork(g_controller.get());
    g_multiAxis = ConfigureAxes(g_controller);

    printf("Enabling Amplifiers...\n");
    g_multiAxis->AmpEnableSet(true);
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
        cerr << "Error during velocity control: " << ex.what() << endl;
        if (g_multiAxis)
            g_multiAxis->Abort();
    }
}

// --- Camera Setup and Priming Utilities ---
void ConfigureCamera()
{
    g_camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
    cout << "Using device: " << g_camera.GetDeviceInfo().GetModelName() << endl;
    g_camera.Open();

    CFeaturePersistence::Load(CONFIG_FILE, &g_camera.GetNodeMap());
    INodeMap &nodemap = g_camera.GetNodeMap();
}

bool TryGrabFrame(int timeoutMs = TIMEOUT_MS, std::ostream &errOut = std::cerr)
{
    try
    {
        g_camera.RetrieveResult(timeoutMs, g_ptrGrabResult, TimeoutHandling_ThrowException);
    }
    catch (const GenericException &e)
    {
        errOut << "Exception during frame grab: " << e.GetDescription() << std::endl;
        return false;
    }

    // Check if the grab result is valid
    if (!g_ptrGrabResult || !g_ptrGrabResult->GrabSucceeded())
    {
        errOut << "Camera grab failed: ";
        if (!g_ptrGrabResult)
        {
            errOut << "Result pointer is null." << std::endl;
        }
        else
        {
            errOut << "Code " << g_ptrGrabResult->GetErrorCode()
                   << ", Desc: " << g_ptrGrabResult->GetErrorDescription() << std::endl;
        }
        return false;
    }
    return true;
}

bool PrimeCamera(std::ostream &errOut = std::cerr)
{
    g_camera.StartGrabbing(GrabStrategy_LatestImageOnly);
    int retries = 0;
    std::ostringstream errorStream;

    while (retries < MAX_RETRIES)
    {
        if (TryGrabFrame(TIMEOUT_MS, errorStream))
        {
            std::cout << "Priming: First frame grabbed successfully." << std::endl;
            return true;
        }
        retries++;
    }

    errOut << "Failed to grab a frame during priming after " << MAX_RETRIES << " retries." << std::endl;
    errOut << errorStream.str(); // Output all accumulated error messages
    return false;
}

// --- Image Processing Function ---
bool ConvertToRGB(Mat &outRgbFrame, TimingStats &convertTiming, std::ostream &errOut = std::cerr)
{
    auto convertStopwatch = ScopedStopwatch(convertTiming);

    if (!g_ptrGrabResult)
    {
        errOut << "Error: Grab result is null!" << endl;
        return false;
    }

    int width = g_ptrGrabResult->GetWidth();
    int height = g_ptrGrabResult->GetHeight();
    const uint8_t *pImageBuffer = static_cast<uint8_t *>(g_ptrGrabResult->GetBuffer());

    if (!pImageBuffer)
    {
        errOut << "Error: Image buffer is null!" << endl;
        return false;
    }
    if (width <= 0 || height <= 0)
    {
        errOut << "Error: Invalid image dimensions! Width: " << width << ", Height: " << height << endl;
        return false;
    }

    Mat bayerFrame(height, width, CV_8UC1, (void *)pImageBuffer);
    if (bayerFrame.empty())
    {
        errOut << "Error: Retrieved empty bayer frame!" << endl;
        return false;
    }

    try
    {
        // Convert Bayer to RGB
        cvtColor(bayerFrame, outRgbFrame, COLOR_BayerBG2RGB);

        if (outRgbFrame.empty())
        {
            errOut << "Error: Bayer-to-RGB conversion produced empty frame!" << endl;
            return false;
        }

        return true; // Successfully converted
    }
    catch (const cv::Exception &e)
    {
        errOut << "OpenCV exception during Bayer-to-RGB conversion: " << e.what() << endl;
        return false;
    }
    catch (const std::exception &e)
    {
        errOut << "Standard exception during Bayer-to-RGB conversion: " << e.what() << endl;
        return false;
    }
    catch (...)
    {
        errOut << "Unknown error during Bayer-to-RGB conversion." << endl;
        return false;
    }
    return false; // Fallback in case of failure
}

bool ProcessFrame(TimingStats &processingTiming, TimingStats &convertTiming, std::ostream &errOut = std::cerr)
{
    auto processingStopwatch = ScopedStopwatch(processingTiming);

    Mat rgbFrame;
    if (!ConvertToRGB(rgbFrame, convertTiming, errOut))
    {

        errOut << "Error: Failed to convert frame to RGB!" << endl;
        return false;
    }

    static bool target_found_last_frame = false;
    bool target_currently_found = false;
    bool result = false;

    // OpenCV Processing
    // GaussianBlur(rgbFrame, rgbFrame, Size(5, 5), 0);
    Mat hsvFrame;
    cvtColor(rgbFrame, hsvFrame, COLOR_RGB2HSV);

    // Define a range for your ball color (not red)
    Scalar lower_ball(lowH, lowS, lowV);
    Scalar upper_ball(highH, highS, highV);

    // Create mask
    Mat mask;
    inRange(hsvFrame, lower_ball, upper_ball, mask);

    // Morphological operations
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
    // erode(mask, mask, kernel);
    // dilate(mask, mask, kernel);
    //  Optional extra smoothing step to close internal gaps
    morphologyEx(mask, mask, MORPH_CLOSE, kernel);

    // Continue with contours
    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // Remove small contours by area
    contours.erase(
        std::remove_if(contours.begin(), contours.end(),
                       [](const vector<Point> &c)
                       {
                           return contourArea(c) < 700.0; // adjust threshold
                       }),
        contours.end());

    int largestContourIndex = -1;
    double largestContourArea = 0;

    if (contours.empty())
    {
        cout << "contours empty" << endl;
    }
    else
    {
        for (int i = 0; i < contours.size(); i++)
        {
            double area = contourArea(contours[i]);
            std::cout << "[DEBUG] Contour #" << i << " — Area: " << area << std::endl;

            if (area > largestContourArea)
            {
                largestContourArea = area;
                largestContourIndex = i;
            }
        }
    }

    if (largestContourIndex != -1)
    {
        Point2f center;
        float radius;
        minEnclosingCircle(contours[largestContourIndex], center, radius);
        std::cout << "Detected radius: " << radius << std::endl;
        if (radius > MIN_CIRCLE_RADIUS)
        {
            target_currently_found = true;
            target_found_last_frame = true;
            circle(rgbFrame, center, (int)radius, Scalar(0, 255, 0), 2);
            circle(rgbFrame, center, 5, Scalar(0, 255, 0), -1);
            circle(rgbFrame, Point(CENTER_X, CENTER_Y), DEAD_ZONE, Scalar(0, 0, 255), 1);

            g_offsetX = center.x - CENTER_X;
            g_offsetY = center.y - CENTER_Y;
        }
    }

    if (!target_currently_found && target_found_last_frame)
    {
        printf("Target lost, stopping motors.\n");
        g_offsetX = 0;
        g_offsetY = 0;
        target_found_last_frame = false;
    }
    result = true;

    imshow("RGB Frame", rgbFrame);
    imshow("Red Mask", mask);
    waitKey(1);
    return result;
}

// --- Main Function ---
int main()
{
    const std::chrono::milliseconds loopInterval(5); // 5ms loop interval
    const std::string SAMPLE_APP_NAME = "Pylon_RSI_Tracking_BayerOnly";
    SampleAppsHelper::PrintHeader(SAMPLE_APP_NAME);
    int exitCode = 0;

    std::signal(SIGQUIT, sigquit_handler);
    std::signal(SIGINT, sigint_handler);

    InitializeRMP();

    // --- Pylon Initialization & Camera Loop ---
    ConfigureCamera();
    if (!PrimeCamera())
    {
        throw std::runtime_error("Camera failed to start streaming images.");
    }

    // --- Main Loop ---
    bool lastPaused = false;
    TimingStats loopTiming, retrieveTiming, processingTiming, convertTiming, motionTiming;
    while (!g_shutdown && g_camera.IsGrabbing())
    {
        ScopedRateLimiter rateLimiter(loopInterval);
        auto loopStopwatch = ScopedStopwatch(loopTiming);

        {
            auto retrieveStopwatch = ScopedStopwatch(retrieveTiming);
            if (!TryGrabFrame())
            {
                ++grabFailures;
                continue;
            }
        }

        if (!ProcessFrame(processingTiming, convertTiming))
        {
            ++processFailures;
            continue;
        }

        // Only stop/resume if the flag changed
        bool paused = g_paused; // Avoid reading the flag multiple times since its volatile
        if (paused && !lastPaused)
            g_multiAxis->Stop();
        else if (!paused && lastPaused)
            g_multiAxis->Resume();
        lastPaused = paused;

        {
            auto motionStopwatch = ScopedStopwatch(motionTiming);
            MoveMotorsWithLimits();
        }
    }

    // Print loop statistics
    printStats("Loop", loopTiming);
    printStats("Retrieve", retrieveTiming);
    printStats("Processing", processingTiming);
    printStats("ConvertToRGB", convertTiming);
    printStats("Motion", motionTiming);

    cout << "--------------------------------------------" << endl;
    cout << "Grab Failures:     " << grabFailures << endl;
    cout << "Process Failures:  " << processFailures << endl;

    destroyAllWindows();

    SampleAppsHelper::PrintFooter(SAMPLE_APP_NAME, exitCode);

    return exitCode;
}