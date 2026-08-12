using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

// Composes the Oracool V1 inventory panel from the artist's component files.
//
// Output is a flat 320x660 opaque PNG. The frames' "alpha" channel is a flattened-export
// artifact (no fully-transparent pixels anywhere, values 0x85-0xFC), so it is used only for
// edge softness, never as a cutout. The real cutout work is:
//   - the silhouette, keyed off its light stone backdrop by luminance
//   - the slot frames, whose interiors are masked out so the silhouette reads through them
//
// Geometry here must stay in step with Source/oracool/inventory_layout.h.

class Surface
{
    public int W, H;
    public byte[] B; // BGRA, 4 bytes per pixel

    public Surface(int w, int h) { W = w; H = h; B = new byte[w * h * 4]; }

    public static Surface Load(string path)
    {
        using (var src = new Bitmap(path))
        {
            var s = new Surface(src.Width, src.Height);
            using (var clone = new Bitmap(src.Width, src.Height, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(clone))
                {
                    g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
                    g.DrawImage(src, 0, 0, src.Width, src.Height);
                }
                var d = clone.LockBits(new Rectangle(0, 0, s.W, s.H), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                Marshal.Copy(d.Scan0, s.B, 0, s.B.Length);
                clone.UnlockBits(d);
            }
            return s;
        }
    }

    public Surface Scaled(int w, int h)
    {
        var outS = new Surface(w, h);
        using (var tmp = new Bitmap(W, H, PixelFormat.Format32bppArgb))
        {
            var d = tmp.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(B, 0, d.Scan0, B.Length);
            tmp.UnlockBits(d);
            using (var dst = new Bitmap(w, h, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(dst))
                {
                    g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
                    g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                    g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
                    g.DrawImage(tmp, new Rectangle(0, 0, w, h));
                }
                var d2 = dst.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                Marshal.Copy(d2.Scan0, outS.B, 0, outS.B.Length);
                dst.UnlockBits(d2);
            }
        }
        return outS;
    }

    public void Save(string path)
    {
        using (var bmp = new Bitmap(W, H, PixelFormat.Format32bppArgb))
        {
            var d = bmp.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(B, 0, d.Scan0, B.Length);
            bmp.UnlockBits(d);
            bmp.Save(path, ImageFormat.Png);
        }
    }

    public int Idx(int x, int y) { return (y * W + x) * 4; }
    public bool In(int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; }

    /// <summary>Alpha-blends src over this surface at (dx,dy), scaling src alpha by opacity.</summary>
    public void Blend(Surface src, int dx, int dy, double opacity, byte[] mask = null)
    {
        for (int y = 0; y < src.H; y++)
        {
            int ty = dy + y;
            if (ty < 0 || ty >= H) continue;
            for (int x = 0; x < src.W; x++)
            {
                int tx = dx + x;
                if (tx < 0 || tx >= W) continue;
                int si = src.Idx(x, y);
                double a = src.B[si + 3] / 255.0 * opacity;
                if (mask != null) a *= mask[y * src.W + x] / 255.0;
                if (a <= 0.0) continue;
                int di = Idx(tx, ty);
                for (int c = 0; c < 3; c++)
                    B[di + c] = (byte)Math.Round(src.B[si + c] * a + B[di + c] * (1.0 - a));
                B[di + 3] = 255;
            }
        }
    }
}

class InvCompose
{
    // --- must mirror Source/oracool/inventory_layout.h ---
    const int PanelW = 320, PanelH = 660;
    const int Cell = 28;
    const int GridCols = 10, GridRows = 7;
    const int GridY = 400;
    const int GridRowsCount = 7;
    const int GridBottom = GridY + GridRowsCount * Cell;
    // Inner edge of the panel's carved bottom border; the frame itself runs 651-659.
    const int PanelInnerBottom = 650;
    // Tab row sits directly on top of the grid's first row.
    const int TabRowY = GridY - Cell;
    const int GridX = (PanelW - GridCols * Cell) / 2;
    const int EquipColLeft = 34, EquipColCentre = (PanelW - 2 * Cell) / 2, EquipColRight = 230;

