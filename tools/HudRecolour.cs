using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

// Recolours the bottom HUD art to sit with the darkened inventory panel.
//
// Measured first, because the obvious diagnosis was wrong. The HUD reads as "bronze/gold" next to
// the panel, which suggests it is too bright - but it is not. Median luma and mean saturation of
// the shipped assets against the panel:
//
//     inventory panel (target)   luma 42.7   saturation 0.09
//     middle_hud                 luma 31.3   saturation 0.25
//     health_orb                 luma 24.8   saturation 0.82
//     mana_orb                   luma 34.6   saturation 0.82
//     menu_icons                 luma 24.0   saturation 0.73
//
// Every HUD asset is already DARKER than the panel. The mismatch is entirely saturation - the
// panel is near-neutral stone at 0.09 and the HUD carries a warm tint. So this desaturates; it
// deliberately does not darken, which would have made the gap worse.
//
// Not everything can be treated the same way, because on two of these assets the colour carries
// meaning rather than style:
//
//   * The orbs' 0.82 is the red and blue spheres themselves. Draining those would destroy the
//     health/mana read at a glance. Only the frame and statue *outside* the sphere are
//     desaturated, using the same circle geometry the engine drains the orb against
//     (hud_layout.h: sphere centres and OrbSphereRadiusPx), with a soft edge so no ring appears.
//   * menu_icons' 0.73 is the three-state signalling - grey idle, gold hovered, lit when the
//     panel is open. That set was chosen over a greyscale alternative *specifically* because it
//     separates states by colour rather than brightness, which survives being drawn at 30px. It is
//     left untouched, and should stay that way.
//
// Reads from tools/hud_source/ (a pristine snapshot) and writes to the asset trees, so re-running
// is idempotent - transforming the installed files in place would compound every run.
//
// Build/run: tools\build_hud_recolour.cmd

class HudRecolour
{
    /// <summary>
    /// How much chroma to remove. The panel sits at 0.09 saturation and middle_hud at 0.25;
    /// keeping 0.09/0.25 = 36% of the chroma lands it on the panel's neutrality.
    /// </summary>
    const double KeepChroma = 0.36;

    /// <summary>
    /// Sphere geometry, mirroring hud_layout.h. The orb art is installed at exactly these screen
    /// sizes, so these are in asset pixels with no scaling.
    /// </summary>
    const int OrbSphereRadius = 44;
    const int HealthSphereCx = 53, HealthSphereCy = 44;
    const int ManaSphereCx = 49, ManaSphereCy = 44;
    /// <summary>Feather width outside the sphere, so the protected area has no visible edge.</summary>
    const int SphereFeather = 6;

    static byte[] GetBgra(Bitmap src, out int w, out int h)
    {
        w = src.Width; h = src.Height;
        using (var clone = new Bitmap(w, h, PixelFormat.Format32bppArgb))
        {
            using (var g = Graphics.FromImage(clone))
            {
                g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
                g.DrawImage(src, 0, 0, w, h);
            }
            var d = clone.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var buf = new byte[w * h * 4];
            Marshal.Copy(d.Scan0, buf, 0, buf.Length);
            clone.UnlockBits(d);
            return buf;
        }
    }

