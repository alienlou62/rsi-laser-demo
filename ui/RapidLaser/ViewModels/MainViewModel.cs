namespace RapidLaser.ViewModels;

public partial class GlobalValueItem : ObservableObject
{
    [ObservableProperty]
    private string _name = string.Empty;

    [ObservableProperty]
    private string _value = string.Empty;

    [ObservableProperty]
    private bool _isNumeric = true;

    public override string ToString() => Name;
}

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
    private ObservableCollection<GlobalValueItem> _globalValues = [];
    partial void OnGlobalValuesChanged(ObservableCollection<GlobalValueItem> value)
    {
        OnGlobal_BallXChanged(Global_BallX);
        OnGlobal_BallYChanged(Global_BallY);
        OnGlobal_BallRadiusChanged(Global_BallRadius);
        OnGlobal_IsMotionEnabledChanged(Global_IsMotionEnabled);
    }

    //global names
    [ObservableProperty]
    private string? _global_BallX;
    partial void OnGlobal_BallXChanged(string? value)
    {
        Global_BallXValid = GlobalValues.Any(g => string.Equals(g.Name, value, StringComparison.OrdinalIgnoreCase));
    }

    [ObservableProperty]
    private bool _global_BallXValid;

    [ObservableProperty]
    private string? _global_BallY;
    partial void OnGlobal_BallYChanged(string? value)
    {
        Global_BallYValid = GlobalValues.Any(g => string.Equals(g.Name, value, StringComparison.OrdinalIgnoreCase));
    }

    [ObservableProperty]
    private bool _global_BallYValid;

    [ObservableProperty]
    private string? _global_BallRadius;
    partial void OnGlobal_BallRadiusChanged(string? value)
    {
        Global_BallRadiusValid = GlobalValues.Any(g => string.Equals(g.Name, value, StringComparison.OrdinalIgnoreCase));
    }

    [ObservableProperty]
    private bool _global_BallRadiusValid;

    [ObservableProperty]
    private string? _global_IsMotionEnabled;
    partial void OnGlobal_IsMotionEnabledChanged(string? value)
    {
        Global_IsMotionEnabledValid = GlobalValues.Any(g => string.Equals(g.Name, value, StringComparison.OrdinalIgnoreCase));
    }

    [ObservableProperty]
    private bool _global_IsMotionEnabledValid;

    //program maps (these hold global values)
    [ObservableProperty]
    private double? _program_BallX = 320.0; // Default center position

    [ObservableProperty]
    private double? _program_BallY = 240.0; // Default center position

    [ObservableProperty]
    private double? _program_BallRadius = 10.0; // Default radius

    [ObservableProperty]
    private bool? _program_IsMotionEnabled;

    [ObservableProperty]
    private double _detectionConfidence = 95.0;

    //program
    [ObservableProperty]
    private bool _isProgramPaused = false;

    [ObservableProperty]
    private bool _isProgramRunning = false;

    [ObservableProperty]
    private string _programStatus = "";

    //camera
    [ObservableProperty]
    private double _frameRate = 30.0;

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
    partial void OnSshUserChanged(string value)
    {
        IsSshAuthenticated = false;
    }

    [ObservableProperty]
    private string _sshPassword = "";
    partial void OnSshPasswordChanged(string value)
    {
        IsSshAuthenticated = false;
    }

    [ObservableProperty]
    private string _sshCommand = "whoami";

    [ObservableProperty]
    private string _sshRunCommand = "";

    [ObservableProperty]
    private bool _isSshCommandRunning = false;

    [ObservableProperty]
    private bool _isSshAuthenticated = false;

    //logging
    [ObservableProperty]
    private string _logOutput = string.Empty;


    /** COMMANDS **/
    //program
    [RelayCommand]
    private async Task ProgramRunAsync()
    {
        try
        {
            LogMessage($"Executing SSH command: {SshRunCommand}");
            var sshResult = await ExecuteSshCommandAsync(SshRunCommand, updateSshOutput: true);

            if (sshResult == null)
            {
                ProgramStatus = "SSH ERROR";
                LogMessage("Program run failed: SSH command returned null");
            }
            else
            {
                ProgramStatus = "";
                LogMessage("Program run completed successfully");
            }
        }
        catch (Exception ex)
        {
            ProgramStatus = $"SSH ERROR: {ex.Message}";
            LogMessage($"Program Run Error: {ex.Message}");
        }
    }

    [RelayCommand]
    private async Task ProgramStopAsync()
    {
        try
        {
            if (_rmp != null)
            {
                LogMessage("Stopping task manager...");
                var result = await _rmp.StopTaskManagerAsync();
            }
            else
            {
                ProgramStatus = "RMP service not available";
                LogMessage("Program stop failed: RMP service not available");
            }
        }
        catch (Exception ex)
        {
            ProgramStatus = $"SHUTDOWN ERROR: {ex.Message}";
            LogMessage($"Program Stop Error: {ex.Message}");
        }
    }

    [RelayCommand]
    private void ToggleMotionPause()
    {
        // Example: Toggle a global value for motion pause
        IsProgramPaused = !IsProgramPaused;
    }

    [RelayCommand]
    private async Task ToggleProgramMotionEnabledAsync(object isMotionEnabled)
    {
        try
        {
            if (string.IsNullOrEmpty(Global_IsMotionEnabled) || _rmp == null)
                return;

            var response = await _rmp.SetBoolGlobalValueAsync(Global_IsMotionEnabled, (bool)isMotionEnabled);
            LogMessage($"Motion enabled set to: {(bool)isMotionEnabled}");
        }
        catch (Exception ex)
        {
            LogMessage($"Motion Control Error: {ex.Message}");
        }

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

                // Test SSH authentication after successful server connection
                await TestSshAuthenticationAsync();
            }
            else
            {
                // rmp service
                _rmp = null;
                LastError = "Failed to connect to server";
                LogMessage("Connection failed: Failed to connect to server");
                IsSshAuthenticated = false;
            }
        }
        catch (Exception ex)
        {
            LastError = ex.Message;
            LogMessage($"Connection Error: {ex.Message}");
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
            IsSshAuthenticated = false;
        }
        catch (Exception ex)
        {
            LastError = ex.Message;
            LogMessage($"Disconnect Error: {ex.Message}");
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
    private void ClearLogOutput()
    {
        LogOutput = string.Empty;
    }

    [RelayCommand]
    private async Task TestSshConnectionAsync()
    {
        if (!IsConnected)
        {
            LogOutput += $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] SSH: Not connected to server\n";
            return;
        }

        await TestSshAuthenticationAsync();

        if (IsSshAuthenticated)
        {
            LogOutput += $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] SSH: Authentication successful\n";
        }
        else
        {
            LogOutput += $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] SSH: Authentication failed\n";
        }
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
    private readonly SemaphoreSlim _pollingSemaphore = new(1, 1);

    private async void OnUpdateTimerElapsed(object? sender, ElapsedEventArgs e)
    {
        // skip if already running
        if (!await _pollingSemaphore.WaitAsync(0))
            return;

        try
        {
            // checks
            if (!IsConnected || _rmp == null)
                return;

            //controller status
            try { ControllerStatus = await _rmp.GetControllerStatusAsync(); }
            catch { ControllerStatus = null; }

            //network status
            try { NetworkStatus = (ControllerStatus != null) ? await _rmp.GetNetworkStatusAsync() : null; }
            catch { NetworkStatus = null; }

            // ball positions
            if (!IsSimulatingBallPosition)
            {
                //tm status
                try
                {
                    TaskManagerStatus = (ControllerStatus != null) ? await _rmp.GetTaskManagerStatusAsync() : null;

                    IsProgramRunning = TaskManagerStatus != null && TaskManagerStatus.State == RTTaskManagerState.Running;
                }
                catch { TaskManagerStatus = null; }

                //tm globals
                if (TaskManagerStatus != null)
                    await UpdateGlobalValues();
            }
            else
            {
                UpdateGlobalValuesWithFakeData();
            }
        }
        catch (Exception ex)
        {
            LogMessage($"Polling Error: {ex.Message}");
        }
        finally
        {
            _pollingSemaphore.Release();
        }
    }


    /** METHODS **/
    //logging
    private void LogMessage(string message)
    {
        var timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
        LogOutput += $"[{timestamp}] {message}\n";
    }

    //globals
    private async Task UpdateGlobalValues()
    {
        if (_rmp == null) return;

        // Update global value names if task manager status is available
        await UpdateGlobalValuesAsync();

        // Update ball position from global values
        UpdateBallPositionFromGlobals();
    }

    private async Task UpdateGlobalValuesAsync()
    {
        if (TaskManagerStatus?.GlobalValues == null) return;

        var globalNames = TaskManagerStatus.GlobalValues.Keys.ToHashSet();
        var currentNames = GlobalValues.Select(x => x.Name).ToHashSet();

        // Only rebuild if names actually changed (added/removed globals)
        var namesChanged = !globalNames.SetEquals(currentNames);

        await Dispatcher.UIThread.InvokeAsync(() =>
        {
            if (namesChanged)
            {
                // Handle added globals
                var newGlobals = globalNames.Except(currentNames).ToList();
                foreach (var name in newGlobals)
                {
                    var valueItem = new GlobalValueItem
                    {
                        Name = name
                    };

                    if (TaskManagerStatus.GlobalValues.TryGetValue(name, out var globalValue))
                    {
                        valueItem.Value = FormatGlobalValue(globalValue);
                        valueItem.IsNumeric = globalValue.ValueCase != FirmwareValue.ValueOneofCase.BoolValue;
                    }

                    GlobalValues.Add(valueItem);
                }

                // Handle removed globals
                var removedGlobals = currentNames.Except(globalNames).ToList();
                for (int i = GlobalValues.Count - 1; i >= 0; i--)
                {
                    if (removedGlobals.Contains(GlobalValues[i].Name))
                    {
                        GlobalValues.RemoveAt(i);
                    }
                }

                // Sort after any add/remove
                SortGlobalValues();

                // Notify that globalValues have changed
                OnGlobalValuesChanged(GlobalValues);
            }

            // Always update values for existing items (this is the common case)
            foreach (var item in GlobalValues)
            {
                if (TaskManagerStatus.GlobalValues.TryGetValue(item.Name, out var globalValue))
                {
                    var newValue = FormatGlobalValue(globalValue);
                    if (item.Value != newValue)
                    {
                        item.Value = newValue;
                    }
                }
            }
        });
    }

    private void SortGlobalValues()
    {
        var sorted = GlobalValues.OrderBy(x => x.Name, StringComparer.OrdinalIgnoreCase).ToList();
        for (int i = 0; i < sorted.Count; i++)
        {
            if (!ReferenceEquals(GlobalValues[i], sorted[i]))
            {
                GlobalValues.Move(GlobalValues.IndexOf(sorted[i]), i);
            }
        }
    }

    private void UpdateBallPositionFromGlobals()
    {
        if (TaskManagerStatus?.GlobalValues == null) return;

        // Extract ball position values with helper method
        Program_BallX           = GetDoubleValueFromGlobal(Global_BallX) ?? Program_BallX;
        Program_BallY           = GetDoubleValueFromGlobal(Global_BallY) ?? Program_BallY;
        Program_BallRadius      = GetDoubleValueFromGlobal(Global_BallRadius) ?? Program_BallRadius;
        Program_IsMotionEnabled = GetBooleanValueFromGlobal(Global_IsMotionEnabled) ?? Program_IsMotionEnabled;

        // Uncomment and use if needed:
        // DetectionConfidence = GetDoubleValueFromGlobal("DetectionConfidence") ?? DetectionConfidence;
    }

    private double? GetDoubleValueFromGlobal(string? globalName)
    {
        if (string.IsNullOrEmpty(globalName) ||
            TaskManagerStatus?.GlobalValues == null ||
            !TaskManagerStatus.GlobalValues.TryGetValue(globalName, out var value))
        {
            return null;
        }

        // Handle different numeric value types that can be converted to double
        return value.ValueCase switch
        {
            FirmwareValue.ValueOneofCase.DoubleValue => value.DoubleValue,
            FirmwareValue.ValueOneofCase.FloatValue => (double)value.FloatValue,
            FirmwareValue.ValueOneofCase.Int32Value => (double)value.Int32Value,
            FirmwareValue.ValueOneofCase.Uint32Value => (double)value.Uint32Value,
            FirmwareValue.ValueOneofCase.Int64Value => (double)value.Int64Value,
            FirmwareValue.ValueOneofCase.Uint64Value => (double)value.Uint64Value,
            FirmwareValue.ValueOneofCase.Int16Value => (double)value.Int16Value,
            FirmwareValue.ValueOneofCase.Uint16Value => (double)value.Uint16Value,
            FirmwareValue.ValueOneofCase.Int8Value => (double)value.Int8Value,
            FirmwareValue.ValueOneofCase.Uint8Value => (double)value.Uint8Value,
            _ => null // Cannot convert to double
        };
    }

    private bool? GetBooleanValueFromGlobal(string? globalName)
    {
        if (string.IsNullOrEmpty(globalName) ||
            TaskManagerStatus?.GlobalValues == null ||
            !TaskManagerStatus.GlobalValues.TryGetValue(globalName, out var value))
        {
            return null;
        }

        // Handle boolean and numeric value types: 0 => false, 1 => true, else null
        return value.ValueCase switch
        {
            FirmwareValue.ValueOneofCase.BoolValue => value.BoolValue,
            FirmwareValue.ValueOneofCase.Int8Value => value.Int8Value == 0 ? false : value.Int8Value == 1 ? true : null,
            FirmwareValue.ValueOneofCase.Uint8Value => value.Uint8Value == 0 ? false : value.Uint8Value == 1 ? true : null,
            FirmwareValue.ValueOneofCase.Int16Value => value.Int16Value == 0 ? false : value.Int16Value == 1 ? true : null,
            FirmwareValue.ValueOneofCase.Uint16Value => value.Uint16Value == 0 ? false : value.Uint16Value == 1 ? true : null,
            FirmwareValue.ValueOneofCase.Int32Value => value.Int32Value == 0 ? false : value.Int32Value == 1 ? true : null,
            FirmwareValue.ValueOneofCase.Uint32Value => value.Uint32Value == 0 ? false : value.Uint32Value == 1 ? true : null,
            FirmwareValue.ValueOneofCase.Int64Value => value.Int64Value == 0 ? false : value.Int64Value == 1 ? true : null,
            FirmwareValue.ValueOneofCase.Uint64Value => value.Uint64Value == 0 ? false : value.Uint64Value == 1 ? true : null,
            FirmwareValue.ValueOneofCase.FloatValue => value.FloatValue == 0f ? false : value.FloatValue == 1f ? true : null,
            FirmwareValue.ValueOneofCase.DoubleValue => value.DoubleValue == 0.0 ? false : value.DoubleValue == 1.0 ? true : null,
            _ => null // Cannot convert to bool
        };
    }

    private string FormatGlobalValue(FirmwareValue value)
    {
        return value.ValueCase switch
        {
            FirmwareValue.ValueOneofCase.None => "None",
            FirmwareValue.ValueOneofCase.BoolValue => value.BoolValue.ToString(),
            FirmwareValue.ValueOneofCase.Int8Value => value.Int8Value.ToString("N0"),
            FirmwareValue.ValueOneofCase.Uint8Value => value.Uint8Value.ToString("N0"),
            FirmwareValue.ValueOneofCase.Int16Value => value.Int16Value.ToString("N0"),
            FirmwareValue.ValueOneofCase.Uint16Value => value.Uint16Value.ToString("N0"),
            FirmwareValue.ValueOneofCase.Int32Value => value.Int32Value.ToString("N0"),
            FirmwareValue.ValueOneofCase.Uint32Value => value.Uint32Value.ToString("N0"),
            FirmwareValue.ValueOneofCase.FloatValue => value.FloatValue.ToString("N3"),
            FirmwareValue.ValueOneofCase.DoubleValue => value.DoubleValue.ToString("N3"),
            FirmwareValue.ValueOneofCase.Int64Value => value.Int64Value.ToString("N0"),
            FirmwareValue.ValueOneofCase.Uint64Value => value.Uint64Value.ToString("N0"),
            _ => value.ToString() // Fallback to string representation
        };
    }

    //simulation
    private void UpdateGlobalValuesWithFakeData()
    {
        // Constants for simulation
        const int CanvasWidth = 640;
        const int CanvasHeight = 480;
        const double MinConfidence = 85.0;
        const double MaxConfidence = 100.0;

        // Generate random ball radius between 10 and 50
        var ballRadius = 10 + _random.NextDouble() * 40; // Random between 10 and 50

        // Calculate valid position bounds  
        var minPosition = ballRadius;
        var maxXPosition = CanvasWidth - ballRadius;
        var maxYPosition = CanvasHeight - ballRadius;

        // Generate random positions and velocities
        Program_BallX = minPosition + _random.NextDouble() * (maxXPosition - minPosition);
        Program_BallY = minPosition + _random.NextDouble() * (maxYPosition - minPosition);
        Program_BallRadius = ballRadius; // Random radius for simulation

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
        Global_IsMotionEnabled = settings["global_IsMotionEnabled"] ?? "";
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
                    ssh_RunCommand = SshRunCommand,

                    //globals
                    global_BallX = Global_BallX,
                    global_BallY = Global_BallY,
                    global_BallRadius = Global_BallRadius,
                    global_IsMotionEnabled = Global_IsMotionEnabled
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
    private async Task TestSshAuthenticationAsync()
    {
        // Reset authentication status
        IsSshAuthenticated = false;

        // Skip if credentials are not provided
        if (string.IsNullOrWhiteSpace(SshUser) || string.IsNullOrWhiteSpace(SshPassword))
        {
            return;
        }

        try
        {
            // Test SSH connection with a simple command that should work on most systems
            var result = await ExecuteSshCommandAsync("whoami", updateSshOutput: false);

            // If we get a result (not null), SSH authentication was successful
            IsSshAuthenticated = !string.IsNullOrWhiteSpace(result);
        }
        catch
        {
            // Any exception means authentication failed
            IsSshAuthenticated = false;
        }
    }

    private async Task<string?> ExecuteSshCommandAsync(string command, bool updateSshOutput = true)
    {
        if (!IsConnected)
        {
            if (updateSshOutput) LogMessage("SSH: Not connected to server");
            return null;
        }

        if (string.IsNullOrWhiteSpace(command))
        {
            if (updateSshOutput)
                LogMessage("SSH: Please enter a command");
            return null;
        }

        if (string.IsNullOrWhiteSpace(SshUser) || string.IsNullOrWhiteSpace(SshPassword))
        {
            if (updateSshOutput)
                LogMessage("SSH: Please enter SSH username and password");
            return null;
        }

        try
        {
            if (updateSshOutput)
            {
                IsSshCommandRunning = true;
                LogMessage($"SSH: Running command '{command}'...");
            }

            var result = await _connectionManager.RunSshCommandAsync(command, SshUser, SshPassword);

            if (updateSshOutput)
            {
                LogMessage($"SSH Command Output:\n{result}");
                LogMessage("SSH: Command completed");
            }

            return result;
        }
        catch (Exception ex)
        {
            var errorMessage = $"SSH Error: {ex.Message}";
            if (updateSshOutput)
            {
                LogMessage(errorMessage);
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
