// Levski's Cube: steadies the foundation (user, 2026-10-01: "now it wobbles around a bit").
//
// The closed-idle loop (frames 0-11) holds its pedestal still; the opening and opened frames (12-25) came from other
// stills and drift by a few pixels. For each frame this finds the shift that lays its pedestal over the first idle frame's (frame 0)
// (rows 150-185, opaque pixels compared), moves the whole frame by it, and - in the stamped variant - replaces everything
// below FoundationCutAt (a level line with a V under the open cube) with idle frame 1's foundation, so the base is the same pixels in every frame.
//
// Usage (PowerShell): Add-Type -Path tools\LevskiCubeSteady.cs -ReferencedAssemblies System.Drawing
//                     [LevskiCubeSteady]::Run(<strip in>, <aligned out>, <aligned+stamped out>)
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class LevskiCubeSteady
{
	const int W = 128, H = 192, Frames = 26;
	const int CompareTop = 150, CompareBottom = 185, MaxShift = 8;
	// Where the idle foundation starts, per column (user, 2026-10-01, red lines drawn on an opened frame): level at row 139
	// out to the open cube's bottom corners, then a V down to row 150 under its front corner at x 60.5 - so the open cube
	// keeps its own bottom edges and everything below them is the idle pedestal. Read off the user's lines at 2.49x zoom.
	const double CutY = 139, VLeftX = 38, VRightX = 88, VTipX = 60.5, VTipY = 150.5;

	public static int FoundationCutAt(int x)
	{
		double cx = x + 0.5;
		if (cx <= VLeftX || cx >= VRightX)
			return (int)CutY;
		double t = cx < VTipX ? (cx - VLeftX) / (VTipX - VLeftX) : (VRightX - cx) / (VRightX - VTipX);
		return (int)Math.Round(CutY + t * (VTipY - CutY));
	}

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

	static int At(int[] strip, int stripWidth, int frame, int x, int y)
	{
		if (x < 0 || x >= W || y < 0 || y >= H)
			return 0;
		return strip[y * stripWidth + frame * W + x];
	}

	static bool Opaque(int argb) { return ((argb >> 24) & 0xFF) > 128; }

	static double Cost(int[] strip, int sw, int frame, int dx, int dy)
	{
		double sum = 0;
		int n = 0;
		for (int y = CompareTop; y <= CompareBottom; y++) {
			for (int x = 0; x < W; x++) {
				int r = At(strip, sw, 0, x, y);
				int f = At(strip, sw, frame, x - dx, y - dy);
				bool ro = Opaque(r), fo = Opaque(f);
				if (!ro && !fo)
					continue;
				n++;
				if (ro != fo) {
					sum += 255; // shape mismatch
					continue;
				}
				sum += Math.Abs(((r >> 16) & 0xFF) - ((f >> 16) & 0xFF)) + Math.Abs(((r >> 8) & 0xFF) - ((f >> 8) & 0xFF)) + Math.Abs((r & 0xFF) - (f & 0xFF));
			}
		}
		return n == 0 ? double.MaxValue : sum / n;
	}

	public static string Run(string input, string alignedOut, string stampedOut)
	{
		int sw;
		int[] strip = Read(input, out sw);
		var aligned = new int[strip.Length];
		var stamped = new int[strip.Length];
		var log = new StringBuilder();
		for (int f = 0; f < Frames; f++) {
			int bestX = 0, bestY = 0;
			double best = Cost(strip, sw, f, 0, 0);
			double atZero = best;
			for (int dy = -MaxShift; dy <= MaxShift; dy++)
				for (int dx = -MaxShift; dx <= MaxShift; dx++) {
					double c = Cost(strip, sw, f, dx, dy);
					if (c < best - 0.01) { best = c; bestX = dx; bestY = dy; }
				}
			if (f < 12) { bestX = 0; bestY = 0; } // the idle loop is the reference and already still
			log.AppendFormat("frame {0,2}: shift ({1,2},{2,2})  cost {3:F1} -> {4:F1}\n", f, bestX, bestY, atZero, best);
			for (int y = 0; y < H; y++)
				for (int x = 0; x < W; x++) {
					int p = At(strip, sw, f, x - bestX, y - bestY);
					aligned[y * sw + f * W + x] = p;
					stamped[y * sw + f * W + x] = y >= FoundationCutAt(x) ?At(strip, sw, 0, x, y) : p;
				}
		}
		Write(alignedOut, aligned, sw, H);
		Write(stampedOut, stamped, sw, H);
		return log.ToString();
	}
}
