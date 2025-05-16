#include <opencv2/opencv.hpp>
#include <iostream>
#include <pylon/PylonIncludes.h>                // Pylon Core
#include <pylon/PixelType.h>                    // Pylon Pixel Types (Defines EPixelType etc)
#include <pylon/BaslerUniversalInstantCamera.h> // Needed for nodemap access via CInstantCamera
#include <csignal>                              // For signal handling
#include <cmath>                                // For std::abs, std::clamp, std::min, std::max
#include <thread>                               // For std::this_thread::sleep_for
#include <chrono>                               // For std::chrono::milliseconds

// --- RMP/RSI Headers ---
// !! REMINDER: Compile using the RMP SDK's build system (CMake) to resolve RMP_DEFAULT_PATH !!
#include "SampleAppsHelper.h"
#include "rsi.h"

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
int lowS = 0;   // Lower Saturation
int highS = 255; // Upper Saturation
int lowV = 35;   // Lower Value
int highV = 190; // Upper Value

// --- Global Variables ---
MotionController *controller = nullptr;
Axis *axisX = nullptr;
Axis *axisY = nullptr;
bool motorsPaused = false;

void MoveMotorsWithLimits(Axis *axisX, Axis *axisY, double offsetX, double offsetY)
{
    static const double POSITION_DEAD_ZONE = 0.0005;
    static const double PIXEL_DEAD_ZONE = DEAD_ZONE;
    static const double MAX_OFFSET = 320.0; // max pixel offset
    static const double MIN_VELOCITY = 0.0001;
    static const double jerkPct = 30.0; // Smoother than 50%

    if (motorsPaused)
    {
        try
        {
            if (axisX && axisX->AmpEnableGet())
                axisX->MoveVelocity(0);
            if (axisY && axisY->AmpEnableGet())
                axisY->MoveVelocity(0);
        }
        catch (const std::exception &ex)
        {
            cerr << "Error stopping motors during pause: " << ex.what() << endl;
        }
        return;
    }

    // --- Dead zone logic ---
    if (std::abs(offsetX) < PIXEL_DEAD_ZONE && std::abs(offsetY) < PIXEL_DEAD_ZONE)
    {
        axisX->MoveVelocity(0);
        axisY->MoveVelocity(0);
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
        if (std::abs(velX) < MIN_VELOCITY)
            velX = 0.0;
        if (std::abs(velY) < MIN_VELOCITY)
            velY = 0.0;

        // --- Velocity mode only (no MoveSCurve to position) ---
        axisX->MoveVelocity(velX);
        axisY->MoveVelocity(velY);
    }
    catch (const std::exception &ex)
    {
        cerr << "Error during velocity control: " << ex.what() << endl;
        try
        {
            if (axisX)
                axisX->MoveVelocity(0);
            if (axisY)
                axisY->MoveVelocity(0);
        }
        catch (...)
        {
        }
    }
}

// --- Configure Function (Copied from your "older code") ---
void Configure()
{ /* ... Function body exactly as before ... */
    MotionController::CreationParameters createParams;
    createParams.CpuAffinity = 3;
    strncpy(createParams.RmpPath, "/rsi/", createParams.PathLengthMaximum);
    strncpy(createParams.NicPrimary, "enp3s0", createParams.PathLengthMaximum); // *** VERIFY ***
    controller = MotionController::Create(&createParams);
    SampleAppsHelper::CheckErrors(controller);
    printf("RSI Controller Created!\n");
}