    static void Save(byte[] buf, int w, int h, string path)
    {
        using (var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb))
        {
            var d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(buf, 0, d.Scan0, buf.Length);
            bmp.UnlockBits(d);
            bmp.Save(path, ImageFormat.Png);
        }
    }

    /// <summary>Blends a pixel toward its own luma. amount 1 = fully neutral.</summary>
    static void Desaturate(byte[] buf, int i, double amount)
    {
        double b = buf[i], g = buf[i + 1], r = buf[i + 2];
        double luma = 0.299 * r + 0.587 * g + 0.114 * b;
        buf[i] = (byte)Math.Round(b + (luma - b) * amount);
        buf[i + 1] = (byte)Math.Round(g + (luma - g) * amount);
        buf[i + 2] = (byte)Math.Round(r + (luma - r) * amount);
    }

    static void Report(string label, byte[] buf, int w, int h)
    {
        double satSum = 0; int n = 0;
        var lum = new System.Collections.Generic.List<double>();
        for (int i = 0; i < buf.Length; i += 4)
        {
            if (buf[i + 3] < 128) continue;
            int b = buf[i], g = buf[i + 1], r = buf[i + 2];
            int mx = Math.Max(r, Math.Max(g, b)), mn = Math.Min(r, Math.Min(g, b));
            satSum += mx == 0 ? 0 : (double)(mx - mn) / mx;
            lum.Add(0.299 * r + 0.587 * g + 0.114 * b);
            n++;
        }
        lum.Sort();
        Console.WriteLine("   {0,-16} luma {1,5:0.0}   saturation {2,5:0.00}", label,
            lum.Count > 0 ? lum[lum.Count / 2] : 0, n > 0 ? satSum / n : 0);
    }

    /// <summary>Uniform desaturation across the whole image.</summary>
    static void ProcessPlate(string inPath, string outPath)
    {
        using (var src = new Bitmap(inPath))
        {
            int w, h;
            var buf = GetBgra(src, out w, out h);
            Console.WriteLine("middle_hud.png");
            Report("before", buf, w, h);
            for (int i = 0; i < buf.Length; i += 4)
            {
                if (buf[i + 3] == 0) continue;
                Desaturate(buf, i, 1.0 - KeepChroma);
            }
            Report("after", buf, w, h);
            Save(buf, w, h, outPath);
        }
    }

    /// <summary>Desaturates only outside the orb's sphere, so the coloured fill is preserved.</summary>
    static void ProcessOrb(string inPath, string outPath, int cx, int cy, string label)
    {
        using (var src = new Bitmap(inPath))
        {
            int w, h;
            var buf = GetBgra(src, out w, out h);
            Console.WriteLine(label);
            Report("before", buf, w, h);

            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    int i = (y * w + x) * 4;
                    if (buf[i + 3] == 0) continue;

                    double dx = x - cx, dy = y - cy;
                    double dist = Math.Sqrt(dx * dx + dy * dy);

                    // Fully protected inside the sphere, fully desaturated beyond the feather,
                    // ramped in between.
                    double outside;
                    if (dist <= OrbSphereRadius) outside = 0.0;
                    else if (dist >= OrbSphereRadius + SphereFeather) outside = 1.0;
                    else outside = (dist - OrbSphereRadius) / SphereFeather;

                    if (outside > 0)
                        Desaturate(buf, i, (1.0 - KeepChroma) * outside);
                }
            }

            Report("after", buf, w, h);
            Save(buf, w, h, outPath);
        }
    }

    // menu_icons.png is a uniform grid: column = state (0 idle, 1 hovered, 2 lit), row = entry.
    const int IconW = 30, IconH = 33;
    const int KeptEntries = 8; // the three mini-map entries were removed; see hud_layout.h

    /// <summary>
    /// Recolours the burger-menu icon sheet and crops it to the entries that still exist.
    ///
    /// This sheet cannot take the uniform desaturation the rest of the HUD gets, because its
    /// colour is the state signal rather than styling - grey idle, gold hovered, red-glow lit.
    /// Flattening all three would leave the states separated by brightness alone, which is the
    /// exact failure the v2 icon sheet was rejected for: at 30px two darkish states are hard to
    /// tell apart.
    ///
    /// So the states are treated differently. Idle and hovered are fully neutralised - their
    /// natural luma already differs enough to read as "normal" and "highlighted". The lit state
    /// keeps half its chroma, so "this panel is open" stays unmistakable while dropping to a
    /// restrained accent instead of a loud glow. That also matches the language the inventory tabs
    /// already set: neutral for inactive, warm for active.
    /// </summary>
    static void ProcessMenuIcons(string inPath, string outPath)
    {
        using (var src = new Bitmap(inPath))
        {
            int w, h;
            var buf = GetBgra(src, out w, out h);
            int rows = h / IconH;
            Console.WriteLine("menu_icons.png ({0} states x {1} rows, keeping {2})", w / IconW, rows, KeptEntries);

            // How much chroma each state keeps, and how its brightness is scaled. Index is column.
            //
            // The brightness column is not cosmetic. Neutralising idle and hovered left them 4
            // luma apart (23.0 vs 27.0) - they were previously separated by *hue*, grey against
            // gold, and removing the hue removed the distinction with it. At 30px that is
            // unreadable, and is exactly why the v2 icon sheet was rejected. Lifting hovered
            // restores the separation in the only channel it has left.
            //
            // lit is lifted more gently: it keeps half its chroma, so it is already distinct by
            // warmth, and it should read as "this panel is open" rather than as the brightest
            // thing on the bar.
            double[] keep = { 0.0, 0.0, 0.50 };
            double[] brightness = { 1.0, 1.60, 1.20 };

            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    int i = (y * w + x) * 4;
                    if (buf[i + 3] == 0) continue;
                    int state = Math.Min(x / IconW, keep.Length - 1);
                    Desaturate(buf, i, 1.0 - keep[state]);
                    double f = brightness[state];
                    if (f != 1.0)
                    {
                        for (int c = 0; c < 3; c++)
                            buf[i + c] = (byte)Math.Max(0, Math.Min(255, Math.Round(buf[i + c] * f)));
                    }
                }
            }

            ReportStates(buf, w, h);

            // Crop away the rows whose entries no longer exist, so the asset cannot disagree with
            // MenuIconCount about how many icons there are.
            int outH = KeptEntries * IconH;
            var cropped = new byte[w * outH * 4];
            Array.Copy(buf, 0, cropped, 0, cropped.Length);
            Save(cropped, w, outH, outPath);
            Console.WriteLine("   cropped {0}x{1} -> {0}x{2}", w, h, outH);
        }
    }

    static void ReportStates(byte[] buf, int w, int h)
    {
        string[] names = { "idle", "hovered", "lit" };
        for (int s = 0; s < 3; s++)
        {
            var lum = new System.Collections.Generic.List<double>();
            double satSum = 0; int n = 0;
            for (int y = 0; y < h; y++)
            {
                for (int x = s * IconW; x < (s + 1) * IconW && x < w; x++)
                {
                    int i = (y * w + x) * 4;
                    if (buf[i + 3] < 128) continue;
                    int b = buf[i], g = buf[i + 1], r = buf[i + 2];
                    int mx = Math.Max(r, Math.Max(g, b)), mn = Math.Min(r, Math.Min(g, b));
                    satSum += mx == 0 ? 0 : (double)(mx - mn) / mx;
                    lum.Add(0.299 * r + 0.587 * g + 0.114 * b);
                    n++;
                }
            }
            lum.Sort();
            Console.WriteLine("   {0,-9} luma {1,5:0.0}   saturation {2,5:0.00}", names[s],
                lum.Count > 0 ? lum[lum.Count / 2] : 0, n > 0 ? satSum / n : 0);
        }
    }

    static void Main(string[] args)
    {
        string srcDir = args[0], outDir = args[1];

        ProcessPlate(Path.Combine(srcDir, "middle_hud.png"), Path.Combine(outDir, "middle_hud.png"));
        ProcessOrb(Path.Combine(srcDir, "health_orb.png"), Path.Combine(outDir, "health_orb.png"),
            HealthSphereCx, HealthSphereCy, "health_orb.png (sphere protected)");
        ProcessOrb(Path.Combine(srcDir, "mana_orb.png"), Path.Combine(outDir, "mana_orb.png"),
            ManaSphereCx, ManaSphereCy, "mana_orb.png (sphere protected)");

        ProcessMenuIcons(Path.Combine(srcDir, "menu_icons.png"), Path.Combine(outDir, "menu_icons.png"));
    }
}
