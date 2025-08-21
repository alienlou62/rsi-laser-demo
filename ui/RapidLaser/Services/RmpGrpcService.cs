
namespace RapidLaser.Services;

public interface IRmpGrpcService
{
    bool IsConnected { get; }

    // ping
    Task<bool> CheckConnectionAsync();

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

    //tasks
    Task<RTTaskCreationParameters> GetTaskCreationParametersAsync(int taskId);
    Task<RTTaskBatchResponse> GetTaskBatchResponseAsync(IEnumerable<int> taskIds);
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
    //ping
    public async Task<bool> CheckConnectionAsync()
    {
        if (_channel == null)
        {
            Console.WriteLine("RMP service is not connected - channel is null because ConnectAsync was not called");
            return false;
        }

        if (_serverClient == null)
        {
            Console.WriteLine("RMP service is not connected - server client is null");
            return false;
        }

        if (!IsConnected)
        {
            Console.WriteLine("RMP service is not connected");
            return false;
        }

        try
        {
            // Ping the server by getting basic info
            var reply = await _serverClient.GetInfoAsync(new(), options: new CallOptions(deadline: DateTime.UtcNow.AddSeconds(2)));
            _isConnected = (reply != null);
        }
        catch (RpcException ex)
        {
            Console.WriteLine($"RMP service connection check failed: {ex.Status.Detail}");
            _isConnected = false;
        }

        return _isConnected;
    }
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

    //tasks
    public async Task<RTTaskCreationParameters> GetTaskCreationParametersAsync(int taskId)
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        if (_firstTaskManagerIndex is null || _firstTaskManagerIndex.HasValue is false)
            throw new InvalidOperationException("No task manager index available. Call GetTaskManagerStatusAsync once to set.");

        var response = await _rmpClient.RTTaskAsync(new()
        {
            Id = taskId,
            ManagerId = _firstTaskManagerIndex.Value,
            Header = infoOptimizationHeader
        });

        return response.Info.CreationParameters;
    }

    public async Task<RTTaskBatchResponse> GetTaskBatchResponseAsync(IEnumerable<int> taskIds)
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        if (_firstTaskManagerIndex is null || _firstTaskManagerIndex.HasValue is false)
            throw new InvalidOperationException("No task manager index available. Call GetTaskManagerStatusAsync once to set.");

        // request
        var request = new RTTaskBatchRequest();

        foreach (var id in taskIds)
        {
            request.Requests.Add(new RTTaskRequest()
            {
                Id = id,
                ManagerId = _firstTaskManagerIndex.Value,
                Header = statusOptimizationHeader,
            });
        }

        // action
        var response = await _rmpClient.RTTaskBatchAsync(request);

        return response;
    }
}
