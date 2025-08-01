using Grpc.Core;
using Rsi.Camera;

namespace RapidLaser.Services;

public class GrpcCameraService : ICameraService
{
    private GrpcChannel? _channel;
    private CameraStreamService.CameraStreamServiceClient? _client;
    private bool _isInitialized;
    private bool _isGrabbing;
    private string _serverAddress = "localhost:50061";

    public bool IsInitialized => _isInitialized;
    public bool IsGrabbing => _isGrabbing;
    public int ImageWidth { get; private set; } = 640;
    public int ImageHeight { get; private set; } = 480;

    public async Task<bool> InitializeAsync()
    {
        try
        {
            // Create gRPC channel to camera server
            _channel = GrpcChannel.ForAddress($"http://{_serverAddress}");
            _client = new CameraStreamService.CameraStreamServiceClient(_channel);
            
            // Test connection by trying to get a frame
            var frameRequest = new FrameRequest
            {
                Format = ImageFormat.FormatRgb,
                CompressionQuality = 85
            };
            
            var response = await _client.GetLatestFrameAsync(frameRequest, 
                deadline: DateTime.UtcNow.AddSeconds(5));
            
            if (response != null)
            {
                ImageWidth = response.Width;
                ImageHeight = response.Height;
                _isInitialized = true;
                return true;
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to initialize gRPC camera service: {ex.Message}");
        }
        
        return false;
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
        if (!_isGrabbing || _client == null)
            return (false, Array.Empty<byte>(), 0, 0);

        try
        {
            var frameRequest = new FrameRequest
            {
                Format = ImageFormat.FormatRgb,
                CompressionQuality = 85
            };
            
            var response = await _client.GetLatestFrameAsync(frameRequest, 
                deadline: DateTime.UtcNow.AddSeconds(1),
                cancellationToken: cancellationToken);
            
            if (response?.ImageData != null && response.ImageData.Length > 0)
            {
                return (true, response.ImageData.ToByteArray(), response.Width, response.Height);
            }
        }
        catch (RpcException rpcEx) when (rpcEx.StatusCode == StatusCode.DeadlineExceeded)
        {
            // Timeout is expected when no new frames are available
            return (false, Array.Empty<byte>(), 0, 0);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error grabbing frame: {ex.Message}");
        }

        return (false, Array.Empty<byte>(), 0, 0);
    }

    public void Dispose()
    {
        StopGrabbingAsync().Wait();
        _channel?.Dispose();
        _client = null;
        _isInitialized = false;
    }

    // Additional method for streaming
    public async IAsyncEnumerable<CameraFrame> StreamFramesAsync(
        int maxFps = 30, 
        ImageFormat format = ImageFormat.FormatRgb, 
        int quality = 85,
        [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken cancellationToken = default)
    {
        if (!_isGrabbing || _client == null)
            yield break;

        var streamRequest = new StreamRequest
        {
            MaxFps = maxFps,
            Format = format,
            CompressionQuality = quality
        };

        using var streamingCall = _client.StreamCameraFrames(streamRequest, cancellationToken: cancellationToken);
        
        await foreach (var frame in streamingCall.ResponseStream.ReadAllAsync(cancellationToken))
        {
            yield return frame;
        }
    }
}
