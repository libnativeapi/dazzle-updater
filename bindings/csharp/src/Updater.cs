namespace Dazzle;

/// <summary>
/// The application's software updater.
/// </summary>
/// <remarks>
/// Checks an appcast feed in the Sparkle format and installs the updates it announces.
/// One instance serves the whole process; get it with <see cref="Instance"/>.
/// </remarks>
public sealed class Updater
{
    private Updater()
    {
    }

    /// <summary>The process-wide updater.</summary>
    public static Updater Instance { get; } = new();

    /// <summary>
    /// The URL of the appcast feed, or <c>null</c> if none is configured.
    /// </summary>
    /// <remarks>
    /// Setting it overrides the app's configuration; set <c>null</c> or <c>""</c> to go
    /// back to the URL the configuration provides (Info.plist <c>SUFeedURL</c> on macOS,
    /// the <c>FeedURL</c> resource of type <c>APPCAST</c> on Windows).
    /// </remarks>
    public string? FeedUrl
    {
        get => CString.Take(NativeMethods.dazzle_updater_get_feed_url());
        set => NativeMethods.dazzle_updater_set_feed_url(CString.TruncateAtNul(value));
    }
}
