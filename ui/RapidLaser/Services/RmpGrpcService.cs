
namespace RapidLaser.Services;

public interface IRmpGrpcService
{
    bool IsConnected { get; }

    //connection
    Task<bool> ConnectAsync(string serverAddress);
    Task DisconnectAsync();

    //globals
    Task<Dictionary<string, object>> GetGlobalValuesAsync();
    Task<T> GetGlobalValueAsync<T>(string name);
    Task<bool> SetGlobalValueAsync<T>(string name, T value);

    //program
    Task<bool> StartMotionAsync();
    Task<bool> StopMotionAsync();

    //status
    Task<MotionControllerStatus> GetControllerStatusAsync();
    Task<NetworkStatus> GetNetworkStatusAsync();
    Task<RTTaskManagerStatus> GetTaskManagerStatusAsync(int index = 0);
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
    //network
    public async Task<bool> ConnectAsync(string serverAddress)
    {
        try
        {
            // Create gRPC channel
            _channel = GrpcChannel.ForAddress($"http://{serverAddress}");

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
    }

    //globals
    public async Task<Dictionary<string, object>> GetGlobalValuesAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        // TODO: Implement with actual proto service calls
        // Example:
        // var request = new GetGlobalValuesRequest();
        // var response = await _rapidGrpcClient.GetGlobalValuesAsync(request);
        // return response.Values.ToDictionary(kv => kv.Key, kv => (object)kv.Value);

        await Task.Delay(100); // Simulate network call
        return new Dictionary<string, object>();
    }

    public async Task<T> GetGlobalValueAsync<T>(string name)
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        // TODO: Implement with actual proto service calls
        // Example:
        // var request = new GetGlobalValueRequest { Name = name };
        // var response = await _rapidGrpcClient.GetGlobalValueAsync(request);
        // return (T)Convert.ChangeType(response.Value, typeof(T));

        await Task.Delay(50); // Simulate network call
        return default(T)!;
    }

    public async Task<bool> SetGlobalValueAsync<T>(string name, T value)
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        // TODO: Implement with actual proto service calls
        // Example:
        // var request = new SetGlobalValueRequest 
        // { 
        //     Name = name, 
        //     Value = value?.ToString() ?? string.Empty 
        // };
        // var response = await _rapidGrpcClient.SetGlobalValueAsync(request);
        // return response.Success;

        await Task.Delay(50); // Simulate network call
        return true;
    }

    //motion
    public async Task<bool> StartMotionAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        // TODO: Implement with actual proto service calls
        await Task.Delay(100); // Simulate network call
        return true;
    }

    public async Task<bool> StopMotionAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        // TODO: Implement with actual proto service calls
        await Task.Delay(100); // Simulate network call
        return true;
    }

    //status
    public async Task<MotionControllerStatus> GetControllerStatusAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        MotionControllerResponse response = await _rmpClient.MotionControllerAsync(new() { Header = statusOptimizationHeader });
        var status = response.Status;
        return status;
    }

    public async Task<NetworkStatus> GetNetworkStatusAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        NetworkResponse response = await _rmpClient.NetworkAsync(new() { Header = statusOptimizationHeader });
        var status = response.Status;
        return status;
    }

    private int? _lastTaskManagerIndex;
    public async Task<RTTaskManagerStatus> GetTaskManagerStatusAsync(int index = 0)
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        RTTaskManagerResponse response;

        if (_lastTaskManagerIndex == null)
        {
            response = await _rmpClient.RTTaskManagerAsync(new()
            {
                Header = infoOptimizationHeader,
                Action = new RTTaskManagerAction { Discover = new() }
            });
            _lastTaskManagerIndex = response.Action.Discover.ManagerIds[0];
        }

        if (_lastTaskManagerIndex is null)
            throw new InvalidOperationException("No task manager index available.");

        response = await _rmpClient.RTTaskManagerAsync(new()
        {
            Id = _lastTaskManagerIndex.Value,
            Header = new()
        });
        var status = response.Status;
        return status;
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

    //globals
    public async Task<Dictionary<string, object>> GetGlobalValuesAsync()
    {
        // Simulate network delay
        await Task.Delay(50);

        // Update some values to simulate live data
        _mockGlobalValues["Temperature"] = 25.0 + (_random.NextDouble() - 0.5) * 2.0;
        _mockGlobalValues["Pressure"] = 1013.25 + (_random.NextDouble() - 0.5) * 10.0;
        _mockGlobalValues["LastUpdateTime"] = DateTime.Now;

        // Simulate ball movement (bouncing ball physics)
        var currentX = (double)_mockGlobalValues["BallX"];
        var currentY = (double)_mockGlobalValues["BallY"];
        var velocityX = (double)_mockGlobalValues["BallVelocityX"];
        var velocityY = (double)_mockGlobalValues["BallVelocityY"];

        // Update position
        var newX = currentX + velocityX * 0.1; // Scale down movement speed
        var newY = currentY + velocityY * 0.1;

        // Bounce off walls (camera bounds are 640x480)
        if (newX <= 20 || newX >= 620) // Account for ball radius
        {
            velocityX = -velocityX;
            newX = Math.Clamp(newX, 20, 620);
        }
        if (newY <= 20 || newY >= 460) // Account for ball radius
        {
            velocityY = -velocityY;
            newY = Math.Clamp(newY, 20, 460);
        }

        // Update values
        _mockGlobalValues["BallX"] = newX;
        _mockGlobalValues["BallY"] = newY;
        _mockGlobalValues["BallVelocityX"] = velocityX;
        _mockGlobalValues["BallVelocityY"] = velocityY;

        // Simulate varying detection confidence
        _mockGlobalValues["DetectionConfidence"] = 90.0 + _random.NextDouble() * 10.0;

        return new Dictionary<string, object>(_mockGlobalValues);
    }

    public async Task<T> GetGlobalValueAsync<T>(string name)
    {
        // Simulate network delay
        await Task.Delay(25);

        if (_mockGlobalValues.TryGetValue(name, out var value))
        {
            if (value is T directValue)
                return directValue;

            try
            {
                return (T)Convert.ChangeType(value, typeof(T));
            }
            catch
            {
                return default(T)!;
            }
        }

        return default(T)!;
    }

    public async Task<bool> SetGlobalValueAsync<T>(string name, T value)
    {
        // Simulate network delay
        await Task.Delay(25);

        _mockGlobalValues[name] = value!;
        return true;
    }

    //motion
    public async Task<bool> StartMotionAsync()
    {
        await Task.Delay(100);

        // Simulate motion by gradually changing positions
        for (int i = 0; i < 50; i++)
        {
            await Task.Delay(100);
            _mockAxisPositions["X"] += _random.NextDouble() * 0.1;
            _mockAxisPositions["Y"] += _random.NextDouble() * 0.1;
            // Clamp X between 0 and 620
            _mockAxisPositions["X"] = Math.Clamp(_mockAxisPositions["X"], 0, 620);
            // Clamp Y between 0 and 480
            _mockAxisPositions["Y"] = Math.Clamp(_mockAxisPositions["Y"], 0, 480);
        }

        return true;
    }

    public async Task<bool> StopMotionAsync()
    {
        await Task.Delay(50);
        return true;
    }

    //status
    public Task<MotionControllerStatus> GetControllerStatusAsync()
    {
        throw new NotImplementedException();
    }

    public Task<NetworkStatus> GetNetworkStatusAsync()
    {
        throw new NotImplementedException();
    }

    public Task<RTTaskManagerStatus> GetTaskManagerStatusAsync(int index = 0)
    {
        throw new NotImplementedException();
    }
}