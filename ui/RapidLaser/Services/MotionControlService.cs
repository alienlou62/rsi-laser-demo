using System;
using System.Threading.Tasks;

namespace RapidLaser.Services
{
    public interface IMotionControlService
    {
        bool IsInitialized { get; }
        bool IsEnabled { get; }
        bool MotionPaused { get; set; }
        double CurrentX { get; }
        double CurrentY { get; }

        Task<bool> InitializeAsync();
        Task<bool> EnableAsync();
        Task DisableAsync();
        Task MoveToAsync(double targetX, double targetY);
        Task AbortMotionAsync();
        Task ClearFaultsAsync();
        void Dispose();
    }

    public class SimulatedMotionControlService : IMotionControlService
    {
        private bool _isInitialized;
        private bool _isEnabled;
        private bool _motionPaused;
        private double _currentX;
        private double _currentY;
        private double _targetX;
        private double _targetY;
        private readonly object _lock = new();

        public bool IsInitialized => _isInitialized;
        public bool IsEnabled => _isEnabled;

        public bool MotionPaused
        {
            get => _motionPaused;
            set => _motionPaused = value;
        }

        public double CurrentX
        {
            get
            {
                lock (_lock)
                    return _currentX;
            }
        }

        public double CurrentY
        {
            get
            {
                lock (_lock)
                    return _currentY;
            }
        }

        public Task<bool> InitializeAsync()
        {
            _isInitialized = true;
            _currentX = 0.0;
            _currentY = 0.0;
            _targetX = 0.0;
            _targetY = 0.0;
            return Task.FromResult(true);
        }

        public Task<bool> EnableAsync()
        {
            if (!_isInitialized)
                return Task.FromResult(false);

            _isEnabled = true;

            // Start motion simulation task
            _ = Task.Run(SimulateMotion);

            return Task.FromResult(true);
        }

        public Task DisableAsync()
        {
            _isEnabled = false;
            return Task.CompletedTask;
        }

        public Task MoveToAsync(double targetX, double targetY)
        {
            if (!_isEnabled || _motionPaused)
                return Task.CompletedTask;

            lock (_lock)
            {
                _targetX = Math.Clamp(targetX, -100, 100); // Simulate axis limits
                _targetY = Math.Clamp(targetY, -100, 100);
            }

            return Task.CompletedTask;
        }

        public Task AbortMotionAsync()
        {
            lock (_lock)
            {
                _targetX = _currentX;
                _targetY = _currentY;
            }
            return Task.CompletedTask;
        }

        public Task ClearFaultsAsync()
        {
            return Task.CompletedTask;
        }

        private async Task SimulateMotion()
        {
            while (_isEnabled)
            {
                if (!_motionPaused)
                {
                    lock (_lock)
                    {
                        // Simulate smooth motion towards target
                        const double maxVelocity = 10.0; // mm/ms
                        const double dt = 0.016; // ~16ms update rate

                        double deltaX = _targetX - _currentX;
                        double deltaY = _targetY - _currentY;

                        double distance = Math.Sqrt(deltaX * deltaX + deltaY * deltaY);

                        if (distance > 0.1) // Only move if significant distance
                        {
                            double velocity = Math.Min(maxVelocity, distance / dt);
                            double moveX = deltaX * velocity * dt / distance;
                            double moveY = deltaY * velocity * dt / distance;

                            _currentX += moveX;
                            _currentY += moveY;
                        }
                    }
                }

                await Task.Delay(16); // ~60Hz update rate
            }
        }

        public void Dispose()
        {
            _isEnabled = false;
            _isInitialized = false;
        }
    }
}
