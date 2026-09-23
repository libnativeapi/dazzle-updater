using Dazzle;

var updater = Updater.Instance;

Console.WriteLine($"Initial feed URL: {updater.FeedUrl ?? "(none)"}");

updater.FeedUrl = "https://example.com/appcast.xml";
Console.WriteLine($"Feed URL:         {updater.FeedUrl ?? "(none)"}");
