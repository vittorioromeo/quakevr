using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace QuakeVR.Installer.Skin;

/// <summary>
/// Textures and colours made in code, for the skin before Quake is found or without it: grimy riveted metal, worn
/// bricks, lava, the flames' colour ramp. Everything tiles seamlessly and is deterministic (seeded).
/// </summary>
static class Procedural
{
    // ---- Noise: tileable value noise and its fractal sum ----

    sealed class Lattice
    {
        readonly float[] _v;
        readonly int _nx;
        readonly int _ny;

        public Lattice(int nx, int ny, int seed)
        {
            _nx = nx;
            _ny = ny;
            var r = new Random(seed);
            _v = new float[nx * ny];
            for (var i = 0; i < _v.Length; ++i)
            {
                _v[i] = (float)r.NextDouble();
            }
        }

        /// <summary>The noise at (u, v) in lattice cells; wraps every nx by ny cells.</summary>
        public float At(float u, float v)
        {
            var x0 = (int)MathF.Floor(u);
            var y0 = (int)MathF.Floor(v);
            var fx = Smooth(u - x0);
            var fy = Smooth(v - y0);
            float V(int x, int y) => _v[(((y % _ny) + _ny) % _ny) * _nx + ((x % _nx) + _nx) % _nx];
            var a = V(x0, y0) + (V(x0 + 1, y0) - V(x0, y0)) * fx;
            var b = V(x0, y0 + 1) + (V(x0 + 1, y0 + 1) - V(x0, y0 + 1)) * fx;
            return a + (b - a) * fy;
        }

        static float Smooth(float t) => t * t * (3 - 2 * t);
    }

    /// <summary>Fractal noise in [0, 1] over a size x size tile: <paramref name="cellsX"/> by <paramref name="cellsY"/>
    /// lattice cells at the first octave, twice as many at each next one.</summary>
    public static float[] Fbm(int size, int cellsX, int cellsY, int octaves, int seed, float persistence = 0.5f)
    {
        var result = new float[size * size];
        var amp = 1f;
        var total = 0f;
        for (var o = 0; o < octaves; ++o)
        {
            var nx = cellsX << o;
            var ny = cellsY << o;
            var lattice = new Lattice(nx, ny, seed * 7919 + o * 104729);
            for (var y = 0; y < size; ++y)
            {
                for (var x = 0; x < size; ++x)
                {
                    result[y * size + x] += amp * lattice.At(x * nx / (float)size, y * ny / (float)size);
                }
            }
            total += amp;
            amp *= persistence;
        }
        for (var i = 0; i < result.Length; ++i)
        {
            result[i] /= total;
        }
        return result;
    }

    // ---- Colour helpers ----

    readonly record struct Rgb(float R, float G, float B)
    {
        public static Rgb Hex(uint c) => new(((c >> 16) & 255) / 255f, ((c >> 8) & 255) / 255f, (c & 255) / 255f);
        public static Rgb operator *(Rgb a, float k) => new(a.R * k, a.G * k, a.B * k);
        public static Rgb operator +(Rgb a, Rgb b) => new(a.R + b.R, a.G + b.G, a.B + b.B);
        public Rgb Lerp(Rgb b, float t) => new(R + (b.R - R) * t, G + (b.G - G) * t, B + (b.B - B) * t);
    }

    static void Put(byte[] bgra, int i, Rgb c, float alpha = 1)
    {
        bgra[i * 4 + 0] = (byte)Math.Clamp(c.B * alpha * 255, 0, 255);
        bgra[i * 4 + 1] = (byte)Math.Clamp(c.G * alpha * 255, 0, 255);
        bgra[i * 4 + 2] = (byte)Math.Clamp(c.R * alpha * 255, 0, 255);
        bgra[i * 4 + 3] = (byte)Math.Clamp(alpha * 255, 0, 255);
    }

    public static BitmapSource ToBitmap(byte[] bgra, int size)
    {
        var b = BitmapSource.Create(size, size, 96, 96, PixelFormats.Pbgra32, null, bgra, size * 4);
        b.Freeze();
        return b;
    }

    // ---- Textures ----

