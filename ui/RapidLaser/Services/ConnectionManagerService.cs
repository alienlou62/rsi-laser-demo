using System;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RapidLaser.Services;

public partial class ConnectionSettings : ObservableObject
{
    [ObservableProperty]
    private string _ipAddress = "localhost";

    [ObservableProperty]
    private int _port = 50051;

    [ObservableProperty]
    private bool _isConnected = false;

    [ObservableProperty]
    private bool _useMockService = false;

    public string ServerAddress => $"{IpAddress}:{Port}";
}

public interface IConnectionManagerService
{
    ConnectionSettings Settings { get; }
    bool IsConnected { get; }
    IRSIGrpcService GrpcService { get; }

    Task<bool> ConnectAsync();
    Task DisconnectAsync();
    void SetMockMode(bool useMock);
}

public partial class ConnectionManagerService : ObservableObject, IConnectionManagerService
{
    [ObservableProperty]
    private ConnectionSettings _settings = new();

    [ObservableProperty]
    private bool _isConnected = false;

    [ObservableProperty]
    private IRSIGrpcService _grpcService;

    private readonly RSIGrpcService _realService;
    private readonly MockRSIGrpcService _mockService;

    public ConnectionManagerService()
    {
        _realService = new RSIGrpcService();
        _mockService = new MockRSIGrpcService();
        _grpcService = _mockService; // Start with mock service
    }

    public async Task<bool> ConnectAsync()
    {
        try
        {
            if (Settings.UseMockService)
            {
                GrpcService = _mockService;
                IsConnected = true;
                Settings.IsConnected = true;
                return true;
            }
            else
            {
                var connected = await _realService.ConnectAsync(Settings.ServerAddress);
                if (connected)
                {
                    GrpcService = _realService;
                    IsConnected = true;
                    Settings.IsConnected = true;
                }
                return connected;
            }
        }
        catch (Exception)
        {
            IsConnected = false;
            Settings.IsConnected = false;
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
        Settings.IsConnected = false;
    }

    public void SetMockMode(bool useMock)
    {
        Settings.UseMockService = useMock;
        if (useMock)
        {
            GrpcService = _mockService;
        }
    }
}