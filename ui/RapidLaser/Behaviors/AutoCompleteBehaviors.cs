using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Reflection;

namespace RapidLaser.Behaviors;

public static class AutoCompleteBehaviors
{
    // Track which AutoCompleteBoxes are currently closing to prevent reopening
    private static readonly HashSet<AutoCompleteBox> _closingDropdowns = new();

    public static readonly AttachedProperty<bool> ShowSuggestionsOnClickProperty =
        AvaloniaProperty.RegisterAttached<AutoCompleteBox, bool>(
            "ShowSuggestionsOnClick",
            typeof(AutoCompleteBehaviors),
            false);

    public static bool GetShowSuggestionsOnClick(AutoCompleteBox element)
    {
        return element.GetValue(ShowSuggestionsOnClickProperty);
    }

    public static void SetShowSuggestionsOnClick(AutoCompleteBox element, bool value)
    {
        element.SetValue(ShowSuggestionsOnClickProperty, value);
    }

    static AutoCompleteBehaviors()
    {
        ShowSuggestionsOnClickProperty.Changed.AddClassHandler<AutoCompleteBox>(OnShowSuggestionsOnClickChanged);
    }

    private static void OnShowSuggestionsOnClickChanged(AutoCompleteBox autoCompleteBox, AvaloniaPropertyChangedEventArgs e)
    {
        if (e.NewValue is bool showOnClick && showOnClick)
        {
            // Attach event handlers
            autoCompleteBox.IsTextCompletionEnabled = false;
            autoCompleteBox.GotFocus += OnAutoCompleteBoxGotFocus;
            autoCompleteBox.PointerPressed += OnAutoCompleteBoxPointerPressed;
            autoCompleteBox.DropDownOpening += OnAutoCompleteBoxDropDownOpening;
            autoCompleteBox.DropDownClosed += OnAutoCompleteBoxDropDownClosed;
        }
        else
        {
            // Detach event handlers
            autoCompleteBox.GotFocus -= OnAutoCompleteBoxGotFocus;
            autoCompleteBox.PointerPressed -= OnAutoCompleteBoxPointerPressed;
            autoCompleteBox.DropDownOpening -= OnAutoCompleteBoxDropDownOpening;
            autoCompleteBox.DropDownClosed -= OnAutoCompleteBoxDropDownClosed;
        }
    }

    // Behavior 1: Open dropdown on focus/click
    private static void OnAutoCompleteBoxGotFocus(object? sender, GotFocusEventArgs e)
    {
        if (sender is AutoCompleteBox autoCompleteBox && !_closingDropdowns.Contains(autoCompleteBox))
        {
            OpenDropdown(autoCompleteBox);
        }
    }

    private static void OnAutoCompleteBoxPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is AutoCompleteBox autoCompleteBox && !_closingDropdowns.Contains(autoCompleteBox))
        {
            OpenDropdown(autoCompleteBox);
        }
    }

    private static void OnAutoCompleteBoxDropDownOpening(object? sender, CancelEventArgs e)
    {
        // Cancel if readonly
        if (sender is AutoCompleteBox autoCompleteBox)
        {
            var prop = autoCompleteBox.GetType().GetProperty("TextBox", BindingFlags.Instance | BindingFlags.NonPublic);
            var tb = (TextBox?)prop?.GetValue(autoCompleteBox);
            if (tb is not null && tb.IsReadOnly)
            {
                e.Cancel = true;
            }
        }
    }

    // Behavior 2: Lose focus on item selection
    private static void OnAutoCompleteBoxDropDownClosed(object? sender, EventArgs e)
    {
        if (sender is not AutoCompleteBox autoCompleteBox) return;

        // Mark as closing to prevent immediate reopening
        _closingDropdowns.Add(autoCompleteBox);

        // Remove focus after dropdown closes
        Dispatcher.UIThread.Post(() =>
        {
            if (autoCompleteBox.IsFocused)
            {
                // Find any other focusable control in the parent to focus
                var parent = autoCompleteBox.Parent;
                while (parent != null)
                {
                    if (parent is Panel panel)
                    {
                        // Look for any other focusable control in the same panel
                        foreach (var child in panel.Children)
                        {
                            if (child is Control control && control != autoCompleteBox && control.Focusable && control.IsVisible)
                            {
                                control.Focus();
                                goto focused;
                            }
                        }
                    }
                    parent = parent.Parent;
                }

                // If no sibling found, focus the top level
                var topLevel = TopLevel.GetTopLevel(autoCompleteBox);
                topLevel?.Focus();

            focused:;
            }
        }, DispatcherPriority.Loaded);

        // Clear closing flag after delay
        System.Threading.Tasks.Task.Delay(100).ContinueWith(_ => _closingDropdowns.Remove(autoCompleteBox));
    }

    private static void OpenDropdown(AutoCompleteBox autoCompleteBox)
    {
        if (!autoCompleteBox.IsDropDownOpen)
        {
            // Use reflection to call internal methods for reliable dropdown opening
            typeof(AutoCompleteBox).GetMethod("PopulateDropDown", BindingFlags.NonPublic | BindingFlags.Instance)
                ?.Invoke(autoCompleteBox, new object[] { autoCompleteBox, EventArgs.Empty });

            typeof(AutoCompleteBox).GetMethod("OpeningDropDown", BindingFlags.NonPublic | BindingFlags.Instance)
                ?.Invoke(autoCompleteBox, new object[] { false });

            if (!autoCompleteBox.IsDropDownOpen)
            {
                // Set the internal field to avoid property change events
                var ipc = typeof(AutoCompleteBox).GetField("_ignorePropertyChange", BindingFlags.NonPublic | BindingFlags.Instance);
                if (ipc?.GetValue(autoCompleteBox) is bool ipcValue && !ipcValue)
                {
                    ipc.SetValue(autoCompleteBox, true);
                }

                autoCompleteBox.SetCurrentValue(AutoCompleteBox.IsDropDownOpenProperty, true);
            }
        }
    }
}
