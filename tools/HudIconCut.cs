using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

// Cuts two button sheets into game-ready strips:
//
//   town_portal_icon.png  - the belt's Town Portal button, 3 states
//   inventory_sort.png    - the inventory SORT button, 3 states
//
// Both sheets are grids of framed tiles on a near-black backdrop, so both are located the same
// way: a brightness sweep for row bands, then column bands within each row.

class HudIconCut
{
    // ---------------------------------------------------------------- shared

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

    static double Luma(int x, int y)
    {
        int i = (y * W + x) * 4;
        return 0.299 * Px[i + 2] + 0.587 * Px[i + 1] + 0.114 * Px[i + 0];
    }

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

    static List<int[]> RowBands(double cut, int minLen)
    {
        return Bands(y => { int n = 0; for (int x = 0; x < W; x++) if (Luma(x, y) > cut) n++; return n; }, H, W / 24, minLen);
    }

    static List<int[]> ColBands(int y0, int y1, double cut, int minLen)
    {
        return Bands(x => { int n = 0; for (int y = y0; y <= y1; y++) if (Luma(x, y) > cut) n++; return n; }, W, (y1 - y0) / 6, minLen);
    }

    /// <summary>
    /// Brightens a column range. Locks the whole bitmap and filters by x rather than locking the
    /// sub-rectangle: LockBits on a sub-rect returns a buffer whose stride belongs to that rect,
    /// and treating it as width*4 silently writes back to the wrong offsets - the first attempt
    /// did exactly that and moved the mean luma by 1 instead of 35%.
    /// </summary>
    static void BrightenColumns(Bitmap bmp, int x0, int x1, double factor)
    {
        var full = new Rectangle(0, 0, bmp.Width, bmp.Height);
        var d = bmp.LockBits(full, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var buf = new byte[bmp.Width * bmp.Height * 4];
        System.Runtime.InteropServices.Marshal.Copy(d.Scan0, buf, 0, buf.Length);
        for (int y = 0; y < bmp.Height; y++)
        {
            for (int x = x0; x < x1 && x < bmp.Width; x++)
            {
                int i = (y * bmp.Width + x) * 4;
                for (int c = 0; c < 3; c++)
                    buf[i + c] = (byte)Math.Max(0, Math.Min(255, Math.Round(buf[i + c] * factor)));
            }
        }
        System.Runtime.InteropServices.Marshal.Copy(buf, 0, d.Scan0, buf.Length);
        bmp.UnlockBits(d);
    }

    // ---------------------------------------------------------------- Town Portal

    /// <summary>
    /// Cuts one design's three states from the Town Portal sheet.
    ///
    /// Only the tile's *interior* is taken, not its frame. The belt cell already has its own
    /// carved bevel in the plate art, so a framed icon would double-frame at 33px. More
    /// importantly the plate has a blue portal ring painted into that cell already - the icon has
    /// to cover it completely or the old ring shows around the new one, so the interior crop
    /// (opaque dark panel plus archway) is what makes this work at all.
    ///
    /// State order is deliberately NOT the sheet's. Per the design call: the portal is always
    /// available, so it should always look energised. ACTIVE becomes the resting state, CLICKED
    /// (the strongest glow) becomes hover, and INACTIVE - the dim, unlit tile - becomes the
    /// pressed state, so clicking visibly dims it.
    /// </summary>
    static void CutTownPortal(string sheetPath, string outPath, int design, int cellW, int cellH)
    {
        Load(sheetPath);
        const double Cut = 42.0;

        var rows = RowBands(Cut, 40);
        Console.WriteLine("Town Portal sheet: {0} row bands", rows.Count);
        foreach (var r in rows) Console.WriteLine("      y {0,4}..{1,-4} h={2}", r[0], r[1], r[1] - r[0] + 1);
        if (rows.Count < 3) throw new Exception("expected 3 design rows, got " + rows.Count);

        // Take the three tallest bands as the design rows, in sheet order. The sheet's title and
        // the per-tile state captions are much shorter, and whether they register at all depends
        // on the brightness cut - selecting by height does not care either way.
        var byHeight = new List<int[]>(rows);
        byHeight.Sort((a, b) => (b[1] - b[0]).CompareTo(a[1] - a[0]));
        var designRows = byHeight.GetRange(0, 3);
        designRows.Sort((a, b) => a[0].CompareTo(b[0]));
        int rowIdx = design / 3, colGroup = design % 3;
        if (rowIdx >= designRows.Count) throw new Exception("design " + design + " is outside the sheet");

        int y0 = designRows[rowIdx][0], y1 = designRows[rowIdx][1];
        var cols = ColBands(y0, y1, Cut, 20);
        Console.WriteLine("   design row {0}: y {1}..{2}, {3} tiles", rowIdx, y0, y1, cols.Count);
        if (cols.Count < 9) throw new Exception("expected 9 tiles across a design row, got " + cols.Count);

        // Three designs per row, three states each.
        int baseCol = colGroup * 3;

        // Sheet order is INACTIVE, ACTIVE, CLICKED. Output order is the game's:
        // 0 = resting, 1 = hovered, 2 = pressed.
        int[] sheetForState = { 1, 2, 0 };

        var outBmp = new Bitmap(cellW * 3, cellH, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;

            for (int state = 0; state < 3; state++)
            {
                var c = cols[baseCol + sheetForState[state]];
                int tw = c[1] - c[0] + 1, th = y1 - y0 + 1;
                // Inset past the tile's own frame - roughly an eighth of its width each side.
                int inset = Math.Max(2, tw / 8);
                var src = new Rectangle(c[0] + inset, y0 + inset, tw - 2 * inset, th - 2 * inset);
                g.DrawImage(Sheet, new Rectangle(state * cellW, 0, cellW, cellH), src, GraphicsUnit.Pixel);
            }
        }

        // Force alpha on, and lift the black floor.
        //
        // The floor matters more than it looks. The engine blits with BlitFromSkipColorIndexZero,
        // and palette entry 0 is pure black - so any pixel dark enough to quantize onto it is not
        // drawn at all. This icon is drawn *over* the portal ring already painted into the plate,
        // so a hole would not show background: it would show the old ring through the new icon.
        // Keeping every channel above the floor guarantees nothing lands on entry 0.
        const int BlackFloor = 12;
        var data = outBmp.LockBits(new Rectangle(0, 0, outBmp.Width, outBmp.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var buf = new byte[outBmp.Width * outBmp.Height * 4];
        System.Runtime.InteropServices.Marshal.Copy(data.Scan0, buf, 0, buf.Length);
        for (int i = 0; i < buf.Length; i += 4)
        {
            buf[i + 3] = 255;
            for (int c = 0; c < 3; c++) if (buf[i + c] < BlackFloor) buf[i + c] = BlackFloor;
        }
        System.Runtime.InteropServices.Marshal.Copy(buf, 0, data.Scan0, buf.Length);
        outBmp.UnlockBits(data);

        outBmp.Save(outPath, ImageFormat.Png);
        Console.WriteLine("   wrote {0} ({1}x{2}, design {3}, states resting/hover/pressed)",
            Path.GetFileName(outPath), cellW * 3, cellH, design + 1);
        outBmp.Dispose();
    }

    // ---------------------------------------------------------------- burger menu button

    /// <summary>
    /// Cuts one design's two states from the burger-menu button sheet into the three the game
    /// draws.
    ///
    /// Same interior-only crop and black-floor lift as the Town Portal button, and for the same
    /// reasons - see CutTownPortal. The Menu cell also has its own carved bevel in the plate art.
    ///
    /// The Menu button is a toggle rather than a momentary action: it stays lit for as long as
    /// its popup is open. So ACTIVE maps to the *open* state, not to a click flash, and hover is
    /// synthesised by brightening the resting state since the sheet has no hover art.
    /// </summary>
    static void CutBurgerMenu(string sheetPath, string outPath, int design, int cellW, int cellH)
    {
        Load(sheetPath);
        // Much lower than the Town Portal sheet's cut, and the value is measured rather than
        // guessed: profiling a design row gives a backdrop of 3.9 and a tile interior of 29.6.
        // 14 sits between them, so a tile reads as one solid run.
        //
        // Getting this wrong is not subtle but it is silent. At 42 the row sweep merged rows and
        // found two instead of three. At 22 the cut ran *through* the tile bodies, so the column
        // sweep locked onto the bright gold bars instead and produced 86px "tiles" - a crop of one
        // button's interior, scaled up to look like a whole icon.
        const double Cut = 14.0;

        if (design != 6)
            throw new Exception("only design 7 is measured; see the note below before using another");

        // Measured, not detected - the same call as the SORT button's source rect.
        //
        // Projection does find these bands, but not reliably: the inactive tile is dim enough
        // that its detected width comes out 93px against the active tile's 159px, and small
        // changes to the threshold merge the pairs or lock onto the gold bars instead. Rather
        // than tune a heuristic until it happens to work on one sheet, the two rects for design 7
        // are pinned from a column profile of its row (band centres x=148 and x=357, row
        // y 749..882) and given a common width so both states crop identically.
        //
        // To use a different design, profile its row the same way and pin its pair here.
        // A square tile centred on the row, not the detected band. The band (y 749..882) is the
        // part bright enough to clear the threshold, which is shorter than the tile itself - using
        // it directly cropped a horizontal slice through the middle of the button.
        const int Tile = 168;
        int cy = (749 + 882) / 2;
        int[] centres = { 148, 357 };            // inactive, active
        Console.WriteLine("   design 7: tiles {0}px square, centred at ({1},{3}) and ({2},{3})",
            Tile, centres[0], centres[1], cy);

        if (cy + Tile / 2 >= H || centres[1] + Tile / 2 >= W)
            throw new Exception("pinned rects fall outside the sheet - it has changed, re-measure");

        // Sheet order is INACTIVE, ACTIVE. Output: 0 resting, 1 hovered, 2 open.
        int[] sheetForState = { 0, 0, 1 };

        var outBmp = new Bitmap(cellW * 3, cellH, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
            for (int state = 0; state < 3; state++)
            {
                // Only a hair of inset here. Unlike the Town Portal tiles - whose ornate frames
                // were decoration to be discarded - design 7's rounded outline IS the button, so
                // cropping it away leaves a bare stack of bars with nothing to sit in.
                const int Inset = 6;
                int cx = centres[sheetForState[state]];
                var src = new Rectangle(cx - Tile / 2 + Inset, cy - Tile / 2 + Inset,
                    Tile - 2 * Inset, Tile - 2 * Inset);
                g.DrawImage(Sheet, new Rectangle(state * cellW, 0, cellW, cellH), src, GraphicsUnit.Pixel);
            }
        }
        BrightenColumns(outBmp, cellW, cellW * 2, 1.60);

        // Keep ONLY the three bars; drop the tile's frame and backing entirely.
        //
        // User request: the surrounding frame was too heavy. The belt cell already has its own
        // carved bevel in the plate art, so the button only needs to contribute the glyph and let
        // the plate be the frame - which also means the lit states light *the bars*, not a whole
        // plate, which is the read that was wanted.
        //
        // The bars are the brightest thing in the tile by a wide margin (the frame is dim enough
        // that it defeated band detection earlier, which is useful here); a luma cut isolates them
        // cleanly. Everything below it becomes fully transparent, and the engine's blit skips it.
        const double BarCut = 55.0;

        // Desaturate to match the panel's neutral stone. Resting and hover go fully neutral; the
        // open state keeps half its chroma so "the menu is showing" stays unmistakable - the same
        // neutral-inactive, warm-active language the inventory tabs already set.
        double[] keepChroma = { 0.0, 0.0, 0.50 };

        var data = outBmp.LockBits(new Rectangle(0, 0, outBmp.Width, outBmp.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var buf = new byte[outBmp.Width * outBmp.Height * 4];
        System.Runtime.InteropServices.Marshal.Copy(data.Scan0, buf, 0, buf.Length);
        int kept = 0;
        for (int y = 0; y < outBmp.Height; y++)
        {
            for (int x = 0; x < outBmp.Width; x++)
            {
                int i = (y * outBmp.Width + x) * 4;
                double luma = 0.299 * buf[i + 2] + 0.587 * buf[i + 1] + 0.114 * buf[i + 0];
                if (luma < BarCut)
                {
                    buf[i] = buf[i + 1] = buf[i + 2] = buf[i + 3] = 0;
                    continue;
                }
                buf[i + 3] = 255;
                kept++;

                int state = Math.Min(x / cellW, keepChroma.Length - 1);
                double amount = 1.0 - keepChroma[state];
                if (amount > 0)
                {
                    double b = buf[i], g = buf[i + 1], r = buf[i + 2];
                    double l = 0.299 * r + 0.587 * g + 0.114 * b;
                    buf[i] = (byte)Math.Round(b + (l - b) * amount);
                    buf[i + 1] = (byte)Math.Round(g + (l - g) * amount);
                    buf[i + 2] = (byte)Math.Round(r + (l - r) * amount);
                }
            }
        }
        // The luma cut alone leaves the tile's rounded frame highlight - its top-left arc is
        // bright enough to survive. Drop it by keeping only rows that actually contain a bar: a
        // bar spans most of the icon's width, the arc never does. Done per state, since the three
        // differ in brightness and therefore in exactly which pixels cleared the cut.
        // Which rows are bar rows is decided ONCE, from the open state, and applied to all three.
        //
        // The bars sit in identical positions in every state, so there is nothing to gain from
        // deciding per state - and plenty to lose. Doing it per state failed on resting: its bars
        // are dim enough that few of their rows clear the width test, so the frame's thin top edge
        // ranked into the top three and survived, while hover and open came out clean. Measuring
        // on the brightest state and reusing the answer removes that whole class of inconsistency.
        const int BrightestState = 2;
        {
            int x0 = BrightestState * cellW, x1 = x0 + cellW;

            // A bar row is lit across most of the icon's width.
            var wide = new bool[outBmp.Height];
            for (int y = 0; y < outBmp.Height; y++)
            {
                int n = 0;
                for (int x = x0; x < x1; x++) if (buf[(y * outBmp.Width + x) * 4 + 3] != 0) n++;
                wide[y] = n >= cellW * 45 / 100;
            }

            // The frame's top edge is full width too, so width alone cannot tell it from a bar.
            // Thickness can: a bar is several rows deep, that edge one or two. Group the rows into
            // runs and keep the three thickest - there are exactly three bars, which makes this
            // exact rather than a guess.
            var runs = new List<int[]>();
            int start = -1;
            for (int y = 0; y <= outBmp.Height; y++)
            {
                bool on = y < outBmp.Height && wide[y];
                if (on && start < 0) start = y;
                if (!on && start >= 0) { runs.Add(new[] { start, y - 1 }); start = -1; }
            }
            runs.Sort((a, b) => (b[1] - b[0]).CompareTo(a[1] - a[0]));

            var keepRow = new bool[outBmp.Height];
            foreach (var r in runs.GetRange(0, Math.Min(3, runs.Count)))
                for (int y = r[0]; y <= r[1]; y++) keepRow[y] = true;

            for (int y = 0; y < outBmp.Height; y++)
            {
                if (keepRow[y]) continue;
                for (int x = 0; x < outBmp.Width; x++)   // every state, same rows
                {
                    int i = (y * outBmp.Width + x) * 4;
                    buf[i] = buf[i + 1] = buf[i + 2] = buf[i + 3] = 0;
                }
            }

            // Trim a sliver off each state's left and right edge. The bars stop short of the tile
            // border, so anything surviving out there is the frame's vertical edge caught inside a
            // kept row - a couple of stray specks rather than anything structural.
            const int EdgeTrim = 2;
            for (int state = 0; state < 3; state++)
            {
                for (int y = 0; y < outBmp.Height; y++)
                {
                    for (int k = 0; k < EdgeTrim; k++)
                    {
                        foreach (int x in new[] { state * cellW + k, state * cellW + cellW - 1 - k })
                        {
                            int i = (y * outBmp.Width + x) * 4;
                            buf[i] = buf[i + 1] = buf[i + 2] = buf[i + 3] = 0;
                        }
                    }
                }
            }
        }

        int barPixels = 0;
        for (int i = 3; i < buf.Length; i += 4) if (buf[i] != 0) barPixels++;

        System.Runtime.InteropServices.Marshal.Copy(buf, 0, data.Scan0, buf.Length);
        outBmp.UnlockBits(data);
        Console.WriteLine("   bars isolated at luma > {0}: {1} kept after cut, {2} after row filter",
            BarCut, kept, barPixels);

        outBmp.Save(outPath, ImageFormat.Png);
        Console.WriteLine("   wrote {0} ({1}x{2}, design {3}, states resting/hover/open, desaturated)",
            Path.GetFileName(outPath), cellW * 3, cellH, design + 1);
        outBmp.Dispose();
    }

    // ---------------------------------------------------------------- SORT button

    /// <summary>
    /// Cuts the SORT button from a two-state sheet (INACTIVE, CLICKED) into the three states the
    /// game draws. The sheet has no hover art, so hover is synthesised by brightening the resting
    /// state - the same approach the menu icons needed once their states stopped differing by hue.
    /// </summary>
    static void CutSort(string sheetPath, string outPath, int size)
    {
        Load(sheetPath);
        const double Cut = 45.0;

        var rows = RowBands(Cut, 20);
        Console.WriteLine("SORT sheet: {0} row bands", rows.Count);
        // The tallest band is the button row; the others are the sheet's title and captions.
        rows.Sort((a, b) => (b[1] - b[0]).CompareTo(a[1] - a[0]));
        var row = rows[0];
        var cols = ColBands(row[0], row[1], Cut, 20);
        Console.WriteLine("   button row y {0}..{1}, {2} buttons", row[0], row[1], cols.Count);
        if (cols.Count < 2) throw new Exception("expected 2 SORT states, got " + cols.Count);

        var outBmp = new Bitmap(size * 3, size, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
            // 0 resting and 1 hover both come from INACTIVE; 2 pressed from CLICKED.
            int[] fromCol = { 0, 0, 1 };
            for (int s = 0; s < 3; s++)
            {
                var c = cols[fromCol[s]];
                var src = new Rectangle(c[0], row[0], c[1] - c[0] + 1, row[1] - row[0] + 1);
                g.DrawImage(Sheet, new Rectangle(s * size, 0, size, size), src, GraphicsUnit.Pixel);
            }
        }
        BrightenColumns(outBmp, size, size * 2, 1.35);

        outBmp.Save(outPath, ImageFormat.Png);
        Console.WriteLine("   wrote {0} ({1}x{2}, hover synthesised from resting)", Path.GetFileName(outPath), size * 3, size);
        outBmp.Dispose();
    }

    /// <summary>
    /// Cuts the level-up indicator's three states.
    ///
    /// Unlike the belt buttons this one is NOT drawn into a slot - it floats under the game clock
    /// on open screen, so the whole icon is kept, glow and all, and its background is keyed out
    /// rather than left opaque. The sheet's own order already matches the game's: a steady lit
    /// resting state, a blazing hovered one, and a dimmed pressed one.
    /// </summary>
    static void CutLevelUp(string sheetPath, string outPath, int cellW, int cellH)
    {
        Load(sheetPath);
        // 60, not 30. These icons carry a heavy glow that fades smoothly into the black backdrop,
        // and a low cut treats the halo as part of the subject: the row sweep swallowed almost the
        // whole sheet (88..810) and the column sweep found seven "icons" instead of three.
        // Profiling column runs at 60, 90 and 120 gives a clean three at all of them; 60 keeps the
        // most glow while still separating the icons.
        const double Cut = 60.0;

        var rows = RowBands(Cut, 40);
        Console.WriteLine("Level-up sheet: {0} row bands", rows.Count);
        if (rows.Count == 0) throw new Exception("no icon row found");
        rows.Sort((a, b) => (b[1] - b[0]).CompareTo(a[1] - a[0]));
        var row = rows[0];

        var cols = ColBands(row[0], row[1], Cut, 30);
        Console.WriteLine("   row y {0}..{1}, {2} icons", row[0], row[1], cols.Count);
        if (cols.Count != 3) throw new Exception("expected 3 states, got " + cols.Count);

        // One source box size for all three states, from the widest detected band. The blazing
        // hover state's glow spreads further than the others, so sizing each state to its own band
        // would render them at different scales and make the icon jump between frames.
        int side = 0;
        foreach (var c in cols) side = Math.Max(side, c[1] - c[0] + 1);
        side = Math.Max(side, row[1] - row[0] + 1);

        var outBmp = new Bitmap(cellW * 3, cellH, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
            for (int s = 0; s < 3; s++)
            {
                var c = cols[s];
                int cx = (c[0] + c[1]) / 2, cy = (row[0] + row[1]) / 2;
                var src = new Rectangle(cx - side / 2, cy - side / 2, side, side);

                // Fit by aspect into the cell rather than stretching to it. The cell is portrait
                // (40x60) while the emblem is square, so stretching would visibly distort it.
                double scale = Math.Min((double)cellW / side, (double)cellH / side);
                int w = Math.Max(1, (int)Math.Round(side * scale));
                int h = Math.Max(1, (int)Math.Round(side * scale));
                var dst = new Rectangle(s * cellW + (cellW - w) / 2, (cellH - h) / 2, w, h);
                g.DrawImage(Sheet, dst, src, GraphicsUnit.Pixel);
            }
        }

        // Key the black backdrop to transparency. Binary at runtime, so hard-threshold it.
        var data = outBmp.LockBits(new Rectangle(0, 0, outBmp.Width, outBmp.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var buf = new byte[outBmp.Width * outBmp.Height * 4];
        System.Runtime.InteropServices.Marshal.Copy(data.Scan0, buf, 0, buf.Length);
        for (int i = 0; i < buf.Length; i += 4)
        {
            double luma = 0.299 * buf[i + 2] + 0.587 * buf[i + 1] + 0.114 * buf[i + 0];
            if (luma < 26) { buf[i] = buf[i + 1] = buf[i + 2] = buf[i + 3] = 0; }
            else buf[i + 3] = 255;
        }
        System.Runtime.InteropServices.Marshal.Copy(buf, 0, data.Scan0, buf.Length);
        outBmp.UnlockBits(data);

        outBmp.Save(outPath, ImageFormat.Png);
        Console.WriteLine("   wrote {0} ({1}x{2}, cell {3}x{4}, states resting/hover/pressed)", Path.GetFileName(outPath), cellW * 3, cellH, cellW, cellH);
        outBmp.Dispose();
    }

    static void Main(string[] args)
    {
        string srcRoot = args[0], outDir = args[1];

        CutLevelUp(Path.Combine(srcRoot, "hud-icons", "level-up-icon-3-states.png"),
            Path.Combine(outDir, "level_up_icon.png"), 40, 60);

        // Design 1 (index 0): a plain square stone tile, the closest match to the belt cell's own
        // carved square. Only its interior is used, so the frame choice matters little - but a
        // square tile crops to a square interior without wasting pixels the way a round one does.
        CutTownPortal(Path.Combine(srcRoot, "bottom-hud", "town-portal-icons-9-designs-3-states.png"),
            Path.Combine(outDir, "town_portal_icon.png"), 0, 27, 29);

        // Design 7 (index 6), chosen by the project owner.
        CutBurgerMenu(Path.Combine(srcRoot, "bottom-hud", "burger-menu-button-9-designs-2-states.png"),
            Path.Combine(outDir, "burger_menu_button.png"), 6, 27, 29);

        CutSort(Path.Combine(srcRoot, "inventory-panel", "sort-button-2-states.png"),
            Path.Combine(outDir, "inventory_sort.png"), 28);
    }
}
