// OracoolAssetStudio - preview and prepare art for Diablo Oracool Edition.
//
// The engine renders to an 8-bit palettized surface. Art authored as full-colour PNG is quantized
// to palette indices at load time (Source/oracool/hud_art.cpp), which can change how it looks in
// ways that are invisible until it is in the game. This tool does that same quantization up front
// so the result can be judged - and adjusted - without a build-and-launch cycle.
//
// Three things it deliberately mirrors from the engine:
//
//   1. Quantization matches only the GLOBAL half of the palette, entries 128-255. Those are
//      identical across town and every dungeon type (see engine/palette.h), which is why one
//      quantization pass works everywhere. Entries 0-127 are level-specific and colour-cycled and
//      must never be matched against.
//   2. Transparency is BINARY. The engine blits with BlitFromSkipColorIndexZero - index 0 is
//      skipped and there is nothing in between, so soft alpha edges become hard ones. Previewing
//      that honestly is the point; a soft-edged source that looks fine in an image editor can come
//      out with a hard halo in game.
//   3. The "dark" preview approximates the engine's light table: at reduced light each colour is
//      scaled toward black and then re-matched to the nearest available palette entry. Colours
//      that are distinct at full light can collapse onto the same entry in the dark, which is a
//      real and easily-missed failure for anything drawn in the world.
//
// Note UI panels themselves are drawn unlit, so for HUD/inventory art the lit preview is the one
// that matters; the dark preview matters for anything that appears in the world (item sprites on
// the ground, for instance).
//
// Build: tools\build_asset_studio.cmd  (in-box csc.exe, no bundled runtime)

