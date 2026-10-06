using System.Buffers.Binary;
using System.Text;

namespace QuakeVR.Installer.Core.Detection;

public sealed record PakEntry(string Name, int Offset, int Length);

/// <summary>Quake's .pak archives: a 12-byte header ("PACK", directory offset, directory length) and 64-byte entries
/// (56-byte name, offset, length). The checks are the engine's (<c>inspectPack</c>, Quake/vr/vr_gamedir.cpp).</summary>
public static class PakFile
{
    public const int EntrySize = 64;

    /// <summary>The directory, or null when the header or an entry is malformed or points past the file's end.
    /// <paramref name="maxEntries"/>: the engine's limit for mission packs is 2048; id1's rerelease pak has more.</summary>
    public static List<PakEntry>? TryReadDirectory(Stream stream, int maxEntries = 2048)
    {
        var size = stream.Length;
        Span<byte> header = stackalloc byte[12];
        stream.Position = 0;
        if (stream.ReadAtLeast(header, 12, throwOnEndOfStream: false) != 12 || !header[..4].SequenceEqual("PACK"u8))
        {
            return null;
        }
        var offset = BinaryPrimitives.ReadInt32LittleEndian(header[4..]);
        var length = BinaryPrimitives.ReadInt32LittleEndian(header[8..]);
        if (offset < 12 || length <= 0 || length % EntrySize != 0 || length / EntrySize > maxEntries || offset > size ||
            length > size - offset)
        {
            return null;
        }
        var directory = new byte[length];
        stream.Position = offset;
        if (stream.ReadAtLeast(directory, length, throwOnEndOfStream: false) != length)
        {
            return null;
        }
        var entries = new List<PakEntry>(length / EntrySize);
        for (var i = 0; i < length; i += EntrySize)
        {
            var e = directory.AsSpan(i, EntrySize);
            var nul = e[..56].IndexOf((byte)0);
            var pos = BinaryPrimitives.ReadInt32LittleEndian(e[56..]);
            var len = BinaryPrimitives.ReadInt32LittleEndian(e[60..]);
            if (nul < 0 || pos < 0 || len < 0 || pos > size || len > size - pos)
            {
                return null;
            }
            entries.Add(new PakEntry(Encoding.Latin1.GetString(e[..nul]), pos, len));
        }
        return entries;
    }

    /// <summary>Writes a pak (tests and the fake packages only).</summary>
    public static void Write(string path, IReadOnlyList<(string Name, byte[] Data)> files)
    {
        using var fs = File.Create(path);
        var dataStart = 12;
        var offsets = new List<int>();
        fs.Position = dataStart;
        foreach (var (_, data) in files)
        {
            offsets.Add((int)fs.Position);
            fs.Write(data);
        }
        var dirOffset = (int)fs.Position;
        foreach (var ((name, data), off) in files.Zip(offsets))
        {
            var entry = new byte[EntrySize];
            Encoding.Latin1.GetBytes(name).CopyTo(entry, 0);
            BinaryPrimitives.WriteInt32LittleEndian(entry.AsSpan(56), off);
            BinaryPrimitives.WriteInt32LittleEndian(entry.AsSpan(60), data.Length);
            fs.Write(entry);
        }
        fs.Position = 0;
        Span<byte> header = stackalloc byte[12];
        "PACK"u8.CopyTo(header);
        BinaryPrimitives.WriteInt32LittleEndian(header[4..], dirOffset);
        BinaryPrimitives.WriteInt32LittleEndian(header[8..], files.Count * EntrySize);
        fs.Write(header);
    }
}
