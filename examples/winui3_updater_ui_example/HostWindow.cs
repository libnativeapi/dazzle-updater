// The example owns only its application window. UpdaterUI owns every update window,
// its controls, release notes, wording and dispatch in core/src/platform/windows/.
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;

namespace DazzleUpdater.WinUI3UpdaterUiExample;

internal sealed class HostWindow : Window
{
    internal HostWindow()
    {
        Title = Startup.HostApplicationName;
        SystemBackdrop = new MicaBackdrop();

        var panel = new StackPanel { Spacing = 12, Padding = new Thickness(20) };
        panel.Children.Add(new TextBlock { Text = Startup.HostApplicationName, FontSize = 20 });
        panel.Children.Add(new TextBlock
        {
            Text = "UpdaterUI in core provides the update windows.",
            TextWrapping = TextWrapping.Wrap,
        });

        var check = new Button { Content = "Check for Updates…" };
        check.Click += (_, _) =>
            Startup.Log($"check: UpdaterUI.CheckForUpdates() -> {Core.CheckForUpdates()}");
        panel.Children.Add(check);

        Content = panel;
        AppWindow.Resize(new SizeInt32(640, 300));
    }
}
