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
int lowH = 105;  // Lower Hue
int highH = 126; // Upper Hue
int lowS = 0;    // Lower Saturation
int highS = 255; // Upper Saturation
int lowV = 35;   // Lower Value
int highV = 190; // Upper Value

// --- Global Variables ---
MotionController *controller = nullptr;
MultiAxis *multiAxis = nullptr;
bool motorsPaused = false;

void MoveMotorsWithLimits(double offsetX, double offsetY)
{
    static const double POSITION_DEAD_ZONE = 0.0005;
    static const double PIXEL_DEAD_ZONE = DEAD_ZONE;
    static const double MAX_OFFSET = 320.0; // max pixel offset
    static const double MIN_VELOCITY = 0.0001;
    static const double jerkPct = 30.0; // Smoother than 50%

    // --- Dead zone logic ---
    if (std::abs(offsetX) < PIXEL_DEAD_ZONE && std::abs(offsetY) < PIXEL_DEAD_ZONE)
    {
        multiAxis->MoveVelocity(std::array{0.0, 0.0}.data());
        return;
    }

    try
    {
        // --- Normalize pixel offsets to [-1, 1] ---
        double normX = std::clamp(offsetX / MAX_OFFSET, -1.0, 1.0);
        double normY = std::clamp(offsetY / MAX_OFFSET, -1.0, 1.0);

        // --- Exponential scaling to reduce velocity when near center ---
        double velX = -std::copysign(1.0, normX) * std::pow(std::abs(normX), 1.5) * MAX_VELOCITY;
        double velY = -std::copysign(1.0, normY) * std::pow(std::abs(normY), 1.5) * MAX_VELOCITY;

        // --- Avoid tiny jitter motions ---
        if (std::abs(velX) < MIN_VELOCITY) velX = 0.0;
        if (std::abs(velY) < MIN_VELOCITY) velY = 0.0;

        // --- Velocity mode only (no MoveSCurve to position) ---
        multiAxis->MoveVelocity(std::array{velX, velY}.data());
    }
    catch (const std::exception &ex)
    {
        cerr << "Error during velocity control: " << ex.what() << endl;
        if (multiAxis) multiAxis->Abort();
    }
}

// --- Create the motion controller ---
void CreateController()
{ /* ... Function body exactly as before ... */
    MotionController::CreationParameters createParams;
    createParams.CpuAffinity = 3;
    strncpy(createParams.RmpPath, "/rsi/", createParams.PathLengthMaximum);
    strncpy(createParams.NicPrimary, "enp3s0", createParams.PathLengthMaximum); // *** VERIFY ***
    controller = MotionController::Create(&createParams);
    SampleAppsHelper::CheckErrors(controller);
    printf("RSI Controller Created!\n");
}

void ConfigureAxes()
{
    constexpr int NUM_AXES = 2; // Number of axes to configure

    // Add a motion supervisor for the multiaxis
    controller->AxisCountSet(NUM_AXES + 1);
    multiAxis = controller->MultiAxisGet(NUM_AXES);
    SampleAppsHelper::CheckErrors(multiAxis);

    for (int i = 0; i < NUM_AXES; i++)
    {
        Axis* axis = controller->AxisGet(i);
        SampleAppsHelper::CheckErrors(axis);
        axis->UserUnitsSet(67108864);
        axis->ErrorLimitActionSet(RSIAction::RSIActionNONE);
        axis->HardwarePosLimitActionSet(RSIAction::RSIActionNONE);
        axis->HardwareNegLimitActionSet(RSIAction::RSIActionNONE);
        axis->PositionSet(0);
        multiAxis->AxisAdd(axis);
    }

    multiAxis->Abort();
    multiAxis->ClearFaults();
}

volatile sig_atomic_t gShutdown = 0;
void signal_handler(int signal)
{
    cout << "Signal handler ran, setting shutdown flag..." << endl;
    gShutdown = 1;

    if (multiAxis) multiAxis->Abort();
}

