using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using Microsoft.ML.OnnxRuntime;
using Microsoft.ML.OnnxRuntime.Tensors;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Packaging;

namespace QuakeVR.Installer.Core.Reliefs;

/// <summary>What one run of the reliefs step did.</summary>
public sealed record ReliefSummary(int Textures, int Made, int Kept, int NotNeeded, int Failed, string Device, TimeSpan Time,
    bool Cancelled, IReadOnlyList<string> Errors)
{
    public string Describe() =>
        $"{Made} reliefs made, {Kept} kept from before, {NotNeeded} not needed (Quake VR's own or stock textures), {Failed} failed " +
        $"({Textures} textures, {Device}, {Time.TotalSeconds:0.0} s){(Cancelled ? ": cancelled, the rest are made by the next run" : "")}";
}

/// <summary>
/// "Make reliefs for Dwell and Quoth" (DWELL.md 9.9, option A): the relief companions of AD's bundle
/// (<c>Misc/quakevr/pbr_generate.py</c>, docs/vr-port/PBR_PIPELINE.md) made on the player's machine from his own copies of
/// mods whose textures may not be shipped. PBRify's two CC0 models (embedded ONNX, <c>pbr_export_onnx.py</c>) run by
/// onnxruntime, DirectML first, the CPU when DirectML is not there or fails. The output is the bundle's format exactly:
/// <c>&lt;install&gt;\reliefs\textures\pbr\&lt;hash&gt;_norm.png</c> named by the drawn texture's pixel hash, and its
/// <c>provenance.txt</c>; the engine searches that folder below every game folder (vr_pbr.cpp, VR_AddReliefsPath).
/// What the generator does that this does not: comparison packs' authored maps (none for these mods) and JPG
/// replacements (not decoded: no relief for those). Only the missing companions are made again.
/// </summary>
public static class ReliefMaker
{
    public const string FolderName = "reliefs";
    public const string Title = "Make reliefs for Dwell and Quoth";
    public const int Tile = 256;
    public const int Padding = 48;
    public const double Depth = 0.5;
    static readonly byte[] Tag = "qvr-pbr-v1\0"u8.ToArray();
    static readonly HashSet<string> Excluded = ["clip", "skip", "hint", "trigger", "origin", "null", "nodraw", "black"];
    const string ProvenanceHeader = "qvr-pbr-provenance-v1";

    public static string Folder(string installDir) => Path.Combine(installDir, FolderName);
    public static string TextureDir(string installDir) => Path.Combine(Folder(installDir), "textures", "pbr");

