using System.Buffers.Binary;
using System.Text;

namespace QuakeVR.Installer.SelfTest;

/// <summary>The reliefs tests' made-up game data (no id or mod data): a BSP29 with chosen miptex, a palette, TGAs.</summary>
static class ReliefFixtures
{
    /// <summary>A palette: index i is (i, 7i mod 256, 255 - i).</summary>
    public static byte[] Palette()
    {
        var p = new byte[768];
        for (var i = 0; i < 256; ++i)
        {
            p[i * 3] = (byte)i;
            p[i * 3 + 1] = (byte)(i * 7);
            p[i * 3 + 2] = (byte)(255 - i);
        }
        return p;
    }

    /// <summary>A BSP29 whose texture lump has these miptex (the first mip only; the rest of the map empty).</summary>
    public static byte[] Bsp(params (string Name, int W, int H, Func<int, int, byte> Pixel)[] textures)
    {
        var miptex = new List<byte[]>();
        foreach (var (name, w, h, pixel) in textures)
        {
            var m = new byte[40 + w * h];
            Encoding.ASCII.GetBytes(name, m);
            BinaryPrimitives.WriteInt32LittleEndian(m.AsSpan(16), w);
            BinaryPrimitives.WriteInt32LittleEndian(m.AsSpan(20), h);
            BinaryPrimitives.WriteInt32LittleEndian(m.AsSpan(24), 40);
            for (var y = 0; y < h; ++y)
            {
                for (var x = 0; x < w; ++x)
                {
                    m[40 + y * w + x] = pixel(x, y);
                }
            }
            miptex.Add(m);
        }
        var lump = new MemoryStream();
        var head = new byte[4 + 4 * miptex.Count];
        BinaryPrimitives.WriteInt32LittleEndian(head, miptex.Count);
        var at = head.Length;
        for (var i = 0; i < miptex.Count; ++i)
        {
            BinaryPrimitives.WriteInt32LittleEndian(head.AsSpan(4 + i * 4), at);
            at += miptex[i].Length;
        }
        lump.Write(head);
        foreach (var m in miptex)
        {
            lump.Write(m);
        }
        var bsp = new byte[124 + lump.Length];
        BinaryPrimitives.WriteInt32LittleEndian(bsp, 29);
        BinaryPrimitives.WriteInt32LittleEndian(bsp.AsSpan(20), 124);
        BinaryPrimitives.WriteInt32LittleEndian(bsp.AsSpan(24), (int)lump.Length);
        lump.ToArray().CopyTo(bsp, 124);
        return bsp;
    }

    /// <summary>An uncompressed 24-bit TGA, bottom-left origin (the usual), of these top-to-bottom RGB rows.</summary>
    public static byte[] Tga24(int w, int h, Func<int, int, (byte R, byte G, byte B)> pixel)
    {
        var d = new byte[18 + w * h * 3];
        d[2] = 2;
        BinaryPrimitives.WriteUInt16LittleEndian(d.AsSpan(12), (ushort)w);
        BinaryPrimitives.WriteUInt16LittleEndian(d.AsSpan(14), (ushort)h);
        d[16] = 24;
        for (var y = 0; y < h; ++y)
        {
            for (var x = 0; x < w; ++x)
            {
                var (r, g, b) = pixel(x, y);
                var o = 18 + ((h - 1 - y) * w + x) * 3;
                (d[o], d[o + 1], d[o + 2]) = (b, g, r);
            }
        }
        return d;
    }

    /// <summary>A 32-bit RLE TGA, top-left origin: one run of <paramref name="run"/> copies of a pixel, then raw pixels.</summary>
    public static byte[] Tga32Rle(int w, int h, int run, (byte R, byte G, byte B, byte A) repeated, (byte R, byte G, byte B, byte A) raw)
    {
        var body = new MemoryStream();
        body.WriteByte((byte)(0x80 | (run - 1)));
        body.Write([repeated.B, repeated.G, repeated.R, repeated.A]);
        var rest = w * h - run;
        body.WriteByte((byte)(rest - 1));
        for (var i = 0; i < rest; ++i)
        {
            body.Write([raw.B, raw.G, raw.R, raw.A]);
        }
        var d = new byte[18];
        d[2] = 10;
        BinaryPrimitives.WriteUInt16LittleEndian(d.AsSpan(12), (ushort)w);
        BinaryPrimitives.WriteUInt16LittleEndian(d.AsSpan(14), (ushort)h);
        d[16] = 32;
        d[17] = 0x28; // top-left, 8 alpha bits
        return [.. d, .. body.ToArray()];
    }
}
