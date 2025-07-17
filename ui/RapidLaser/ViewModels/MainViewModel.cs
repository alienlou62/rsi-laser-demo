namespace RapidLaser.ViewModels;

public partial class MainViewModel : ViewModelBase, IDisposable
{
    private Window? _mainWindow;

    // SERVICES
    private readonly ICameraService _cameraService;
    private readonly IImageProcessingService _imageProcessingService;
    private readonly IConnectionManagerService _connectionManager;
    private IRmpGrpcService? _rmp = null;


    // FIELDS
    /// updater/polling
    private readonly System.Timers.Timer _updateTimer;

    /// globals
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

    [ObservableProperty]
    private double _cpuUsage = 23.0;

    [ObservableProperty]
    private double _memoryUsage = 145.0; // MB

    [ObservableProperty]
    private double _ballDetectionLoopTime = 33.0; // ms

    [ObservableProperty]
    private long _ballDetectionIterations = 45123;

    /// program
    [ObservableProperty]
    private bool _isProgramPaused = false;

    [ObservableProperty]
    private bool _isProgramRunning = false;

    [ObservableProperty]
    private double _motionControlLoopTime = 16.0; // ms

    /// Camera/Display Properties
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

    [ObservableProperty]
    private string _managerState = "Stopped";

    [ObservableProperty]
    private string _tasksActive = "0/2";

    [ObservableProperty]
    private string _ballDetectionStatus = "Inactive";

    [ObservableProperty]
    private string _motionControlStatus = "Inactive";

    // Display Properties
    public string IsProgramPausedDisplay => IsProgramPaused ? "Yes" : "No";

    // New property to disable connection inputs when connected
    public bool ConnectionInputsEnabled => !IsConnected;

    // mocks
    [ObservableProperty]
    private bool _isSimulatingBallPosition = false;


    // ===== COMMANDS ===== 
    // tasks
    [RelayCommand]
    private async Task StartTasks()
    {
        // Example: Start motion via RMP
        try
        {
            var result = await _rmp.StartMotionAsync();
            IsProgramRunning = result;
            SystemStatus = IsProgramRunning ? "TASKS STARTING" : "START ERROR";
        }
        catch (Exception ex)
        {
            SystemStatus = $"START ERROR: {ex.Message}";
        }
    }

    [RelayCommand]
    private async Task ShutdownTasks()
    {
        // Example: Stop motion via RMP
        try
        {
            var result = await _rmp.StopMotionAsync();
            IsProgramRunning = !result;
            SystemStatus = IsProgramRunning ? "TASKS STOPPED" : "SHUTDOWN ERROR";
        }
        catch (Exception ex)
        {
            SystemStatus = $"SHUTDOWN ERROR: {ex.Message}";
        }
    }

    [RelayCommand]
    private void ToggleMotionPause()
    {
        // Example: Toggle a global value for motion pause
        IsProgramPaused = !IsProgramPaused;
        _ = _rmp.SetGlobalValueAsync("IsProgramPaused", IsProgramPaused);
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

                // rmp service
                _rmp = _connectionManager.GrpcService;

                UpdateConnectionStatus();
            }
            else
            {
                // rmp service
                _rmp = null;
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

    private void UpdateConnectionStatus()
    {
        ConnectionStatus = IsConnected ? "Connected" : "Disconnected";
        OnPropertyChanged(nameof(ConnectionInputsEnabled));
    }

    partial void OnIsConnectedChanged(bool value)
    {
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


        // Initialize services (in real app, use DI)
        _cameraService = new SimulatedCameraService();
        _imageProcessingService = new SimulatedImageProcessingService();

        // Initialize connection settings
        IpAddress      = _connectionManager.Settings.IpAddress;
        Port           = _connectionManager.Settings.Port;
        UseMockService = _connectionManager.Settings.UseMockService;
        IsConnected    = _connectionManager.IsConnected;

        UpdateConnectionStatus();

        // Setup UI update timer
        _updateTimer = new System.Timers.Timer(100); // Update UI every 100ms
        _updateTimer.Elapsed += OnUpdateTimerElapsed;
        _updateTimer.Start();
    }


    // ===== POLL =====
    private async void OnUpdateTimerElapsed(object? sender, ElapsedEventArgs e)
    {
        if (!IsConnected) 
            return;

        try
        {
            if (!IsSimulatingBallPosition)
            {
                var globals = await _rmp.GetGlobalValuesAsync();
                if (globals.TryGetValue("BallX", out var ballX) && ballX is double)
                    BallXPosition = (double)ballX;
                if (globals.TryGetValue("BallY", out var ballY) && ballY is double)
                    BallYPosition = (double)ballY;
                if (globals.TryGetValue("BallVelocityX", out var velX) && velX is double)
                    BallVelocityX = (double)velX;
                if (globals.TryGetValue("BallVelocityY", out var velY) && velY is double)
                    BallVelocityY = (double)velY;
                if (globals.TryGetValue("DetectionConfidence", out var confidence) && confidence is double)
                    DetectionConfidence = (double)confidence;
            }
            else
            {

            }
            // Add more global values as needed
        }
        catch { }
    }


    // ===== METHODS ===== 
    public void Dispose()
    {
        _updateTimer?.Stop();
        _updateTimer?.Dispose();
    }

    public void SetMainWindow(Window window)
    {
        _mainWindow = window;
    }

}
