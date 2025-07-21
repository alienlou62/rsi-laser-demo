namespace RapidLaser.Services;

public interface IConnectionManagerService
{
    bool IsConnected { get; }
    IRmpGrpcService GrpcService { get; }

    Task<bool> ConnectAsync();
    Task DisconnectAsync();
    void SetMockMode(bool useMock);
    Task<string> RunSshCommandAsync(string command);
    string IpAddress { get; set; }
    int Port { get; set; }
    bool UseMockService { get; set; }
}

public partial class ConnectionManagerService : ObservableObject, IConnectionManagerService
{
    // connection
    public string SshUser = "rsi";
    public string SshPass = "admin";

    [ObservableProperty]
    private string _ipAddress = "localhost";

    [ObservableProperty]
    private int _port = 50061;

    [ObservableProperty]
    private bool _useMockService = false;

    public string ServerAddress => $"{IpAddress}:{Port}";

    //
    [ObservableProperty]
    private bool _isConnected = false;

    [ObservableProperty]
    private IRmpGrpcService _grpcService;

    private readonly RmpGrpcService _realService;
    private readonly RmpGrpcService_Mock _mockService;

    public ConnectionManagerService()
    {
        _realService = new RmpGrpcService();
        _mockService = new RmpGrpcService_Mock();
        _grpcService = _mockService; // Start with mock service
    }

    public async Task<bool> ConnectAsync()
    {
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
                var connected = await _realService.ConnectAsync(ServerAddress);
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

    // SSH
    public async Task<string> RunSshCommandAsync(string command)
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
            using var client = new SshClient(IpAddress, SshUser, SshPass);

            // Set connection timeout
            client.ConnectionInfo.Timeout = TimeSpan.FromSeconds(30);

            await Task.Run(client.Connect);

            if (!client.IsConnected)
            {
                throw new InvalidOperationException("Failed to establish SSH connection.");
            }

            var sshCommand = client.CreateCommand(command);
            sshCommand.CommandTimeout = TimeSpan.FromSeconds(30);

            var result = await Task.Run(sshCommand.Execute);

            client.Disconnect();

            return result;
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException($"SSH command execution failed: {ex.Message}", ex);
        }
    }
}