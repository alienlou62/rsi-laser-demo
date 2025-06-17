#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <numbers>
#include <algorithm>
#include <cstddef>
#include <cstdint>

// --- Camera and Image Processing Headers ---
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <pylon/PylonIncludes.h>

// --- RMP/RSI Headers ---
#include "rsi.h"

#include "camera_helpers.h"
#include "image_processing.h"
#include "misc_helpers.h"
#include "motion_control.h"
#include "rmp_helpers.h"
#include "timing_helpers.h"
#include "image_helpers.h"

#ifndef SANDBOX_DIR
#define SANDBOX_DIR ""
#endif // SANDBOX_DIR

using namespace RSI::RapidCode;
using namespace cv;
using namespace Pylon;
using namespace GenApi;

double CircleFitError(const std::vector<cv::Point>& pts, const cv::Point2f& center, float radius)
{
  double sum = 0.0;
  for (const auto& p : pts) {
    double d = cv::norm(center - cv::Point2f(p));
    sum += (d - radius) * (d - radius);
  }
  return pts.empty() ? 0.0 : sum / pts.size();
}

void FitCircleTaubin(const std::vector<cv::Point> &pts, cv::Point2f &center, float &radius)
{
  // Taubin fit method for circle fitting : https://people.cas.uab.edu/~mosya/cl/CPPcircle.html
  const size_t numPoints = pts.size();

  double sum_x = 0, sum_y = 0;
  for (const auto &p : pts) {
    sum_x += p.x;
    sum_y += p.y;
  }
  double mean_x = sum_x / numPoints;
  double mean_y = sum_y /numPoints;

  // Center data
  std::vector<cv::Point2f> cpts;
  cpts.reserve(numPoints);
  for (const auto &p : pts) cpts.emplace_back(p.x - mean_x, p.y - mean_y);

  // Compute moments
  double Mxx=0, Myy=0, Mxy=0, Mxz=0, Myz=0, Mzz=0;
  for (const auto &p : cpts) {
    double x = p.x, y = p.y;
    double z = x*x + y*y;
    Mxx += x*x;
    Myy += y*y;
    Mxy += x*y;
    Mxz += x*z;
    Myz += y*z;
    Mzz += z*z;
  }
  Mxx /= numPoints; Myy /= numPoints; Mxy /= numPoints;
  Mxz /= numPoints; Myz /= numPoints; Mzz /= numPoints;

  // Taubin’s eigenproblem coefficients
  double Mz = Mxx + Myy;
  double Cov_xy = Mxx*Myy - Mxy*Mxy;
  double A3 = 4*Mz;
  double A2 = -3*Mz*Mz - Mzz;
  double A1 = Mzz*Mz + 4*Cov_xy*Mz - Mxz*Mxz - Myz*Myz;
  double A0 = Mxz*Mxz*Myy + Myz*Myz*Mxx - Mzz*Cov_xy - 2*Mxz*Myz*Mxy;
  double xnew = 0;
  const int iter_max = 20;
  const double epsilon = 1e-12;

  for (int i = 0; i < iter_max; ++i) {
    double y = A3*xnew*xnew*xnew + A2*xnew*xnew + A1*xnew + A0;
    double Dy = 3*A3*xnew*xnew + 2*A2*xnew + A1;
    double xold = xnew;
    xnew = xold - y/Dy;
    if (fabs((xnew - xold)/xnew) < epsilon) break;
  }

  double det = xnew*xnew + xnew*Mz + Cov_xy;
  double a = (Mxz*(Myy - xnew) - Myz*Mxy) / det / 2.0;
  double b = (Myz*(Mxx - xnew) - Mxz*Mxy) / det / 2.0;
  double r = sqrt(a*a + b*b + (Mz + xnew));

  center = cv::Point2f(static_cast<float>(a + mean_x), static_cast<float>(b + mean_y));
  radius = static_cast<float>(r);
}

