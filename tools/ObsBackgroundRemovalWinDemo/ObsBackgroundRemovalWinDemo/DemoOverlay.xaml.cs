namespace ObsBackgroundRemovalWinDemo
{
    public partial class DemoOverlay : System.Windows.Window
    {
        public DemoOverlay()
        {
            InitializeComponent();

            this.Loaded += (s, e) =>
            {
                var demoControlWindow = new DemoControlWindow(this);
                demoControlWindow.Owner = this;
                demoControlWindow.Show();
            };
        }
    }
}