using System.Buffers.Binary;

namespace QuakeVR.Installer.Core.Detection;

/// <summary>
/// The engine's structural check of one pack resource (<c>validResource</c>, Quake/vr/vr_gamedir.cpp), ported line by
/// line so the installer and the game agree on what "intact" means: an .mdl's skins and frames, a .spr's frames, a
/// .wav's fmt and data chunks, and anything else as a BSP (29, 30, BSP2 or 2PSB) whose lumps fit the file.
/// </summary>
public static class ResourceValidator
{
    public static bool IsValid(ReadOnlySpan<byte> bytes, string name)
    {
        long length = bytes.Length;
        if (length < 12)
        {
            return false;
        }
        var r = new Reader(bytes);

        var ext = Path.GetExtension(name).TrimStart('.');
        if (ext.Equals("mdl", StringComparison.OrdinalIgnoreCase))
        {
            if (length < 84 || !bytes[..4].SequenceEqual("IDPO"u8) || r.Integer(4) != 6)
            {
                return false;
            }
            int skins = r.Integer(48), width = r.Integer(52), height = r.Integer(56);
            int vertices = r.Integer(60), triangles = r.Integer(64), frames = r.Integer(68);
            if (skins <= 0 || width <= 0 || height <= 0 || vertices <= 0 || triangles <= 0 || frames <= 0)
            {
                return false;
            }
            r.Cursor = 84;
            for (var i = 0; i < skins; ++i)
            {
                var type = r.Integer(r.Cursor);
                if (!r.Advance(4))
                {
                    return false;
                }
                var count = 1;
                if (type == 1)
                {
                    count = r.Integer(r.Cursor);
                    if (count <= 0 || !r.Advance(4 + (long)count * 4))
                    {
                        return false;
                    }
                }
                else if (type != 0)
                {
                    return false;
                }
                var image = (long)width * height;
                if (image > length || !r.Advance(image * count))
                {
                    return false;
                }
            }
            if (!r.Advance((long)vertices * 12 + (long)triangles * 16))
            {
                return false;
            }
            for (var i = 0; i < frames; ++i)
            {
                var type = r.Integer(r.Cursor);
                if (!r.Advance(4))
                {
                    return false;
                }
                var count = 1;
                if (type == 1)
                {
                    count = r.Integer(r.Cursor);
                    if (count <= 0 || !r.Advance(12 + (long)count * 4))
                    {
                        return false;
                    }
                }
                else if (type != 0)
                {
                    return false;
                }
                var frame = 24 + (long)vertices * 4;
                if (frame > length || !r.Advance(frame * count))
                {
                    return false;
                }
            }
            return true;
        }
        if (ext.Equals("spr", StringComparison.OrdinalIgnoreCase))
        {
            if (length < 36 || !bytes[..4].SequenceEqual("IDSP"u8) || r.Integer(4) != 1 || r.Integer(24) <= 0)
            {
                return false;
            }
            r.Cursor = 36;
            for (var i = 0; i < r.Integer(24); ++i)
            {
                var type = r.Integer(r.Cursor);
                if (!r.Advance(4))
                {
                    return false;
                }
                var count = 1;
                if (type == 1)
                {
                    count = r.Integer(r.Cursor);
                    if (count <= 0 || !r.Advance(4 + (long)count * 4))
                    {
                        return false;
                    }
                }
                else if (type != 0)
                {
                    return false;
                }
                for (var j = 0; j < count; ++j)
                {
                    int w = r.Integer(r.Cursor + 8), h = r.Integer(r.Cursor + 12);
                    if (w <= 0 || h <= 0 || !r.Advance(16 + (long)w * h))
                    {
                        return false;
                    }
                }
            }
            return true;
        }
        if (ext.Equals("wav", StringComparison.OrdinalIgnoreCase))
        {
            if (length < 44 || !bytes[..4].SequenceEqual("RIFF"u8) || !bytes[8..12].SequenceEqual("WAVE"u8) ||
                r.Integer(4) < 36 || (long)r.Integer(4) + 8 > length)
            {
                return false;
            }
            bool format = false, samples = false;
            r.Cursor = 12;
            var end = (long)r.Integer(4) + 8;
            while (r.Cursor < end)
            {
                if (end - r.Cursor < 8)
                {
                    return false;
                }
                var size = r.Integer(r.Cursor + 4);
                if (size < 0 || size > end - r.Cursor - 8)
                {
                    return false;
                }
                if (bytes.Slice((int)r.Cursor, 4).SequenceEqual("fmt "u8))
                {
                    format = size >= 16;
                }
                if (bytes.Slice((int)r.Cursor, 4).SequenceEqual("data"u8))
                {
                    samples = size > 0;
                }
                // As the engine: original pack WAVs may carry junk LIST chunks or miss the last pad byte.
                if (format && samples)
                {
                    return true;
                }
                if (!r.Advance(8 + (long)size + (size & 1)))
                {
                    return false;
                }
            }
            return format && samples;
        }
        if (length < 124 || (r.Integer(0) != 29 && r.Integer(0) != 30 && !bytes[..4].SequenceEqual("BSP2"u8) &&
                             !bytes[..4].SequenceEqual("2PSB"u8)))
        {
            return false;
        }
        for (var i = 0; i < 15; ++i)
        {
            int pos = r.Integer(4 + i * 8), size = r.Integer(8 + i * 8);
            if (pos < 0 || size < 0 || pos > length || size > length - pos)
            {
                return false;
            }
        }
        return r.Integer(120) >= 64; // at least the world model
    }

    ref struct Reader(ReadOnlySpan<byte> data)
    {
        readonly ReadOnlySpan<byte> _data = data;
        public long Cursor;

        public readonly int Integer(long pos) => pos >= 0 && pos <= _data.Length && _data.Length - pos >= 4
            ? BinaryPrimitives.ReadInt32LittleEndian(_data[(int)pos..]) : 0;

        public bool Advance(long count)
        {
            if (count < 0 || count > _data.Length - Cursor)
            {
                return false;
            }
            Cursor += count;
            return true;
        }
    }
}
