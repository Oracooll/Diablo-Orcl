// Oracool asset pipeline: builds objects\orclroar.cel - Levski's Roar, the town monument - from the
// single green-keyed painting the user dropped in Oracool.MPQ's root on 2026-08-20.
//
// A sibling of tools/ReliquaryCel.cs and tools/WaypointCel.cs; the CEL encoder is the same one, and
// WaypointCel's header documents the format in full. Three things are specific to this asset.
//
// ## One frame, not a trio
//
// OBJ_STAND is static: animLen 0, no Animated flag (Source/objdat.cpp's OBJ_STAND row). The chest
// needed six frames because it opens; a monument needs exactly one, and shipping six copies of it
// would quadruple the file for nothing.
//
// ## The anchor is the plaza's near corner, NOT the centre of the content box
//
// DrawObject places a sprite bottom-anchored and horizontally centred - screenPosition.x is
// targetBufferPosition.x - CalculateWidth2(width), i.e. (width - 64) / 2 (scrollrt.cpp:589). So
// whatever ends up at the horizontal centre of the frame is what sits on the object's tile.
//
// This painting has a long hard cast shadow running up and to the LEFT, well past the plaza. Framing
// on the content box would therefore centre the frame on the shadow's midpoint and shove the
// monument a third of a tile to the right of where it was placed - the sprite would look fine in
// isolation and be wrong in the world, which is the failure mode that survives a green build.
//
// So the anchor is measured: the lowest opaque row is the plaza's near corner (the bottom vertex of
// the base diamond, which is where the floor visually is), and the frame is padded SYMMETRICALLY
// about that column. The shadow then simply occupies whatever space it needs on the left, with dead
// space matching it on the right.
//
// ## The shadow is kept, and it is not ours to move
//
// CEL transparency is binary, so a soft shadow cannot survive the format and every vanilla object's
// shadow is painted into its art. This one already is - opaque black, drawn by the artist. It is
// kept as delivered, on the same principle the new chest was ("leave it as drawn"): the alternative
// is keying out a large black region by luma, which would also eat the monument's own darkest
// stone.
//
// Usage: MonumentCel.exe <source.png> <palette.pal> <out.cel> <targetWidth> [previewDir]

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

internal static class MonumentCel
{
	// The source is a chroma-key plate, not an RGBA export, so the key happens here. Same graded
	// test the chest and town-portal cuts use: alpha by green DOMINANCE rather than a binary
	// threshold, with the green spill pulled down to the other channels. A hard key leaves a green
	// rim on every anti-aliased edge, and this art has thousands of them - chain links, wreath
	// leaves, the lettering.
	private const int GreenLeadCut = 60;

	// The downsample is what introduces partial alpha along the silhouette; half-way is the neutral
	// threshold for it and leaves the outline where the art put it.
	private const int AlphaCut = 128;

	// Loose enough to catch the faint edge pixels the render will drop, so the measurement below
	// frames the same silhouette the encoder emits.
	private const int BboxAlphaCut = 40;

