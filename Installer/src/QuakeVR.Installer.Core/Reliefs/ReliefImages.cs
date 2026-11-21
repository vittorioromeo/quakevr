using System.Buffers.Binary;
using System.IO.Compression;
using System.Text;

namespace QuakeVR.Installer.Core.Reliefs;

/// <summary>Straight (not premultiplied) RGBA pixels, rows top to bottom: what the engine hashes and draws.</summary>
public sealed record RgbaImage(int Width, int Height, byte[] Pixels);

/// <summary>
/// The image files the reliefs step reads and writes, without a window toolkit: PNG (8 and 16 bits a channel, every
/// colour type, not interlaced) and TGA (true colour, grey, colour-mapped, RLE or not) decoded to <see cref="RgbaImage"/>
/// as the engine and <c>pbr_generate.py</c> (Pillow's <c>convert("RGBA")</c>) see them; companions written as 8-bit RGBA
/// PNGs. A file it cannot decode (a JPG, an interlaced PNG) is null: its texture gets no relief.
/// </summary>
public static class ReliefImages
{
    static readonly byte[] PngSignature = [137, 80, 78, 71, 13, 10, 26, 10];
    static readonly uint[] CrcTable = MakeCrcTable();

    public static RgbaImage? Decode(string name, byte[] data)
    {
        try
        {
            return Path.GetExtension(name).ToLowerInvariant() switch
            {
                ".png" => DecodePng(data),
                ".tga" => DecodeTga(data),
                _ => null,
            };
        }
        catch (Exception e) when (e is InvalidDataException or IndexOutOfRangeException or ArgumentException)
        {
            return null;
        }
    }

