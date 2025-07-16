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
    private readonly IConnectionManagerService _connectionManager;
    private readonly GlobalValueService? _globalValueService;

    // Timer for UI updates
    private readonly System.Timers.Timer _updateTimer;

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

    // Connection Properties
    [ObservableProperty]
    private string _ipAddress = "localhost";

    [ObservableProperty]
    private int _port = 50061;

    [ObservableProperty]
    private bool _isConnected = false;

    [ObservableProperty]
    private bool _useMockService = true; // Start with mock for testing

    [ObservableProperty]
    private bool _isConnecting = false;

    [ObservableProperty]
    private string _connectionStatus = "Disconnected";

    [ObservableProperty]
    private string _lastError = string.Empty;

    // Display Properties
    public string MotionPausedDisplay => MotionPaused ? "Yes" : "No";
    public string MotionToggleButtonText => MotionPaused ? "Resume Motion" : "Pause Motion";
    public string MotionToggleButtonColor => MotionPaused ? "#4CAF50" : "#FF6B35";

    // New property to disable connection inputs when connected
    public bool ConnectionInputsEnabled => !IsConnected;


    // ===== COMMANDS ===== 
    // tasks
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

    // connection
    [RelayCommand]
    private async Task ConnectAsync()
    {
        if (IsConnecting) return;

        try
        {
            IsConnecting = true;
            LastError = string.Empty;

            // Update connection manager settings
            _connectionManager.Settings.IpAddress = IpAddress;
            _connectionManager.Settings.Port = Port;
            _connectionManager.Settings.UseMockService = UseMockService;

            var success = await _connectionManager.ConnectAsync();

            if (success)
            {
                IsConnected = true;
                UpdateConnectionStatus(); // Use the simplified status

                // Start global value polling if available
                _globalValueService?.StartPolling(TimeSpan.FromSeconds(1));
            }
            else
            {
                LastError = "Failed to connect to server";
                ConnectionStatus = "Connection Failed";
            }
        }
        catch (Exception ex)
        {
            LastError = ex.Message;
            ConnectionStatus = "Connection Error";
        }
        finally
        {
            IsConnecting = false;
        }
    }

    [RelayCommand]
    private async Task DisconnectAsync()
    {
        try
        {
            // Stop global value polling
            _globalValueService?.StopPolling();

            await _connectionManager.DisconnectAsync();
            IsConnected = false;
            UpdateConnectionStatus();
        }
        catch (Exception ex)
        {
            LastError = ex.Message;
        }
    }

    [RelayCommand]
    private void ToggleMockService()
    {
        _connectionManager.SetMockMode(UseMockService);
        UpdateConnectionStatus();
    }

    [RelayCommand]
    private async Task TestConnectionAsync()
    {
        if (!IsConnected) return;

        try
        {
            // Test by getting a simple global value
            var testValue = await _connectionManager.GrpcService.GetGlobalValueAsync<string>("SystemStatus");
            LastError = $"Test successful. SystemStatus: {testValue ?? "N/A"}";
        }
        catch (Exception ex)
        {
            LastError = $"Test failed: {ex.Message}";
        }
    }

    private void UpdateConnectionStatus()
    {
        if (IsConnected)
        {
            ConnectionStatus = "Connected";
        }
        else
        {
            ConnectionStatus = "Disconnected";
        }

        // Notify that ConnectionInputsEnabled has changed
        OnPropertyChanged(nameof(ConnectionInputsEnabled));
    }

    partial void OnIsConnectedChanged(bool value)
    {
        // Update connection inputs enabled state when connection status changes
        OnPropertyChanged(nameof(ConnectionInputsEnabled));
    }

    partial void OnUseMockServiceChanged(bool value)
    {
        _connectionManager.SetMockMode(value);
        UpdateConnectionStatus();
    }

    partial void OnIpAddressChanged(string value)
    {
        if (_connectionManager?.Settings != null)
        {
            _connectionManager.Settings.IpAddress = value;
        }
    }

    partial void OnPortChanged(int value)
    {
        if (_connectionManager?.Settings != null)
        {
            _connectionManager.Settings.Port = value;
        }
    }

    // window
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


    // ===== CONSTRUCTOR ===== 
    public MainViewModel()
    {
        // Initialize connection manager
        _connectionManager = new ConnectionManagerService();

        // Initialize global value service
        _globalValueService = new GlobalValueService(_connectionManager);

        // Initialize services (in real app, use DI)
        _cameraService = new SimulatedCameraService();
        _imageProcessingService = new SimulatedImageProcessingService();
        _motionControlService = new SimulatedMotionControlService();
        _rtTaskManagerService = new SimulatedRTTaskManagerService(
            _cameraService, _imageProcessingService, _motionControlService);

        // Subscribe to service events
        _rtTaskManagerService.GlobalsUpdated += OnGlobalsUpdated;
        _rtTaskManagerService.TaskStatusChanged += OnTaskStatusChanged;

        // Subscribe to global value service events
        _globalValueService.GlobalValuesUpdated += OnGlobalValuesUpdated;

        // Initialize connection settings
        IpAddress = _connectionManager.Settings.IpAddress;
        Port = _connectionManager.Settings.Port;
        UseMockService = _connectionManager.Settings.UseMockService;
        IsConnected = _connectionManager.IsConnected;

        UpdateConnectionStatus();

        // Setup UI update timer
        _updateTimer = new System.Timers.Timer(100); // Update UI every 100ms
        _updateTimer.Elapsed += OnUpdateTimerElapsed;
        _updateTimer.Start();
    }


    // ===== POLL =====
    private void OnUpdateTimerElapsed(object? sender, ElapsedEventArgs e)
    {
        // Update UI properties that change continuously
        if (_rtTaskManagerService.IsRunning)
        {
            CpuUsage                = _rtTaskManagerService.CpuUsage;
            MemoryUsage             = _rtTaskManagerService.MemoryUsage;
            BallDetectionLoopTime   = _rtTaskManagerService.BallDetectionLoopTime;
            BallDetectionIterations = _rtTaskManagerService.BallDetectionIterations;
            MotionControlLoopTime   = _rtTaskManagerService.MotionControlLoopTime;

            // Update motion positions
            BallXPosition = _motionControlService.CurrentX;
            BallYPosition = _motionControlService.CurrentY;

            // Update motion paused status
            MotionPaused = _motionControlService.MotionPaused;
        }
    }


    // ===== METHODS ===== 
    public void Dispose()
    {
        _updateTimer?.Stop();
        _updateTimer?.Dispose();
        _globalValueService?.StopPolling();
        if (_globalValueService != null)
        {
            _globalValueService.GlobalValuesUpdated -= OnGlobalValuesUpdated;
        }
        _rtTaskManagerService?.Dispose();
    }

    public void SetMainWindow(Window window)
    {
        _mainWindow = window;
    }


    // ===== EVENTS ===== 
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

    private void OnGlobalValuesUpdated(object? sender, GlobalValuesUpdatedEventArgs e)
    {
        // Update ball position from gRPC global values when connected
        if (e.Values.TryGetValue("BallX", out var ballX) && ballX is double)
        {
            BallXPosition = (double)ballX;
        }

        if (e.Values.TryGetValue("BallY", out var ballY) && ballY is double)
        {
            BallYPosition = (double)ballY;
        }

        if (e.Values.TryGetValue("BallVelocityX", out var velX) && velX is double)
        {
            BallVelocityX = (double)velX;
        }

        if (e.Values.TryGetValue("BallVelocityY", out var velY) && velY is double)
        {
            BallVelocityY = (double)velY;
        }

        if (e.Values.TryGetValue("DetectionConfidence", out var confidence) && confidence is double)
        {
            DetectionConfidence = (double)confidence;
        }
    }

}
