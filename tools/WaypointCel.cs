// Oracool asset pipeline: turns the user's two-state waypoint painting into a Diablo .CEL sprite.
//
// This is the first tool in the project that ENCODES a CEL rather than decoding one
// (tools/oracool_cel_to_png.ps1 is the decoder, and documents the format). Everything the HUD and
// inventory work shipped so far is a PNG loaded through the oracool hud_art machinery, which is our
// own code and can therefore read whatever we like. Objects in the world are different: they go
// through the engine's own LoadCel/CelToClx path, so replacing one means producing a real CEL.
//
// Two format facts drive the whole file:
//
//   1. CEL scanlines are stored BOTTOM-UP, like a BMP. (Learned the hard way - the first contact
//      sheets this project generated came out upside down.)
//   2. A CEL is 8-bit indexed into the LEVEL's palette, and only indices 128-255 are shared between
//      level types; 0-127 are re-defined per level and colour-cycled. The waypoint appears in town
//      AND on all 16 dungeon levels, so it must quantise into the upper half only or it would come
//      out a different colour on every level. Verified directly: town.pal, l1_1.pal, l2_1.pal,
//      l3_1.pal and l4_1.pal differ in 127 of 128 low entries and in 0 of 128 high ones.
//
// The upper half happens to suit this art unusually well - 128-135 is a pure blue ramp for the
// glow and the flames, 176-188 a blue-grey stone ramp for the platform.
//
// Usage: WaypointCel.exe <source.png> <palette.pal> <out.cel> [previewDir]

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

internal static class WaypointCel
{
	// Frame width on screen. The engine draws an object with the frame's bottom-left at the tile's
	// bottom vertex, horizontally centred via CalculateWidth2 ((width - 64) / 2), so an even width
	// centres cleanly and 144 makes the platform read as roughly a 2x2-tile landmark - anchored at
	// its bottom tile, which is exactly how every other oversized object in the game behaves.
	private const int FrameWidth = 144;

	// Alpha below this is dropped outright. The source's outer aura fades to nothing over a couple
	// of hundred pixels; CEL transparency is binary (a pixel is either in a run or skipped), so a
	// low cut would ring the sprite with a hard-edged rectangle of almost-black.
	private const int AlphaCut = 96;

	// ...and even above the alpha cut, a pixel that is nearly black contributes nothing on a dark
	// dungeon floor while still costing a hard silhouette edge. Dropping those lets the aura fade
	// out by *area* instead of by opacity, which is the only kind of fade binary transparency has.
	private const int LumaCut = 26;

	// ...but ONLY for a painting being fitted. Finished pixel art (the EXACT path below) gets no luma
	// cut at all, and that is not a tweak - it is the difference between the sprite working and not.
	// The user's shadowed repaint has 1766 pixels under LumaCut in its dormant frame, a quarter of the
	// sprite: the platform's own dark stone and the shadow beneath it. Cutting them left a skeleton of
	// blue glow lines with no platform under it. There is no soft aura here to fade out by area, so
	// there is nothing for the cut to do except delete the subject.
	private const int ExactLumaCut = 0;

	// Bounding box detection uses a far looser cut than rendering does, so the box still contains
	// the glow the render will thin out - otherwise the two states would be framed differently.
	private const int BboxAlphaCut = 40;

