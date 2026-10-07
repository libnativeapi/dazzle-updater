// The application itself: what App.xaml merges, the theme a screenshot run may ask for,
// and the window the user is looking at.
using Microsoft.UI.Xaml;

namespace DazzleUpdater.WinUI3UpdaterUiExample;

public partial class App : Application
{
    private Window? _host;

    public App()
    {
        Startup.Initialize();
        InitializeComponent();
        Startup.Log("theme resources merged");
        UnhandledException += (_, error) => Startup.Log($"unhandled: {error.Message}");
        // A screenshot run can ask for the dark theme on a light machine.
        if (Environment.GetEnvironmentVariable("DAZZLE_DEMO_THEME") == "dark")
        {
            RequestedTheme = ApplicationTheme.Dark;
            Startup.Log("theme: dark, as asked");
        }
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        try
        {
            Startup.Log("OnLaunched");
            _host = new HostWindow();
            _host.Activate();
            // A host that checks on launch is a normal host; it is also how a run without a
            // hand on the mouse reaches the module's windows.
            switch (Environment.GetEnvironmentVariable("DAZZLE_DEMO_CHECK"))
            {
                case "1":
                    Startup.Log($"check: UpdaterUI.CheckForUpdates() -> {Core.CheckForUpdates()}");
                    break;
                case "background":
                    // A silent check, which is what asks the permission question the first time.
                    Startup.Log($"check: UpdaterUI.CheckForUpdatesInBackground() -> {Core.CheckForUpdatesInBackground()}");
                    break;
            }
        }
        catch (Exception error)
        {
            Startup.Log($"OnLaunched failed: {error}");
            throw;
        }
    }

}
