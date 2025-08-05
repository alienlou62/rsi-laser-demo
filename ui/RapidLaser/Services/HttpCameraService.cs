using System.Text.Json;

namespace RapidLaser.Services;

public class HttpCameraService : ICameraService
{
    private readonly HttpClient _httpClient;
    private readonly string _serverUrl;
    private bool _isInitialized;
    private bool _isGrabbing;

    public bool IsInitialized => _isInitialized;
    public bool IsGrabbing => _isGrabbing;
    public int ImageWidth { get; private set; } = 640;
    public int ImageHeight { get; private set; } = 480;

    public HttpCameraService(string serverUrl = "http://localhost:8080")
    {
        _httpClient = new HttpClient();
        _httpClient.Timeout = TimeSpan.FromSeconds(5);
        _serverUrl = serverUrl;
    }

    public async Task<bool> InitializeAsync()
    {
        try
        {
            // Test connection to camera server
            var response = await _httpClient.GetAsync($"{_serverUrl}/camera/status");
            _isInitialized = response.IsSuccessStatusCode;
            return _isInitialized;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to initialize HTTP camera service: {ex.Message}");
            _isInitialized = false;
            return false;
        }
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

        try
        {
            var response = await _httpClient.GetAsync($"{_serverUrl}/camera/frame", cancellationToken);
            if (!response.IsSuccessStatusCode)
                return (false, Array.Empty<byte>(), 0, 0);

            var jsonContent = await response.Content.ReadAsStringAsync(cancellationToken);
            var frameData = JsonSerializer.Deserialize<CameraFrameData>(jsonContent);

            if (frameData?.ImageData == null)
                return (false, Array.Empty<byte>(), 0, 0);

            // Convert base64 JPEG to byte array
            var imageBytes = Convert.FromBase64String(frameData.ImageData);
            
            // Update dimensions from frame data
            ImageWidth = frameData.Width;
            ImageHeight = frameData.Height;

            return (true, imageBytes, frameData.Width, frameData.Height);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to grab frame from HTTP camera service: {ex.Message}");
            return (false, Array.Empty<byte>(), 0, 0);
        }
    }

    public void Dispose()
    {
        _isGrabbing = false;
        _isInitialized = false;
        _httpClient?.Dispose();
    }
}

// Data model matching the JSON structure from SimpleCameraServer.cs
public class CameraFrameData
{
    public int Width { get; set; }
    public int Height { get; set; }
    public double BallCenterX { get; set; }
    public double BallCenterY { get; set; }
    public double BallRadius { get; set; }
    public bool BallDetected { get; set; }
    public string? ImageData { get; set; }
    public long Timestamp { get; set; }
}
