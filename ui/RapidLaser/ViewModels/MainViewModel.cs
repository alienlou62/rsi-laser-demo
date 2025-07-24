namespace RapidLaser.ViewModels;

public partial class MainViewModel : ViewModelBase, IDisposable
{
    /** UI **/
    private Window? _mainWindow;


    /** SERVICES **/
    private readonly ICameraService _cameraService;
    private readonly IImageProcessingService _imageProcessingService;
    private readonly IConnectionManagerService _connectionManager;
    private IRmpGrpcService? _rmp = null;


    /** FIELDS **/
    //polling
    private double _updateIntervalMs = 100;
    private readonly System.Timers.Timer _updateTimer;
    private readonly Random _random = new();

    //rmp
    [ObservableProperty]
    private MotionControllerStatus? _controllerStatus;

    [ObservableProperty]
    private NetworkStatus? _networkStatus;

    [ObservableProperty]
    private RTTaskManagerStatus? _taskManagerStatus;

    //globals
    [ObservableProperty]
    private ObservableCollection<string> _globalValueNames = new();

    [ObservableProperty]
    private string _global_BallX = string.Empty;

    [ObservableProperty]
    private string _global_BallY = string.Empty;

    [ObservableProperty]
    private string _global_BallRadius = string.Empty;

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

    //program
    [ObservableProperty]
    private bool _isProgramPaused = false;

    [ObservableProperty]
    private bool _isProgramRunning = false;

    [ObservableProperty]
    private double _motionControlLoopTime = 16.0; // ms

    //camera
    [ObservableProperty]
    private double _frameRate = 30.0;

    [ObservableProperty]
    private string _systemStatus = "SYSTEM ONLINE";

    [ObservableProperty]
    private int _binaryThreshold = 128;

    [ObservableProperty]
    private int _objectsDetected = 1;

    //server
    [ObservableProperty]
    private string _ipAddress = string.Empty;

    [ObservableProperty]
    private int _port;

    [ObservableProperty]
    private bool _isConnected = false;

    [ObservableProperty]
    private bool _useMockService = true; // Start with mock for testing

    [ObservableProperty]
    private bool _isConnecting = false;

    [ObservableProperty]
    private string _lastError = string.Empty;

    //state
    [ObservableProperty]
    private string _managerState = "Stopped";

    [ObservableProperty]
    private string _tasksActive = "0/2";

    [ObservableProperty]
    private string _ballDetectionStatus = "Inactive";

    [ObservableProperty]
    private string _motionControlStatus = "Inactive";

    //display
    [ObservableProperty]
    private string _isProgramPausedDisplay = string.Empty;

    //mocks
    [ObservableProperty]
    private bool _isSimulatingBallPosition = false;

    //ssh
    [ObservableProperty]
    private string _sshUser = "";

    [ObservableProperty]
    private string _sshPassword = "";

    [ObservableProperty]
    private string _sshCommand = "whoami";

    [ObservableProperty]
    private string _sshRunCommand = "";

    [ObservableProperty]
    private string _sshOutput = string.Empty;

    [ObservableProperty]
    private bool _isSshCommandRunning = false;

    [ObservableProperty]
    private string _sshStatus = "Ready";


