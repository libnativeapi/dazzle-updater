// What a WinUI 3 window needs that the framework does not take from a XAML size: a
// Mica/acrylic backdrop, a size in DIPs scaled by the monitor's DPI, and the app icon.
using System.Runtime.InteropServices;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;

namespace Dazzle.WinUI3UpdaterUiExample;

internal static partial class Windowing
{
    [LibraryImport("user32.dll")]
    private static partial uint GetDpiForWindow(IntPtr window);

    [LibraryImport("user32.dll")]
    private static partial IntPtr GetForegroundWindow();

    /// <summary>The window's backdrop: Mica where the system has it (specs/updater-ui.md).</summary>
    internal static void UseSystemBackdrop(Window window)
    {
        try
        {
            window.SystemBackdrop = new MicaBackdrop();
        }
        catch (Exception error)
        {
            Startup.Log($"backdrop: {error.GetType().Name}");
        }
    }

    /// <summary>
    /// Resizes to a content size in DIPs, as AppKit's contentRect is.
    /// </summary>
    /// <remarks>
    /// ResizeClient, not Resize: AppWindow.Resize sets the whole window, which leaves the
    /// content short by the title bar — 32 pt of a 128 pt window, and the status window's
    /// buttons fell off the bottom.
    /// </remarks>
    internal static void ResizeContent(Window window, double width, double height)
    {
        var handle = WinRT.Interop.WindowNative.GetWindowHandle(window);
        var scale = GetDpiForWindow(handle) / 96.0;
        var appWindow = AppWindow.GetFromWindowId(
            Microsoft.UI.Win32Interop.GetWindowIdFromWindow(handle));
        appWindow.ResizeClient(new SizeInt32(
            (int)Math.Round(width * scale), (int)Math.Round(height * scale)));
        Startup.Log($"window: content {width:0}x{height:0} at {scale:0.##}x -> " +
                    $"client {appWindow.ClientSize.Width}x{appWindow.ClientSize.Height}, " +
                    $"presenter {appWindow.Presenter.Kind}");
    }

    /// <summary>The height of the title bar we draw, the Fluent caption height.</summary>
    internal const double TitleBarHeight = 32;

    /// <summary>
    /// The modern title bar: the caption area becomes part of our content, so Mica runs
    /// behind it and the system's buttons sit on the material with the title drawn at 12 pt.
    /// </summary>
    /// <remarks>
    /// A WinUI 3 window that does not do this keeps the classic caption bar — an opaque
    /// strip with the app icon — which is what this looked like at first. Sparkle's windows
    /// have a transparent title bar with the title over it too, so this is also the faithful
    /// arrangement: the window's own content keeps the height the spec gives it, and the
    /// title bar is added above it.
    /// </remarks>
    internal static void ExtendTitleBar(Window window, string title, FrameworkElement content)
    {
        window.ExtendsContentIntoTitleBar = true;

        var bar = new Grid { Height = TitleBarHeight, Padding = new Thickness(12, 0, 12, 0) };
        bar.Children.Add(new TextBlock
        {
            Text = title,
            FontSize = Theme.CaptionSize,
            Foreground = Theme.SecondaryText,
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
        });
        window.SetTitleBar(bar);

        var root = new Grid();
        root.RowDefinitions.Add(new RowDefinition { Height = new GridLength(TitleBarHeight) });
        root.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        Grid.SetRow(bar, 0);
        Grid.SetRow(content, 1);
        root.Children.Add(bar);
        root.Children.Add(content);
        window.Content = root;
    }

    /// <summary>
    /// Removes the caption bar, so there is no close button: the permission question has to
    /// be answered. WinUI keeps a thin border, which is what its own dialogs have too.
    /// </summary>
    internal static void HideCaptionBar(Window window)
    {
        var handle = WinRT.Interop.WindowNative.GetWindowHandle(window);
        var appWindow = AppWindow.GetFromWindowId(
            Microsoft.UI.Win32Interop.GetWindowIdFromWindow(handle));
        if (appWindow.Presenter is OverlappedPresenter presenter)
        {
            presenter.SetBorderAndTitleBar(true, false);
            presenter.IsResizable = false;
            presenter.IsMaximizable = false;
            presenter.IsMinimizable = false;
        }
    }

    /// <summary>Brings the window forward without a title-bar dance.</summary>
    internal static void BringForward(Window window)
    {
        window.Activate();
    }

    internal static void CenterOnScreen(Window window)
    {
        var handle = WinRT.Interop.WindowNative.GetWindowHandle(window);
        var appWindow = AppWindow.GetFromWindowId(
            Microsoft.UI.Win32Interop.GetWindowIdFromWindow(handle));
        var area = DisplayArea.GetFromWindowId(appWindow.Id, DisplayAreaFallback.Nearest);
        appWindow.Move(new PointInt32(
            area.WorkArea.X + ((area.WorkArea.Width - appWindow.Size.Width) / 2),
            area.WorkArea.Y + ((area.WorkArea.Height - appWindow.Size.Height) / 2)));
    }
}
