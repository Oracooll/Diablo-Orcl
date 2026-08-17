// Oracool asset pipeline: builds objects\orclstash.cel - the town Stash Chest's Grand Reliquary -
// from the delivered pack's three RGBA state masters.
//
// The pack ships a finished CEL already, so why re-encode one? Two reasons, both from the user
// (2026-08-18): "this is too big. scale down to half the size and use the mirror asset." The
// delivered sprite is 160 wide, two and a half floor tiles, which read as a building rather than a
// chest. Halving it means resampling, and resampling an already-quantised, already-binary-alpha CEL
// would blend palette indices as if they were numbers. So the scale-down goes back to the RGBA
// masters and redoes the last two steps of the pack's own pipeline - downsample, then quantise -
// in that order.
//
// It also paints the contact shadow the delivered art has none of (user, 2026-08-18: "it is lacking
// proper shadow") - see the shadow constants below for how, and why it cannot simply be a soft one.
//
// A sibling of tools/WaypointCel.cs, which documents the CEL format in detail; the encoder here is
// the same one. Three differences worth knowing:
//
//   1. ONE shared content box across all three states, not a per-state box. The pack's whole
//      geometry contract is "one crop, one scale, one bottom-centred placement", so the chest does
//      not jump when it opens. Boxing each state separately would break exactly that.
//   2. Mirrored. The user picked the opposite facing. Flipping the RGBA master here is equivalent
//      to the pack's own mirrored CEL (an exact scanline reversal) but keeps full colour depth for
//      the downsample, so it is the better source of the two.
//   3. No luma cut. The waypoint had a soft outer glow that had to fade out by area; this chest is
//      blackened iron with a hard silhouette, and discarding near-black pixels would punch holes in
//      its own shadowed faces.
//
// Six frames out, the trio written twice: chest3.cel's two-variant convention, which the engine
// side depends on - AddStashChestObject pins frame 4 as closed and OperateStashChest steps to 6.
//
// Usage: ReliquaryCel.exe <closed.png> <opening.png> <open.png> <palette.pal> <out.cel> [previewDir]

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

internal static class ReliquaryCel
{
	// Half, exactly, of whatever the shared content box measures - which is the user's instruction
	// taken literally rather than a new size picked by eye.
	private const double ScaleFactor = 0.5;

	// The masters are already binary-alpha; only the downsample introduces partial alpha, along the
	// silhouette. Half-way is the neutral threshold for that and keeps the outline where the art
	// put it.
	private const int AlphaCut = 128;

	// Loose enough to include the faint edge pixels the render will drop, so all three states get
	// framed identically.
	private const int BboxAlphaCut = 40;

	// --- Contact shadow -------------------------------------------------------------------------
	//
	// The delivered pack has no shadow at all: its palette-and-format doc says "no soft shadow,
	// aura, or partial-alpha pixel remains in the runtime file", because CEL transparency is binary
	// and a soft shadow cannot survive it. The engine has no object-shadow pass either - monsters
	// and players get theirs from their own sprites, and DrawObject does exactly one ClxDraw. So a
	// shadow has to be painted into the art, opaque, the way every vanilla object's is.
	//
	// It is painted AFTER quantisation, directly in index space, and only into cells that are still
	// transparent - so it can never eat a pixel of the chest, and it cannot be dragged off its
	// intended colours by the nearest-colour search.
	//
	// Softness comes from stepping down a palette ramp by radius rather than from alpha, since
	// there is no alpha to step. 251-254 is the top of the town palette's neutral grey ramp
	// (61,61,61 down to 17,17,17): dark, but not the pure black that would read as a hole punched
	// in the floor. The cool blue-grey ramp at 188-191 was tried first and read as a puddle of
	// water rather than a shadow - a shadow desaturates what is under it, it does not tint it.
	private static readonly byte[] ShadowRamp = { 254, 253, 252, 251 };

	// Radii as a fraction of the chest's own contact width, and the flattening that makes the
	// ellipse sit in the isometric ground plane rather than standing up in the picture plane
	// (Diablo's floor diamonds are 64x32, so anything round on the ground reads as ~2:1; a little
	// flatter than that keeps the skirt from crowding the tiles in front).
	private const double ShadowWidthFactor = 1.08;
	private const double ShadowFlatten = 2.6;

	// How far the ellipse's centre tucks up under the chest, as a fraction of its own vertical
	// radius. Zero would centre it on the base line and make the chest look like it is hovering
	// over a puddle; a full radius would hide the shadow entirely behind the chest.
	private const double ShadowTuck = 0.40;

