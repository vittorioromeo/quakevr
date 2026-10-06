using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Detection;

namespace QuakeVR.Installer.Skin;

/// <summary>
/// The pictures of the skin: wall and metal textures, the lava, the fire's colours, the status bar's digits. Either
/// read at run time from the player's own Quake (never shipped: id Software's data stays in their pak files), or
/// generated here (before Quake is found, or without it).
/// </summary>
sealed class SkinAssets
{
    // The textures the skin looks for, best first (id1's maps: start, e1m1, e1m2).
    public static readonly string[] BackdropNames = ["wbrick1_5", "city4_6", "bricka2_2", "rock4_1", "wall9_8"];
    public static readonly string[] PanelNames = ["wizmet1_2", "metal5_4", "met5_1", "metal5_1", "wmet4_4"];
    public static readonly string[] PlateNames = ["metal1_4", "metal5_3", "cop1_1", "metal5_6", "wizmet1_1"];
    public static readonly string[] TrimNames = ["wizmet1_3", "m5_3", "metal6_4", "cop3_4", "wmet3_1"];
    public static readonly string[] LavaNames = ["*lava1"];
    public static readonly string[] MapsToRead = ["maps/start.bsp", "maps/e1m1.bsp", "maps/e1m2.bsp"];

    public required bool FromQuake { get; init; }
    public required string Description { get; init; }
    /// <summary>The page background (stone, bricks), tiled.</summary>
    public required BitmapSource Backdrop { get; init; }
    /// <summary>The sidebar's metal.</summary>
    public required BitmapSource Panel { get; init; }
    /// <summary>Button faces and the footer.</summary>
    public required BitmapSource Plate { get; init; }
    /// <summary>Card edges and plaques.</summary>
    public required BitmapSource Trim { get; init; }
    /// <summary>The lava for the progress bar (square, BGRA), warped as the engine warps liquids.</summary>
    public required byte[] LavaBgra { get; init; }
    public required int LavaSize { get; init; }
    /// <summary>The flames' colours, premultiplied BGRA, from cold (transparent) to white-hot.</summary>
    public required uint[] FireRamp { get; init; }
    /// <summary>The status bar's big digits (Quake only).</summary>
    public IReadOnlyDictionary<char, BitmapSource> Digits { get; init; } = new Dictionary<char, BitmapSource>();
    /// <summary>The scale the textures are drawn at (Quake's 64-texel textures read best at 2x, crisp).</summary>
    public required double TextureScale { get; init; }

    // ---- From the player's Quake ----

    /// <summary>The skin from a detected Quake, or null when its files cannot be read.</summary>
    public static SkinAssets? FromQuakeInstall(QuakeInstall quake)
    {
        using var fs = QuakeFileSystem.Open(quake);
        return fs is null ? null : FromFileSystem(fs, quake.Name);
    }

    public static SkinAssets? FromFileSystem(QuakeFileSystem fs, string what)
    {
        var palette = fs.Read("gfx/palette.lmp");
        if (palette is not { Length: >= QuakeFormats.PaletteSize })
        {
            return null;
        }
        var textures = new Dictionary<string, IndexedImage>(StringComparer.OrdinalIgnoreCase);
        foreach (var map in MapsToRead)
        {
            if (fs.Read(map) is { } bsp)
            {
                foreach (var t in QuakeFormats.ReadBspTextures(bsp))
                {
                    textures.TryAdd(t.Name, t);
                }
            }
        }
        IndexedImage? Pick(string[] names) => names.Select(n => textures.GetValueOrDefault(n)).FirstOrDefault(t => t is not null);
        if (Pick(BackdropNames) is not { } backdrop || Pick(PanelNames) is not { } panel || Pick(PlateNames) is not { } plate ||
            Pick(TrimNames) is not { } trim)
        {
            return null;
        }
        var digits = new Dictionary<char, BitmapSource>();
        if (fs.Read("gfx.wad") is { } wad)
        {
            var pics = QuakeFormats.ReadWad(wad);
            for (var c = '0'; c <= '9'; ++c)
            {
                if (pics.GetValueOrDefault($"NUM_{c}") is { } pic)
                {
                    digits[c] = ToBitmap(pic, palette, transparent255: true);
                }
            }
        }
        var lava = Pick(LavaNames);
        var lavaBgra = lava is { Width: 64, Height: 64 } ? QuakeFormats.ToBgra(lava, palette) : Procedural.Lava(64);
        return new SkinAssets
        {
            FromQuake = true,
            Description = $"Quake textures from {what}",
            Backdrop = ToBitmap(backdrop, palette),
            Panel = ToBitmap(panel, palette),
            Plate = ToBitmap(plate, palette),
            Trim = ToBitmap(trim, palette),
            LavaBgra = lavaBgra,
            LavaSize = 64,
            FireRamp = Procedural.FireRamp(palette),
            Digits = digits,
            TextureScale = 2,
        };
    }

