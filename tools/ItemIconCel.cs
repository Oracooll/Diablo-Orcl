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
//   spec = <sheet.png>,<srcX>,<srcY>,<srcW>,<srcH>,<cellW>,<cellH>,<name>

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
	// Rendering cut, applied after downscaling. Higher than the bbox cut because a scaled-down
	// edge pixel that is half backdrop reads as a dark fringe around the icon.
	private const int RenderLumaCut = 55;

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
			if (parts.Length != 8) {
				Console.Error.WriteLine("Bad spec: " + args[i]);
				return 1;
			}
			string sheetPath = parts[0];
			Rectangle srcBox = new Rectangle(int.Parse(parts[1]), int.Parse(parts[2]), int.Parse(parts[3]), int.Parse(parts[4]));
			int cellW = int.Parse(parts[5]);
			int cellH = int.Parse(parts[6]);
			string name = parts[7];

			using (Bitmap sheet = new Bitmap(sheetPath)) {
				Rectangle content = ContentBox(sheet, srcBox);
				Console.WriteLine("{0}: search box {1} -> content {2}", name, srcBox, content);

				using (Bitmap cell = FitInto(sheet, content, cellW, cellH)) {
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

	/** @brief Flattens to palette indices, 0 meaning transparent (safe: output is 128-255 only). */
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
						int b = row[x * 4], g = row[x * 4 + 1], r = row[x * 4 + 2];
						if (Math.Max(r, Math.Max(g, b)) < RenderLumaCut)
							continue;
						int key = (r << 16) | (g << 8) | b;
						byte idx;
						if (!cache.TryGetValue(key, out idx)) {
							idx = Nearest(pal, r, g, b);
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
		Console.WriteLine("  {0}: {1}x{2}, {3} opaque px, {4} distinct source colours", label, w, h, opaque, cache.Count);
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
