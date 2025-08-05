using System.Net.Http;
using System.Text.Json;
using System.Text.Json.Serialization;

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

    // Expose the latest frame data for ball detection
    public CameraFrameData? LastFrameData { get; private set; }

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
            var response = await _httpClient.GetAsync($"{_serverUrl}/status");
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
        {
            Console.WriteLine($"HttpCameraService: Not grabbing (_isGrabbing = {_isGrabbing})");
            return (false, Array.Empty<byte>(), 0, 0);
        }

        try
        {
            Console.WriteLine($"HttpCameraService: Making request to {_serverUrl}/camera/frame");
            var response = await _httpClient.GetAsync($"{_serverUrl}/camera/frame", cancellationToken);
            if (!response.IsSuccessStatusCode)
            {
                Console.WriteLine($"HttpCameraService: HTTP request failed with status {response.StatusCode}: {response.ReasonPhrase}");
                return (false, Array.Empty<byte>(), 0, 0);
            }

            var jsonContent = await response.Content.ReadAsStringAsync(cancellationToken);
            Console.WriteLine($"HttpCameraService: Received JSON response length: {jsonContent.Length}");
            Console.WriteLine($"HttpCameraService: JSON content preview: {jsonContent.Substring(0, Math.Min(200, jsonContent.Length))}");

            var frameData = JsonSerializer.Deserialize<CameraFrameData>(jsonContent);
            Console.WriteLine($"HttpCameraService: Deserialized frameData - Width: {frameData?.Width}, Height: {frameData?.Height}, ImageData null: {frameData?.ImageData == null}");

            if (frameData?.ImageData == null)
            {
                Console.WriteLine($"HttpCameraService: No image data in response (frameData={frameData}, ImageData={(frameData?.ImageData != null ? "not null" : "null")})");
                return (false, Array.Empty<byte>(), 0, 0);
            }

            // Store frame data for ball detection access
            LastFrameData = frameData;

            // Convert base64 JPEG to byte array
            var base64Data = frameData.ImageData;
            if (base64Data.StartsWith("data:image/jpeg;base64,"))
            {
                base64Data = base64Data.Substring("data:image/jpeg;base64,".Length);
            }
            var imageBytes = Convert.FromBase64String(base64Data);

            // Update dimensions from frame data
            ImageWidth = frameData.Width;
            ImageHeight = frameData.Height;

            Console.WriteLine($"HttpCameraService: Successfully grabbed frame {frameData.Width}x{frameData.Height}, image bytes: {imageBytes.Length}");
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
    [JsonPropertyName("width")]
    public int Width { get; set; }

    [JsonPropertyName("height")]
    public int Height { get; set; }

    [JsonPropertyName("centerX")]
    public double CenterX { get; set; }

    [JsonPropertyName("centerY")]
    public double CenterY { get; set; }

    [JsonPropertyName("ballDetected")]
    public bool BallDetected { get; set; }

    [JsonPropertyName("imageData")]
    public string? ImageData { get; set; }

    [JsonPropertyName("timestamp")]
    public long Timestamp { get; set; }

    [JsonPropertyName("confidence")]
    public double Confidence { get; set; }

    [JsonPropertyName("targetX")]
    public double TargetX { get; set; }

    [JsonPropertyName("targetY")]
    public double TargetY { get; set; }

    [JsonPropertyName("rtTaskRunning")]
    public bool RtTaskRunning { get; set; }

    [JsonPropertyName("format")]
    public string? Format { get; set; }

    [JsonPropertyName("imageSize")]
    public int ImageSize { get; set; }

    [JsonPropertyName("frameNumber")]
    public int FrameNumber { get; set; }
}
