using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

// Cuts three inventory assets out of the artist's reference sheets:
//
//   inventory_tabs.png   - ten roman numerals in three states, from "tab-numerals-i-x-3-states.png"
//   inventory_sort.png   - the SORT button in three states, from "sort-button-3-states.png"
//   inventory_sygil.png  - the Paladin class banner, from "Character sygil.png"
//
// Each sheet needs a different separation strategy, established by probing them first:
//
//   Tab Numbers   HAS a real alpha channel (over a million fully-transparent pixels). The
//                 glyphs are already cut; bounds come straight from alpha.
//   Sort Button   claims 32-bit but has NO transparent pixel anywhere - a flattened export.
//                 Keyed off its near-black backdrop by luma.
//   Character sygil  no alpha at all, on near-black. Same luma treatment.
//
// Do not assume the declared pixel format means anything here; it lied on two of the three.

class AssetCut
{
    static Bitmap Sheet;
    static int W, H;
    static byte[] Px; // BGRA

    static void Load(string path)
    {
        using (var src = new Bitmap(path))
        {
            W = src.Width; H = src.Height;
            var clone = new Bitmap(W, H, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(clone))
            {
                g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
                g.DrawImage(src, 0, 0, W, H);
            }
            var d = clone.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            Px = new byte[W * H * 4];
            System.Runtime.InteropServices.Marshal.Copy(d.Scan0, Px, 0, Px.Length);
            clone.UnlockBits(d);
            if (Sheet != null) Sheet.Dispose();
            Sheet = clone;
        }
    }

    static byte Alpha(int x, int y) { return Px[(y * W + x) * 4 + 3]; }
    static double Luma(int x, int y)
    {
        int i = (y * W + x) * 4;
        return 0.299 * Px[i + 2] + 0.587 * Px[i + 1] + 0.114 * Px[i + 0];
    }

    /// <summary>Contiguous runs where a per-index count clears a threshold.</summary>
    static List<int[]> Bands(Func<int, int> count, int extent, int minCount, int minLen)
    {
        var res = new List<int[]>();
        int start = -1;
        for (int i = 0; i < extent; i++)
        {
            bool on = count(i) >= minCount;
            if (on && start < 0) start = i;
            if ((!on || i == extent - 1) && start >= 0)
            {
                int end = on ? i : i - 1;
                if (end - start + 1 >= minLen) res.Add(new[] { start, end });
                start = -1;
            }
        }
        return res;
    }

    /// <summary>Draws src rect into dst rect, fitted by aspect ("contain") and centred.</summary>
    static void DrawContained(Graphics g, Rectangle src, Rectangle box)
    {
        double scale = Math.Min((double)box.Width / src.Width, (double)box.Height / src.Height);
        int w = Math.Max(1, (int)Math.Round(src.Width * scale));
        int h = Math.Max(1, (int)Math.Round(src.Height * scale));
        var dst = new Rectangle(box.X + (box.Width - w) / 2, box.Y + (box.Height - h) / 2, w, h);
        g.DrawImage(Sheet, dst, src, GraphicsUnit.Pixel);
    }

    // ---------------------------------------------------------------- tab numerals

    static void CutTabs(string sheetPath, string outPath, int cell, int inactiveH, int activeH)
    {
        Load(sheetPath);
        const int ACut = 40;

        var rows = Bands(y => { int n = 0; for (int x = 0; x < W; x++) if (Alpha(x, y) > ACut) n++; return n; },
                         H, W / 40, 20);
        Console.WriteLine("Tab Numbers: {0} glyph rows", rows.Count);
        foreach (var r in rows) Console.WriteLine("   y {0,4}..{1,-4}", r[0], r[1]);
        if (rows.Count != 3) throw new Exception("expected 3 numeral rows, got " + rows.Count);

        var outBmp = new Bitmap(cell * 10, cell * 3, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;

            for (int s = 0; s < 3; s++)
            {
                int y0 = rows[s][0], y1 = rows[s][1];
                var cols = Bands(x =>
                {
                    int n = 0;
                    for (int y = y0; y <= y1; y++) if (Alpha(x, y) > ACut) n++;
                    return n;
                }, W, 1, 8);
                Console.WriteLine("   state {0}: {1} glyphs", s, cols.Count);
                if (cols.Count != 10) throw new Exception("expected 10 numerals in row " + s + ", got " + cols.Count);

                // Row 1 (index 1, gold) is the selected state and is drawn larger. All three
                // are fitted by aspect into a common height, so "VIII" stays the same height as
                // "I" and simply runs wider - scaling each to fill the cell would make "I"
                // enormous.
                int boxH = (s == 1) ? activeH : inactiveH;
                int boxW = (s == 1) ? cell : cell - 4;

                for (int t = 0; t < 10; t++)
                {
                    var c = cols[t];
                    var src = new Rectangle(c[0], y0, c[1] - c[0] + 1, y1 - y0 + 1);
                    var box = new Rectangle(t * cell + (cell - boxW) / 2, s * cell + (cell - boxH) / 2, boxW, boxH);
                    DrawContained(g, src, box);
                }
            }
        }
        GradeTabs(outBmp, cell);
        outBmp.Save(outPath, ImageFormat.Png);
        Console.WriteLine("   wrote {0} ({1}x{2}, cell {3})", Path.GetFileName(outPath), cell * 10, cell * 3, cell);
        outBmp.Dispose();
    }

