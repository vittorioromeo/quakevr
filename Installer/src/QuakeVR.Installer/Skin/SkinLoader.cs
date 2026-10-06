using System.Windows;
using QuakeVR.Installer.Audio;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Detection;

namespace QuakeVR.Installer.Skin;

/// <summary>
/// Puts the skin in place: the generated one at start, then the player's own Quake's textures and sounds as soon as a
/// Quake is known (the silent detection at start, or a folder picked by hand). Read on a worker thread, applied on the
/// UI thread; a Quake that cannot be read keeps the generated skin.
/// </summary>
static class SkinLoader
{
    static bool _fromQuake;
    static Task<bool>? _loading;

    public static bool Disabled { get; set; }

    public static void ApplyGenerated() => SkinResources.Apply(Application.Current.Resources, SkinAssets.Generated());

    /// <summary>Loads the skin from <paramref name="quake"/> unless one already came from a Quake.</summary>
    public static Task<bool> LoadFromQuakeAsync(QuakeInstall quake, bool sounds)
    {
        if (Disabled || _fromQuake || !quake.Playable)
        {
            return Task.FromResult(_fromQuake);
        }
        if (_loading is { IsCompleted: false })
        {
            return _loading;
        }
        return _loading = LoadAsync(quake, sounds);
    }

    static async Task<bool> LoadAsync(QuakeInstall quake, bool sounds)
    {
        var skin = await Task.Run(() =>
        {
            try
            {
                using var fs = QuakeFileSystem.Open(quake);
                if (fs is null)
                {
                    return null;
                }
                if (sounds)
                {
                    UiSounds.UseQuake(fs);
                }
                return SkinAssets.FromFileSystem(fs, quake.Name);
            }
            catch (Exception e) when (e is System.IO.IOException or UnauthorizedAccessException or InvalidOperationException or ArgumentException)
            {
                return null;
            }
        });
        if (skin is null)
        {
            return false;
        }
        SkinResources.Apply(Application.Current.Resources, skin);
        _fromQuake = true;
        return true;
    }
}