void FitCircleLeastSquares(const std::vector<Point> &contour, Point2f &center, float &radius)
{
  /*
  Least Squares Circle Fitting method:
  Setup a linear system:
    (x - a)^2 + (y - b)^2 = r^2

  which can be rearranged to:
    2ax + 2by + (r^2 - a^2 - b^2) - x^2 - y^2 = 0
    2ax + 2by + (r^2 - a^2 - b^2) = x^2 + y^2
    2a*x + 2b*y + c = x^2 + y^2 
    (where c is the center of the circle r^2 - a^2 - b^2)

  We can express this as a matrix equation:
    A * X = B
  where:
    A = [x_i, y_i, 1], X = (2a, 2b, c), B = [x_i^2 + y_i^2] 

  We can then solve for X using least squares, which can be done using OpenCV's solve function.
  The solution will give us 2a, 2b, and c, from which we can derive the center (a, b) and radius (r = sqrt(c + a^2 + b^2)).
  */

  // Put the contour points into matrices
  Mat A(contour.size(), 3, CV_32F);
  Mat B(contour.size(), 1, CV_32F);
  Mat X(3, 1, CV_32F);

  // Get raw pointers to the data blocks outside the loop for better performance
  float* A_data = reinterpret_cast<float*>(A.data);
  float* B_data = reinterpret_cast<float*>(B.data);
  const size_t numPoints = contour.size();

  for (size_t i = 0; i < numPoints; ++i)
  {
    const auto& point = contour[i];

    A_data[3 * i + 0] = point.x;
    A_data[3 * i + 1] = point.y;
    A_data[3 * i + 2] = 1.0f;

    B_data[i] = point.x * point.x + point.y * point.y;
  }
  
  // Solve the linear system using SVD decomposition and compute the circle parameters
  solve( A, B, X, cv::DECOMP_SVD );
  center.x = X.at<float>(0) * 0.5f;
  center.y = X.at<float>(1) * 0.5f;
  radius = sqrt( X.at<float>(2) + center.x*center.x + center.y*center.y );
}

bool DetectBall(const Mat& in, Vec3f& out)
{
  std::vector<std::vector<cv::Point>> contours;
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(in, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  // Find the most circular contour, that is of a minimum size
  double minError = 120; 
  for (int i = 0; i < contours.size(); ++i)
  {
    const std::vector<Point>& contour = contours[i];
    if (hierarchy[i][3] != -1) continue; // Skip internal contours
    if (contour.size() < 100) continue; // Filter small contours

    Point2f center;
    float radius;
    FitCircleTaubin(contour, center, radius);
    double error = CircleFitError(contour, center, radius);
    if (error < minError)
    {
      minError = error;
      out = Vec3f(center.x, center.y, radius);
    }
  }
  return true;
}

void ExtractV(const Mat& in, Mat& out)
{
  // Extract the V channel from a YUV image
  for (int i = 0; i < in.rows; ++i)
  {
    const uchar* const inRow = in.ptr<uchar>(i);    // 2 channels per pixel
    uchar* outRow = out.ptr<uchar>(i / 2);

    for (int j = 0; j < CameraHelpers::IMAGE_WIDTH; j += 2)
    {
      outRow[j / 2] = inRow[2 * j + 3];      // channel 1 of pixel j+1 (V)
    }
  }
}

int ProcessYUYVImages()
{
  int exitCode = 0;
  
  // Setup the image reader/writer which will automatically read image into in and write processed image to out each loop
  Mat in = ImageProcessing::CreateYUYVMat(CameraHelpers::IMAGE_WIDTH, CameraHelpers::IMAGE_HEIGHT);
  Mat out(CameraHelpers::IMAGE_HEIGHT / 2, CameraHelpers::IMAGE_WIDTH / 2, CV_8UC3); // Subsampled 3 channel output
  ImageHelpers::ImageReaderWriter readerWriter(ImageHelpers::ImageType::YUYV, in, out);

  TimingStats timing;

  Mat yuv(out.size(), CV_8UC3);
  Mat v(out.size(), CV_8UC1);
  Mat mask(out.size(), CV_8UC1);
  Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(7, 7));
  Vec3f ball;

  const double lowThreshold = 50; // Lower threshold for Canny edge detection
  const double highThreshold = 100; // Upper threshold for Canny edge detection
  for (int index : readerWriter)
  {
    Stopwatch stopwatch(timing);
    ExtractV(in, v); // Extract the V channel from the YUYV image
    threshold(v, mask, 145, 255, THRESH_BINARY);
    morphologyEx(mask, mask, MORPH_CLOSE, kernel);
    morphologyEx(mask, mask, MORPH_OPEN, kernel);

    bool foundBall = DetectBall(mask, ball); // Detect the ball in the mask
    stopwatch.Stop(); // Stop the stopwatch for timing

    cvtColor(mask, out, COLOR_GRAY2BGR); // Convert mask to 3-channel BGR for output
    if (foundBall) // If radius is valid
    {
      Point2f center(ball[0], ball[1]);
      float radius = ball[2];

      // Align circle to pixel grid
      center.x = std::round(center.x);
      center.y = std::round(center.y);
      radius = std::round(radius);
      circle(out, center, static_cast<int>(radius), Scalar(0, 0, 255), 1); // Draw circle around detected ball
    }
  }

  printStats<std::chrono::microseconds>("YUYV Image Processing", timing);

  return exitCode;
}