volatile sig_atomic_t gShutdown = 0;
void signal_handler(int signal)
{
    cout << "Signal handler ran, setting shutdown flag..." << endl;
    gShutdown = 1;

    if (axisX)
    {
        try
        {
            axisX->MoveVelocity(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            axisX->AmpEnableSet(false);
        }
        catch (...)
        {
        }
    }
    if (axisY)
    {
        try
        {
            axisY->MoveVelocity(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            axisY->AmpEnableSet(false);
        }
        catch (...)
        {
        }
    }
}

template <typename DurationT>
struct Stopwatch {
    using clock = std::chrono::steady_clock;
    using time_point = typename clock::time_point;

    DurationT total = DurationT::zero();
    DurationT min = DurationT::max();
    DurationT max = DurationT::zero();
    DurationT last = DurationT::zero();
    uint64_t count = 0;

    time_point t_start;
    bool running = false;

    void Start() {
        t_start = clock::now();
        running = true;
    }

    void Stop() {
        if (!running) {
            std::cerr << "Stopwatch::Stop() called without matching Start()!\n";
            return;
        }
        time_point t_end = clock::now();
        last = std::chrono::duration_cast<DurationT>(t_end - t_start);
        total += last;
        if (last < min) min = last;
        if (last > max) max = last;
        ++count;
        running = false;
    }

    double average() const {
        if (count == 0) return 0.0;
        return static_cast<double>(total.count()) / static_cast<double>(count);
    }
};

// Helper function for units
template <typename DurationT>
constexpr const char* getDurationUnits() {
    if constexpr (std::is_same_v<DurationT, std::chrono::hours>)        return "h";
    else if constexpr (std::is_same_v<DurationT, std::chrono::minutes>) return "min";
    else if constexpr (std::is_same_v<DurationT, std::chrono::seconds>) return "s";
    else if constexpr (std::is_same_v<DurationT, std::chrono::milliseconds>) return "ms";
    else if constexpr (std::is_same_v<DurationT, std::chrono::microseconds>) return "us";
    else if constexpr (std::is_same_v<DurationT, std::chrono::nanoseconds>)  return "ns";
    else return "unknown";
}

template <typename DurationT>
void printStats(const std::string& name, const Stopwatch<DurationT>& stats) {
    constexpr const char* units = getDurationUnits<DurationT>();
    std::cout << name << ":\n";
    std::cout << "  Iterations: " << stats.count << "\n";
    std::cout << "  Last:       " << stats.last.count() << " " << units << "\n";
    std::cout << "  Min:        " << stats.min.count() << " " << units << "\n";
    std::cout << "  Max:        " << stats.max.count() << " " << units << "\n";
    std::cout << "  Average:    " << stats.average() << " " << units << "\n";
}

// --- Main Function ---
int main()
{
    using std::chrono::steady_clock;
    using std::chrono::milliseconds;
    using std::chrono::microseconds;
    using std::chrono::duration_cast;

    const milliseconds loopInterval(5); // 5ms loop interval

    const std::string SAMPLE_APP_NAME = "Pylon_RSI_Tracking_BayerOnly";
    std::signal(SIGINT, signal_handler);
    SampleAppsHelper::PrintHeader(SAMPLE_APP_NAME);
    int exitCode = 0;

    try
    { // Outer try for RMP
        // --- RMP Initialization ---
        Configure();
        SampleAppsHelper::StartTheNetwork(controller);
        axisX = controller->AxisGet(0);
        SampleAppsHelper::CheckErrors(axisX);
        axisY = controller->AxisGet(1);
        SampleAppsHelper::CheckErrors(axisY);
        // Axis Configs
        axisX->UserUnitsSet(67108864);
        axisY->UserUnitsSet(67108864);
        axisX->ErrorLimitTriggerValueSet(.5);
        axisY->ErrorLimitTriggerValueSet(.5);
        axisX->ErrorLimitActionSet(RSIAction::RSIActionNONE);
        axisY->ErrorLimitActionSet(RSIAction::RSIActionNONE);
        axisX->HardwarePosLimitTriggerStateSet(1);
        axisX->HardwareNegLimitTriggerStateSet(1); /* ... other limit settings ... */
        axisX->HardwarePosLimitActionSet(RSIAction::RSIActionNONE);
        axisX->HardwareNegLimitActionSet(RSIAction::RSIActionNONE);
        axisX->HardwarePosLimitDurationSet(2);
        axisX->HardwareNegLimitDurationSet(2);
        axisY->HardwarePosLimitTriggerStateSet(1);
        axisY->HardwareNegLimitTriggerStateSet(1); /* ... other limit settings ... */
        axisY->HardwarePosLimitActionSet(RSIAction::RSIActionNONE);
        axisY->HardwareNegLimitActionSet(RSIAction::RSIActionNONE);
        axisY->HardwarePosLimitDurationSet(2);
        axisY->HardwareNegLimitDurationSet(2);
        axisX->ClearFaults();
        axisY->ClearFaults();

        // Enable Amps
        printf("Enabling Amplifiers...\n");
        axisX->AmpEnableSet(true);
        axisY->AmpEnableSet(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        axisX->CommandPositionSet(0.0);
        axisY->CommandPositionSet(0.0);
        printf("Initial positions commanded to zero.\n");
        // --- End RMP Initialization ---

        // --- Pylon Initialization & Camera Loop ---
        PylonInitialize();
        try
        { // Inner try for Pylon/OpenCV
            CInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());
            cout << "Using device: " << camera.GetDeviceInfo().GetModelName() << endl;
            camera.Open();

            // --- Camera Configuration (Force BayerBG8, No Mono8 Fallback) ---
            INodeMap &nodemap = camera.GetNodeMap();

            CEnumerationPtr exposureAuto(nodemap.GetNode("ExposureAuto"));
            if (IsAvailable(exposureAuto) && IsWritable(exposureAuto))
            {
                exposureAuto->FromString("Off");
            }

            CFloatPtr exposureTime(nodemap.GetNode("ExposureTime"));
            if (IsAvailable(exposureTime) && IsWritable(exposureTime))
            {
                exposureTime->SetValue(5000.0); // µs
            }

            cout << "Configuring camera..." << endl;
            // Set TriggerSelector to FrameStart before disabling TriggerMode
            CEnumerationPtr triggerSelector(nodemap.GetNode("TriggerSelector"));
            if (IsAvailable(triggerSelector) && IsWritable(triggerSelector))
            {
                triggerSelector->FromString("FrameStart");
            }

            CEnumerationPtr triggerMode(nodemap.GetNode("TriggerMode"));
            if (IsAvailable(triggerMode) && IsWritable(triggerMode))
            {
                triggerMode->FromString("Off");
            }

            CEnumerationPtr acqMode(nodemap.GetNode("AcquisitionMode"));
            if (IsAvailable(acqMode))
                acqMode->FromString("Continuous");
            // *** Attempt to set ONLY BayerBG8 Pixel Format ***
            CEnumerationPtr pixelFormat(nodemap.GetNode("PixelFormat"));

            // Create window for trackbars
            namedWindow("Trackbars", WINDOW_AUTOSIZE);

            // Create trackbars (each linked to one of the HSV threshold variables)
            createTrackbar("LowH", "Trackbars", &lowH, 179); // Hue range: 0-179
            createTrackbar("HighH", "Trackbars", &highH, 179);
            createTrackbar("LowS", "Trackbars", &lowS, 255); // Saturation range: 0-255
            createTrackbar("HighS", "Trackbars", &highS, 255);
            createTrackbar("LowV", "Trackbars", &lowV, 255); // Value range: 0-255
            createTrackbar("HighV", "Trackbars", &highV, 255);

            if (IsAvailable(pixelFormat))
            {
                try
                {
                    pixelFormat->FromString("BayerBG8"); // Set desired Bayer format
                    cout << "- PixelFormat: BayerBG8" << endl;
                }
                catch (const GenericException &e)
                {
                    // If setting Bayer fails, throw an error and exit.
                    string errMsg = "Error: Failed to set required PixelFormat 'BayerBG8'. Exception: ";
                    errMsg += e.GetDescription();
                    throw std::runtime_error(errMsg); // Stop execution
                }
            }
            else
            {
                throw std::runtime_error("PixelFormat node not available!"); // Stop if node missing
            }

            camera.MaxNumBuffer = 3;
            GenApi::CIntegerPtr(camera.GetNodeMap().GetNode("GevSCPD"))->SetValue(30000);
            camera.StartGrabbing(GrabStrategy_OneByOne);
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Let it warm up
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
            Stopwatch<milliseconds> loopStopwatch, retrieveStopwatch, processingStopwatch, motionStopwatch;
            steady_clock::time_point next = steady_clock::now();
            while (!gShutdown && camera.IsGrabbing())
            {
                // When the next loop iteration is due
                next += loopInterval;

                loopStopwatch.Start();
                try
                {
                    retrieveStopwatch.Start();
                    camera.RetrieveResult(200, ptrGrabResult, TimeoutHandling_Return);
                    retrieveStopwatch.Stop();
                }
                catch (const TimeoutException &timeout_e)
                {
                    cerr << "Camera Retrieve Timeout: " << timeout_e.GetDescription() << endl;
                    if (target_found_last_frame && !motorsPaused)
                    {
                        MoveMotorsWithLimits(axisX, axisY, 0, 0);
                    }
                    target_found_last_frame = false;
                    continue;
                }

                processingStopwatch.Start();
                if (ptrGrabResult->GrabSucceeded())
                {
                    cout << "Grab succeeded" << endl;

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
                        continue;
                    }*/

                    const uint8_t *pImageBuffer = (uint8_t *)ptrGrabResult->GetBuffer();
                    Mat bayerFrame(height, width, CV_8UC1, (void *)pImageBuffer);
                    Mat rgbFrame;
                    if (bayerFrame.empty())
                    {
                        cerr << "Error: Retrieved empty bayer frame!" << endl;
                        continue;
                    }

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
                    Mat mask;
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

                    double offsetX = 0; double offsetY = 0;
                    bool target_currently_found = false;
                    if (largestContourIndex != -1)
                    {
                        Point2f center;
                        float radius;
                        minEnclosingCircle(contours[largestContourIndex], center, radius);
                        std::cout << "Detected radius: " << radius << std::endl;
                        if (radius > MIN_CIRCLE_RADIUS)
                        {
                            // if (radius > MIN_CIRCLE_RADIUS)
                            //{
                            target_currently_found = true;
                            target_found_last_frame = true;
                            circle(rgbFrame, center, (int)radius, Scalar(0, 255, 0), 2);
                            circle(rgbFrame, center, 5, Scalar(0, 255, 0), -1);
                            circle(rgbFrame, Point(CENTER_X, CENTER_Y), DEAD_ZONE, Scalar(0, 0, 255), 1);

                            offsetX = center.x - CENTER_X;
                            offsetY = center.y - CENTER_Y;
                            //}
                        }
                    }

                    if (!target_currently_found && target_found_last_frame && !motorsPaused)
                    {
                        printf("Target lost, stopping motors.\n");
                        offsetX = 0; offsetY = 0;
                        target_found_last_frame = false;
                    }
                    processingStopwatch.Stop();

                    motionStopwatch.Start();
                    MoveMotorsWithLimits(axisX, axisY, offsetX, offsetY);
                    motionStopwatch.Stop();

                    imshow("Processed Frame", rgbFrame);
                    imshow("Red Mask", mask);

                    loopStopwatch.Stop();

                    // --- WaitKey immediately after imshow ---
                    int key = waitKey(1);

                    if (key == 'q' || key == 'Q')
                    {
                        motorsPaused = !motorsPaused;
                        printf("Motors %s.\n", motorsPaused ? "paused" : "resumed");
                        if (motorsPaused)
                            MoveMotorsWithLimits(axisX, axisY, 0, 0);
                    }
                    else if (key == 27) // ESC key
                    {
                        cout << "ESC pressed, initiating shutdown..." << endl;
                        gShutdown = 1;         // <<< set shutdown flag
                        camera.StopGrabbing(); // <<< stop camera manually
                        break;                 // <<< immediately break from loop
                    }

                    // --- Sleep to maintain loop interval ---
                    this_thread::sleep_until(next);

                    // Check for timer overruns
                    if (steady_clock::now() > next + loopInterval)
                    {
                        // cerr << "Warning: Loop overruns detected! Loop time exceeded " << loopInterval.count() << "ms." << endl;
                        next = steady_clock::now(); // Reset next to current time
                    }
                }
            }

            // Print loop statistics
            printStats("Loop", loopStopwatch);
            printStats("Retrieve", retrieveStopwatch);
            printStats("Processing", processingStopwatch);
            printStats("Motion", motionStopwatch);

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
            if (axisX)
            {
                try
                {
                    if (axisX->AmpEnableGet())
                        axisX->AmpEnableSet(false);

                    bool motionDoneX = false;
                    try
                    {
                        motionDoneX = axisX->MotionDoneWait(2000);
                    }
                    catch (...)
                    {
                        cerr << "Axis X: MotionDoneWait threw exception during cleanup." << endl;
                    }

                    if (!motionDoneX)
                    {
                        cerr << "Axis X still moving during cleanup. Forcing immediate stop." << endl;
                        try
                        {
                            axisX->MoveVelocity(0);
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        }
                        catch (...)
                        {
                            cerr << "Axis X: Failed to force MoveVelocity(0)." << endl;
                        }
                    }

                    try
                    {
                        axisX->ClearFaults();
                    }
                    catch (...)
                    {
                        cerr << "Axis X: Failed to clear faults during cleanup." << endl;
                    }
                }
                catch (...)
                {
                    cerr << "Unexpected error cleaning up Axis X." << endl;
                }
            }

            if (axisY)
            {
                try
                {
                    if (axisY->AmpEnableGet())
                        axisY->AmpEnableSet(false);

                    bool motionDoneY = false;
                    try
                    {
                        motionDoneY = axisY->MotionDoneWait(2000);
                    }
                    catch (...)
                    {
                        cerr << "Axis Y: MotionDoneWait threw exception during cleanup." << endl;
                    }

                    if (!motionDoneY)
                    {
                        cerr << "Axis Y still moving during cleanup. Forcing immediate stop." << endl;
                        try
                        {
                            axisY->MoveVelocity(0);
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        }
                        catch (...)
                        {
                            cerr << "Axis Y: Failed to force MoveVelocity(0)." << endl;
                        }
                    }

                    try
                    {
                        axisY->ClearFaults();
                    }
                    catch (...)
                    {
                        cerr << "Axis Y: Failed to clear faults during cleanup." << endl;
                    }
                }
                catch (...)
                {
                    cerr << "Unexpected error cleaning up Axis Y." << endl;
                }
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