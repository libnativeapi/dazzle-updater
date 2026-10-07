// The Windows look, as values rather than theme-resource lookups: the prototype's
// Windows tokens (prototype/src/styles/native.css) and the Fluent metrics the spec names.
//
// Named Fluent resources ("BodyStrongTextBlockStyle", "AccentButtonStyle", …) are not
// reachable from a code-only, unpackaged WinUI 3 app — merging XamlControlsResources from
// the Application constructor throws there — so the windows state what they mean instead.
// specs/updater-ui.md lists the Fluent field each value corresponds to.
using Microsoft.UI.Text;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using Windows.UI;
using Windows.UI.Text;
using Windows.UI.ViewManagement;

namespace Dazzle.WinUI3UpdaterUiExample;

internal static class Theme
{
    internal const double Margin = 20;
    internal const double IconSize = 64;
    internal const double IconGap = 8;
    internal const double RowGap = 8;
    internal const double ButtonGap = 8;            // Windows: 8 (macOS: 12)
    internal const double MinimumButtonWidth = 120;

    /// <summary>Type and controls: the Fluent sizes, in DIPs.</summary>
    internal const double BodySize = 14;            // Body
    internal const double CaptionSize = 12;         // Caption
    internal const double SubtitleSize = 20;        // Subtitle
    internal const double ButtonHeight = 32;        // ButtonHeight
    internal const double ButtonRadius = 4;         // ControlCornerRadius
    internal static FontWeight HeadingWeight => FontWeights.SemiBold;   // BodyStrong

    private static bool Dark => Application.Current.RequestedTheme == ApplicationTheme.Dark;

    internal static Brush Text => Solid(Dark ? "#FFFFFF" : "#1B1B1B");
    internal static Brush SecondaryText => Solid(Dark ? "#C5C5C5" : "#5D5D5D");
    internal static Brush CardBackground => Solid(Dark ? "#2B2B2B" : "#FFFFFF");
    internal static Brush CardStroke => Solid(Dark ? "#3D3D3D" : "#E5E5E5");
    internal static Brush ControlFill => Solid(Dark ? "#2D2D2D" : "#FBFBFB");
    internal static Brush ControlStroke => Solid(Dark ? "#3D3D3D" : "#E5E5E5");
    internal static Brush OnAccent => Solid(Dark ? "#000000" : "#FFFFFF");

    /// <summary>The user's accent colour, as the system reports it.</summary>
    internal static Brush Accent
    {
        get
        {
            try
            {
                return new SolidColorBrush(new UISettings().GetColorValue(UIColorType.Accent));
            }
            catch (Exception error)
            {
                Startup.Log($"accent: {error.GetType().Name}");
                return Solid("#0F6CBD");
            }
        }
    }

    /// <summary>The application's own icon, as the spec asks the windows to show it.</summary>
    internal static ImageSource? AppIcon()
    {
        try
        {
            var path = Environment.ProcessPath;
            if (path is null)
            {
                return null;
            }
            using var icon = System.Drawing.Icon.ExtractAssociatedIcon(path);
            if (icon is null)
            {
                return null;
            }
            using var bitmap = icon.ToBitmap();
            using var stream = new MemoryStream();
            bitmap.Save(stream, System.Drawing.Imaging.ImageFormat.Png);
            stream.Position = 0;
            var image = new Microsoft.UI.Xaml.Media.Imaging.BitmapImage();
            image.SetSource(stream.AsRandomAccessStream());
            return image;
        }
        catch (Exception error)
        {
            Startup.Log($"icon: {error.GetType().Name}");
            return null;
        }
    }

    private static SolidColorBrush Solid(string hex)
    {
        var value = uint.Parse(hex.TrimStart('#'), System.Globalization.NumberStyles.HexNumber);
        return new SolidColorBrush(Color.FromArgb(
            255,
            (byte)((value >> 16) & 0xFF),
            (byte)((value >> 8) & 0xFF),
            (byte)(value & 0xFF)));
    }
}