    /// <summary>Worn bricks in dark mortar: per-brick tint, bevelled edges, chips and grime streaks.</summary>
    public static byte[] Bricks(int size, int seed)
    {
        var bgra = new byte[size * size * 4];
        var surface = Fbm(size, 8, 8, 5, seed, 0.55f);
        var chips = Fbm(size, 16, 16, 3, seed + 1, 0.5f);
        var streaks = Fbm(size, 16, 2, 3, seed + 2, 0.5f);
        var rng = new Random(seed);
        var rowH = size / 4;
        var brickW = size / 2;
        var tints = new float[64];
        for (var i = 0; i < tints.Length; ++i)
        {
            tints[i] = (float)rng.NextDouble();
        }
        var stoneA = Rgb.Hex(0x3E3229);
        var stoneB = Rgb.Hex(0x5E4A3A);
        var mortar = Rgb.Hex(0x16120F);
        for (var y = 0; y < size; ++y)
        {
            var row = y / rowH;
            var offset = row % 2 == 0 ? 0 : brickW / 2;
            for (var x = 0; x < size; ++x)
            {
                var i = y * size + x;
                var bx = ((x + offset) % size) / brickW;
                var lx = (x + offset) % brickW;
                var ly = y % rowH;
                var tint = tints[(row * 4 + bx) % tints.Length];
                var n = surface[i];
                var c = stoneA.Lerp(stoneB, Math.Clamp(n * 1.2f - 0.1f + (tint - 0.5f) * 0.35f, 0, 1));
                // Mortar (3 texels), then a bevel: lit from the top left.
                var edge = Math.Min(Math.Min(lx, brickW - 1 - lx), Math.Min(ly, rowH - 1 - ly));
                var chip = chips[i] > 0.68f && edge < 5;
                if (edge < 2 || chip)
                {
                    c = mortar.Lerp(c, chip ? 0.25f : 0.1f);
                }
                else if (edge < 4)
                {
                    var lit = (lx < 4 || ly < 4) && !(brickW - 1 - lx < 4 || rowH - 1 - ly < 4);
                    c *= lit ? 1.18f : 0.72f;
                }
                // Grime running down, and darker at the bottom of each brick.
                c *= 0.78f + 0.3f * streaks[i] - 0.12f * (ly / (float)rowH);
                Put(bgra, i, c);
            }
        }
        return bgra;
    }

    /// <summary>Brushed, scratched metal plates with seams, optional rivets and rust.</summary>
    public static byte[] Metal(int size, int seed, bool rivets, float rust)
    {
        var bgra = new byte[size * size * 4];
        var body = Fbm(size, 4, 4, 5, seed, 0.55f);
        var brushed = Fbm(size, 2, 32, 3, seed + 1, 0.6f);
        var rustMap = Fbm(size, 6, 6, 4, seed + 2, 0.55f);
        var scratches = Fbm(size, 64, 3, 2, seed + 3, 0.5f);
        var dark = Rgb.Hex(0x2E2925);
        var light = Rgb.Hex(0x6A5F55);
        var rustColour = Rgb.Hex(0x6B3517);
        var plate = size / 2;
        for (var y = 0; y < size; ++y)
        {
            for (var x = 0; x < size; ++x)
            {
                var i = y * size + x;
                var c = dark.Lerp(light, Math.Clamp(body[i] * 0.8f + brushed[i] * 0.5f - 0.25f, 0, 1));
                if (scratches[i] > 0.74f)
                {
                    c *= 1.15f;
                }
                var r = rustMap[i] - (1 - rust) * 0.5f - 0.2f;
                if (r > 0)
                {
                    c = c.Lerp(rustColour * (0.7f + body[i] * 0.6f), Math.Clamp(r * 4, 0, 0.85f));
                }
                // Seams between plates: a dark groove, lit below and right.
                var px = x % plate;
                var py = y % plate;
                if (px == 0 || py == 0)
                {
                    c *= 0.35f;
                }
                else if (px == 1 || py == 1)
                {
                    c *= 1.3f;
                }
                else if (px == plate - 1 || py == plate - 1)
                {
                    c *= 0.7f;
                }
                // Grime collecting at the bottom of each plate.
                c *= 1 - 0.18f * MathF.Pow(py / (float)plate, 3);
                if (rivets)
                {
                    foreach (var (rx, ry) in new[] { (6, 6), (plate - 7, 6), (6, plate - 7), (plate - 7, plate - 7) })
                    {
                        var dx = px - rx;
                        var dy = py - ry;
                        var d2 = dx * dx + dy * dy;
                        if (d2 <= 9)
                        {
                            var shade = 1.25f - (dx + dy) * 0.12f;
                            c = light * shade * 0.95f;
                        }
                        else if (d2 <= 14 && dx + dy > 0)
                        {
                            c *= 0.45f; // The rivet's shadow.
                        }
                    }
                }
                Put(bgra, i, c);
            }
        }
        return bgra;
    }

