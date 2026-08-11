using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

// Cuts the ten inventory tab buttons, in their three states, out of the artist's reference
// sheet and packs them into one strip.
//
// The sheet holds two sets: a large one (three full-width rows, captioned above) and a small
// one (three rows, captioned at the left). The large set is used - downscaling to the 24x28
// the panel needs preserves far more detail than upscaling the small set would.
//
// Detection is by projection rather than hardcoded offsets: the buttons are light stone on a
// dark charcoal backdrop, so row and column sums separate them cleanly. Only the caption text
// shares that brightness, and it is rejected by band height.

class TabCut
{
    static int W, H;
    static byte[] Px; // BGRA

    static void Load(string path)
    {
        using (var src = new Bitmap(path))
        {
            W = src.Width; H = src.Height;
            using (var clone = new Bitmap(W, H, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(clone))
                {
                    g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
                    g.DrawImage(src, 0, 0, W, H);
                }
                var d = clone.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                Px = new byte[W * H * 4];
                Marshal.Copy(d.Scan0, Px, 0, Px.Length);
                clone.UnlockBits(d);
            }
        }
    }

    static double Luma(int x, int y)
    {
        int i = (y * W + x) * 4;
        return 0.299 * Px[i + 2] + 0.587 * Px[i + 1] + 0.114 * Px[i + 0];
    }

    /// <summary>Contiguous runs where the per-row "bright pixel" count clears a threshold.</summary>
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

    static void Main(string[] args)
    {
        // args: <sheet> <out> <tabW> <tabH> <grow>
        //
        // Every output cell is (tabW + 2*grow) square-ish - the size of a *selected* tab. The
        // selected state fills its cell; the two unselected states are drawn inset by `grow`
        // on each side with transparent padding around them. The game then blits any tab from
        // a uniform grid at one position and the size difference comes out of the art, with no
        // per-state placement at the call site. Mirrors TabCellSize in inventory_layout.h.
        string sheet = args[0], outPath = args[1];
        int tabW = int.Parse(args[2]), tabH = int.Parse(args[3]);
        int grow = args.Length > 4 ? int.Parse(args[4]) : 0;
        int cellW = tabW + 2 * grow, cellH = tabH + 2 * grow;
        Load(sheet);

        // Backdrop is charcoal (~46). Buttons run from ~90 (pressed) to ~210 (selected), so a
        // cut at 70 catches every state including the darkest.
        const double Cut = 70.0;

        // Row bands: a button row is >=40px tall; caption text is much shorter and sparser.
        var rows = Bands(y => { int n = 0; for (int x = 0; x < W; x++) if (Luma(x, y) > Cut) n++; return n; },
                         H, W / 12, 40);
        Console.WriteLine("row bands ({0}):", rows.Count);
        foreach (var r in rows) Console.WriteLine("   y {0,4}..{1,-4} h={2}", r[0], r[1], r[1] - r[0] + 1);

        if (rows.Count < 3) throw new Exception("expected at least 3 button rows");

        // The large set is the three tallest bands, in sheet order.
        var byHeight = new List<int[]>(rows);
        byHeight.Sort((a, b) => (b[1] - b[0]).CompareTo(a[1] - a[0]));
        var chosen = byHeight.GetRange(0, 3);
        chosen.Sort((a, b) => a[0].CompareTo(b[0]));
        Console.WriteLine("using rows: {0}..{1}, {2}..{3}, {4}..{5}",
            chosen[0][0], chosen[0][1], chosen[1][0], chosen[1][1], chosen[2][0], chosen[2][1]);

        var outBmp = new Bitmap(cellW * 10, cellH * 3, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;

            using (var sheetBmp = new Bitmap(sheet))
            {
                for (int s = 0; s < 3; s++)
                {
                    int y0 = chosen[s][0], y1 = chosen[s][1];

                    // Tighten to the button body. The selected state carries a small diamond
                    // marker beneath each button, which the band sweep picks up - left in, it
                    // squashes the button to fit and smears the diamonds into a blurred strip
                    // along the bottom edge. The diamonds span only ~10px per button, so a
                    // stricter width test (a third of the sheet) excludes them while every row
                    // of an actual button clears it comfortably.
                    int strict = W / 3;
                    while (y1 > y0)
                    {
                        int n = 0;
                        for (int x = 0; x < W; x++) if (Luma(x, y1) > Cut) n++;
                        if (n >= strict) break;
                        y1--;
                    }
                    Console.WriteLine("state {0}: body y {1}..{2} (was ..{3})", s, y0, y1, chosen[s][1]);
                    var cols = Bands(x =>
                    {
                        int n = 0;
                        for (int y = y0; y <= y1; y++) if (Luma(x, y) > Cut) n++;
                        return n;
                    }, W, (y1 - y0) / 3, 20);

                    Console.WriteLine("state {0}: {1} columns", s, cols.Count);
                    if (cols.Count != 10)
                    {
                        Console.WriteLine("   WARNING expected 10, got {0}:", cols.Count);
                        foreach (var c in cols) Console.WriteLine("      x {0,4}..{1,-4} w={2}", c[0], c[1], c[1] - c[0] + 1);
                        if (cols.Count < 10) throw new Exception("column detection failed for state " + s);
                    }

                    // State 1 is ON/SELECTED - it fills its cell. The others are inset.
                    bool selected = (s == 1);
                    int inset = selected ? 0 : grow;

                    for (int t = 0; t < 10; t++)
                    {
                        var c = cols[t];
                        var src = new Rectangle(c[0], y0, c[1] - c[0] + 1, y1 - y0 + 1);
                        var dst = new Rectangle(t * cellW + inset, s * cellH + inset,
                            cellW - 2 * inset, cellH - 2 * inset);
                        g.DrawImage(sheetBmp, dst, src, GraphicsUnit.Pixel);
                    }
                }
            }
        }
        outBmp.Save(outPath, ImageFormat.Png);
        outBmp.Dispose();
        Console.WriteLine("wrote {0}  ({1}x{2}, 10 tabs x 3 states, cell {3}x{4}, unselected inset by {5})",
            outPath, cellW * 10, cellH * 3, cellW, cellH, grow);
    }
}