    // Shoulders and amulet flank the helm at half the armor-to-side-column gap, so they sit
    // inset from the gloves/bracers column below them.
    const int ArmorToSideGap = EquipColCentre - (EquipColLeft + 2 * Cell);
    const int HeadFlankGap = ArmorToSideGap / 2;
    const int ShouldersX = EquipColCentre - HeadFlankGap - 2 * Cell;
    const int AmuletX = EquipColCentre + 2 * Cell + HeadFlankGap;

    // Rings share the belt's row; weapon/shield sit the same distance below the rings as the
    // gloves sit above them.
    const int BeltAndRingRowY = 182;
    const int GlovesRowBottom = 104 + 2 * Cell;
    const int RingRowGap = BeltAndRingRowY - GlovesRowBottom;
    const int WeaponRowY = BeltAndRingRowY + Cell + RingRowGap;

    struct Slot
    {
        public string Name; public int X, Y, CW, CH;
        public Slot(string n, int x, int y, int cw, int ch) { Name = n; X = x; Y = y; CW = cw; CH = ch; }
    }

    static readonly Slot[] Slots = {
        new Slot("Helm",      EquipColCentre,  18, 2, 2),
        new Slot("Shoulders", ShouldersX,      36, 2, 2),
        new Slot("Amulet",    AmuletX,         48, 1, 1),
        new Slot("Chest",     EquipColCentre,  88, 2, 3),
        new Slot("Gloves",    EquipColLeft,   104, 2, 2),
        new Slot("Bracers",   EquipColRight,  104, 2, 2),
        new Slot("RingLeft",  EquipColLeft + Cell / 2,  BeltAndRingRowY, 1, 1),
        new Slot("RingRight", EquipColRight + Cell / 2, BeltAndRingRowY, 1, 1),
        new Slot("Belt",      EquipColCentre, BeltAndRingRowY, 2, 1),
        new Slot("Weapon",    EquipColLeft,   WeaponRowY, 2, 3),
        new Slot("Shield",    EquipColRight,  WeaponRowY, 2, 3),
        new Slot("Legs",      EquipColCentre, 220, 2, 2),
        new Slot("Boots",     EquipColCentre, 284, 2, 2),
    };

    /// <summary>
    /// Uniform darkening applied to the finished panel.
    ///
    /// Measured against the original panel art: with the stash and the new inventory both on
    /// screen in one screenshot, under the same palette, the old panel's empty interior has a
    /// median luma of 43. This composition's own median, before darkening, is 74. Hence 0.58.
    ///
    /// The first attempt used 0.43, from comparing the two panels *as they appeared in that
    /// screenshot* - but the inventory was full of items and the stash was empty, so item sprites
    /// and quality-highlight blocks inflated the inventory's median to 100. That over-darkened the
    /// panel to 32 and collapsed it from 41 palette colours to 18, which bands the stone. Measure
    /// the source art, which has no items in it, not a screenshot of it in use.
    ///
    /// Applied last, over the whole composition, so the background, silhouette, slot frames and
    /// sygil keep their relative relationships and only the overall level moves.
    /// </summary>
    const double PanelDarken = 0.58;

    static string dir;
    static Surface Frame(int cw, int ch)
    {
        string name = string.Format("slot-frame-{0}x{1}.png", cw, ch);
        if (cw == 1 && ch == 1) name = "slot-frame-1x1.png";
        return Surface.Load(Path.Combine(dir, name));
    }

    /// <summary>
    /// Mask that keeps a frame's border ring and drops its interior, so whatever is behind
    /// (background, silhouette) shows through the slot. The interior is found from the
    /// frame's own dimensions rather than by colour: outer size minus the cells it wraps.
    /// Edges are feathered by one pixel so the ring does not terminate in a hard step.
    /// </summary>
    static byte[] BorderMask(Surface frame, int cw, int ch)
    {
        int bx = (frame.W - cw * Cell) / 2;
        int by = (frame.H - ch * Cell) / 2;
        var m = new byte[frame.W * frame.H];
        for (int y = 0; y < frame.H; y++)
        {
            for (int x = 0; x < frame.W; x++)
            {
                // Distance into the interior region; <=0 means we are on the border ring.
                int d = Math.Min(Math.Min(x - bx, y - by),
                                 Math.Min(frame.W - 1 - bx - x, frame.H - 1 - by - y));
                byte v;
                if (d < 0) v = 255;
                else if (d == 0) v = 128; // one-pixel feather along the inner edge
                else v = 0;
                m[y * frame.W + x] = v;
            }
        }
        return m;
    }