    /// <summary>Glowing lava: bright veins in a dark crust.</summary>
    public static byte[] Lava(int size)
    {
        var bgra = new byte[size * size * 4];
        var n = Fbm(size, 3, 3, 4, 77, 0.55f);
        var veins = Fbm(size, 4, 4, 3, 78, 0.5f);
        var ramp = new[] { Rgb.Hex(0x2A0602), Rgb.Hex(0x7A1604), Rgb.Hex(0xC8400A), Rgb.Hex(0xF08A1E), Rgb.Hex(0xFFD36A) };
        for (var i = 0; i < size * size; ++i)
        {
            var ridge = 1 - MathF.Abs(veins[i] * 2 - 1);
            var t = Math.Clamp(n[i] * 0.6f + MathF.Pow(ridge, 3) * 0.7f, 0, 0.999f) * (ramp.Length - 1);
            var k = (int)t;
            Put(bgra, i, ramp[k].Lerp(ramp[k + 1], t - k));
        }
        return bgra;
    }

    /// <summary>The flames' colours: 256 premultiplied BGRA values from transparent to white-hot. With Quake's palette,
    /// each colour is the palette's nearest one (the game's own 8-bit fire), the alpha still fading the cold end.</summary>
    public static uint[] FireRamp(byte[]? palette)
    {
        var stops = new (float At, Rgb C, float A)[]
        {
            (0.00f, Rgb.Hex(0x000000), 0f),
            (0.10f, Rgb.Hex(0x3C0802), 0.0f),
            (0.25f, Rgb.Hex(0x8E1A04), 0.55f),
            (0.45f, Rgb.Hex(0xD24A0C), 0.9f),
            (0.70f, Rgb.Hex(0xF5901E), 1f),
            (0.86f, Rgb.Hex(0xFFC850), 1f),
            (1.00f, Rgb.Hex(0xFFF2C0), 1f),
        };
        var ramp = new uint[256];
        for (var i = 0; i < 256; ++i)
        {
            var t = i / 255f;
            var k = 0;
            while (k < stops.Length - 2 && t > stops[k + 1].At)
            {
                ++k;
            }
            var f = (t - stops[k].At) / (stops[k + 1].At - stops[k].At);
            var c = stops[k].C.Lerp(stops[k + 1].C, f);
            var a = stops[k].A + (stops[k + 1].A - stops[k].A) * f;
            if (palette is not null && a > 0)
            {
                c = Nearest(palette, c);
            }
            var bgr = new byte[4];
            Put(bgr, 0, c, a);
            ramp[i] = BitConverter.ToUInt32(bgr);
        }
        return ramp;
    }

    static Rgb Nearest(byte[] palette, Rgb c)
    {
        var best = 0;
        var bestD = float.MaxValue;
        for (var i = 0; i < 255; ++i)
        {
            var dr = palette[i * 3] / 255f - c.R;
            var dg = palette[i * 3 + 1] / 255f - c.G;
            var db = palette[i * 3 + 2] / 255f - c.B;
            var d = dr * dr * 0.3f + dg * dg * 0.59f + db * db * 0.11f;
            if (d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
        return new Rgb(palette[best * 3] / 255f, palette[best * 3 + 1] / 255f, palette[best * 3 + 2] / 255f);
    }
}
