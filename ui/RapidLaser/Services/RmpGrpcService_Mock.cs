namespace RapidLaser.Services;

public class RmpGrpcService_Mock : IRSIGrpcService
{
    private readonly Dictionary<string, object> _mockGlobalValues;
    private readonly Dictionary<string, double> _mockAxisPositions;
    private readonly Random _random = new();

    public bool IsConnected { get; private set; } = true; // Mock is always "connected"

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

    public async Task<bool> StartMotionAsync()
    {
        await Task.Delay(100);

        // Simulate motion by gradually changing positions
        Task.Run(async () =>
        {
            for (int i = 0; i < 50; i++)
            {
                await Task.Delay(100);
                _mockAxisPositions["X"] += _random.NextDouble() * 0.1;
                _mockAxisPositions["Y"] += _random.NextDouble() * 0.1;
            }
        });

        return true;
    }

    public async Task<bool> StopMotionAsync()
    {
        await Task.Delay(50);
        return true;
    }

    public async Task<Dictionary<string, double>> GetAxisPositionsAsync()
    {
        // Simulate network delay
        await Task.Delay(30);

        // Add some small random variations to simulate real motion
        var positions = new Dictionary<string, double>();
        foreach (var kvp in _mockAxisPositions)
        {
            positions[kvp.Key] = kvp.Value + (_random.NextDouble() - 0.5) * 0.001;
        }

        return positions;
    }

    // Helper methods for testing
    public void SimulateError()
    {
        _mockGlobalValues["ErrorCount"] = (int)_mockGlobalValues["ErrorCount"] + 1;
        _mockGlobalValues["SystemStatus"] = "Error";
    }

    public void ClearErrors()
    {
        _mockGlobalValues["ErrorCount"] = 0;
        _mockGlobalValues["SystemStatus"] = "Running";
    }

    public void SetAxisPosition(string axis, double position)
    {
        if (_mockAxisPositions.ContainsKey(axis))
        {
            _mockAxisPositions[axis] = position;
        }
    }
}