    public static RgbaImage? DecodePng(byte[] d)
    {
        if (d.Length < 8 || !d.AsSpan(0, 8).SequenceEqual(PngSignature))
        {
            return null;
        }
        int w = 0, h = 0, depth = 0, type = -1, interlace = 0;
        byte[]? plte = null, trns = null;
        var idat = new MemoryStream();
        for (var p = 8; p + 12 <= d.Length;)
        {
            var len = BinaryPrimitives.ReadInt32BigEndian(d.AsSpan(p));
            if (len < 0 || p + 12L + len > d.Length)
            {
                return null;
            }
            var kind = Encoding.ASCII.GetString(d, p + 4, 4);
            var body = d.AsSpan(p + 8, len);
            if (kind == "IHDR" && len >= 13)
            {
                w = BinaryPrimitives.ReadInt32BigEndian(body);
                h = BinaryPrimitives.ReadInt32BigEndian(body[4..]);
                depth = body[8];
                type = body[9];
                interlace = body[12];
            }
            else if (kind == "PLTE")
            {
                plte = body.ToArray();
            }
            else if (kind == "tRNS")
            {
                trns = body.ToArray();
            }
            else if (kind == "IDAT")
            {
                idat.Write(body);
            }
            else if (kind == "IEND")
            {
                break;
            }
            p += 12 + len;
        }
        var channels = type switch { 0 => 1, 2 => 3, 3 => 1, 4 => 2, 6 => 4, _ => 0 };
        if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || interlace != 0 || channels == 0 ||
            !(depth == 8 || (depth == 16 && type != 3)) || (type == 3 && plte is null))
        {
            return null;
        }
        var bpp = channels * depth / 8;
        var stride = w * bpp;
        var raw = new byte[(stride + 1) * h];
        idat.Position = 0;
        using (var z = new ZLibStream(idat, CompressionMode.Decompress))
        {
            if (z.ReadAtLeast(raw, raw.Length, throwOnEndOfStream: false) != raw.Length)
            {
                return null;
            }
        }
        var prev = new byte[stride];
        var cur = new byte[stride];
        var output = new byte[w * h * 4];
        for (var y = 0; y < h; ++y)
        {
            var filter = raw[y * (stride + 1)];
            var src = raw.AsSpan(y * (stride + 1) + 1, stride);
            for (var i = 0; i < stride; ++i)
            {
                int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
                cur[i] = (byte)(src[i] + filter switch
                {
                    0 => 0,
                    1 => a,
                    2 => b,
                    3 => (a + b) >> 1,
                    4 => Paeth(a, b, c),
                    _ => throw new InvalidDataException("PNG filter"),
                });
            }
            for (var x = 0; x < w; ++x)
            {
                var o = (y * w + x) * 4;
                int Sample(int k) => depth == 8 ? cur[x * bpp + k] : cur[x * bpp + 2 * k]; // 16 bits: the high byte
                int Full(int k) => depth == 8 ? cur[x * bpp + k] : (cur[x * bpp + 2 * k] << 8) | cur[x * bpp + 2 * k + 1];
                int TrnsValue(int k) => trns is { } t && t.Length >= 2 * k + 2 ? (t[2 * k] << 8) | t[2 * k + 1] : -1;
                switch (type)
                {
                    case 0:
                        output[o] = output[o + 1] = output[o + 2] = (byte)Sample(0);
                        output[o + 3] = (byte)(Full(0) == TrnsValue(0) ? 0 : 255);
                        break;
                    case 2:
                        output[o] = (byte)Sample(0);
                        output[o + 1] = (byte)Sample(1);
                        output[o + 2] = (byte)Sample(2);
                        output[o + 3] = (byte)(Full(0) == TrnsValue(0) && Full(1) == TrnsValue(1) && Full(2) == TrnsValue(2) ? 0 : 255);
                        break;
                    case 3:
                        var index = cur[x];
                        if (index * 3 + 2 >= plte!.Length)
                        {
                            return null;
                        }
                        output[o] = plte[index * 3];
                        output[o + 1] = plte[index * 3 + 1];
                        output[o + 2] = plte[index * 3 + 2];
                        output[o + 3] = trns is { } t && index < t.Length ? t[index] : (byte)255;
                        break;
                    case 4:
                        output[o] = output[o + 1] = output[o + 2] = (byte)Sample(0);
                        output[o + 3] = (byte)Sample(1);
                        break;
                    default:
                        output[o] = (byte)Sample(0);
                        output[o + 1] = (byte)Sample(1);
                        output[o + 2] = (byte)Sample(2);
                        output[o + 3] = (byte)Sample(3);
                        break;
                }
            }
            (prev, cur) = (cur, prev);
        }
        return new RgbaImage(w, h, output);
    }

    static int Paeth(int a, int b, int c)
    {
        int p = a + b - c, pa = Math.Abs(p - a), pb = Math.Abs(p - b), pc = Math.Abs(p - c);
        return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
    }

    public static RgbaImage? DecodeTga(byte[] d)
    {
        if (d.Length < 18)
        {
            return null;
        }
        int idLength = d[0], mapType = d[1], type = d[2];
        int mapFirst = BinaryPrimitives.ReadUInt16LittleEndian(d.AsSpan(3)), mapLength = BinaryPrimitives.ReadUInt16LittleEndian(d.AsSpan(5));
        int mapBits = d[7];
        int w = BinaryPrimitives.ReadUInt16LittleEndian(d.AsSpan(12)), h = BinaryPrimitives.ReadUInt16LittleEndian(d.AsSpan(14));
        int bits = d[16], descriptor = d[17];
        var rle = type >= 9;
        var baseType = rle ? type - 8 : type;
        if (w == 0 || h == 0 || baseType is < 1 or > 3 || (baseType == 1 && (mapType != 1 || bits != 8)) ||
            (baseType == 2 && bits is not (15 or 16 or 24 or 32)) || (baseType == 3 && bits is not (8 or 16)))
        {
            return null;
        }
        var p = 18 + idLength;
        byte[]? map = null;
        var mapBytes = (mapBits + 7) / 8;
        if (mapType == 1)
        {
            map = new byte[mapLength * 4];
            for (var i = 0; i < mapLength; ++i)
            {
                TrueColour(d.AsSpan(p + i * mapBytes), mapBits, map.AsSpan(i * 4));
            }
            p += mapLength * mapBytes;
        }
        var bytes = (bits + 7) / 8;
        var output = new byte[w * h * 4];
        var pixel = new byte[4];
        void Convert(ReadOnlySpan<byte> s, Span<byte> o)
        {
            switch (baseType)
            {
                case 1:
                    var index = s[0] - mapFirst;
                    if (index < 0 || index >= mapLength)
                    {
                        throw new InvalidDataException("TGA colour index");
                    }
                    map.AsSpan(index * 4, 4).CopyTo(o);
                    break;
                case 2:
                    TrueColour(s, bits, o);
                    break;
                default:
                    o[0] = o[1] = o[2] = s[0];
                    o[3] = bits == 16 ? s[1] : (byte)255;
                    break;
            }
        }
        var count = w * h;
        for (var n = 0; n < count;)
        {
            if (!rle)
            {
                Convert(d.AsSpan(p, bytes), output.AsSpan(n++ * 4, 4));
                p += bytes;
                continue;
            }
            var header = d[p++];
            var run = (header & 0x7f) + 1;
            if ((header & 0x80) != 0)
            {
                Convert(d.AsSpan(p, bytes), pixel);
                p += bytes;
                for (var i = 0; i < run && n < count; ++i)
                {
                    pixel.CopyTo(output.AsSpan(n++ * 4, 4));
                }
            }
            else
            {
                for (var i = 0; i < run && n < count; ++i)
                {
                    Convert(d.AsSpan(p, bytes), output.AsSpan(n++ * 4, 4));
                    p += bytes;
                }
            }
        }
        if ((descriptor & 0x20) == 0) // bottom-left origin: rows top to bottom
        {
            var row = new byte[w * 4];
            for (var y = 0; y < h / 2; ++y)
            {
                var a = output.AsSpan(y * w * 4, w * 4);
                var b = output.AsSpan((h - 1 - y) * w * 4, w * 4);
                a.CopyTo(row);
                b.CopyTo(a);
                row.CopyTo(b);
            }
        }
        if ((descriptor & 0x10) != 0) // right to left
        {
            for (var y = 0; y < h; ++y)
            {
                for (var x = 0; x < w / 2; ++x)
                {
                    var a = output.AsSpan((y * w + x) * 4, 4);
                    var b = output.AsSpan((y * w + w - 1 - x) * 4, 4);
                    a.CopyTo(pixel);
                    b.CopyTo(a);
                    pixel.CopyTo(b);
                }
            }
        }
        return new RgbaImage(w, h, output);
    }

    static void TrueColour(ReadOnlySpan<byte> s, int bits, Span<byte> o)
    {
        if (bits is 15 or 16)
        {
            var v = s[0] | (s[1] << 8);
            int Expand(int x) => (x << 3) | (x >> 2);
            o[0] = (byte)Expand((v >> 10) & 31);
            o[1] = (byte)Expand((v >> 5) & 31);
            o[2] = (byte)Expand(v & 31);
            o[3] = 255;
            return;
        }
        o[0] = s[2];
        o[1] = s[1];
        o[2] = s[0];
        o[3] = bits == 32 ? s[3] : (byte)255;
    }

    /// <summary>An 8-bit RGBA PNG (each row's filter chosen by the smallest sum of magnitudes, as PNG encoders do).</summary>
    public static byte[] EncodePng(RgbaImage image)
    {
        int w = image.Width, h = image.Height, stride = w * 4;
        var px = image.Pixels;
        var filtered = new byte[(stride + 1) * h];
        var candidate = new byte[5][];
        for (var f = 0; f < 5; ++f)
        {
            candidate[f] = new byte[stride];
        }
        for (var y = 0; y < h; ++y)
        {
            var best = 0;
            var bestSum = long.MaxValue;
            for (var f = 0; f < 5; ++f)
            {
                long sum = 0;
                for (var i = 0; i < stride; ++i)
                {
                    int x = px[y * stride + i], a = i >= 4 ? px[y * stride + i - 4] : 0;
                    int b = y > 0 ? px[(y - 1) * stride + i] : 0, c = y > 0 && i >= 4 ? px[(y - 1) * stride + i - 4] : 0;
                    var v = (byte)(x - f switch { 0 => 0, 1 => a, 2 => b, 3 => (a + b) >> 1, _ => Paeth(a, b, c) });
                    candidate[f][i] = v;
                    sum += v < 128 ? v : 256 - v;
                }
                if (sum < bestSum)
                {
                    bestSum = sum;
                    best = f;
                }
            }
            filtered[y * (stride + 1)] = (byte)best;
            candidate[best].CopyTo(filtered, y * (stride + 1) + 1);
        }
        var compressed = new MemoryStream();
        using (var z = new ZLibStream(compressed, CompressionLevel.Optimal, leaveOpen: true))
        {
            z.Write(filtered);
        }
        var output = new MemoryStream();
        output.Write(PngSignature);
        var header = new byte[13];
        BinaryPrimitives.WriteInt32BigEndian(header, w);
        BinaryPrimitives.WriteInt32BigEndian(header.AsSpan(4), h);
        header[8] = 8;
        header[9] = 6;
        Chunk(output, "IHDR", header);
        Chunk(output, "IDAT", compressed.ToArray());
        Chunk(output, "IEND", []);
        return output.ToArray();
    }

    static void Chunk(Stream s, string kind, byte[] body)
    {
        var len = new byte[4];
        BinaryPrimitives.WriteInt32BigEndian(len, body.Length);
        s.Write(len);
        var typed = new byte[4 + body.Length];
        Encoding.ASCII.GetBytes(kind, typed);
        body.CopyTo(typed, 4);
        s.Write(typed);
        var crc = new byte[4];
        BinaryPrimitives.WriteUInt32BigEndian(crc, Crc32(typed));
        s.Write(crc);
    }

    static uint Crc32(byte[] data)
    {
        var c = 0xffffffffu;
        foreach (var b in data)
        {
            c = CrcTable[(c ^ b) & 0xff] ^ (c >> 8);
        }
        return c ^ 0xffffffffu;
    }

    static uint[] MakeCrcTable()
    {
        var table = new uint[256];
        for (var n = 0u; n < 256; ++n)
        {
            var c = n;
            for (var k = 0; k < 8; ++k)
            {
                c = (c & 1) != 0 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            }
            table[n] = c;
        }
        return table;
    }
}
