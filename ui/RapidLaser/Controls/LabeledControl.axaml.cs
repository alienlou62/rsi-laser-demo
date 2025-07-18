using Avalonia.Metadata;

namespace RapidLaser.Controls;

public class LabeledControl : TemplatedControl
{
    public static readonly StyledProperty<string> LabelProperty =
        AvaloniaProperty.Register<LabeledControl, string>(nameof(Label));

    public static readonly StyledProperty<object?> ContentProperty =
        AvaloniaProperty.Register<LabeledControl, object?>(nameof(Content));

    public static readonly StyledProperty<double> LabelWidthProperty =
        AvaloniaProperty.Register<LabeledControl, double>(nameof(LabelWidth), 60);

    public string Label
    {
        get => GetValue(LabelProperty);
        set => SetValue(LabelProperty, value);
    }

    [Content]
    public object? Content
    {
        get => GetValue(ContentProperty);
        set => SetValue(ContentProperty, value);
    }

    public double LabelWidth
    {
        get => GetValue(LabelWidthProperty);
        set => SetValue(LabelWidthProperty, value);
    }
}