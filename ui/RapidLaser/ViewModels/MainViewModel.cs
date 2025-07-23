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

    //storage
    private IConfigurationSection _settings;

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
    private string _userSelectedGlobalTargetX = string.Empty;

    [ObservableProperty]
    private string _userSelectedGlobalTargetY = string.Empty;

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
        //storage
        _settings = new ConfigurationBuilder()
            .AddJsonFile("RapidLaser.json")
            .Build()
            .GetSection("Settings");

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
        if (!IsConnected)
            return;

        try
        {
            //globals
            if (!IsSimulatingBallPosition)
            {
                //rmp status
                if (_rmp != null)
                {
                    ControllerStatus  = await _rmp.GetControllerStatusAsync();
                    NetworkStatus     = (ControllerStatus != null) ? await _rmp.GetNetworkStatusAsync() : null;
                    TaskManagerStatus = (ControllerStatus != null) ? await _rmp.GetTaskManagerStatusAsync() : null;

                    // Update global value names collection
                    if (TaskManagerStatus?.GlobalValues != null)
                    {
                        var globalNames = TaskManagerStatus.GlobalValues.Select(kv => kv.Key).ToList();

                        // Update the collection on UI thread if names have changed
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
                }

                if (TaskManagerStatus?.GlobalValues != null)
                {
                    // Ball X
                    if (!string.IsNullOrEmpty(UserSelectedGlobalTargetX) &&
                        TaskManagerStatus.GlobalValues.TryGetValue(UserSelectedGlobalTargetX, out var targetX) &&
                        targetX.ValueCase == FirmwareValue.ValueOneofCase.DoubleValue)
                        BallXPosition = targetX.DoubleValue;

                    // Ball Y
                    if (!string.IsNullOrEmpty(UserSelectedGlobalTargetY) &&
                        TaskManagerStatus.GlobalValues.TryGetValue(UserSelectedGlobalTargetY, out var targetY) &&
                        targetY.ValueCase == FirmwareValue.ValueOneofCase.DoubleValue)
                        BallYPosition = targetY.DoubleValue;

                    // Handle other globals
                    // if (TaskManagerStatus.GlobalValues.TryGetValue("BallVelocityX", out var velX) &&
                    //     velX.ValueCase == FirmwareValue.ValueOneofCase.DoubleValue)
                    //     BallVelocityX = velX.DoubleValue;
                    // if (TaskManagerStatus.GlobalValues.TryGetValue("BallVelocityY", out var velY) &&
                    //     velY.ValueCase == FirmwareValue.ValueOneofCase.DoubleValue)
                    //     BallVelocityY = velY.DoubleValue;
                    // if (TaskManagerStatus.GlobalValues.TryGetValue("DetectionConfidence", out var confidence) &&
                    //     confidence.ValueCase == FirmwareValue.ValueOneofCase.DoubleValue)
                    //     DetectionConfidence = confidence.DoubleValue;
                }
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
        }
        catch
        {
        }
    }


    /** METHODS **/
    //storage
    private void StorageLoad()
    {
        //server
        IpAddress         = _settings["server_IpAddress"] ?? "localhost";
        Port              = int.TryParse(_settings["server_Port"], out var port) ? port : 50061;
        var autoReconnect = bool.TryParse(_settings["server_AutoReconnect"], out var reconnect) && reconnect;

        //polling
        _updateIntervalMs = int.TryParse(_settings["polling_IntervalMs"], out var pollingInterval) ? pollingInterval : 100;

        //ssh
        SshUser           = _settings["ssh_Username"] ?? "";
        SshPassword       = _settings["ssh_Password"] ?? "";
        SshRunCommand     = _settings["ssh_RunCommand"] ?? "";
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
                    ssh_Password = SshPassword
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