    /** COMMANDS **/
    [RelayCommand]
    private async Task StartTasks()
    {
        try
        {
            var sshResult = await ExecuteSshCommandAsync(SshRunCommand, updateSshOutput: false);
            if (sshResult != null)
            {
                IsProgramRunning = true;
                SystemStatus = "TASKS STARTING VIA SSH";
            }
            else
            {
                SystemStatus = "SSH START ERROR";
            }
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
            if (_rmp != null)
            {
                var result = await _rmp.StopMotionAsync();
                IsProgramRunning = !result;
                SystemStatus = IsProgramRunning ? "SHUTDOWN ERROR" : "TASKS STOPPED";
            }
            else
            {
                SystemStatus = "RMP service not available";
            }

            // Alternative: Use SSH command execution
            // var sshResult = await ExecuteSshCommandAsync("sudo systemctl stop rapidcode-demo", updateSshOutput: false);
            // if (sshResult != null)
            // {
            //     IsProgramRunning = false;
            //     SystemStatus = "TASKS STOPPED VIA SSH";
            // }
            // else
            // {
            //     SystemStatus = "SSH SHUTDOWN ERROR";
            // }
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
    }

    //connection
    [RelayCommand]
    private async Task ConnectAsync()
    {
        // verify ip address is valid 
        if (!IsValidIpAddress(IpAddress))
        {
            LastError = "Invalid IP address.";
            return;
        }

        // verify port is valid
        if (Port <= 0)
        {
            LastError = "Invalid port.";
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
            }
            else
            {
                // rmp service
                _rmp = null;
                LastError = "Failed to connect to server";
            }
        }
        catch (Exception ex)
        {
            LastError = ex.Message;
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
    }

    partial void OnUseMockServiceChanged(bool value)
    {
        _connectionManager.SetMockMode(value);
    }

    //ssh
    [RelayCommand]
    private async Task RunSshCommandAsync()
    {
        if (IsSshCommandRunning) return;

        await ExecuteSshCommandAsync(SshCommand);
    }

    [RelayCommand]
    private void ClearSshOutput()
    {
        SshOutput = string.Empty;
        SshStatus = "Ready";
    }

    //window
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


    /** CONSTRUCTOR **/
    public MainViewModel()
    {
        StorageLoad();

        //services
        _connectionManager      = new ConnectionManagerService();
        _cameraService          = new SimulatedCameraService();
        _imageProcessingService = new SimulatedImageProcessingService();

        //connection
        UseMockService = _connectionManager.UseMockService;
        IsConnected    = _connectionManager.IsConnected;

        //polling
        _updateTimer = new System.Timers.Timer(_updateIntervalMs);
        _updateTimer.Elapsed += OnUpdateTimerElapsed;
        _updateTimer.Start();
    }


    /** POLLING **/
    private async void OnUpdateTimerElapsed(object? sender, ElapsedEventArgs e)
    {
        if (!IsConnected || _rmp == null)
            return;

        try
        {
            // Get controller status first
            ControllerStatus = await _rmp.GetControllerStatusAsync();
            NetworkStatus     = (ControllerStatus != null) ? await _rmp.GetNetworkStatusAsync() : null;
            TaskManagerStatus = (ControllerStatus != null) ? await _rmp.GetTaskManagerStatusAsync() : null;

            // ball positions
            if (!IsSimulatingBallPosition)
            {
                await UpdateRealBallPositionAsync();
            }
            else
            {
                UpdateSimulatedBallPosition();
            }
        }
        catch (Exception ex)
        {
            // Log the exception instead of swallowing it silently
            // _logger?.LogError(ex, "Error occurred during update timer polling");

            // Optionally set error state or notify user
            // HandlePollingError(ex);
        }
    }

    private async Task UpdateRealBallPositionAsync()
    {
        if (_rmp == null) return;

        // Update global value names if task manager status is available
        await UpdateGlobalValueNamesAsync();

        // Update ball position from global values
        UpdateBallPositionFromGlobals();
    }


    /** METHODS **/
    //globals
    private async Task UpdateGlobalValueNamesAsync()
    {
        if (TaskManagerStatus?.GlobalValues == null) return;

        var globalNames = TaskManagerStatus.GlobalValues.Keys.ToList();

        // Only update UI if names have actually changed
        if (!GlobalValueNames.SequenceEqual(globalNames))
        {
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                GlobalValueNames.Clear();
                foreach (var name in globalNames)
                {
                    GlobalValueNames.Add(name);
                }
            });
        }
    }

    private void UpdateBallPositionFromGlobals()
    {
        if (TaskManagerStatus?.GlobalValues == null) return;

        // Extract ball position values with helper method
        BallXPosition = GetDoubleValueFromGlobal(Global_BallX) ?? BallXPosition;
        BallYPosition = GetDoubleValueFromGlobal(Global_BallY) ?? BallYPosition;

        // Uncomment and use if needed:
        // BallVelocityX = GetDoubleValueFromGlobal("BallVelocityX") ?? BallVelocityX;
        // BallVelocityY = GetDoubleValueFromGlobal("BallVelocityY") ?? BallVelocityY;
        // DetectionConfidence = GetDoubleValueFromGlobal("DetectionConfidence") ?? DetectionConfidence;
    }

    private double? GetDoubleValueFromGlobal(string? globalName)
    {
        if (string.IsNullOrEmpty(globalName) ||
            TaskManagerStatus?.GlobalValues == null ||
            !TaskManagerStatus.GlobalValues.TryGetValue(globalName, out var value) ||
            value.ValueCase != FirmwareValue.ValueOneofCase.DoubleValue)
        {
            return null;
        }

        return value.DoubleValue;
    }

    //simulation
    private void UpdateSimulatedBallPosition()
    {
        // Constants for simulation
        const int BallRadius = 20;
        const int CanvasWidth = 620;
        const int CanvasHeight = 480;
        const double MaxVelocity = 12.5;
        const double MinConfidence = 85.0;
        const double MaxConfidence = 100.0;

        // Calculate valid position bounds
        var minPosition = BallRadius;
        var maxXPosition = CanvasWidth - BallRadius;
        var maxYPosition = CanvasHeight - BallRadius;

        // Generate random positions and velocities
        BallXPosition = minPosition + _random.NextDouble() * (maxXPosition - minPosition);
        BallYPosition = minPosition + _random.NextDouble() * (maxYPosition - minPosition);

        BallVelocityX = (_random.NextDouble() - 0.5) * 2 * MaxVelocity;
        BallVelocityY = (_random.NextDouble() - 0.5) * 2 * MaxVelocity;
        DetectionConfidence = MinConfidence + _random.NextDouble() * (MaxConfidence - MinConfidence);
    }

    //storage
    private void StorageLoad()
    {
        //storage
        var configPath = Path.Combine(AppContext.BaseDirectory, "RapidLaser.json");
        var settings = new ConfigurationBuilder()
                            .AddJsonFile(configPath)
                            .Build()
                            .GetSection("Settings");

        //server
        IpAddress         = settings["server_IpAddress"] ?? "localhost";
        Port              = int.TryParse(settings["server_Port"], out var port) ? port : 50061;
        var autoReconnect = bool.TryParse(settings["server_AutoReconnect"], out var reconnect) && reconnect;

        //polling
        _updateIntervalMs = int.TryParse(settings["polling_IntervalMs"], out var pollingInterval) ? pollingInterval : 100;

        //ssh
        SshUser           = settings["ssh_Username"] ?? "";
        SshPassword       = settings["ssh_Password"] ?? "";
        SshRunCommand     = settings["ssh_RunCommand"] ?? "";

        //globals
        Global_BallX      = settings["global_BallX"] ?? "";
        Global_BallY      = settings["global_BallY"] ?? "";
        Global_BallRadius = settings["global_BallRadius"] ?? "";
    }

    private void StorageSave()
    {
        try
        {
            // Create updated configuration object
            var configData = new
            {
                Settings = new
                {
                    server_ipAddress = IpAddress,
                    server_port = Port.ToString(),
                    //server_autoReconnect = AutoReconnect,
                    polling_IntervalMs = _updateIntervalMs,
                    ssh_Username = SshUser,
                    ssh_Password = SshPassword,
                    //globals
                    global_BallX = Global_BallX,
                    global_BallY = Global_BallY
                    //global_BallRadius = BallRadius // Uncomment if needed
                }
            };

            // Serialize to JSON with proper formatting
            var json = JsonSerializer.Serialize(configData, new JsonSerializerOptions
            {
                WriteIndented = true
            });

            var configPath = Path.Combine(AppContext.BaseDirectory, "RapidLaser.json");
            File.WriteAllText(configPath, json);
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Failed to save configuration: {ex.Message}");
        }
    }

    //ssh
    private async Task<string?> ExecuteSshCommandAsync(string command, bool updateSshOutput = true)
    {
        if (!IsConnected)
        {
            if (updateSshOutput) SshStatus = "Not connected to server";
            return null;
        }

        if (string.IsNullOrWhiteSpace(command))
        {
            if (updateSshOutput)
                SshStatus = "Please enter a command";
            return null;
        }

        if (string.IsNullOrWhiteSpace(SshUser) || string.IsNullOrWhiteSpace(SshPassword))
        {
            if (updateSshOutput)
                SshStatus = "Please enter SSH username and password";
            return null;
        }

        try
        {
            if (updateSshOutput)
            {
                IsSshCommandRunning = true;
                SshStatus = "Running command...";
                SshOutput = string.Empty;
            }

            var result = await _connectionManager.RunSshCommandAsync(command, SshUser, SshPassword);

            if (updateSshOutput)
            {
                SshOutput = result;
                SshStatus = "Command completed";
            }

            return result;
        }
        catch (Exception ex)
        {
            var errorMessage = $"Error: {ex.Message}";
            if (updateSshOutput)
            {
                SshOutput = errorMessage;
                SshStatus = "Command failed";
            }
            return null;
        }
        finally
        {
            if (updateSshOutput)
            {
                IsSshCommandRunning = false;
            }
        }
    }

    //window 
    public void Dispose()
    {
        _updateTimer?.Stop();
        _updateTimer?.Dispose();

        GC.SuppressFinalize(this);
    }

    public void SetMainWindow(Window window)
    {
        _mainWindow = window;
        _mainWindow.Closed += (s, e) =>
        {
            StorageSave();
            Dispose();
        };
    }

}