	private static int Main(string[] args)
	{
		if (args.Length < 4) {
			Console.Error.WriteLine("Usage: MonumentCel.exe <source.png> <palette.pal> <out.cel> <targetWidth> [previewDir]");
			return 2;
		}

		string sourcePath = args[0];
		byte[] pal = File.ReadAllBytes(args[1]);
		string outPath = args[2];
		int targetWidth = int.Parse(args[3]);
		string previewDir = args.Length > 4 && args[4] != "-" ? args[4] : null;

		if (pal.Length != 768) {
			Console.Error.WriteLine("Palette must be exactly 768 bytes (256 RGB triples), got " + pal.Length);
			return 1;
		}

		using (Bitmap plate = new Bitmap(sourcePath))
		using (Bitmap keyed = ChromaKey(plate)) {
			Console.WriteLine("source {0}x{1}", plate.Width, plate.Height);

			Rectangle box = ContentBox(keyed);
			int anchorX = AnchorColumn(keyed, box);
			Console.WriteLine("content box {0}; floor anchor at x={1}", box, anchorX);

			// Symmetric about the anchor: whichever side reaches further sets the half-width, and
			// the other side is padded to match. This is what keeps the monument on its own tile.
			int half = Math.Max(anchorX - box.Left, box.Right - 1 - anchorX);
			Rectangle framed = new Rectangle(anchorX - half, box.Top, 2 * half + 1, box.Height);
			Console.WriteLine("anchored crop {0} (padded {1}px left, {2}px right)",
				framed, box.Left - framed.Left, framed.Right - box.Right);

			double scale = (double)targetWidth / framed.Width;
			int frameWidth = EvenUp((int)Math.Round(framed.Width * scale));
			int frameHeight = (int)Math.Round(framed.Height * scale);

			byte[] idx;
			using (Bitmap scaled = ScaleTo(keyed, framed, frameWidth, frameHeight)) {
				idx = Quantise(scaled, pal);
			}

			if (previewDir != null)
				WritePreview(idx, frameWidth, frameHeight, pal, Path.Combine(previewDir, "levski_roar.png"));

			byte[] cel = EncodeCel(new byte[][] { idx }, frameWidth, frameHeight);
			Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath)));
			File.WriteAllBytes(outPath, cel);
			Console.WriteLine("wrote {0} ({1} bytes, 1 frame, {2}x{3})", outPath, cel.Length, frameWidth, frameHeight);
			Console.WriteLine("REMINDER: OracoolLevskiRoarAnimWidth in Source/objdat.h must read {0}.", frameWidth);
		}

		return 0;
	}

	/** @brief Even widths centre cleanly under the engine's CalculateWidth2 ((width - 64) / 2). */
	private static int EvenUp(int v)
	{
		return (v % 2 == 0) ? v : v + 1;
	}

	/** @brief Green plate to straight RGBA, grading the edges rather than thresholding them. */
	private static Bitmap ChromaKey(Bitmap src)
	{
		Bitmap dst = new Bitmap(src.Width, src.Height, PixelFormat.Format32bppArgb);
		BitmapData sd = src.LockBits(new Rectangle(0, 0, src.Width, src.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		BitmapData dd = dst.LockBits(new Rectangle(0, 0, dst.Width, dst.Height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				for (int y = 0; y < src.Height; y++) {
					byte* s = (byte*)sd.Scan0 + y * sd.Stride;
					byte* d = (byte*)dd.Scan0 + y * dd.Stride;
					for (int x = 0; x < src.Width; x++) {
						int b = s[x * 4 + 0], g = s[x * 4 + 1], r = s[x * 4 + 2];
						int lead = g - Math.Max(r, b);
						if (lead <= 0) {
							d[x * 4 + 0] = (byte)b; d[x * 4 + 1] = (byte)g; d[x * 4 + 2] = (byte)r; d[x * 4 + 3] = 255;
						} else if (lead < GreenLeadCut) {
							// Partial edge: fade it out, and pull the green spill down to whatever
							// the other two channels agree on so the rim does not read green.
							int a = 255 * (GreenLeadCut - lead) / GreenLeadCut;
							int g2 = Math.Max(r, b);
							d[x * 4 + 0] = (byte)b; d[x * 4 + 1] = (byte)g2; d[x * 4 + 2] = (byte)r; d[x * 4 + 3] = (byte)a;
						}
					}
				}
			}
		} finally {
			src.UnlockBits(sd);
			dst.UnlockBits(dd);
		}
		return dst;
	}

	/** @brief Tight box of everything meaningfully opaque in `bmp`. */
	private static Rectangle ContentBox(Bitmap bmp)
	{
		int minX = int.MaxValue, minY = int.MaxValue, maxX = -1, maxY = -1;
		BitmapData data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				for (int y = 0; y < bmp.Height; y++) {
					byte* row = (byte*)data.Scan0 + y * data.Stride;
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
			throw new InvalidOperationException("source is entirely keyed out - check GreenLeadCut");
		return new Rectangle(minX, minY, maxX - minX + 1, maxY - minY + 1);
	}

	/**
	 * @brief The column the object stands on: the midpoint of the lowest opaque row.
	 *
	 * On an isometric base the lowest row is the near corner of the footprint diamond, a handful of
	 * pixels wide, so its midpoint is the diamond's own axis. Taking the widest row instead would
	 * pick up the flower beds, which are not symmetric about the plinth.
	 */
	private static int AnchorColumn(Bitmap bmp, Rectangle box)
	{
		BitmapData data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				for (int y = box.Bottom - 1; y >= box.Top; y--) {
					byte* row = (byte*)data.Scan0 + y * data.Stride;
					int left = -1, right = -1;
					for (int x = box.Left; x < box.Right; x++) {
						if (row[x * 4 + 3] < BboxAlphaCut)
							continue;
						if (left < 0) left = x;
						right = x;
					}
					if (left >= 0)
						return (left + right) / 2;
				}
			}
		} finally {
			bmp.UnlockBits(data);
		}
		throw new InvalidOperationException("no opaque row found");
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
	 * Index 0 is safe as the transparency marker precisely because the output is restricted to
	 * 128-255 - no real pixel can ever land on it.
	 */
	private static byte[] Quantise(Bitmap bmp, byte[] pal)
	{
		int w = bmp.Width, h = bmp.Height;
		byte[] outIdx = new byte[w * h];
		Dictionary<int, byte> cache = new Dictionary<int, byte>();
		int opaque = 0;

		BitmapData data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
		try {
			unsafe {
				for (int y = 0; y < h; y++) {
					byte* row = (byte*)data.Scan0 + y * data.Stride;
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

		Console.WriteLine("  {0} opaque px ({1:P1} of frame), {2} distinct source colours", opaque, opaque / (double)(w * h), cache.Count);
		return outIdx;
	}

	/** @brief Nearest entry in the SHARED half of the palette (128-255) by luma-weighted distance. */
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

	/** @brief Packs frames into a single-group CEL. See tools/WaypointCel.cs for the format notes. */
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
		// optional 10-byte sub-header and skips it. Asserted rather than assumed.
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
