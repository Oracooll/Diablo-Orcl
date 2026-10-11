/**
 * @file oracool/gpu_post.cpp
 *
 * The world's tail passes on the graphics card - see gpu_post.h.
 */
// d3d11.h brings windows.h: no min and max macros over std::min and std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "oracool/gpu_post.h"

#include <string>

#include "engine/surface.hpp"
#include "options.h"

#if defined(_WIN32) && !defined(USE_SDL1)
#define ORCL_GPU_POST 1
#endif

#ifdef ORCL_GPU_POST
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <fmt/format.h>

#include "levels/gendung.h"
#include "oracool/colour_light.h"
#include "oracool/post_fx.h"
#include "utils/log.hpp"
#endif

// The CPU's share (the shadow tracing, the table packing) runs every frame from the Debug build: optimised even there,
// as colour_light.cpp is.
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

#ifdef ORCL_GPU_POST

namespace {

using Microsoft::WRL::ComPtr;

// The passes, as pixel shaders over a full-screen triangle. Every sum is the CPU pass's own integer sum (post_fx.cpp),
// so the picture comes out the same to the pixel: colours are read back to 0..255 integers, the bloom's quarter
// buffers are 32-bit integer textures, and the zoom axes, vignette axes, mood curves and sharpening curve are the very
// tables the CPU uses, uploaded.
constexpr char ShaderSource[] = R"HLSL(
// Scalars only, so the packing is the C++ struct's own: four to a register.
cbuffer Params : register(b0)
{
	int SrcW, SrcH, QuarterW, QuarterH;   // the pre-zoom world; the bloom's quarter buffers
	int DstW, DstH, Flags, ZoomMode;      // the viewport; 1 bloom 2 mood 4 vignette 8 relief 16 outlines; 0 copy 1 nearest 2 Scale2x 3 sharp
	int BlurAxis, BloomStrength, CasPeak, ReliefShift;
	int ReliefMax, ReliefU0, ReliefV0, TintU0;
	int TintV0, TintLeft, TintRight, TintTop;
	int TintBottom, HazeU0, HazeV0, WaterL;
	int WaterR, WaterT, WaterB, T1;
	int T2, T3, LavaL, LavaR;
	int LavaT, LavaB, Nbx, Nby;
	int P1, P2, SegL, SegR;
	int O0, O1, O2, O3;                   // the hover outlines' colours the relief leaves be
	int O4, O5, TargetW, TargetH;         // ...and the splats' target
};
#define SrcSize int2(SrcW, SrcH)
#define QuarterSize int2(QuarterW, QuarterH)
#define DstSize int2(DstW, DstH)

Texture2D<float4> Src : register(t0);
Texture2D<int4> Quarter : register(t1);
Texture2D<int4> Mood : register(t2);
Texture2D<int> VignetteCols : register(t3);
Texture2D<int> VignetteRows : register(t4);
Texture2D<int4> ZoomCols : register(t5);
Texture2D<int4> ZoomRows : register(t6);
Texture2D<int> CasCurve : register(t7);
Texture2D<float4> Kept : register(t8);     // the world before the sprites (the relief's mask)
Texture2D<int2> ReliefDir : register(t9);  // FIELD_H x FIELD_W: (y, x) -> the light's pull, 8.8
Texture2D<int4> Tints : register(t10);     // FIELD_H x FIELD_W: (y, x) -> r, g, b, 8.8
Texture2D<uint> Liquid : register(t11);    // FIELD_H x FIELD_W: 0 none, 1 lava, 2 water
Texture2D<int> SineTable : register(t12);  // 1024
Texture2D<uint> Strength : register(t13);  // Nbx x Nby: the haze's strength per 8 px block
Texture2D<float4> Outlines : register(t14);// alpha 1 where no outline is

// Stage 3's, declared here so the composite can use them.
SamplerState Linear : register(s0);
Texture2D<float> LightVis : register(t15); // FIELD_H x (FIELD_W * 12): each light's visibility per tile (the CPU's rays)
Texture2D<float4> WideGlow : register(t16);
Texture2D<float4> ShaftGlow : register(t17);
Texture2D<float4> GradeTable : register(t18); // 4 x 1: gain, lift, gamma, (saturation, contrast, black, strength)
Texture2D<float4> Level : register(t19);   // the glow chain's input level

cbuffer Stage3 : register(b1)
{
	float GroundU0, GroundV0, FirstTileX, FirstTileY;
	float FirstBufX, FirstBufY, Seconds;
	int LightCount;
	int HeatCount, ShaftCount;
	float PplStrength, WideStrength;
	float ShaftStrength, SurfaceStrength, HeatAmplitude, Stage3Pad;
	float4 LightA[32]; // x, y, reach, intensity (tiles)
	float4 LightB[32]; // r, g, b, kind
	float4 HeatSrc[16]; // x, y, radius, strength (tiles)
	float4 Shaft[4];   // screen x, y (pixels), strength
};

cbuffer PassParams : register(b2)
{
	float InTexelX, InTexelY, OutW, OutH;
};

struct VsOut {
	float4 pos : SV_Position;
};

VsOut VS(uint id : SV_VertexID)
{
	VsOut o;
	const float2 uv = float2((id << 1) & 2, id & 2);
	o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
	return o;
}

int3 Px(int2 p)
{
	return int3(round(Src.Load(int3(p, 0)).rgb * 255.0));
}

float4 Out(int3 c)
{
	return float4(c / 255.0, 0); // the X byte stays 0, as every CPU write leaves it
}

int4 PSBright(VsOut i) : SV_Target
{
	const int2 q = int2(i.pos.xy);
	int3 sum = 0;
	[unroll] for (int dy = 0; dy < 4; dy++) {
		[unroll] for (int dx = 0; dx < 4; dx++) {
			const int2 p = q * 4 + int2(dx, dy);
			if (p.x >= SrcSize.x || p.y >= SrcSize.y)
				continue;
			const int3 c = Px(p);
			const int luma = (c.r * 77 + c.g * 150 + c.b * 29) >> 8;
			const int t = luma - 176;
			const int k = t <= 0 ? 0 : t * t * 256 / (79 * 79);
			sum += c * k;
		}
	}
	return int4(sum >> 4, 0);
}

int4 PSBlur(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	const int2 stepBy = BlurAxis == 0 ? int2(1, 0) : int2(0, 1);
	const int length = BlurAxis == 0 ? QuarterSize.x : QuarterSize.y;
	const int at = BlurAxis == 0 ? p.x : p.y;
	int3 sum = 0;
	[unroll] for (int k = -3; k <= 3; k++) {
		const int j = at + k;
		if (j < 0 || j >= length)
			continue;
		sum += Quarter.Load(int3(p + stepBy * k, 0)).rgb;
	}
	return int4(sum / 7, 0);
}

float4 PSComposite(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	int3 c = Px(p);
	if ((Flags & 1) != 0) {
		const int qy = (2 * p.y + 1) * 65536 / 8 - 32768;
		const int y0 = qy >> 16;
		const int ya = clamp(y0, 0, QuarterSize.y - 1);
		const int yb = clamp(y0 + 1, 0, QuarterSize.y - 1);
		const int fy = (qy >> 8) & 0xFF;
		const int qx = (2 * p.x + 1) * 65536 / 8 - 32768;
		const int x0 = qx >> 16;
		const int xa = clamp(x0, 0, QuarterSize.x - 1);
		const int xb = clamp(x0 + 1, 0, QuarterSize.x - 1);
		const int fx = (qx >> 8) & 0xFF;
		const int3 va = (Quarter.Load(int3(xa, ya, 0)).rgb * (256 - fy) + Quarter.Load(int3(xa, yb, 0)).rgb * fy) >> 8;
		const int3 vb = (Quarter.Load(int3(xb, ya, 0)).rgb * (256 - fy) + Quarter.Load(int3(xb, yb, 0)).rgb * fy) >> 8;
		const int3 add = (((va * (256 - fx) + vb * fx) >> 8) * BloomStrength) >> 16;
		c = min(c + add, 255);
	}
	if ((Flags & 128) != 0) {
		// Stage 3: the wide glow and the light shafts, added on as the bloom is.
		const float2 uv = (p + 0.5) / float2(SrcSize);
		const float3 glow = WideGlow.SampleLevel(Linear, uv, 0).rgb * WideStrength + ShaftGlow.SampleLevel(Linear, uv, 0).rgb * ShaftStrength;
		c = int3(min(c + glow * 255.0 + 0.5, 255.0));
	}
	if ((Flags & 2) != 0)
		c = int3(Mood.Load(int3(c.r, 0, 0)).r, Mood.Load(int3(c.g, 0, 0)).g, Mood.Load(int3(c.b, 0, 0)).b);
	if ((Flags & 32) != 0) {
		// Stage 3: the filmic grade, after the mood and before the vignette.
		const float4 gain = GradeTable.Load(int3(0, 0, 0)), lift = GradeTable.Load(int3(1, 0, 0));
		const float4 gamma = GradeTable.Load(int3(2, 0, 0)), misc = GradeTable.Load(int3(3, 0, 0));
		const float3 x0 = c / 255.0;
		float3 x = saturate(x0 * gain.rgb + lift.rgb * (1.0 - x0));
		x = pow(max(x, 1e-5), gamma.rgb);
		const float luma = dot(x, float3(0.299, 0.587, 0.114));
		x = lerp(luma.xxx, x, misc.x);
		x = lerp(x, x * x * (3.0 - 2.0 * x), misc.y);
		x = saturate((x - misc.z) / (1.0 - misc.z));
		c = int3(round(saturate(lerp(x0, x, misc.w)) * 255.0));
	}
	if ((Flags & 4) != 0) {
		const int k = (VignetteCols.Load(int3(p.x, 0, 0)) * VignetteRows.Load(int3(p.y, 0, 0))) >> 8;
		c = (c * k) >> 8;
	}
	if ((Flags & 16) != 0) {
		// The deferred monster outlines, on after the mood as the CPU lays them (DrawMonsterWallOutlines).
		const float4 o = Outlines.Load(int3(p, 0));
		if (o.a < 0.5)
			c = int3(round(o.rgb * 255.0));
	}
	return Out(c);
}

int3 Mix(int3 a, int3 b, int w)
{
	return (a * (256 - w) + b * w) >> 8;
}

float4 PSZoom(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	if (ZoomMode == 0)
		return Out(Px(p));
	if (ZoomMode == 1)
		return Out(Px(int2(min(p.x * SrcSize.x / DstSize.x, SrcSize.x - 1), min(p.y * SrcSize.y / DstSize.y, SrcSize.y - 1))));
	if (ZoomMode == 2) {
		// Scale2x / EPX, as ZoomScaleFiltered: each source pixel becomes four.
		const int2 s = p / 2;
		const int3 c0 = Px(s);
		const int3 a = Px(int2(s.x, max(s.y - 1, 0)));              // above
		const int3 b = Px(int2(min(s.x + 1, SrcSize.x - 1), s.y));  // right
		const int3 c = Px(int2(max(s.x - 1, 0), s.y));              // left
		const int3 d = Px(int2(s.x, min(s.y + 1, SrcSize.y - 1)));  // below
		const bool ca = all(c == a), cd = all(c == d), ab = all(a == b), ac = all(a == c), bd = all(b == d), dc = all(d == c), db = all(d == b), ba = all(b == a);
		const int corner = (p.x & 1) + 2 * (p.y & 1);
		if (corner == 0)
			return Out(ca && !cd && !ab ? a : c0);
		if (corner == 1)
			return Out(ab && !ac && !bd ? b : c0);
		if (corner == 2)
			return Out(dc && !db && !ca ? c : c0);
		return Out(bd && !ba && !dc ? d : c0);
	}
	// The sharp bilinear: the CPU's own axis tables.
	const int3 col = ZoomCols.Load(int3(p.x, 0, 0)).xyz;
	const int3 row = ZoomRows.Load(int3(p.y, 0, 0)).xyz;
	const int3 a = Px(int2(col.x, row.x));
	const int3 upper = col.z == 0 ? a : Mix(a, Px(int2(col.y, row.x)), col.z);
	if (row.z == 0)
		return Out(upper);
	const int3 c = Px(int2(col.x, row.y));
	const int3 lower = col.z == 0 ? c : Mix(c, Px(int2(col.y, row.y)), col.z);
	return Out(Mix(upper, lower, row.z));
}

float4 PSSharpen(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	const int3 c = Px(p);
	if (p.x == 0 || p.y == 0 || p.x >= DstSize.x - 1 || p.y >= DstSize.y - 1)
		return Out(c);
	const int3 n = Px(p + int2(0, -1));
	const int3 s = Px(p + int2(0, 1));
	const int3 w = Px(p + int2(-1, 0));
	const int3 e = Px(p + int2(1, 0));
	const int mn = min(min(min(c.g, n.g), min(s.g, w.g)), e.g);
	const int mx = max(max(max(c.g, n.g), max(s.g, w.g)), e.g);
	if (mx == mn || mx == 0)
		return Out(c);
	const int room = mn < 255 - mx ? mn : 255 - mx;
	const int ratio = clamp(room * 256 / mx, 0, 256);
	const int lobe = -((CasCurve.Load(int3(ratio, 0, 0)) * CasPeak) >> 8);
	if (lobe == 0)
		return Out(c);
	const int denominator = 256 + 4 * lobe;
	if (denominator <= 0)
		return Out(c);
	const int rcp = 65536 / denominator;
	const int3 sum = n + s + w + e;
	return Out(clamp(((c * 256 + lobe * sum) * rcp) >> 16, 0, 255));
}
// ----- Stage 2 ----------------------------------------------------------------------------------------------------

float4 PSCopy(VsOut i) : SV_Target
{
	return Out(Px(int2(i.pos.xy)));
}

int Luma(int3 c)
{
	return (c.r * 77 + c.g * 150 + c.b * 29) >> 8;
}

/** colour_light.cpp's ReliefLumaRow for one pixel: -1 where it is the world as kept, or a hover outline's colour. */
int SurfaceLuma(int2 p)
{
	const int3 c = Px(p);
	if (all(c == int3(round(Kept.Load(int3(p, 0)).rgb * 255.0))))
		return -1;
	const int rgb = (c.r << 16) | (c.g << 8) | c.b;
	if (rgb == O0 || rgb == O1 || rgb == O2 || rgb == O3 || rgb == O4 || rgb == O5)
		return -1;
	return Luma(c);
}

/** The bilinear sample of a per-tile field at screen pixel (x, py), as colour_light.cpp's loops take it. */
int4 FieldWeights(int x, int py, int u0, int v0, out int2 a, out int2 b, out int2 c, out int2 d)
{
	const int u = u0 + 2048 * py + 1024 * x;
	const int v = v0 + 2048 * py - 1024 * x;
	const int iu = u >> 16;
	const int iv = v >> 16;
	const int fu = (u >> 8) & 0xFF;
	const int fv = (v >> 8) & 0xFF;
	const int x0 = clamp(iu, 0, FIELD_W - 1), x1 = clamp(iu + 1, 0, FIELD_W - 1);
	const int y0 = clamp(iv, 0, FIELD_H - 1), y1 = clamp(iv + 1, 0, FIELD_H - 1);
	a = int2(y0, x0);
	b = int2(y0, x1);
	c = int2(y1, x0);
	d = int2(y1, x1);
	return int4((256 - fu) * (256 - fv), fu * (256 - fv), (256 - fu) * fv, fu * fv);
}

int2 ReliefDirAt(int x, int py)
{
	int2 a, b, c, d;
	const int4 w = FieldWeights(x, py, ReliefU0, ReliefV0, a, b, c, d);
	return (ReliefDir.Load(int3(a, 0)) * w.x + ReliefDir.Load(int3(b, 0)) * w.y + ReliefDir.Load(int3(c, 0)) * w.z + ReliefDir.Load(int3(d, 0)) * w.w) / 65536;
}

float4 PSRelief(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	const int3 c = Px(p);
	const int l = SurfaceLuma(p);
	if (l < 0)
		return Out(c);
	// The direction joined linearly over 8 px spans from x = 0, as ApplyReliefPlan's loop.
	const int from = (p.x / 8) * 8;
	const int to = min(from + 8, SrcW);
	const int len = to - from;
	const int2 d0 = ReliefDirAt(from, p.y);
	const int2 d1 = ReliefDirAt(to, p.y);
	if (len <= 0 || (all(d0 == 0) && all(d1 == 0)))
		return Out(c);
	const int2 f = (d0 << 8) + (p.x - from) * (((d1 - d0) << 8) / len);
	int left = p.x > 0 ? SurfaceLuma(p - int2(1, 0)) : l;
	int right = p.x + 1 < SrcW ? SurfaceLuma(p + int2(1, 0)) : l;
	int above = SurfaceLuma(int2(p.x, max(p.y - 1, 0)));
	int below = SurfaceLuma(int2(p.x, min(p.y + 1, SrcH - 1)));
	left = left < 0 ? l : left;
	right = right < 0 ? l : right;
	above = above < 0 ? l : above;
	below = below < 0 ? l : below;
	int delta = -(((right - left) * (f.x >> 8) + (below - above) * (f.y >> 8)) >> ReliefShift);
	delta = clamp(delta, -ReliefMax, ReliefMax);
	if (delta == 0)
		return Out(c);
	return Out(min((c * (256 + delta)) >> 8, 255));
}

int3 TintAt(int x, int py)
{
	int2 a, b, c, d;
	const int4 w = FieldWeights(x, py, TintU0, TintV0, a, b, c, d);
	return (Tints.Load(int3(a, 0)).rgb * w.x + Tints.Load(int3(b, 0)).rgb * w.y + Tints.Load(int3(c, 0)).rgb * w.z + Tints.Load(int3(d, 0)).rgb * w.w) >> 16;
}

float4 PSTint(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	const int3 c = Px(p);
	if (p.x < TintLeft || p.x > TintRight || p.y < TintTop || p.y > TintBottom)
		return Out(c);
	// 8 px spans from the rectangle's left edge, as colour_light.cpp's TintRect.
	const int end = TintRight + 1;
	const int from = TintLeft + ((p.x - TintLeft) / 8) * 8;
	const int to = min(from + 8, end);
	const int len = to - from;
	const int3 t0 = TintAt(from, p.y);
	const int3 t1 = TintAt(to, p.y);
	if (len <= 0 || (all(t0 == 256) && all(t1 == 256)) || all(c == 0))
		return Out(c);
	const int3 f = (t0 << 8) + (p.x - from) * (((t1 - t0) << 8) / len);
	return Out(min((c * (f >> 8)) >> 8, 255));
}

int SineAt(uint i)
{
	return SineTable.Load(int3(i & 1023, 0, 0));
}

float4 PSHazeWater(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	const int3 c = Px(p);
	if (p.x < WaterL || p.x > WaterR || p.y < WaterT || p.y > WaterB)
		return Out(c);
	const int u = HazeU0 + 2048 * p.y + 1024 * p.x;
	const int v = HazeV0 + 2048 * p.y - 1024 * p.x;
	const int iu = (u + 0x8000) >> 16;
	const int iv = (v + 0x8000) >> 16;
	if (iu < 0 || iu >= FIELD_W || iv < 0 || iv >= FIELD_H || Liquid.Load(int3(iv, iu, 0)) != 2 || all(c == 0))
		return Out(c);
	const int s = SineAt(asuint((u * 3) >> 7) + asuint(T1)) + SineAt(asuint((v * 5) >> 8) - asuint(T2)) + SineAt(asuint((u + v) >> 7) + asuint(T3));
	const int k = 256 + ((s * 27) >> 10);
	return Out(min((c * k) >> 8, 255));
}

int RowStrength(int bx, int by0, int by1, int fy)
{
	return (int(Strength.Load(int3(bx, by0, 0))) * (8 - fy) + int(Strength.Load(int3(bx, by1, 0))) * fy) / 8;
}

float4 PSHazeLava(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	const int3 c = Px(p);
	if (p.x < LavaL || p.x > LavaR || p.y < LavaT || p.y > LavaB)
		return Out(c);
	const int t = p.y - LavaT - 4;
	const int by0 = t < 0 ? 0 : min(t / 8, Nby - 1);
	const int by1 = min(by0 + 1, Nby - 1);
	const int fy = t < 0 ? 0 : t % 8;
	const int tx = p.x - LavaL - 4;
	const int bx0 = tx < 0 ? 0 : min(tx >> 3, Nbx - 1);
	const int bx1 = min(bx0 + 1, Nbx - 1);
	const int fx = tx < 0 ? 0 : tx & 7;
	const int a = (RowStrength(bx0, by0, by1, fy) * (8 - fx) + RowStrength(bx1, by0, by1, fy) * fx) >> 3;
	if (a == 0)
		return Out(c);
	const int wave = (SineAt(asuint(p.y * 40 + p.x * 6) + asuint(P1)) * 170 + SineAt(asuint(p.y * 67 - p.x * 3) + asuint(P2)) * 86) >> 8;
	const int at = p.x * 256 + ((a * wave * 3) >> 9);
	const int ii = clamp(at >> 8, SegL, SegR);
	const int jj = min(ii + 1, SegR);
	return Out(Mix(Px(int2(ii, p.y)), Px(int2(jj, p.y)), at & 0xFF));
}

struct SplatIn {
	float2 pos : POSITION;
	float4 col : COLOR;
};
struct SplatOut {
	float4 pos : SV_Position;
	float4 col : COLOR;
};

SplatOut VSSplat(SplatIn i)
{
	SplatOut o;
	o.pos = float4((i.pos.x + 0.5) / TargetW * 2.0 - 1.0, 1.0 - (i.pos.y + 0.5) / TargetH * 2.0, 0, 1);
	o.col = i.col;
	return o;
}

float4 PSSplat(SplatOut i) : SV_Target
{
	return i.col; // premultiplied colour, cover in alpha: ONE + INV_SRC_ALPHA (oracool/splats.h)
}

// ----- Stage 3: the effects only the card runs ------------------------------------------------------------------------
// Float maths from here: these have no CPU original to match.


/** The floor tile under pixel @p p's centre, in tiles (the GroundMap's sums). */
float2 Ground(float2 p)
{
	return float2(GroundU0 + p.y / 32.0 + p.x / 64.0, GroundV0 + p.y / 32.0 - p.x / 64.0);
}

/** A tile's centre on screen. */
float2 TileToScreen(float2 t)
{
	const float2 d = t - float2(FirstTileX, FirstTileY);
	return float2(FirstBufX + 32.0 + (d.x - d.y) * 32.0, FirstBufY - 14.5 + (d.x + d.y) * 16.0);
}

float VisTile(int k, int2 t)
{
	if (t.x < 0 || t.y < 0 || t.x >= FIELD_W || t.y >= FIELD_H)
		return 0;
	return LightVis.Load(int3(t.y, k * FIELD_W + t.x, 0));
}

/** Light @p k's visibility at ground @p g: bilinear between tile centres, so a shadow's edge is soft. */
float Visibility(int k, float2 g)
{
	const float2 b = floor(g);
	const float2 f = g - b;
	const int2 t = int2(b);
	return lerp(lerp(VisTile(k, t), VisTile(k, t + int2(1, 0)), f.x), lerp(VisTile(k, t + int2(0, 1)), VisTile(k, t + int2(1, 1)), f.x), f.y);
}

/**
 * Per-pixel lights: each light's round pool, in its colour, stopped softly by walls - the shadows traced once a tile on
 * the CPU (audit round 1: three rays a pixel a light cost an iGPU tens of milliseconds).
 */
float4 PSPerPixelLights(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	const int3 c = Px(p);
	const float2 g = Ground(float2(p));
	float3 sum = 0;
	[loop] for (int k = 0; k < LightCount; k++) {
		const float4 a = LightA[k];
		const float dist = length(g - a.xy);
		if (dist >= a.z)
			continue;
		float w = 1.0 - dist / a.z;
		w *= w;
		sum += LightB[k].rgb * (w * a.w * Visibility(k, g));
	}
	return Out(int3(min(c * (1.0 + PplStrength * sum) + 0.5, 255.0)));
}

/** Lit sprites: the surface's normal from its brightness, lit by each light from where it hangs. */
float SurfaceHeight(int2 p, int fallback)
{
	const int l = SurfaceLuma(clamp(p, int2(0, 0), SrcSize - 1));
	return l < 0 ? fallback : l;
}

float4 PSLitSprites(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	const int3 c = Px(p);
	const int l = SurfaceLuma(p);
	if (l < 0)
		return Out(c);
	// A Sobel two pixels wide: the shape, not the grain.
	const float gx = (SurfaceHeight(p + int2(2, -2), l) + 2 * SurfaceHeight(p + int2(2, 0), l) + SurfaceHeight(p + int2(2, 2), l))
	    - (SurfaceHeight(p + int2(-2, -2), l) + 2 * SurfaceHeight(p + int2(-2, 0), l) + SurfaceHeight(p + int2(-2, 2), l));
	const float gy = (SurfaceHeight(p + int2(-2, 2), l) + 2 * SurfaceHeight(p + int2(0, 2), l) + SurfaceHeight(p + int2(2, 2), l))
	    - (SurfaceHeight(p + int2(-2, -2), l) + 2 * SurfaceHeight(p + int2(0, -2), l) + SurfaceHeight(p + int2(2, -2), l));
	const float3 n = normalize(float3(-gx / 340.0, -gy / 340.0, 1.0));
	const float2 g = Ground(float2(p)); // the ground map's origin is a pixel's centre already
	float delta = 0;
	[loop] for (int k = 0; k < LightCount; k++) {
		const float4 a = LightA[k];
		const float dist = length(g - a.xy);
		if (dist >= a.z || dist < 0.5)
			continue; // a light on this very tile lights its sprite from inside: no direction
		float w = 1.0 - dist / a.z;
		w *= w * a.w;
		const float2 at = TileToScreen(a.xy) - float2(0, 40.0); // lights hang above the floor
		const float3 L = normalize(float3(at - float2(p), 48.0));
		delta += w * (max(dot(n, L), 0.0) - L.z);
	}
	const float f = clamp(1.0 + SurfaceStrength * delta, 0.72, 1.32);
	return Out(int3(min(c * f + 0.5, 255.0)));
}

/** Heat distortion: the picture bent over lava, fire spells and flames. */
float4 PSHeat(VsOut i) : SV_Target
{
	const int2 p = int2(i.pos.xy);
	float m = 0;
	// Heat rises: the lava a little below the pixel counts.
	const int2 lava = int2(floor(Ground(p + float2(0, 14)) + 0.5));
	if ((Flags & 64) != 0 && lava.x >= 0 && lava.y >= 0 && lava.x < FIELD_W && lava.y < FIELD_H && Liquid.Load(int3(lava.y, lava.x, 0)) == 1)
		m = 1.0;
	const float2 g = Ground(p + float2(0, 20));
	[loop] for (int k = 0; k < HeatCount; k++) {
		const float d = length(g - HeatSrc[k].xy);
		if (d < HeatSrc[k].z)
			m = max(m, (1.0 - d / HeatSrc[k].z) * HeatSrc[k].w);
	}
	if (m <= 0.0)
		return Out(Px(p));
	const float2 offset = float2(sin(p.y * 0.21 + Seconds * 5.1 + p.x * 0.05), cos(p.y * 0.17 - Seconds * 3.7 + p.x * 0.11)) * (HeatAmplitude * m);
	return float4(Src.SampleLevel(Linear, (p + 0.5 + offset) / float2(SrcSize), 0).rgb, 0);
}

/** The glow chain: the bright pass at half size, a soft knee. */
float4 PSGlowBright(VsOut i) : SV_Target
{
	const float3 c = Src.SampleLevel(Linear, (i.pos.xy * 2.0) / float2(SrcSize), 0).rgb;
	const float l = dot(c, float3(0.299, 0.587, 0.114));
	return float4(c * smoothstep(0.62, 0.95, l), 1);
}

/** Down a level: four bilinear taps, a 4x4 box. */
float4 PSGlowDown(VsOut i) : SV_Target
{
	const float2 uv = i.pos.xy / float2(OutW, OutH);
	const float2 t = float2(InTexelX, InTexelY);
	return 0.25 * (Level.SampleLevel(Linear, uv + t * float2(-1, -1), 0) + Level.SampleLevel(Linear, uv + t * float2(1, -1), 0)
	    + Level.SampleLevel(Linear, uv + t * float2(-1, 1), 0) + Level.SampleLevel(Linear, uv + t * float2(1, 1), 0));
}

/** Up a level, added on: a tent of nine taps. */
float4 PSGlowUp(VsOut i) : SV_Target
{
	const float2 uv = i.pos.xy / float2(OutW, OutH);
	const float2 t = float2(InTexelX, InTexelY);
	float4 s = Level.SampleLevel(Linear, uv, 0) * 4.0;
	s += (Level.SampleLevel(Linear, uv + t * float2(-1, 0), 0) + Level.SampleLevel(Linear, uv + t * float2(1, 0), 0)
	    + Level.SampleLevel(Linear, uv + t * float2(0, -1), 0) + Level.SampleLevel(Linear, uv + t * float2(0, 1), 0)) * 2.0;
	s += Level.SampleLevel(Linear, uv + t * float2(-1, -1), 0) + Level.SampleLevel(Linear, uv + t * float2(1, -1), 0)
	    + Level.SampleLevel(Linear, uv + t * float2(-1, 1), 0) + Level.SampleLevel(Linear, uv + t * float2(1, 1), 0);
	return s / 16.0;
}

/** Light shafts: the bright pass smeared along the ray to each shaft light, fading. */
float4 PSShafts(VsOut i) : SV_Target
{
	const float2 uv = i.pos.xy / float2(OutW, OutH);
	float3 sum = 0;
	[loop] for (int k = 0; k < ShaftCount; k++) {
		const float2 light = Shaft[k].xy / float2(SrcSize);
		const float2 toLight = light - uv;
		const float reach = length(toLight * float2(SrcSize)) / 360.0;
		if (reach >= 1.0)
			continue;
		// From the light outward: the samples nearest it count most, each further one a little less.
		float3 ray = 0;
		float weight = 1.0;
		[loop] for (int s = 23; s >= 0; s--) {
			ray += Level.SampleLevel(Linear, uv + toLight * (s / 24.0) * 0.9, 0).rgb * weight;
			weight *= 0.9;
		}
		sum += ray * (Shaft[k].z * (1.0 - reach) / 8.0);
	}
	return float4(sum, 1);
}
)HLSL";

