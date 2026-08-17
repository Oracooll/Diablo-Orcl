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

			int frameWidth = EvenUp((int)Math.Round(shared.Width * ScaleFactor));
			int frameHeight = (int)Math.Round(shared.Height * ScaleFactor);
			Console.WriteLine("frame {0}x{1}", frameWidth, frameHeight);

			byte[][] trio = new byte[3][];
			for (int i = 0; i < 3; i++) {
				using (Bitmap scaled = ScaleTo(states[i], shared, frameWidth, frameHeight)) {
					trio[i] = Quantise(scaled, pal, stateNames[i]);
					if (previewDir != null)
						WritePreview(trio[i], frameWidth, frameHeight, pal, Path.Combine(previewDir, "reliquary_" + stateNames[i] + ".png"));
				}
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
