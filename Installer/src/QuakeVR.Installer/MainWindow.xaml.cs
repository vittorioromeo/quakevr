using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace QuakeVR.Installer;

public partial class MainWindow
{
    public MainWindow()
    {
        InitializeComponent();
        SourceInitialized += (_, _) => DarkTitleBar(new WindowInteropHelper(this).Handle);
    }

    /// <summary>A dark title bar matching the sidebar (Windows 10 20H1+ / 11; ignored elsewhere).</summary>
    static void DarkTitleBar(IntPtr hwnd)
    {
        var on = 1;
        _ = DwmSetWindowAttribute(hwnd, 20, ref on, sizeof(int)); // DWMWA_USE_IMMERSIVE_DARK_MODE
        var caption = 0x00151110; // COLORREF 0x00BBGGRR: the sidebar's #0F1115
        _ = DwmSetWindowAttribute(hwnd, 35, ref caption, sizeof(int)); // DWMWA_CAPTION_COLOR (Windows 11)
    }

    [DllImport("dwmapi.dll")]
    static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);
}
