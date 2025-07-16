namespace RapidLaser.Services;

public class RTTaskGlobals
{
    public bool CameraReady { get; set; }
    public bool NewTarget { get; set; }
    public double TargetX { get; set; }
    public double TargetY { get; set; }
}

public class TimingStats
{
    public double MinTime { get; private set; } = double.MaxValue;
    public double MaxTime { get; private set; } = double.MinValue;
    public double TotalTime { get; private set; }
    public long Count { get; private set; }
    public double AverageTime => Count > 0 ? TotalTime / Count : 0;

    public void AddSample(double timeMs)
    {
        MinTime = Math.Min(MinTime, timeMs);
        MaxTime = Math.Max(MaxTime, timeMs);
        TotalTime += timeMs;
        Count++;
    }

    public void Reset()
    {
        MinTime = double.MaxValue;
        MaxTime = double.MinValue;
        TotalTime = 0;
        Count = 0;
    }
}

public interface IRTTaskManagerService : IDisposable
{
    bool IsRunning { get; }
    string ManagerState { get; }
    int TasksActive { get; }
    double CpuUsage { get; }
    double MemoryUsage { get; }

    // Task-specific status
    string BallDetectionStatus { get; }
    double BallDetectionLoopTime { get; }
    long BallDetectionIterations { get; }

    string MotionControlStatus { get; }
    double MotionControlLoopTime { get; }

    // Global data access
    RTTaskGlobals Globals { get; }

    // Control methods
    Task StartTasksAsync();
    Task ShutdownTasksAsync();

    // Events for real-time updates
    event EventHandler<RTTaskGlobals>? GlobalsUpdated;
    event EventHandler? TaskStatusChanged;
}

public class SimulatedRTTaskManagerService : IRTTaskManagerService, IDisposable
{
    private readonly ICameraService _cameraService;
    private readonly IImageProcessingService _imageProcessingService;
    private readonly IMotionControlService _motionControlService;

    private bool _isRunning;
    private CancellationTokenSource? _cancellationTokenSource;
    private Task? _mainLoopTask;

    private readonly TimingStats _ballDetectionTiming = new();
    private readonly TimingStats _motionControlTiming = new();
    private long _ballDetectionIterations;

    public RTTaskGlobals Globals { get; } = new();

    public bool IsRunning => _isRunning;
    public string ManagerState => _isRunning ? "Running" : "Stopped";
    public int TasksActive => _isRunning ? 2 : 0;
    public double CpuUsage => _isRunning ? 20 + Random.Shared.NextDouble() * 10 : 0;
    public double MemoryUsage => _isRunning ? 140 + Random.Shared.NextDouble() * 20 : 100;

    public string BallDetectionStatus => _isRunning ? "Active" : "Inactive";
    public double BallDetectionLoopTime => _ballDetectionTiming.AverageTime;
    public long BallDetectionIterations => _ballDetectionIterations;

    public string MotionControlStatus => _isRunning ? "Active" : "Inactive";
    public double MotionControlLoopTime => _motionControlTiming.AverageTime;

    public event EventHandler<RTTaskGlobals>? GlobalsUpdated;
    public event EventHandler? TaskStatusChanged;

    public SimulatedRTTaskManagerService(
        ICameraService cameraService,
        IImageProcessingService imageProcessingService,
        IMotionControlService motionControlService)
    {
        _cameraService = cameraService;
        _imageProcessingService = imageProcessingService;
        _motionControlService = motionControlService;
    }

    public async Task StartTasksAsync()
    {
        if (_isRunning)
            return;

        // Initialize services
        await _cameraService.InitializeAsync();
        await _motionControlService.InitializeAsync();
        await _motionControlService.EnableAsync();
        await _cameraService.StartGrabbingAsync();

        Globals.CameraReady = _cameraService.IsGrabbing;

        _isRunning = true;
        _cancellationTokenSource = new CancellationTokenSource();

        // Start main processing loop
        _mainLoopTask = Task.Run(MainProcessingLoop, _cancellationTokenSource.Token);

        TaskStatusChanged?.Invoke(this, EventArgs.Empty);
    }

    public async Task ShutdownTasksAsync()
    {
        if (!_isRunning)
            return;

        _isRunning = false;
        _cancellationTokenSource?.Cancel();

        try
        {
            await _mainLoopTask;
        }
        catch (OperationCanceledException) { }
        catch { }

        await _cameraService.StopGrabbingAsync();
        await _motionControlService.DisableAsync();

        Globals.CameraReady = false;
        Globals.NewTarget = false;

        TaskStatusChanged?.Invoke(this, EventArgs.Empty);
    }

    private async Task MainProcessingLoop()
    {
        while (!_cancellationTokenSource.Token.IsCancellationRequested && _isRunning)
        {
            try
            {
                var loopStartTime = DateTime.UtcNow;

                // Frame retrieval and processing
                var grabResult = await _cameraService.TryGrabFrameAsync(_cancellationTokenSource.Token);
                if (grabResult.success)
                {
                    // Get current motor positions
                    double initialX = _motionControlService.CurrentX;
                    double initialY = _motionControlService.CurrentY;

                    // Image processing
                    var processingStartTime = DateTime.UtcNow;
                    var detection = await _imageProcessingService.ProcessFrameAsync(
                        grabResult.imageData, grabResult.width, grabResult.height);
                    var processingTime = (DateTime.UtcNow - processingStartTime).TotalMilliseconds;
                    _ballDetectionTiming.AddSample(processingTime);
                    _ballDetectionIterations++;

                    if (detection.BallDetected)
                    {
                        // Calculate target positions
                        double targetX = initialX + detection.OffsetX;
                        double targetY = initialY + detection.OffsetY;

                        // Motion control
                        var motionStartTime = DateTime.UtcNow;
                        await _motionControlService.MoveToAsync(targetX, targetY);
                        var motionTime = (DateTime.UtcNow - motionStartTime).TotalMilliseconds;
                        _motionControlTiming.AddSample(motionTime);

                        // Update globals
                        Globals.TargetX = targetX;
                        Globals.TargetY = targetY;
                        Globals.NewTarget = true;

                        GlobalsUpdated?.Invoke(this, Globals);
                    }
                }

                // Maintain ~200 Hz loop rate (5ms)
                var loopTime = (DateTime.UtcNow - loopStartTime).TotalMilliseconds;
                var remainingTime = Math.Max(0, 5 - loopTime);
                if (remainingTime > 0)
                {
                    await Task.Delay(TimeSpan.FromMilliseconds(remainingTime), _cancellationTokenSource.Token);
                }
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (Exception ex)
            {
                // Log error in real implementation
                Console.WriteLine($"Error in main processing loop: {ex.Message}");
            }
        }
    }

    public void Dispose()
    {
        ShutdownTasksAsync().Wait();
        _cancellationTokenSource?.Dispose();
        _cameraService?.Dispose();
        _motionControlService?.Dispose();
    }
}