struct Params {
	int32_t srcW, srcH, quarterW, quarterH;
	int32_t dstW, dstH, flags, zoomMode;
	int32_t blurAxis, bloomStrength, casPeak, reliefShift;
	int32_t reliefMax, reliefU0, reliefV0, tintU0;
	int32_t tintV0, tintLeft, tintRight, tintTop;
	int32_t tintBottom, hazeU0, hazeV0, waterL;
	int32_t waterR, waterT, waterB, t1;
	int32_t t2, t3, lavaL, lavaR;
	int32_t lavaT, lavaB, nbx, nby;
	int32_t p1, p2, segL, segR;
	int32_t o0, o1, o2, o3;
	int32_t o4, o5, targetW, targetH;
};
static_assert(sizeof(Params) == 48 * 4, "the cbuffer's layout, four scalars to a register");

constexpr int FlagBloom = 1, FlagMood = 2, FlagVignette = 4, FlagRelief = 8, FlagOutlines = 16;
constexpr int FlagGrade = 32, FlagLavaHeat = 64, FlagGlow = 128;

/** Stage 3's cbuffer (b1), the shader's layout: four scalars to a register, then the arrays. */
struct Stage3Params {
	float groundU0, groundV0, firstTileX, firstTileY;
	float firstBufX, firstBufY, seconds;
	int32_t lightCount;
	int32_t heatCount, shaftCount;
	float pplStrength, wideStrength;
	float shaftStrength, surfaceStrength, heatAmplitude, pad;
	float lightA[32][4];
	float lightB[32][4];
	float heat[16][4];
	float shaft[4][4];
};
static_assert(sizeof(Stage3Params) == 16 * 4 + (32 + 32 + 16 + 4) * 16, "the Stage3 cbuffer's layout");

