namespace RapidLaser.Services;

public interface ICameraService
{
    bool IsInitialized { get; }
    bool IsGrabbing { get; }
    double FrameRate { get; }
    int ImageWidth { get; }
    int ImageHeight { get; }

    Task<bool> InitializeAsync();
    Task<bool> StartGrabbingAsync();
    Task StopGrabbingAsync();
    Task<(bool success, byte[] imageData, int width, int height)> TryGrabFrameAsync(CancellationToken cancellationToken = default);
    void Dispose();
}

public class SimulatedCameraService : ICameraService
{
    private bool _isInitialized;
    private bool _isGrabbing;
    private readonly Random _random = new();

    public bool IsInitialized => _isInitialized;
    public bool IsGrabbing => _isGrabbing;
    public double FrameRate => 30.0;
    public int ImageWidth => 640;
    public int ImageHeight => 480;

    public Task<bool> InitializeAsync()
    {
        _isInitialized = true;
        return Task.FromResult(true);
    }

    public Task<bool> StartGrabbingAsync()
    {
        if (!_isInitialized)
            return Task.FromResult(false);

        _isGrabbing = true;
        return Task.FromResult(true);
    }

    public Task StopGrabbingAsync()
    {
        _isGrabbing = false;
        return Task.CompletedTask;
    }

    public async Task<(bool success, byte[] imageData, int width, int height)> TryGrabFrameAsync(CancellationToken cancellationToken = default)
    {
        if (!_isGrabbing)
            return (false, Array.Empty<byte>(), 0, 0);

        // Simulate frame grab delay
        await Task.Delay(33, cancellationToken); // ~30 FPS

        // Generate simulated YUV image data with a "ball" 
        var imageData = GenerateSimulatedFrame();

        return (true, imageData, ImageWidth, ImageHeight);
    }

    private byte[] GenerateSimulatedFrame()
    {
        // Create simulated YUV 4:2:2 (YUYV) image data
        var data = new byte[ImageWidth * ImageHeight * 2]; // 2 bytes per pixel for YUYV

        // Fill with gray background
        for (int i = 0; i < data.Length; i += 4)
        {
            data[i] = 128;     // Y1 (luminance)
            data[i + 1] = 128; // U (chrominance)
            data[i + 2] = 128; // Y2 (luminance)
            data[i + 3] = 128; // V (chrominance)
        }

        // Add a simulated moving ball (red circle)
        int ballCenterX = (int)(320 + 100 * Math.Sin(DateTime.Now.Millisecond / 1000.0 * Math.PI));
        int ballCenterY = (int)(240 + 50 * Math.Cos(DateTime.Now.Millisecond / 1000.0 * Math.PI));
        int ballRadius = 20;

        for (int y = Math.Max(0, ballCenterY - ballRadius); y < Math.Min(ImageHeight, ballCenterY + ballRadius); y++)
        {
            for (int x = Math.Max(0, ballCenterX - ballRadius); x < Math.Min(ImageWidth, ballCenterX + ballRadius); x++)
            {
                double distance = Math.Sqrt((x - ballCenterX) * (x - ballCenterX) + (y - ballCenterY) * (y - ballCenterY));
                if (distance <= ballRadius)
                {
                    int pixelIndex = (y * ImageWidth + x) * 2;
                    if (pixelIndex < data.Length - 1)
                    {
                        data[pixelIndex] = 76;      // Y (red luminance)
                        if (x % 2 == 0)
                        {
                            data[pixelIndex + 1] = 84;  // U (red chrominance)
                            data[pixelIndex + 3] = 255; // V (red chrominance)
                        }
                    }
                }
            }
        }

        return data;
    }

    public void Dispose()
    {
        _isGrabbing = false;
        _isInitialized = false;
    }
}
