using DazzleUpdater;

var updater = Updater.Instance;
var source = ReleaseIndexSource.Instance;

Console.WriteLine($"source:          {updater.Source ?? "(none)"}");
Console.WriteLine($"current version: {updater.CurrentVersion ?? "(none)"}");
Console.WriteLine($"release index:   {source.Url ?? "(none)"}");
Console.WriteLine($"public key set:  {updater.PublicKey is not null}");
