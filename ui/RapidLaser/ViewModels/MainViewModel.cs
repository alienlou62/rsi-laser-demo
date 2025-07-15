using System.Diagnostics;
using System.Timers;

namespace RapidLaser.ViewModels;

public partial class MainViewModel : ViewModelBase, IDisposable
{
    // Reference to the main window for window operations
    private Window? _mainWindow;

    // Services
    private readonly ICameraService _cameraService;
    private readonly IImageProcessingService _imageProcessingService;
    private readonly IMotionControlService _motionControlService;
    private readonly IRTTaskManagerService _rtTaskManagerService;

    // Timer for UI updates
    private readonly System.Timers.Timer _updateTimer;

    public MainViewModel()
    {
        // Initialize services (in real app, use DI)
        _cameraService = new SimulatedCameraService();
        _imageProcessingService = new SimulatedImageProcessingService();
        _motionControlService = new SimulatedMotionControlService();
        _rtTaskManagerService = new SimulatedRTTaskManagerService(
            _cameraService, _imageProcessingService, _motionControlService);

        // Subscribe to service events
        _rtTaskManagerService.GlobalsUpdated += OnGlobalsUpdated;
        _rtTaskManagerService.TaskStatusChanged += OnTaskStatusChanged;

        // Setup UI update timer
        _updateTimer = new System.Timers.Timer(100); // Update UI every 100ms
        _updateTimer.Elapsed += OnUpdateTimerElapsed;
        _updateTimer.Start();
    }

    public void SetMainWindow(Window window)
    {
        _mainWindow = window;
    }

    // Event handlers
    private void OnGlobalsUpdated(object? sender, RTTaskGlobals globals)
    {
        // Update UI properties with latest global values
        BallXPosition = globals.TargetX;
        BallYPosition = globals.TargetY;
    }

    private void OnTaskStatusChanged(object? sender, EventArgs e)
    {
        // Update task status properties
        OnPropertyChanged(nameof(ManagerState));
        OnPropertyChanged(nameof(TasksActive));
        OnPropertyChanged(nameof(BallDetectionStatus));
        OnPropertyChanged(nameof(MotionControlStatus));
    }

    private void OnUpdateTimerElapsed(object? sender, ElapsedEventArgs e)
    {
        // Update UI properties that change continuously
        if (_rtTaskManagerService.IsRunning)
        {
            CpuUsage = _rtTaskManagerService.CpuUsage;
            MemoryUsage = _rtTaskManagerService.MemoryUsage;
            BallDetectionLoopTime = _rtTaskManagerService.BallDetectionLoopTime;
            BallDetectionIterations = _rtTaskManagerService.BallDetectionIterations;
            MotionControlLoopTime = _rtTaskManagerService.MotionControlLoopTime;

            // Update motion positions
            BallXPosition = _motionControlService.CurrentX;
            BallYPosition = _motionControlService.CurrentY;

            // Update motion paused status
            MotionPaused = _motionControlService.MotionPaused;
        }
    }

    // RTTask Global Values
    [ObservableProperty]
    private double _ballXPosition = 320;

    [ObservableProperty]
    private double _ballYPosition = 240;

    [ObservableProperty]
    private double _ballVelocityX = 12.5;

    [ObservableProperty]
    private double _ballVelocityY = -8.2;

    [ObservableProperty]
    private double _detectionConfidence = 95.0;

    // RTTaskManager Status
    public string ManagerState => _rtTaskManagerService?.ManagerState ?? "Stopped";
    public string TasksActive => _rtTaskManagerService?.TasksActive.ToString() + "/2" ?? "0/2";

    [ObservableProperty]
    private double _cpuUsage = 23.0;

    [ObservableProperty]
    private double _memoryUsage = 145.0; // MB

    // Ball Detection Task Status
    public string BallDetectionStatus => _rtTaskManagerService?.BallDetectionStatus ?? "Inactive";

    [ObservableProperty]
    private double _ballDetectionLoopTime = 33.0; // ms

    [ObservableProperty]
    private long _ballDetectionIterations = 45123;

    // Motion Control Task Status
    public string MotionControlStatus => _rtTaskManagerService?.MotionControlStatus ?? "Inactive";

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(MotionPausedDisplay))]
    [NotifyPropertyChangedFor(nameof(MotionToggleButtonText))]
    [NotifyPropertyChangedFor(nameof(MotionToggleButtonColor))]
    private bool _motionPaused = false;

    [ObservableProperty]
    private double _motionControlLoopTime = 16.0; // ms

    // Camera/Display Properties
    [ObservableProperty]
    private double _frameRate = 30.0;

    [ObservableProperty]
    private string _systemStatus = "SYSTEM ONLINE";

    [ObservableProperty]
    private int _binaryThreshold = 128;

    [ObservableProperty]
    private int _objectsDetected = 1;

    // Display Properties
    public string MotionPausedDisplay => MotionPaused ? "Yes" : "No";
    public string MotionToggleButtonText => MotionPaused ? "Resume Motion" : "Pause Motion";
    public string MotionToggleButtonColor => MotionPaused ? "#4CAF50" : "#FF6B35";

    // Commands for UI actions
    [RelayCommand]
    private async Task StartTasks()
    {
        try
        {
            await _rtTaskManagerService.StartTasksAsync();
            SystemStatus = "TASKS STARTING";
        }
        catch (Exception ex)
        {
            SystemStatus = $"START ERROR: {ex.Message}";
        }
    }

    [RelayCommand]
    private async Task ShutdownTasks()
    {
        try
        {
            await _rtTaskManagerService.ShutdownTasksAsync();
            SystemStatus = "TASKS STOPPED";
        }
        catch (Exception ex)
        {
            SystemStatus = $"SHUTDOWN ERROR: {ex.Message}";
        }
    }

    [RelayCommand]
    private void LaunchRsiConfig()
    {
        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "RSIConfig.exe", // Replace with actual path
                UseShellExecute = true
            });
        }
        catch
        {
            SystemStatus = "RSIConfig not found";
        }
    }

    [RelayCommand]
    private void ToggleMotionPause()
    {
        _motionControlService.MotionPaused = !_motionControlService.MotionPaused;
        MotionPaused = _motionControlService.MotionPaused;
    }

    [RelayCommand]
    private void MinimizeWindow()
    {
        if (_mainWindow != null)
        {
            _mainWindow.WindowState = WindowState.Minimized;
        }
    }

    [RelayCommand]
    private void MaximizeWindow()
    {
        if (_mainWindow != null)
        {
            _mainWindow.WindowState = _mainWindow.WindowState == WindowState.Maximized
                ? WindowState.Normal
                : WindowState.Maximized;
        }
    }

    [RelayCommand]
    private void CloseWindow()
    {
        _mainWindow?.Close();
    }

    public void Dispose()
    {
        _updateTimer?.Stop();
        _updateTimer?.Dispose();
        _rtTaskManagerService?.Dispose();
    }
}
