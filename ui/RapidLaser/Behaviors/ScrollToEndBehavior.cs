
namespace RapidLaser.Behaviors;

public static class ScrollToEndBehavior
{
    // Track scroll state for each ScrollViewer
    private static readonly Dictionary<ScrollViewer, ScrollState> _scrollStates = new();

    public static readonly AttachedProperty<bool> IsEnabledProperty =
        AvaloniaProperty.RegisterAttached<ScrollViewer, bool>(
            "IsEnabled",
            typeof(ScrollToEndBehavior),
            false);

    public static bool GetIsEnabled(ScrollViewer element)
    {
        return element.GetValue(IsEnabledProperty);
    }

    public static void SetIsEnabled(ScrollViewer element, bool value)
    {
        element.SetValue(IsEnabledProperty, value);
    }

    static ScrollToEndBehavior()
    {
        IsEnabledProperty.Changed.AddClassHandler<ScrollViewer>(OnIsEnabledChanged);
    }

    private static void OnIsEnabledChanged(ScrollViewer scrollViewer, AvaloniaPropertyChangedEventArgs e)
    {
        if (e.NewValue is bool isEnabled && isEnabled)
        {
            // Attach event handlers
            var scrollState = new ScrollState();
            _scrollStates[scrollViewer] = scrollState;

            scrollViewer.ScrollChanged += OnScrollChanged;
            scrollViewer.SizeChanged += OnSizeChanged;
            scrollViewer.PropertyChanged += OnPropertyChanged;

            // Initial scroll to end when enabled
            ScrollToEnd(scrollViewer, scrollState);
        }
        else
        {
            // Detach event handlers
            scrollViewer.ScrollChanged -= OnScrollChanged;
            scrollViewer.SizeChanged -= OnSizeChanged;
            scrollViewer.PropertyChanged -= OnPropertyChanged;

            // Remove scroll state
            _scrollStates.Remove(scrollViewer);
        }
    }

    private static void OnScrollChanged(object? sender, ScrollChangedEventArgs e)
    {
        if (sender is ScrollViewer scrollViewer &&
            _scrollStates.TryGetValue(scrollViewer, out var scrollState) &&
            !scrollState.IsScrolling)
        {
            // Check if user has scrolled away from the bottom
            // Use a small tolerance (5px) to account for rounding errors
            scrollState.ShouldScrollToEnd = Math.Abs(scrollViewer.Offset.Y - (scrollViewer.Extent.Height - scrollViewer.Viewport.Height)) < 5;
        }
    }

    private static void OnPropertyChanged(object? sender, AvaloniaPropertyChangedEventArgs e)
    {
        if (sender is ScrollViewer scrollViewer &&
            _scrollStates.TryGetValue(scrollViewer, out var scrollState) &&
            scrollState.ShouldScrollToEnd &&
            (e.Property.Name == "Extent" || e.Property.Name == "ExtentHeight"))
        {
            // Content size changed, scroll to end if we should
            ScrollToEnd(scrollViewer, scrollState);
        }
    }

    private static void OnSizeChanged(object? sender, SizeChangedEventArgs e)
    {
        if (sender is ScrollViewer scrollViewer &&
            _scrollStates.TryGetValue(scrollViewer, out var scrollState) &&
            scrollState.ShouldScrollToEnd)
        {
            ScrollToEnd(scrollViewer, scrollState);
        }
    }

    private static void ScrollToEnd(ScrollViewer scrollViewer, ScrollState scrollState)
    {
        scrollState.IsScrolling = true;

        // Scroll to the bottom
        scrollViewer.ScrollToEnd();

        scrollState.IsScrolling = false;
    }

    private class ScrollState
    {
        public bool ShouldScrollToEnd { get; set; } = true;
        public bool IsScrolling { get; set; }
    }
}