// --- Image Processing Function ---
bool processFrame(const CGrabResultPtr& ptrGrabResult, CInstantCamera& camera, double& offsetX, double& offsetY, Mat& rgbFrame, Mat& mask, bool& target_found_last_frame, TimingStats<std::chrono::milliseconds>& processingTiming) {
    auto processingStopwatch = ScopedStopwatch(processingTiming);
    offsetX = 0;
    offsetY = 0;
    bool target_currently_found = false;
    bool result = false;

    int width = ptrGrabResult->GetWidth();
    int height = ptrGrabResult->GetHeight();

    std::cout << "Pixel type raw value: " << ptrGrabResult->GetPixelType() << std::endl;
    CEnumerationPtr pixelFormatNode(camera.GetNodeMap().GetNode("PixelFormat"));
    if (IsAvailable(pixelFormatNode))
    {
        cout << "PixelFormat (string): " << pixelFormatNode->ToString() << endl;
    }
    else
    {
        cout << "PixelFormat node not available!" << endl;
    }

    /*if (ptrGrabResult->GetPixelType() != Pylon::PixelType_BayerBG8)
    {
        cerr << "Error: Unexpected pixel format received: "
             << Pylon::CPixelTypeMapper::GetNameByPixelType(ptrGrabResult->GetPixelType())
             << ". Expected BayerBG8." << endl;
        processingStopwatch.Stop();
        return false;
    }*/

    const uint8_t *pImageBuffer = (uint8_t *)ptrGrabResult->GetBuffer();
    Mat bayerFrame(height, width, CV_8UC1, (void *)pImageBuffer);
    if (bayerFrame.empty())
    {
        cerr << "Error: Retrieved empty bayer frame!" << endl;
    }
    else
    {
        // Convert Bayer to RGB
        cvtColor(bayerFrame, rgbFrame, COLOR_BayerBG2RGB);

        // OpenCV Processing
        GaussianBlur(rgbFrame, rgbFrame, Size(5, 5), 0);
        Mat hsvFrame;
        cvtColor(rgbFrame, hsvFrame, COLOR_RGB2HSV);

        // Define a range for your ball color (not red)
        Scalar lower_ball(lowH, lowS, lowV);
        Scalar upper_ball(highH, highS, highV);

        // Create mask
        inRange(hsvFrame, lower_ball, upper_ball, mask);

        // Morphological operations
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
        erode(mask, mask, kernel);
        dilate(mask, mask, kernel);
        // Optional extra smoothing step to close internal gaps
        morphologyEx(mask, mask, MORPH_CLOSE, kernel);

        // Continue with contours
        vector<vector<Point>> contours;
        findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        // Remove small contours by area
        contours.erase(
            std::remove_if(contours.begin(), contours.end(),
                        [](const vector<Point> &c)
                        {
                            return contourArea(c) < 500.0; // adjust threshold
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

                offsetX = center.x - CENTER_X;
                offsetY = center.y - CENTER_Y;
            }
        }

        if (!target_currently_found && target_found_last_frame && !motorsPaused)
        {
            printf("Target lost, stopping motors.\n");
            offsetX = 0;
            offsetY = 0;
            target_found_last_frame = false;
        }
        result = true;
    }
    return result;
}

// --- Main Function ---
int main()
{
    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;
    using std::chrono::steady_clock;

    const milliseconds loopInterval(5); // 5ms loop interval

    const std::string SAMPLE_APP_NAME = "Pylon_RSI_Tracking_BayerOnly";
    std::signal(SIGINT, signal_handler);
    SampleAppsHelper::PrintHeader(SAMPLE_APP_NAME);
    int exitCode = 0;

    try
    { // Outer try for RMP
        // --- RMP Initialization ---
        CreateController();
        SampleAppsHelper::StartTheNetwork(controller);
        ConfigureAxes();

        // Enable Amps
        printf("Enabling Amplifiers...\n");
        multiAxis->AmpEnableSet(true);
        // --- End RMP Initialization ---

        // --- Pylon Initialization & Camera Loop ---
        PylonInitialize();
        try
        { // Inner try for Pylon/OpenCV
            CInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());
            cout << "Using device: " << camera.GetDeviceInfo().GetModelName() << endl;
            camera.Open();


            CFeaturePersistence::Load(CONFIG_FILE, &camera.GetNodeMap());


            // --- Lock TLParams before configuration ---
            GenApi::CIntegerPtr tlLocked(camera.GetTLNodeMap().GetNode("TLParamsLocked"));
            if (IsAvailable(tlLocked) && IsWritable(tlLocked))
            {
                tlLocked->SetValue(1);
            }

            camera.StartGrabbing(GrabStrategy_OneByOne);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Let it warm up
                                                                          // --- Unlock TLParams after configuration ---
            if (IsAvailable(tlLocked) && IsWritable(tlLocked))
            {
                tlLocked->SetValue(0);
            }

            cout << "Checking camera grabbing status after StartGrabbing..." << endl;
            if (!camera.IsGrabbing())
            {
                cerr << "Error: Camera is NOT grabbing after StartGrabbing call!" << endl;
                // maybe attempt StartGrabbing() again here
            }
            else
            {
                cout << "Camera is confirmed to be grabbing." << endl;
            }

            cout << "Camera grabbing started." << endl;

            // --- Prime the camera to verify frames are coming ---
            bool grabbedFirstFrame = false;
            auto startTime = std::chrono::steady_clock::now();
            while (!gShutdown && camera.IsGrabbing())
            {
                try
                {
                    CGrabResultPtr grabResult;
                    camera.RetrieveResult(5000, grabResult, TimeoutHandling_Return);
                    if (grabResult && grabResult->GrabSucceeded())
                    {
                        grabbedFirstFrame = true;
                        cout << "Priming: First frame grabbed successfully." << endl;
                        break;
                    }
                }
                catch (const GenericException &e)
                {
                    cerr << "Exception during priming grab: " << e.GetDescription() << endl;
                }

                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count() > 10)
                {
                    cerr << "Timeout waiting for first frame. Exiting startup." << endl;
                    gShutdown = 1;
                    break;
                }
            }

            if (!grabbedFirstFrame)
            {
                cerr << "First frame not received, shutting down camera." << endl;
                camera.StopGrabbing();
                throw std::runtime_error("Camera failed to start streaming images.");
            }

            CGrabResultPtr ptrGrabResult;
            bool target_found_last_frame = false;

            // --- Main Loop ---
            TimingStats<milliseconds> loopTiming, retrieveTiming, processingTiming, motionTiming;
            while (!gShutdown && camera.IsGrabbing())
            {   
                ScopedRateLimiter rateLimiter(loopInterval);
                auto loopStopwatch = ScopedStopwatch(loopTiming);
                
                try
                {
                    auto retrieveStopwatch = ScopedStopwatch(retrieveTiming);
                    camera.RetrieveResult(200, ptrGrabResult, TimeoutHandling_Return);
                }
                catch (const TimeoutException &timeout_e)
                {
                    cerr << "Camera Retrieve Timeout: " << timeout_e.GetDescription() << endl;
                    if (target_found_last_frame && !motorsPaused)
                    {
                        MoveMotorsWithLimits(0, 0);
                    }
                    target_found_last_frame = false;
                    continue;
                }

                double offsetX = 0, offsetY = 0;
                Mat rgbFrame, mask;
                bool processed = false;
                if (ptrGrabResult->GrabSucceeded())
                {
                    cout << "Grab succeeded" << endl;
                    processed = processFrame(ptrGrabResult, camera, offsetX, offsetY, rgbFrame, mask, target_found_last_frame, processingTiming);
                }

                if (processed)
                {
                    auto motionStopwatch = ScopedStopwatch(motionTiming);
                    MoveMotorsWithLimits(offsetX, offsetY);
                    motionStopwatch.Stop();

                    imshow("Processed Frame", rgbFrame);
                    imshow("Red Mask", mask);
                }

                // --- WaitKey immediately after imshow ---
                int key = waitKey(1);

                if (key == 'q' || key == 'Q')
                {
                    motorsPaused = !motorsPaused;
                    printf("Motors %s.\n", motorsPaused ? "paused" : "resumed");
                    if (motorsPaused)
                        multiAxis->Stop();
                    else
                        multiAxis->Resume();
                }
                else if (key == 27) // ESC key
                {
                    cout << "ESC pressed, initiating shutdown..." << endl;
                    gShutdown = 1;         // <<< set shutdown flag
                    camera.StopGrabbing(); // <<< stop camera manually
                    break;                 // <<< immediately break from loop
                }
            }

            // Print loop statistics
            printStats("Loop", loopTiming);
            printStats("Retrieve", retrieveTiming);
            printStats("Processing", processingTiming);
            printStats("Motion", motionTiming);

            cout << "Stopping camera grabbing..." << endl;
            try
            {
                if (camera.IsGrabbing())
                {
                    camera.StopGrabbing();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Let buffers clear
                }
                if (camera.IsOpen())
                {
                    camera.Close();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow graceful camera closing
                }
            }
            catch (const GenericException &e)
            {
                cerr << "Error during camera shutdown: " << e.GetDescription() << endl;
            }
        } // End Inner Pylon/OpenCV Try
        catch (const GenericException &e)
        {
            cerr << "Pylon Exception: " << e.GetDescription() << endl;
            exitCode = 1;
        }
        catch (const std::exception &e)
        {
            cerr << "Standard Exception (Pylon/OpenCV): " << e.what() << endl;
            exitCode = 1;
        }
        catch (...)
        {
            cerr << "Unknown Exception (Pylon/OpenCV)." << endl;
            exitCode = 1;
        }

        cout << "Terminating Pylon system..." << endl;
        PylonTerminate();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } // End Outer RMP Try
    catch (const std::exception &ex)
    {
        cerr << "Top Level Exception (RMP Init?): " << ex.what() << endl;
        exitCode = 1;
    }
    catch (...)
    {
        cerr << "Unknown Top Level Exception." << endl;
        exitCode = 1;
    }

    // --- RMP/RSI Cleanup ---
    cout << "Starting RMP/RSI Cleanup..." << endl;
    cout << "Starting RMP/RSI Cleanup..." << endl;
    if (controller)
    {
        try
        {
            if (multiAxis)
            {
                multiAxis->Abort();
                multiAxis->ClearFaults();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            cout << "Deleting RMP/RSI Controller..." << endl;
            try
            {
                controller->Delete();
                controller = nullptr;
            }
            catch (...)
            {
                cerr << "Controller delete failed during cleanup." << endl;
            }
        }
        catch (const std::exception &cleanup_ex)
        {
            cerr << "Exception during RMP/RSI cleanup: " << cleanup_ex.what() << endl;
            if (exitCode == 0)
                exitCode = 1;
        }
    }
    else
    {
        cout << "RMP/RSI Controller not initialized, skipping cleanup." << endl;
    }
    cout << "Cleanup finished." << endl;

    destroyAllWindows();
    SampleAppsHelper::PrintFooter(SAMPLE_APP_NAME, exitCode);
    cout << "System fully cleaned up. Waiting 1s before exit..." << endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    return exitCode;
}