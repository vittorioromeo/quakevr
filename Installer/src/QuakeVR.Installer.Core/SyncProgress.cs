namespace QuakeVR.Installer.Core;

/// <summary>An <see cref="IProgress{T}"/> that runs its handler on the reporting thread, in order (consoles and
/// tests; the window uses <see cref="Progress{T}"/>, which posts to the UI thread).</summary>
public sealed class SyncProgress<T>(Action<T> handler) : IProgress<T>
{
    public void Report(T value) => handler(value);
}
