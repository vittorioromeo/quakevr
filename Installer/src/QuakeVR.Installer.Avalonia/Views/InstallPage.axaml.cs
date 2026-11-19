using System.Collections.Specialized;
using Avalonia.Controls;
using Avalonia.Threading;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer.Views;

public partial class InstallPage : UserControl
{
    public InstallPage()
    {
        InitializeComponent();
        // Keep the newest log line in view.
        DataContextChanged += (_, _) =>
        {
            if (DataContext is MainViewModel vm)
            {
                vm.Log.CollectionChanged += (_, c) =>
                {
                    if (c.Action == NotifyCollectionChangedAction.Add)
                    {
                        Dispatcher.UIThread.Post(() => LogScroll.ScrollToEnd());
                    }
                };
            }
        };
    }
}