	private static int Main(string[] args)
	{
		if (args.Length < 5) {
			Console.Error.WriteLine("Usage: ReliquaryCel.exe <closed.png> <opening.png> <open.png> <palette.pal> <out.cel> [previewDir]");
			return 2;
		}

		string[] statePaths = { args[0], args[1], args[2] };
		string[] stateNames = { "closed", "opening", "open" };
		string palettePath = args[3];
		string outPath = args[4];
		string previewDir = args.Length > 5 ? args[5] : null;

		byte[] pal = File.ReadAllBytes(palettePath);
		if (pal.Length != 768) {
			Console.Error.WriteLine("Palette must be exactly 768 bytes (256 RGB triples), got " + pal.Length);
			return 1;
		}

		Bitmap[] states = new Bitmap[3];
		try {
			for (int i = 0; i < 3; i++) {
				states[i] = Mirror(new Bitmap(statePaths[i]));
				Console.WriteLine("{0}: {1}x{2} (mirrored)", stateNames[i], states[i].Width, states[i].Height);
			}

			// One box for all three - see the header note.
			Rectangle shared = ContentBox(states[0]);
			for (int i = 1; i < 3; i++)
				shared = Rectangle.Union(shared, ContentBox(states[i]));
			Console.WriteLine("shared content box {0}", shared);

			int artWidth = EvenUp((int)Math.Round(shared.Width * ScaleFactor));
			int artHeight = (int)Math.Round(shared.Height * ScaleFactor);

			byte[][] art = new byte[3][];
			for (int i = 0; i < 3; i++) {
				using (Bitmap scaled = ScaleTo(states[i], shared, artWidth, artHeight)) {
					art[i] = Quantise(scaled, pal, stateNames[i]);
				}
			}

			// The shadow is measured ONCE, from the closed state, and the identical ellipse is
			// stamped into all three. Measuring per state would let it breathe as the lid moves,
			// which is exactly the kind of wobble the shared content box exists to prevent.
			Shadow shadow = PlanShadow(art[0], artWidth, artHeight);
			int frameWidth = EvenUp(artWidth + 2 * shadow.PadX);
			int frameHeight = artHeight + shadow.PadY;
			int offsetX = (frameWidth - artWidth) / 2;
			Console.WriteLine("art {0}x{1}, frame {2}x{3} (shadow pad {4} x, {5} y)",
				artWidth, artHeight, frameWidth, frameHeight, shadow.PadX, shadow.PadY);

			byte[][] trio = new byte[3][];
			for (int i = 0; i < 3; i++) {
				trio[i] = Inset(art[i], artWidth, artHeight, frameWidth, frameHeight, offsetX);
				StampShadow(trio[i], frameWidth, frameHeight, shadow, offsetX);
				if (previewDir != null)
					WritePreview(trio[i], frameWidth, frameHeight, pal, Path.Combine(previewDir, "reliquary_" + stateNames[i] + ".png"));
			}

			// Frames 1-3 and 4-6 are the same trio. The duplication is the point: it mirrors
			// chest3.cel's two closed-variant convention, so "closed is 4, open is 6" survives.
			byte[][] frames = { trio[0], trio[1], trio[2], trio[0], trio[1], trio[2] };

			byte[] cel = EncodeCel(frames, frameWidth, frameHeight);
			Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath)));
			File.WriteAllBytes(outPath, cel);
			Console.WriteLine("wrote {0} ({1} bytes, {2} frames, {3}x{4})", outPath, cel.Length, frames.Length, frameWidth, frameHeight);
			Console.WriteLine("REMINDER: OracoolStashChestAnimWidth in Source/objdat.h must read {0}.", frameWidth);
		} finally {
			foreach (Bitmap b in states)
				if (b != null) b.Dispose();
		}

		return 0;
	}

	/** @brief Even widths centre cleanly under the engine's CalculateWidth2 ((width - 64) / 2). */
	private static int EvenUp(int v)
	{
		return (v % 2 == 0) ? v : v + 1;
	}

	/** @brief The planned contact ellipse, in ART coordinates plus the padding the frame needs. */
	private struct Shadow
	{
		public double CentreX;
		public double CentreY;
		public double RadiusX;
		public double RadiusY;
		public int PadX;
		public int PadY;
	}

	/**
	 * @brief Sizes the contact shadow from the chest's own footprint rather than from constants.
	 *
	 * Two different measurements, because they answer two different questions, and conflating them
	 * was the first attempt's bug. WIDTH comes from the widest opaque row in the bottom third - the
	 * plinth, the part that actually rests on the floor, rather than the lid. The CONTACT ROW is the
	 * lowest opaque row of all: on an isometric base that is the near corner of the footprint
	 * diamond, which is where the floor visually is. Using the widest row's own y as the contact row
	 * put the ellipse a third of the way up the chest, where the chest itself covered all but 34
	 * pixels of it.
	 */
	private static Shadow PlanShadow(byte[] idx, int width, int height)
	{
		int bandTop = height - Math.Max(1, height / 3);
		int bestLeft = -1, bestRight = -1, bestSpan = -1, baseRow = 0;
		for (int y = 0; y < height; y++) {
			int left = -1, right = -1;
			for (int x = 0; x < width; x++) {
				if (idx[y * width + x] == 0)
					continue;
				if (left < 0) left = x;
				right = x;
			}
			if (left < 0)
				continue;
			baseRow = y; // last row with anything opaque in it wins
			if (y >= bandTop && right - left > bestSpan) {
				bestSpan = right - left;
				bestLeft = left;
				bestRight = right;
			}
		}
		if (bestSpan < 0)
			throw new InvalidOperationException("no opaque pixels in the contact band");

		Shadow s = new Shadow();
		s.CentreX = (bestLeft + bestRight) / 2.0;
		s.RadiusX = (bestSpan + 1) / 2.0 * ShadowWidthFactor;
		s.RadiusY = s.RadiusX / ShadowFlatten;
		s.CentreY = baseRow - s.RadiusY * ShadowTuck;

		// Grow the canvas by whatever the ellipse pokes out of it. Extra rows BELOW are the point:
		// the engine anchors a sprite by its bottom edge, so those rows lift the chest until its
		// base sits on the ellipse's centre - which is precisely where the floor now appears to be.
		s.PadX = (int)Math.Ceiling(Math.Max(0, Math.Max(s.RadiusX - s.CentreX, s.CentreX + s.RadiusX - width)));
		s.PadY = (int)Math.Ceiling(Math.Max(0, s.CentreY + s.RadiusY - (height - 1)));
		Console.WriteLine("  contact span {0}px at row {1}; ellipse r {2:F1} x {3:F1}",
			bestSpan + 1, baseRow, s.RadiusX, s.RadiusY);
		return s;
	}

	/** @brief Copies the art into a larger frame, bottom-aligned above the shadow padding. */
	private static byte[] Inset(byte[] art, int artWidth, int artHeight, int frameWidth, int frameHeight, int offsetX)
	{
		byte[] outIdx = new byte[frameWidth * frameHeight];
		for (int y = 0; y < artHeight; y++)
			for (int x = 0; x < artWidth; x++)
				outIdx[y * frameWidth + (x + offsetX)] = art[y * artWidth + x];
		return outIdx;
	}

	/**
	 * @brief Paints the ellipse into transparent cells only, stepping down ShadowRamp by radius.
	 *
	 * Writing only where the frame is still transparent is what makes this safe to run on all three
	 * states from one measurement: wherever the chest is, the chest wins, and the shadow simply
	 * fills in around it.
	 */
	private static void StampShadow(byte[] idx, int width, int height, Shadow s, int offsetX)
	{
		double cx = s.CentreX + offsetX;
		double cy = s.CentreY;
		int painted = 0;

		int x0 = Math.Max(0, (int)Math.Floor(cx - s.RadiusX));
		int x1 = Math.Min(width - 1, (int)Math.Ceiling(cx + s.RadiusX));
		int y0 = Math.Max(0, (int)Math.Floor(cy - s.RadiusY));
		int y1 = Math.Min(height - 1, (int)Math.Ceiling(cy + s.RadiusY));

		for (int y = y0; y <= y1; y++) {
			for (int x = x0; x <= x1; x++) {
				if (idx[y * width + x] != 0)
					continue;
				double dx = (x - cx) / s.RadiusX;
				double dy = (y - cy) / s.RadiusY;
				double d = Math.Sqrt(dx * dx + dy * dy);
				if (d > 1.0)
					continue;
				// Darkest at the core, lightest at the rim; the ramp's own steps are the gradient.
				int step = (int)(d * ShadowRamp.Length);
				if (step >= ShadowRamp.Length)
					step = ShadowRamp.Length - 1;
				idx[y * width + x] = ShadowRamp[step];
				painted++;
			}
		}
		Console.WriteLine("  shadow: {0} px", painted);
	}

	private static Bitmap Mirror(Bitmap source)
	{
		using (source) {
			Bitmap dst = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb);
			using (Graphics g = Graphics.FromImage(dst)) {
				g.CompositingMode = CompositingMode.SourceCopy;
				g.DrawImage(source, new Rectangle(0, 0, dst.Width, dst.Height), source.Width, 0, -source.Width, source.Height, GraphicsUnit.Pixel);
			}
			return dst;
		}
	}

	/** @brief Tight box of everything meaningfully opaque in `bmp`. */
	private static Rectangle ContentBox(Bitmap bmp)
	{
		int minX = int.MaxValue, minY = int.MaxValue, maxX = -1, maxY = -1;
		BitmapData data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;
				for (int y = 0; y < bmp.Height; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = 0; x < bmp.Width; x++) {
						if (row[x * 4 + 3] < BboxAlphaCut)
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
			throw new InvalidOperationException("state is entirely transparent");
		return new Rectangle(minX, minY, maxX - minX + 1, maxY - minY + 1);
	}

	private static Bitmap ScaleTo(Bitmap source, Rectangle srcBox, int width, int height)
	{
		Bitmap dst = new Bitmap(width, height, PixelFormat.Format32bppArgb);
		using (Graphics g = Graphics.FromImage(dst)) {
			g.CompositingMode = CompositingMode.SourceCopy;
			g.InterpolationMode = InterpolationMode.HighQualityBicubic;
			g.PixelOffsetMode = PixelOffsetMode.HighQuality;
			g.DrawImage(source, new Rectangle(0, 0, width, height), srcBox, GraphicsUnit.Pixel);
		}
		return dst;
	}

	/**
	 * @brief Flattens the scaled RGBA frame into palette indices, 0 meaning "transparent here".
	 *
	 * Index 0 is safe as the in-memory transparency marker precisely because the output is
	 * restricted to 128-255 - no real pixel can ever land on it.
	 */
	private static byte[] Quantise(Bitmap bmp, byte[] pal, string label)
	{
		int w = bmp.Width, h = bmp.Height;
		byte[] outIdx = new byte[w * h];
		Dictionary<int, byte> cache = new Dictionary<int, byte>();
		int opaque = 0;

		BitmapData data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;
				for (int y = 0; y < h; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = 0; x < w; x++) {
						int bch = row[x * 4 + 0], gch = row[x * 4 + 1], rch = row[x * 4 + 2], ach = row[x * 4 + 3];
						if (ach < AlphaCut)
							continue;
						int key = (rch << 16) | (gch << 8) | bch;
						byte idx;
						if (!cache.TryGetValue(key, out idx)) {
							idx = Nearest(pal, rch, gch, bch);
							cache[key] = idx;
						}
						outIdx[y * w + x] = idx;
						opaque++;
					}
				}
			}
		} finally {
			bmp.UnlockBits(data);
		}

		Console.WriteLine("  {0}: {1} opaque px ({2:P1} of frame), {3} distinct source colours",
			label, opaque, opaque / (double)(w * h), cache.Count);
		return outIdx;
	}

	/** @brief Nearest entry in the SHARED half of the palette (128-255) by luma-weighted distance.
	 * Same 2/4/3 weighting the pack's own quantiser used, so the re-cut lands on its colours. */
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

	/**
	 * @brief Packs frames into a single-group CEL. See tools/WaypointCel.cs for the format notes.
	 */
	private static byte[] EncodeCel(byte[][] frames, int width, int height)
	{
		List<byte[]> bodies = new List<byte[]>();
		foreach (byte[] frame in frames)
			bodies.Add(EncodeFrame(frame, width, height));

		int headerSize = 4 + 4 * (frames.Length + 1);
		using (MemoryStream ms = new MemoryStream())
		using (BinaryWriter bw = new BinaryWriter(ms)) {
			bw.Write((uint)frames.Length);
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

	private static byte[] EncodeFrame(byte[] idx, int width, int height)
	{
		List<byte> outBytes = new List<byte>();
		// Bottom-up: the last row of the image is written first.
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

		// The decoder treats a frame whose first little-endian uint16 is exactly 10 as carrying an
		// optional 10-byte sub-header and skips it. See WaypointCel.cs for why this cannot happen
		// while every index is >= 128 - asserted rather than assumed.
		if (outBytes.Count >= 2 && outBytes[0] == 10 && outBytes[1] == 0)
			throw new InvalidOperationException("frame would be misread as having a 10-byte sub-header");

		return outBytes.ToArray();
	}

	/** @brief Decodes the quantised indices straight back to PNG so the result can be eyeballed. */
	private static void WritePreview(byte[] idx, int width, int height, byte[] pal, string path)
	{
		Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path)));
		using (Bitmap bmp = new Bitmap(width, height, PixelFormat.Format32bppArgb)) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					byte i = idx[y * width + x];
					bmp.SetPixel(x, y, i == 0
						? Color.FromArgb(0, 0, 0, 0)
						: Color.FromArgb(255, pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]));
				}
			}
			bmp.Save(path, ImageFormat.Png);
		}
		Console.WriteLine("  preview -> " + path);
	}
}
