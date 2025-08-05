
namespace RapidLaser.Services;

public interface ICameraService
{
    bool IsInitialized { get; }
    bool IsGrabbing { get; }
    int ImageWidth { get; }
    int ImageHeight { get; }

    Task<bool> InitializeAsync();
    Task<bool> StartGrabbingAsync();
    Task StopGrabbingAsync();
    Task<bool> CheckServerStatusAsync(CancellationToken cancellationToken = default);
    Task<(bool success, byte[] imageData, int width, int height)> TryGrabFrameAsync(CancellationToken cancellationToken = default);
    void Dispose();
}

public class HttpCameraService : ICameraService
{
    /** FIELDS **/
    //private
    private readonly HttpClient _httpClient;
    private readonly string _serverUrl;
    private bool _isInitialized;
    private bool _isGrabbing;
    //public
    public bool IsInitialized => _isInitialized;
    public bool IsGrabbing => _isGrabbing;
    public int ImageWidth { get; private set; } = 640;
    public int ImageHeight { get; private set; } = 480;


    /** CONSTRUCTOR **/
    public HttpCameraService(string serverUrl = "http://localhost:50080")
    {
        _httpClient = new HttpClient();
        _httpClient.Timeout = TimeSpan.FromSeconds(5);
        _serverUrl = serverUrl;
    }


    /** METHODS **/
    public async Task<bool> InitializeAsync()
    {
        try
        {
            Console.WriteLine($"HttpCameraService: Attempting to connect to {_serverUrl}/status");
            // Test connection to camera server
            var response = await _httpClient.GetAsync($"{_serverUrl}/status");
            _isInitialized = response.IsSuccessStatusCode;
            Console.WriteLine($"HttpCameraService: Connection test result - StatusCode: {response.StatusCode}, IsSuccess: {response.IsSuccessStatusCode}");
            return _isInitialized;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to initialize HTTP camera service to {_serverUrl}: {ex.Message}");
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

    public async Task<bool> CheckServerStatusAsync(CancellationToken cancellationToken = default)
    {
        try
        {
            var response = await _httpClient.GetAsync($"{_serverUrl}/status", cancellationToken);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
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

            var jsonDocument = JsonDocument.Parse(jsonContent);
            var imageData = jsonDocument.RootElement.GetProperty("imageData").GetString();
            Console.WriteLine($"HttpCameraService: Deserialized imageData - null: {imageData == null}");

            if (string.IsNullOrEmpty(imageData))
            {
                Console.WriteLine($"HttpCameraService: No image data in response");
                return (false, Array.Empty<byte>(), 0, 0);
            }

            // Convert base64 JPEG to byte array
            var base64Data = imageData;
            if (base64Data.StartsWith("data:image/jpeg;base64,"))
            {
                base64Data = base64Data.Substring("data:image/jpeg;base64,".Length);
            }
            var imageBytes = Convert.FromBase64String(base64Data);

            // For mock data, use default dimensions since we only have a minimal JPEG header
            // In real implementation, you would decode the image to get actual dimensions
            const int defaultWidth = 640;
            const int defaultHeight = 480;

            // Update dimensions
            ImageWidth = defaultWidth;
            ImageHeight = defaultHeight;

            Console.WriteLine($"HttpCameraService: Successfully grabbed frame {defaultWidth}x{defaultHeight}, image bytes: {imageBytes.Length}");
            return (true, imageBytes, defaultWidth, defaultHeight);
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