struct PassParams {
	float inTexelX, inTexelY, outW, outH;
};

/** The filmic grade per area: gain, lift, gamma, (saturation, contrast, black point, strength). */
struct Grade {
	float gain[4], lift[4], gamma[4], misc[4];
};
Grade GradeFor(dungeon_type type)
{
	switch (type) {
	case DTYPE_CATHEDRAL: return { { 1.02F, 1.00F, 1.04F, 0 }, { 0.000F, 0.005F, 0.015F, 0 }, { 1.05F, 1.05F, 1.00F, 0 }, { 1.06F, 0.18F, 0.025F, 1 } };
	case DTYPE_CATACOMBS: return { { 1.05F, 1.00F, 0.94F, 0 }, { 0.012F, 0.006F, 0.000F, 0 }, { 1.04F, 1.05F, 1.08F, 0 }, { 1.08F, 0.20F, 0.025F, 1 } };
	case DTYPE_CAVES: return { { 1.00F, 1.03F, 0.95F, 0 }, { 0.000F, 0.010F, 0.004F, 0 }, { 1.06F, 1.03F, 1.08F, 0 }, { 1.07F, 0.18F, 0.030F, 1 } };
	case DTYPE_HELL: return { { 1.07F, 0.98F, 0.95F, 0 }, { 0.015F, 0.000F, 0.000F, 0 }, { 1.02F, 1.06F, 1.08F, 0 }, { 1.10F, 0.22F, 0.030F, 1 } };
	case DTYPE_NEST: return { { 1.03F, 1.05F, 0.92F, 0 }, { 0.006F, 0.012F, 0.000F, 0 }, { 1.05F, 1.02F, 1.10F, 0 }, { 1.08F, 0.20F, 0.030F, 1 } };
	case DTYPE_CRYPT: return { { 1.03F, 0.98F, 1.05F, 0 }, { 0.008F, 0.000F, 0.014F, 0 }, { 1.04F, 1.07F, 1.02F, 0 }, { 1.06F, 0.20F, 0.030F, 1 } };
	default: return { { 1.02F, 1.01F, 0.99F, 0 }, { 0.004F, 0.004F, 0.008F, 0 }, { 1.00F, 1.00F, 1.02F, 0 }, { 1.08F, 0.14F, 0.015F, 1 } }; // town
	}
}

