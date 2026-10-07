// Every touch of the generated C# binding lives here, in one small class.
//
// Two reasons. The native library may be missing (a build without it, a machine where
// dazzle_updater.dll did not travel), and .NET resolves a referenced type when the method that
// names it is compiled — not where the caller's try block is. Keeping the reference in its
// own method means the caller's try/catch can catch the failure and say so in the log.
using DazzleUpdater;

namespace DazzleUpdater.WinUI3UpdaterUiExample;

internal static class Core
{
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

    /// <summary>The host application's name, the one the windows then show.</summary>
    internal static void SetAppName(string name) => UpdaterUI.Instance.AppName = name;

    /// <summary>
    /// Points the updater at a release index, when the environment names one: the feed and
    /// key a run against a local signed index needs (see README, "Against a local feed").
    /// Without them the application has no source, and a check ends in the Error window.
    /// </summary>
    internal static void Configure()
    {
        if (Environment.GetEnvironmentVariable("DAZZLE_DEMO_FEED") is { Length: > 0 } feed)
        {
            ReleaseIndexSource.Instance.Url = feed;
        }
        if (Environment.GetEnvironmentVariable("DAZZLE_DEMO_PUBLIC_KEY") is { Length: > 0 } key)
        {
            Updater.Instance.PublicKey = key;
        }
        if (Environment.GetEnvironmentVariable("DAZZLE_DEMO_VERSION") is { Length: > 0 } version)
        {
            Updater.Instance.CurrentVersion = version;
        }
    }

    /// <summary>What core answers at startup: the updater's configuration, not a check —
    /// a check blocks, and this runs on the UI thread.</summary>
    internal static string Describe()
    {
        var updater = Updater.Instance;
        return $"source={updater.Source ?? "(none)"} feed={ReleaseIndexSource.Instance.Url ?? "(none)"} " +
               $"current={updater.CurrentVersion ?? "(none)"} " +
               $"appName={UpdaterUI.Instance.AppName ?? "(none)"} " +
               $"automaticChecks={UpdaterUI.Instance.IsAutomaticChecksEnabled} " +
               $"automaticDownloads={UpdaterUI.Instance.IsAutomaticDownloadsEnabled}";
    }
}
