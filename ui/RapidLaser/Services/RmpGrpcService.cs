using Grpc.Net.Client;

namespace RapidLaser.Services;

public interface IRmpGrpcService
{
    Task<bool> ConnectAsync(string serverAddress);
    Task DisconnectAsync();
    Task<MotionControllerStatus?> GetControllerStatusAsync();
    Task<NetworkStatus?> GetNetworkStatusAsync();
    Task<RTTaskManagerStatus?> GetTaskManagerStatusAsync();
    Task<bool> SetBoolGlobalValueAsync(string name, bool value);
    Task<bool> StopTaskManagerAsync();
}

public class RmpGrpcService : IRmpGrpcService
{
    private GrpcChannel? _channel;
    private Rsi.Grpc.RapidCodeService.RapidCodeServiceClient? _client;

    public async Task<bool> ConnectAsync(string serverAddress)
    {
        try
        {
            _channel = GrpcChannel.ForAddress($"http://{serverAddress}");
            _client = new Rsi.Grpc.RapidCodeService.RapidCodeServiceClient(_channel);
            
            // Test connection
            await _client.GetControllerStatusAsync(new Rsi.Grpc.Empty());
            return true;
        }
        catch
        {
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
            _client = null;
        }
    }

    public async Task<MotionControllerStatus?> GetControllerStatusAsync()
    {
        if (_client == null) return null;
        
        try
        {
            var response = await _client.GetControllerStatusAsync(new Rsi.Grpc.Empty());
            return response;
        }
        catch
        {
            return null;
        }
    }

    public async Task<NetworkStatus?> GetNetworkStatusAsync()
    {
        if (_client == null) return null;
        
        try
        {
            var response = await _client.GetNetworkStatusAsync(new Rsi.Grpc.Empty());
            return response;
        }
        catch
        {
            return null;
        }
    }

    public async Task<RTTaskManagerStatus?> GetTaskManagerStatusAsync()
    {
        if (_client == null) return null;
        
        try
        {
            var response = await _client.GetTaskManagerStatusAsync(new Rsi.Grpc.Empty());
            return response;
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetBoolGlobalValueAsync(string name, bool value)
    {
        if (_client == null) return false;
        
        try
        {
            var request = new Rsi.Grpc.SetBoolGlobalValueRequest
            {
                Name = name,
                Value = value
            };
            
            var response = await _client.SetBoolGlobalValueAsync(request);
            return response.Success;
        }
        catch
        {
            return false;
        }
    }

    public async Task<bool> StopTaskManagerAsync()
    {
        if (_client == null) return false;
        
        try
        {
            var response = await _client.StopTaskManagerAsync(new Rsi.Grpc.Empty());
            return response.Success;
        }
        catch
        {
            return false;
        }
    }
}

public class RmpGrpcService_Mock : IRmpGrpcService
{
    private bool _isConnected = false;
    private readonly Random _random = new();

    public Task<bool> ConnectAsync(string serverAddress)
    {
        _isConnected = true;
        return Task.FromResult(true);
    }

    public Task DisconnectAsync()
    {
        _isConnected = false;
        return Task.CompletedTask;
    }

    public Task<MotionControllerStatus?> GetControllerStatusAsync()
    {
        if (!_isConnected) return Task.FromResult<MotionControllerStatus?>(null);
        
        var status = new MotionControllerStatus
        {
            IsInitialized = true,
            State = MotionControllerState.Idle,
            AxisCount = 4
        };
        
        return Task.FromResult<MotionControllerStatus?>(status);
    }

    public Task<NetworkStatus?> GetNetworkStatusAsync()
    {
        if (!_isConnected) return Task.FromResult<NetworkStatus?>(null);
        
        var status = new NetworkStatus
        {
            IsConnected = true,
            NetworkType = "Mock"
        };
        
        return Task.FromResult<NetworkStatus?>(status);
    }

    public Task<RTTaskManagerStatus?> GetTaskManagerStatusAsync()
    {
        if (!_isConnected) return Task.FromResult<RTTaskManagerStatus?>(null);
        
        var status = new RTTaskManagerStatus
        {
            State = RTTaskManagerState.Running,
            TaskCount = 2,
            ActiveTaskCount = 1
        };
        
        // Add some mock global values
        status.GlobalValues.Add("BallX", CreateMockDoubleValue(320.0 + _random.NextDouble() * 100));
        status.GlobalValues.Add("BallY", CreateMockDoubleValue(240.0 + _random.NextDouble() * 100));
        status.GlobalValues.Add("BallRadius", CreateMockDoubleValue(15.0 + _random.NextDouble() * 10));
        status.GlobalValues.Add("IsMotionEnabled", CreateMockBoolValue(true));
        
        return Task.FromResult<RTTaskManagerStatus?>(status);
    }

    public Task<bool> SetBoolGlobalValueAsync(string name, bool value)
    {
        return Task.FromResult(_isConnected);
    }

    public Task<bool> StopTaskManagerAsync()
    {
        return Task.FromResult(_isConnected);
    }
    
    private FirmwareValue CreateMockDoubleValue(double value)
    {
        return new FirmwareValue { DoubleValue = value };
    }
    
    private FirmwareValue CreateMockBoolValue(bool value)
    {
        return new FirmwareValue { BoolValue = value };
    }
}
