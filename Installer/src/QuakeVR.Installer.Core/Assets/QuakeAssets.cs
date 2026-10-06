using System.Buffers.Binary;
using System.Text;
using QuakeVR.Installer.Core.Detection;

namespace QuakeVR.Installer.Core.Assets;

/// <summary>An 8-bit image in Quake's palette (a lump, a WAD picture or a map's texture).</summary>
public sealed record IndexedImage(string Name, int Width, int Height, byte[] Pixels);

/// <summary>Sound samples, mono, as floats in [-1, 1].</summary>
public sealed record WaveSound(int SampleRate, float[] Samples);

/// <summary>
/// One opened .pak: its directory and reads by name. The installer's skin reads the player's own Quake through this at
/// run time (textures, palette, sounds): none of id Software's data is shipped with Quake VR or its installer.
/// </summary>
public sealed class PakArchive : IDisposable
{
    readonly FileStream _stream;
    readonly Dictionary<string, PakEntry> _entries;

    PakArchive(FileStream stream, Dictionary<string, PakEntry> entries)
    {
        _stream = stream;
        _entries = entries;
    }

    public IEnumerable<string> Names => _entries.Keys;

    public static PakArchive? TryOpen(string path)
    {
        FileStream? fs = null;
        try
        {
            fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            if (PakFile.TryReadDirectory(fs, maxEntries: 1 << 16) is not { } list)
            {
                fs.Dispose();
                return null;
            }
            var entries = new Dictionary<string, PakEntry>(StringComparer.OrdinalIgnoreCase);
            foreach (var e in list)
            {
                entries.TryAdd(e.Name, e);
            }
            return new PakArchive(fs, entries);
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            fs?.Dispose();
            return null;
        }
    }

    public bool Contains(string name) => _entries.ContainsKey(name);

    public byte[]? Read(string name)
    {
        if (!_entries.TryGetValue(name, out var e))
        {
            return null;
        }
        lock (_stream)
        {
            var data = new byte[e.Length];
            _stream.Position = e.Offset;
            return _stream.ReadAtLeast(data, e.Length, throwOnEndOfStream: false) == e.Length ? data : null;
        }
    }

    public void Dispose() => _stream.Dispose();
}

/// <summary>Several paks searched like the engine does: the last one opened wins (pak1 over pak0).</summary>
public sealed class QuakeFileSystem : IDisposable
{
    readonly List<PakArchive> _paks;

    public QuakeFileSystem(IEnumerable<PakArchive> paks) => _paks = [.. paks];

    public int PakCount => _paks.Count;

    public IEnumerable<string> Names => _paks.SelectMany(p => p.Names).Distinct(StringComparer.OrdinalIgnoreCase);

    public bool Contains(string name) => _paks.Any(p => p.Contains(name));

    public byte[]? Read(string name)
    {
        for (var i = _paks.Count - 1; i >= 0; --i)
        {
            if (_paks[i].Read(name) is { } data)
            {
                return data;
            }
        }
        return null;
    }

    /// <summary>The paks of a detected Quake: the original's id1 (pak0, pak1) when it has one, else the rerelease's.</summary>
    public static QuakeFileSystem? Open(QuakeInstall quake)
    {
        var dirs = new List<string>();
        if (quake.Original.Playable)
        {
            dirs.Add(Path.Combine(quake.InstallDir, "id1"));
        }
        if (quake.Rerelease.Playable && quake.RereleaseRoot is { } rr)
        {
            dirs.Add(Path.Combine(rr, "id1"));
        }
        foreach (var dir in dirs)
        {
            if (OpenDir(dir) is { } fs)
            {
                return fs;
            }
        }
        return null;
    }

    /// <summary>pak0.pak, pak1.pak... in a game folder (stops at the first missing one, as the engine does).</summary>
    public static QuakeFileSystem? OpenDir(string gameDir)
    {
        var paks = new List<PakArchive>();
        for (var i = 0; i < 10; ++i)
        {
            var path = Path.Combine(gameDir, $"pak{i}.pak");
            if (!File.Exists(path) || PakArchive.TryOpen(path) is not { } pak)
            {
                break;
            }
            paks.Add(pak);
        }
        return paks.Count > 0 ? new QuakeFileSystem(paks) : null;
    }