void SubsampleBayer(const Mat& in, Mat& out)
{
  // Subsample the Bayer image by taking every second 2x2 pixel block
  for (int y = 0; y < CameraHelpers::IMAGE_HEIGHT; y += 4) {
    const uchar* const row0 = in.ptr<uchar>(y);
    const uchar* const row1 = in.ptr<uchar>(y + 1);

    uchar* outRow0 = out.ptr<uchar>(y / 2);
    uchar* outRow1 = out.ptr<uchar>(y / 2 + 1);

    for (int x = 0; x < CameraHelpers::IMAGE_WIDTH; x += 4)
    {
      outRow0[x / 2    ] = row0[x    ];
      outRow0[x / 2 + 1] = row0[x + 1];
      outRow1[x / 2    ] = row1[x    ];
      outRow1[x / 2 + 1] = row1[x + 1];
    }
  }
}

void SubsampleYUYV(const Mat& in, Mat& out)
{
  for (int i = 0; i < CameraHelpers::IMAGE_HEIGHT; i += 2)
  {
    const uchar* const inRow = in.ptr<uchar>(i);    // 2 channels per pixel
    uchar* outRow = out.ptr<uchar>(i / 2);          // 3 channels per pixel

    for (int j = 0; j < CameraHelpers::IMAGE_WIDTH; j += 2)
    {
      outRow[3 * (j / 2)    ] = inRow[2 * j    ];      // channel 0 of pixel j (Y)
      outRow[3 * (j / 2) + 1] = inRow[2 * j + 1];      // channel 1 of pixel j (U)
      outRow[3 * (j / 2) + 2] = inRow[2 * j + 3];      // channel 1 of pixel j+1 (V)
    }
  }
}

int ProcessBayerImages()
{
  int exitCode = 0;
  
  // Setup the image reader/writer which will automatically read image into in and write processed image to out each loop
  Mat in = ImageProcessing::CreateBayerMat(CameraHelpers::IMAGE_WIDTH, CameraHelpers::IMAGE_HEIGHT);
  Mat out(CameraHelpers::IMAGE_HEIGHT / 2, CameraHelpers::IMAGE_WIDTH / 2, CV_8UC3); // Subsampled 3 channel output
  ImageHelpers::ImageReaderWriter readerWriter(ImageHelpers::ImageType::BAYER, in, out);

  TimingStats timing;

  Mat subsampled(out.size(), CV_8UC1);
  Mat rgb(out.size(), CV_8UC3);
  Mat hsv(out.size(), CV_8UC3);
  Mat mask1(out.size(), CV_8UC1);
  Mat mask2(out.size(), CV_8UC1);
  Mat mask(out.size(), CV_8UC1);
  Vec3f ball;
  for (int index : readerWriter)
  {
    Stopwatch stopwatch(timing);
    SubsampleBayer(in, subsampled);
    cvtColor(subsampled, rgb, COLOR_BayerBG2BGR);
    cvtColor(rgb, hsv, COLOR_RGB2HSV);

    inRange(hsv, Scalar(0, 30, 30), Scalar(15, 255, 255), mask1);
    inRange(hsv, Scalar(160, 30, 30), Scalar(180, 255, 255), mask2);
    bitwise_or(mask1, mask2, mask);

    cvtColor(mask, out, COLOR_GRAY2BGR); // Convert mask to 3-channel BGR for output

    bool foundBall = DetectBall(mask, ball);

    // Draw the detected ball if it exists
    if (foundBall) // If radius is valid
    {
      Point2f center(ball[0], ball[1]);
      float radius = ball[2];
      circle(out, center, static_cast<int>(radius), Scalar(0, 255, 0), 2); // Draw circle around detected ball
    }
  }

  printStats<std::chrono::microseconds>("Bayer Image Processing", timing);

  return exitCode;
}

volatile sig_atomic_t g_shutdown = false;
void sigint_handler(int signal)
{
  std::cout << "SIGINT handler ran, toggling shutdown flag..." << std::endl;
  g_shutdown = true;
}

// --- Main Function ---
int main()
{
  const std::string EXECUTABLE_NAME = "Sandbox";
  MiscHelpers::PrintHeader(EXECUTABLE_NAME);
  std::signal(SIGINT, sigint_handler);
  cv::setNumThreads(0); // Turn off OpenCV threading

  int exitCode = 0;
  // exitCode += ProcessBayerImages();
  exitCode += ProcessYUYVImages();

  MiscHelpers::PrintFooter(EXECUTABLE_NAME, exitCode);
  return exitCode;
}
