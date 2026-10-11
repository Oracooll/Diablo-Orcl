/**
 * @file oracool/gpu_post.cpp
 *
 * The world's tail passes on the graphics card - see gpu_post.h.
 */
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
#include <cstring>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <fmt/format.h>

#include "oracool/post_fx.h"
#include "utils/log.hpp"
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
cbuffer Params : register(b0)
{
	int2 SrcSize;     // the pre-zoom world
	int2 QuarterSize; // the bloom's quarter buffers
	int2 DstSize;     // the viewport (the zoom's output)
	int Flags;        // 1 bloom, 2 mood, 4 vignette
	int ZoomMode;     // 0 copy, 1 nearest, 2 Scale2x, 3 sharp bilinear
	int BlurAxis;     // 0 along rows, 1 along columns
	int BloomStrength;
	int CasPeak;
	int Pad0;
};

Texture2D<float4> Src : register(t0);
Texture2D<int4> Quarter : register(t1);
Texture2D<int4> Mood : register(t2);
Texture2D<int> VignetteCols : register(t3);
Texture2D<int> VignetteRows : register(t4);
Texture2D<int4> ZoomCols : register(t5);
Texture2D<int4> ZoomRows : register(t6);
Texture2D<int> CasCurve : register(t7);

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
	if ((Flags & 2) != 0)
		c = int3(Mood.Load(int3(c.r, 0, 0)).r, Mood.Load(int3(c.g, 0, 0)).g, Mood.Load(int3(c.b, 0, 0)).b);
	if ((Flags & 4) != 0) {
		const int k = (VignetteCols.Load(int3(p.x, 0, 0)) * VignetteRows.Load(int3(p.y, 0, 0))) >> 8;
		c = (c * k) >> 8;
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
)HLSL";

struct Params {
	int32_t srcW, srcH;
	int32_t quarterW, quarterH;
	int32_t dstW, dstH;
	int32_t flags;
	int32_t zoomMode;
	int32_t blurAxis;
	int32_t bloomStrength;
	int32_t casPeak;
	int32_t pad0;
};
static_assert(sizeof(Params) % 16 == 0, "a constant buffer is whole 16-byte registers");

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
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> bright, blur, composite, zoom, sharpen;
	ComPtr<ID3D11Buffer> params;
	ComPtr<ID3D11RasterizerState> rasterizer;
	Target source, quarterA, quarterB, mid, zoomed, sharpened;
	ComPtr<ID3D11Texture2D> staging;
	int stagingW = 0, stagingH = 0;
	Target moodCurves, vignetteCols, vignetteRows, zoomCols, zoomRows, casCurve;
	int moodFor = -1;
	int vignetteFor[2] = { -1, -1 };
	int zoomColsFor[2] = { -1, -1 };
	int zoomRowsFor[2] = { -1, -1 };
};

Device *Gpu = nullptr;
bool Broken = false;
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
	const HRESULT hr = D3DCompile(ShaderSource, sizeof(ShaderSource) - 1, "gpu_post", nullptr, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors);
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
	if (!MakePixelShader("PSBright", Gpu->bright) || !MakePixelShader("PSBlur", Gpu->blur) || !MakePixelShader("PSComposite", Gpu->composite)
	    || !MakePixelShader("PSZoom", Gpu->zoom) || !MakePixelShader("PSSharpen", Gpu->sharpen))
		return false;
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

/** A table as a one-row integer texture of @p texels, each @p texelBytes wide. */
bool UploadTable(Target &t, const void *values, int texels, int texelBytes, DXGI_FORMAT format)
{
	if (!Ensure(t, texels, 1, format, D3D11_BIND_SHADER_RESOURCE))
		return false;
	Gpu->context->UpdateSubresource(t.texture.Get(), 0, nullptr, values, static_cast<UINT>(texels * texelBytes), 0);
	return true;
}

void RunPass(ID3D11PixelShader *shader, const Target &target, std::initializer_list<std::pair<UINT, ID3D11ShaderResourceView *>> inputs)
{
	ID3D11DeviceContext *ctx = Gpu->context.Get();
	ID3D11ShaderResourceView *none[8] = {};
	ctx->PSSetShaderResources(0, 8, none);
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
	if (!*sgOptions.Oracool.gpuRenderer || Broken)
		return false;
	return Init();
}

bool GpuWorldPost(const Surface &src, const Surface &dst, const GpuWorldPostSettings &settings)
{
	if (src.isIndexed() || dst.isIndexed() || src.w() <= 0 || src.h() <= 0 || dst.w() <= 0 || dst.h() <= 0)
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

	ID3D11DeviceContext *ctx = Gpu->context.Get();
	// The world, up.
	ctx->UpdateSubresource(Gpu->source.texture.Get(), 0, nullptr, src.at<uint32_t>(0, 0), static_cast<UINT>(src.pitch()), 0);

	Params params {};
	params.srcW = w;
	params.srcH = h;
	params.quarterW = qw;
	params.quarterH = qh;
	params.dstW = dw;
	params.dstH = dh;
	params.flags = (settings.bloom ? 1 : 0) | (settings.mood ? 2 : 0) | (settings.vignette ? 4 : 0);
	params.zoomMode = zoomMode;
	params.bloomStrength = BloomStrengthShared;
	params.casPeak = CasPeakShared;
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->IASetInputLayout(nullptr);
	ctx->VSSetShader(Gpu->vs.Get(), nullptr, 0);
	ctx->RSSetState(Gpu->rasterizer.Get());
	ID3D11Buffer *cbuffer = Gpu->params.Get();
	ctx->PSSetConstantBuffers(0, 1, &cbuffer);
	const auto setParams = [&]() { ctx->UpdateSubresource(Gpu->params.Get(), 0, nullptr, &params, 0, 0); };
	setParams();

	if (settings.bloom) {
		// The bright pass into the quarter buffer, then the two box passes twice (rows, columns, rows, columns).
		RunPass(Gpu->bright.Get(), Gpu->quarterA, { { 0, Gpu->source.srv.Get() } });
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
	    { { 0, Gpu->source.srv.Get() }, { 1, settings.bloom ? Gpu->quarterA.srv.Get() : nullptr }, { 2, settings.mood ? Gpu->moodCurves.srv.Get() : nullptr },
	        { 3, settings.vignette ? Gpu->vignetteCols.srv.Get() : nullptr }, { 4, settings.vignette ? Gpu->vignetteRows.srv.Get() : nullptr } });
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
	ID3D11ShaderResourceView *none[8] = {};
	ctx->PSSetShaderResources(0, 8, none);
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

bool GpuWorldPostAvailable()
{
	return false;
}

bool GpuWorldPost(const Surface &, const Surface &, const GpuWorldPostSettings &)
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
