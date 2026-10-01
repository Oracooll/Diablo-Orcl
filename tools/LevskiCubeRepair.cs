// Levski's Cube: repairs what ChatGPT's stills could not give (user, 2026-10-01).
//
// 1. The opening's left sub-cubes. Opening frames 16-19 (game frames) were drawn with the left pair of floating cubes cut
//    off at a straight edge, x 21 - the pixels were never in the stills or the sheet. Each cut cube is rebuilt from its
//    right-hand twin: the twin's cube pixels (not the purple glow around it) are mirrored and laid where the cut cube was,
//    their inner edge on the cut cube's inner edge and their bottom on its bottom. The closing plays these frames backwards, so
//    it is mended with them.
// 2. The opened loop's tear. Its six frames are six different stills, so the panel, the glow and all four sub-cubes jump
//    from frame to frame. The loop now holds the one whole frame, OpenHoldFrame; the game's own rune pulse, glow pulse and
//    live grid (levski_roar.cpp) keep it alive.
//
// Runs ONCE, after LevskiCubeSteady, on a sheet freshly built from the stills (build_levski_cube_sheet.cmd does exactly
// that). It is not idempotent: run on its own output it mirrors the already rebuilt cubes again.
// Usage (PowerShell): Add-Type -Path tools\LevskiCubeRepair.cs -ReferencedAssemblies System.Drawing
//                     [LevskiCubeRepair]::Run(<strip in>, <strip out>)
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class LevskiCubeRepair
{
	const int W = 128, H = 192;
	const int OpenFirst = 20, OpenLast = 25, OpenHoldFrame = 20;
	const int CutX = 21; // the straight edge the stills cut the left cubes at

	// Per repaired frame: the rows each sub-cube pair lives in, and the column the left cube's window stops at (where the
	// central cube begins - the lower pair sits against it). Read off a 10 px grid of the steadied sheet.
	struct Pair { public int Top, Bottom, LeftWindowEnd, RightWindowStart; }
	static readonly int[] Frames = { 16, 17, 18, 19 };
	static readonly Pair[][] Pairs = {
		new[] { new Pair { Top = 44, Bottom = 84, LeftWindowEnd = 52, RightWindowStart = 72 }, new Pair { Top = 88, Bottom = 130, LeftWindowEnd = 40, RightWindowStart = 84 } },
		new[] { new Pair { Top = 38, Bottom = 82, LeftWindowEnd = 50, RightWindowStart = 74 }, new Pair { Top = 88, Bottom = 130, LeftWindowEnd = 40, RightWindowStart = 84 } },
		new[] { new Pair { Top = 32, Bottom = 76, LeftWindowEnd = 48, RightWindowStart = 76 }, new Pair { Top = 86, Bottom = 130, LeftWindowEnd = 40, RightWindowStart = 84 } },
		new[] { new Pair { Top = 26, Bottom = 70, LeftWindowEnd = 48, RightWindowStart = 76 }, new Pair { Top = 86, Bottom = 130, LeftWindowEnd = 40, RightWindowStart = 84 } },
	};

	static int[] Read(string path, out int width)
	{
		using (var bmp = new Bitmap(path)) {
			width = bmp.Width;
			var data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
			var px = new int[bmp.Width * bmp.Height];
			for (int y = 0; y < bmp.Height; y++)
				Marshal.Copy(data.Scan0 + y * data.Stride, px, y * bmp.Width, bmp.Width);
			bmp.UnlockBits(data);
			return px;
		}
	}

	static void Write(string path, int[] px, int width, int height)
	{
		using (var bmp = new Bitmap(width, height, PixelFormat.Format32bppArgb)) {
			var data = bmp.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
			for (int y = 0; y < height; y++)
				Marshal.Copy(px, y * width, data.Scan0 + y * data.Stride, width);
			bmp.UnlockBits(data);
			bmp.Save(path, ImageFormat.Png);
		}
	}

	/// <summary>A sub-cube's own pixel: opaque and not the magenta glow (gold frame, blue faces, dark outline).</summary>
	static bool CubePixel(int argb)
	{
		if (((argb >> 24) & 0xFF) <= 128)
			return false;
		int r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF;
		int max = Math.Max(r, Math.Max(g, b));
		if (max < 70)
			return true; // the dark outline and the shadowed faces
		if (r > g && g > b && r > 90 && b < r * 3 / 4)
			return true; // the gold frame
		if (b > g && b > r && r < b * 7 / 10 && g < b * 9 / 10)
			return true; // the blue rune faces (not the magenta glow, which is red as much as blue)
		return false; // glow, the panel, its teal edge and its white lines
	}

	/// <summary>
	/// The biggest 8-connected blob of cube pixels inside the window: one floating sub-cube, without the panel's corner dots
	/// or the bits of the central cube's frame a plain bounding box swept in (which made the mirrored twin too wide).
	/// </summary>
	static Rectangle CubeBlob(int[] s, int sw, int frame, int x0, int x1, int y0, int y1, out bool[] mask)
	{
		int ww = x1 - x0 + 1, wh = y1 - y0 + 1;
		var label = new int[ww * wh];
		int best = 0, bestSize = 0, next = 0;
		var stack = new System.Collections.Generic.Stack<int>();
		for (int i = 0; i < label.Length; i++) {
			if (label[i] != 0 || !CubePixel(s[(y0 + i / ww) * sw + frame * W + x0 + i % ww]))
				continue;
			int id = ++next, size = 0;
			label[i] = id;
			stack.Push(i);
			while (stack.Count > 0) {
				int c = stack.Pop();
				size++;
				int cx = c % ww, cy = c / ww;
				for (int dy = -1; dy <= 1; dy++)
					for (int dx = -1; dx <= 1; dx++) {
						int nx = cx + dx, ny = cy + dy;
						if (nx < 0 || ny < 0 || nx >= ww || ny >= wh)
							continue;
						int n = ny * ww + nx;
						if (label[n] == 0 && CubePixel(s[(y0 + ny) * sw + frame * W + x0 + nx])) {
							label[n] = id;
							stack.Push(n);
						}
					}
			}
			if (size > bestSize) { bestSize = size; best = id; }
		}
		mask = new bool[W * H];
		int l = int.MaxValue, r = -1, t = int.MaxValue, b = -1;
		for (int i = 0; i < label.Length; i++) {
			if (best == 0 || label[i] != best)
				continue;
			int x = x0 + i % ww, y = y0 + i / ww;
			mask[y * W + x] = true;
			if (x < l) l = x;
			if (x > r) r = x;
			if (y < t) t = y;
			if (y > b) b = y;
		}
		return r < 0 ? Rectangle.Empty : Rectangle.FromLTRB(l, t, r + 1, b + 1);
	}

	public static string Run(string input, string output)
	{
		int sw;
		int[] s = Read(input, out sw);
		var log = new StringBuilder();
		for (int i = 0; i < Frames.Length; i++) {
			int f = Frames[i];
			foreach (Pair p in Pairs[i]) {
				bool[] leftMask, rightMask;
				Rectangle left = CubeBlob(s, sw, f, 0, p.LeftWindowEnd - 1, p.Top, p.Bottom, out leftMask);
				Rectangle right = CubeBlob(s, sw, f, p.RightWindowStart, W - 1, p.Top, p.Bottom, out rightMask);
				if (left.IsEmpty || right.IsEmpty) {
					log.AppendFormat("frame {0}: rows {1}-{2} - no pair found, left as it was\n", f, p.Top, p.Bottom);
					continue;
				}
				// Already whole (a frame the stills did not cut): leave it.
				if (left.Left > CutX && left.Width >= right.Width - 1) {
					log.AppendFormat("frame {0}: rows {1}-{2} - left cube whole ({3} wide), kept\n", f, p.Top, p.Bottom, left.Width);
					continue;
				}
				// Inner edge on the cut cube's inner edge, bottom on its bottom.
				int dstRight = left.Right, dstTop = left.Bottom - right.Height; // feet together: the cut blob can run a few rows taller
				// Drawn over the cut cube, not after clearing it: the twin covers it, and clearing punched holes in the panel
				// and the glow behind it. Only what the twin leaves uncovered on its OUTER half - open air, nothing behind - is
				// cleared afterwards (the cut blob runs a few rows taller than the twin).
				var covered = new bool[W * H];
				for (int y = right.Top; y < right.Bottom; y++)
					for (int x = right.Left; x < right.Right; x++) {
						if (!rightMask[y * W + x])
							continue;
						int src = s[y * sw + f * W + x];
						int dx = dstRight - 1 - (x - right.Left);
						int dy = dstTop + (y - right.Top);
						if (dx < 0 || dx >= W || dy < 0 || dy >= H)
							continue;
						s[dy * sw + f * W + dx] = src;
						covered[dy * W + dx] = true;
					}
				int outerHalfEnd = dstRight - right.Width / 2;
				for (int y = left.Top; y < left.Bottom; y++)
					for (int x = left.Left; x < Math.Min(left.Right, outerHalfEnd); x++)
						if (leftMask[y * W + x] && !covered[y * W + x])
							s[y * sw + f * W + x] = 0;
				log.AppendFormat("frame {0}: rows {1}-{2} - left cube {3} rebuilt from right {4}\n", f, p.Top, p.Bottom, left, right);
			}
		}
		for (int f = OpenFirst; f <= OpenLast; f++) {
			if (f == OpenHoldFrame)
				continue;
			for (int y = 0; y < H; y++)
				for (int x = 0; x < W; x++)
					s[y * sw + f * W + x] = s[y * sw + OpenHoldFrame * W + x];
		}
		log.AppendFormat("opened loop: frames {0}-{1} hold frame {2}\n", OpenFirst, OpenLast, OpenHoldFrame);
		Write(output, s, sw, H);
		return log.ToString();
	}
}
