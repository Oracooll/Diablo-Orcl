// Oracool asset pipeline: builds objects\orclstash.cel - the town Stash Chest as the gold-filled
// sarcophagus (user, 2026-09-08: "take l5sarco-gold.png and use it as stash chest. opening
// animation and reverse for closing").
//
// The source is an INDEXED PNG sheet, 640x96, five 128x96 frames left to right, closed to open,
// carrying the Crypt's palette (it is Hellfire's l5sarco with the bones replaced by coins, so the
// finished sprite goes to the PRIVATE asset folder). The chest stands in town, so each pixel is
// looked up in the sheet's own palette and re-matched against town.pal; index 0 is the sheet's
// transparent colour and stays 0, which the CEL decoder treats as "no pixel".
//
// Five frames out, in the sheet's order: 1 closed, 5 open. The engine steps 1->5 on opening and
// 5->1 on closing (objects.cpp, UpdateStashChestAnimation). No frame-numbering convention is
// borrowed from chest3.cel any more.
//
// The encoder is the one in tools/WaypointCel.cs, which documents the CEL format.
//
// Usage: SarcoCel.exe <sheet.png> <town.pal> <out.cel> [previewDir] [scalePercent]
//   scalePercent: the frame drawn at this size (user, 2026-09-08: "reduce stash size by 30%" - 70).
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

internal static class SarcoCel
{
	private const int SheetFrameWidth = 128;
	private const int SheetFrameHeight = 96;
	private const int FrameCount = 5;
	private static int FrameWidth = SheetFrameWidth;
	private static int FrameHeight = SheetFrameHeight;

	private static int Main(string[] args)
	{
		if (args.Length < 3) {
			Console.Error.WriteLine("Usage: SarcoCel.exe <sheet.png> <town.pal> <out.cel> [previewDir]");
			return 2;
		}
		string sheetPath = args[0];
		byte[] pal = File.ReadAllBytes(args[1]);
		string outPath = args[2];
		string previewDir = args.Length > 3 && args[3] != "-" ? args[3] : null;
		int scalePercent = args.Length > 4 ? int.Parse(args[4]) : 100;
		FrameWidth = (SheetFrameWidth * scalePercent + 50) / 100;
		FrameHeight = (SheetFrameHeight * scalePercent + 50) / 100;
		if (pal.Length != 768) {
			Console.Error.WriteLine("Palette must be exactly 768 bytes, got " + pal.Length);
			return 2;
		}

		byte[][] frames = new byte[FrameCount][];
		using (Bitmap sheet = new Bitmap(sheetPath)) {
			Console.WriteLine("sheet {0}x{1} {2}", sheet.Width, sheet.Height, sheet.PixelFormat);
			if (sheet.Width != SheetFrameWidth * FrameCount || sheet.Height != SheetFrameHeight) {
				Console.Error.WriteLine("Expected a {0}x{1} sheet", SheetFrameWidth * FrameCount, SheetFrameHeight);
				return 2;
			}

			// GDI+ promotes an indexed PNG with a tRNS chunk to 32-bit ARGB on load, so the sheet is
			// read as colours with alpha: the transparent index arrives as alpha 0.
			// Scaled here, in full colour, before the palette match - a resample of palette indices
			// would average numbers that are not colours. Each frame is resampled on its own so the
			// seams between frames cannot bleed into a neighbour.
			int sheetW = FrameWidth * FrameCount, sheetH = FrameHeight;
			byte[] rgba = new byte[sheetW * sheetH * 4];
			using (Bitmap argb = new Bitmap(sheetW, sheetH, PixelFormat.Format32bppArgb)) {
				using (Graphics g = Graphics.FromImage(argb)) {
					g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
					g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
					g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
					for (int f = 0; f < FrameCount; f++)
						g.DrawImage(sheet, new Rectangle(f * FrameWidth, 0, FrameWidth, FrameHeight), new Rectangle(f * SheetFrameWidth, 0, SheetFrameWidth, SheetFrameHeight), GraphicsUnit.Pixel);
				}
				BitmapData data = argb.LockBits(new Rectangle(0, 0, argb.Width, argb.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
				try {
					for (int y = 0; y < argb.Height; y++)
						System.Runtime.InteropServices.Marshal.Copy(data.Scan0 + y * data.Stride, rgba, y * argb.Width * 4, argb.Width * 4);
				} finally {
					argb.UnlockBits(data);
				}
			}

			Dictionary<int, byte> cache = new Dictionary<int, byte>();
			for (int f = 0; f < FrameCount; f++) {
				byte[] frame = new byte[FrameWidth * FrameHeight];
				int opaque = 0;
				for (int y = 0; y < FrameHeight; y++) {
					for (int x = 0; x < FrameWidth; x++) {
						int at = ((y * sheetW) + f * FrameWidth + x) * 4;
						if (rgba[at + 3] < 128)
							continue; // the sheet's transparent index, kept as the CEL's 0
						int src = (rgba[at + 2] << 16) | (rgba[at + 1] << 8) | rgba[at];
						Color c = Color.FromArgb(rgba[at + 2], rgba[at + 1], rgba[at]);
						byte idx;
						if (!cache.TryGetValue(src, out idx)) {
							idx = Nearest(pal, c.R, c.G, c.B);
							cache[src] = idx;
						}
						frame[y * FrameWidth + x] = idx;
						opaque++;
					}
				}
				Console.WriteLine("  frame {0}: {1} opaque px", f + 1, opaque);
				frames[f] = frame;
				if (previewDir != null)
					WritePreview(frame, FrameWidth, FrameHeight, pal, Path.Combine(previewDir, "stash_" + (f + 1) + ".png"));
			}
			Console.WriteLine("  {0} distinct source colours matched", cache.Count);
		}

		byte[] cel = EncodeCel(frames, FrameWidth, FrameHeight);
		Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath)));
		File.WriteAllBytes(outPath, cel);
		Console.WriteLine("wrote {0} ({1} bytes, {2} frames, {3}x{4})", outPath, cel.Length, FrameCount, FrameWidth, FrameHeight);
		Console.WriteLine("REMINDER: OracoolStashChestAnimWidth in Source/objdat.h must read {0}.", FrameWidth);
		return 0;
	}

	/** @brief Nearest town-palette entry by luma-weighted distance, over the whole palette but 0 (the transparent marker). */
	private static byte Nearest(byte[] pal, int r, int g, int b)
	{
		long best = long.MaxValue;
		byte bestIdx = 1;
		for (int i = 1; i < 256; i++) {
			long dr = r - pal[i * 3], dg = g - pal[i * 3 + 1], db = b - pal[i * 3 + 2];
			long d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
			if (d < best) {
				best = d;
				bestIdx = (byte)i;
			}
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
					bmp.SetPixel(x, y, i == 0 ? Color.Transparent : Color.FromArgb(255, pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]));
				}
			}
			bmp.Save(path, ImageFormat.Png);
		}
	}
}
