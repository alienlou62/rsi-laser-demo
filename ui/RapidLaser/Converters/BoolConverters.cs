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
    /// Converts ball position to canvas position (centers the ball circle)
    /// </summary>
    public static readonly FuncValueConverter<double, double> BallPositionToCenterOffsetConverter =
        new(position => position - 20); // Offset by half the circle diameter (40/2 = 20)
}

public static class StringConverters
{
    /// <summary>
    /// Converts string to visibility (visible if not null or empty)
    /// </summary>
    public static readonly FuncValueConverter<string?, bool> IsNotNullOrEmpty =
        new(str => !string.IsNullOrEmpty(str));
}