    /// <summary>
    /// Keys the paladin out of his light stone backdrop. The figure is dark (luma ~36) on a
    /// light plate (luma ~192), so alpha rises as luma falls. Returns a soft matte, which
    /// suits a watermark: hard-keying it would leave a jagged edge against the panel stone.
    /// </summary>
    static byte[] SilhouetteMask(Surface s)
    {
        const double BackdropLuma = 185.0, FigureLuma = 45.0;
        var m = new byte[s.W * s.H];
        for (int y = 0; y < s.H; y++)
        {
            for (int x = 0; x < s.W; x++)
            {
                int i = s.Idx(x, y);
                double luma = 0.299 * s.B[i + 2] + 0.587 * s.B[i + 1] + 0.114 * s.B[i + 0];
                double t = (BackdropLuma - luma) / (BackdropLuma - FigureLuma);
                t = Math.Max(0.0, Math.Min(1.0, t));
                m[y * s.W + x] = (byte)Math.Round(t * 255.0);
            }
        }
        return m;
    }

    /// <summary>Scales every colour channel toward black, leaving alpha alone.</summary>
    static void Darken(Surface s, double factor)
    {
        for (int i = 0; i < s.B.Length; i += 4)
        {
            for (int c = 0; c < 3; c++)
                s.B[i + c] = (byte)Math.Max(0, Math.Min(255, Math.Round(s.B[i + c] * factor)));
        }
    }

    /// <summary>
    /// User request: the 70 backpack grid cells read as flat, uniform panels next to DevilutionX's
    /// own shared-stash grid, whose cells carry a mottled, blotchy stone grain. Reusing the stash's
    /// own art directly was ruled out (it is DevilutionX's own baked, non-Oracool asset - the same
    /// reasoning that kept town.pal out of a shipped commit earlier in this project); this generates
    /// an equivalent-effect grain from scratch instead, procedurally, so nothing is copied.
    ///
    /// Blotchy rather than per-pixel static on purpose - zooming the stash's own texture showed
    /// chunky multi-pixel patches, not fine noise, so a single random value is drawn per BlockPx
    /// block and nearest-neighbour filled, which reads as coarse stone grain instead of TV static.
    /// Skewed dark (mostly 0 to -NoiseRange, occasionally up to +NoiseHighlight) because the
    /// reference reads as scattered darker patches over the base tone, not scattered highlights.
    ///
    /// Confined to each cell's interior (the frame border itself, held in the caller's mask, is
    /// left untouched) and seeded, so the output is deterministic across rebuilds.
    /// </summary>
    static void ApplyGridGrain(Surface panel, int seed)
    {
        const int BlockPx = 3;
        const int NoiseRange = 48;
        const int NoiseHighlight = 20;
        const int InteriorMargin = 3; // stays clear of the frame ring the grid tiling already drew

        var rng = new Random(seed);
        for (int r = 0; r < GridRows; r++)
        {
            for (int c = 0; c < GridCols; c++)
            {
                int cellX = GridX + c * Cell, cellY = GridY + r * Cell;
                for (int by = InteriorMargin; by < Cell - InteriorMargin; by += BlockPx)
                {
                    for (int bx = InteriorMargin; bx < Cell - InteriorMargin; bx += BlockPx)
                    {
                        int delta = rng.Next(-NoiseRange, NoiseHighlight + 1);
                        int bw = Math.Min(BlockPx, Cell - InteriorMargin - bx);
                        int bh = Math.Min(BlockPx, Cell - InteriorMargin - by);
                        for (int y = 0; y < bh; y++)
                        {
                            int py = cellY + by + y;
                            if (!panel.In(0, py)) continue;
                            for (int x = 0; x < bw; x++)
                            {
                                int px = cellX + bx + x;
                                if (!panel.In(px, 0)) continue;
                                int i = panel.Idx(px, py);
                                for (int ch = 0; ch < 3; ch++)
                                    panel.B[i + ch] = (byte)Math.Max(0, Math.Min(255, panel.B[i + ch] + delta));
                            }
                        }
                    }
                }
            }
        }
    }

