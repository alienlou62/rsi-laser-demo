# RSI Laser Ball Tracking - GitHub Copilot Instructions

## Project Overview

This is a real-time laser ball tracking application built with **Avalonia UI** (C#) that interfaces with **RSI RapidCode** motion control systems. The application performs high-speed ball detection and tracking using computer vision and precise motion control.

## Architecture

### Frontend (Avalonia UI)

- **MVVM Pattern**: Uses CommunityToolkit.Mvvm with proper command binding
- **Modern Dark Theme**: Custom styled UI with real-time data visualization
- **Main Components**:
  - Live camera feed display with ball detection overlay
  - Image processing mask visualization
  - Real-time RTTask status and globals display
  - Motion control interface with pause/resume functionality

### Backend Services

The application follows a service-oriented architecture with clear interfaces:

#### Core Services

1. **ICameraService** - Camera frame acquisition (Pylon SDK integration)
2. **IImageProcessingService** - Ball detection and image processing (OpenCV-based)
3. **IMotionControlService** - RSI RapidCode motion control
4. **IRTTaskManagerService** - Real-time task orchestration

#### Service Implementation Strategy

- **Simulated Services**: For development and testing without hardware
- **Production Services**: Will use gRPC clients to communicate with RapidCode engine

## Key Technical Requirements

### Performance Constraints

- **Real-time Processing**: 200Hz main loop (5ms cycle time)
- **Low Latency**: Frame grab → processing → motion command < 16ms
- **Thread Safety**: All services must be thread-safe for concurrent access

### Hardware Integration

- **Camera**: Basler cameras via Pylon SDK (YUV 4:2:2 format)
- **Motion Control**: RSI RapidCode multi-axis systems
- **Image Processing**: OpenCV for ball detection and tracking

### Data Flow

```
Camera → Frame Grab → Ball Detection → Position Calculation → Motion Command
   ↓                      ↓                    ↓                  ↓
RTTask Globals ← UI Updates ← Status Monitoring ← Performance Stats
```

## Code Style Guidelines

### C# Conventions

- Use **nullable reference types** throughout
- Use **global usings file** (`GlobalUsings.cs`) for common namespace imports
- Use **file-scoped namespaces** (`namespace MyNamespace;`) instead of block syntax
- Implement **IDisposable** for resource management
- Use **async/await** patterns for I/O operations
- Apply **thread-safe patterns** for shared data access
  Always use the "Is" naming convention for boolean values (e.g., IsInitialized, IsEnabled, IsReady)

### Global Usings Implementation

Create a `GlobalUsings.cs` file at the project root with common namespace imports:

```csharp
// Global using directives for common namespaces
global using System;
global using System.Collections.Generic;
global using System.Threading.Tasks;

// Avalonia-specific global usings
global using Avalonia.Controls;
global using Avalonia.Threading;

// MVVM and Data Binding
global using CommunityToolkit.Mvvm.ComponentModel;
global using CommunityToolkit.Mvvm.Input;

// Project-specific namespaces
global using RapidLaser.Services;
```

### File-Scoped Namespace Guidelines

Always use file-scoped namespaces to reduce indentation and improve readability:

```csharp
// ✅ Preferred: File-scoped namespace
namespace RapidLaser.Services;

public interface ICameraService
{
    Task<bool> InitializeAsync();
}

public class CameraService : ICameraService
{
    public async Task<bool> InitializeAsync()
    {
        // Implementation
        return true;
    }
}
```

Instead of:

```csharp
// ❌ Avoid: Block-scoped namespace
namespace RapidLaser.Services
{
    public interface ICameraService
    {
        Task<bool> InitializeAsync();
    }

    public class CameraService : ICameraService
    {
        public async Task<bool> InitializeAsync()
        {
            // Implementation
            return true;
        }
    }
}
```

### Service Implementation Patterns

```csharp
// Interface definition
public interface IMyService : IDisposable
{
    bool IsInitialized { get; }
    Task<bool> InitializeAsync();
    // ... other members
}

// Implementation with proper error handling
public class MyService : IMyService
{
    private readonly object _lock = new();
    private bool _isInitialized;

    public bool IsInitialized => _isInitialized;

    public async Task<bool> InitializeAsync()
    {
        try
        {
            // Initialization logic
            _isInitialized = true;
            return true;
        }
        catch (Exception ex)
        {
            // Log error
            return false;
        }
    }

    public void Dispose()
    {
        // Cleanup logic
    }
}
```

### Dependency Injection Patterns

Always inject services via constructors for better testability and explicit dependencies:

```csharp
// ✅ Preferred: Constructor injection
public class MainViewModel : ViewModelBase
{
    private readonly ICameraService _cameraService;
    private readonly IMotionControlService _motionControlService;

    public MainViewModel(
        ICameraService cameraService,
        IMotionControlService motionControlService)
    {
        _cameraService = cameraService ?? throw new ArgumentNullException(nameof(cameraService));
        _motionControlService = motionControlService ?? throw new ArgumentNullException(nameof(motionControlService));
    }
}

// Register services in App.axaml.cs or Program.cs
services.AddSingleton<ICameraService, SimulatedCameraService>();
services.AddSingleton<IMotionControlService, SimulatedMotionControlService>();
services.AddTransient<MainViewModel>();
```

### UI and Icon Guidelines

Use **Material.Icons.Avalonia** for consistent iconography:

```xml
<!-- Add to project -->
<PackageReference Include="Material.Icons.Avalonia" />

<!-- Usage in XAML -->
<Button>
    <StackPanel Orientation="Horizontal">
        <avalonia:MaterialIcon Kind="Play" Width="16" Height="16" />
        <TextBlock Text="Start" Margin="4,0,0,0" />
    </StackPanel>
</Button>
```

### MVVM Patterns

- Use **CommunityToolkit.Mvvm** attributes (`[ObservableProperty]`, `[RelayCommand]`)
- Implement **INotifyPropertyChanged** for computed properties
- Use **commands instead of event handlers** for UI interactions
- **Prefer commands in ViewModels** over code-behind in Views (except for MainWindow operations like drag/minimize/close)

#### Boolean-based Styling and Content Changes

For styling and content changes based on boolean values, always use XAML binding-classes as described in the Avalonia documentation: https://docs.avaloniaui.net/docs/guides/data-binding/binding-classes

Do not create extra fields in your ViewModel for display or color properties derived from booleans, such as:

```csharp
// Display Properties
public string IsProgramPausedDisplay => IsProgramPaused ? "Yes" : "No";
public string MotionToggleButtonColor => IsProgramPaused ? "#4CAF50" : "#FF6B35";
```

Instead, use binding-classes in your XAML for conditional styling/content, for example:

```xml
<StackPanel Margin="20">
  <ListBox ItemsSource="{Binding ItemList}">
    <ListBox.Styles>
      <Style Selector="TextBlock.class1">
        <Setter Property="Background" Value="OrangeRed" />
      </Style>
      <Style Selector="TextBlock.class2">
        <Setter Property="Background" Value="PaleGreen" />
      </Style>
    </ListBox.Styles>
    <ListBox.ItemTemplate>
      <DataTemplate>
        <StackPanel>
          <TextBlock
              Classes.class1="{Binding IsClass1 }"
              Classes.class2="{Binding !IsClass1 }"
              Text="{Binding Title}"/>
        </StackPanel>
      </DataTemplate>
    </ListBox.ItemTemplate>
  </ListBox>
</StackPanel>
```

This approach keeps your ViewModel clean and leverages Avalonia's styling system for UI changes.

### XAML Styling Over Code Logic

Avoid computed display properties in ViewModels. Use XAML styling, selectors, and converters instead:

```csharp
// ❌ Avoid: Computed display properties in ViewModel
public string MotionPausedDisplay => MotionPaused ? "Yes" : "No";
public string MotionToggleButtonText => MotionPaused ? "Resume Motion" : "Pause Motion";
public string MotionToggleButtonColor => MotionPaused ? "#4CAF50" : "#FF6B35";
```

```xml
<!-- ✅ Preferred: Use XAML styling and selectors -->
<Button Classes="motion-toggle">
    <Button.Styles>
        <Style Selector="Button.motion-toggle">
            <Setter Property="Content" Value="Pause Motion" />
            <Setter Property="Background" Value="#FF6B35" />
        </Style>
        <Style Selector="Button.motion-toggle:checked">
            <Setter Property="Content" Value="Resume Motion" />
            <Setter Property="Background" Value="#4CAF50" />
        </Style>
    </Button.Styles>
</Button>
```

## RSI RapidCode Integration

### gRPC Proto Setup

- Proto files location: `C:\RSI\10.6.8\protos`
- Generated classes will be available for production service implementations
- Use gRPC clients for communication with RapidCode engine

### RTTask Globals Structure

```csharp
public class RTTaskGlobals
{
    public bool CameraReady { get; set; }
    public bool NewTarget { get; set; }
    public double TargetX { get; set; }
    public double TargetY { get; set; }
}
```

### Motion Control Patterns

- Always check **IsInitialized** and **IsEnabled** before commands
- Use **axis limits** and **safety checks**
- Implement **abort** and **fault clearing** functionality
- Handle **RsiError** exceptions appropriately

## Image Processing Guidelines

### Ball Detection Algorithm

1. **Extract V channel** from YUV image data
2. **Apply binary threshold** for red object detection
3. **Morphological operations** (close/open) for noise reduction
4. **Contour detection** and circle fitting
5. **Calculate offsets** from image center

### Performance Optimization

- **Minimize memory allocations** in processing loops
- **Reuse Mat objects** where possible
- **Use appropriate data types** (CV_8UC1 for masks, CV_8UC2 for YUV)
- **ROI processing** for targeted areas when possible

## UI/UX Considerations

### Real-time Updates

- Update UI at **~10Hz** (100ms intervals) to avoid overwhelming
- Use **background timers** for continuous data updates
- **Throttle expensive operations** like image display updates

### Actipro Software Avalonia Controls

The project uses **Actipro Software Avalonia Controls** (https://github.com/Actipro/Avalonia-Controls) for enhanced UI styling and theming capabilities. This library provides professional-grade controls and styling options that simplify color management and visual consistency.

#### Key Actipro Features to Use

- **Theme Resources**: Use Actipro's predefined theme resources for consistent color schemes
- **Control Styles**: Leverage Actipro's enhanced control styles for professional appearance
- **Color Management**: Utilize Actipro's color system for dynamic theming
- **Typography**: Use Actipro's typography system for consistent text styling

#### Implementation Guidelines

```xml
<!-- Use Actipro theme resources instead of custom colors -->
<Border Background="{DynamicResource Theme.Background.Primary}"
        BorderBrush="{DynamicResource Theme.Border.Default}">

<!-- Leverage Actipro control classes for styling -->
<Button Classes="accent primary" Content="Primary Action" />
<TextBlock Classes="theme-text-heading" Text="Section Header" />
```

#### Color and Styling Best Practices

- **Prefer Actipro theme resources** over hardcoded color values
- **Use Actipro control classes** for consistent styling across the application
- **Leverage theme variants** for light/dark mode support
- **Apply Actipro typography classes** for text hierarchy and readability
- **Utilize Actipro's spacing and layout helpers** for consistent margins and padding

### Visual Feedback

- **Color coding** for status indicators (green=good, red=error, yellow=warning)
- **Numeric displays** with appropriate precision (positions to 0.1, percentages to 1%)
- **Progress indicators** for long-running operations

### Responsive Design

- Support **window resize** and **maximize/minimize**
- **Custom window controls** for modern appearance
- **Dark theme** optimized for long-term use

## Error Handling Strategy

### Service Layer

- **Graceful degradation** when hardware unavailable
- **Detailed error logging** with context information
- **Recovery mechanisms** for transient failures
- **User-friendly error messages** in UI

### Common Error Scenarios

1. **Camera disconnected** → Switch to simulated mode, show warning
2. **Motion controller fault** → Abort motion, display fault info
3. **Processing timeout** → Skip frame, increment failure counter
4. **Memory pressure** → Reduce processing frequency temporarily

## Testing Approach

### Unit Testing

- **Mock interfaces** for isolated service testing
- **Timing validation** for performance-critical paths
- **Error condition simulation** for robust error handling

### Integration Testing

- **Hardware-in-loop** testing with actual camera/motion systems
- **Performance profiling** under various load conditions
- **Long-duration stability** testing

## Development Workflow

### Branch Strategy

- **main**: Production-ready code
- **develop**: Integration branch for new features
- **feature/\***: Individual feature development
- **hotfix/\***: Critical bug fixes

### Key Dependencies

```xml
<!-- Core Avalonia packages -->
<PackageReference Include="Avalonia" />
<PackageReference Include="CommunityToolkit.Mvvm" />
<PackageReference Include="Material.Icons.Avalonia" />

<!-- gRPC and Protocol Buffers -->
<PackageReference Include="Grpc.Net.Client" />
<PackageReference Include="Google.Protobuf" />

<!-- Image processing (when integrated) -->
<!-- <PackageReference Include="OpenCvSharp4" /> -->
```

## Future Enhancements

### Phase 2 Development

1. **Real camera integration** (replace simulated camera service)
2. **OpenCV integration** for actual image processing
3. **RapidCode gRPC clients** (replace simulated motion service)
4. **Performance optimization** and profiling
5. **Configuration management** system
6. **Data logging and replay** functionality

### Advanced Features

- **Multi-ball tracking** capabilities
- **Machine learning** for improved detection
- **Predictive motion** algorithms
- **Web-based monitoring** interface
- **Data analytics** and reporting

## Troubleshooting Common Issues

### Build Issues

- Ensure **RSI RapidCode SDK** is installed at `C:\RSI\10.6.8\`
- Check **proto file references** in project file
- Verify **package versions** in Directory.Packages.props

### Runtime Issues

- **Camera not detected** → Check simulated vs. real camera service
- **Motion commands ignored** → Verify IsEnabled and MotionPaused states
- **UI freezing** → Check for blocking operations on UI thread
- **High CPU usage** → Review timer intervals and processing frequency

---

## Quick Reference

### Key Files

- `MainViewModel.cs` - Primary UI logic and command handling
- `Services/` - All backend service implementations
- `Views/MainView.axaml` - Main UI layout and styling
- `Assets/protos/` - RapidCode protocol buffer definitions

### Important Interfaces

- `ICameraService` - Frame acquisition
- `IImageProcessingService` - Ball detection
- `IMotionControlService` - Motion commands
- `IRTTaskManagerService` - Task orchestration

Remember: This is a **high-performance real-time application**. Always consider timing, thread safety, and resource management in your implementations.