    /// <summary>The uninstaller's: the whole folder (only Setup writes there). Files removed.</summary>
    public static int Remove(string installDir)
    {
        var dir = Folder(installDir);
        if (!Directory.Exists(dir))
        {
            return 0;
        }
        var n = Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories).Count();
        try
        {
            Directory.Delete(dir, recursive: true);
            return n;
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            return 0;
        }
    }

    public static string ContentHash(int w, int h, ReadOnlySpan<byte> rgba)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        hash.AppendData(Tag);
        Span<byte> dims = stackalloc byte[8];
        BitConverter.TryWriteBytes(dims, (uint)w);
        BitConverter.TryWriteBytes(dims[4..], (uint)h);
        hash.AppendData(dims);
        hash.AppendData(rgba);
        return Convert.ToHexStringLower(hash.GetHashAndReset());
    }

    public static bool Eligible(string name) => !Excluded.Contains(name) && !name.StartsWith('*') && !name.StartsWith("sky", StringComparison.Ordinal);

    /// <summary>gfx/palette.lmp from the first of these game folders that has one (pak or loose).</summary>
    public static byte[]? FindPalette(IEnumerable<string> gameDirs)
    {
        foreach (var dir in gameDirs.Where(Directory.Exists))
        {
            using var files = GameFiles.Open(dir);
            if (files.Read("gfx/palette.lmp") is { Length: QuakeFormats.PaletteSize } palette)
            {
                return palette;
            }
        }
        return null;
    }

    /// <summary>
    /// The step. <paramref name="modDirs"/>: the Dwell and Quoth folders the game plays; <paramref name="textureRoots"/>:
    /// the folders whose textures/ replacements the engine draws over them, highest first (the install's quakevr);
    /// <paramref name="installDir"/>: where quakevr's bundle and stock list are and the reliefs folder goes.
    /// Never throws for a texture: its failure is counted and the rest go on.
    /// </summary>
    public static ReliefSummary Run(string installDir, IReadOnlyList<string> modDirs, IReadOnlyList<string> textureRoots, byte[] palette,
        bool forceCpu, IProgress<InstallProgress>? progress, CancellationToken ct, int? limit = null, string? quakevrDir = null)
    {
        var clock = Stopwatch.StartNew();
        void Report(double f, string status, string? log = null, LogLevel level = LogLevel.Info) =>
            progress?.Report(new InstallProgress(f, status, log, level));
        var errors = new List<string>();
        Report(0, "Reliefs: reading the maps' textures");
        var sources = Scan(modDirs, textureRoots, palette, errors);
        var anchors = Anchors(sources);
        var bundle = Path.Combine(quakevrDir ?? Path.Combine(installDir, "quakevr"), "textures", "pbr");
        var stock = ReadStock(Path.Combine(bundle, "stock.txt"));
        var outDir = TextureDir(installDir);
        var work = new List<string>();
        int kept = 0, notNeeded = 0;
        foreach (var (digest, s) in sources.OrderBy(p => p.Key, StringComparer.Ordinal))
        {
            var prefix = digest[..32];
            if (!s.Drawn || stock.Contains(prefix) || File.Exists(Path.Combine(bundle, prefix + "_norm.png")))
            {
                ++notNeeded;
            }
            else if (File.Exists(Path.Combine(outDir, prefix + "_norm.png")))
            {
                ++kept;
            }
            else
            {
                work.Add(digest);
            }
        }
        if (limit is { } l)
        {
            work = [.. work.Take(l)];
        }
        var provenance = ReadProvenance(Path.Combine(outDir, "provenance.txt"), outDir);
        Report(0.02, "Reliefs: starting", $"Reliefs: {sources.Count} textures in {string.Join(", ", modDirs.Select(Path.GetFileName))}: " +
                                          $"{work.Count} to make, {kept} already made, {notNeeded} not needed.");
        if (work.Count == 0)
        {
            WriteProvenance(outDir, provenance);
            return new ReliefSummary(sources.Count, 0, kept, notNeeded, 0, "none needed", clock.Elapsed, false, errors);
        }
        Directory.CreateDirectory(outDir);
        using var models = new Models(forceCpu);
        Report(0.03, "Reliefs: models loaded", $"Reliefs: PBRify's models on {models.Device}.");
        int made = 0, failed = 0;
        var cancelled = false;
        try
        {
            for (var i = 0; i < work.Count; ++i)
            {
                if (ct.IsCancellationRequested)
                {
                    cancelled = true;
                    break;
                }
                var digest = work[i];
                var s = sources[digest];
                try
                {
                    var anchor = sources[anchors.GetValueOrDefault(digest, digest)];
                    var (image, outputDigest) = Make(models, s, anchor);
                    var path = Path.Combine(outDir, digest[..32] + "_norm.png");
                    File.WriteAllBytes(path + ".qvrnew", ReliefImages.EncodePng(image));
                    File.Move(path + ".qvrnew", path, overwrite: true);
                    provenance[digest[..32]] = $"{digest[..32]} {outputDigest} 3";
                    ++made;
                }
                catch (Exception e) when (e is OnnxRuntimeException or InvalidDataException or IOException or UnauthorizedAccessException)
                {
                    ++failed;
                    errors.Add($"{string.Join(",", s.Names.Order())}: {e.Message}");
                }
                if ((i + 1) % 50 == 0)
                {
                    WriteProvenance(outDir, provenance);
                }
                Report(0.03 + 0.97 * (i + 1) / work.Count, $"Reliefs: {i + 1} of {work.Count} ({models.Device}; Cancel skips the rest)");
            }
        }
        finally
        {
            WriteProvenance(outDir, provenance);
        }
        return new ReliefSummary(sources.Count, made, kept, notNeeded, failed, models.Device, clock.Elapsed, cancelled, errors);
    }

    // ---- Discovery: pbr_generate.py's scan, for one machine's own folders ------------------------------------------

    internal sealed class Source(int w, int h, byte[] rgba)
    {
        public int Width { get; } = w;
        public int Height { get; } = h;
        public byte[] Rgba { get; } = rgba;
        public bool[] Holes { get; } = new bool[w * h];
        public bool[] Emission { get; } = new bool[w * h];
        public HashSet<string> Names { get; } = [];
        public HashSet<string> Maps { get; } = [];
        public bool Override { get; set; }
        public bool Drawn { get; set; }
    }

    /// <summary>A game folder's files as the engine mounts them: loose files below pak0, pak1, ... (contiguous).</summary>
    internal sealed class GameFiles : IDisposable
    {
        readonly Dictionary<string, Func<byte[]?>> _files = new(StringComparer.OrdinalIgnoreCase);
        readonly List<PakArchive> _paks = [];

        public IEnumerable<string> Names => _files.Keys;

        public static GameFiles Open(string dir)
        {
            var g = new GameFiles();
            foreach (var p in Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories))
            {
                if (!p.EndsWith(".pak", StringComparison.OrdinalIgnoreCase))
                {
                    g._files[Path.GetRelativePath(dir, p).Replace('\\', '/').ToLowerInvariant()] = () => File.ReadAllBytes(p);
                }
            }
            for (var i = 0; ; ++i)
            {
                var path = Path.Combine(dir, $"pak{i}.pak");
                if (!File.Exists(path) || PakArchive.TryOpen(path) is not { } pak)
                {
                    break;
                }
                g._paks.Add(pak);
                foreach (var name in pak.Names)
                {
                    var n = name.Replace('\\', '/').ToLowerInvariant();
                    g._files[n] = () => pak.Read(name);
                }
            }
            return g;
        }

        public bool Contains(string name) => _files.ContainsKey(name);
        public byte[]? Read(string name) => _files.TryGetValue(name, out var read) ? read() : null;

        public void Dispose()
        {
            foreach (var p in _paks)
            {
                p.Dispose();
            }
        }
    }

    internal static Dictionary<string, Source> Scan(IReadOnlyList<string> modDirs, IReadOnlyList<string> textureRoots, byte[] palette, List<string> errors)
    {
        var sources = new Dictionary<string, Source>(StringComparer.Ordinal);
        var opened = new Dictionary<string, GameFiles>(StringComparer.OrdinalIgnoreCase);
        GameFiles Index(string dir) => opened.TryGetValue(dir, out var g) ? g : opened[dir] = GameFiles.Open(dir);
        try
        {
            foreach (var mod in modDirs.Where(Directory.Exists))
            {
                var files = Index(mod);
                var roots = textureRoots.Where(Directory.Exists).Select(Index).ToList();
                if (!textureRoots.Any(r => string.Equals(Path.GetFullPath(r), Path.GetFullPath(mod), StringComparison.OrdinalIgnoreCase)))
                {
                    roots.Add(files);
                }
                foreach (var mapname in files.Names.Where(n => n.EndsWith(".bsp", StringComparison.Ordinal)).Order(StringComparer.Ordinal).ToList())
                {
                    try
                    {
                        var bsp = files.Read(mapname) ?? throw new InvalidDataException("unreadable");
                        foreach (var tex in QuakeFormats.ReadBspTextures(bsp))
                        {
                            var name = tex.Name.ToLowerInvariant();
                            if (!Eligible(name))
                            {
                                continue;
                            }
                            var (w, h) = (tex.Width, tex.Height);
                            var rgba = new byte[w * h * 4];
                            for (var i = 0; i < w * h; ++i)
                            {
                                var index = tex.Pixels[i];
                                rgba[i * 4] = palette[index * 3];
                                rgba[i * 4 + 1] = palette[index * 3 + 1];
                                rgba[i * 4 + 2] = palette[index * 3 + 2];
                                rgba[i * 4 + 3] = 255;
                            }
                            var (found, replacement) = SelectedOverride(mapname, name, roots);
                            // On the player's machine every root is mounted wherever the mod is: a replacement there is
                            // what the engine draws (and hashes), never the miptex.
                            var s = Add(sources, w, h, rgba, name, mapname, drawn: !found);
                            var fence = name.StartsWith('{') || name.StartsWith('#');
                            for (var i = 0; i < w * h; ++i)
                            {
                                var index = tex.Pixels[i];
                                s.Holes[i] |= fence && index == 255;
                                s.Emission[i] |= index >= 224 && index != 255;
                            }
                            if (replacement is not null)
                            {
                                var o = Add(sources, replacement.Width, replacement.Height, replacement.Pixels, name, mapname, drawn: true);
                                o.Override = true;
                                for (var i = 0; i < o.Holes.Length; ++i)
                                {
                                    o.Holes[i] |= replacement.Pixels[i * 4 + 3] < 128;
                                }
                            }
                        }
                    }
                    catch (Exception e) when (e is InvalidDataException or IOException or ArgumentException)
                    {
                        errors.Add($"{mapname}: {e.Message}");
                    }
                }
            }
        }
        finally
        {
            foreach (var g in opened.Values)
            {
                g.Dispose();
            }
        }
        return sources;
    }

    static Source Add(Dictionary<string, Source> sources, int w, int h, byte[] rgba, string name, string mapname, bool drawn)
    {
        var digest = ContentHash(w, h, rgba);
        if (!sources.TryGetValue(digest, out var s))
        {
            sources[digest] = s = new Source(w, h, rgba);
        }
        s.Drawn |= drawn;
        s.Names.Add(name);
        s.Maps.Add(mapname);
        return s;
    }

    /// <summary>Image_LoadImage's choice: textures/&lt;map&gt;/&lt;name&gt; before textures/&lt;name&gt;, PNG before TGA before JPG
    /// across all roots in order. Found and undecodable (a JPG): found, no image.</summary>
    static (bool Found, RgbaImage? Image) SelectedOverride(string mapname, string name, List<GameFiles> roots)
    {
        var mapbase = mapname.StartsWith("maps/", StringComparison.Ordinal) ? mapname[5..] : mapname;
        mapbase = mapbase[..^4];
        foreach (var b in new[] { $"textures/{mapbase}/{name}", $"textures/{name}" })
        {
            foreach (var ext in new[] { ".png", ".tga", ".jpg" })
            {
                foreach (var root in roots)
                {
                    if (root.Contains(b + ext))
                    {
                        return (true, root.Read(b + ext) is { } data ? ReliefImages.Decode(ext, data) : null);
                    }
                }
            }
        }
        return (false, null);
    }

    // ---- Animation families: a lighting-only frame shares its first frame's relief (pbr_generate.py animations) -----

    internal static double[] DetailSignature(Source s)
    {
        var cells = new double[64 * 64];
        for (var y = 0; y < 256; ++y)
        {
            var sy = y * s.Height / 256;
            for (var x = 0; x < 256; ++x)
            {
                var sx = x * s.Width / 256;
                var o = (sy * s.Width + sx) * 4;
                cells[(y / 4) * 64 + x / 4] += (s.Rgba[o] * .299 + s.Rgba[o + 1] * .587 + s.Rgba[o + 2] * .114) / 16;
            }
        }
        var result = new double[64 * 64];
        for (var y = 0; y < 64; ++y)
        {
            for (var x = 0; x < 64; ++x)
            {
                double blur = 0;
                for (var dy = -2; dy <= 2; ++dy)
                {
                    for (var dx = -2; dx <= 2; ++dx)
                    {
                        blur += cells[((y + dy + 64) % 64) * 64 + (x + dx + 64) % 64];
                    }
                }
                result[y * 64 + x] = cells[y * 64 + x] - blur / 25;
            }
        }
        var mean = result.Average();
        for (var i = 0; i < result.Length; ++i)
        {
            result[i] -= mean;
        }
        return result;
    }

    static double Dot(double[] a, double[] b, int shiftY = 0, int shiftX = 0)
    {
        double sum = 0;
        for (var y = 0; y < 64; ++y)
        {
            for (var x = 0; x < 64; ++x)
            {
                sum += a[y * 64 + x] * b[((y - shiftY + 64) % 64) * 64 + (x - shiftX + 64) % 64];
            }
        }
        return sum;
    }

    internal static Dictionary<string, string> Anchors(Dictionary<string, Source> sources)
    {
        var families = new Dictionary<(string, string, bool, int, int), List<(char Frame, string Digest)>>();
        foreach (var (digest, s) in sources)
        {
            foreach (var name in s.Names.Where(IsFrame))
            {
                foreach (var map in s.Maps)
                {
                    var key = (map, name[2..], s.Override, s.Width, s.Height);
                    if (!families.TryGetValue(key, out var list))
                    {
                        families[key] = list = [];
                    }
                    list.Add((name[1], digest));
                }
            }
        }
        var anchors = new Dictionary<string, string>(StringComparer.Ordinal);
        var conflicts = new HashSet<string>(StringComparer.Ordinal);
        foreach (var frames in families.Values)
        {
            var anchor = frames.OrderBy(f => f.Frame).ThenBy(f => f.Digest, StringComparer.Ordinal).First().Digest;
            var sa = DetailSignature(sources[anchor]);
            foreach (var (_, digest) in frames)
            {
                if (digest != anchor)
                {
                    var sb = DetailSignature(sources[digest]);
                    var norm = Math.Sqrt(Dot(sa, sa) * Dot(sb, sb));
                    var same = norm > 1e-6 ? Dot(sa, sb) / norm : 1.0;
                    var moved = same;
                    if (same is >= .5 and < .999)
                    {
                        for (var shift = 1; shift < 64; ++shift)
                        {
                            moved = Math.Max(moved, Math.Max(Dot(sa, sb, shift, 0) / norm, Dot(sa, sb, 0, shift) / norm));
                            if (moved > same + .01)
                            {
                                break;
                            }
                        }
                    }
                    if (same < .5 || moved > same + .01)
                    {
                        anchors[digest] = digest;
                        continue;
                    }
                }
                if (anchors.TryGetValue(digest, out var existing) && existing != anchor)
                {
                    conflicts.Add(digest);
                }
                else
                {
                    anchors[digest] = anchor;
                }
            }
        }
        foreach (var (digest, s) in sources)
        {
            if (anchors.ContainsKey(digest) && s.Names.Any(n => !IsFrame(n)))
            {
                anchors[digest] = digest;
            }
        }
        foreach (var digest in conflicts)
        {
            anchors[digest] = digest;
        }
        return anchors;
    }

    static bool IsFrame(string name) => name.Length > 2 && name[0] == '+' && (char.IsAsciiDigit(name[1]) || name[1] is >= 'a' and <= 'j');

    // ---- Inference and packing: pbr_generate.py generate, for a texture without authored maps ----------------------

    internal sealed class Models : IDisposable
    {
        InferenceSession _normal, _height;
        public string Device { get; private set; }

        public Models(bool forceCpu)
        {
            if (!forceCpu)
            {
                try
                {
                    (_normal, _height) = (Load("pbrify_normal", true), Load("pbrify_height", true));
                    Device = "DirectML";
                    return;
                }
                catch (Exception e) when (e is OnnxRuntimeException or DllNotFoundException or EntryPointNotFoundException or TypeInitializationException)
                {
                }
            }
            (_normal, _height) = (Load("pbrify_normal", false), Load("pbrify_height", false));
            Device = "CPU";
        }

        static InferenceSession Load(string name, bool directMl)
        {
            using var stream = typeof(ReliefMaker).Assembly.GetManifestResourceStream($"QuakeVR.Reliefs.{name}.onnx")
                               ?? throw new InvalidDataException($"{name}.onnx is not in Setup");
            var bytes = new byte[stream.Length];
            stream.ReadExactly(bytes);
            using var options = new SessionOptions();
            if (directMl)
            {
                options.EnableMemoryPattern = false;
                options.ExecutionMode = ExecutionMode.ORT_SEQUENTIAL;
                options.AppendExecutionProvider_DML(0);
            }
            return new InferenceSession(bytes, options);
        }

        /// <summary>One model over the periodic texture, tile by tile with true wrapped context; centres only survive.
        /// HWC, clipped to [0, 1]. A DirectML failure falls back to the CPU once, then retries.</summary>
        public float[] Predict(bool normal, byte[] rgb, int w, int h)
        {
            try
            {
                return Tiles(normal ? _normal : _height, rgb, w, h);
            }
            catch (OnnxRuntimeException) when (Device == "DirectML")
            {
                _normal.Dispose();
                _height.Dispose();
                (_normal, _height) = (Load("pbrify_normal", false), Load("pbrify_height", false));
                Device = "CPU (DirectML failed)";
                return Tiles(normal ? _normal : _height, rgb, w, h);
            }
        }

        static float[] Tiles(InferenceSession session, byte[] rgb, int w, int h)
        {
            var result = new float[w * h * 3];
            var input = session.InputMetadata.Keys.First();
            for (var y = 0; y < h; y += Tile)
            {
                for (var x = 0; x < w; x += Tile)
                {
                    int ch = Math.Min(Tile, h - y), cw = Math.Min(Tile, w - x);
                    int ph = ch + 2 * Padding, pw = cw + 2 * Padding;
                    var tensor = new DenseTensor<float>([1, 3, ph, pw]);
                    var buffer = tensor.Buffer.Span;
                    for (var py = 0; py < ph; ++py)
                    {
                        var sy = (((y - Padding + py) % h) + h) % h;
                        for (var px = 0; px < pw; ++px)
                        {
                            var sx = (((x - Padding + px) % w) + w) % w;
                            var o = (sy * w + sx) * 3;
                            for (var c = 0; c < 3; ++c)
                            {
                                buffer[(c * ph + py) * pw + px] = rgb[o + c] / 255f;
                            }
                        }
                    }
                    using var outputs = session.Run([NamedOnnxValue.CreateFromTensor(input, tensor)]);
                    var pred = outputs.First().AsTensor<float>();
                    if (pred.Dimensions.Length != 4 || pred.Dimensions[2] != ph || pred.Dimensions[3] != pw || pred.Dimensions[1] != 3)
                    {
                        throw new InvalidDataException("the model must give 1x3xHxW at 1x scale");
                    }
                    for (var cy = 0; cy < ch; ++cy)
                    {
                        for (var cx = 0; cx < cw; ++cx)
                        {
                            for (var c = 0; c < 3; ++c)
                            {
                                var v = pred[0, c, cy + Padding, cx + Padding];
                                if (!float.IsFinite(v))
                                {
                                    throw new InvalidDataException("nonfinite model output");
                                }
                                result[((y + cy) * w + x + cx) * 3 + c] = Math.Clamp(v, 0f, 1f);
                            }
                        }
                    }
                }
            }
            return result;
        }

        public void Dispose()
        {
            _normal.Dispose();
            _height.Dispose();
        }
    }

    internal static (RgbaImage Image, string Digest) Make(Models models, Source s, Source anchor)
    {
        int w = anchor.Width, h = anchor.Height, n = w * h;
        var masked = new bool[n];
        for (var i = 0; i < n; ++i)
        {
            masked[i] = anchor.Holes[i] || anchor.Emission[i];
        }
        var rgb = FillMasked(anchor.Rgba, masked, w, h);
        var normalPred = models.Predict(true, rgb, w, h);
        var heightRgb = models.Predict(false, rgb, w, h);
        var prediction = new float[n];
        for (var i = 0; i < n; ++i)
        {
            prediction[i] = (heightRgb[i * 3] + heightRgb[i * 3 + 1] + heightRgb[i * 3 + 2]) / 3f;
        }
        var normal = NormalBytes(normalPred, n);
        var height = CalibratedHeight(prediction, anchor.Holes, Depth);
        var orientation = new byte[n];
        for (var i = 0; i < n; ++i)
        {
            orientation[i] = (byte)Math.Round(255 * (.75 + .25 * prediction[i]), MidpointRounding.ToEven);
        }
        if (GreenCorrelation(normal, orientation, w, h) < -.2)
        {
            for (var i = 0; i < n; ++i)
            {
                normal[i * 3 + 1] = (byte)(255 - normal[i * 3 + 1]);
            }
        }
        var output = new byte[n * 4];
        for (var i = 0; i < n; ++i)
        {
            var hole = s.Width == w && s.Height == h && s.Holes[i];
            output[i * 4] = hole ? (byte)128 : normal[i * 3];
            output[i * 4 + 1] = hole ? (byte)128 : normal[i * 3 + 1];
            output[i * 4 + 2] = hole ? (byte)255 : normal[i * 3 + 2];
            output[i * 4 + 3] = hole ? (byte)255 : height[i];
        }
        return (new RgbaImage(w, h, output), ContentHash(w, h, output));
    }

    /// <summary>Periodic nearest-neighbour expansion of the valid colours over cutouts and glow (pbr_generate.py fill_masked).</summary>
    internal static byte[] FillMasked(byte[] rgba, bool[] mask, int w, int h)
    {
        var n = w * h;
        var result = new byte[n * 3];
        var valid = new bool[n];
        for (var i = 0; i < n; ++i)
        {
            result[i * 3] = rgba[i * 4];
            result[i * 3 + 1] = rgba[i * 4 + 1];
            result[i * 3 + 2] = rgba[i * 4 + 2];
            valid[i] = !mask[i];
        }
        if (!valid.Any(v => v))
        {
            Array.Fill(result, (byte)128);
            return result;
        }
        for (var iteration = 0; iteration < Math.Max(w, h) * 2 && !valid.All(v => v); ++iteration)
        {
            var oldValid = (bool[])valid.Clone();
            var oldResult = (byte[])result.Clone();
            foreach (var (dy, dx) in new[] { (1, 0), (-1, 0), (0, 1), (0, -1) })
            {
                for (var y = 0; y < h; ++y)
                {
                    for (var x = 0; x < w; ++x)
                    {
                        var i = y * w + x;
                        var j = ((y - dy + h) % h) * w + (x - dx + w) % w; // np.roll(a, shift)[i] == a[i - shift]
                        if (!valid[i] && oldValid[j])
                        {
                            result[i * 3] = oldResult[j * 3];
                            result[i * 3 + 1] = oldResult[j * 3 + 1];
                            result[i * 3 + 2] = oldResult[j * 3 + 2];
                            valid[i] = true;
                        }
                    }
                }
            }
        }
        return result;
    }

    internal static byte[] NormalBytes(float[] pred, int n)
    {
        var output = new byte[n * 3];
        for (var i = 0; i < n; ++i)
        {
            float x = pred[i * 3] * 2 - 1, y = pred[i * 3 + 1] * 2 - 1, z = Math.Max(pred[i * 3 + 2] * 2 - 1, .05f);
            var length = Math.Max(MathF.Sqrt(x * x + y * y + z * z), 1e-6f);
            output[i * 3] = (byte)Math.Round((x / length + 1) * 127.5f, MidpointRounding.ToEven);
            output[i * 3 + 1] = (byte)Math.Round((y / length + 1) * 127.5f, MidpointRounding.ToEven);
            output[i * 3 + 2] = (byte)Math.Round((z / length + 1) * 127.5f, MidpointRounding.ToEven);
        }
        return output;
    }

    /// <summary>Bounded relative relief: the learned 5th..95th percentile span fills the depth bound (gain at most 8,
    /// spans below 0.02 flat), holes excluded from the percentiles.</summary>
    internal static byte[] CalibratedHeight(float[] prediction, bool[] holes, double depth)
    {
        var n = prediction.Length;
        var output = new byte[n];
        var valid = prediction.Where((_, i) => !holes[i]).Select(v => (double)v).Order().ToArray();
        double Percentile(double q)
        {
            var pos = q / 100 * (valid.Length - 1);
            var lo = (int)Math.Floor(pos);
            var hi = Math.Min(lo + 1, valid.Length - 1);
            return valid[lo] + (valid[hi] - valid[lo]) * (pos - lo);
        }
        if (valid.Length < 16)
        {
            Array.Fill(output, (byte)255);
            return output;
        }
        double low = Percentile(5), high = Percentile(95), span = high - low;
        if (span < .02)
        {
            Array.Fill(output, (byte)255);
            return output;
        }
        var gain = Math.Min(1 / span, 8);
        for (var i = 0; i < n; ++i)
        {
            var recession = Math.Clamp((high - prediction[i]) * gain, 0, 1);
            output[i] = (byte)Math.Round(255 * (1 - depth * recession), MidpointRounding.ToEven);
        }
        return output;
    }

    internal static double GreenCorrelation(byte[] normal, byte[] height, int w, int h)
    {
        double gg = 0, nn = 0, gn = 0;
        for (var y = 0; y < h; ++y)
        {
            for (var x = 0; x < w; ++x)
            {
                double gradient = height[((y + 1) % h) * w + x] - height[((y - 1 + h) % h) * w + x];
                var green = normal[(y * w + x) * 3 + 1] - 127.5;
                gg += gradient * gradient;
                nn += green * green;
                gn += gradient * green;
            }
        }
        var d = Math.Sqrt(gg * nn);
        return d > 1e-6 ? gn / d : 0;
    }

    // ---- Indexes ---------------------------------------------------------------------------------------------------

    static HashSet<string> ReadStock(string path)
    {
        if (!File.Exists(path))
        {
            return [];
        }
        var lines = File.ReadAllLines(path);
        return lines.Length > 0 && lines[0].Trim() == "qvr-pbr-stock-v1"
            ? [.. lines.Skip(1).Select(l => l.Trim()).Where(l => l.Length == 32)]
            : [];
    }

    /// <summary>The existing index's records whose companion is still there (prefix to line).</summary>
    static SortedDictionary<string, string> ReadProvenance(string path, string outDir)
    {
        var result = new SortedDictionary<string, string>(StringComparer.Ordinal);
        if (!File.Exists(path))
        {
            return result;
        }
        var lines = File.ReadAllLines(path);
        if (lines.Length == 0 || lines[0].Trim() != ProvenanceHeader)
        {
            return result;
        }
        foreach (var line in lines.Skip(1).Select(l => l.Trim()))
        {
            var parts = line.Split(' ');
            if (parts.Length == 3 && parts[0].Length == 32 && File.Exists(Path.Combine(outDir, parts[0] + "_norm.png")))
            {
                result[parts[0]] = line;
            }
        }
        return result;
    }

    static void WriteProvenance(string outDir, SortedDictionary<string, string> lines)
    {
        if (lines.Count == 0 && !Directory.Exists(outDir))
        {
            return;
        }
        Directory.CreateDirectory(outDir);
        var text = new StringBuilder(ProvenanceHeader).Append('\n');
        foreach (var line in lines.Values)
        {
            text.Append(line).Append('\n');
        }
        var path = Path.Combine(outDir, "provenance.txt");
        File.WriteAllText(path + ".qvrnew", text.ToString(), Encoding.ASCII);
        File.Move(path + ".qvrnew", path, overwrite: true);
    }
}
