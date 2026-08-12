// Oracool asset pipeline: cuts inventory icons for the six new worn item types out of the user's
// art sheets and packs them into a single .CEL.
//
// Second CEL encoder in the project after tools/WaypointCel.cs, and it shares that file's two hard
// format facts - scanlines are stored bottom-up, and only palette indices 128-255 are identical
// across town and all four dungeon tilesets. Item icons are drawn on the inventory panel rather
// than in the world, so the palette restriction matters less there than it did for the waypoint,
// but the icons also appear on the ground-item cursor, so it still applies.
//
// Unlike the waypoint, the frames here are NOT all the same size: the belt is 2x1 (56x28) and the
// other five are 2x2 (56x56). A CEL stores no widths at all - the engine supplies them per frame
// from a table - so the frame order here must match InvItemWidth3/InvItemHeight3 in cursor.cpp and
// the ICURS_ORACOOL_* values in itemdat.h.
//
// Usage: ItemIconCel.exe <palette.pal> <out.cel> <previewDir> <spec> [<spec> ...]
//   spec = <sheet.png>,<srcX>,<srcY>,<srcW>,<srcH>,<cellW>,<cellH>,<name>[,<backdropCut>[,<fillPunctures>[,<mode>]]]
//
// backdropCut overrides BackdropLumaCut for that one spec (default: the constant below). Added
// for single AI-rendered "product shot" icons (a soft vignette glow around the subject, not the
// hand-painted sheets' flat near-black canvas) - the vignette can sit well above the sheets' 30
// cut, and a fill that stops partway up the glow ramp leaves a hazy ring baked into the icon. Safe
// to raise per-image because the flood fill is connectivity-, not brightness-, gated: it can only
// reach as far as a physically unbroken dark path from the canvas edge, so a higher cut still
// can't breach into a subject's own shadowed interior as long as brighter material - metal, cloth,
// whatever encloses it - forms an unbroken firewall around it.
//
// mode selects the background-removal method: "dark" (default) is the flood fill above, built for
// the hand-painted sheets' and early product shots' near-black canvas. "green" is a flat chroma
// key for flat-green-background renders - user request, after the "dark" path's fundamental
// ambiguity (a dark backdrop and a dark item interior look identical by brightness alone, only
// solved there by connectivity) kept causing trouble: torn edges, then enclosed punctures, each
// its own investigation. Green vs. brown/bronze/iron needs no such analysis - see
// ExtractWithGreenKey. backdropCut is meaningless in "green" mode but the field still has to be
// present for positional parsing; pass the default (30) as a placeholder.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

internal static class ItemIconCel
{
	// The art sheets paint on near-black rather than transparency, so content is found by
	// brightness. 40 keeps the dark leather of a plain glove and drops the backdrop's noise.
	private const int BboxLumaCut = 40;
	// Backdrop flood-fill threshold. Bug postmortem (user report: "new items seemed transparent"):
	// the first cut dropped every pixel below a brightness of 55, but dark leather IS below 55, so
	// the icons came out riddled with holes. Brightness cannot separate a dark item from a dark
	// backdrop - connectivity can: the backdrop is one contiguous dark region touching the crop's
	// edges, while an item's dark interior is enclosed by its brighter silhouette. So backdrop
	// removal is a flood fill inward from the edges through pixels below this cut, and interior
	// pixels survive no matter how dark they are.
	//
	// 30 comes from measurement, not taste: the sheets' canvas is luma 3-17 and item interiors
	// start around 70. The first fill used 48 and leaked through shadowed silhouette edges into
	// the items themselves.
	private const int BackdropLumaCut = 30;

	// The canvas texture has bright specks above the fill cut; the fill correctly walls around
	// them and they survive as floating opaque dots. Anything smaller than this many source
	// pixels is noise - real secondary pieces (a pauldron's strap) run to hundreds.
	private const int MinIslandArea = 60;