    /// <summary>Linear interpolation between two colour stops.</summary>
    static void Lerp(double t, int[] a, int[] b, out double r, out double g, out double bl)
    {
        r = a[0] + (b[0] - a[0]) * t;
        g = a[1] + (b[1] - a[1]) * t;
        bl = a[2] + (b[2] - a[2]) * t;
    }

    /// <summary>
    /// Post-processes the cut numeral strip.
    ///
    /// Two problems, both only visible once the glyphs are down at 26px:
    ///
    /// 1. The sheet has a strong orange radial glow behind the numerals, and it comes through
    ///    in the soft alpha as a dark orange fringe. The game blits with binary transparency
    ///    (index 0 skipped, nothing in between), so that fringe would land as solid orange
    ///    pixels rather than fading out. Alpha is hardened to a threshold here instead.
    ///
    /// 2. The selected state reads orange rather than gold. It IS the artist's gold row, but
    ///    each glyph carries a deep bevel shadow, and averaging that with the highlights while
    ///    downscaling pulls the mean to a muddy bronze (measured R147 G093 B020). Re-mapping
    ///    luminance onto an explicit gold ramp restores the metal while keeping the bevel
    ///    shading, which simply brightening or saturating would not.
    /// </summary>
    static void GradeTabs(Bitmap bmp, int cell)
    {
        const int SelectedState = 1;
        const byte AlphaCut = 96;

        // Shadow, mid and highlight stops of a warm gold.
        int[] shadow = { 74, 52, 12 }, mid = { 196, 154, 46 }, highlight = { 255, 240, 178 };

        var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
        var data = bmp.LockBits(rect, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var buf = new byte[bmp.Width * bmp.Height * 4];
        System.Runtime.InteropServices.Marshal.Copy(data.Scan0, buf, 0, buf.Length);

        for (int y = 0; y < bmp.Height; y++)
        {
            bool selectedRow = (y / cell) == SelectedState;
            for (int x = 0; x < bmp.Width; x++)
            {
                int i = (y * bmp.Width + x) * 4;
                if (buf[i + 3] < AlphaCut) { buf[i] = buf[i + 1] = buf[i + 2] = buf[i + 3] = 0; continue; }
                buf[i + 3] = 255;
                if (!selectedRow) continue;

                double luma = (0.299 * buf[i + 2] + 0.587 * buf[i + 1] + 0.114 * buf[i + 0]) / 255.0;
                // The source mean sits low, so bias the ramp upward - otherwise the whole glyph
                // lands in the shadow half and stays bronze.
                double t = Math.Min(1.0, Math.Pow(luma, 0.62) * 1.15);
                double r, g, b;
                if (t < 0.5) Lerp(t / 0.5, shadow, mid, out r, out g, out b);
                else Lerp((t - 0.5) / 0.5, mid, highlight, out r, out g, out b);
                buf[i + 2] = (byte)Math.Max(0, Math.Min(255, Math.Round(r)));
                buf[i + 1] = (byte)Math.Max(0, Math.Min(255, Math.Round(g)));
                buf[i + 0] = (byte)Math.Max(0, Math.Min(255, Math.Round(b)));
            }
        }

        System.Runtime.InteropServices.Marshal.Copy(buf, 0, data.Scan0, buf.Length);
        bmp.UnlockBits(data);
        Console.WriteLine("   graded: alpha hardened at {0}, selected state remapped to gold", AlphaCut);
    }

    // ---------------------------------------------------------------- SORT button

    static void CutSort(string sheetPath, string outPath, int size)
    {
        Load(sheetPath);
        const double Cut = 45.0;

        // Two row bands: the buttons, then the caption text beneath them. The buttons are the
        // taller band, and come first.
        var rows = Bands(y => { int n = 0; for (int x = 0; x < W; x++) if (Luma(x, y) > Cut) n++; return n; },
                         H, 10, 10);
        Console.WriteLine("Sort Button: {0} row bands", rows.Count);
        foreach (var r in rows) Console.WriteLine("   y {0,4}..{1,-4} h={2}", r[0], r[1], r[1] - r[0] + 1);
        if (rows.Count == 0) throw new Exception("no button row found");
        var row = rows[0];

        var cols = Bands(x =>
        {
            int n = 0;
            for (int y = row[0]; y <= row[1]; y++) if (Luma(x, y) > Cut) n++;
            return n;
        }, W, 3, 10);
        Console.WriteLine("   {0} button columns", cols.Count);
        if (cols.Count != 3) throw new Exception("expected 3 SORT states, got " + cols.Count);

        var outBmp = new Bitmap(size * 3, size, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
            for (int s = 0; s < 3; s++)
            {
                var c = cols[s];
                var src = new Rectangle(c[0], row[0], c[1] - c[0] + 1, row[1] - row[0] + 1);
                g.DrawImage(Sheet, new Rectangle(s * size, 0, size, size), src, GraphicsUnit.Pixel);
            }
        }
        outBmp.Save(outPath, ImageFormat.Png);
        Console.WriteLine("   wrote {0} ({1}x{2}, 3 states at {3}px)", Path.GetFileName(outPath), size * 3, size, size);
        outBmp.Dispose();
    }

    // ---------------------------------------------------------------- class sygil

    static void CutSygil(string sheetPath, string outPath, int targetH)
    {
        Load(sheetPath);
        // Low cut on purpose. The backdrop is near-black (luma ~1-8), while the banner's outer
        // frame is much darker than its lettering - at a cut of 30 the sweep caught only the
        // bright interiors and split the five banners into seven fragments, cropping the top
        // and bottom frame bars clean off. 12 clears the backdrop while keeping the frame.
        const double Cut = 12.0;

        // Five class banners stacked vertically. The Paladin is the first.
        var rows = Bands(y => { int n = 0; for (int x = 0; x < W; x++) if (Luma(x, y) > Cut) n++; return n; },
                         H, W / 20, 30);
        Console.WriteLine("Character sygil: {0} banner rows", rows.Count);
        foreach (var r in rows) Console.WriteLine("   y {0,4}..{1,-4} h={2}", r[0], r[1], r[1] - r[0] + 1);
        if (rows.Count == 0) throw new Exception("no banner found");
        var row = rows[0];

        var cols = Bands(x =>
        {
            int n = 0;
            for (int y = row[0]; y <= row[1]; y++) if (Luma(x, y) > Cut) n++;
            return n;
        }, W, 1, 20);
        if (cols.Count == 0) throw new Exception("could not bound the Paladin banner");
        int x0 = cols[0][0], x1 = cols[cols.Count - 1][1];
        var src = new Rectangle(x0, row[0], x1 - x0 + 1, row[1] - row[0] + 1);
        Console.WriteLine("   Paladin banner: {0}", src);

        int outW = (int)Math.Round(src.Width * (double)targetH / src.Height);
        var outBmp = new Bitmap(outW, targetH, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
            g.DrawImage(Sheet, new Rectangle(0, 0, outW, targetH), src, GraphicsUnit.Pixel);
        }

        // The banner's own backdrop is near-black and must not paint a black slab over the
        // panel's stone. Convert darkness to transparency so only the gold frame and lettering
        // survive, with a soft ramp so the edges do not alias.
        var data = outBmp.LockBits(new Rectangle(0, 0, outW, targetH), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var buf = new byte[outW * targetH * 4];
        System.Runtime.InteropServices.Marshal.Copy(data.Scan0, buf, 0, buf.Length);
        const double Floor = 14.0, Ceil = 60.0;
        for (int i = 0; i < buf.Length; i += 4)
        {
            double luma = 0.299 * buf[i + 2] + 0.587 * buf[i + 1] + 0.114 * buf[i + 0];
            double a = (luma - Floor) / (Ceil - Floor);
            a = Math.Max(0.0, Math.Min(1.0, a));
            buf[i + 3] = (byte)Math.Round(a * 255.0);
        }
        System.Runtime.InteropServices.Marshal.Copy(buf, 0, data.Scan0, buf.Length);
        outBmp.UnlockBits(data);

        outBmp.Save(outPath, ImageFormat.Png);
        Console.WriteLine("   wrote {0} ({1}x{2})", Path.GetFileName(outPath), outW, targetH);
        outBmp.Dispose();
    }

    /// <summary>
    /// Cuts a standalone class plaque (Paladin Sygil v2 and friends) from its black backdrop.
    ///
    /// Deliberately NOT keyed by luminance the way the banner sheet was. This plaque's lettering
    /// is *carved* - the glyphs are darker than the stone around them - so a brightness cut
    /// would punch straight through the word and leave holes. Instead the backdrop is flood
    /// filled inward from the borders: it consumes the black surround (and the stray UI text
    /// bleeding in at the left edge, which is connected to it) while stopping dead at the
    /// plaque, so everything enclosed by the stone survives regardless of how dark it is.
    /// </summary>
    static void CutSygilPlaque(string sheetPath, string outPath, int targetH)
    {
        Load(sheetPath);
        const double BackdropCut = 45.0;

        var background = new bool[W * H];
        var stack = new Stack<int>();
        Action<int, int> seed = (x, y) =>
        {
            int i = y * W + x;
            if (!background[i] && Luma(x, y) < BackdropCut) { background[i] = true; stack.Push(i); }
        };
        for (int x = 0; x < W; x++) { seed(x, 0); seed(x, H - 1); }
        for (int y = 0; y < H; y++) { seed(0, y); seed(W - 1, y); }

        while (stack.Count > 0)
        {
            int i = stack.Pop();
            int x = i % W, y = i / W;
            if (x > 0) seed(x - 1, y);
            if (x < W - 1) seed(x + 1, y);
            if (y > 0) seed(x, y - 1);
            if (y < H - 1) seed(x, y + 1);
        }

        int minX = W, minY = H, maxX = -1, maxY = -1;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (!background[y * W + x])
                {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
        if (maxX < 0) throw new Exception("flood fill consumed the whole plaque - lower BackdropCut");
        Console.WriteLine("Sygil plaque: bounds x {0}..{1}, y {2}..{3}", minX, maxX, minY, maxY);

        int srcW = maxX - minX + 1, srcH = maxY - minY + 1;
        int outW = (int)Math.Round(srcW * (double)targetH / srcH);

        // Punch the flood-filled backdrop out of the source before scaling, so the resample
        // blends plaque against transparency rather than against black (which would leave a
        // dark halo once the alpha is hardened).
        var cut = new Bitmap(srcW, srcH, PixelFormat.Format32bppArgb);
        var cd = cut.LockBits(new Rectangle(0, 0, srcW, srcH), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        var cbuf = new byte[srcW * srcH * 4];
        for (int y = 0; y < srcH; y++)
            for (int x = 0; x < srcW; x++)
            {
                int si = ((minY + y) * W + (minX + x)) * 4, di = (y * srcW + x) * 4;
                bool bg = background[(minY + y) * W + (minX + x)];
                cbuf[di] = Px[si]; cbuf[di + 1] = Px[si + 1]; cbuf[di + 2] = Px[si + 2];
                cbuf[di + 3] = bg ? (byte)0 : (byte)255;
            }
        System.Runtime.InteropServices.Marshal.Copy(cbuf, 0, cd.Scan0, cbuf.Length);
        cut.UnlockBits(cd);

        var outBmp = new Bitmap(outW, targetH, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
            g.DrawImage(cut, new Rectangle(0, 0, outW, targetH));
        }
        cut.Dispose();

        // Binary transparency at runtime, so harden rather than leave a soft edge.
        var data = outBmp.LockBits(new Rectangle(0, 0, outW, targetH), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var buf = new byte[outW * targetH * 4];
        System.Runtime.InteropServices.Marshal.Copy(data.Scan0, buf, 0, buf.Length);
        for (int i = 0; i < buf.Length; i += 4)
            buf[i + 3] = buf[i + 3] >= 128 ? (byte)255 : (byte)0;
        System.Runtime.InteropServices.Marshal.Copy(buf, 0, data.Scan0, buf.Length);
        outBmp.UnlockBits(data);

        outBmp.Save(outPath, ImageFormat.Png);
        Console.WriteLine("   wrote {0} ({1}x{2})  <-- SygilSize in inventory_layout.h must match",
            Path.GetFileName(outPath), outW, targetH);
        outBmp.Dispose();
    }

    static void Main(string[] args)
    {
        string dir = args[0], outDir = args[1];
        // cell, inactive cap-height, active cap-height. The last two are equal: the selected
        // tab is distinguished by colour, not size.
        CutTabs(Path.Combine(dir, "tab-numerals-i-x-3-states.png"), Path.Combine(outDir, "inventory_tabs.png"), 28, 20, 20);
        CutSort(Path.Combine(dir, "sort-button-3-states.png"), Path.Combine(outDir, "inventory_sort.png"), 28);
        CutSygilPlaque(Path.Combine(dir, "sygil-paladin-stone-plaque.png"), Path.Combine(outDir, "inventory_sygil.png"), 48);
        if (Sheet != null) Sheet.Dispose();
    }
}