/** A texture with the views a pass needs. */
struct Target {
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11ShaderResourceView> srv;
	ComPtr<ID3D11RenderTargetView> rtv;
	int w = 0, h = 0;
};

struct Device {
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D11VertexShader> vs, splatVs;
	ComPtr<ID3D11PixelShader> bright, blur, composite, zoom, sharpen;
	ComPtr<ID3D11PixelShader> copy, relief, tint, hazeWater, hazeLava, splat;
	ComPtr<ID3D11PixelShader> perPixelLights, litSprites, heat, glowBright, glowDown, glowUp, shafts;
	ComPtr<ID3D11SamplerState> linear;
	ComPtr<ID3D11BlendState> addBlend;
	ComPtr<ID3D11Buffer> stage3Params, passParams;
	Target glow[5], shaftGlow, gradeTable, blocks;
	int gradeFor = -1;
	ComPtr<ID3D11InputLayout> splatLayout;
	ComPtr<ID3D11BlendState> splatBlend;
	ComPtr<ID3D11Buffer> params;
	ComPtr<ID3D11Buffer> splatBuffer;
	size_t splatCapacity = 0;
	ComPtr<ID3D11RasterizerState> rasterizer;
	Target source, quarterA, quarterB, mid, zoomed, sharpened;
	Target work[2], kept, outlines;
	bool keptValid = false;
	ComPtr<ID3D11Texture2D> staging;
	int stagingW = 0, stagingH = 0;
	Target moodCurves, vignetteCols, vignetteRows, zoomCols, zoomRows, casCurve;
	Target reliefDir, tints, liquid, sine, strength;
	int moodFor = -1;
	int vignetteFor[2] = { -1, -1 };
	int zoomColsFor[2] = { -1, -1 };
	int zoomRowsFor[2] = { -1, -1 };
};

Device *Gpu = nullptr;
bool Broken = false;
bool OptionWasOn = true;

/**
 * D3DCompile, looked up at run time (audit round 1): linked, d3dcompiler_47.dll is a load-time import, and a Windows
 * without it would not start the game at all instead of falling back to the CPU passes.
 */
pD3DCompile CompileFn = nullptr;
bool CompilerLookedUp = false;

pD3DCompile Compiler()
{
	if (!CompilerLookedUp) {
		CompilerLookedUp = true;
		if (HMODULE dll = LoadLibraryW(L"d3dcompiler_47.dll"); dll != nullptr)
			CompileFn = reinterpret_cast<pD3DCompile>(reinterpret_cast<void *>(GetProcAddress(dll, "D3DCompile")));
	}
	return CompileFn;
}
bool UseWarp = false;
std::string Status;

void Fail(const char *what, HRESULT hr)
{
	Status = fmt::format("{} failed (0x{:08X}) - the CPU passes run instead", what, static_cast<uint32_t>(hr));
	LogError("Graphics card renderer: {}", Status);
	Broken = true;
	delete Gpu;
	Gpu = nullptr;
}

bool Compile(const char *entry, const char *profile, ComPtr<ID3DBlob> &blob)
{
	ComPtr<ID3DBlob> errors;
	const pD3DCompile compile = Compiler();
	if (compile == nullptr) {
		Fail("finding the shader compiler (d3dcompiler_47.dll)", E_NOINTERFACE);
		return false;
	}
	// The dungeon's size, which the per-tile fields are laid out by.
	static const std::string FieldW = std::to_string(MAXDUNX);
	static const std::string FieldH = std::to_string(MAXDUNY);
	const D3D_SHADER_MACRO macros[] = { { "FIELD_W", FieldW.c_str() }, { "FIELD_H", FieldH.c_str() }, { nullptr, nullptr } };
	const HRESULT hr = compile(ShaderSource, sizeof(ShaderSource) - 1, "gpu_post", macros, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors);
	if (FAILED(hr)) {
		if (errors != nullptr)
			LogError("Graphics card renderer: shader {}: {}", entry, static_cast<const char *>(errors->GetBufferPointer()));
		Fail("compiling a shader", hr);
		return false;
	}
	return true;
}

bool MakePixelShader(const char *entry, ComPtr<ID3D11PixelShader> &shader)
{
	ComPtr<ID3DBlob> blob;
	if (!Compile(entry, "ps_4_0", blob))
		return false;
	const HRESULT hr = Gpu->device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
	if (FAILED(hr)) {
		Fail("making a pixel shader", hr);
		return false;
	}
	return true;
}

bool Init()
{
	if (Gpu != nullptr)
		return true;
	if (Broken)
		return false;
	Gpu = new Device();
	const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
	D3D_FEATURE_LEVEL got {};
	HRESULT hr = D3D11CreateDevice(nullptr, UseWarp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 3, D3D11_SDK_VERSION,
	    &Gpu->device, &got, &Gpu->context);
	if (FAILED(hr)) {
		Fail("making the Direct3D 11 device", hr);
		return false;
	}
	ComPtr<ID3DBlob> vsBlob;
	if (!Compile("VS", "vs_4_0", vsBlob))
		return false;
	hr = Gpu->device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &Gpu->vs);
	if (FAILED(hr)) {
		Fail("making the vertex shader", hr);
		return false;
	}
	ComPtr<ID3DBlob> splatVsBlob;
	if (!Compile("VSSplat", "vs_4_0", splatVsBlob))
		return false;
	hr = Gpu->device->CreateVertexShader(splatVsBlob->GetBufferPointer(), splatVsBlob->GetBufferSize(), nullptr, &Gpu->splatVs);
	if (FAILED(hr)) {
		Fail("making the splat vertex shader", hr);
		return false;
	}
	const D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	static_assert(sizeof(Splat) == 24, "a splat is the layout's two elements");
	hr = Gpu->device->CreateInputLayout(layout, 2, splatVsBlob->GetBufferPointer(), splatVsBlob->GetBufferSize(), &Gpu->splatLayout);
	if (FAILED(hr)) {
		Fail("making the splat layout", hr);
		return false;
	}
	if (!MakePixelShader("PSBright", Gpu->bright) || !MakePixelShader("PSBlur", Gpu->blur) || !MakePixelShader("PSComposite", Gpu->composite)
	    || !MakePixelShader("PSZoom", Gpu->zoom) || !MakePixelShader("PSSharpen", Gpu->sharpen) || !MakePixelShader("PSCopy", Gpu->copy)
	    || !MakePixelShader("PSRelief", Gpu->relief) || !MakePixelShader("PSTint", Gpu->tint) || !MakePixelShader("PSHazeWater", Gpu->hazeWater)
	    || !MakePixelShader("PSHazeLava", Gpu->hazeLava) || !MakePixelShader("PSSplat", Gpu->splat))
		return false;
	if (!MakePixelShader("PSPerPixelLights", Gpu->perPixelLights) || !MakePixelShader("PSLitSprites", Gpu->litSprites) || !MakePixelShader("PSHeat", Gpu->heat)
	    || !MakePixelShader("PSGlowBright", Gpu->glowBright) || !MakePixelShader("PSGlowDown", Gpu->glowDown) || !MakePixelShader("PSGlowUp", Gpu->glowUp)
	    || !MakePixelShader("PSShafts", Gpu->shafts))
		return false;
	{
		D3D11_BUFFER_DESC desc {};
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Stage3Params);
		HRESULT made = Gpu->device->CreateBuffer(&desc, nullptr, &Gpu->stage3Params);
		if (SUCCEEDED(made)) {
			desc.ByteWidth = sizeof(PassParams);
			made = Gpu->device->CreateBuffer(&desc, nullptr, &Gpu->passParams);
		}
		if (FAILED(made)) {
			Fail("making Stage 3's constant buffers", made);
			return false;
		}
		D3D11_SAMPLER_DESC sd {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		made = Gpu->device->CreateSamplerState(&sd, &Gpu->linear);
		if (FAILED(made)) {
			Fail("making the linear sampler", made);
			return false;
		}
		D3D11_BLEND_DESC add {};
		add.RenderTarget[0].BlendEnable = TRUE;
		add.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		add.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		add.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		add.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		add.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		add.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		add.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		made = Gpu->device->CreateBlendState(&add, &Gpu->addBlend);
		if (FAILED(made)) {
			Fail("making the additive blend", made);
			return false;
		}
	}
	D3D11_BUFFER_DESC cb {};
	cb.ByteWidth = sizeof(Params);
	cb.Usage = D3D11_USAGE_DEFAULT;
	cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = Gpu->device->CreateBuffer(&cb, nullptr, &Gpu->params);
	if (FAILED(hr)) {
		Fail("making the constant buffer", hr);
		return false;
	}
	D3D11_RASTERIZER_DESC rs {};
	rs.FillMode = D3D11_FILL_SOLID;
	rs.CullMode = D3D11_CULL_NONE;
	rs.DepthClipEnable = TRUE;
	hr = Gpu->device->CreateRasterizerState(&rs, &Gpu->rasterizer);
	if (FAILED(hr)) {
		Fail("making the rasterizer state", hr);
		return false;
	}
	// The splats: colour + under * (1 - cover); the alpha channel left as it is.
	D3D11_BLEND_DESC bd {};
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = Gpu->device->CreateBlendState(&bd, &Gpu->splatBlend);
	if (FAILED(hr)) {
		Fail("making the splat blend", hr);
		return false;
	}
	// The adapter's name, for the log and the debug command.
	ComPtr<IDXGIDevice> dxgi;
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC desc {};
	std::string name = UseWarp ? "WARP (software)" : "unknown adapter";
	if (SUCCEEDED(Gpu->device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&desc))) {
		name.clear();
		for (const wchar_t ch : desc.Description) {
			if (ch == 0)
				break;
			name.push_back(ch < 128 ? static_cast<char>(ch) : '?');
		}
	}
	Status = fmt::format("{} (feature level {}.{})", name, (got >> 12) & 0xF, (got >> 8) & 0xF);
	Log("Graphics card renderer: {}", Status);
	return true;
}

