namespace RapidLaser.Services;

public interface IConnectionManagerService
{
    bool IsConnected { get; }
    bool IsRunning { get; }
    bool UseMockService { get; set; }

    ICameraService CameraService { get; }
    IRmpGrpcService GrpcService { get; }

    Task<bool> ConnectAsync(string ipAddress, int port);
    Task DisconnectAsync();
    void SetMockMode(bool useMock);
    Task<string> RunSshCommandAsync(string command, string sshUser, string sshPass);
}

public partial class ConnectionManagerService : ObservableObject, IConnectionManagerService
{
    // connection
    private string _latestIpAddress = string.Empty;
    private int _latestPort;

    [ObservableProperty]
    private bool _useMockService = false;

    [ObservableProperty]
    private bool _isConnected = false;

    [ObservableProperty]
    private bool _isRunning = false;

    [ObservableProperty]
    private ICameraService _cameraService;

    [ObservableProperty]
    private IRmpGrpcService _grpcService;

    private readonly HttpCameraService _httpCameraService;
    private readonly RmpGrpcService _realGrpcService;
    private readonly RmpGrpcService_Mock _mockGrpcService;

    // CONSTRUCTOR
    public ConnectionManagerService()
    {
        _httpCameraService = new HttpCameraService("http://localhost:8080");
        _realGrpcService = new RmpGrpcService();
        _mockGrpcService = new RmpGrpcService_Mock();

        _cameraService = _httpCameraService; // Always use HTTP camera service
        _grpcService = _mockGrpcService; // Start with mock gRPC service
    }

    // METHODS
    public async Task<bool> ConnectAsync(string ip, int port)
    {
        _latestIpAddress = ip;
        _latestPort = port;

        try
        {
            if (UseMockService)
            {
                // Use mock gRPC service only
                GrpcService = _mockGrpcService;
                IsConnected = true;
                IsRunning = true;
                return true;
            }
            else
            {
                // Only connect gRPC service (to the specified IP/port)
                var serverAddress = $"{_latestIpAddress}:{_latestPort}";
                var grpcConnected = await _realGrpcService.ConnectAsync(serverAddress);

                if (grpcConnected)
                {
                    GrpcService = _realGrpcService;
                    IsConnected = true;
                    IsRunning = true;
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }
        catch (Exception)
        {
            IsConnected = false;
            IsRunning = false;
            return false;
        }
    }

    public async Task DisconnectAsync()
    {
        if (GrpcService != null)
        {
            await GrpcService.DisconnectAsync();
        }

        IsConnected = false;
        IsRunning = false;
    }

    public void SetMockMode(bool useMock)
    {
        UseMockService = useMock;

        if (useMock)
        {
            GrpcService = _mockGrpcService;
        }
    }

    /// ssh
    public async Task<string> RunSshCommandAsync(string command, string sshUser, string sshPass)
    {
        if (!IsConnected)
        {
            throw new InvalidOperationException("Not connected to server. Please connect first.");
        }

        if (string.IsNullOrWhiteSpace(command))
        {
            throw new ArgumentException("Command cannot be null or empty.", nameof(command));
        }

        try
        {
            using var client = new SshClient(_latestIpAddress, sshUser, sshPass);

            // Set connection timeout
            client.ConnectionInfo.Timeout = TimeSpan.FromSeconds(30);

            await Task.Run(client.Connect);

            if (!client.IsConnected)
            {
                throw new InvalidOperationException("Failed to establish SSH connection.");
            }

            // Wrap the command in a login shell to ensure environment is loaded
            var wrappedCommand = $"bash -l -c \"{command.Replace("\"", "\\\"")}\"";
            var sshCommand = client.CreateCommand(wrappedCommand);
            sshCommand.CommandTimeout = TimeSpan.FromSeconds(30);

            sshCommand.Execute();
            var result = string.IsNullOrEmpty(sshCommand.Error) ? sshCommand.Result : sshCommand.Error;

            client.Disconnect();

            return result;
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException($"SSH command execution failed: {ex.Message}", ex);
        }
    }
}