using Xunit;

namespace Dazzle.Tests;

public class UpdaterTests
{
    [Fact]
    public void FeedUrlOverridesAndResets()
    {
        var updater = Updater.Instance;
        updater.FeedUrl = "https://example.com/appcast.xml";
        Assert.Equal("https://example.com/appcast.xml", updater.FeedUrl);

        updater.FeedUrl = null;
        // The test host has no SUFeedURL in its Info.plist, so nothing is configured.
        Assert.Null(updater.FeedUrl);
    }

    [Fact]
    public void FeedUrlRoundTripsNonAsciiText()
    {
        var updater = Updater.Instance;
        updater.FeedUrl = "https://例子.example/更新.xml";
        Assert.Equal("https://例子.example/更新.xml", updater.FeedUrl);
        updater.FeedUrl = null;
    }
}
