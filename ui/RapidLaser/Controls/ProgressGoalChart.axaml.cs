using System;
using Avalonia;
using Avalonia.Controls;

namespace RapidLaser.Controls;

public partial class ProgressGoalChart : UserControl
{
    public static readonly StyledProperty<double> TargetProperty =
        AvaloniaProperty.Register<ProgressGoalChart, double>(nameof(Target), 100000.0);

    public static readonly StyledProperty<double> AverageProperty =
        AvaloniaProperty.Register<ProgressGoalChart, double>(nameof(Average));

    public static readonly StyledProperty<double> MaximumProperty =
        AvaloniaProperty.Register<ProgressGoalChart, double>(nameof(Maximum));

    public double Target
    {
        get => GetValue(TargetProperty);
        set => SetValue(TargetProperty, value);
    }

    public double Average
    {
        get => GetValue(AverageProperty);
        set => SetValue(AverageProperty, value);
    }

    public double Maximum
    {
        get => GetValue(MaximumProperty);
        set => SetValue(MaximumProperty, value);
    }

    public ProgressGoalChart()
    {
        InitializeComponent();
        UpdateChart();
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);

        if (change.Property == TargetProperty ||
            change.Property == AverageProperty ||
            change.Property == MaximumProperty)
        {
            UpdateChart();
        }
    }

    protected override void OnSizeChanged(SizeChangedEventArgs e)
    {
        base.OnSizeChanged(e);
        UpdateChart();
    }

    private void UpdateChart()
    {
        if (AverageBar == null || TargetLine == null || MaxLine == null ||
            TargetLabel == null || AverageLabel == null || MaxLabel == null) return;

        var target = Target;
        var average = Average;
        var maximum = Maximum;

        // Find the controls
        var availableWidth = Bounds.Width;
        if (availableWidth <= 0) return;

        // Determine scale - use the maximum of target and maximum for range
        var maxValue = Math.Max(target, Math.Max(average, maximum));
        if (maxValue <= 0) return;

        var scale = availableWidth / maxValue;

        // Update average bar width
        var averageWidth = Math.Max(2, average * scale);
        AverageBar.Width = Math.Min(averageWidth, availableWidth);

        // Update target line position
        var targetPosition = Math.Min(target * scale, availableWidth - 3);
        TargetLine.Margin = new Thickness(targetPosition, 0, 0, 0);

        // Update max line position  
        var maxPosition = Math.Min(maximum * scale, availableWidth - 2);
        MaxLine.Width = Math.Min(maxPosition, availableWidth);

        // Update labels
        TargetLabel.Text = $"Target\n{target:N0}";
        AverageLabel.Text = $"Avg\n{average:N4}";
        MaxLabel.Text = $"Max\n{maximum:N4}";

        // Position labels appropriately
        TargetLabel.Margin = new Thickness(Math.Max(0, targetPosition - 30), 0, 0, 0);
        MaxLabel.Margin = new Thickness(0, 0, Math.Max(0, availableWidth - maxPosition - 50), 0);
    }
}
