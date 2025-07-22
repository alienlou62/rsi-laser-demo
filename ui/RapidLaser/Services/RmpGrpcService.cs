using Grpc.Core;
using RSI.RapidCodeRemote;
using static RSI.RapidServer.ServerControlService;

namespace RapidLaser.Services;

public interface IRmpGrpcService
{
    bool IsConnected { get; }
    Task<bool> ConnectAsync(string serverAddress);
    Task DisconnectAsync();

    // Global value operations
    Task<Dictionary<string, object>> GetGlobalValuesAsync();
    Task<T> GetGlobalValueAsync<T>(string name);
    Task<bool> SetGlobalValueAsync<T>(string name, T value);

    // Motion control operations
    Task<bool> StartMotionAsync();
    Task<bool> StopMotionAsync();
    Task<Dictionary<string, double>> GetAxisPositionsAsync();
}

public class RmpGrpcService : IRmpGrpcService
{
    /** FIELDS **/
    //private
    private GrpcChannel? _channel;
    private bool _isConnected = false;
    private RMPService.RMPServiceClient? _rmpClient;
    private ServerControlServiceClient? _serverClient;

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

    //controller

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
    public async Task<Dictionary<string, double>> GetAxisPositionsAsync()
    {
        if (!_isConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        // TODO: Implement with actual proto service calls
        await Task.Delay(100); // Simulate network call
        return new Dictionary<string, double>();
    }
}