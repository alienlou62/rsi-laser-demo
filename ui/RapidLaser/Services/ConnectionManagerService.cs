namespace RapidLaser.Services;

public interface IConnectionManagerService
{
    bool IsConnected { get; }
    bool IsRunning { get; }

    ICameraService? CameraService { get; }
    IRmpGrpcService? RmpGrpcService { get; }

    //rmp grpc
    Task<bool> ConnectAsync(string ipAddress, int port);
    Task DisconnectAsync();
    //ssh
    Task<string> RunSshCommandAsync(string command, string sshUser, string sshPass);
}

public partial class ConnectionManagerService : ObservableObject, IConnectionManagerService
{
    /** SERVICES **/
    public ICameraService? CameraService { get; }
    public IRmpGrpcService? RmpGrpcService { get; }

    /** FIELDS **/
    private string _latestIpAddress = string.Empty;
    private int _latestPort;

    [ObservableProperty]
    private bool _isConnected = false;

    [ObservableProperty]
    private bool _isRunning = false;


    /** CONSTRUCTOR **/
    public ConnectionManagerService()
    {
        CameraService  = new HttpCameraService("http://localhost:50080");
        RmpGrpcService = new RmpGrpcService();
    }


    /**  METHODS **/
    //rmp grpc
    public async Task<bool> ConnectAsync(string ip, int port)
    {
        _latestIpAddress = ip;
        _latestPort = port;

        try
        {
            // Only connect gRPC service (to the specified IP/port)
            var serverAddress = $"{_latestIpAddress}:{_latestPort}";
            var grpcConnected = await RmpGrpcService.ConnectAsync(serverAddress);

            if (grpcConnected)
            {
                IsConnected = true;
                IsRunning = true;
                return true;
            }
            else
            {
                return false;
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
        if (RmpGrpcService != null)
        {
            await RmpGrpcService.DisconnectAsync();
        }

        IsConnected = false;
        IsRunning = false;
    }

    //ssh
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