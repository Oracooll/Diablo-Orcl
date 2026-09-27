# Builds one animated GIF per original missile graphic - the spell animations - into
# Resources\01. Blizzard Assets\Spells Animations, named after the spells that use them, so a new
# skill's visual can be picked by eye (user, 2026-09-11).
#
# Reads the sheets tools\oracool_art_export.exe already wrote to 01. Blizzard Assets\Animated Items 2 (a row
# per direction, frames left to right) and the game's own tables - Source\spelldat.cpp (spell ->
# missiles), Source\misdat.cpp (missile -> graphic, graphic -> frame width, directions, frame count and
# frame delay) - so the names and the timing are the game's. No art is read from or written into the
# repository: the output stays beside the other extracted, copyrighted material.
#
#   powershell -ExecutionPolicy Bypass -File tools\BuildSpellAnimationGifs.ps1
param(
	[string]$Repo = (Split-Path -Parent $PSScriptRoot),
	[string]$Art = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Art) { $Art = Join-Path (Split-Path -Parent $Repo) 'Resources\01. Blizzard Assets' }
$Sheets = Join-Path $Art 'Animated Items 2' # the exporter's missiles/, renamed by the user
$Out = Join-Path $Art 'Spells Animations'
New-Item -ItemType Directory -Force $Out | Out-Null

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class SpellGif
{
	// One GIF from one sheet: every direction in a grid, each looping its own length.
	public static int Make(string png, string gif, int frameWidth, int dirs, int[] lens, int delayCs, int scale, int background)
	{
		int[] src;
		int sheetWidth, sheetHeight;
		using (Bitmap bmp = new Bitmap(png))
		{
			sheetWidth = bmp.Width;
			sheetHeight = bmp.Height;
			src = new int[sheetWidth * sheetHeight];
			BitmapData data = bmp.LockBits(new Rectangle(0, 0, sheetWidth, sheetHeight), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
			for (int y = 0; y < sheetHeight; y++)
				Marshal.Copy(new IntPtr(data.Scan0.ToInt64() + (long)y * data.Stride), src, y * sheetWidth, sheetWidth);
			bmp.UnlockBits(data);
		}
		int frameHeight = sheetHeight / dirs;
		int columnsInSheet = sheetWidth / frameWidth;
		int cols = dirs == 16 ? 4 : (dirs >= 9 ? 3 : Math.Min(dirs, 4));
		int rows = (dirs + cols - 1) / cols;
		int cellW = frameWidth * scale, cellH = frameHeight * scale;
		int w = cols * cellW, h = rows * cellH;

		int[] len = new int[dirs];
		int maxLen = 1;
		for (int d = 0; d < dirs; d++)
		{
			int l = d < lens.Length ? lens[d] : 0;
			if (l <= 0 || l > columnsInSheet) l = columnsInSheet;
			len[d] = l;
			maxLen = Math.Max(maxLen, l);
		}

		List<int> palette = new List<int>();
		Dictionary<int, int> index = new Dictionary<int, int>();
		palette.Add(background & 0xFFFFFF);
		index[background & 0xFFFFFF] = 0;
		List<byte[]> frames = new List<byte[]>();
		for (int t = 0; t < maxLen; t++)
		{
			byte[] f = new byte[w * h];
			for (int d = 0; d < dirs; d++)
			{
				int fi = t % len[d];
				int cx = (d % cols) * cellW, cy = (d / cols) * cellH;
				for (int y = 0; y < frameHeight; y++)
				{
					for (int x = 0; x < frameWidth; x++)
					{
						int argb = src[(d * frameHeight + y) * sheetWidth + fi * frameWidth + x];
						if (((argb >> 24) & 0xFF) < 128)
							continue;
						int rgb = argb & 0xFFFFFF;
						int k;
						if (!index.TryGetValue(rgb, out k))
						{
							if (palette.Count < 256)
							{
								k = palette.Count;
								palette.Add(rgb);
								index[rgb] = k;
							}
							else
							{
								k = Nearest(palette, rgb);
							}
						}
						for (int sy = 0; sy < scale; sy++)
							for (int sx = 0; sx < scale; sx++)
								f[(cy + y * scale + sy) * w + cx + x * scale + sx] = (byte)k;
					}
				}
			}
			frames.Add(f);
		}
		Write(gif, w, h, frames, palette, delayCs);
		return maxLen;
	}

	static int Nearest(List<int> palette, int rgb)
	{
		int best = 0, bestD = int.MaxValue;
		int r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
		for (int i = 0; i < palette.Count; i++)
		{
			int c = palette[i];
			int dr = ((c >> 16) & 0xFF) - r, dg = ((c >> 8) & 0xFF) - g, db = (c & 0xFF) - b;
			int dist = dr * dr + dg * dg + db * db;
			if (dist < bestD) { bestD = dist; best = i; }
		}
		return best;
	}

	static void Write(string path, int w, int h, List<byte[]> frames, List<int> palette, int delayCs)
	{
		int bits = 1;
		while ((1 << bits) < palette.Count) bits++;
		int tableSize = 1 << bits;
		using (FileStream fs = new FileStream(path, FileMode.Create))
		using (BinaryWriter bw = new BinaryWriter(fs))
		{
			bw.Write(new byte[] { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61 }); // GIF89a
			bw.Write((ushort)w);
			bw.Write((ushort)h);
			bw.Write((byte)(0x80 | (7 << 4) | (bits - 1)));
			bw.Write((byte)0);
			bw.Write((byte)0);
			for (int i = 0; i < tableSize; i++)
			{
				int c = i < palette.Count ? palette[i] : 0;
				bw.Write((byte)((c >> 16) & 0xFF));
				bw.Write((byte)((c >> 8) & 0xFF));
				bw.Write((byte)(c & 0xFF));
			}
			bw.Write(new byte[] { 0x21, 0xFF, 0x0B });
			bw.Write(System.Text.Encoding.ASCII.GetBytes("NETSCAPE2.0"));
			bw.Write(new byte[] { 0x03, 0x01, 0x00, 0x00, 0x00 }); // loop forever
			int minCode = Math.Max(2, bits);
			foreach (byte[] f in frames)
			{
				bw.Write(new byte[] { 0x21, 0xF9, 0x04, 0x04 }); // graphic control: do not dispose, opaque
				bw.Write((ushort)delayCs);
				bw.Write((byte)0);
				bw.Write((byte)0);
				bw.Write((byte)0x2C);
				bw.Write((ushort)0);
				bw.Write((ushort)0);
				bw.Write((ushort)w);
				bw.Write((ushort)h);
				bw.Write((byte)0);
				bw.Write((byte)minCode);
				byte[] lzw = Lzw(f, minCode);
				for (int p = 0; p < lzw.Length; p += 255)
				{
					int n = Math.Min(255, lzw.Length - p);
					bw.Write((byte)n);
					bw.Write(lzw, p, n);
				}
				bw.Write((byte)0);
			}
			bw.Write((byte)0x3B);
		}
	}

	static byte[] Lzw(byte[] px, int minCode)
	{
		List<byte> output = new List<byte>();
		int acc = 0, nbits = 0;
		int clear = 1 << minCode, eoi = clear + 1;
		int codeSize = minCode + 1, next = eoi + 1;
		Dictionary<int, int> dict = new Dictionary<int, int>();
		Action<int> emit = null;
		emit = delegate(int code)
		{
			acc |= code << nbits;
			nbits += codeSize;
			while (nbits >= 8)
			{
				output.Add((byte)(acc & 0xFF));
				acc >>= 8;
				nbits -= 8;
			}
		};
		emit(clear);
		int prefix = px[0];
		for (int i = 1; i < px.Length; i++)
		{
			int k = px[i];
			int key = (prefix << 8) | k;
			int code;
			if (dict.TryGetValue(key, out code))
			{
				prefix = code;
				continue;
			}
			emit(prefix);
			if (next < 4096)
			{
				dict[key] = next++;
				if (next > (1 << codeSize) && codeSize < 12)
					codeSize++;
			}
			else
			{
				emit(clear);
				dict.Clear();
				codeSize = minCode + 1;
				next = eoi + 1;
			}
			prefix = k;
		}
		emit(prefix);
		emit(eoi);
		if (nbits > 0)
			output.Add((byte)(acc & 0xFF));
		return output.ToArray();
	}
}
'@

# ---- the game's own tables
$misdat = Get-Content (Join-Path $Repo 'Source\misdat.cpp')
$spelldat = Get-Content (Join-Path $Repo 'Source\spelldat.cpp')

$missileGraphic = @{}
foreach ($l in $misdat) {
	if ($l -match '^\s*/\*(\w+)\*/\s*\{\s*&\w+,\s*&\w+,\s*\w+,\s*\w+,\s*MissileGraphicID::(\w+)') {
		$missileGraphic[$matches[1]] = $matches[2]
	}
}
$graphicMissiles = @{}
foreach ($m in $missileGraphic.Keys) {
	$g = $missileGraphic[$m]
	if (-not $graphicMissiles.ContainsKey($g)) { $graphicMissiles[$g] = New-Object System.Collections.Generic.List[string] }
	$graphicMissiles[$g].Add($m)
}

$graphicSpells = @{}
foreach ($l in $spelldat) {
	if ($l -match '/\*SpellID::(\w+)\*/\s*\{\s*P_\("spell",\s*"([^"]+)"\).*?\{\s*MissileID::(\w+)\s*,\s*MissileID::(\w+)') {
		$name = $matches[2]
		foreach ($mid in @($matches[3], $matches[4])) {
			if ($mid -eq 'Null') { continue }
			$g = $missileGraphic[$mid]
			if (-not $g -or $g -eq 'None') { continue }
			if (-not $graphicSpells.ContainsKey($g)) { $graphicSpells[$g] = New-Object System.Collections.Generic.List[string] }
			if (-not $graphicSpells[$g].Contains($name)) { $graphicSpells[$g].Add($name) }
		}
	}
}

function LensFor([string]$macro) {
	if ($macro -eq 'AnimLen_9_4') { return @(9, 4) }
	if ($macro -eq 'AnimLen_15_14_3') { return @(15, 14, 3) }
	if ($macro -eq 'AnimLen_13_11') { return @(13, 11) }
	if ($macro -eq 'AnimLen_16x8_8') { return @(16, 16, 16, 16, 16, 16, 16, 16, 8) }
	if ($macro -match '^AnimLen_(\d+)$') { return @([int]$matches[1]) * 16 }
	return @()
}
# misdat.cpp's MissileAnimDelays, direction 0: extra ticks per frame. A frame shows (delay + 1) ticks at 20 per second.
$delayTicks = @{ 0 = 0; 1 = 1; 2 = 2; 3 = 0; 4 = 1 }

$rows = @()
foreach ($l in $misdat) {
	if ($l -notmatch '^\s*/\*(\w+)\*/\s*\{\s*\{\},\s*(\d+),\s*(-?\d+),\s*(?:"(\w+)"|\{\}),\s*(\d+),\s*MissileGraphicsFlags::(\w+),\s*(\d+),\s*(AnimLen_\w+)') { continue }
	$graphic = $matches[1]; $width = [int]$matches[2]; $file = $matches[4]; $dirs = [int]$matches[5]
	$flags = $matches[6]; $delayIdx = [int]$matches[7]; $lenMacro = $matches[8]
	if (-not $file -or $width -le 0) { continue }
	$png = Join-Path $Sheets "$file.png"
	if (-not (Test-Path $png)) { continue } # Oracool's own sheets are not original art and are not exported here

	$spells = if ($graphicSpells.ContainsKey($graphic)) { $graphicSpells[$graphic] -join ', ' } else { '' }
	$kind = if ($spells) { 'Spell' } elseif ($flags -eq 'MonsterOwned') { 'Monster' } else { 'Effect' }
	$label = if ($spells) { "Spell - $spells ($graphic, $file)" } else { "$kind - $graphic ($file)" }
	$safe = ($label -replace '[\\/:*?"<>|]', '_')
	if ($safe.Length -gt 150) { $safe = $safe.Substring(0, 147) + '...' }
	$gif = Join-Path $Out "$safe.gif"

	$delayCs = ($delayTicks[$delayIdx] + 1) * 5
	$scale = if ($dirs -eq 1) { 2 } else { 1 }
	[int[]]$lens = LensFor $lenMacro
	$frames = [SpellGif]::Make($png, $gif, $width, $dirs, $lens, $delayCs, $scale, 0x181818)
	$missiles = if ($graphicMissiles.ContainsKey($graphic)) { $graphicMissiles[$graphic] -join ', ' } else { '' }
	$rows += [pscustomobject]@{ Gif = "$safe.gif"; Graphic = $graphic; File = $file; Kind = $kind; Spells = $spells; Missiles = $missiles; Dirs = $dirs; Frames = $frames; FrameMs = $delayCs * 10 }
	Write-Host ("{0,-8} {1}" -f $kind, "$safe.gif")
}

# ---- the index
$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Spell animations')
$md.Add('')
$md.Add('EXTRACTED FROM DIABDAT.MPQ / HELLFIRE.MPQ - COPYRIGHTED. Reference only; never built into a shipped archive.')
$md.Add('')
$md.Add('One looping GIF per original missile graphic, built by `tools\BuildSpellAnimationGifs.ps1` from the sheets in `..\missiles`. Names come from the game''s own tables:')
$md.Add('')
$md.Add('- **Spell - ...**: the player spells whose missile draws with this graphic (Source\spelldat.cpp -> Source\misdat.cpp).')
$md.Add('- **Monster - ...**: a graphic only monsters use.')
$md.Add('- **Effect - ...**: an impact, explosion or effect that a spell or monster spawns in code, not listed on a spell''s row.')
$md.Add('')
$md.Add('A graphic with more than one direction shows every direction at once, in a grid; one with a single direction is drawn at 2x. Each plays at the game''s own speed (20 ticks a second).')
$md.Add('')
$md.Add('| GIF | Graphic | Sheet | Spells | Missiles that use it | Directions | Frames | ms per frame |')
$md.Add('|---|---|---|---|---|---:|---:|---:|')
foreach ($r in ($rows | Sort-Object Kind, Gif)) {
	$md.Add(('| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} |' -f $r.Gif, $r.Graphic, $r.File, $r.Spells, $r.Missiles, $r.Dirs, $r.Frames, $r.FrameMs))
}
[System.IO.File]::WriteAllLines((Join-Path $Out 'README.md'), $md)
Write-Host ("{0} GIFs in {1}" -f $rows.Count, $Out)
