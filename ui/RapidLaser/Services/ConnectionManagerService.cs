namespace RapidLaser.Services;

public interface IConnectionManagerService
{
    bool IsConnected { get; }
    bool UseMockService { get; set; }

    IRmpGrpcService GrpcService { get; }

    // grpc
    Task<bool> ConnectAsync(string ipAddress, int port);
    Task DisconnectAsync();
    void SetMockMode(bool useMock);
    // ssh
    Task<string> RunSshCommandAsync(string command, string sshUser, string sshPass);
}

public partial class ConnectionManagerService : ObservableObject, IConnectionManagerService
{
    // connection
    private string _latestIpAddress = string.Empty;
    private int _latestPort;

    [ObservableProperty]
    private bool _useMockService = false;

    //
    [ObservableProperty]
    private bool _isConnected = false;

    [ObservableProperty]
    private IRmpGrpcService _grpcService;

    private readonly RmpGrpcService _realService;
    private readonly RmpGrpcService_Mock _mockService;


    // CONSTRUCTOR
    public ConnectionManagerService()
    {
        _realService = new RmpGrpcService();
        _mockService = new RmpGrpcService_Mock();
        _grpcService = _mockService; // Start with mock service
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
                GrpcService = _mockService;
                IsConnected = true;
                return true;
            }
            else
            {
                var serverAddress = $"{_latestIpAddress}:{_latestPort}";
                var connected = await _realService.ConnectAsync(serverAddress);
                if (connected)
                {
                    GrpcService = _realService;
                    IsConnected = true;
                }
                return connected;
            }
        }
        catch (Exception)
        {
            IsConnected = false;
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
    }

    public void SetMockMode(bool useMock)
    {
        UseMockService = useMock;

        if (useMock)
        {
            GrpcService = _mockService;
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