/** (Re)makes @p t at @p w x @p h in @p format, with the views @p bind asks for, when its size differs. */
bool Ensure(Target &t, int w, int h, DXGI_FORMAT format, UINT bind)
{
	if (t.texture != nullptr && t.w == w && t.h == h)
		return true;
	t = Target {};
	D3D11_TEXTURE2D_DESC d {};
	d.Width = static_cast<UINT>(w);
	d.Height = static_cast<UINT>(h);
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = format;
	d.SampleDesc.Count = 1;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = bind;
	HRESULT hr = Gpu->device->CreateTexture2D(&d, nullptr, &t.texture);
	if (FAILED(hr)) {
		Fail("making a texture", hr);
		return false;
	}
	if ((bind & D3D11_BIND_SHADER_RESOURCE) != 0) {
		hr = Gpu->device->CreateShaderResourceView(t.texture.Get(), nullptr, &t.srv);
		if (FAILED(hr)) {
			Fail("making a texture view", hr);
			return false;
		}
	}
	if ((bind & D3D11_BIND_RENDER_TARGET) != 0) {
		hr = Gpu->device->CreateRenderTargetView(t.texture.Get(), nullptr, &t.rtv);
		if (FAILED(hr)) {
			Fail("making a render target", hr);
			return false;
		}
	}
	t.w = w;
	t.h = h;
	return true;
}

/** The most lights Stage 3 lights the view with (audit round 1: 32 cost an iGPU too much). */
constexpr size_t MaxGpuLights = 12;

/** How much of a light at (@p fx, @p fy) reaches tile (@p tx, @p ty): three rays a little apart, each stopped by a wall. */
float TileVisibility(const uint8_t *blocks, float fx, float fy, int tx, int ty)
{
	const int sx = static_cast<int>(std::floor(fx + 0.5F));
	const int sy = static_cast<int>(std::floor(fy + 0.5F));
	const float dx = static_cast<float>(tx) - fx;
	const float dy = static_cast<float>(ty) - fy;
	const float len = std::sqrt(dx * dx + dy * dy);
	const float sideX = len > 1e-4F ? -dy / len * 0.3F : 0.0F;
	const float sideY = len > 1e-4F ? dx / len * 0.3F : 0.0F;
	int open = 0;
	for (int r = -1; r <= 1; r++) {
		const float ax = fx + sideX * static_cast<float>(r);
		const float ay = fy + sideY * static_cast<float>(r);
		bool ray = true;
		for (int s = 1; s < 9; s++) {
			const float t = static_cast<float>(s) / 9.0F;
			const int cx = static_cast<int>(std::floor(ax + (static_cast<float>(tx) - ax) * t + 0.5F));
			const int cy = static_cast<int>(std::floor(ay + (static_cast<float>(ty) - ay) * t + 0.5F));
			if (cx == tx && cy == ty)
				break; // the lit tile is never its own blocker
			if (std::abs(cx - sx) <= 1 && std::abs(cy - sy) <= 1)
				continue; // a light in a wall (a wall torch) lights past the walls round it
			if (cx < 0 || cy < 0 || cx >= MAXDUNX || cy >= MAXDUNY || blocks[static_cast<size_t>(cx) * MAXDUNY + cy] != 0) {
				ray = false;
				break;
			}
		}
		open += ray ? 1 : 0;
	}
	return static_cast<float>(open) / 3.0F;
}

/** A table as a one-row integer texture of @p texels, each @p texelBytes wide. */
bool UploadTable(Target &t, const void *values, int texels, int texelBytes, DXGI_FORMAT format)
{
	if (!Ensure(t, texels, 1, format, D3D11_BIND_SHADER_RESOURCE))
		return false;
	Gpu->context->UpdateSubresource(t.texture.Get(), 0, nullptr, values, static_cast<UINT>(texels * texelBytes), 0);
	return true;
}

/** A per-tile field (FieldW x FieldH, x major) as a FieldH x FieldW texture: texel (y, x). */
bool UploadField(Target &t, const void *values, int texelBytes, DXGI_FORMAT format)
{
	if (!Ensure(t, MAXDUNY, MAXDUNX, format, D3D11_BIND_SHADER_RESOURCE))
		return false;
	Gpu->context->UpdateSubresource(t.texture.Get(), 0, nullptr, values, static_cast<UINT>(MAXDUNY * texelBytes), 0);
	return true;
}

void RunPass(ID3D11PixelShader *shader, const Target &target, std::initializer_list<std::pair<UINT, ID3D11ShaderResourceView *>> inputs)
{
	ID3D11DeviceContext *ctx = Gpu->context.Get();
	ID3D11ShaderResourceView *none[20] = {};
	ctx->PSSetShaderResources(0, 20, none);
	ID3D11RenderTargetView *rtv = target.rtv.Get();
	ctx->OMSetRenderTargets(1, &rtv, nullptr);
	for (const auto &[slot, srv] : inputs)
		ctx->PSSetShaderResources(slot, 1, &srv);
	D3D11_VIEWPORT vp {};
	vp.Width = static_cast<float>(target.w);
	vp.Height = static_cast<float>(target.h);
	vp.MaxDepth = 1.0F;
	ctx->RSSetViewports(1, &vp);
	ctx->PSSetShader(shader, nullptr, 0);
	ctx->Draw(3, 0);
}

} // namespace

bool GpuWorldPostAvailable()
{
	const bool on = *sgOptions.Oracool.gpuRenderer;
	// Switching the option off and on again retries a card that failed (audit round 1: a driver reset left the CPU
	// passes for the rest of the session).
	if (on && !OptionWasOn)
		Broken = false;
	OptionWasOn = on;
	if (!on || Broken)
		return false;
	return Init();
}

void GpuKeepWorld(const Surface &src)
{
	if (Gpu == nullptr || src.isIndexed() || src.w() <= 0 || src.h() <= 0)
		return;
	Gpu->keptValid = false;
	if (!Ensure(Gpu->kept, src.w(), src.h(), DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_SHADER_RESOURCE))
		return;
	Gpu->context->UpdateSubresource(Gpu->kept.texture.Get(), 0, nullptr, src.at<uint32_t>(0, 0), static_cast<UINT>(src.pitch()), 0);
	Gpu->keptValid = true;
}

