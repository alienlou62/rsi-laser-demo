using Rsi.Camera;

namespace RapidLaser.Services;

public class GrpcImageProcessingService : IImageProcessingService
{
    public int BinaryThreshold { get; set; } = 155;

    public Task<BallDetectionResult> ProcessFrameAsync(byte[] yuyvImageData, int width, int height)
    {
        // This implementation processes gRPC camera frames that already include ball detection
        // In a real scenario, this would be called with the CameraFrame from gRPC
        throw new NotImplementedException("Use ProcessCameraFrameAsync for gRPC frames");
    }

    // New method specifically for processing gRPC camera frames
    public Task<BallDetectionResult> ProcessCameraFrameAsync(CameraFrame cameraFrame)
    {
        try
        {
            var ballDetection = cameraFrame.BallDetection;
            var stats = cameraFrame.Stats;

            // Create binary mask from ball detection data
            byte[] maskData = Array.Empty<byte>();
            if (ballDetection.Detected)
            {
                maskData = GenerateBinaryMaskFromDetection(
                    cameraFrame.Width,
                    cameraFrame.Height,
                    (int)ballDetection.CenterX,
                    (int)ballDetection.CenterY,
                    (int)ballDetection.Radius
                );
            }

            var result = new BallDetectionResult
            {
                BallDetected = ballDetection.Detected,
                OffsetX = ballDetection.CenterX - (cameraFrame.Width / 2.0),
                OffsetY = ballDetection.CenterY - (cameraFrame.Height / 2.0),
                CenterX = ballDetection.CenterX,
                CenterY = ballDetection.CenterY,
                Confidence = ballDetection.Confidence,
                ObjectsDetected = ballDetection.Detected ? 1 : 0,
                MaskImageData = maskData
            };

            return Task.FromResult(result);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error processing camera frame: {ex.Message}");

            // Return empty result on error
            return Task.FromResult(new BallDetectionResult
            {
                BallDetected = false,
                OffsetX = 0,
                OffsetY = 0,
                CenterX = 0,
                CenterY = 0,
                Confidence = 0,
                ObjectsDetected = 0,
                MaskImageData = Array.Empty<byte>()
            });
        }
    }

    private byte[] GenerateBinaryMaskFromDetection(int width, int height, int ballCenterX, int ballCenterY, int ballRadius)
    {
        // Create binary mask (1 byte per pixel, 0 = black, 255 = white)
        var maskData = new byte[width * height];

        // Fill background with black
        Array.Fill(maskData, (byte)0);

        // Draw white circle for detected ball
        for (int y = Math.Max(0, ballCenterY - ballRadius); y < Math.Min(height, ballCenterY + ballRadius); y++)
        {
            for (int x = Math.Max(0, ballCenterX - ballRadius); x < Math.Min(width, ballCenterX + ballRadius); x++)
            {
                double distance = Math.Sqrt((x - ballCenterX) * (x - ballCenterX) + (y - ballCenterY) * (y - ballCenterY));
                if (distance <= ballRadius)
                {
                    int pixelIndex = y * width + x;
                    if (pixelIndex < maskData.Length)
                    {
                        maskData[pixelIndex] = 255; // White pixel for detected object
                    }
                }
            }
        }

        return maskData;
    }
}
