using System;
using System.Threading;
using Avalonia;
using Avalonia.Animation;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Shapes;
using Avalonia.Media;
using Avalonia.Styling;
using Avalonia.Threading;

namespace RapidLaser.Controls;

public class ProgressRing : TemplatedControl
{
    // Color property
    public static readonly StyledProperty<IBrush> RingColorProperty =
        AvaloniaProperty.Register<ProgressRing, IBrush>(nameof(RingColor), Brushes.Red);

    // Ring thickness property
    public static readonly StyledProperty<double> RingThicknessProperty =
        AvaloniaProperty.Register<ProgressRing, double>(nameof(RingThickness), 4.0);

    // Is spinning property
    public static readonly StyledProperty<bool> IsSpinningProperty =
        AvaloniaProperty.Register<ProgressRing, bool>(nameof(IsSpinning), true);

    // Animation duration property (in seconds)
    public static readonly StyledProperty<double> AnimationDurationProperty =
        AvaloniaProperty.Register<ProgressRing, double>(nameof(AnimationDuration), 2.0);

    // Arc length property (0-100, percentage of circle)
    public static readonly StyledProperty<double> ArcLengthProperty =
        AvaloniaProperty.Register<ProgressRing, double>(nameof(ArcLength), 75.0);

    // Minimum arc length for animation (0-100, percentage of circle)
    public static readonly StyledProperty<double> MinArcLengthProperty =
        AvaloniaProperty.Register<ProgressRing, double>(nameof(MinArcLength), 20.0);

    // Maximum arc length for animation (0-100, percentage of circle)
    public static readonly StyledProperty<double> MaxArcLengthProperty =
        AvaloniaProperty.Register<ProgressRing, double>(nameof(MaxArcLength), 80.0);

    // Arc length animation duration (in seconds)
    public static readonly StyledProperty<double> ArcAnimationDurationProperty =
        AvaloniaProperty.Register<ProgressRing, double>(nameof(ArcAnimationDuration), 3.0);

    // Enable arc length animation
    public static readonly StyledProperty<bool> IsArcAnimationEnabledProperty =
        AvaloniaProperty.Register<ProgressRing, bool>(nameof(IsArcAnimationEnabled), false);

    public ProgressRing()
    {
        Unloaded += OnUnloaded;
    }

    private void OnUnloaded(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        // Clean up animations and timers
        _arcLengthAnimation?.RunAsync(this)?.Dispose();
        _arcLengthAnimation = null;

        _spinTimer?.Stop();
        _spinTimer = null;
    }

    private Avalonia.Controls.Shapes.Path? _progressRingElement;
    private Canvas? _canvasElement;
    private Animation? _arcLengthAnimation;
    private DispatcherTimer? _spinTimer;
    private double _currentSpinAngle = 0;

    public IBrush RingColor
    {
        get => GetValue(RingColorProperty);
        set => SetValue(RingColorProperty, value);
    }

    public double RingThickness
    {
        get => GetValue(RingThicknessProperty);
        set => SetValue(RingThicknessProperty, value);
    }

    public bool IsSpinning
    {
        get => GetValue(IsSpinningProperty);
        set => SetValue(IsSpinningProperty, value);
    }

    public double AnimationDuration
    {
        get => GetValue(AnimationDurationProperty);
        set => SetValue(AnimationDurationProperty, value);
    }

    public double ArcLength
    {
        get => GetValue(ArcLengthProperty);
        set => SetValue(ArcLengthProperty, Math.Max(1, Math.Min(100, value)));
    }

    public double MinArcLength
    {
        get => GetValue(MinArcLengthProperty);
        set => SetValue(MinArcLengthProperty, Math.Max(1, Math.Min(100, value)));
    }

    public double MaxArcLength
    {
        get => GetValue(MaxArcLengthProperty);
        set => SetValue(MaxArcLengthProperty, Math.Max(1, Math.Min(100, value)));
    }

    public double ArcAnimationDuration
    {
        get => GetValue(ArcAnimationDurationProperty);
        set => SetValue(ArcAnimationDurationProperty, Math.Max(0.1, value));
    }

    public bool IsArcAnimationEnabled
    {
        get => GetValue(IsArcAnimationEnabledProperty);
        set => SetValue(IsArcAnimationEnabledProperty, value);
    }

    protected override void OnApplyTemplate(TemplateAppliedEventArgs e)
    {
        base.OnApplyTemplate(e);

        _progressRingElement = e.NameScope.Find<Avalonia.Controls.Shapes.Path>("PART_ProgressRingElement");
        _canvasElement = e.NameScope.Find<Canvas>("PART_CanvasElement");
        UpdateRing();
        UpdateArcAnimation();
        UpdateSpinAnimation();
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);

