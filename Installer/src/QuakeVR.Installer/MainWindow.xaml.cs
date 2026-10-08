using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace QuakeVR.Installer;

public partial class MainWindow
{
    // The window's size (DIPs), and its frame (title bar and borders) on Windows 11: the screenshot harness renders a
    // client area of the same size.
    public const double DefaultWidth = 1200, DefaultHeight = 850, MinimumWidth = 1000, MinimumHeight = 720;
    public const double FrameWidth = 16, FrameHeight = 40;

    public MainWindow()
    {
        InitializeComponent();
        FitToWorkArea(SystemParameters.WorkArea);
        SourceInitialized += (_, _) => DarkTitleBar(new WindowInteropHelper(this).Handle);
    }

    /// <summary>Never larger than the screen's work area (in DIPs, so a short screen or high scaling: 1366 x 768 at
    /// 100% leaves 720 above the taskbar, 1080p at 150% only 672): the pages and the sidebar scroll there.</summary>
    void FitToWorkArea(Rect area)
    {
        MinWidth = Math.Min(MinWidth, area.Width);
        MinHeight = Math.Min(MinHeight, area.Height);
        Width = Math.Min(Width, area.Width);
        Height = Math.Min(Height, area.Height);
    }

    /// <summary>A dark title bar matching the sidebar (Windows 10 20H1+ / 11; ignored elsewhere).</summary>
    static void DarkTitleBar(IntPtr hwnd)
    {
        var on = 1;
        _ = DwmSetWindowAttribute(hwnd, 20, ref on, sizeof(int)); // DWMWA_USE_IMMERSIVE_DARK_MODE
        var caption = 0x00080A0C; // COLORREF 0x00BBGGRR: the skin's soot black #0C0A08
        _ = DwmSetWindowAttribute(hwnd, 35, ref caption, sizeof(int)); // DWMWA_CAPTION_COLOR (Windows 11)
    }

    [DllImport("dwmapi.dll")]
    static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);
}