bool GpuWorldPost(const Surface &src, const Surface &dst, const GpuWorldPostSettings &settings, const GpuStage2 *stage2)
{
	if (src.isIndexed() || dst.isIndexed() || src.w() <= 0 || src.h() <= 0 || dst.w() <= 0 || dst.h() <= 0)
		return false;
	const GpuStage2 none {};
	const GpuStage2 &extra = stage2 != nullptr ? *stage2 : none;
	const bool anyStage3 = extra.stage3 != nullptr
	    && (settings.perPixelLights || settings.litSprites || settings.wideBloom || settings.lightShafts || settings.heatDistortion || settings.filmicGrade);
	const bool anyStage2 = anyStage3 || extra.relief != nullptr || extra.tint != nullptr || (extra.splats != nullptr && !extra.splats->empty())
	    || extra.haze != nullptr || (extra.outlines != nullptr && extra.outlineBox.size.width > 0 && extra.outlineBox.size.height > 0);
	// Nothing the card does better (audit round 1): no pass at all and no filtered zoom - the CPU's copy or nearest zoom
	// costs less than the round trip.
	if (!settings.bloom && !settings.mood && !settings.vignette && settings.zoom != GpuZoom::Filtered && !anyStage2)
		return false;
	if (!Init())
		return false;
	const int w = src.w();
	const int h = src.h();
	const int qw = (w + 3) / 4;
	const int qh = (h + 3) / 4;
	const bool zooming = settings.zoom != GpuZoom::None;
	const int dw = zooming ? dst.w() : w;
	const int dh = zooming ? dst.h() : h;
	if (zooming && (dw < w || dh < h))
		return false; // a zoom only enlarges

	constexpr UINT Both = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	if (!Ensure(Gpu->source, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_SHADER_RESOURCE)
	    || !Ensure(Gpu->mid, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, Both))
		return false;
	if (settings.bloom && (!Ensure(Gpu->quarterA, qw, qh, DXGI_FORMAT_R32G32B32A32_SINT, Both) || !Ensure(Gpu->quarterB, qw, qh, DXGI_FORMAT_R32G32B32A32_SINT, Both)))
		return false;
	if (zooming && (!Ensure(Gpu->zoomed, dw, dh, DXGI_FORMAT_B8G8R8A8_UNORM, Both) || !Ensure(Gpu->sharpened, dw, dh, DXGI_FORMAT_B8G8R8A8_UNORM, Both)))
		return false;
	if (anyStage2 && (!Ensure(Gpu->work[0], w, h, DXGI_FORMAT_B8G8R8A8_UNORM, Both) || !Ensure(Gpu->work[1], w, h, DXGI_FORMAT_B8G8R8A8_UNORM, Both)))
		return false;

	// The tables, rebuilt only when what they depend on moves.
	if (settings.mood && Gpu->moodFor != static_cast<int>(settings.moodType)) {
		std::array<uint8_t, 256> r, g, b;
		MoodCurves(settings.moodType, r.data(), g.data(), b.data());
		std::array<int32_t, 256 * 4> packed {};
		for (size_t i = 0; i < 256; i++) {
			packed[i * 4] = r[i];
			packed[i * 4 + 1] = g[i];
			packed[i * 4 + 2] = b[i];
		}
		if (!UploadTable(Gpu->moodCurves, packed.data(), 256, 16, DXGI_FORMAT_R32G32B32A32_SINT))
			return false;
		Gpu->moodFor = static_cast<int>(settings.moodType);
	}
	if (settings.vignette && (Gpu->vignetteFor[0] != w || Gpu->vignetteFor[1] != h)) {
		std::vector<uint16_t> axis;
		std::vector<int32_t> values;
		VignetteAxis(axis, w);
		values.assign(axis.begin(), axis.end());
		if (!UploadTable(Gpu->vignetteCols, values.data(), w, 4, DXGI_FORMAT_R32_SINT))
			return false;
		VignetteAxis(axis, h);
		values.assign(axis.begin(), axis.end());
		if (!UploadTable(Gpu->vignetteRows, values.data(), h, 4, DXGI_FORMAT_R32_SINT))
			return false;
		Gpu->vignetteFor[0] = w;
		Gpu->vignetteFor[1] = h;
	}
	int zoomMode = 0;
	if (zooming && (dw != w || dh != h)) {
		if (settings.zoom == GpuZoom::Nearest)
			zoomMode = 1;
		else if (w * 2 == dw && h * 2 == dh)
			zoomMode = 2;
		else
			zoomMode = 3;
	}
	if (zoomMode == 3) {
		const auto axisTable = [](Target &t, int from, int to, int (&builtFor)[2]) {
			if (builtFor[0] == from && builtFor[1] == to && t.texture != nullptr)
				return true;
			std::vector<int> i0, i1, wt;
			ZoomAxis(from, to, i0, i1, wt);
			std::vector<int32_t> packed(static_cast<size_t>(to) * 4);
			for (int i = 0; i < to; i++) {
				packed[static_cast<size_t>(i) * 4] = i0[static_cast<size_t>(i)];
				packed[static_cast<size_t>(i) * 4 + 1] = i1[static_cast<size_t>(i)];
				packed[static_cast<size_t>(i) * 4 + 2] = wt[static_cast<size_t>(i)];
			}
			if (!Ensure(t, to, 1, DXGI_FORMAT_R32G32B32A32_SINT, D3D11_BIND_SHADER_RESOURCE))
				return false;
			Gpu->context->UpdateSubresource(t.texture.Get(), 0, nullptr, packed.data(), static_cast<UINT>(to * 16), 0);
			builtFor[0] = from;
			builtFor[1] = to;
			return true;
		};
		if (!axisTable(Gpu->zoomCols, w, dw, Gpu->zoomColsFor) || !axisTable(Gpu->zoomRows, h, dh, Gpu->zoomRowsFor))
			return false;
	}
	const bool sharpen = zooming && settings.sharpen && dw >= 3 && dh >= 3;
	if (sharpen && Gpu->casCurve.texture == nullptr) {
		std::array<int32_t, 257> curve;
		const uint16_t *amplitude = CasAmplitudeTable();
		for (size_t i = 0; i < curve.size(); i++)
			curve[i] = amplitude[i];
		if (!UploadTable(Gpu->casCurve, curve.data(), 257, 4, DXGI_FORMAT_R32_SINT))
			return false;
	}

	Params params {};
	params.srcW = w;
	params.srcH = h;
	params.quarterW = qw;
	params.quarterH = qh;
	params.dstW = dw;
	params.dstH = dh;
	params.flags = (settings.bloom ? FlagBloom : 0) | (settings.mood ? FlagMood : 0) | (settings.vignette ? FlagVignette : 0);
	params.zoomMode = zoomMode;
	params.bloomStrength = BloomStrengthShared;
	params.casPeak = CasPeakShared;
	params.targetW = w;
	params.targetH = h;

	// ----- Stage 2's data ---------------------------------------------------------------------------------------
	const bool relief = extra.relief != nullptr && Gpu->keptValid && Gpu->kept.w == w && Gpu->kept.h == h;
	if (relief) {
		const ReliefPlan &r = *extra.relief;
		if (r.reliefPass) { // Lit Sprites' plan needs only the mask (audit round 1)
			static std::vector<int32_t> field(static_cast<size_t>(MAXDUNX) * MAXDUNY * 2); // reused, not made each frame
			for (size_t i = 0; i < static_cast<size_t>(MAXDUNX) * MAXDUNY; i++) {
				field[i * 2] = r.dirX[i];
				field[i * 2 + 1] = r.dirY[i];
			}
			if (!UploadField(Gpu->reliefDir, field.data(), 8, DXGI_FORMAT_R32G32_SINT))
				return false;
		}
		params.flags |= FlagRelief;
		params.reliefShift = r.slopeShift;
		params.reliefMax = r.maxDelta;
		params.reliefU0 = static_cast<int32_t>(r.u0);
		params.reliefV0 = static_cast<int32_t>(r.v0);
		params.o0 = static_cast<int32_t>(r.outline[0]);
		params.o1 = static_cast<int32_t>(r.outline[1]);
		params.o2 = static_cast<int32_t>(r.outline[2]);
		params.o3 = static_cast<int32_t>(r.outline[3]);
		params.o4 = static_cast<int32_t>(r.outline[4]);
		params.o5 = static_cast<int32_t>(r.outline[5]);
	}
	const bool tint = extra.tint != nullptr && extra.tint->tints != nullptr && extra.tint->left <= extra.tint->right && extra.tint->top <= extra.tint->bottom;
	if (tint) {
		const TintPlan &t = *extra.tint;
		static std::vector<int32_t> field(static_cast<size_t>(MAXDUNX) * MAXDUNY * 4); // reused, not made each frame
		for (size_t i = 0; i < static_cast<size_t>(MAXDUNX) * MAXDUNY; i++) {
			field[i * 4] = t.tints[i].r;
			field[i * 4 + 1] = t.tints[i].g;
			field[i * 4 + 2] = t.tints[i].b;
		}
		if (!UploadField(Gpu->tints, field.data(), 16, DXGI_FORMAT_R32G32B32A32_SINT))
			return false;
		params.tintU0 = static_cast<int32_t>(t.u0);
		params.tintV0 = static_cast<int32_t>(t.v0);
		params.tintLeft = t.left;
		params.tintRight = t.right;
		params.tintTop = t.top;
		params.tintBottom = t.bottom;
	}
	bool liquidSent = false;
	const bool haze = extra.haze != nullptr && (extra.haze->water || extra.haze->lava) && extra.haze->liquid != nullptr;
	if (haze) {
		const HazePlan &z = *extra.haze;
		if (!UploadField(Gpu->liquid, z.liquid, 1, DXGI_FORMAT_R8_UINT))
			return false;
		liquidSent = z.liquid == (extra.stage3 != nullptr ? extra.stage3->liquid : nullptr);
		if (Gpu->sine.texture == nullptr && !UploadTable(Gpu->sine, HazeSineTable(), 1024, 4, DXGI_FORMAT_R32_SINT))
			return false;
		if (z.lava) {
			// Grown to the largest lava box so far, not made again each time the box changes size (audit).
			if (Gpu->strength.texture == nullptr || z.nbx > Gpu->strength.w || z.nby > Gpu->strength.h) {
				const int sw = std::max(z.nbx, Gpu->strength.texture != nullptr ? Gpu->strength.w : 0);
				const int sh = std::max(z.nby, Gpu->strength.texture != nullptr ? Gpu->strength.h : 0);
				Gpu->strength = Target {};
				if (!Ensure(Gpu->strength, sw, sh, DXGI_FORMAT_R8_UINT, D3D11_BIND_SHADER_RESOURCE))
					return false;
			}
			const D3D11_BOX region { 0, 0, 0, static_cast<UINT>(z.nbx), static_cast<UINT>(z.nby), 1 };
			Gpu->context->UpdateSubresource(Gpu->strength.texture.Get(), 0, &region, z.strength, static_cast<UINT>(z.nbx), 0);
		}
		params.hazeU0 = static_cast<int32_t>(z.u0);
		params.hazeV0 = static_cast<int32_t>(z.v0);
		params.waterL = z.water ? z.waterL : 0;
		params.waterR = z.water ? z.waterR : -1;
		params.waterT = z.water ? z.waterT : 0;
		params.waterB = z.water ? z.waterB : -1;
		params.t1 = static_cast<int32_t>(z.t1);
		params.t2 = static_cast<int32_t>(z.t2);
		params.t3 = static_cast<int32_t>(z.t3);
		params.lavaL = z.lava ? z.lavaL : 0;
		params.lavaR = z.lava ? z.lavaR : -1;
		params.lavaT = z.lava ? z.lavaT : 0;
		params.lavaB = z.lava ? z.lavaB : -1;
		params.nbx = z.nbx;
		params.nby = z.nby;
		params.p1 = static_cast<int32_t>(z.p1);
		params.p2 = static_cast<int32_t>(z.p2);
		params.segL = z.segL;
		params.segR = z.segR;
	}
	const bool splats = extra.splats != nullptr && !extra.splats->empty();
	if (splats) {
		const size_t bytes = extra.splats->size() * sizeof(Splat);
		if (Gpu->splatBuffer == nullptr || Gpu->splatCapacity < bytes) {
			Gpu->splatBuffer.Reset();
			D3D11_BUFFER_DESC vb {};
			vb.ByteWidth = static_cast<UINT>(std::max<size_t>(bytes * 2, 64 * 1024));
			vb.Usage = D3D11_USAGE_DYNAMIC;
			vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			const HRESULT hr = Gpu->device->CreateBuffer(&vb, nullptr, &Gpu->splatBuffer);
			if (FAILED(hr)) {
				Fail("making the splat buffer", hr);
				return false;
			}
			Gpu->splatCapacity = vb.ByteWidth;
		}
		D3D11_MAPPED_SUBRESOURCE mapped {};
		const HRESULT hr = Gpu->context->Map(Gpu->splatBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(hr)) {
			Fail("filling the splat buffer", hr);
			return false;
		}
		std::memcpy(mapped.pData, extra.splats->data(), bytes);
		Gpu->context->Unmap(Gpu->splatBuffer.Get(), 0);
	}
	const bool outlines = extra.outlines != nullptr && extra.outlineBox.size.width > 0 && extra.outlineBox.size.height > 0;
	if (outlines) {
		if (!Ensure(Gpu->outlines, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, Both))
			return false;
		const float clear[4] = { 0, 0, 0, 1 };
		Gpu->context->ClearRenderTargetView(Gpu->outlines.rtv.Get(), clear);
		const Rectangle &box = extra.outlineBox;
		const D3D11_BOX region { static_cast<UINT>(box.position.x), static_cast<UINT>(box.position.y), 0, static_cast<UINT>(box.position.x + box.size.width),
			static_cast<UINT>(box.position.y + box.size.height), 1 };
		Gpu->context->UpdateSubresource(Gpu->outlines.texture.Get(), 0, &region, extra.outlines->at<uint32_t>(box.position.x, box.position.y),
		    static_cast<UINT>(extra.outlines->pitch()), 0);
		params.flags |= FlagOutlines;
	}

	// ----- Stage 3's data -----------------------------------------------------------------------------------------
	Stage3Params s3 {};
	bool perPixelLights = false, litSprites = false, heatPass = false, wideBloom = false, shafts = false;
	if (anyStage3) {
		const GpuStage3 &in = *extra.stage3;
		const double cx = in.firstTileBuffer.x + 32.0, cy = in.firstTileBuffer.y - 14.5;
		s3.groundU0 = static_cast<float>(in.firstTile.x + (0.5 - cx) / 64.0 + (0.5 - cy) / 32.0);
		s3.groundV0 = static_cast<float>(in.firstTile.y + (0.5 - cy) / 32.0 - (0.5 - cx) / 64.0);
		s3.firstTileX = static_cast<float>(in.firstTile.x);
		s3.firstTileY = static_cast<float>(in.firstTile.y);
		s3.firstBufX = static_cast<float>(in.firstTileBuffer.x);
		s3.firstBufY = static_cast<float>(in.firstTileBuffer.y);
		s3.seconds = in.seconds;
		s3.pplStrength = 0.5F;
		// The chain adds its five levels together: a fifth of what a single level would take (audit round 1: big fires
		// went white).
		s3.wideStrength = settings.wideBloom ? 0.06F : 0.0F;
		s3.shaftStrength = settings.lightShafts ? 0.35F : 0.0F;
		s3.surfaceStrength = 0.55F;
		s3.heatAmplitude = 1.6F;
		// The lights that reach the view at all (audit round 1): the ground the view's corners stand on, as a box.
		float minU = 1e9F, maxU = -1e9F, minV = 1e9F, maxV = -1e9F;
		for (const float px : { 0.0F, static_cast<float>(w) }) {
			for (const float py : { 0.0F, static_cast<float>(h) + 64.0F }) {
				const float u = s3.groundU0 + py / 32.0F + px / 64.0F;
				const float v = s3.groundV0 + py / 32.0F - px / 64.0F;
				minU = std::min(minU, u), maxU = std::max(maxU, u), minV = std::min(minV, v), maxV = std::max(maxV, v);
			}
		}
		size_t lights = 0;
		std::array<const GpuLight *, MaxGpuLights> kept {};
		for (size_t k = 0; k < in.lightCount && lights < MaxGpuLights; k++) {
			const GpuLight &l = in.lights[k];
			if (l.x + l.reach < minU || l.x - l.reach > maxU || l.y + l.reach < minV || l.y - l.reach > maxV)
				continue;
			kept[lights] = &l;
			s3.lightA[lights][0] = l.x, s3.lightA[lights][1] = l.y, s3.lightA[lights][2] = l.reach, s3.lightA[lights][3] = l.intensity;
			s3.lightB[lights][0] = l.r, s3.lightB[lights][1] = l.g, s3.lightB[lights][2] = l.b, s3.lightB[lights][3] = static_cast<float>(l.kind);
			lights++;
		}
		s3.lightCount = static_cast<int32_t>(lights);
		const size_t heats = std::min<size_t>(in.heatCount, 16);
		for (size_t k = 0; k < heats; k++) {
			const GpuHeat &h3 = in.heat[k];
			s3.heat[k][0] = h3.x, s3.heat[k][1] = h3.y, s3.heat[k][2] = h3.radius, s3.heat[k][3] = h3.strength;
		}
		s3.heatCount = static_cast<int32_t>(heats);
		// The shafts: the flames' and spells' lights nearest the middle of the view, on screen or just off it.
		int shaftCount = 0;
		for (size_t k = 0; k < lights && shaftCount < 4; k++) {
			const GpuLight &l = *kept[k];
			if (l.kind == 0)
				continue;
			const float dx = l.x - s3.firstTileX, dy = l.y - s3.firstTileY;
			const float sx = s3.firstBufX + 32.0F + (dx - dy) * 32.0F;
			const float sy = s3.firstBufY - 14.5F + (dx + dy) * 16.0F - 30.0F;
			if (sx < -64.0F || sy < -64.0F || sx > w + 64.0F || sy > h + 64.0F)
				continue;
			s3.shaft[shaftCount][0] = sx, s3.shaft[shaftCount][1] = sy, s3.shaft[shaftCount][2] = l.kind == 1 ? 0.6F : 0.9F;
			shaftCount++;
		}
		s3.shaftCount = shaftCount;
		perPixelLights = settings.perPixelLights && lights > 0 && in.blocks != nullptr;
		litSprites = settings.litSprites && relief && lights > 0;
		heatPass = settings.heatDistortion && (heats > 0 || in.liquid != nullptr);
		wideBloom = settings.wideBloom;
		shafts = settings.lightShafts && shaftCount > 0;
		if (perPixelLights) {
			// Each light's shadows, once a tile: three rays a little apart, each stopped by the first wall.
			static std::vector<uint8_t> vis(static_cast<size_t>(MAXDUNX) * MAXDUNY * MaxGpuLights);
			std::fill(vis.begin(), vis.begin() + static_cast<ptrdiff_t>(lights * MAXDUNX * MAXDUNY), 0);
			for (size_t k = 0; k < lights; k++) {
				const GpuLight &l = *kept[k];
				// Its reach, clipped to the ground the view shows (audit round 2: the whole circle traced every frame).
				const int x0 = std::max({ static_cast<int>(std::floor(l.x - l.reach)) - 1, static_cast<int>(std::floor(minU)) - 1, 0 });
				const int x1 = std::min({ static_cast<int>(std::ceil(l.x + l.reach)) + 1, static_cast<int>(std::ceil(maxU)) + 1, MAXDUNX - 1 });
				const int y0 = std::max({ static_cast<int>(std::floor(l.y - l.reach)) - 1, static_cast<int>(std::floor(minV)) - 1, 0 });
				const int y1 = std::min({ static_cast<int>(std::ceil(l.y + l.reach)) + 1, static_cast<int>(std::ceil(maxV)) + 1, MAXDUNY - 1 });
				for (int x = x0; x <= x1; x++)
					for (int y = y0; y <= y1; y++)
						vis[(k * MAXDUNX + static_cast<size_t>(x)) * MAXDUNY + static_cast<size_t>(y)] = static_cast<uint8_t>(TileVisibility(in.blocks, l.x, l.y, x, y) * 255.0F + 0.5F);
			}
			if (!Ensure(Gpu->blocks, MAXDUNY, MAXDUNX * static_cast<int>(MaxGpuLights), DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE))
				return false;
			// Only the rows of the lights in use.
			const D3D11_BOX rows { 0, 0, 0, static_cast<UINT>(MAXDUNY), static_cast<UINT>(std::max<size_t>(lights, 1) * MAXDUNX), 1 };
			Gpu->context->UpdateSubresource(Gpu->blocks.texture.Get(), 0, &rows, vis.data(), MAXDUNY, 0);
		}
		if (heatPass && in.liquid != nullptr) {
			if (!liquidSent && !UploadField(Gpu->liquid, in.liquid, 1, DXGI_FORMAT_R8_UINT)) // once a frame (audit round 1)
				return false;
			params.flags |= FlagLavaHeat;
		}
		if (settings.filmicGrade) {
			if (Gpu->gradeFor != static_cast<int>(settings.moodType)) {
				const Grade grade = GradeFor(settings.moodType);
				if (!UploadTable(Gpu->gradeTable, &grade, 4, 16, DXGI_FORMAT_R32G32B32A32_FLOAT))
					return false;
				Gpu->gradeFor = static_cast<int>(settings.moodType);
			}
			params.flags |= FlagGrade;
		}
		if (wideBloom || shafts) {
			int lw = (w + 1) / 2, lh = (h + 1) / 2;
			for (Target &level : Gpu->glow) {
				if (!Ensure(level, std::max(lw, 1), std::max(lh, 1), DXGI_FORMAT_R16G16B16A16_FLOAT, Both))
					return false;
				lw = (lw + 1) / 2;
				lh = (lh + 1) / 2;
			}
			if (shafts && !Ensure(Gpu->shaftGlow, Gpu->glow[0].w, Gpu->glow[0].h, DXGI_FORMAT_R16G16B16A16_FLOAT, Both))
				return false;
			params.flags |= FlagGlow;
		}
	}

	ID3D11DeviceContext *ctx = Gpu->context.Get();
	// The world, up.
	ctx->UpdateSubresource(Gpu->source.texture.Get(), 0, nullptr, src.at<uint32_t>(0, 0), static_cast<UINT>(src.pitch()), 0);
	Gpu->keptValid = false; // one frame's

	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->IASetInputLayout(nullptr);
	ctx->VSSetShader(Gpu->vs.Get(), nullptr, 0);
	ctx->RSSetState(Gpu->rasterizer.Get());
	ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	ID3D11Buffer *cbuffer = Gpu->params.Get();
	ctx->PSSetConstantBuffers(0, 1, &cbuffer);
	ctx->VSSetConstantBuffers(0, 1, &cbuffer);
	ID3D11Buffer *stage3Buffers[2] = { Gpu->stage3Params.Get(), Gpu->passParams.Get() };
	ctx->PSSetConstantBuffers(1, 2, stage3Buffers);
	ID3D11SamplerState *sampler = Gpu->linear.Get();
	ctx->PSSetSamplers(0, 1, &sampler);
	if (anyStage3)
		ctx->UpdateSubresource(Gpu->stage3Params.Get(), 0, nullptr, &s3, 0, 0);
	const auto setParams = [&]() { ctx->UpdateSubresource(Gpu->params.Get(), 0, nullptr, &params, 0, 0); };
	setParams();

	// ----- Stage 2: relief, tint, splats, water, lava - each into the other work target ------------------------------
	const Target *current = &Gpu->source;
	int next = 0;
	const auto step = [&](ID3D11PixelShader *shader, std::initializer_list<std::pair<UINT, ID3D11ShaderResourceView *>> more) {
		Target &out = Gpu->work[next];
		next ^= 1;
		ID3D11DeviceContext *c = Gpu->context.Get();
		ID3D11ShaderResourceView *clearViews[20] = {};
		c->PSSetShaderResources(0, 20, clearViews);
		ID3D11RenderTargetView *rtv = out.rtv.Get();
		c->OMSetRenderTargets(1, &rtv, nullptr);
		ID3D11ShaderResourceView *srcView = current->srv.Get();
		c->PSSetShaderResources(0, 1, &srcView);
		for (const auto &[slot, srv] : more)
			c->PSSetShaderResources(slot, 1, &srv);
		D3D11_VIEWPORT vp {};
		vp.Width = static_cast<float>(out.w);
		vp.Height = static_cast<float>(out.h);
		vp.MaxDepth = 1.0F;
		c->RSSetViewports(1, &vp);
		c->PSSetShader(shader, nullptr, 0);
		c->Draw(3, 0);
		current = &out;
	};
	if (litSprites)
		step(Gpu->litSprites.Get(), { { 8, Gpu->kept.srv.Get() } }); // Stage 3, in place of the relief
	else if (relief && extra.relief->reliefPass)
		step(Gpu->relief.Get(), { { 8, Gpu->kept.srv.Get() }, { 9, Gpu->reliefDir.srv.Get() } });
	if (tint)
		step(Gpu->tint.Get(), { { 10, Gpu->tints.srv.Get() } });
	if (perPixelLights)
		step(Gpu->perPixelLights.Get(), { { 15, Gpu->blocks.srv.Get() } });
	if (splats) {
		if (current == &Gpu->source)
			step(Gpu->copy.Get(), {});
		// Onto the current picture itself, blended, in the CPU's order.
		ID3D11ShaderResourceView *clearViews[20] = {};
		ctx->PSSetShaderResources(0, 20, clearViews);
		ID3D11RenderTargetView *rtv = current->rtv.Get();
		ctx->OMSetRenderTargets(1, &rtv, nullptr);
		D3D11_VIEWPORT vp {};
		vp.Width = static_cast<float>(current->w);
		vp.Height = static_cast<float>(current->h);
		vp.MaxDepth = 1.0F;
		ctx->RSSetViewports(1, &vp);
		const UINT stride = sizeof(Splat), offset = 0;
		ID3D11Buffer *vb = Gpu->splatBuffer.Get();
		ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
		ctx->IASetInputLayout(Gpu->splatLayout.Get());
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		ctx->VSSetShader(Gpu->splatVs.Get(), nullptr, 0);
		ctx->PSSetShader(Gpu->splat.Get(), nullptr, 0);
		ctx->OMSetBlendState(Gpu->splatBlend.Get(), nullptr, 0xFFFFFFFF);
		ctx->Draw(static_cast<UINT>(extra.splats->size()), 0);
		ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->IASetInputLayout(nullptr);
		ID3D11Buffer *noBuffer = nullptr;
		ctx->IASetVertexBuffers(0, 1, &noBuffer, &stride, &offset);
		ctx->VSSetShader(Gpu->vs.Get(), nullptr, 0);
	}
	if (haze && extra.haze->water)
		step(Gpu->hazeWater.Get(), { { 11, Gpu->liquid.srv.Get() }, { 12, Gpu->sine.srv.Get() } });
	if (haze && extra.haze->lava)
		step(Gpu->hazeLava.Get(), { { 12, Gpu->sine.srv.Get() }, { 13, Gpu->strength.srv.Get() } });
	if (heatPass) {
		step(Gpu->heat.Get(), { { 11, (params.flags & FlagLavaHeat) != 0 ? Gpu->liquid.srv.Get() : nullptr } });
	}
	// Stage 3's glow: the bright pass at half size, the shafts from it, then down the chain and back up, added on.
	if (wideBloom || shafts) {
		const auto setPass = [&](float inTexelX, float inTexelY, const Target &out) {
			const PassParams pass { inTexelX, inTexelY, static_cast<float>(out.w), static_cast<float>(out.h) };
			ctx->UpdateSubresource(Gpu->passParams.Get(), 0, nullptr, &pass, 0, 0);
		};
		setPass(0, 0, Gpu->glow[0]);
		RunPass(Gpu->glowBright.Get(), Gpu->glow[0], { { 0, current->srv.Get() } });
		if (shafts) {
			setPass(0, 0, Gpu->shaftGlow);
			RunPass(Gpu->shafts.Get(), Gpu->shaftGlow, { { 19, Gpu->glow[0].srv.Get() } });
		}
		if (wideBloom) {
			for (int k = 1; k < 5; k++) {
				setPass(1.0F / Gpu->glow[k - 1].w, 1.0F / Gpu->glow[k - 1].h, Gpu->glow[k]);
				RunPass(Gpu->glowDown.Get(), Gpu->glow[k], { { 19, Gpu->glow[k - 1].srv.Get() } });
			}
			ctx->OMSetBlendState(Gpu->addBlend.Get(), nullptr, 0xFFFFFFFF);
			for (int k = 3; k >= 0; k--) {
				setPass(1.0F / Gpu->glow[k + 1].w, 1.0F / Gpu->glow[k + 1].h, Gpu->glow[k]);
				RunPass(Gpu->glowUp.Get(), Gpu->glow[k], { { 19, Gpu->glow[k + 1].srv.Get() } });
			}
			ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		}
	}

	// ----- Stage 1: bloom, mood, vignette (and the outlines), then the zoom and the sharpening ---------------------
	if (settings.bloom) {
		// The bright pass into the quarter buffer, then the two box passes twice (rows, columns, rows, columns).
		RunPass(Gpu->bright.Get(), Gpu->quarterA, { { 0, current->srv.Get() } });
		for (int pass = 0; pass < 2; pass++) {
			params.blurAxis = 0;
			setParams();
			RunPass(Gpu->blur.Get(), Gpu->quarterB, { { 1, Gpu->quarterA.srv.Get() } });
			params.blurAxis = 1;
			setParams();
			RunPass(Gpu->blur.Get(), Gpu->quarterA, { { 1, Gpu->quarterB.srv.Get() } });
		}
	}
	RunPass(Gpu->composite.Get(), Gpu->mid,
	    { { 0, current->srv.Get() }, { 1, settings.bloom ? Gpu->quarterA.srv.Get() : nullptr }, { 2, settings.mood ? Gpu->moodCurves.srv.Get() : nullptr },
	        { 3, settings.vignette ? Gpu->vignetteCols.srv.Get() : nullptr }, { 4, settings.vignette ? Gpu->vignetteRows.srv.Get() : nullptr },
	        { 14, outlines ? Gpu->outlines.srv.Get() : nullptr }, { 16, wideBloom ? Gpu->glow[0].srv.Get() : nullptr },
	        { 17, shafts ? Gpu->shaftGlow.srv.Get() : nullptr }, { 18, (params.flags & FlagGrade) != 0 ? Gpu->gradeTable.srv.Get() : nullptr } });
	const Target *last = &Gpu->mid;
	if (zooming) {
		RunPass(Gpu->zoom.Get(), Gpu->zoomed,
		    { { 0, Gpu->mid.srv.Get() }, { 5, zoomMode == 3 ? Gpu->zoomCols.srv.Get() : nullptr }, { 6, zoomMode == 3 ? Gpu->zoomRows.srv.Get() : nullptr } });
		last = &Gpu->zoomed;
		if (sharpen) {
			RunPass(Gpu->sharpen.Get(), Gpu->sharpened, { { 0, Gpu->zoomed.srv.Get() }, { 7, Gpu->casCurve.srv.Get() } });
			last = &Gpu->sharpened;
		}
	}
	ID3D11ShaderResourceView *noViews[20] = {};
	ctx->PSSetShaderResources(0, 20, noViews);
	ctx->OMSetRenderTargets(0, nullptr, nullptr);

	// And back down into the back buffer.
	if (Gpu->staging == nullptr || Gpu->stagingW != last->w || Gpu->stagingH != last->h) {
		Gpu->staging.Reset();
		D3D11_TEXTURE2D_DESC d {};
		d.Width = static_cast<UINT>(last->w);
		d.Height = static_cast<UINT>(last->h);
		d.MipLevels = 1;
		d.ArraySize = 1;
		d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		d.SampleDesc.Count = 1;
		d.Usage = D3D11_USAGE_STAGING;
		d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		const HRESULT hr = Gpu->device->CreateTexture2D(&d, nullptr, &Gpu->staging);
		if (FAILED(hr)) {
			Fail("making the read-back texture", hr);
			return false;
		}
		Gpu->stagingW = last->w;
		Gpu->stagingH = last->h;
	}
	ctx->CopyResource(Gpu->staging.Get(), last->texture.Get());
	D3D11_MAPPED_SUBRESOURCE mapped {};
	const HRESULT hr = ctx->Map(Gpu->staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) {
		Fail("reading the picture back", hr);
		return false;
	}
	const Surface &out = zooming ? dst : src;
	for (int y = 0; y < last->h; y++)
		std::memcpy(out.at<uint32_t>(0, y), static_cast<const uint8_t *>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch, static_cast<size_t>(last->w) * 4);
	ctx->Unmap(Gpu->staging.Get(), 0);
	return true;
}

void ShutdownGpuWorldPost()
{
	delete Gpu;
	Gpu = nullptr;
}

void GpuWorldPostUseSoftwareDeviceForTests()
{
	ShutdownGpuWorldPost();
	Broken = false;
	UseWarp = true;
}

const char *GpuWorldPostStatus()
{
	return Status.c_str();
}

#else // !ORCL_GPU_POST

void GpuKeepWorld(const Surface &)
{
}

bool GpuWorldPostAvailable()
{
	return false;
}

bool GpuWorldPost(const Surface &, const Surface &, const GpuWorldPostSettings &, const GpuStage2 *)
{
	return false;
}

void ShutdownGpuWorldPost()
{
}

void GpuWorldPostUseSoftwareDeviceForTests()
{
}

const char *GpuWorldPostStatus()
{
	return "not available on this platform";
}

#endif

} // namespace devilution::oracool

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
