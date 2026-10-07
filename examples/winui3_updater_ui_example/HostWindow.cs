// The host application, and nothing else: dazzle's update windows are core's now
// (core/src/platform/windows/updater_ui_backend_winui3.cpp), drawn by the state machine
// from UiContent. This window is what an application that uses the module looks like — its
// own name and version, and the call.
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Dazzle.WinUI3UpdaterUiExample;

internal sealed partial class HostWindow : Window
{
    internal HostWindow()
    {
        Title = Startup.HostApplicationName;
        Windowing.UseSystemBackdrop(this);

        var panel = new StackPanel
        {
            Spacing = Theme.RowGap,
            Padding = new Thickness(Theme.Margin),
        };

        var header = new StackPanel { Orientation = Orientation.Horizontal, Spacing = Theme.IconGap };
        if (Theme.AppIcon() is { } icon)
        {
            header.Children.Add(new Image { Source = icon, Width = Theme.IconSize, Height = Theme.IconSize });
        }
        var title = new StackPanel { Spacing = 2, VerticalAlignment = VerticalAlignment.Center };
        title.Children.Add(new TextBlock
        {
            Text = Startup.HostApplicationName,
            FontSize = Theme.SubtitleSize,
        });
        title.Children.Add(new TextBlock
        {
            Text = "A host application. The update windows would be dazzle's.",
            FontSize = Theme.CaptionSize,
            Foreground = Theme.SecondaryText,
        });
        header.Children.Add(title);
        panel.Children.Add(header);

        var actions = new StackPanel { Orientation = Orientation.Horizontal, Spacing = Theme.RowGap };
        var check = new Button
        {
            Content = "Check for Updates…",
            MinWidth = 150,
            MinHeight = Theme.ButtonHeight,
            CornerRadius = new CornerRadius(Theme.ButtonRadius),
            Background = Theme.Accent,
            Foreground = Theme.OnAccent,
        };
        check.Click += (_, _) =>
        {
            // Everything the user sees after this is the module's: the state machine picks
            // the state, core builds the content, the Windows backend draws the window.
            var started = Core.CheckForUpdates();
            Startup.Log($"check: UpdaterUI.CheckForUpdates() -> {started}");
        };
        actions.Children.Add(check);
        panel.Children.Add(actions);

        Content = panel;
        Windowing.ResizeContent(this, 420, 170);
        Windowing.CenterOnScreen(this);
    }
}
