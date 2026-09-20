// FramesCel - packs a folder of real-alpha PNG frames (all the same size, already at the engine's
// frame width, bottom-centre anchored) into one single-group CEL, quantised to the shared half of
// the town palette exactly as MonumentCel.cs does for the one-frame Roar. Written 2026-09-20 for the
// Stonegate (batch 42: 17 frames of 192 x 304) and reusable for any delivered frame set - the Cube's
// idle loop (batch 43) among them.
//
// Unlike MonumentCel there is no chroma key, no crop and no scaling: the artist delivers frames at
// the size the brief states, with real alpha, and this tool takes them as they are. Every frame
// must have the same width and height; the width must be even (the engine centres odd widths a
// half-pixel off - see CalculateWidth2).
//
// Usage: FramesCel.exe <frameDir> <pattern> <palette.pal> <out.cel> [previewDir|-] [desaturatePercent]
//   pattern is a file glob evaluated in frameDir, e.g. "stonegate_gold_*.png"; several patterns may
//   be given separated by ';' and are taken in that order, each sorted by name.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;

internal static class FramesCel
{
	private const int AlphaCut = 128;
	private static int DesaturatePercent = 0;

	private static int Main(string[] args)
	{
		if (args.Length < 4) {
			Console.Error.WriteLine("Usage: FramesCel.exe <frameDir> <pattern[;pattern...]> <palette.pal> <out.cel> [previewDir|-] [desaturatePercent]");
			return 2;
		}
		string dir = args[0];
		string[] patterns = args[1].Split(';');
		byte[] pal = File.ReadAllBytes(args[2]);
		string outPath = args[3];
		string previewDir = args.Length > 4 && args[4] != "-" ? args[4] : null;
		DesaturatePercent = args.Length > 5 ? int.Parse(args[5]) : 0;
		if (pal.Length != 768) {
			Console.Error.WriteLine("Palette must be exactly 768 bytes, got " + pal.Length);
			return 1;
		}

		List<string> files = new List<string>();
		foreach (string p in patterns)
			files.AddRange(Directory.GetFiles(dir, p.Trim()).OrderBy(f => f, StringComparer.OrdinalIgnoreCase));
		if (files.Count == 0) {
			Console.Error.WriteLine("No frames matched in " + dir);
			return 1;
		}

		int width = -1, height = -1;
		List<byte[]> frames = new List<byte[]>();
		foreach (string f in files) {
			using (Bitmap bmp = new Bitmap(f)) {
				if (width < 0) { width = bmp.Width; height = bmp.Height; }
				if (bmp.Width != width || bmp.Height != height) {
					Console.Error.WriteLine("Frame size mismatch: " + Path.GetFileName(f) + " is " + bmp.Width + "x" + bmp.Height + ", expected " + width + "x" + height);
					return 1;
				}
				Console.WriteLine("{0}", Path.GetFileName(f));
				byte[] idx = Quantise(bmp, pal);
				frames.Add(idx);
				if (previewDir != null)
					WritePreview(idx, width, height, pal, Path.Combine(previewDir, Path.GetFileNameWithoutExtension(f) + "_preview.png"));
			}
		}
		if (width % 2 != 0)
			Console.Error.WriteLine("WARNING: frame width " + width + " is odd; the engine centres it a half-pixel off");

		byte[] cel = EncodeCel(frames.ToArray(), width, height);
		Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath)));
		File.WriteAllBytes(outPath, cel);
		Console.WriteLine("wrote {0} ({1} bytes, {2} frames, {3}x{4})", outPath, cel.Length, frames.Count, width, height);
		Console.WriteLine("REMINDER: the object's animWidth in Source/objdat.h must read {0}, and its frame count {1}.", width, frames.Count);
		return 0;
	}

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
						if (DesaturatePercent > 0) {
							int luma = (299 * rch + 587 * gch + 114 * bch) / 1000;
							rch += (luma - rch) * DesaturatePercent / 100;
							gch += (luma - gch) * DesaturatePercent / 100;
							bch += (luma - bch) * DesaturatePercent / 100;
						}
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
		Console.WriteLine("  {0} opaque px ({1:P1}), {2} distinct colours", opaque, opaque / (double)(w * h), cache.Count);
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
			if (d < best) { best = d; bestIdx = (byte)i; }
		}
		return bestIdx;
	}

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
			foreach (byte[] body in bodies) { offset += (uint)body.Length; bw.Write(offset); }
			foreach (byte[] body in bodies) bw.Write(body);
			return ms.ToArray();
		}
	}

	private static byte[] EncodeFrame(byte[] idx, int width, int height)
	{
		List<byte> outBytes = new List<byte>();
		for (int y = height - 1; y >= 0; y--) {
			int x = 0;
			while (x < width) {
				int start = x;
				if (idx[y * width + x] == 0) {
					while (x < width && idx[y * width + x] == 0 && x - start < 128) x++;
					outBytes.Add((byte)(256 - (x - start)));
				} else {
					while (x < width && idx[y * width + x] != 0 && x - start < 127) x++;
					outBytes.Add((byte)(x - start));
					for (int i = start; i < x; i++) outBytes.Add(idx[y * width + i]);
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
			for (int y = 0; y < height; y++)
				for (int x = 0; x < width; x++) {
					byte i = idx[y * width + x];
					bmp.SetPixel(x, y, i == 0 ? Color.FromArgb(0, 0, 0, 0) : Color.FromArgb(255, pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]));
				}
			bmp.Save(path, ImageFormat.Png);
		}
	}
}
