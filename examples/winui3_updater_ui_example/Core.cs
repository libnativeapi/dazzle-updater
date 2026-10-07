// Every touch of the generated C# binding lives here, in one small class.
//
// Two reasons. The native library may be missing (a build without it, a machine where
// dazzle.dll did not travel), and .NET resolves a referenced type when the method that
// names it is compiled — not where the caller's try block is. Keeping the reference in its
// own method means the caller's try/catch can catch the failure and say so in the log.
using Dazzle;

namespace Dazzle.WinUI3UpdaterUiExample;

internal static class Core
{
    /// <summary>The application's name as core reports it, or null.</summary>
    internal static string? AppName()
    {
        var name = UpdaterUI.Instance.AppName;
        return string.IsNullOrEmpty(name) ? null : name;
    }

    /// <summary>
    /// Asks the module to check for updates; the windows that follow are the module's.
    /// </summary>
    internal static bool CheckForUpdates() => UpdaterUI.Instance.CheckForUpdates();

    /// <summary>
    /// A check the user did not ask for: the one that asks the permission question when it
    /// has never been answered.
    /// </summary>
    internal static bool CheckForUpdatesInBackground() =>
        UpdaterUI.Instance.CheckForUpdatesInBackground();

    /// <summary>Lets the module draw what it was asked for from another thread.</summary>
    internal static void Pump() => UpdaterUI.Instance.Pump();

    /// <summary>
    /// Hands the module this application's XAML thread: WinUI 3 windows can only be created
    /// on the thread that started the Application, and without it the module has nowhere to
    /// draw.
    /// </summary>
    internal static void SetDispatcherQueue(nint queue) =>
        UpdaterUI.Instance.SetDispatcherQueue((long)queue);

    /// <summary>The host application's name, the one the windows then show.</summary>
    internal static void SetAppName(string name) => UpdaterUI.Instance.AppName = name;

    internal static bool AutomaticDownloads
    {
        get => UpdaterUI.Instance.IsAutomaticDownloadsEnabled;
        set => UpdaterUI.Instance.IsAutomaticDownloadsEnabled = value;
    }

    /// <summary>What core answers at startup: the updater's state and one real check.</summary>
    internal static string Describe()
    {
        var updater = Updater.Instance;
        var started = updater.CheckForUpdates();
        return $"source={updater.Source ?? "(none)"} current={updater.CurrentVersion ?? "(none)"} " +
               $"appName={UpdaterUI.Instance.AppName ?? "(none)"} " +
               $"automaticChecks={UpdaterUI.Instance.IsAutomaticChecksEnabled} " +
               $"automaticDownloads={UpdaterUI.Instance.IsAutomaticDownloadsEnabled} " +
               $"CheckForUpdates()={started} error={updater.ErrorCode ?? "(none)"}: {updater.ErrorMessage ?? "(none)"}";
    }
}