    /// <summary>Every texture of the maps the skin reads (the contact sheet the harness draws to choose them).</summary>
    public static List<(string Name, BitmapSource Image)> AllTextures(QuakeFileSystem fs)
    {
        var palette = fs.Read("gfx/palette.lmp") ?? new byte[QuakeFormats.PaletteSize];
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var list = new List<(string, BitmapSource)>();
        foreach (var map in MapsToRead)
        {
            foreach (var t in fs.Read(map) is { } bsp ? QuakeFormats.ReadBspTextures(bsp) : [])
            {
                if (seen.Add(t.Name) && !t.Name.StartsWith('+') && !t.Name.StartsWith("sky", StringComparison.OrdinalIgnoreCase))
                {
                    list.Add((t.Name, ToBitmap(t, palette)));
                }
            }
        }
        return list;
    }

    static BitmapSource ToBitmap(IndexedImage image, byte[] palette, bool transparent255 = false)
    {
        var bgra = QuakeFormats.ToBgra(image, palette, transparent255);
        var b = BitmapSource.Create(image.Width, image.Height, 96, 96, PixelFormats.Pbgra32, null, bgra, image.Width * 4);
        b.Freeze();
        return b;
    }

    // ---- Generated ----

    public static SkinAssets Generated() => new()
    {
        FromQuake = false,
        Description = "generated textures",
        Backdrop = Procedural.ToBitmap(Procedural.Bricks(128, 11), 128),
        Panel = Procedural.ToBitmap(Procedural.Metal(128, 23, rivets: true, rust: 0.55f), 128),
        Plate = Procedural.ToBitmap(Procedural.Metal(128, 37, rivets: false, rust: 0.7f), 128),
        Trim = Procedural.ToBitmap(Procedural.Metal(64, 41, rivets: false, rust: 0.6f), 64),
        LavaBgra = Procedural.Lava(64),
        LavaSize = 64,
        FireRamp = Procedural.FireRamp(null),
        TextureScale = 1,
    };
}

/// <summary>Publishes the skin's pictures as the brushes the theme uses (DynamicResource: a later skin replaces them).</summary>
static class SkinResources
{
    public static SkinAssets Current { get; private set; } = null!;

    public static event Action? Changed;

    public static void Apply(ResourceDictionary resources, SkinAssets skin)
    {
        Current = skin;
        var s = skin.TextureScale;
        resources["SkinBackdropBrush"] = Tile(skin.Backdrop, s);
        resources["SkinPanelBrush"] = Tile(skin.Panel, s);
        resources["SkinPlateBrush"] = Tile(skin.Plate, s);
        resources["SkinTrimBrush"] = Tile(skin.Trim, s);
        resources["SkinIsQuake"] = skin.FromQuake;
        Changed?.Invoke();
    }

    static ImageBrush Tile(BitmapSource image, double scale)
    {
        var brush = new ImageBrush(image)
        {
            TileMode = TileMode.Tile,
            Stretch = Stretch.Fill,
            ViewportUnits = BrushMappingMode.Absolute,
            Viewport = new Rect(0, 0, image.PixelWidth * scale, image.PixelHeight * scale),
        };
        RenderOptions.SetBitmapScalingMode(brush, scale > 1 ? BitmapScalingMode.NearestNeighbor : BitmapScalingMode.Linear);
        RenderOptions.SetCachingHint(brush, CachingHint.Cache);
        brush.Freeze();
        return brush;
    }
}
