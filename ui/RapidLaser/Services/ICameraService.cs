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
