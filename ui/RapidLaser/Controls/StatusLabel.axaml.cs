namespace RapidLaser.Controls;

public class StatusLabel : TemplatedControl
{
    public static readonly StyledProperty<string> TitleProperty =
        AvaloniaProperty.Register<StatusLabel, string>(nameof(Title), string.Empty);

    public static readonly StyledProperty<string> ValueProperty =
        AvaloniaProperty.Register<StatusLabel, string>(nameof(Value), string.Empty);

    public static readonly StyledProperty<bool?> IsActiveProperty =
        AvaloniaProperty.Register<StatusLabel, bool?>(nameof(IsActive), null);

    public static readonly StyledProperty<bool> IsValueVisibleProperty =
        AvaloniaProperty.Register<StatusLabel, bool>(nameof(IsValueVisible), true);

    public string Title
    {
        get => GetValue(TitleProperty);
        set => SetValue(TitleProperty, value);
    }

    public string Value
    {
        get => GetValue(ValueProperty);
        set => SetValue(ValueProperty, value);
    }

    public bool? IsActive
    {
        get => GetValue(IsActiveProperty);
        set => SetValue(IsActiveProperty, value);
    }

    public bool IsValueVisible
    {
        get => GetValue(IsValueVisibleProperty);
        set => SetValue(IsValueVisibleProperty, value);
    }
}
