using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RapidLaser.Services;

public interface IGlobalValueService
{
    event EventHandler<GlobalValuesUpdatedEventArgs>? GlobalValuesUpdated;

    Task<Dictionary<string, object>> GetAllGlobalValuesAsync();
    Task<T> GetGlobalValueAsync<T>(string name);
    Task<bool> SetGlobalValueAsync<T>(string name, T value);
    void StartPolling(TimeSpan interval);
    void StopPolling();
}

public class GlobalValuesUpdatedEventArgs : EventArgs
{
    public Dictionary<string, object> Values { get; }

    public GlobalValuesUpdatedEventArgs(Dictionary<string, object> values)
    {
        Values = values;
    }
}

public partial class GlobalValueService : ObservableObject, IGlobalValueService
{
    private readonly IConnectionManagerService _connectionManager;
    private readonly System.Timers.Timer _pollingTimer;

    [ObservableProperty]
    private Dictionary<string, object> _currentValues = new();

    public event EventHandler<GlobalValuesUpdatedEventArgs>? GlobalValuesUpdated;

    public GlobalValueService(IConnectionManagerService connectionManager)
    {
        _connectionManager = connectionManager;
        _pollingTimer = new System.Timers.Timer();
        _pollingTimer.Elapsed += OnPollingTimerElapsed;
    }

    public async Task<Dictionary<string, object>> GetAllGlobalValuesAsync()
    {
        if (!_connectionManager.IsConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        return await _connectionManager.GrpcService.GetGlobalValuesAsync();
    }

    public async Task<T> GetGlobalValueAsync<T>(string name)
    {
        if (!_connectionManager.IsConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        return await _connectionManager.GrpcService.GetGlobalValueAsync<T>(name);
    }

    public async Task<bool> SetGlobalValueAsync<T>(string name, T value)
    {
        if (!_connectionManager.IsConnected)
            throw new InvalidOperationException("Not connected to gRPC server");

        return await _connectionManager.GrpcService.SetGlobalValueAsync(name, value);
    }

    public void StartPolling(TimeSpan interval)
    {
        _pollingTimer.Interval = interval.TotalMilliseconds;
        _pollingTimer.Start();
    }

    public void StopPolling()
    {
        _pollingTimer.Stop();
    }

    private async void OnPollingTimerElapsed(object? sender, System.Timers.ElapsedEventArgs e)
    {
        try
        {
            if (_connectionManager.IsConnected)
            {
                var values = await GetAllGlobalValuesAsync();
                CurrentValues = values;
                GlobalValuesUpdated?.Invoke(this, new GlobalValuesUpdatedEventArgs(values));
            }
        }
        catch (Exception ex)
        {
            // Log error but continue polling
            Console.WriteLine($"Error polling global values: {ex.Message}");
        }
    }
}
