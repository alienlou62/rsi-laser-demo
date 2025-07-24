using System;
using System.Globalization;
using Avalonia.Data.Converters;
using Avalonia.Media;

namespace RapidLaser.Converters;

public static class BoolConverters
{
    /// <summary>
    /// Converts connection status to color brush
    /// </summary>
    public static readonly FuncValueConverter<bool, IBrush> ConnectedToColorConverter =
        new(connected => connected
            ? new SolidColorBrush(Color.FromRgb(76, 175, 80))  // Green for connected
            : new SolidColorBrush(Color.FromRgb(244, 67, 54))); // Red for disconnected

    /// <summary>
    /// Converts connecting state to button text
    /// </summary>
    public static readonly FuncValueConverter<bool, string> ConnectingToTextConverter =
        new(connecting => connecting ? "Connecting..." : "Connect");

    /// <summary>
    /// Converts polling state to button text
    /// </summary>
    public static readonly FuncValueConverter<bool, string> PollingToTextConverter =
        new(polling => polling ? "Stop Polling" : "Start Polling");

    /// <summary>
    /// Converts connection status to button text
    /// </summary>
    public static readonly FuncValueConverter<bool, string> ConnectedToButtonTextConverter =
        new(connected => connected ? "Disconnect" : "Connect");

    /// <summary>
    /// Converts connection status to button color
    /// </summary>
    public static readonly FuncValueConverter<bool, IBrush> ConnectedToButtonColorConverter =
        new(connected => connected
            ? new SolidColorBrush(Color.FromRgb(244, 67, 54))  // Red for disconnect
            : new SolidColorBrush(Color.FromRgb(0, 120, 212))); // Blue for connect

    /// <summary>
    /// Converts string to bool (true if not null or empty)
    /// </summary>
    public static readonly FuncValueConverter<string?, bool> StringNotEmptyToBoolConverter =
        new(str => !string.IsNullOrEmpty(str));

    /// <summary>
    /// Converts ball X position to canvas position (centers the ball circle and clamps to canvas bounds)
    /// Takes both position and radius as input via MultiBinding
    /// </summary>
    public static readonly FuncMultiValueConverter<double, double> BallPositionToCenterOffsetConverter =
        new(values =>
        {
            var valueList = values.ToList();
            if (valueList.Count < 2) return 0;

            double position = valueList[0];
            double ballRadius = valueList[1] / 2.0; // Convert diameter to radius
            const int canvasWidth = 620;

            // Clamp position to canvas bounds accounting for ball radius
            double clampedPosition = Math.Max(ballRadius, Math.Min(position, canvasWidth - ballRadius));

            // Return position offset by radius to center the ball
            return clampedPosition - ballRadius;
        });

    /// <summary>
    /// Converts ball Y position to canvas position (centers the ball circle and clamps to canvas bounds)
    /// Takes both position and radius as input via MultiBinding
    /// </summary>
    public static readonly FuncMultiValueConverter<double, double> BallYPositionToCenterOffsetConverter =
        new(values =>
        {
            var valueList = values.ToList();
            if (valueList.Count < 2) return 0;

            double position = valueList[0];
            double ballRadius = valueList[1] / 2.0; // Convert diameter to radius
            const int canvasHeight = 480;

            // Clamp position to canvas bounds accounting for ball radius
            double clampedPosition = Math.Max(ballRadius, Math.Min(position, canvasHeight - ballRadius));

            // Return position offset by radius to center the ball
            return clampedPosition - ballRadius;
        });

    /// <summary>
    /// Converts nullable bool status to color brush for status labels
    /// null = white, true = green, false = red
    /// </summary>
    public static readonly FuncValueConverter<bool?, IBrush> StatusColorConverter =
        new(isActive => isActive switch
        {
            true => new SolidColorBrush(Color.FromRgb(76, 175, 80)),   // Green for active/true
            false => new SolidColorBrush(Color.FromRgb(244, 67, 54)),  // Red for inactive/false
            null => new SolidColorBrush(Color.FromRgb(255, 255, 255))  // White for neutral/null
        });

    /// <summary>
    /// Converts a value to boolean by comparing with a parameter string
    /// Returns true if the value equals the parameter, false otherwise
    /// Usage: {Binding SomeProperty, Converter={x:Static converters:BoolConverters.StringEqualsParameterConverter}, ConverterParameter=ExpectedValue}
    /// </summary>
    public static readonly IValueConverter StringEqualsParameterConverter = new FuncValueConverter<object?, object?, bool>((value, parameter) =>
    {
        string? valueStr = value?.ToString();
        string? parameterStr = parameter?.ToString();
        return string.Equals(valueStr, parameterStr, StringComparison.OrdinalIgnoreCase);
    });
}

public static class StringConverters
{
    /// <summary>
    /// Converts string to visibility (visible if not null or empty)
    /// </summary>
    public static readonly FuncValueConverter<string?, bool> IsNotNullOrEmpty =
        new(str => !string.IsNullOrEmpty(str));
}