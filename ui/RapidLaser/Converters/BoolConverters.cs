using System;
using System.Globalization;
using Avalonia;
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

            double centerX = valueList[0];      // Center X position of the ball
            double ballRadius = valueList[1];   // Radius (not diameter)
            const int canvasWidth = 640;

            // Clamp center position to canvas bounds accounting for ball radius
            double clampedCenterX = Math.Max(ballRadius, Math.Min(centerX, canvasWidth - ballRadius));

            // Return top-left position (center - radius) for Canvas.Left
            return clampedCenterX - ballRadius;
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

            double centerY = valueList[0];      // Center Y position of the ball
            double ballRadius = valueList[1];   // Radius (not diameter)
            const int canvasHeight = 480;

            // Clamp center position to canvas bounds accounting for ball radius
            double clampedCenterY = Math.Max(ballRadius, Math.Min(centerY, canvasHeight - ballRadius));

            // Return top-left position (center - radius) for Canvas.Top
            return clampedCenterY - ballRadius;
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

    /// <summary>
    /// Converts available width to responsive canvas width maintaining 4:3 aspect ratio
    /// Parameter should be the maximum width (e.g., "640")
    /// </summary>
    public static readonly IValueConverter ResponsiveCanvasWidthConverter = new FuncValueConverter<double, object?, double>((availableWidth, parameter) =>
    {
        if (!double.TryParse(parameter?.ToString(), out double maxWidth))
            maxWidth = 640; // Default max width

        // Use 90% of available width to leave some margin, but don't exceed maxWidth
        double targetWidth = Math.Min(availableWidth * 0.9, maxWidth);

        // Ensure minimum width
        return Math.Max(targetWidth, 320);
    });

    /// <summary>
    /// Converts available width to responsive canvas height maintaining 4:3 aspect ratio
    /// Parameter should be the maximum width (e.g., "640")
    /// </summary>
    public static readonly IValueConverter ResponsiveCanvasHeightConverter = new FuncValueConverter<double, object?, double>((availableWidth, parameter) =>
    {
        if (!double.TryParse(parameter?.ToString(), out double maxWidth))
            maxWidth = 640; // Default max width

        // Use 90% of available width to leave some margin, but don't exceed maxWidth
        double targetWidth = Math.Min(availableWidth * 0.9, maxWidth);
        targetWidth = Math.Max(targetWidth, 320); // Ensure minimum width

        // Calculate height maintaining 4:3 aspect ratio (width / height = 4/3, so height = width * 3/4)
        return targetWidth * 0.75;
    });

    /// <summary>
    /// Converts ball position and canvas width to responsive canvas position
    /// First binding: Ball X/Y position (0-640 for X, 0-480 for Y)
    /// Second binding: Ball radius
    /// Third binding: Current canvas width
    /// Fourth binding: IsResponsive flag
    /// </summary>
    public static readonly FuncMultiValueConverter<object?, double> ResponsiveBallPositionConverter = new(values =>
    {
        var valueList = values.ToList();
        if (valueList.Count < 4) return 0;

        if (!double.TryParse(valueList[0]?.ToString(), out double originalPosition)) return 0;
        if (!double.TryParse(valueList[1]?.ToString(), out double ballRadius)) ballRadius = 10;
        if (!double.TryParse(valueList[2]?.ToString(), out double currentCanvasWidth)) return 0;
        if (valueList[3] is not bool isResponsive) return 0;

        if (!isResponsive)
        {
            // Fixed mode - use original position minus radius for top-left positioning
            return originalPosition - ballRadius;
        }

        // Responsive mode - scale position proportionally
        const double originalCanvasWidth = 640.0;
        double scaleFactor = currentCanvasWidth / originalCanvasWidth;
        double scaledPosition = originalPosition * scaleFactor;
        double scaledRadius = ballRadius * scaleFactor;

        // Return top-left position (center - radius) for Canvas positioning
        return scaledPosition - scaledRadius;
    });

    /// <summary>
    /// Converts ball Y position and canvas dimensions to responsive canvas Y position
    /// First binding: Ball Y position (0-480)
    /// Second binding: Ball radius
    /// Third binding: Current canvas width (used to calculate height via aspect ratio)
    /// Fourth binding: IsResponsive flag
    /// </summary>
    public static readonly FuncMultiValueConverter<object?, double> ResponsiveBallYPositionConverter = new(values =>
    {
        var valueList = values.ToList();
        if (valueList.Count < 4) return 0;

        if (!double.TryParse(valueList[0]?.ToString(), out double originalYPosition)) return 0;
        if (!double.TryParse(valueList[1]?.ToString(), out double ballRadius)) ballRadius = 10;
        if (!double.TryParse(valueList[2]?.ToString(), out double currentCanvasWidth)) return 0;
        if (valueList[3] is not bool isResponsive) return 0;

        if (!isResponsive)
        {
            // Fixed mode - use original position minus radius for top-left positioning
            return originalYPosition - ballRadius;
        }

        // Responsive mode - scale position proportionally
        const double originalCanvasWidth = 640.0;
        double scaleFactor = currentCanvasWidth / originalCanvasWidth;
        double scaledYPosition = originalYPosition * scaleFactor;
        double scaledRadius = ballRadius * scaleFactor;

        // Return top-left position (center - radius) for Canvas positioning
        return scaledYPosition - scaledRadius;
    });

    /// <summary>
    /// Converts ball radius to responsive size
    /// First binding: Ball radius
    /// Second binding: Current canvas width
    /// Third binding: IsResponsive flag
    /// </summary>
    public static readonly FuncMultiValueConverter<object?, double> ResponsiveBallSizeConverter = new(values =>
    {
        var valueList = values.ToList();
        if (valueList.Count < 3) return 20; // Default diameter

        if (!double.TryParse(valueList[0]?.ToString(), out double ballRadius)) ballRadius = 10;
        if (!double.TryParse(valueList[1]?.ToString(), out double currentCanvasWidth)) return ballRadius * 2;
        if (valueList[2] is not bool isResponsive) return ballRadius * 2;

        if (!isResponsive)
        {
            // Fixed mode - use original size (diameter = radius * 2)
            return ballRadius * 2;
        }

        // Responsive mode - scale size proportionally
        const double originalCanvasWidth = 640.0;
        double scaleFactor = currentCanvasWidth / originalCanvasWidth;
        double scaledRadius = ballRadius * scaleFactor;

        // Return diameter (radius * 2)
        return scaledRadius * 2;
    });

    /// <summary>
    /// Converts canvas width to horizontal crosshair start point (responsive)
    /// </summary>
    public static readonly FuncValueConverter<double, Point> ResponsiveHorizontalCrosshairStartConverter =
        new(canvasWidth => new Point(0, canvasWidth * 0.75 / 2));

    /// <summary>
    /// Converts canvas width to horizontal crosshair end point (responsive)
    /// </summary>
    public static readonly FuncValueConverter<double, Point> ResponsiveHorizontalCrosshairEndConverter =
        new(canvasWidth => new Point(canvasWidth, canvasWidth * 0.75 / 2));

    /// <summary>
    /// Converts canvas width to vertical crosshair start point (responsive)
    /// </summary>
    public static readonly FuncValueConverter<double, Point> ResponsiveVerticalCrosshairStartConverter =
        new(canvasWidth => new Point(canvasWidth / 2, 0));

    /// <summary>
    /// Converts canvas width to vertical crosshair end point (responsive)
    /// </summary>
    public static readonly FuncValueConverter<double, Point> ResponsiveVerticalCrosshairEndConverter =
        new(canvasWidth => new Point(canvasWidth / 2, canvasWidth * 0.75));

    /// <summary>
    /// Converts boolean to Stretch mode for responsive canvas
    /// true = Uniform (responsive), false = None (fixed size)
    /// </summary>
    public static readonly FuncValueConverter<bool, Stretch> ResponsiveStretchConverter =
        new(isResponsive => isResponsive ? Stretch.Uniform : Stretch.None);

    /// <summary>
    /// Converts radius to diameter (radius * 2)
    /// </summary>
    public static readonly FuncValueConverter<double, double> RadiusToDiameterConverter =
        new(radius => radius * 2);

    /// <summary>
    /// Converts responsive mode to size information display
    /// true = "640×480 (Responsive)", false = "640×480"
    /// </summary>
    public static readonly FuncValueConverter<bool, string> BoolToSizeInfoConverter =
        new(isResponsive => isResponsive ? "640×480 (Responsive)" : "640×480");

    /// <summary>
    /// Converts boolean to Yes/No string
    /// </summary>
    public static readonly FuncValueConverter<bool, string> BoolToYesNoConverter =
        new(value => value ? "Yes" : "No");

    /// <summary>
    /// Converts boolean to streaming status string
    /// </summary>
    public static readonly FuncValueConverter<bool, string> BoolToStreamingStatusConverter =
        new(isStreaming => isStreaming ? "Streaming" : "Stopped");

    /// <summary>
    /// Converts boolean to success/danger brush for status display
    /// </summary>
    public static readonly FuncValueConverter<bool, IBrush> BoolToSuccessDangerBrushConverter =
        new(isSuccess => isSuccess
            ? new SolidColorBrush(Color.FromRgb(76, 175, 80))   // Green for success
            : new SolidColorBrush(Color.FromRgb(244, 67, 54))); // Red for danger
}

public static class StringConverters
{
    /// <summary>
    /// Converts string to visibility (visible if not null or empty)
    /// </summary>
    public static readonly FuncValueConverter<string?, bool> IsNotNullOrEmpty =
        new(str => !string.IsNullOrEmpty(str));
}