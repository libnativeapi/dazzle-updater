// The application itself: what App.xaml merges, the theme a screenshot run may ask for,
// and the window the user is looking at.
using System.Runtime.InteropServices;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;

namespace Dazzle.WinUI3UpdaterUiExample;

public partial class App : Application
{
    private Window? _host;
    private DispatcherTimer? _pump;

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
            // The module draws on this thread or not at all.
            Core.SetDispatcherQueue(Marshal.GetIUnknownForObject(DispatcherQueue.GetForCurrentThread()));
            // Dazzle's windows live on this thread, so a state the module reached on another
            // one — an update that failed to check, a download that finished — is drawn when
            // the host pumps, which is what this timer is for.
            _pump = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
            _pump.Tick += (_, _) => Core.Pump();
            _pump.Start();
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