        if (change.Property == RingColorProperty ||
            change.Property == RingThicknessProperty ||
            change.Property == ArcLengthProperty)
        {
            UpdateRing();
        }
        else if (change.Property == IsArcAnimationEnabledProperty ||
                 change.Property == MinArcLengthProperty ||
                 change.Property == MaxArcLengthProperty ||
                 change.Property == ArcAnimationDurationProperty)
        {
            UpdateArcAnimation();
        }
        else if (change.Property == IsSpinningProperty ||
                 change.Property == AnimationDurationProperty)
        {
            UpdateSpinAnimation();
        }
    }

    protected override void OnSizeChanged(SizeChangedEventArgs e)
    {
        base.OnSizeChanged(e);
        UpdateRing();
    }

    private void UpdateRing()
    {
        if (_progressRingElement is null) return;

        // Use fixed coordinate system: 100x100 canvas with center at (50, 50)
        const double canvasSize = 100;
        const double centerX = 50;
        const double centerY = 50;

        // Calculate radius accounting for stroke thickness
        var radius = (canvasSize - RingThickness * 2) / 2.0;

        // Calculate arc angle from ArcLength percentage
        var arcAngle = Math.Max(1, Math.Min(100, ArcLength)) / 100.0 * 360.0;

        // Create the arc path geometry
        var geometry = CreateArcGeometry(centerX, centerY, radius, 0, arcAngle);
        _progressRingElement.Data = geometry;
    }

    private Geometry CreateArcGeometry(double centerX, double centerY, double radius, double startAngle, double sweepAngle)
    {
        // Convert angles to radians (start from top, going clockwise)
        var startRadians = (startAngle - 90) * Math.PI / 180.0;
        var endRadians = (startAngle + sweepAngle - 90) * Math.PI / 180.0;

        // Calculate start and end points relative to center
        var startX = centerX + radius * Math.Cos(startRadians);
        var startY = centerY + radius * Math.Sin(startRadians);
        var endX = centerX + radius * Math.Cos(endRadians);
        var endY = centerY + radius * Math.Sin(endRadians);

        // Determine if this is a large arc (> 180 degrees)
        var isLargeArc = sweepAngle > 180;

        // Create path geometry
        var pathGeometry = new PathGeometry();
        var pathFigure = new PathFigure
        {
            StartPoint = new Point(startX, startY),
            IsClosed = false
        };

        // Add arc segment
        var arcSegment = new ArcSegment
        {
            Point = new Point(endX, endY),
            Size = new Size(radius, radius),
            SweepDirection = SweepDirection.Clockwise,
            IsLargeArc = isLargeArc
        };

        pathFigure.Segments?.Add(arcSegment);
        pathGeometry.Figures?.Add(pathFigure);

        return pathGeometry;
    }

    private void UpdateArcAnimation()
    {
        // Stop any existing animation
        if (_arcLengthAnimation != null)
        {
            _arcLengthAnimation.RunAsync(this)?.Dispose();
            _arcLengthAnimation = null;
        }

        if (!IsArcAnimationEnabled) return;

        // Create new arc length animation
        _arcLengthAnimation = new Animation
        {
            Duration = TimeSpan.FromSeconds(ArcAnimationDuration),
            IterationCount = IterationCount.Infinite,
            PlaybackDirection = PlaybackDirection.Alternate,
            Children =
            {
                new KeyFrame
                {
                    Cue = new Cue(0.0),
                    Setters = { new Setter(ArcLengthProperty, MinArcLength) }
                },
                new KeyFrame
                {
                    Cue = new Cue(1.0),
                    Setters = { new Setter(ArcLengthProperty, MaxArcLength) }
                }
            }
        };

        // Start the animation
        _arcLengthAnimation.RunAsync(this);
    }

    private void UpdateSpinAnimation()
    {
        // Stop any existing spin timer
        _spinTimer?.Stop();
        _spinTimer = null;

        if (!IsSpinning || _canvasElement == null) return;

        // Set up render transform if not already set
        if (_canvasElement.RenderTransform is not RotateTransform)
        {
            _canvasElement.RenderTransform = new RotateTransform();
        }

        // Create timer for smooth spinning at 60fps
        _spinTimer = new DispatcherTimer(DispatcherPriority.Render)
        {
            Interval = TimeSpan.FromMilliseconds(16) // ~60fps
        };

        _spinTimer.Tick += (_, _) =>
        {
            if (_canvasElement?.RenderTransform is RotateTransform rotateTransform)
            {
                // Calculate rotation speed based on AnimationDuration
                var degreesPerSecond = 360.0 / AnimationDuration;
                var degreesPerFrame = degreesPerSecond * 0.016; // 16ms per frame

                _currentSpinAngle += degreesPerFrame;
                if (_currentSpinAngle >= 360.0)
                    _currentSpinAngle -= 360.0;

                rotateTransform.Angle = _currentSpinAngle;
            }
        };

        _spinTimer.Start();
    }
}