    static void Main(string[] args)
    {
        dir = args[0];
        string outPath = args[1];

        var panel = Surface.Load(Path.Combine(dir, "panel-background-320x660.png"));
        if (panel.W != PanelW || panel.H != PanelH)
            throw new Exception(string.Format("background is {0}x{1}, expected {2}x{3}", panel.W, panel.H, PanelW, PanelH));
        for (int i = 3; i < panel.B.Length; i += 4) panel.B[i] = 255;

        // Silhouette: spans the whole area above the tab row, not just helm-to-boots. Running
        // it down to the tabs fills what would otherwise be a dead band of bare stone between
        // the boots slot (ends 340) and the tab row (starts 368), and matches the concept,
        // where the figure's feet sit below the boots slot rather than inside it.
        const int SilTop = 14, SilGapAboveTabs = 2;
        var sil = Surface.Load(Path.Combine(dir, "paladin-silhouette.png"));
        int silH = TabRowY - SilGapAboveTabs - SilTop;
        int silW = (int)Math.Round(sil.W * (double)silH / sil.H);
        var silScaled = sil.Scaled(silW, silH);
        var silMask = SilhouetteMask(silScaled);
        panel.Blend(silScaled, (PanelW - silW) / 2, SilTop, 0.72, silMask);
        Console.WriteLine("silhouette {0}x{1} at y {2}..{3}", silW, silH, SilTop, SilTop + silH);

        // Equipment slot frames, border ring only.
        foreach (var s in Slots)
        {
            var f = Frame(s.CW, s.CH);
            var mask = BorderMask(f, s.CW, s.CH);
            // Centre the frame on the cell rect so the border straddles it evenly, whatever
            // the frame's own border thickness happens to be.
            int cx = s.X + s.CW * Cell / 2, cy = s.Y + s.CH * Cell / 2;
            panel.Blend(f, cx - f.W / 2, cy - f.H / 2, 1.0, mask);
            Console.WriteLine("slot {0,-10} cell({1},{2}) {3}x{4}  frame {5}x{6}", s.Name, s.X, s.Y, s.CW * Cell, s.CH * Cell, f.W, f.H);
        }

        // Item grid: the 1x1 frame tiled at cell pitch. Neighbouring borders overlap, which
        // is what gives the shared-gridline look rather than 70 separate boxes.
        var cellFrame = Frame(1, 1);
        var cellMask = BorderMask(cellFrame, 1, 1);
        for (int r = 0; r < GridRows; r++)
        {
            for (int c = 0; c < GridCols; c++)
            {
                int cx = GridX + c * Cell + Cell / 2;
                int cy = GridY + r * Cell + Cell / 2;
                panel.Blend(cellFrame, cx - cellFrame.W / 2, cy - cellFrame.H / 2, 1.0, cellMask);
            }
        }
        Console.WriteLine("grid {0}x{1} cells at ({2},{3}), bottom {4}", GridCols, GridRows, GridX, GridY, GridY + GridRows * Cell);

        // User request: give the grid cells the same mottled stone-grain feel as DevilutionX's own
        // shared-stash grid, without reusing that grid's own art - see ApplyGridGrain's doc comment.
        const int GridGrainSeed = 20260812;
        ApplyGridGrain(panel, GridGrainSeed);
        Console.WriteLine("grid grain applied (seed {0})", GridGrainSeed);

        // Class sygil, centred in the band between the grid and the mana orb. Baked in rather
        // than drawn at runtime because it never changes state - see inventory_layout.h for why
        // it sits in the top of the footer instead of the middle.
        if (args.Length > 2)
        {
            var sygil = Surface.Load(args[2]);
            int sx = (PanelW - sygil.W) / 2;
            int sy = GridBottom + (PanelInnerBottom - GridBottom - sygil.H) / 2;
            panel.Blend(sygil, sx, sy, 1.0);
            Console.WriteLine("sygil {0}x{1} at ({2},{3})", sygil.W, sygil.H, sx, sy);
        }

        Darken(panel, PanelDarken);
        panel.Save(outPath);
        Console.WriteLine("wrote {0}  (darkened x{1:0.00})", outPath, PanelDarken);
    }
}