using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace OracoolAssetStudio
{
    /// <summary>A 256-entry Diablo palette loaded from a raw 768-byte .pal file.</summary>
    class Palette
    {
        public readonly Color[] Entries = new Color[256];
        public string Name = "(none)";

        public static Palette Load(string path)
        {
            var bytes = File.ReadAllBytes(path);
            if (bytes.Length < 768)
                throw new Exception("Not a Diablo palette: expected 768 bytes, got " + bytes.Length);
            var p = new Palette { Name = Path.GetFileName(path) };
            for (int i = 0; i < 256; i++)
                p.Entries[i] = Color.FromArgb(bytes[i * 3], bytes[i * 3 + 1], bytes[i * 3 + 2]);
            return p;
        }

        /// <summary>Nearest entry within [first,last] by squared RGB distance.</summary>
        public int NearestIndex(int r, int g, int b, int first, int last)
        {
            int best = first, bestDist = int.MaxValue;
            for (int i = first; i <= last; i++)
            {
                Color c = Entries[i];
                int dr = c.R - r, dg = c.G - g, db = c.B - b;
                int d = dr * dr + dg * dg + db * db;
                if (d < bestDist) { bestDist = d; best = i; }
            }
            return best;
        }
    }

    /// <summary>Colour adjustments applied to the source before quantization.</summary>
    struct Adjustments
    {
        public double Brightness;  // -1 .. +1
        public double Contrast;    // -1 .. +1
        public double Saturation;  // -1 .. +1  (-1 = greyscale)
        public double Gamma;       // 0.2 .. 3.0
        public double HueShift;    // degrees

        public static Adjustments Default()
        {
            return new Adjustments { Brightness = 0, Contrast = 0, Saturation = 0, Gamma = 1.0, HueShift = 0 };
        }

        public bool IsIdentity()
        {
            return Brightness == 0 && Contrast == 0 && Saturation == 0 && Gamma == 1.0 && HueShift == 0;
        }
    }

    static class Img
    {
        public static byte[] GetBgra(Bitmap src, out int w, out int h)
        {
            w = src.Width; h = src.Height;
            using (var clone = new Bitmap(w, h, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(clone))
                {
                    g.CompositingMode = CompositingMode.SourceCopy;
                    g.DrawImage(src, 0, 0, w, h);
                }
                var data = clone.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                var buf = new byte[w * h * 4];
                Marshal.Copy(data.Scan0, buf, 0, buf.Length);
                clone.UnlockBits(data);
                return buf;
            }
        }

        public static Bitmap FromBgra(byte[] buf, int w, int h)
        {
            var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
            var data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(buf, 0, data.Scan0, buf.Length);
            bmp.UnlockBits(data);
            return bmp;
        }

        static double Clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

        /// <summary>Applies the colour adjustments in place. Alpha is untouched.</summary>
        public static void Apply(byte[] buf, Adjustments a)
        {
            if (a.IsIdentity()) return;

            // Contrast as a pivot around mid-grey; the classic (259*(c+255))/(255*(259-c)) curve.
            double c = a.Contrast * 255.0;
            double contrastFactor = (259.0 * (c + 255.0)) / (255.0 * (259.0 - c));
            double cosH = Math.Cos(a.HueShift * Math.PI / 180.0);
            double sinH = Math.Sin(a.HueShift * Math.PI / 180.0);

            for (int i = 0; i < buf.Length; i += 4)
            {
                double b = buf[i] / 255.0, g = buf[i + 1] / 255.0, r = buf[i + 2] / 255.0;

                if (a.Gamma != 1.0)
                {
                    double inv = 1.0 / a.Gamma;
                    r = Math.Pow(r, inv); g = Math.Pow(g, inv); b = Math.Pow(b, inv);
                }

                if (a.Brightness != 0) { r += a.Brightness; g += a.Brightness; b += a.Brightness; }

                if (a.Contrast != 0)
                {
                    r = (r - 0.5) * contrastFactor + 0.5;
                    g = (g - 0.5) * contrastFactor + 0.5;
                    b = (b - 0.5) * contrastFactor + 0.5;
                }

                if (a.Saturation != 0)
                {
                    double luma = 0.299 * r + 0.587 * g + 0.114 * b;
                    double s = 1.0 + a.Saturation;
                    r = luma + (r - luma) * s;
                    g = luma + (g - luma) * s;
                    b = luma + (b - luma) * s;
                }

                if (a.HueShift != 0)
                {
                    // Rotation about the luma axis in YIQ - cheap and good enough for asset tuning.
                    double y = 0.299 * r + 0.587 * g + 0.114 * b;
                    double iq1 = 0.596 * r - 0.274 * g - 0.322 * b;
                    double iq2 = 0.211 * r - 0.523 * g + 0.312 * b;
                    double i2 = iq1 * cosH - iq2 * sinH;
                    double q2 = iq1 * sinH + iq2 * cosH;
                    r = y + 0.956 * i2 + 0.621 * q2;
                    g = y - 0.272 * i2 - 0.647 * q2;
                    b = y - 1.106 * i2 + 1.703 * q2;
                }

                buf[i] = (byte)Math.Round(Clamp01(b) * 255.0);
                buf[i + 1] = (byte)Math.Round(Clamp01(g) * 255.0);
                buf[i + 2] = (byte)Math.Round(Clamp01(r) * 255.0);
            }
        }
    }

    /// <summary>Result of quantizing an adjusted image against a palette.</summary>
    class Quantized
    {
        public int Width, Height;
        public byte[] Index;        // palette index per pixel
        public bool[] Transparent;  // true where the pixel is dropped entirely
        public int DistinctColors;
        public int TransparentCount;
        public double MeanError;    // mean RGB distance introduced by quantization
        public double MaxError;
    }

    static class Quantizer
    {
        public const int GlobalFirst = 128, GlobalLast = 255;

        public static Quantized Run(byte[] bgra, int w, int h, Palette pal, bool globalHalfOnly, int alphaCut)
        {
            int first = globalHalfOnly ? GlobalFirst : 0;
            int last = globalHalfOnly ? GlobalLast : 255;

            var q = new Quantized { Width = w, Height = h, Index = new byte[w * h], Transparent = new bool[w * h] };
            var seen = new bool[256];
            double errSum = 0; int errCount = 0;

            // Memoize: art is highly repetitive and the nearest-entry scan is the hot path.
            var cache = new System.Collections.Generic.Dictionary<int, int>();

            for (int p = 0, i = 0; i < bgra.Length; i += 4, p++)
            {
                if (bgra[i + 3] < alphaCut) { q.Transparent[p] = true; q.TransparentCount++; continue; }

                int b = bgra[i], g = bgra[i + 1], r = bgra[i + 2];
                int key = (r << 16) | (g << 8) | b;
                int idx;
                if (!cache.TryGetValue(key, out idx))
                {
                    idx = pal.NearestIndex(r, g, b, first, last);
                    cache[key] = idx;
                }
                q.Index[p] = (byte)idx;
                seen[idx] = true;

                Color pc = pal.Entries[idx];
                double d = Math.Sqrt((pc.R - r) * (pc.R - r) + (pc.G - g) * (pc.G - g) + (pc.B - b) * (pc.B - b));
                errSum += d; errCount++;
                if (d > q.MaxError) q.MaxError = d;
            }

            foreach (bool s in seen) if (s) q.DistinctColors++;
            q.MeanError = errCount > 0 ? errSum / errCount : 0;
            return q;
        }

        /// <summary>
        /// Approximates the engine's light table: scale each palette entry toward black by
        /// `light` (1.0 = full brightness) and re-match to the nearest *available* entry. The
        /// re-match is the important part - it is what reveals colours collapsing together in the
        /// dark, which simple darkening of the output would hide.
        /// </summary>
        public static int[] BuildLightRemap(Palette pal, double light, bool globalHalfOnly)
        {
            int first = globalHalfOnly ? GlobalFirst : 0;
            int last = globalHalfOnly ? GlobalLast : 255;
            var map = new int[256];
            for (int i = 0; i < 256; i++)
            {
                Color c = pal.Entries[i];
                int r = (int)Math.Round(c.R * light);
                int g = (int)Math.Round(c.G * light);
                int b = (int)Math.Round(c.B * light);
                map[i] = pal.NearestIndex(r, g, b, first, last);
            }
            return map;
        }

        public static Bitmap Render(Quantized q, Palette pal, int[] lightRemap, Color background)
        {
            var buf = new byte[q.Width * q.Height * 4];
            for (int p = 0, i = 0; p < q.Index.Length; p++, i += 4)
            {
                Color c;
                if (q.Transparent[p]) c = background;
                else
                {
                    int idx = q.Index[p];
                    if (lightRemap != null) idx = lightRemap[idx];
                    c = pal.Entries[idx];
                }
                buf[i] = c.B; buf[i + 1] = c.G; buf[i + 2] = c.R; buf[i + 3] = 255;
            }
            return Img.FromBgra(buf, q.Width, q.Height);
        }

        /// <summary>Exports palette-exact RGBA with hard alpha, ready for the engine's loader.</summary>
        public static Bitmap Export(Quantized q, Palette pal)
        {
            var buf = new byte[q.Width * q.Height * 4];
            for (int p = 0, i = 0; p < q.Index.Length; p++, i += 4)
            {
                if (q.Transparent[p]) { buf[i] = buf[i + 1] = buf[i + 2] = buf[i + 3] = 0; continue; }
                Color c = pal.Entries[q.Index[p]];
                buf[i] = c.B; buf[i + 1] = c.G; buf[i + 2] = c.R; buf[i + 3] = 255;
            }
            return Img.FromBgra(buf, q.Width, q.Height);
        }
    }
}
