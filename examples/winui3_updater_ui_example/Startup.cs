// A real WinUI 3 host application for dazzle's update windows: it wires the generated
// C# binding (which builds core/ with CMake and loads dazzle.dll) to a scripted round so
// the three windows can be looked at on Windows, next to the macOS ones.
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;

namespace Dazzle.WinUI3UpdaterUiExample;

public static class Startup
{
    /// <summary>The host application's own name, which core is told and the windows show.</summary>
    internal const string HostApplicationName = "Dazzle Demo";

    /// <summary>What the app did, for runs started without a console.</summary>
    internal static readonly string LogPath =
        Path.Combine(Path.GetTempPath(), "dazzle-winui3-example.log");

    internal static void Log(string line)
    {
        try
        {
            File.AppendAllText(LogPath, $"{DateTime.Now:HH:mm:ss} {line}{Environment.NewLine}");
        }
        catch (IOException)
        {
            // A log is a convenience; never a reason to fail a run.
        }
    }

    /// <summary>
    /// The generated C# binding, loaded and asked for real — this is the C ABI of core, and
    /// the same calls macOS drives its windows with. Windows has no updater platform layer
    /// yet, so a check answers that it is unsupported; everything that does exist is asked
    /// here, and a missing native library only costs the log line.
    /// </summary>
    private static void ProbeCore() => Startup.Log($"core: {Core.Describe()}");

    /// <summary>Everything the XAML-generated entry point cannot know: the log, core.</summary>
    [STAThread]
    internal static void Initialize()
    {
        File.WriteAllText(LogPath, string.Empty);
        Startup.Log($"start: {Environment.OSVersion} / {System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription}");
        AppDomain.CurrentDomain.UnhandledException += (_, error) =>
            Startup.Log($"unhandled: {error.ExceptionObject}");
        try
        {
            Core.SetAppName(HostApplicationName);
            ProbeCore();
        }
        catch (Exception error)
        {
            // The windows still work without core; the log says which half is missing.
            Startup.Log($"core: unavailable ({error.GetType().Name}: {error.Message})");
        }
    }
}