	private static int Main(string[] args)
	{
		if (args.Length < 3) {
			Console.Error.WriteLine("Usage: WaypointCel.exe <source.png> <palette.pal> <out.cel> [previewDir]");
			return 2;
		}

		string sourcePath = args[0];
		string palettePath = args[1];
		string outPath = args[2];
		string previewDir = args.Length > 3 ? args[3] : null;

		byte[] pal = File.ReadAllBytes(palettePath);
		if (pal.Length != 768) {
			Console.Error.WriteLine("Palette must be exactly 768 bytes (256 RGB triples), got " + pal.Length);
			return 1;
		}

		using (Bitmap source = new Bitmap(sourcePath)) {
			Console.WriteLine("source {0}x{1}", source.Width, source.Height);

			int half = source.Width / 2;
			string[] names = { "dormant", "active" };
			byte[][] frames = new byte[2][];
			int frameHeight;

			// EXACT path (2026-09-21). A source that is ALREADY two FrameWidth-wide frames is finished
			// pixel art, not a painting to be fitted - the user's repaint of the platform, the one with
			// the shadow under it, arrives at 288x106. Fitting it would be actively wrong: the content
			// box is tighter than the frame, so bbox-and-scale would blow the platform up to fill 144
			// and eat the very margin the new shadow lives in, then resample every pixel of a sprite
			// that is already at its final size. Taken 1:1 and only quantised.
			if (source.Width == 2 * FrameWidth) {
				frameHeight = source.Height;
				Console.WriteLine("exact source: frame {0}x{1}, taken 1:1 (no bbox, no scale)", FrameWidth, frameHeight);
				for (int f = 0; f < 2; f++) {
					using (Bitmap cut = source.Clone(new Rectangle(f * FrameWidth, 0, FrameWidth, frameHeight), PixelFormat.Format32bppArgb)) {
						frames[f] = Quantise(cut, pal, names[f], ExactLumaCut);
						if (previewDir != null)
							WritePreview(frames[f], FrameWidth, frameHeight, pal, Path.Combine(previewDir, "waypoint_" + names[f] + ".png"));
					}
				}
			} else {
				Rectangle dormantBox = new Rectangle(0, 0, half, source.Height);
				Rectangle activeBox = new Rectangle(half, 0, source.Width - half, source.Height);

				// One shared source box, expressed in each half's own coordinates, so the platform does
				// not jump between the two frames when the game swaps them.
				Rectangle a = ContentBox(source, dormantBox);
				Rectangle b = ContentBox(source, activeBox);
				b.X -= half;
				Rectangle shared = Rectangle.Union(a, b);
				Console.WriteLine("dormant box {0}, active box {1}, shared {2}", a, b, shared);

				frameHeight = (int)Math.Round(shared.Height * (double)FrameWidth / shared.Width);
				Console.WriteLine("frame {0}x{1}", FrameWidth, frameHeight);

				Rectangle[] boxes = { new Rectangle(shared.X, shared.Y, shared.Width, shared.Height),
					new Rectangle(shared.X + half, shared.Y, shared.Width, shared.Height) };

				for (int f = 0; f < 2; f++) {
					using (Bitmap scaled = ScaleTo(source, boxes[f], FrameWidth, frameHeight)) {
						frames[f] = Quantise(scaled, pal, names[f], LumaCut);
						if (previewDir != null)
							WritePreview(frames[f], FrameWidth, frameHeight, pal, Path.Combine(previewDir, "waypoint_" + names[f] + ".png"));
					}
				}
			}

			byte[] cel = EncodeCel(frames, FrameWidth, frameHeight);
			Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath)));
			File.WriteAllBytes(outPath, cel);
			Console.WriteLine("wrote {0} ({1} bytes, {2} frames, {3}x{4})", outPath, cel.Length, frames.Length, FrameWidth, frameHeight);
		}

		return 0;
	}

	/** @brief Tight box of everything meaningfully opaque inside `region`, in source coordinates. */
	private static Rectangle ContentBox(Bitmap bmp, Rectangle region)
	{
		int minX = int.MaxValue, minY = int.MaxValue, maxX = -1, maxY = -1;
		BitmapData data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				byte* basePtr = (byte*)data.Scan0;
				for (int y = region.Top; y < region.Bottom; y++) {
					byte* row = basePtr + y * data.Stride;
					for (int x = region.Left; x < region.Right; x++) {
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
			throw new InvalidOperationException("region is entirely transparent");
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
	private static byte[] Quantise(Bitmap bmp, byte[] pal, string label, int lumaCut)
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
						if (Math.Max(rch, Math.Max(gch, bch)) < lumaCut)
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

	/**
	 * @brief Nearest entry in the SHARED half of the palette (128-255) by luma-weighted distance.
	 *
	 * The weights matter more than they look. A first pass used 3/6/1, which under-weights blue so
	 * hard that the platform's cool grey stone snapped to the palette's warm gold ramp (192-207)
	 * instead of its grey ramp (240-254) - blue is the one channel that separates those two, so
	 * discounting it discards exactly the information the choice turns on. 2/4/3 is the usual
	 * perceptual weighting and keeps the stone the colour the art painted it.
	 */
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
	 * @brief Packs frames into a single-group CEL.
	 *
	 * Header is a uint32 frame count followed by frameCount+1 uint32 byte offsets (the last being
	 * end-of-file). Each frame is control-byte RLE, scanline by scanline, BOTTOM-UP: 0x01-0x7F is
	 * an opaque run of that many raw index bytes, 0x80-0xFF a transparent run of 256-control
	 * pixels. Runs never cross a row boundary.
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
		// optional 10-byte sub-header and skips it. An opaque run of 10 would need a following
		// index byte of 0 to collide, which cannot happen while every index is >= 128; a
		// transparent run always writes a control byte >= 0x80. Assert rather than assume.
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