	private static int Main(string[] args)
	{
		if (args.Length < 4) {
			Console.Error.WriteLine("Usage: ItemIconCel.exe <palette.pal> <out.cel> <previewDir> <spec> [<spec> ...]");
			Console.Error.WriteLine("  spec = <sheet.png>,<srcX>,<srcY>,<srcW>,<srcH>,<cellW>,<cellH>,<name>");
			return 2;
		}

		byte[] pal = File.ReadAllBytes(args[0]);
		if (pal.Length != 768) {
			Console.Error.WriteLine("Palette must be exactly 768 bytes, got " + pal.Length);
			return 1;
		}
		string outPath = args[1];
		string previewDir = args[2];

		List<byte[]> frames = new List<byte[]>();
		List<int> widths = new List<int>();
		List<int> heights = new List<int>();

		for (int i = 3; i < args.Length; i++) {
			string[] parts = args[i].Split(',');
			if (parts.Length < 8 || parts.Length > 11) {
				Console.Error.WriteLine("Bad spec: " + args[i]);
				return 1;
			}
			string sheetPath = parts[0];
			Rectangle srcBox = new Rectangle(int.Parse(parts[1]), int.Parse(parts[2]), int.Parse(parts[3]), int.Parse(parts[4]));
			int cellW = int.Parse(parts[5]);
			int cellH = int.Parse(parts[6]);
			string name = parts[7];
			int backdropCut = parts.Length >= 9 ? int.Parse(parts[8]) : BackdropLumaCut;
			// Opt-in, per spec, deliberately not on by default - see FillEnclosedPunctures's own
			// comment for why: it is only proven safe on the helm so far. The belt's own enclosed
			// gap was explicitly reviewed and confirmed intentional in an earlier pass ("daylight
			// through the middle of the loop, the way a real belt looks laid flat"); silently
			// re-deciding that via a general pipeline change, for six icons nobody asked to revisit,
			// is exactly the kind of side effect this flag exists to prevent.
			bool fillPunctures = parts.Length >= 10 && parts[9] == "true";
			bool greenKey = parts.Length == 11 && parts[10] == "green";

			using (Bitmap sheet = new Bitmap(sheetPath)) {
				Rectangle content = greenKey ? ContentBoxByGreenKey(sheet, srcBox) : ContentBox(sheet, srcBox);
				Console.WriteLine("{0}: search box {1} -> content {2}", name, srcBox, content);

				// Backdrop removed at full resolution, BEFORE scaling: real alpha goes onto the
				// crop, and the scale then blends it into soft edges for PostProcess to resolve.
				using (Bitmap crop = greenKey ? ExtractWithGreenKey(sheet, content) : ExtractWithAlpha(sheet, content, backdropCut))
				using (Bitmap cell = FitInto(crop, new Rectangle(0, 0, crop.Width, crop.Height), cellW, cellH)) {
					PostProcess(cell, fillPunctures);
					byte[] idx = Quantise(cell, pal, name);
					frames.Add(idx);
					widths.Add(cellW);
					heights.Add(cellH);
					if (previewDir != "-")
						WritePreview(idx, cellW, cellH, pal, Path.Combine(previewDir, "icon_" + name + ".png"));
				}
			}
		}

		byte[] cel = EncodeCel(frames, widths, heights);
		Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath)));
		File.WriteAllBytes(outPath, cel);
		Console.WriteLine("wrote {0} ({1} bytes, {2} frames)", outPath, cel.Length, frames.Count);
		return 0;
	}

	/** @brief Tight box of everything bright enough to be art inside `region`. */
	private static Rectangle ContentBox(Bitmap bmp, Rectangle region)
	{
		int minX = int.MaxValue, minY = int.MaxValue, maxX = -1, maxY = -1;
		BitmapData data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;
				for (int y = region.Top; y < Math.Min(region.Bottom, bmp.Height); y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = region.Left; x < Math.Min(region.Right, bmp.Width); x++) {
						int b = row[x * 4], g = row[x * 4 + 1], r = row[x * 4 + 2];
						if (Math.Max(r, Math.Max(g, b)) < BboxLumaCut)
							continue;
						if (x < minX) minX = x;
						if (x > maxX) maxX = x;
						if (y < minY) minY = y;
						if (y > maxY) maxY = y;
					}
				}
			}
		} finally {
			bmp.UnlockBits(data);
		}
		if (maxX < 0)
			throw new InvalidOperationException("search box contains no art");
		return new Rectangle(minX, minY, maxX - minX + 1, maxY - minY + 1);
	}

	// Excess of green over the stronger of red/blue. Measured directly against the actual green-
	// screen renders (corners and edge midpoints sampled across three separate images) rather than
	// assumed to be a clean #00FF00: the flat fill carries some low-level dither/noise, sitting
	// around excess 190-245 everywhere sampled. Leather/bronze/iron item material never gets
	// remotely close - warm browns and greys both have green as the weakest or a middling channel,
	// never the dominant one - so GreenKeyFullThreshold and GreenKeyNoneThreshold sit with wide,
	// measured margin on both sides, not just inside the gap between the two closest observed
	// values.
	private const int GreenKeyFullThreshold = 80; // excess at/above this: fully background
	private const int GreenKeyNoneThreshold = 20;  // excess at/below this: fully opaque, kept as-is

	private static bool IsGreenBackground(int r, int g, int b)
	{
		return (g - Math.Max(r, b)) >= GreenKeyFullThreshold;
	}

	/** @brief Tight box of everything NOT part of the flat green backdrop inside `region`. */
	private static Rectangle ContentBoxByGreenKey(Bitmap bmp, Rectangle region)
	{
		int minX = int.MaxValue, minY = int.MaxValue, maxX = -1, maxY = -1;
		BitmapData data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;
				for (int y = region.Top; y < Math.Min(region.Bottom, bmp.Height); y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = region.Left; x < Math.Min(region.Right, bmp.Width); x++) {
						int b = row[x * 4], g = row[x * 4 + 1], r = row[x * 4 + 2];
						if (IsGreenBackground(r, g, b))
							continue;
						if (x < minX) minX = x;
						if (x > maxX) maxX = x;
						if (y < minY) minY = y;
						if (y > maxY) maxY = y;
					}
				}
			}
		} finally {
			bmp.UnlockBits(data);
		}
		if (maxX < 0)
			throw new InvalidOperationException("search box contains no art");
		return new Rectangle(minX, minY, maxX - minX + 1, maxY - minY + 1);
	}

	/**
	 * @brief Copies `srcBox` out of the sheet with green-background pixels made transparent.
	 *
	 * User request: "only make transparent the GREEN pixels" - a plain per-pixel chroma key, not
	 * ExtractWithAlpha's luma flood fill. That flood fill exists purely to solve one problem: a
	 * dark backdrop and a dark item interior are indistinguishable by brightness alone, so only
	 * connectivity from the canvas edge can tell them apart - and that same fix is what left
	 * enclosed punctures behind once a downscale thinned away the connecting path. Green versus
	 * this item family's browns, bronzes and iron greys has no such ambiguity: nothing in any of
	 * these renders is remotely green, so a flat per-pixel test is both simpler and safer than
	 * flood-filling, with no risk of it misreading a shadowed fold as part of the backdrop.
	 *
	 * The threshold is a soft ramp, not a hard cutoff, specifically to avoid trading one edge
	 * artefact for another: a hard cutoff leaves a visible bright-green fringe on anti-aliased
	 * edge pixels (a blend of item colour and background green that isn't green ENOUGH to key out
	 * whole, but reads as a sickly halo once kept whole). The ramp instead hands those pixels to
	 * PostProcess as partial alpha, and its existing edge compositing blends them toward the dark
	 * panel tone exactly as it already does for the luma path - reused, not reinvented.
	 */
	private static Bitmap ExtractWithGreenKey(Bitmap sheet, Rectangle srcBox)
	{
		int w = srcBox.Width, h = srcBox.Height;
		Bitmap crop = new Bitmap(w, h, PixelFormat.Format32bppArgb);
		using (Graphics g = Graphics.FromImage(crop)) {
			g.CompositingMode = CompositingMode.SourceCopy;
			g.DrawImage(sheet, new Rectangle(0, 0, w, h), srcBox, GraphicsUnit.Pixel);
		}

		BitmapData data = crop.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;
				for (int y = 0; y < h; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = 0; x < w; x++) {
						byte* p = row + x * 4;
						int b = p[0], gCh = p[1], r = p[2];
						int excess = gCh - Math.Max(r, b);
						int alpha;
						if (excess >= GreenKeyFullThreshold)
							alpha = 0;
						else if (excess <= GreenKeyNoneThreshold)
							alpha = 255;
						else
							alpha = 255 - (excess - GreenKeyNoneThreshold) * 255 / (GreenKeyFullThreshold - GreenKeyNoneThreshold);
						p[3] = (byte)alpha;
					}
				}
			}
		} finally {
			crop.UnlockBits(data);
		}
		return crop;
	}

	/**
	 * @brief Copies `srcBox` out of the sheet with the backdrop made genuinely transparent.
	 *
	 * The backdrop is every below-BackdropLumaCut pixel reachable from the crop's edges without
	 * crossing a brighter one - a flood fill. An item's own dark pixels are enclosed by its
	 * brighter silhouette, so the fill never reaches them.
	 */
	private static Bitmap ExtractWithAlpha(Bitmap sheet, Rectangle srcBox, int backdropLumaCut)
	{
		int w = srcBox.Width, h = srcBox.Height;
		Bitmap crop = new Bitmap(w, h, PixelFormat.Format32bppArgb);
		using (Graphics g = Graphics.FromImage(crop)) {
			g.CompositingMode = CompositingMode.SourceCopy;
			g.DrawImage(sheet, new Rectangle(0, 0, w, h), srcBox, GraphicsUnit.Pixel);
		}

		bool[] isBackdrop = new bool[w * h];
		Queue<int> queue = new Queue<int>();

		BitmapData data = crop.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;
				Func<int, int, bool> isDark = (x, y) => {
					byte* p = basePtr + y * data.Stride + x * 4;
					return Math.Max(p[2], Math.Max(p[1], p[0])) < backdropLumaCut;
				};

				for (int x = 0; x < w; x++) {
					foreach (int y in new[] { 0, h - 1 }) {
						if (!isBackdrop[y * w + x] && isDark(x, y)) { isBackdrop[y * w + x] = true; queue.Enqueue(y * w + x); }
					}
				}
				for (int y = 0; y < h; y++) {
					foreach (int x in new[] { 0, w - 1 }) {
						if (!isBackdrop[y * w + x] && isDark(x, y)) { isBackdrop[y * w + x] = true; queue.Enqueue(y * w + x); }
					}
				}

				int[] dx = { 1, -1, 0, 0 };
				int[] dy = { 0, 0, 1, -1 };
				while (queue.Count > 0) {
					int cell = queue.Dequeue();
					int cx = cell % w, cy = cell / w;
					for (int d = 0; d < 4; d++) {
						int nx = cx + dx[d], ny = cy + dy[d];
						if (nx < 0 || nx >= w || ny < 0 || ny >= h)
							continue;
						int n = ny * w + nx;
						if (isBackdrop[n] || !isDark(nx, ny))
							continue;
						isBackdrop[n] = true;
						queue.Enqueue(n);
					}
				}

				// Island filter: opaque components below MinIslandArea are canvas noise, not item.
				int[] label = new int[w * h];
				int nextLabel = 0;
				List<int> areas = new List<int>();
				List<List<int>> members = new List<List<int>>();
				for (int start = 0; start < w * h; start++) {
					if (isBackdrop[start] || label[start] != 0)
						continue;
					nextLabel++;
					List<int> cells = new List<int>();
					Queue<int> bfs = new Queue<int>();
					label[start] = nextLabel;
					bfs.Enqueue(start);
					while (bfs.Count > 0) {
						int cell = bfs.Dequeue();
						cells.Add(cell);
						int cx = cell % w, cy = cell / w;
						for (int d = 0; d < 4; d++) {
							int nx = cx + dx[d], ny = cy + dy[d];
							if (nx < 0 || nx >= w || ny < 0 || ny >= h)
								continue;
							int n = ny * w + nx;
							if (isBackdrop[n] || label[n] != 0)
								continue;
							label[n] = nextLabel;
							bfs.Enqueue(n);
						}
					}
					areas.Add(cells.Count);
					members.Add(cells);
				}
				for (int c = 0; c < members.Count; c++) {
					if (areas[c] >= MinIslandArea)
						continue;
					foreach (int cell in members[c])
						isBackdrop[cell] = true;
				}

				// Quality postmortem (user report, viewed against a WHITE background where the
				// dark game panel had been hiding it): the flood fill was leaking through hairline
				// shadow creases IN the leather itself - the seams between a wrist-wrap's straps,
				// the sliver connecting a dangling buckle-strap to the hand - and treating them as
				// open background. Two symptoms from one cause: interior pixels punched through
				// solid material, and small decorative bits (a strap end, a stud) severed into
				// free-floating islands too large for the debris filter below to catch.
				//
				// Fix: erode the backdrop set by one layer. A true backdrop pixel is deep inside a
				// wide dark region and has nearly all 8 neighbours also marked backdrop; a leaked
				// pixel sits in a seam only 1-2px wide and has few. Reclaiming low-neighbour-count
				// backdrop pixels closes the leaks without touching the real background, which is
				// wide enough everywhere that this costs it only its outermost fringe - exactly
				// what the edge-compositing in PostProcess exists to blend cleanly anyway.
				const int MinBackdropNeighbors = 5;
				bool[] erodedBackdrop = (bool[])isBackdrop.Clone();
				for (int y = 0; y < h; y++) {
					for (int x = 0; x < w; x++) {
						int i = y * w + x;
						if (!isBackdrop[i])
							continue;
						int neighbors = 0;
						for (int oy = -1; oy <= 1; oy++) {
							for (int ox = -1; ox <= 1; ox++) {
								if (ox == 0 && oy == 0)
									continue;
								int nx = x + ox, ny = y + oy;
								if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
									neighbors++; // treat off-canvas as backdrop, not a leak signal
									continue;
								}
								if (isBackdrop[ny * w + nx])
									neighbors++;
							}
						}
						if (neighbors < MinBackdropNeighbors)
							erodedBackdrop[i] = false;
					}
				}
				isBackdrop = erodedBackdrop;

				for (int y = 0; y < h; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = 0; x < w; x++) {
						if (isBackdrop[y * w + x])
							row[x * 4 + 3] = 0;
					}
				}
			}
		} finally {
			crop.UnlockBits(data);
		}
		return crop;
	}

	/**
	 * @brief Scales `srcBox` to fit inside cellW x cellH with its aspect preserved, centred.
	 *
	 * "Contain", not "stretch": a glove is roughly square and a belt is roughly 3:1, so stretching
	 * either into its cell would distort it badly. The letterboxing is transparent, which costs
	 * nothing - CEL skips transparent runs.
	 */
	private static Bitmap FitInto(Bitmap source, Rectangle srcBox, int cellW, int cellH)
	{
		double scale = Math.Min(cellW / (double)srcBox.Width, cellH / (double)srcBox.Height);
		int drawW = Math.Max(1, (int)Math.Round(srcBox.Width * scale));
		int drawH = Math.Max(1, (int)Math.Round(srcBox.Height * scale));

		Bitmap dst = new Bitmap(cellW, cellH, PixelFormat.Format32bppArgb);
		using (Graphics g = Graphics.FromImage(dst)) {
			g.CompositingMode = CompositingMode.SourceCopy;
			g.InterpolationMode = InterpolationMode.HighQualityBicubic;
			g.PixelOffsetMode = PixelOffsetMode.HighQuality;
			g.Clear(Color.FromArgb(0, 0, 0, 0));
			g.DrawImage(source, new Rectangle((cellW - drawW) / 2, (cellH - drawH) / 2, drawW, drawH), srcBox, GraphicsUnit.Pixel);
		}
		return dst;
	}

	/**
	 * @brief Resolves the downscaled cell into hard-edged, quantiser-ready pixels.
	 *
	 * Quality postmortem (user report: the icons "look a bit like parts of them are gone due to bad
	 * background removal"). The background removal was actually fine by this point - the damage
	 * came from what happened to its output AFTER the downscale:
	 *
	 * 1. Every edge pixel the bicubic scale left at less than 50% alpha was simply dropped, so any
	 *    feature thinner than ~2 source-scaled pixels (boot tops, cuff rims, straps) came out torn.
	 *    Now: pixels with meaningful partial alpha are COMPOSITED over a dark tone close to the
	 *    inventory slot backdrop and kept. Binary transparency cannot fade, but it can darken - the
	 *    edge reads as a clean anti-aliased outline on the dark panels it always sits on.
	 * 2. Dropping those pixels also orphaned fragments whose thin connections vanished, leaving
	 *    floating crumbs beside the icon. The source-resolution island filter cannot see these -
	 *    they only become islands after the downscale - so a second, final-resolution sweep runs
	 *    here.
	 * 3. The source art's tonal range (roughly 25..170) collapsed into two or three palette
	 *    entries, reading as flat mud. A per-icon percentile stretch spreads it across the ramp
	 *    before quantisation gets its one chance.
	 *
	 * fillPunctures additionally closes enclosed transparent holes with opaque black - see Pass 4
	 * below. Opt-in per caller; do not default this to true without re-reviewing every existing
	 * icon first, the belt specifically (see the spec-parsing comment on fillPunctures in Main).
	 */
	private static void PostProcess(Bitmap bmp, bool fillPunctures)
	{
		int w = bmp.Width, h = bmp.Height;
		// The tone the game paints behind items: InvDrawSlotBack's darkened slot. Edge pixels blend
		// toward this, so on the actual panel the outline is seamless.
		const int BackR = 38, BackG = 33, BackB = 30;
		const int KeepAlpha = 56; // below this an edge pixel is discarded; above, composited

		BitmapData data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;

				// Pass 1: tonal percentiles over meaningfully-opaque pixels.
				List<int> lumas = new List<int>();
				for (int y = 0; y < h; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = 0; x < w; x++) {
						if (row[x * 4 + 3] < 128)
							continue;
						lumas.Add((row[x * 4 + 2] * 3 + row[x * 4 + 1] * 6 + row[x * 4]) / 10);
					}
				}
				double gain = 1.0;
				int bias = 0;
				if (lumas.Count >= 32) {
					lumas.Sort();
					int p05 = lumas[lumas.Count * 5 / 100];
					int p95 = lumas[lumas.Count * 95 / 100];
					if (p95 - p05 >= 24) {
						// Map [p05, p95] onto [20, 168]. The first pass used a 205 ceiling and
						// turned every brown into bright copper - brightened warm tones land on the
						// palette's orange ramp instead of its red-brown ones, so the ceiling is
						// what controls the icons' apparent hue, not just their brightness. The
						// dither carries the mid-tone gradients now, so the stretch only needs to
						// lift the art out of the bottom two ramp entries, not maximise contrast.
						gain = (168.0 - 20.0) / (p95 - p05);
						gain = Math.Min(gain, 1.5);
						bias = 20 - (int)(p05 * gain);
					}
				}

				// Pass 2: stretch + edge compositing.
				for (int y = 0; y < h; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = 0; x < w; x++) {
						byte* p = row + x * 4;
						int a = p[3];
						if (a < KeepAlpha) {
							p[3] = 0;
							continue;
						}
						int r = (int)(p[2] * gain) + bias;
						int g = (int)(p[1] * gain) + bias;
						int b = (int)(p[0] * gain) + bias;
						r = Math.Max(0, Math.Min(255, r));
						g = Math.Max(0, Math.Min(255, g));
						b = Math.Max(0, Math.Min(255, b));
						if (a < 255) {
							r = (r * a + BackR * (255 - a)) / 255;
							g = (g * a + BackG * (255 - a)) / 255;
							b = (b * a + BackB * (255 - a)) / 255;
						}
						p[2] = (byte)r;
						p[1] = (byte)g;
						p[0] = (byte)b;
						p[3] = 255;
					}
				}

				// Pass 3: final-resolution island sweep.
				//
				// Measured directly rather than guessed, after the erosion pass above turned out
				// not to reach these (the gap between a dangling buckle-strap/stud and the hand it
				// hangs off is 5-7px wide post-downscale - a real gap, not a hairline leak - and
				// closing it safely risks fusing the intentional gaps between splayed claw-tips
				// elsewhere on the same icon). Connected-component sizes across all six icons:
				// debris tops out at 57px (a gloves fragment), real content starts at 171px
				// (the smaller of the two boot pieces - boots legitimately renders as a pair, so
				// nothing here assumes "one icon = one island"). 90 sits with margin in the gap
				// and confirmed to change no icon's silhouette in the least-detailed direction
				// (belt, at 56x28, has no fragmented content to lose).
				const int MinCellIsland = 90;
				bool[] visited = new bool[w * h];
				int[] dx = { 1, -1, 0, 0 };
				int[] dy = { 0, 0, 1, -1 };
				for (int start = 0; start < w * h; start++) {
					int sx = start % w, sy = start / w;
					if (visited[start] || (basePtr + sy * data.Stride)[sx * 4 + 3] == 0)
						continue;
					List<int> cells = new List<int>();
					Queue<int> bfs = new Queue<int>();
					visited[start] = true;
					bfs.Enqueue(start);
					while (bfs.Count > 0) {
						int cell = bfs.Dequeue();
						cells.Add(cell);
						int cx = cell % w, cy = cell / w;
						for (int d = 0; d < 4; d++) {
							int nx = cx + dx[d], ny = cy + dy[d];
							if (nx < 0 || nx >= w || ny < 0 || ny >= h)
								continue;
							int n = ny * w + nx;
							if (visited[n] || (basePtr + ny * data.Stride)[nx * 4 + 3] == 0)
								continue;
							visited[n] = true;
							bfs.Enqueue(n);
						}
					}
					if (cells.Count < MinCellIsland) {
						foreach (int cell in cells)
							(basePtr + (cell / w) * data.Stride)[(cell % w) * 4 + 3] = 0;
					}
				}

				// Pass 4: fill enclosed transparent "punctures" with opaque black. Opt-in
				// (fillPunctures) - see this function's doc comment for why it isn't unconditional.
				//
				// User report ("full of punctures... I don't approve it") on the helm - the mirror
				// image of Pass 3's floating debris, same root cause. At full source resolution the
				// helmet's shadowed interior is validly connected to the crop's real background by a
				// thin dark channel; that channel is exactly the kind of detail Pass 3's own comment
				// describes thinning below survival during the downscale into a 56px cell. Once it's
				// gone, whatever was on the far side of it - background at cut time - is stranded:
				// still transparent, but now with no path out, enclosed by opaque material on every
				// side. A flood fill from THIS cell's own four edges (not the original crop's, which
				// no longer means anything at this resolution) finds every pixel still genuinely open
				// to the outside; anything transparent it can't reach only used to be open by way of a
				// bridge that the downscale has already erased, and reads as a hole rather than a
				// silhouette to anyone looking at the final icon.
				if (fillPunctures) {
					bool[] reachesEdge = new bool[w * h];
					Queue<int> queue = new Queue<int>();
					for (int x = 0; x < w; x++) {
						foreach (int y in new[] { 0, h - 1 }) {
							int i = y * w + x;
							if (!reachesEdge[i] && (basePtr + y * data.Stride)[x * 4 + 3] == 0) { reachesEdge[i] = true; queue.Enqueue(i); }
						}
					}
					for (int y = 0; y < h; y++) {
						foreach (int x in new[] { 0, w - 1 }) {
							int i = y * w + x;
							if (!reachesEdge[i] && (basePtr + y * data.Stride)[x * 4 + 3] == 0) { reachesEdge[i] = true; queue.Enqueue(i); }
						}
					}
					while (queue.Count > 0) {
						int cell = queue.Dequeue();
						int cx = cell % w, cy = cell / w;
						for (int d = 0; d < 4; d++) {
							int nx = cx + dx[d], ny = cy + dy[d];
							if (nx < 0 || nx >= w || ny < 0 || ny >= h)
								continue;
							int n = ny * w + nx;
							if (reachesEdge[n] || (basePtr + ny * data.Stride)[nx * 4 + 3] != 0)
								continue;
							reachesEdge[n] = true;
							queue.Enqueue(n);
						}
					}
					for (int y = 0; y < h; y++) {
						byte* row = basePtr + y * data.Stride;
						for (int x = 0; x < w; x++) {
							int i = y * w + x;
							if (row[x * 4 + 3] != 0 || reachesEdge[i])
								continue;
							row[x * 4] = 0;
							row[x * 4 + 1] = 0;
							row[x * 4 + 2] = 0;
							row[x * 4 + 3] = 255;
						}
					}
				}
			}
		} finally {
			bmp.UnlockBits(data);
		}
	}

	/**
	 * @brief Flattens to palette indices, 0 meaning transparent (safe: output is 128-255 only).
	 *
	 * Floyd-Steinberg at reduced strength, not plain nearest-match. With ~128 usable entries a
	 * nearest match collapses the art's gradients into two or three flat patches - the "muddy"
	 * look. Error diffusion trades that for fine speckle, which is exactly what the original
	 * game's own item icons do; it is the texture this palette was drawn for. Error never
	 * diffuses into transparent pixels, so the silhouette stays crisp.
	 */
	private static byte[] Quantise(Bitmap bmp, byte[] pal, string label)
	{
		// Full-strength FS at 56px shimmers; half-strength keeps the ramps without the noise.
		const double DitherStrength = 0.5;

		int w = bmp.Width, h = bmp.Height;
		byte[] outIdx = new byte[w * h];
		double[] errR = new double[w * h];
		double[] errG = new double[w * h];
		double[] errB = new double[w * h];
		bool[] opaqueMask = new bool[w * h];
		int opaque = 0;

		BitmapData data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;
				for (int y = 0; y < h; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = 0; x < w; x++)
						opaqueMask[y * w + x] = row[x * 4 + 3] >= 128;
				}

				for (int y = 0; y < h; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = 0; x < w; x++) {
						int i = y * w + x;
						if (!opaqueMask[i])
							continue;

						double r = row[x * 4 + 2] + errR[i];
						double g = row[x * 4 + 1] + errG[i];
						double b = row[x * 4] + errB[i];
						int ri = (int)Math.Max(0, Math.Min(255, Math.Round(r)));
						int gi = (int)Math.Max(0, Math.Min(255, Math.Round(g)));
						int bi = (int)Math.Max(0, Math.Min(255, Math.Round(b)));

						byte idx = Nearest(pal, ri, gi, bi);
						outIdx[i] = idx;
						opaque++;

						double dr = (r - pal[idx * 3]) * DitherStrength;
						double dg = (g - pal[idx * 3 + 1]) * DitherStrength;
						double db = (b - pal[idx * 3 + 2]) * DitherStrength;

						// Standard FS kernel: right 7/16, below-left 3/16, below 5/16, below-right 1/16.
						if (x + 1 < w && opaqueMask[i + 1]) {
							errR[i + 1] += dr * 7 / 16; errG[i + 1] += dg * 7 / 16; errB[i + 1] += db * 7 / 16;
						}
						if (y + 1 < h) {
							if (x > 0 && opaqueMask[i + w - 1]) {
								errR[i + w - 1] += dr * 3 / 16; errG[i + w - 1] += dg * 3 / 16; errB[i + w - 1] += db * 3 / 16;
							}
							if (opaqueMask[i + w]) {
								errR[i + w] += dr * 5 / 16; errG[i + w] += dg * 5 / 16; errB[i + w] += db * 5 / 16;
							}
							if (x + 1 < w && opaqueMask[i + w + 1]) {
								errR[i + w + 1] += dr * 1 / 16; errG[i + w + 1] += dg * 1 / 16; errB[i + w + 1] += db * 1 / 16;
							}
						}
					}
				}
			}
		} finally {
			bmp.UnlockBits(data);
		}
		Console.WriteLine("  {0}: {1}x{2}, {3} opaque px", label, w, h, opaque);
		return outIdx;
	}

	/** @brief Nearest entry in the shared half of the palette (128-255), 2/4/3 weighted - see
	 * WaypointCel.cs for why the weighting matters. */
	private static byte Nearest(byte[] pal, int r, int g, int b)
	{
		long best = long.MaxValue;
		byte bestIdx = 128;
		for (int i = 128; i < 256; i++) {
			long dr = r - pal[i * 3], dg = g - pal[i * 3 + 1], db = b - pal[i * 3 + 2];
			long d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
			if (d < best) {
				best = d;
				bestIdx = (byte)i;
			}
		}
		return bestIdx;
	}

	private static byte[] EncodeCel(List<byte[]> frames, List<int> widths, List<int> heights)
	{
		List<byte[]> bodies = new List<byte[]>();
		for (int i = 0; i < frames.Count; i++)
			bodies.Add(EncodeFrame(frames[i], widths[i], heights[i]));

		int headerSize = 4 + 4 * (frames.Count + 1);
		using (MemoryStream ms = new MemoryStream())
		using (BinaryWriter bw = new BinaryWriter(ms)) {
			bw.Write((uint)frames.Count);
			uint offset = (uint)headerSize;
			bw.Write(offset);
			foreach (byte[] body in bodies) {
				offset += (uint)body.Length;
				bw.Write(offset);
			}
			foreach (byte[] body in bodies)
				bw.Write(body);
			return ms.ToArray();
		}
	}

	/** @brief One frame, control-byte RLE, BOTTOM-UP. See WaypointCel.cs for the format notes. */
	private static byte[] EncodeFrame(byte[] idx, int width, int height)
	{
		List<byte> outBytes = new List<byte>();
		for (int y = height - 1; y >= 0; y--) {
			int x = 0;
			while (x < width) {
				int start = x;
				if (idx[y * width + x] == 0) {
					while (x < width && idx[y * width + x] == 0 && x - start < 128)
						x++;
					outBytes.Add((byte)(256 - (x - start)));
				} else {
					while (x < width && idx[y * width + x] != 0 && x - start < 127)
						x++;
					outBytes.Add((byte)(x - start));
					for (int i = start; i < x; i++)
						outBytes.Add(idx[y * width + i]);
				}
			}
		}
		if (outBytes.Count >= 2 && outBytes[0] == 10 && outBytes[1] == 0)
			throw new InvalidOperationException("frame would be misread as having a 10-byte sub-header");
		return outBytes.ToArray();
	}

	private static void WritePreview(byte[] idx, int width, int height, byte[] pal, string path)
	{
		Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path)));
		using (Bitmap bmp = new Bitmap(width, height, PixelFormat.Format32bppArgb)) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					byte i = idx[y * width + x];
					bmp.SetPixel(x, y, i == 0 ? Color.FromArgb(0, 0, 0, 0)
					                          : Color.FromArgb(255, pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]));
				}
			}
			bmp.Save(path, ImageFormat.Png);
		}
	}
}
