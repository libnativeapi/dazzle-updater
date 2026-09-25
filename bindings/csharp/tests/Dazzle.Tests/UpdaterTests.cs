using Xunit;

namespace Dazzle.Tests;

public class UpdaterTests
{
    [Fact]
    public void SourceDefaultsToTheReleaseIndexAndRoundTrips()
    {
        var updater = Updater.Instance;
        updater.Source = null;
        Assert.Equal("release-index", updater.Source);

        updater.Source = "appcast";
        Assert.Equal("appcast", updater.Source);

        updater.Source = null;
        Assert.Equal("release-index", updater.Source);
    }

    [Fact]
    public void ReleaseIndexUrlSetsClearsAndRoundTripsNonAsciiText()
    {
        var source = ReleaseIndexSource.Instance;
        source.Url = "https://example.com/myapp/releases.json";
        Assert.Equal("https://example.com/myapp/releases.json", source.Url);

        source.Url = "https://例子.example/更新.json";
        Assert.Equal("https://例子.example/更新.json", source.Url);

        source.Url = null;
        Assert.Null(source.Url);
    }

    [Fact]
    public void CurrentVersionAndPublicKeyRoundTrip()
    {
        var updater = Updater.Instance;
        updater.CurrentVersion = "1.4.0";
        Assert.Equal("1.4.0", updater.CurrentVersion);

        updater.PublicKey = "11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=";
        Assert.Equal("11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=", updater.PublicKey);
    }

    [Fact]
    public void ActionsRefuseToRunOutOfOrderWithAStableCode()
    {
        var updater = Updater.Instance;
        Assert.False(updater.IsUpdateAvailable);

        Assert.False(updater.DownloadUpdate());
        Assert.Equal("no-update", updater.ErrorCode);

        Assert.False(updater.InstallUpdate());
        Assert.Equal("no-update", updater.ErrorCode);
    }

    [Fact]
    public void AnUnknownSourceFailsTheCheck()
    {
        var updater = Updater.Instance;
        updater.Source = "nope";
        Assert.False(updater.CheckForUpdates());
        Assert.Equal("unknown-source", updater.ErrorCode);
        Assert.False(string.IsNullOrEmpty(updater.ErrorMessage));

        updater.Source = null;
        ReleaseIndexSource.Instance.Url = null;
        Assert.False(updater.CheckForUpdates());
        Assert.Equal("invalid-index", updater.ErrorCode);
    }
}
