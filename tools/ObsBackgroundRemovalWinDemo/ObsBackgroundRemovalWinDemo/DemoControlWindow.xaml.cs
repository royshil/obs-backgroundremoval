using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using FlaUI.UIA3;
using NAudio.Wave;
using OpenQA.Selenium.Edge;
using System.Diagnostics;
using System.IO;
using System.Media;
using System.Windows;

namespace ObsBackgroundRemovalWinDemo
{
    public partial class DemoControlWindow : System.Windows.Window
    {
        private readonly DemoOverlay _demoOverlay;
        private CancellationTokenSource _cts = new();
        private Task? _task;
        private bool isRunning = false;

        public DemoControlWindow(DemoOverlay demoOverlay)
        {
            InitializeComponent();

            _demoOverlay = demoOverlay;
        }

        private void RunDemo(CancellationToken token)
        {
            try
            {
                var app = FlaUI.Core.Application.Launch(
                    @"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
                    "http://royshil.github.io/obs-backgroundremoval/ --inprivate --new-window --force-renderer-accessibility --start-maximized --remote-debugging-port=9222");

                var options = new EdgeOptions
                {
                    DebuggerAddress = "localhost:9222"
                };

                using var driver = new EdgeDriver(options);

                var waveOut = new WaveOutEvent();
                var silence = new SilenceProvider(WaveFormat.CreateIeeeFloatWaveFormat(48000, 2));
                waveOut.Init(silence);
                waveOut.Play();

                var automation = new UIA3Automation();

                var startNavigationSound = new SoundPlayer(@"C:\Windows\Media\Windows Navigation Start.wav");
                var desktop = automation.GetDesktop();

                Thread.Sleep(2000);

                var window = desktop
                    .FindAllChildren()
                    .First(w => w.Name.StartsWith("OBS Background Removal"));

                var windowsLink = window.FindFirstDescendant(cf => cf.ByName("Windows"))!;
                windowsLink.Patterns.ScrollItem.Pattern.ScrollIntoView();
                driver.ExecuteScript("document.querySelector('a[href*=windows]').scrollIntoView({behavior:'smooth'});");
                Thread.Sleep(2000);
                ClickSmoothly(windowsLink);
                startNavigationSound.Play();

                Thread.Sleep(1000);

                var zipLink = window.FindFirstDescendant(cf => cf.ByName("Click here to download the latest Windows version (ZIP)"))!;
                Thread.Sleep(100);
                ClickSmoothly(zipLink);
                startNavigationSound.Play();

                Thread.Sleep(12000);
            }
            catch
            {
                throw;
            }
            finally
            {
                Application.Current.Dispatcher.Invoke(() =>
                {
                    Application.Current.Shutdown();
                });
            }
        }

        private static void ClickSmoothly(AutomationElement element)
        {
            using (Keyboard.Pressing(VirtualKeyShort.LCONTROL))
            {
                Mouse.MoveTo(element.GetClickablePoint());
                Thread.Sleep(1000);
            }
            element.Click();
        }

        private void StartStopButton_Click(object sender, RoutedEventArgs e)
        {
            if (isRunning)
            {
                _cts.Cancel();
            }
            else
            {
                isRunning = true;
                StartStopButton.Content = "■";

                var token = _cts.Token;
                _task = Task.Run(() => RunDemo(token), token);
            }
        }

        public static void ClearEnvironment()
        {
            var downloadPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");

            var di = new DirectoryInfo(downloadPath);

            foreach (var file in di.GetFiles())
            {
                file.Delete();
            }

            foreach (var dir in di.GetDirectories())
            {
                dir.Delete(true);
            }

            var runningEdges = Process.GetProcessesByName("msedge");

            foreach (var proc in runningEdges)
            {
                proc.CloseMainWindow();
            }
        }
    }
}
