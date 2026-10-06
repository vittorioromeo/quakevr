using System.Collections.Specialized;
using System.Windows;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer.Views;

public partial class InstallPage
{
    public InstallPage()
    {
        InitializeComponent();
        // Keep the newest log line in view.
        DataContextChanged += (_, e) =>
        {
            if (e.NewValue is MainViewModel vm)
            {
                vm.Log.CollectionChanged += (_, c) =>
                {
                    if (c.Action == NotifyCollectionChangedAction.Add)
                    {
                        Dispatcher.BeginInvoke(() => LogScroll.ScrollToEnd());
                    }
                };
            }
        };
    }
}