    public void Dispose()
    {
        foreach (var p in _paks)
        {
            p.Dispose();
        }
    }
}

/// <summary>Quake's image and sound formats: palette.lmp, .lmp pictures, WAD2 (gfx.wad), a BSP's textures, .wav.</summary>
public static class QuakeFormats
{
    public const int PaletteSize = 768;

    /// <summary>A .lmp picture: width, height, then the pixels.</summary>
    public static IndexedImage? ReadLmp(string name, byte[] data)
    {
        if (data.Length < 8)
        {
            return null;
        }
        var w = BinaryPrimitives.ReadInt32LittleEndian(data);
        var h = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(4));
        if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || (long)w * h > data.Length - 8)
        {
            return null;
        }
        return new IndexedImage(name, w, h, data.AsSpan(8, w * h).ToArray());
    }

    /// <summary>The pictures of a WAD2 file (qpic 'B', miptex 'D', and CONCHARS 'E', a raw 128x128).</summary>
    public static Dictionary<string, IndexedImage> ReadWad(byte[] data)
    {
        var result = new Dictionary<string, IndexedImage>(StringComparer.OrdinalIgnoreCase);
        if (data.Length < 12 || !data.AsSpan(0, 4).SequenceEqual("WAD2"u8))
        {
            return result;
        }
        var count = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(4));
        var table = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(8));
        if (count < 0 || count > 1 << 16 || table < 12 || (long)table + count * 32L > data.Length)
        {
            return result;
        }
        for (var i = 0; i < count; ++i)
        {
            var info = data.AsSpan(table + i * 32, 32);
            var pos = BinaryPrimitives.ReadInt32LittleEndian(info);
            var size = BinaryPrimitives.ReadInt32LittleEndian(info[4..]);
            var type = info[12];
            var compression = info[13];
            var nul = info[16..].IndexOf((byte)0);
            var name = Encoding.Latin1.GetString(nul < 0 ? info[16..] : info[16..(16 + nul)]);
            if (compression != 0 || pos < 0 || size < 0 || (long)pos + size > data.Length)
            {
                continue;
            }
            var lump = data.AsSpan(pos, size);
            IndexedImage? image = type switch
            {
                _ when name.Equals("CONCHARS", StringComparison.OrdinalIgnoreCase) && size >= 128 * 128 =>
                    new IndexedImage(name, 128, 128, lump[..(128 * 128)].ToArray()),
                (byte)'B' => ReadLmp(name, lump.ToArray()),
                (byte)'D' => ReadMiptex(lump, name),
                (byte)'E' when size >= 128 * 128 => new IndexedImage(name, 128, 128, lump[..(128 * 128)].ToArray()),
                _ => null,
            };
            if (image is not null)
            {
                result.TryAdd(name, image);
            }
        }
        return result;
    }

    /// <summary>The textures of a map (BSP29 or BSP2: the texture lump is the third in both).</summary>
    public static List<IndexedImage> ReadBspTextures(byte[] bsp)
    {
        var result = new List<IndexedImage>();
        if (bsp.Length < 4 + 15 * 8)
        {
            return result;
        }
        var version = BinaryPrimitives.ReadInt32LittleEndian(bsp);
        var bsp2 = bsp.AsSpan(0, 4).SequenceEqual("BSP2"u8) || bsp.AsSpan(0, 4).SequenceEqual("2PSB"u8);
        if (version != 29 && !bsp2)
        {
            return result;
        }
        var ofs = BinaryPrimitives.ReadInt32LittleEndian(bsp.AsSpan(4 + 2 * 8));
        var len = BinaryPrimitives.ReadInt32LittleEndian(bsp.AsSpan(4 + 2 * 8 + 4));
        if (ofs < 0 || len < 4 || (long)ofs + len > bsp.Length)
        {
            return result;
        }
        var lump = bsp.AsSpan(ofs, len);
        var count = BinaryPrimitives.ReadInt32LittleEndian(lump);
        if (count < 0 || 4L + count * 4L > len)
        {
            return result;
        }
        for (var i = 0; i < count; ++i)
        {
            var at = BinaryPrimitives.ReadInt32LittleEndian(lump[(4 + i * 4)..]);
            if (at > 0 && at < len && ReadMiptex(lump[at..], null) is { } tex)
            {
                result.Add(tex);
            }
        }
        return result;
    }

    /// <summary>A miptex: name[16], width, height, four mip offsets (from the miptex's start); the first mip only.</summary>
    public static IndexedImage? ReadMiptex(ReadOnlySpan<byte> m, string? name)
    {
        if (m.Length < 40)
        {
            return null;
        }
        var nul = m[..16].IndexOf((byte)0);
        name ??= Encoding.Latin1.GetString(nul < 0 ? m[..16] : m[..nul]);
        var w = BinaryPrimitives.ReadInt32LittleEndian(m[16..]);
        var h = BinaryPrimitives.ReadInt32LittleEndian(m[20..]);
        var mip0 = BinaryPrimitives.ReadInt32LittleEndian(m[24..]);
        if (w <= 0 || h <= 0 || w > 2048 || h > 2048 || mip0 <= 0 || (long)mip0 + (long)w * h > m.Length)
        {
            return null;
        }
        return new IndexedImage(name, w, h, m.Slice(mip0, w * h).ToArray());
    }

    /// <summary>BGRA pixels (premultiplied alpha; index 255 transparent when asked, as in the engine's pictures).</summary>
    public static byte[] ToBgra(IndexedImage image, byte[] palette, bool transparent255 = false)
    {
        var bgra = new byte[image.Width * image.Height * 4];
        for (var i = 0; i < image.Pixels.Length; ++i)
        {
            var c = image.Pixels[i];
            if (transparent255 && c == 255)
            {
                continue;
            }
            bgra[i * 4 + 0] = palette[c * 3 + 2];
            bgra[i * 4 + 1] = palette[c * 3 + 1];
            bgra[i * 4 + 2] = palette[c * 3 + 0];
            bgra[i * 4 + 3] = 255;
        }
        return bgra;
    }

    /// <summary>A RIFF/WAVE PCM file (8 or 16 bit, mono or stereo) as mono floats. Null for anything else.</summary>
    public static WaveSound? ReadWav(byte[] data)
    {
        if (data.Length < 12 || !data.AsSpan(0, 4).SequenceEqual("RIFF"u8) || !data.AsSpan(8, 4).SequenceEqual("WAVE"u8))
        {
            return null;
        }
        int channels = 0, rate = 0, bits = 0, format = 0;
        var pos = 12;
        while (pos + 8 <= data.Length)
        {
            var id = data.AsSpan(pos, 4);
            var size = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(pos + 4));
            var body = pos + 8;
            if (size < 0 || body + (long)size > data.Length)
            {
                size = data.Length - body; // Some of Quake's files have a data chunk longer than the file.
            }
            if (id.SequenceEqual("fmt "u8) && size >= 16)
            {
                format = BinaryPrimitives.ReadInt16LittleEndian(data.AsSpan(body));
                channels = BinaryPrimitives.ReadInt16LittleEndian(data.AsSpan(body + 2));
                rate = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(body + 4));
                bits = BinaryPrimitives.ReadInt16LittleEndian(data.AsSpan(body + 14));
            }
            else if (id.SequenceEqual("data"u8))
            {
                if (format != 1 || channels is < 1 or > 2 || rate is < 4000 or > 192000 || bits is not (8 or 16))
                {
                    return null;
                }
                var frame = channels * bits / 8;
                var frames = size / frame;
                var samples = new float[frames];
                for (var f = 0; f < frames; ++f)
                {
                    float sum = 0;
                    for (var c = 0; c < channels; ++c)
                    {
                        var at = body + f * frame + c * bits / 8;
                        sum += bits == 8 ? (data[at] - 128) / 128f : BinaryPrimitives.ReadInt16LittleEndian(data.AsSpan(at)) / 32768f;
                    }
                    samples[f] = sum / channels;
                }
                return new WaveSound(rate, samples);
            }
            pos = body + size + (size & 1);
        }
        return null;
    }
}
