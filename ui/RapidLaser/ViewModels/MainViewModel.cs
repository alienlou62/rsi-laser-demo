namespace RapidLaser.ViewModels;

public partial class MainViewModel : ViewModelBase, IDisposable
{
    private Window? _mainWindow;

    // CONFIGURATION
    private IConfigurationSection _settings;

    // SERVICES
    private readonly ICameraService _cameraService;
    private readonly IImageProcessingService _imageProcessingService;
    private readonly IConnectionManagerService _connectionManager;
    private IRmpGrpcService? _rmp = null;


    // FIELDS
    /// updater/polling
    private double _updateIntervalMs = 100;
    private readonly System.Timers.Timer _updateTimer;
    private readonly Random _random = new();

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
    private string _ipAddress;
    partial void OnIpAddressChanged(string value)
    {
        SaveSettingsToConfig();
    }

    [ObservableProperty]
    private int _port;
    partial void OnPortChanged(int value)
    {
        SaveSettingsToConfig();
    }

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

    //
    [ObservableProperty]
    private string _managerState = "Stopped";

    [ObservableProperty]
    private string _tasksActive = "0/2";

    [ObservableProperty]
    private string _ballDetectionStatus = "Inactive";

    [ObservableProperty]
    private string _motionControlStatus = "Inactive";

    // Display Properties
    [ObservableProperty]
    private string _isProgramPausedDisplay = string.Empty;

    // mocks
    [ObservableProperty]
    private bool _isSimulatingBallPosition = false;

    // SSH
    [ObservableProperty]
    private string _sshUser = "";

    [ObservableProperty]
    private string _sshPassword = "";

    [ObservableProperty]
    private string _sshCommand = "whoami";

    [ObservableProperty]
    private string _sshOutput = string.Empty;

    [ObservableProperty]
    private bool _isSshCommandRunning = false;

    [ObservableProperty]
    private string _sshStatus = "Ready";


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
        // verify ip address is valid 
        if (!IsValidIpAddress(IpAddress))
        {
            LastError = "Invalid IP address.";
            ConnectionStatus = "Connection Failed";
            return;
        }

        // verify port is valid
        if (Port <= 0)
        {
            LastError = "Invalid port.";
            ConnectionStatus = "Connection Failed";
            return;
        }

        if (IsConnecting) return;

        try
        {
            IsConnecting = true;
            LastError = string.Empty;

            // Update connection manager settings
            _connectionManager.UseMockService = UseMockService;

            var success = await _connectionManager.ConnectAsync(IpAddress, Port);

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

    private static bool IsValidIpAddress(string ipAddress)
    {
        if (string.IsNullOrWhiteSpace(ipAddress))
            return false;

        // Allow localhost
        if (string.Equals(ipAddress, "localhost", StringComparison.OrdinalIgnoreCase))
            return true;

        // Allow standard IP address formats
        return System.Net.IPAddress.TryParse(ipAddress, out _);
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
    }

    partial void OnUseMockServiceChanged(bool value)
    {
        _connectionManager.SetMockMode(value);
        UpdateConnectionStatus();
        SaveSettingsToConfig();
    }

    // SSH Commands
    [RelayCommand]
    private async Task RunSshCommandAsync()
    {
        if (IsSshCommandRunning) return;

        if (!IsConnected)
        {
            SshStatus = "Not connected to server";
            return;
        }

        if (string.IsNullOrWhiteSpace(SshCommand))
        {
            SshStatus = "Please enter a command";
            return;
        }

        if (string.IsNullOrWhiteSpace(SshUser) || string.IsNullOrWhiteSpace(SshPassword))
        {
            SshStatus = "Please enter SSH username and password";
            return;
        }

        try
        {
            IsSshCommandRunning = true;
            SshStatus = "Running command...";
            SshOutput = string.Empty;

            var result = await _connectionManager.RunSshCommandAsync(SshCommand, SshUser, SshPassword);

            SshOutput = result;
            SshStatus = "Command completed";
        }
        catch (Exception ex)
        {
            SshOutput = $"Error: {ex.Message}";
            SshStatus = "Command failed";
        }
        finally
        {
            IsSshCommandRunning = false;
        }
    }

    [RelayCommand]
    private void ClearSshOutput()
    {
        SshOutput = string.Empty;
        SshStatus = "Ready";
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
        // Load configuration from JSON file
        _settings = new ConfigurationBuilder()
            .AddJsonFile("RapidLaser.json")
            .Build()
            .GetSection("Settings");

        // Load settings and apply them
        LoadSettingsFromConfig();

        // Initialize connection manager
        _connectionManager = new ConnectionManagerService();

        // Initialize services (in real app, use DI)
        _cameraService = new SimulatedCameraService();
        _imageProcessingService = new SimulatedImageProcessingService();

        // Initialize connection settings
        UseMockService = _connectionManager.UseMockService;
        IsConnected = _connectionManager.IsConnected;

        UpdateConnectionStatus();

        // Setup UI update timer
        var updateInterval = int.Parse(_settings["pollingIntervalMs"] ?? "100");
        _updateTimer = new System.Timers.Timer(updateInterval); // Update UI every 100ms by default
        _updateTimer.Elapsed += OnUpdateTimerElapsed;
        _updateTimer.Start();
    }


    // ===== CONFIGURATION METHODS =====
    private void LoadSettingsFromConfig()
    {
        // Load settings from configuration with fallback defaults
        IpAddress = _settings["ipAddress"] ?? "localhost";
        Port = int.TryParse(_settings["port"], out var port) ? port : 50061;
        _updateIntervalMs = int.TryParse(_settings["pollingIntervalMs"], out var pollingInterval) ? pollingInterval : 100;
        SshUser = _settings["sshUsername"] ?? "";
        SshPassword = _settings["sshPassword"] ?? "";

        // autoReconnect can be stored if needed later
        // var autoReconnect = bool.TryParse(_settings["autoReconnect"], out var reconnect) && reconnect;
    }

    private void SaveSettingsToConfig()
    {
        try
        {
            // Create updated configuration object
            var configData = new
            {
                Settings = new
                {
                    ipAddress = IpAddress,
                    port = Port.ToString(),
                    autoReconnect = true.ToString().ToLower(),
                    pollingIntervalMs = _settings["pollingIntervalMs"] ?? "100",
                    sshUsername = _settings["sshUsername"] ?? "",
                    sshPassword = _settings["sshPassword"] ?? ""
                }
            };

            // Serialize to JSON with proper formatting
            var json = JsonSerializer.Serialize(configData, new JsonSerializerOptions
            {
                WriteIndented = true
            });

            // Write to file
            File.WriteAllText("RapidLaser.json", json);
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Failed to save configuration: {ex.Message}");
        }
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
                // Simulate random ball position within canvas bounds (accounting for ball radius)
                const int ballRadius = 20; // Half of 40px ball diameter
                const int canvasWidth = 620;
                const int canvasHeight = 480;

                BallXPosition = ballRadius + _random.NextDouble() * (canvasWidth - 2 * ballRadius); // Random X between 20-600
                BallYPosition = ballRadius + _random.NextDouble() * (canvasHeight - 2 * ballRadius); // Random Y between 20-460

                // Optional: Add some randomization to other simulation values
                BallVelocityX = (_random.NextDouble() - 0.5) * 25; // Random velocity between -12.5 and 12.5
                BallVelocityY = (_random.NextDouble() - 0.5) * 25; // Random velocity between -12.5 and 12.5
                DetectionConfidence = 85 + _random.NextDouble() * 15; // Random confidence between 85-100%
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
