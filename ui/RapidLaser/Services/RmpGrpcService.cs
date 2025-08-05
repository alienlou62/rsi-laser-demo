
namespace RapidLaser.Services;

public interface IRmpGrpcService
{
    bool IsConnected { get; }

    //grpc connection
    Task<bool> ConnectAsync(string ip, int port);
    Task DisconnectAsync();

    //controller
    Task<MotionControllerStatus> GetControllerStatusAsync();

    //network
    Task<NetworkStatus> GetNetworkStatusAsync();

    //taskmanager
    Task<RTTaskManagerStatus> GetTaskManagerStatusAsync(int index = 0);
    Task<bool> StopTaskManagerAsync();
    Task<RTTaskManagerResponse> SetBoolGlobalValueAsync(string name, bool value);
}

public class RmpGrpcService : IRmpGrpcService
{
    /** FIELDS **/
    //private
    private GrpcChannel? _channel;
    private bool _isConnected = false;
    private RMPService.RMPServiceClient? _rmpClient;
    private ServerControlServiceClient? _serverClient;

    RequestHeader statusOptimizationHeader = new() { Optimization = new() { SkipConfig = true, SkipInfo = true, SkipStatus = false } };
    RequestHeader infoOptimizationHeader = new() { Optimization = new() { SkipConfig = true, SkipInfo = false, SkipStatus = true } };


    //public
    public bool IsConnected => _isConnected;


    /** METHODS **/
    //grpc network
    public async Task<bool> ConnectAsync(string ip, int port)
    {
        try
        {
            // Create gRPC channel
            _channel = GrpcChannel.ForAddress($"http://{ip}:{port}");

            // Check if the channel is valid
            _serverClient = new ServerControlServiceClient(_channel);
            var reply = await _serverClient.GetInfoAsync(new(), options: new CallOptions(deadline: DateTime.UtcNow.AddSeconds(2)));

            _isConnected = (reply != null);

            // Initialize the gRPC clients with the generated proto clients
            _rmpClient = new RMPService.RMPServiceClient(_channel);

            return _isConnected;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to connect to gRPC server: {ex.Message}");
            await DisconnectAsync();
            return false;
        }
    }

    public async Task DisconnectAsync()
    {
        if (_channel != null)
        {
            await _channel.ShutdownAsync();
            _channel.Dispose();
            _channel = null;
        }

        _isConnected = false;
        _firstTaskManagerIndex = null;
    }

    //controller
    public async Task<MotionControllerStatus> GetControllerStatusAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        MotionControllerResponse response = await _rmpClient.MotionControllerAsync(new() { Header = statusOptimizationHeader });
        var status = response.Status;
        return status;
    }

    //network
    public async Task<NetworkStatus> GetNetworkStatusAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        NetworkResponse response = await _rmpClient.NetworkAsync(new() { Header = statusOptimizationHeader });
        var status = response.Status;
        return status;
    }

    //taskmanager
    private int? _firstTaskManagerIndex;
    public async Task<RTTaskManagerStatus> GetTaskManagerStatusAsync(int index = 0)
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        RTTaskManagerResponse response;

        if (_firstTaskManagerIndex == null)
        {
            response = await _rmpClient.RTTaskManagerAsync(new()
            {
                Action = new RTTaskManagerAction { Discover = new() },
                Header = infoOptimizationHeader
            },
                options: new CallOptions(deadline: DateTime.UtcNow.AddSeconds(10))
            );

            _firstTaskManagerIndex = response.Action.Discover.ManagerIds[0];
        }

        if (_firstTaskManagerIndex is null)
            throw new InvalidOperationException("No task manager index available.");

        response = await _rmpClient.RTTaskManagerAsync(new()
        {
            Id = _firstTaskManagerIndex.Value,
            Header = statusOptimizationHeader,
        });
        var status = response.Status;
        return status;
    }

    public async Task<bool> StopTaskManagerAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        if (_firstTaskManagerIndex is null)
            throw new InvalidOperationException("No task manager index available.");

        try
        {
            var response = await _rmpClient.RTTaskManagerAsync(new()
            {
                Id = _firstTaskManagerIndex.Value,
                Action = new RTTaskManagerAction { Shutdown = new() },
                Header = infoOptimizationHeader,
            });

            return true;
        }
        catch
        {
            return false;
        }
    }

    public async Task<RTTaskManagerResponse> SetBoolGlobalValueAsync(string name, bool value)
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        if (_firstTaskManagerIndex is null)
            throw new InvalidOperationException("No task manager index available.");

        //set action
        var action = new RTTaskManagerAction();
        action.GlobalValueSets.Add(new RTTaskManagerAction.Types.GlobalValueSet
        {
            Name = name,
            Value = new FirmwareValue { BoolValue = value }
        });

        //call action request
        var response = await _rmpClient.RTTaskManagerAsync(new()
        {
            Id     = _firstTaskManagerIndex.Value,
            Action = action,
            Header = infoOptimizationHeader
        });

        return response;
    }

}

public class RmpGrpcService_Mock : IRmpGrpcService
{
    /** FIELDS **/
    //private
    private readonly Dictionary<string, object> _mockGlobalValues;
    private readonly Dictionary<string, double> _mockAxisPositions;
    private readonly Random _random = new();

    public bool IsConnected { get; private set; } = true; // Mock is always "connected"


    /** CONSTRUCTOR **/
    public RmpGrpcService_Mock()
    {
        // Initialize with some mock global values
        _mockGlobalValues = new Dictionary<string, object>
        {
            { "SystemStatus", "Running" },
            { "Temperature", 25.5 },
            { "Pressure", 1013.25 },
            { "LaserPower", 75.0 },
            { "ProcessingSpeed", 100.0 },
            { "ErrorCount", 0 },
            { "TotalCycles", 12345 },
            { "LastUpdateTime", DateTime.Now },
            { "OperatorName", "TestUser" },
            { "BatchNumber", "BATCH_001" },
            { "BallX", 320.0 },  // Ball position in camera coordinates
            { "BallY", 240.0 },  // Ball position in camera coordinates
            { "BallVelocityX", 12.5 },
            { "BallVelocityY", -8.2 },
            { "DetectionConfidence", 95.0 }
        };

        // Initialize mock axis positions
        _mockAxisPositions = new Dictionary<string, double>
        {
            { "X", 0.0 },
            { "Y", 0.0 },
            { "Z", 0.0 },
            { "A", 0.0 }
        };
    }


    /** METHODS **/
    //network
    public Task<bool> ConnectAsync(string serverAddress)
    {
        IsConnected = true;
        return Task.FromResult(true);
    }

    public Task DisconnectAsync()
    {
        IsConnected = false;
        return Task.CompletedTask;
    }

    //controller
    public Task<MotionControllerStatus> GetControllerStatusAsync()
    {
        throw new NotImplementedException();
    }

    //network
    public Task<NetworkStatus> GetNetworkStatusAsync()
    {
        throw new NotImplementedException();
    }

    //taskmanager
    public async Task<bool> StopTaskManagerAsync()
    {
        await Task.Delay(50);
        return true;
    }

    public Task<RTTaskManagerStatus> GetTaskManagerStatusAsync(int index = 0)
    {
        throw new NotImplementedException();
    }

    public Task<RTTaskManagerResponse> SetBoolGlobalValueAsync(string name, bool value)
    {
        throw new NotImplementedException();
    }
}