namespace RapidLaser.Services;

public struct BallDetectionResult
{
    public bool BallDetected { get; init; }
    public double OffsetX { get; init; }
    public double OffsetY { get; init; }
    public double CenterX { get; init; }
    public double CenterY { get; init; }
    public double Confidence { get; init; }
    public int ObjectsDetected { get; init; }
    public byte[] MaskImageData { get; init; }
}

public interface IImageProcessingService
{
    Task<BallDetectionResult> ProcessFrameAsync(byte[] yuyvImageData, int width, int height);
    int BinaryThreshold { get; set; }
}

public class SimulatedImageProcessingService : IImageProcessingService
{
    private readonly Random _random = new();

    public int BinaryThreshold { get; set; } = 155;

    public Task<BallDetectionResult> ProcessFrameAsync(byte[] yuyvImageData, int width, int height)
    {
        // Simulate image processing delay
        Task.Delay(15); // ~15ms processing time

        // Simulate ball detection from YUV data
        // In real implementation, this would:
        // 1. Extract V channel from YUV
        // 2. Apply threshold to create binary mask
        // 3. Find contours and fit circles
        // 4. Calculate ball center and offsets

        var result = SimulateBallDetection(width, height);

        return Task.FromResult(result);
    }

    private BallDetectionResult SimulateBallDetection(int width, int height)
    {
        // Simulate moving ball position 
        double time = DateTime.Now.Millisecond / 1000.0;
        double ballCenterX = 320 + 100 * Math.Sin(time * Math.PI);
        double ballCenterY = 240 + 50 * Math.Cos(time * Math.PI);

        // Calculate offset from image center (320, 240)
        double offsetX = ballCenterX - 320;
        double offsetY = ballCenterY - 240;

        // Simulate confidence (decrease when ball is near edges)
        double distanceFromCenter = Math.Sqrt(offsetX * offsetX + offsetY * offsetY);
        double confidence = Math.Max(70, 100 - (distanceFromCenter / 150 * 30));

        // Generate binary mask image data
        var maskData = GenerateBinaryMask(width, height, (int)ballCenterX, (int)ballCenterY, 20);

        return new BallDetectionResult
        {
            BallDetected = true,
            OffsetX = offsetX,
            OffsetY = offsetY,
            CenterX = ballCenterX,
            CenterY = ballCenterY,
            Confidence = confidence,
            ObjectsDetected = 1,
            MaskImageData = maskData
        };
    }

    private byte[] GenerateBinaryMask(int width, int height, int ballCenterX, int ballCenterY, int ballRadius)
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
