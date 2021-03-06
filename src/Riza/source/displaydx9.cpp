//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2005 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//
//	This file is the DirectX 9 driver for the video display subsystem.
//	It does traditional point sampled and bilinearly filtered upsampling
//	as well as a special multipass algorithm for emulated bicubic
//	filtering.
//

#include <vd2/system/vdtypes.h>

#define DIRECTDRAW_VERSION 0x0900
#define INITGUID
#include <d3d9.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/binary.h>
#include <vd2/system/refcount.h>
#include <vd2/system/math.h>
#include <vd2/system/time.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/text.h>
#include <vd2/Kasumi/region.h>

#include <vd2/Riza/direct3d.h>
#include "displaydrv.h"
#include "displaydrvdx9.h"

namespace {
	#include "displaydx9_shader.inl"
}

#define VDDEBUG_DX9DISP VDDEBUG

#define D3D_DO(x) VDVERIFY(SUCCEEDED(mpD3DDevice->x))

using namespace nsVDD3D9;

bool VDCreateD3D9TextureGeneratorFullSizeRTT(IVDD3D9TextureGenerator **ppGenerator);

///////////////////////////////////////////////////////////////////////////

class VDD3D9TextureGeneratorHEvenOdd : public vdrefcounted<IVDD3D9TextureGenerator> {
public:
	bool GenerateTexture(VDD3D9Manager *pManager, IVDD3D9Texture *pTexture) {
		IDirect3DDevice9 *dev = pManager->GetDevice();
		vdrefptr<IDirect3DTexture9> tex;
		HRESULT hr = dev->CreateTexture(16, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, ~tex, NULL);
		if (FAILED(hr))
			return false;

		D3DLOCKED_RECT lr;
		hr = tex->LockRect(0, &lr, NULL, 0);
		VDASSERT(SUCCEEDED(hr));
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to load horizontal even/odd texture.\n");
			return false;
		}

		for(int i=0; i<16; ++i)
			((uint32 *)lr.pBits)[i] = (uint32)-(sint32)(i&1);

		VDVERIFY(SUCCEEDED(tex->UnlockRect(0)));

		pTexture->SetD3DTexture(tex);
		return true;
	}
};

class VDD3D9TextureGeneratorCubicFilter : public vdrefcounted<IVDD3D9TextureGenerator> {
public:
	VDD3D9TextureGeneratorCubicFilter(IVDVideoDisplayDX9Manager::CubicMode mode) : mCubicMode(mode) {}

	bool GenerateTexture(VDD3D9Manager *pManager, IVDD3D9Texture *pTexture) {
		IDirect3DDevice9 *dev = pManager->GetDevice();
		vdrefptr<IDirect3DTexture9> tex;
		HRESULT hr = dev->CreateTexture(256, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, ~tex, NULL);
		if (FAILED(hr))
			return false;

		D3DLOCKED_RECT lr;
		hr = tex->LockRect(0, &lr, NULL, 0);
		VDASSERT(SUCCEEDED(hr));
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to load cubic filter texture.\n");
			return false;
		}

		MakeCubic4Texture((uint32 *)lr.pBits, lr.Pitch, -0.75, mCubicMode);

		VDVERIFY(SUCCEEDED(tex->UnlockRect(0)));

		pTexture->SetD3DTexture(tex);
		return true;
	}

protected:
	IVDVideoDisplayDX9Manager::CubicMode mCubicMode;

	static void MakeCubic4Texture(uint32 *texture, ptrdiff_t pitch, double A, IVDVideoDisplayDX9Manager::CubicMode mode) {
		int i;

		uint32 *p0 = texture;
		uint32 *p1 = vdptroffset(texture, pitch);
		uint32 *p2 = vdptroffset(texture, pitch*2);
		uint32 *p3 = vdptroffset(texture, pitch*3);

		for(i=0; i<256; i++) {
			double d = (double)(i&63) / 64.0;
			int y1, y2, y3, y4, ydiff;

			// Coefficients for all four pixels *must* add up to 1.0 for
			// consistent unity gain.
			//
			// Two good values for A are -1.0 (original VirtualDub bicubic filter)
			// and -0.75 (closely matches Photoshop).

			double c1 =         +     A*d -       2.0*A*d*d +       A*d*d*d;
			double c2 = + 1.0             -     (A+3.0)*d*d + (A+2.0)*d*d*d;
			double c3 =         -     A*d + (2.0*A+3.0)*d*d - (A+2.0)*d*d*d;
			double c4 =                   +           A*d*d -       A*d*d*d;

			const int maxval = 255;
			double scale = maxval / (c1 + c2 + c3 + c4);

			y1 = (int)floor(0.5 + c1 * scale);
			y2 = (int)floor(0.5 + c2 * scale);
			y3 = (int)floor(0.5 + c3 * scale);
			y4 = (int)floor(0.5 + c4 * scale);

			ydiff = maxval - y1 - y2 - y3 - y4;

			int ywhole = ydiff<0 ? (ydiff-2)/4 : (ydiff+2)/4;
			ydiff -= ywhole*4;

			y1 += ywhole;
			y2 += ywhole;
			y3 += ywhole;
			y4 += ywhole;

			if (ydiff < 0) {
				if (y1<y4)
					y1 += ydiff;
				else
					y4 += ydiff;
			} else if (ydiff > 0) {
				if (y2 > y3)
					y2 += ydiff;
				else
					y3 += ydiff;
			}

			switch(mode) {
			case IVDVideoDisplayDX9Manager::kCubicUsePS1_4Path:
				p0[i] = (-y1 << 24) + (y2 << 16) + (y3 << 8) + (-y4);
				break;
			case IVDVideoDisplayDX9Manager::kCubicUseFF3Path:
				p0[i] = -y1 * 0x020202 + (-y4 << 25);
				p1[i] = y2 * 0x010101 + (y3<<24);

				if (y2 > y3)
					y2 += y3&1;
				else
					y3 += y2&1;

				y2>>=1;
				y3>>=1;

				p2[i] = -y1 * 0x010101 + (-y4 << 24);
				p3[i] = y2 * 0x010101 + (y3<<24);
				break;

			case IVDVideoDisplayDX9Manager::kCubicUsePS1_1Path:
				p0[i] = -y1 * 0x010101 + (-y4 << 24);
				p1[i] = y2 * 0x010101 + (y3<<24);

				p2[i] = -y1 * 0x010101 + (-y4 << 24);
				p3[i] = y2 * 0x010101 + (y3<<24);
				break;

			case IVDVideoDisplayDX9Manager::kCubicUseFF2Path:
				p0[i] = -y1 * 0x010101;
				p1[i] = y2 * 0x010101;

				p2[i] = y3 * 0x010101;
				p3[i] = -y4 * 0x010101;
				break;
			}
		}
	}
};

class VDD3D9TextureGeneratorCubicFilterFF2 : public VDD3D9TextureGeneratorCubicFilter {
public:
	VDD3D9TextureGeneratorCubicFilterFF2() : VDD3D9TextureGeneratorCubicFilter(IVDVideoDisplayDX9Manager::kCubicUseFF2Path) {}
};

class VDD3D9TextureGeneratorCubicFilterFF3 : public VDD3D9TextureGeneratorCubicFilter {
public:
	VDD3D9TextureGeneratorCubicFilterFF3() : VDD3D9TextureGeneratorCubicFilter(IVDVideoDisplayDX9Manager::kCubicUseFF3Path) {}
};

class VDD3D9TextureGeneratorCubicFilterPS1_1 : public VDD3D9TextureGeneratorCubicFilter {
public:
	VDD3D9TextureGeneratorCubicFilterPS1_1() : VDD3D9TextureGeneratorCubicFilter(IVDVideoDisplayDX9Manager::kCubicUsePS1_1Path) {}
};

class VDD3D9TextureGeneratorCubicFilterPS1_4 : public VDD3D9TextureGeneratorCubicFilter {
public:
	VDD3D9TextureGeneratorCubicFilterPS1_4() : VDD3D9TextureGeneratorCubicFilter(IVDVideoDisplayDX9Manager::kCubicUsePS1_4Path) {}
};

///////////////////////////////////////////////////////////////////////////

class VDFontRendererD3D9 : public vdrefcounted<IVDFontRendererD3D9> {
public:
	VDFontRendererD3D9();

	bool Init(VDD3D9Manager *d3dmgr);
	void Shutdown();

	bool Begin();
	void DrawTextLine(int x, int y, uint32 textColor, uint32 outlineColor, const char *s);
	void End();

protected:
	VDD3D9Manager *mpD3DManager;
	vdrefptr<IDirect3DTexture9> mpD3DFontTexture;

	struct GlyphLayoutInfo {
		int		mGlyph;
		float	mX;
	};

	typedef vdfastvector<GlyphLayoutInfo> GlyphLayoutInfos;
	GlyphLayoutInfos mGlyphLayoutInfos;

	struct GlyphInfo {
		vdrect32f	mPos;
		vdrect32f	mUV;
		float		mAdvance;
	};

	GlyphInfo mGlyphInfo[256];
};

bool VDCreateFontRendererD3D9(IVDFontRendererD3D9 **pp) {
	*pp = new_nothrow VDFontRendererD3D9();
	if (*pp)
		(*pp)->AddRef();
	return *pp != NULL;
}

VDFontRendererD3D9::VDFontRendererD3D9()
	: mpD3DManager(NULL)
{
}

bool VDFontRendererD3D9::Init(VDD3D9Manager *d3dmgr) {
	mpD3DManager = d3dmgr;

	vdfastvector<uint32> tempbits(256*256, 0);
	VDPixmap temppx={0};
	temppx.data = tempbits.data();
	temppx.w = 256;
	temppx.h = 256;
	temppx.format = nsVDPixmap::kPixFormat_XRGB8888;
	temppx.pitch = 256*sizeof(uint32);

	VDTextLayoutMetrics metrics;
	VDPixmapPathRasterizer rast;

	VDPixmapRegion outlineRegion;
	VDPixmapRegion charRegion;
	VDPixmapRegion charOutlineRegion;
	VDPixmapCreateRoundRegion(outlineRegion, 16.0f);

	static const float kFontSize = 16.0f;

	int x = 1;
	int y = 1;
	int lineHeight = 0;
	GlyphInfo *pgi = mGlyphInfo;
	for(int c=0; c<256; ++c) {
		char s[2]={(char)c, 0};

		VDPixmapGetTextExtents(NULL, kFontSize, s, metrics);

		if (metrics.mExtents.valid()) {
			int x1 = VDCeilToInt(metrics.mExtents.left * 8.0f - 0.5f);
			int y1 = VDCeilToInt(metrics.mExtents.top * 8.0f - 0.5f);
			int x2 = VDCeilToInt(metrics.mExtents.right * 8.0f - 0.5f);
			int y2 = VDCeilToInt(metrics.mExtents.bottom * 8.0f - 0.5f);
			int ix1 = x1 >> 3;
			int iy1 = y1 >> 3;
			int ix2 = (x2 + 7) >> 3;
			int iy2 = (y2 + 7) >> 3;
			int w = (ix2 - ix1) + 4;
			int h = (iy2 - iy1) + 4;

			if (x + w > 255) {
				x = 1;
				y += lineHeight + 1;
				lineHeight = 0;
			}

			if (lineHeight < h) {
				lineHeight = h;
				VDASSERT(lineHeight+y < 255);
			}

			rast.Clear();
			VDPixmapConvertTextToPath(rast, NULL, kFontSize * 64.0f, (float)(-ix1*64), (float)(-iy1*64), s);
			rast.ScanConvert(charRegion);
			VDPixmapConvolveRegion(charOutlineRegion, charRegion, outlineRegion);

			VDPixmapFillRegionAntialiased8x(temppx, charOutlineRegion, x*8+16, y*8+16, 0x000000FF);
			VDPixmapFillRegionAntialiased8x(temppx, charRegion, x*8+16, y*8+16, 0xFFFFFFFF);

			pgi->mAdvance = metrics.mAdvance;
			pgi->mPos.set((float)(ix1 - 2), (float)(iy1 - 2), (float)(ix2 + 2), (float)(iy2 + 2));
			pgi->mUV.set((float)x, (float)y, (float)(x+w), (float)(y+h));
			pgi->mUV.scale(1.0f / 256.0f, 1.0f / 256.0f);

			x += w+1;
		} else {
			pgi->mAdvance = metrics.mAdvance;
			pgi->mPos.clear();
			pgi->mUV.clear();
		}
		++pgi;
	}

	// create texture
	IDirect3DDevice9 *dev = mpD3DManager->GetDevice();
	HRESULT hr = dev->CreateTexture(256, 256, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, ~mpD3DFontTexture, NULL);
	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create font cache texture.\n");
		Shutdown();
		return false;
	}

	// copy into texture
	D3DLOCKED_RECT lr;
	hr = mpD3DFontTexture->LockRect(0, &lr, NULL, 0);
	VDASSERT(SUCCEEDED(hr));
	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to load font cache texture.\n");
		Shutdown();
		return false;
	}
	
	uint32 *dst = (uint32 *)lr.pBits;
	const uint32 *src = tempbits.data();
	for(int y=0; y<256; ++y) {
		for(int x=0; x<256; ++x) {
			uint32 c = src[x];
			dst[x] = ((c >> 8) & 0xff) * 0x010101 + (c << 24);
		}

		src += 256;
		vdptrstep(dst, lr.Pitch);
	}

	VDVERIFY(SUCCEEDED(mpD3DFontTexture->UnlockRect(0)));
	return true;
}

void VDFontRendererD3D9::Shutdown() {
	mpD3DFontTexture = NULL;
	mpD3DManager = NULL;
}

bool VDFontRendererD3D9::Begin() {
	if (!mpD3DManager)
		return false;

	IDirect3DDevice9 *dev = mpD3DManager->GetDevice();

	D3DVIEWPORT9 vp;
	HRESULT hr = dev->GetViewport(&vp);
	if (FAILED(hr))
		return false;

	const D3DMATRIX ident={
		1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1
	};

	dev->SetTransform(D3DTS_WORLD, &ident);
	dev->SetTransform(D3DTS_VIEW, &ident);

	const D3DMATRIX proj = {
		2.0f / (float)vp.Width, 0.0f, 0.0f, 0.0f,
		0.0f, -2.0f / (float)vp.Height, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		-1.0f - 1.0f / (float)vp.Width, 1.0f + 1.0f / (float)vp.Height, 0.0f, 1.0f
	};

	dev->SetTransform(D3DTS_PROJECTION, &proj);

	dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

	dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	// Rite of passage for any 3D programmer:
	// "Why the *&#$ didn't it draw anything!?"
	dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	dev->SetRenderState(D3DRS_LIGHTING, FALSE);
	dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	dev->SetRenderState(D3DRS_ZENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);

	dev->SetVertexShader(NULL);
	dev->SetVertexDeclaration(mpD3DManager->GetVertexDeclaration());
	dev->SetPixelShader(NULL);
	dev->SetStreamSource(0, mpD3DManager->GetVertexBuffer(), 0, sizeof(nsVDD3D9::Vertex));
	dev->SetIndices(mpD3DManager->GetIndexBuffer());
	dev->SetTexture(0, mpD3DFontTexture);
	return true;
}

void VDFontRendererD3D9::DrawTextLine(int x, int y, uint32 textColor, uint32 outlineColor, const char *s) {
	const uint32 kMaxQuads = nsVDD3D9::kVertexBufferSize / 4;
	size_t len = strlen(s);

	mGlyphLayoutInfos.clear();
	mGlyphLayoutInfos.reserve(len);

	float xpos = (float)x;
	for(size_t i=0; i<len; ++i) {
		char c = *s++;
		const GlyphInfo& gi = mGlyphInfo[(int)c & 0xff];

		if (!gi.mPos.empty()) {
			mGlyphLayoutInfos.push_back();
			GlyphLayoutInfo& gli = mGlyphLayoutInfos.back();
			gli.mGlyph = (int)c & 0xff;
			gli.mX = xpos;
		}

		xpos += gi.mAdvance;
	}

	float ypos = (float)y;

	IDirect3DDevice9 *dev = mpD3DManager->GetDevice();
	for(int i=0; i<2; ++i) {
		uint32 vertexColor;

		switch(i) {
		case 0:
			vertexColor = outlineColor;
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
			dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			break;
		case 1:
			vertexColor = textColor;
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
			dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
			break;
		}

		uint32 glyphCount = (uint32)mGlyphLayoutInfos.size();
		uint32 glyphStart = 0;
		const GlyphLayoutInfo *pgli = mGlyphLayoutInfos.data();
		while(glyphStart < glyphCount) {
			uint32 glyphsToRender = glyphCount - glyphStart;
			if (glyphsToRender > kMaxQuads)
				glyphsToRender = kMaxQuads;

			nsVDD3D9::Vertex *vx = mpD3DManager->LockVertices(glyphsToRender * 4);
			if (!vx)
				break;

			for(uint32 i=0; i<glyphsToRender; ++i) {
				const GlyphInfo& gi = mGlyphInfo[pgli->mGlyph];

				new(vx  ) nsVDD3D9::Vertex(pgli->mX + gi.mPos.left,  ypos + gi.mPos.top,    vertexColor, gi.mUV.left,  gi.mUV.top   );
				new(vx+1) nsVDD3D9::Vertex(pgli->mX + gi.mPos.left,  ypos + gi.mPos.bottom, vertexColor, gi.mUV.left,  gi.mUV.bottom);
				new(vx+2) nsVDD3D9::Vertex(pgli->mX + gi.mPos.right, ypos + gi.mPos.bottom, vertexColor, gi.mUV.right, gi.mUV.bottom);
				new(vx+3) nsVDD3D9::Vertex(pgli->mX + gi.mPos.right, ypos + gi.mPos.top,    vertexColor, gi.mUV.right, gi.mUV.top   );
				vx += 4;
				++pgli;
			}

			mpD3DManager->UnlockVertices();

			uint16 *idx = mpD3DManager->LockIndices(glyphsToRender * 6);
			if (!idx)
				break;

			uint32 vidx = 0;
			for(uint32 i=0; i<glyphsToRender; ++i) {
				idx[0] = vidx;
				idx[1] = vidx+1;
				idx[2] = vidx+2;
				idx[3] = vidx;
				idx[4] = vidx+2;
				idx[5] = vidx+3;
				vidx += 4;
				idx += 6;
			}

			mpD3DManager->UnlockIndices();

			mpD3DManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4*glyphsToRender, 0, 2*glyphsToRender);

			glyphStart += glyphsToRender;
		}
	}
}

void VDFontRendererD3D9::End() {
	IDirect3DDevice9 *dev = mpD3DManager->GetDevice();

	dev->SetTexture(0, NULL);
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayDX9Manager : public IVDVideoDisplayDX9Manager, public VDD3D9Client {
public:
	struct EffectContext {
		IDirect3DTexture9 *mpSourceTexture1;
		IDirect3DTexture9 *mpSourceTexture2;
		IDirect3DTexture9 *mpSourceTexture3;
		IDirect3DTexture9 *mpInterpFilterH;
		IDirect3DTexture9 *mpInterpFilterV;
		uint32 mSourceW;
		uint32 mSourceH;
		uint32 mSourceTexW;
		uint32 mSourceTexH;
		uint32 mInterpHTexW;
		uint32 mInterpHTexH;
		uint32 mInterpVTexW;
		uint32 mInterpVTexH;
	};

	VDVideoDisplayDX9Manager();
	~VDVideoDisplayDX9Manager();

	int AddRef();
	int Release();

	bool Init();
	void Shutdown();

	CubicMode InitBicubic();
	void ShutdownBicubic();

	IVDD3D9Texture	*GetTempRTT(int i) const { return mpRTTs[i]; }
	IVDD3D9Texture	*GetFilterTexture() const { return mpFilterTexture; }
	IVDD3D9Texture	*GetHEvenOddTexture() const { return mpHEvenOddTexture; }

	void		DetermineBestTextureFormat(int srcFormat, int& dstFormat, D3DFORMAT& dstD3DFormat);

	bool ValidateBicubicShader(CubicMode mode);

	bool RunEffect(const EffectContext& ctx, const RECT& rClient, const TechniqueInfo& technique, IDirect3DSurface9 *pRTOverride);

public:
	void OnPreDeviceReset() {}
	void OnPostDeviceReset() {}

protected:
	bool InitEffect();
	void ShutdownEffect();

	VDD3D9Manager		*mpManager;
	vdrefptr<IVDD3D9Texture>	mpFilterTexture;
	vdrefptr<IVDD3D9Texture>	mpHEvenOddTexture;
	vdrefptr<IVDD3D9Texture>	mpRTTs[2];

	vdfastvector<IDirect3DVertexShader9 *>	mVertexShaders;
	vdfastvector<IDirect3DPixelShader9 *>	mPixelShaders;

	CubicMode			mCubicMode;
	int					mCubicRefCount;

	int					mRefCount;
};

///////////////////////////////////////////////////////////////////////////

static VDVideoDisplayDX9Manager *g_pVDDisplayDX9;

bool VDInitDisplayDX9(VDVideoDisplayDX9Manager **ppManager) {
	if (!g_pVDDisplayDX9) {
		g_pVDDisplayDX9 = new VDVideoDisplayDX9Manager;
		if (!g_pVDDisplayDX9->Init()) {
			delete g_pVDDisplayDX9;
			return false;
		}
	}
		
	*ppManager = g_pVDDisplayDX9;
	g_pVDDisplayDX9->AddRef();
	return true;
}

VDVideoDisplayDX9Manager::VDVideoDisplayDX9Manager()
	: mpManager(NULL)
	, mCubicRefCount(0)
	, mRefCount(0)
{
}

VDVideoDisplayDX9Manager::~VDVideoDisplayDX9Manager() {
	VDASSERT(!mRefCount);
	VDASSERT(!mCubicRefCount);

	g_pVDDisplayDX9 = NULL;
}

int VDVideoDisplayDX9Manager::AddRef() {
	return ++mRefCount;
}

int VDVideoDisplayDX9Manager::Release() {
	int rc = --mRefCount;
	if (!rc) {
		Shutdown();
		delete this;
	}
	return rc;
}

bool VDVideoDisplayDX9Manager::Init() {
	VDASSERT(!mpManager);
	mpManager = VDInitDirect3D9(this);
	if (!mpManager)
		return false;

	if (!mpManager->CreateSharedTexture<VDD3D9TextureGeneratorHEvenOdd>("hevenodd", ~mpHEvenOddTexture)) {
		Shutdown();
		return false;
	}

	if (!InitEffect()) {
		Shutdown();
		return false;
	}

	return true;
}

void VDVideoDisplayDX9Manager::Shutdown() {
	VDASSERT(!mCubicRefCount);

	mpHEvenOddTexture = NULL;

	ShutdownEffect();

	if (mpManager) {
		VDDeinitDirect3D9(mpManager, this);
		mpManager = NULL;
	}
}

bool VDVideoDisplayDX9Manager::InitEffect() {
	IDirect3DDevice9 *pD3DDevice = mpManager->GetDevice();
	const D3DCAPS9& caps = mpManager->GetCaps();

	// initialize vertex shaders
	if (g_effect.mVertexShaderCount && mVertexShaders.empty()) {
		mVertexShaders.resize(g_effect.mVertexShaderCount, NULL);
		for(uint32 i=0; i<g_effect.mVertexShaderCount; ++i) {
			const uint32 *pVertexShaderData = g_shaderData + g_effect.mVertexShaderOffsets[i];

			if ((pVertexShaderData[0] & 0xffff) > (caps.VertexShaderVersion & 0xffff))
				continue;

			HRESULT hr = pD3DDevice->CreateVertexShader((const DWORD *)pVertexShaderData, &mVertexShaders[i]);
			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Unable to create vertex shader #%d.\n", i+1);
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Vertex shader version is: %x.\n", pVertexShaderData[0]);
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Supported vertex shader version is: %x.\n", caps.VertexShaderVersion);
				return false;
			}
		}
	}

	// initialize pixel shaders
	if (g_effect.mPixelShaderCount && mPixelShaders.empty()) {
		mPixelShaders.resize(g_effect.mPixelShaderCount, NULL);
		for(uint32 i=0; i<g_effect.mPixelShaderCount; ++i) {
			const uint32 *pPixelShaderData = g_shaderData + g_effect.mPixelShaderOffsets[i];

			if ((pPixelShaderData[0] & 0xffff) > (caps.PixelShaderVersion & 0xffff))
				continue;

			HRESULT hr = pD3DDevice->CreatePixelShader((const DWORD *)pPixelShaderData, &mPixelShaders[i]);
			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Unable to create pixel shader #%d.\n", i+1);
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Pixel shader version is: %x.\n", pPixelShaderData[0]);
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Supported pixel shader version is: %x.\n", caps.PixelShaderVersion);
				return false;
			}
		}
	}

	return true;
}

void VDVideoDisplayDX9Manager::ShutdownEffect() {
	while(!mPixelShaders.empty()) {
		IDirect3DPixelShader9 *ps = mPixelShaders.back();
		mPixelShaders.pop_back();

		if (ps)
			ps->Release();
	}

	while(!mVertexShaders.empty()) {
		IDirect3DVertexShader9 *vs = mVertexShaders.back();
		mVertexShaders.pop_back();

		if (vs)
			vs->Release();
	}
}

VDVideoDisplayDX9Manager::CubicMode VDVideoDisplayDX9Manager::InitBicubic() {
	VDASSERT(mRefCount > 0);

	if (++mCubicRefCount > 1)
		return mCubicMode;

	mCubicMode = (CubicMode)kMaxCubicMode;
	while(mCubicMode > kCubicNotPossible) {
		if (ValidateBicubicShader(mCubicMode))
			break;
		mCubicMode = (CubicMode)(mCubicMode - 1);
	}

	if (mCubicMode == kCubicNotPossible) {
		ShutdownBicubic();
		return mCubicMode;
	}

	bool success = false;

	switch(mCubicMode) {
		case kCubicUseFF2Path:
			success = mpManager->CreateSharedTexture<VDD3D9TextureGeneratorCubicFilterFF2>("cubicfilter", ~mpFilterTexture);
			break;
		case kCubicUseFF3Path:
			success = mpManager->CreateSharedTexture<VDD3D9TextureGeneratorCubicFilterFF3>("cubicfilter", ~mpFilterTexture);
			break;
		case kCubicUsePS1_1Path:
		case kCubicUsePS1_4Path:
			success = true;
			break;
	}

	if (!success) {
		ShutdownBicubic();
		return kCubicNotPossible;
	}

	// create horizontal resampling texture
	if (!mpManager->CreateSharedTexture("rtt1", VDCreateD3D9TextureGeneratorFullSizeRTT, ~mpRTTs[0])) {
		ShutdownBicubic();
		return kCubicNotPossible;
	}

	// create vertical resampling texture
	if (mCubicMode < kCubicUsePS1_1Path) {
		if (!mpManager->CreateSharedTexture("rtt2", VDCreateD3D9TextureGeneratorFullSizeRTT, ~mpRTTs[1])) {
			ShutdownBicubic();
			return kCubicNotPossible;
		}
	}


	return mCubicMode;
}

void VDVideoDisplayDX9Manager::ShutdownBicubic() {
	VDASSERT(mCubicRefCount > 0);
	if (--mCubicRefCount)
		return;

	mpFilterTexture = NULL;
	mpRTTs[1] = NULL;
	mpRTTs[0] = NULL;
}

namespace {
	D3DFORMAT GetD3DTextureFormatForPixmapFormat(int format) {
		using namespace nsVDPixmap;

		switch(format) {
			case nsVDPixmap::kPixFormat_XRGB1555:
				return D3DFMT_X1R5G5B5;

			case nsVDPixmap::kPixFormat_RGB565:
				return D3DFMT_R5G6B5;

			case nsVDPixmap::kPixFormat_RGB888:
				return D3DFMT_R8G8B8;				// No real hardware supports this format, in practice.

			case nsVDPixmap::kPixFormat_XRGB8888:
				return D3DFMT_X8R8G8B8;

			case nsVDPixmap::kPixFormat_YUV422_UYVY:
				return D3DFMT_UYVY;

			case nsVDPixmap::kPixFormat_YUV422_YUYV:
				return D3DFMT_YUY2;

			default:
				return D3DFMT_UNKNOWN;
		}
	}
}

void VDVideoDisplayDX9Manager::DetermineBestTextureFormat(int srcFormat, int& dstFormat, D3DFORMAT& dstD3DFormat) {
	using namespace nsVDPixmap;

	// Try direct format first. If that doesn't work, try a fallback (in practice, we
	// only have one).

	dstFormat = srcFormat;
	for(int i=0; i<2; ++i) {
		dstD3DFormat = GetD3DTextureFormatForPixmapFormat(dstFormat);
		if (dstD3DFormat && mpManager->IsTextureFormatAvailable(dstD3DFormat)) {
			dstFormat = srcFormat;
			return;
		}

		// fallback
		switch(dstFormat) {
			case kPixFormat_XRGB1555:
				dstFormat = kPixFormat_RGB565;
				break;

			case kPixFormat_RGB565:
				dstFormat = kPixFormat_XRGB1555;
				break;

			case kPixFormat_YUV422_UYVY:
				dstFormat =	kPixFormat_YUV422_YUYV;
				break;

			case kPixFormat_YUV422_YUYV:
				dstFormat = kPixFormat_YUV422_UYVY;
				break;

			default:
				goto fail;
		}
	}
fail:

	// Just use X8R8G8B8. We always know this works (we reject the device if it doesn't).
	dstFormat = kPixFormat_XRGB8888;
	dstD3DFormat = D3DFMT_X8R8G8B8;
}

bool VDVideoDisplayDX9Manager::ValidateBicubicShader(CubicMode mode) {
	const TechniqueInfo *pTechInfo;
	switch(mode) {
		case kCubicUsePS1_4Path:
			pTechInfo = &g_technique_bicubic1_4;
			break;																														
		case kCubicUsePS1_1Path:																				
			pTechInfo = &g_technique_bicubic1_1;
			break;																														
		case kCubicUseFF3Path:																				
			pTechInfo = &g_technique_bicubicFF3;
			break;																														
		case kCubicUseFF2Path:																				
			pTechInfo = &g_technique_bicubicFF2;
			break;
		default:
			return false;
	}

	// Validate caps bits.
	const D3DCAPS9& caps = mpManager->GetCaps();
	if ((caps.PrimitiveMiscCaps & pTechInfo->mPrimitiveMiscCaps) != pTechInfo->mPrimitiveMiscCaps)
		return false;
	if (caps.MaxSimultaneousTextures < pTechInfo->mMaxSimultaneousTextures)
		return false;
	if (caps.MaxTextureBlendStages < pTechInfo->mMaxSimultaneousTextures)
		return false;
	if ((caps.SrcBlendCaps & pTechInfo->mSrcBlendCaps) != pTechInfo->mSrcBlendCaps)
		return false;
	if ((caps.DestBlendCaps & pTechInfo->mDestBlendCaps) != pTechInfo->mDestBlendCaps)
		return false;
	if ((caps.TextureOpCaps & pTechInfo->mTextureOpCaps) != pTechInfo->mTextureOpCaps)
		return false;
	if (pTechInfo->mPSVersionRequired) {
		if (caps.PixelShaderVersion < pTechInfo->mPSVersionRequired)
			return false;
		if (caps.PixelShader1xMaxValue < pTechInfo->mPixelShader1xMaxValue * 0.95f)
			return false;
	}

	// Validate shaders.
	IDirect3DDevice9 *pDevice = mpManager->GetDevice();
	HRESULT hr = pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2);
	if (FAILED(hr))
		return false;

	const PassInfo *pPasses = pTechInfo->mpPasses;
	for(uint32 stage = 0; stage < pTechInfo->mPassCount; ++stage) {
		const PassInfo& pi = *pPasses++;

		const uint32 stateStart = pi.mStateStart, stateEnd = pi.mStateEnd;
		for(uint32 stateIdx = stateStart; stateIdx != stateEnd; ++stateIdx) {
			uint32 token = g_states[stateIdx];
			uint32 tokenIndex = (token >> 12) & 0xFFF;
			uint32 tokenValue = token & 0xFFF;

			if (tokenValue == 0xFFF)
				tokenValue = g_states[++stateIdx];

			hr = S_OK;
			switch(token >> 28) {
				case 0:		// render state
					hr = pDevice->SetRenderState((D3DRENDERSTATETYPE)tokenIndex, tokenValue);
					break;
				case 1:		// texture stage state
					hr = pDevice->SetTextureStageState((token >> 24)&15, (D3DTEXTURESTAGESTATETYPE)tokenIndex, tokenValue);
					break;
				case 2:		// sampler state
					hr = pDevice->SetSamplerState((token >> 24)&15, (D3DSAMPLERSTATETYPE)tokenIndex, tokenValue);
					break;
				case 3:		// texture
				case 8:		// vertex bool constant
				case 9:		// vertex int constant
				case 10:	// vertex float constant
				case 12:	// pixel bool constant
				case 13:	// pixel int constant
				case 14:	// pixel float constant
					// ignore.
					break;
			}

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to set state! hr=%08x\n", hr);
				return false;
			}
		}

		HRESULT hr = pDevice->SetVertexShader(pi.mVertexShaderIndex >= 0 ? mVertexShaders[pi.mVertexShaderIndex] : NULL);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't set vertex shader! hr=%08x\n", hr);
			return false;
		}

		hr = pDevice->SetPixelShader(pi.mPixelShaderIndex >= 0 ? mPixelShaders[pi.mPixelShaderIndex] : NULL);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't set pixel shader! hr=%08x\n", hr);
			return false;
		}

		DWORD passes;
		hr = pDevice->ValidateDevice(&passes);

		if (FAILED(hr))
			return false;
	}

	return true;
}

bool VDVideoDisplayDX9Manager::RunEffect(const EffectContext& ctx, const RECT& rClient, const TechniqueInfo& technique, IDirect3DSurface9 *pRTOverride) {
	IDirect3DTexture9 *const textures[10]={
		NULL,
		ctx.mpSourceTexture1,
		ctx.mpSourceTexture2,
		ctx.mpSourceTexture3,
		mpRTTs[0] ? mpRTTs[0]->GetD3DTexture() : NULL,
		mpRTTs[1] ? mpRTTs[1]->GetD3DTexture() : NULL,
		mpFilterTexture ? mpFilterTexture->GetD3DTexture() : NULL,
		mpHEvenOddTexture ? mpHEvenOddTexture->GetD3DTexture() : NULL,
		ctx.mpInterpFilterH,
		ctx.mpInterpFilterV
	};

	const D3DPRESENT_PARAMETERS& pparms = mpManager->GetPresentParms();
	int clippedWidth = std::min<int>(rClient.right, pparms.BackBufferWidth);
	int clippedHeight = std::min<int>(rClient.bottom, pparms.BackBufferHeight);

	if (clippedWidth <= 0 || clippedHeight <= 0)
		return true;

	struct StdParamData {
		float vpsize[4];			// (viewport size)			vpwidth, vpheight, 1/vpheight, 1/vpwidth
		float cvpsize[4];			// (clipped viewport size)	cvpwidth, cvpheight, 1/cvpheight, 1/cvpwidth
		float texsize[4];			// (texture size)			texwidth, texheight, 1/texheight, 1/texwidth
		float tex2size[4];			// (texture2 size)			tex2width, tex2height, 1/tex2height, 1/tex2width
		float srcsize[4];			// (source size)			srcwidth, srcheight, 1/srcheight, 1/srcwidth
		float tempsize[4];			// (temp rtt size)			tempwidth, tempheight, 1/tempheight, 1/tempwidth
		float temp2size[4];			// (temp2 rtt size)			tempwidth, tempheight, 1/tempheight, 1/tempwidth
		float vpcorrect[4];			// (viewport correction)	2/vpwidth, 2/vpheight, -1/vpheight, 1/vpwidth
		float vpcorrect2[4];		// (viewport correction)	2/vpwidth, -2/vpheight, 1+1/vpheight, -1-1/vpwidth
		float tvpcorrect[4];		// (temp vp correction)		2/tvpwidth, 2/tvpheight, -1/tvpheight, 1/tvpwidth
		float tvpcorrect2[4];		// (temp vp correction)		2/tvpwidth, -2/tvpheight, 1+1/tvpheight, -1-1/tvpwidth
		float t2vpcorrect[4];		// (temp2 vp correction)	2/tvpwidth, 2/tvpheight, -1/tvpheight, 1/tvpwidth
		float t2vpcorrect2[4];		// (temp2 vp correction)	2/tvpwidth, -2/tvpheight, 1+1/tvpheight, -1-1/tvpwidth
		float time[4];				// (time)
		float interphtexsize[4];	// (time)
		float interpvtexsize[4];	// (time)
	};

	static const struct StdParam {
		int offset;
	} kStdParamInfo[]={
		offsetof(StdParamData, vpsize),
		offsetof(StdParamData, cvpsize),
		offsetof(StdParamData, texsize),
		offsetof(StdParamData, tex2size),
		offsetof(StdParamData, srcsize),
		offsetof(StdParamData, tempsize),
		offsetof(StdParamData, temp2size),
		offsetof(StdParamData, vpcorrect),
		offsetof(StdParamData, vpcorrect2),
		offsetof(StdParamData, tvpcorrect),
		offsetof(StdParamData, tvpcorrect2),
		offsetof(StdParamData, t2vpcorrect),
		offsetof(StdParamData, t2vpcorrect2),
		offsetof(StdParamData, time),
		offsetof(StdParamData, interphtexsize),
		offsetof(StdParamData, interpvtexsize),
	};

	StdParamData data;

	data.vpsize[0] = (float)rClient.right;
	data.vpsize[1] = (float)rClient.bottom;
	data.vpsize[2] = 1.0f / (float)rClient.bottom;
	data.vpsize[3] = 1.0f / (float)rClient.right;
	data.cvpsize[0] = (float)clippedWidth;
	data.cvpsize[0] = (float)clippedHeight;
	data.cvpsize[0] = 1.0f / (float)clippedHeight;
	data.cvpsize[0] = 1.0f / (float)clippedWidth;
	data.texsize[0] = (float)(int)ctx.mSourceTexW;
	data.texsize[1] = (float)(int)ctx.mSourceTexH;
	data.texsize[2] = 1.0f / (float)(int)ctx.mSourceTexH;
	data.texsize[3] = 1.0f / (float)(int)ctx.mSourceTexW;
	data.tex2size[0] = 1.f;
	data.tex2size[1] = 1.f;
	data.tex2size[2] = 1.f;
	data.tex2size[3] = 1.f;
	data.srcsize[0] = (float)(int)ctx.mSourceW;
	data.srcsize[1] = (float)(int)ctx.mSourceH;
	data.srcsize[2] = 1.0f / (float)(int)ctx.mSourceH;
	data.srcsize[3] = 1.0f / (float)(int)ctx.mSourceW;
	data.tempsize[0] = 1.f;
	data.tempsize[1] = 1.f;
	data.tempsize[2] = 1.f;
	data.tempsize[3] = 1.f;
	data.temp2size[0] = 1.f;
	data.temp2size[1] = 1.f;
	data.temp2size[2] = 1.f;
	data.temp2size[3] = 1.f;
	data.vpcorrect[0] = 2.0f / (float)clippedWidth;
	data.vpcorrect[1] = 2.0f / (float)clippedHeight;
	data.vpcorrect[2] = -1.0f / (float)clippedHeight;
	data.vpcorrect[3] = 1.0f / (float)clippedWidth;
	data.vpcorrect2[0] = 2.0f / (float)clippedWidth;
	data.vpcorrect2[1] = -2.0f / (float)clippedHeight;
	data.vpcorrect2[2] = 1.0f + 1.0f / (float)clippedHeight;
	data.vpcorrect2[3] = -1.0f - 1.0f / (float)clippedWidth;
	data.tvpcorrect[0] = 2.0f;
	data.tvpcorrect[1] = 2.0f;
	data.tvpcorrect[2] = -1.0f;
	data.tvpcorrect[3] = 1.0f;
	data.tvpcorrect2[0] = 2.0f;
	data.tvpcorrect2[1] = -2.0f;
	data.tvpcorrect2[2] = 0.f;
	data.tvpcorrect2[3] = 2.0f;
	data.t2vpcorrect[0] = 2.0f;
	data.t2vpcorrect[1] = 2.0f;
	data.t2vpcorrect[2] = -1.0f;
	data.t2vpcorrect[3] = 1.0f;
	data.t2vpcorrect2[0] = 2.0f;
	data.t2vpcorrect2[1] = -2.0f;
	data.t2vpcorrect2[2] = 0.f;
	data.t2vpcorrect2[3] = 2.0f;
	data.interphtexsize[0] = (float)ctx.mInterpHTexW;
	data.interphtexsize[1] = (float)ctx.mInterpHTexH;
	data.interphtexsize[2] = ctx.mInterpHTexH ? 1.0f / (float)ctx.mInterpHTexH : 0.0f;
	data.interphtexsize[3] = ctx.mInterpHTexW ? 1.0f / (float)ctx.mInterpHTexW : 0.0f;
	data.interpvtexsize[0] = (float)ctx.mInterpVTexW;
	data.interpvtexsize[1] = (float)ctx.mInterpVTexH;
	data.interpvtexsize[2] = ctx.mInterpVTexH ? 1.0f / (float)ctx.mInterpVTexH : 0.0f;
	data.interpvtexsize[3] = ctx.mInterpVTexW ? 1.0f / (float)ctx.mInterpVTexW : 0.0f;

	uint32 t = VDGetAccurateTick();
	data.time[0] = (t % 1000) / 1000.0f;
	data.time[1] = (t % 5000) / 5000.0f;
	data.time[2] = (t % 10000) / 10000.0f;
	data.time[3] = (t % 30000) / 30000.0f;

	if (ctx.mpSourceTexture2) {
		D3DSURFACE_DESC desc;

		HRESULT hr = ctx.mpSourceTexture2->GetLevelDesc(0, &desc);
		if (FAILED(hr))
			return false;

		float w = (float)desc.Width;
		float h = (float)desc.Height;

		data.tex2size[0] = w;
		data.tex2size[1] = h;
		data.tex2size[2] = 1.0f / h;
		data.tex2size[3] = 1.0f / w;
	}

	if (mpRTTs[0]) {
		data.tempsize[0] = (float)mpRTTs[0]->GetWidth();
		data.tempsize[1] = (float)mpRTTs[0]->GetHeight();
		data.tempsize[2] = 1.0f / data.tempsize[1];
		data.tempsize[3] = 1.0f / data.tempsize[0];
		data.tvpcorrect[0] = 2.0f * data.tempsize[3];
		data.tvpcorrect[1] = 2.0f * data.tempsize[2];
		data.tvpcorrect[2] = -data.tempsize[2];
		data.tvpcorrect[3] = data.tempsize[3];
		data.tvpcorrect2[0] = 2.0f * data.tempsize[3];
		data.tvpcorrect2[1] = -2.0f * data.tempsize[2];
		data.tvpcorrect2[2] = 1.0f + data.tempsize[2];
		data.tvpcorrect2[3] = -1.0f - data.tempsize[3];
	}

	if (mpRTTs[1]) {
		data.temp2size[0] = (float)mpRTTs[1]->GetWidth();
		data.temp2size[1] = (float)mpRTTs[1]->GetHeight();
		data.temp2size[2] = 1.0f / data.temp2size[1];
		data.temp2size[3] = 1.0f / data.temp2size[0];
		data.t2vpcorrect[0] = 2.0f * data.tempsize[3];
		data.t2vpcorrect[1] = 2.0f * data.tempsize[2];
		data.t2vpcorrect[2] = -data.tempsize[2];
		data.t2vpcorrect[3] = data.tempsize[3];
		data.t2vpcorrect2[0] = 2.0f * data.tempsize[3];
		data.t2vpcorrect2[1] = -2.0f * data.tempsize[2];
		data.t2vpcorrect2[2] = 1.0f + data.tempsize[2];
		data.t2vpcorrect2[3] = -1.0f - data.tempsize[3];
	}

	enum { kStdParamCount = sizeof kStdParamInfo / sizeof kStdParamInfo[0] };

	uint32 nPasses = technique.mPassCount;
	const PassInfo *pPasses = technique.mpPasses;
	IDirect3DDevice9 *dev = mpManager->GetDevice();
	while(nPasses--) {
		const PassInfo& pi = *pPasses++;

		// bind vertex and pixel shaders
		HRESULT hr = dev->SetVertexShader(pi.mVertexShaderIndex >= 0 ? mVertexShaders[pi.mVertexShaderIndex] : NULL);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't set vertex shader! hr=%08x\n", hr);
			return false;
		}

		hr = dev->SetPixelShader(pi.mPixelShaderIndex >= 0 ? mPixelShaders[pi.mPixelShaderIndex] : NULL);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't set pixel shader! hr=%08x\n", hr);
			return false;
		}

		// set states
		const uint32 stateStart = pi.mStateStart, stateEnd = pi.mStateEnd;
		for(uint32 stateIdx = stateStart; stateIdx != stateEnd; ++stateIdx) {
			uint32 token = g_states[stateIdx];
			uint32 tokenIndex = (token >> 12) & 0xFFF;
			uint32 tokenValue = token & 0xFFF;

			if (tokenValue == 0xFFF)
				tokenValue = g_states[++stateIdx];

			HRESULT hr;
			switch(token >> 28) {
				case 0:		// render state
					hr = dev->SetRenderState((D3DRENDERSTATETYPE)tokenIndex, tokenValue);
					break;
				case 1:		// texture stage state
					switch((D3DTEXTURESTAGESTATETYPE)tokenIndex) {
					case D3DTSS_BUMPENVMAT00:
					case D3DTSS_BUMPENVMAT01:
					case D3DTSS_BUMPENVMAT10:
					case D3DTSS_BUMPENVMAT11:
						{
							union {
								uint32 i;
								float f;
							} converter = {tokenValue};

							if (pi.mBumpEnvScale) {
								const float *param = (const float *)&data + 4*(pi.mBumpEnvScale - 1);

								switch((D3DTEXTURESTAGESTATETYPE)tokenIndex) {
								case D3DTSS_BUMPENVMAT00:
								case D3DTSS_BUMPENVMAT10:
									converter.f *= param[3];
									break;
								case D3DTSS_BUMPENVMAT01:
								case D3DTSS_BUMPENVMAT11:
								default:
									converter.f *= param[2];
									break;
								}
							}
						
							hr = dev->SetTextureStageState((token >> 24)&15, (D3DTEXTURESTAGESTATETYPE)tokenIndex, converter.i);
						}
						break;
					default:
						hr = dev->SetTextureStageState((token >> 24)&15, (D3DTEXTURESTAGESTATETYPE)tokenIndex, tokenValue);
						break;
					}
					break;
				case 2:		// sampler state
					hr = dev->SetSamplerState((token >> 24)&15, (D3DSAMPLERSTATETYPE)tokenIndex, tokenValue);
					break;
				case 3:		// texture
					VDASSERT(tokenValue < 10);
					hr = dev->SetTexture(tokenIndex, textures[tokenValue]);
					break;
				case 8:		// vertex bool constant
					hr = dev->SetVertexShaderConstantB(tokenIndex, (const BOOL *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 9:		// vertex int constant
					hr = dev->SetVertexShaderConstantI(tokenIndex, (const INT *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 10:	// vertex float constant
					hr = dev->SetVertexShaderConstantF(tokenIndex, (const float *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 12:	// pixel bool constant
					hr = dev->SetPixelShaderConstantB(tokenIndex, (const BOOL *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 13:	// pixel int constant
					hr = dev->SetPixelShaderConstantI(tokenIndex, (const INT *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 14:	// pixel float constant
					hr = dev->SetPixelShaderConstantF(tokenIndex, (const float *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
			}

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to set state! hr=%08x\n", hr);
				return false;
			}
		}

		// change render target
		if (pi.mRenderTarget >= 0) {
			if (!mpManager->EndScene())
				return false;

			HRESULT hr;
			switch(pi.mRenderTarget) {
				case 0:
					hr = dev->SetRenderTarget(0, pRTOverride ? pRTOverride : mpManager->GetRenderTarget());
					break;
				case 1:
					if (mpRTTs[0]) {
						IDirect3DSurface9 *pSurf;
						hr = mpRTTs[0]->GetD3DTexture()->GetSurfaceLevel(0, &pSurf);
						if (SUCCEEDED(hr)) {
							hr = dev->SetRenderTarget(0, pSurf);
							pSurf->Release();
						}
					}
					break;
				case 2:
					if (mpRTTs[1]) {
						IDirect3DSurface9 *pSurf;
						hr = mpRTTs[1]->GetD3DTexture()->GetSurfaceLevel(0, &pSurf);
						if (SUCCEEDED(hr)) {
							hr = dev->SetRenderTarget(0, pSurf);
							pSurf->Release();
						}
					}
					break;
			}

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to set render target! hr=%08x\n", hr);
				return false;
			}
		}

		// change viewport
		D3DVIEWPORT9 vp;
		if (pi.mViewportW | pi.mViewportH) {
			HRESULT hr;

			IDirect3DSurface9 *rt;
			hr = dev->GetRenderTarget(0, &rt);
			if (SUCCEEDED(hr)) {
				D3DSURFACE_DESC desc;
				hr = rt->GetDesc(&desc);
				if (SUCCEEDED(hr)) {
					const DWORD hsizes[3]={ desc.Width, ctx.mSourceW, clippedWidth };
					const DWORD vsizes[3]={ desc.Height, ctx.mSourceH, clippedHeight };

					vp.X = 0;
					vp.Y = 0;
					vp.Width = hsizes[pi.mViewportW];
					vp.Height = vsizes[pi.mViewportH];
					vp.MinZ = 0;
					vp.MaxZ = 1;

					hr = dev->SetViewport(&vp);
				}
				rt->Release();
			}

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to set viewport! hr=%08x\n", hr);
				return false;
			}
		} else {
			HRESULT hr = dev->GetViewport(&vp);
			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to retrieve viewport! hr=%08x\n", hr);
				return false;
			}
		}

		// clear target
		if (pi.mbRTDoClear) {
			hr = dev->Clear(0, NULL, D3DCLEAR_TARGET, pi.mRTClearColor, 0, 0);
			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to clear viewport! hr=%08x\n", hr);
				return false;
			}
		}

		// render!
		if (Vertex *pvx = mpManager->LockVertices(4)) {
			const float ustep = 1.0f / (float)(int)ctx.mSourceTexW;
			const float vstep = 1.0f / (float)(int)ctx.mSourceTexH;
			const float u0 = 0.0f;
			const float v0 = 0.0f;
			const float u1 = u0 + (int)ctx.mSourceW * ustep;
			const float v1 = v0 + (int)ctx.mSourceH * vstep;

			const float invVpW = 1.f / (float)vp.Width;
			const float invVpH = 1.f / (float)vp.Height;

			const float x0 = -1.f - invVpW;
			const float y0 = 1.f + invVpH;
			const float x1 = pi.mbClipPosition ? x0 + rClient.right * 2.0f * invVpW : 1.f - invVpW;
			const float y1 = pi.mbClipPosition ? y0 - rClient.bottom * 2.0f * invVpH : -1.f + invVpH;

			pvx[0].SetFF2(x0, y0, 0xFFFFFFFF, u0, v0, 0, 0);
			pvx[1].SetFF2(x1, y0, 0xFFFFFFFF, u1, v0, 1, 0);
			pvx[2].SetFF2(x0, y1, 0xFFFFFFFF, u0, v1, 0, 1);
			pvx[3].SetFF2(x1, y1, 0xFFFFFFFF, u1, v1, 1, 1);

			mpManager->UnlockVertices();
		}

		if (!mpManager->BeginScene())
			return false;

		hr = mpManager->DrawArrays(D3DPT_TRIANGLESTRIP, 0, 2);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to draw primitive! hr=%08x\n", hr);
			return false;
		}
	}

	// NVPerfHUD 3.1 draws a bit funny if we leave this set to REVSUBTRACT, even
	// with alpha blending off....
	dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

	return true;
}

///////////////////////////////////////////////////////////////////////////

class VDVideoUploadContextD3D9 : public vdrefcounted<IVDVideoUploadContextD3D9>, public VDD3D9Client {
public:
	VDVideoUploadContextD3D9();
	~VDVideoUploadContextD3D9();

	IDirect3DTexture9 *GetD3DTexture(int i = 0) { return mpD3DConversionTextures[0] ? mpD3DConversionTextures[i] : mpD3DImageTextures[i]; }

	bool Init(const VDPixmap& source, bool allowConversion, int buffers = 1);
	void Shutdown();

	bool Update(const VDPixmap& source, int fieldMask);

protected:
	void OnPreDeviceReset();
	void OnPostDeviceReset();
	bool ReinitVRAMTextures();

	VDD3D9Manager	*mpManager;
	vdrefptr<VDVideoDisplayDX9Manager> mpVideoManager;

	enum UploadMode {
		kUploadModeNormal,
		kUploadModeDirect8,
		kUploadModeDirect16
	} mUploadMode;

	int mBufferCount;
	int	mConversionTexW;
	int	mConversionTexH;

	VDPixmap			mTexFmt;

	IDirect3DTexture9	*mpD3DImageTextures[3];
	IDirect3DTexture9	*mpD3DImageTexture2a;
	IDirect3DTexture9	*mpD3DImageTexture2b;
	IDirect3DTexture9	*mpD3DConversionTextures[3];
};

bool VDCreateVideoUploadContextD3D9(IVDVideoUploadContextD3D9 **ppContext) {
	return VDRefCountObjectFactory<VDVideoUploadContextD3D9, IVDVideoUploadContextD3D9>(ppContext);
}

VDVideoUploadContextD3D9::VDVideoUploadContextD3D9()
	: mpManager(NULL)
	, mpD3DImageTexture2a(NULL)
	, mpD3DImageTexture2b(NULL)
{
	for(int i=0; i<3; ++i) {
		mpD3DImageTextures[i] = NULL;
		mpD3DConversionTextures[i] = NULL;
	}
}

VDVideoUploadContextD3D9::~VDVideoUploadContextD3D9() {
	Shutdown();
}

bool VDVideoUploadContextD3D9::Init(const VDPixmap& source, bool allowConversion, int buffers) {
	mBufferCount = buffers;

	VDASSERT(!mpManager);
	mpManager = VDInitDirect3D9(this);
	if (!mpManager)
		return false;

	if (!VDInitDisplayDX9(~mpVideoManager)) {
		Shutdown();
		return false;
	}

	// check capabilities
	const D3DCAPS9& caps = mpManager->GetCaps();

	if (caps.MaxTextureWidth < (uint32)source.w || caps.MaxTextureHeight < (uint32)source.h) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: source image is larger than maximum texture size\n");
		Shutdown();
		return false;
	}

	// create source texture
	int texw = source.w;
	int texh = source.h;

	mpManager->AdjustTextureSize(texw, texh);

	memset(&mTexFmt, 0, sizeof mTexFmt);
	mTexFmt.format		= nsVDPixmap::kPixFormat_XRGB8888;
	mTexFmt.w			= texw;
	mTexFmt.h			= texh;

	HRESULT hr;
	D3DFORMAT d3dfmt;
	IDirect3DDevice9 *dev = mpManager->GetDevice();

	if ((	source.format == nsVDPixmap::kPixFormat_YUV410_Planar ||
			source.format == nsVDPixmap::kPixFormat_YUV420_Planar ||
			source.format == nsVDPixmap::kPixFormat_YUV422_Planar ||
			source.format == nsVDPixmap::kPixFormat_YUV444_Planar)
		&& mpManager->IsTextureFormatAvailable(D3DFMT_L8) && caps.PixelShaderVersion >= D3DPS_VERSION(1, 1))
	{
		mUploadMode = kUploadModeDirect8;
		d3dfmt = D3DFMT_L8;

		uint32 subw = texw;
		uint32 subh = texh;

		switch(source.format) {
			case nsVDPixmap::kPixFormat_YUV444_Planar:
				break;
			case nsVDPixmap::kPixFormat_YUV422_Planar:
				subw >>= 1;
				break;
			case nsVDPixmap::kPixFormat_YUV420_Planar:
				subw >>= 1;
				subh >>= 1;
				break;
			case nsVDPixmap::kPixFormat_YUV410_Planar:
				subw >>= 2;
				subh >>= 2;
				break;
		}

		if (subw < 1)
			subw = 1;
		if (subh < 1)
			subh = 1;

		hr = dev->CreateTexture(subw, subh, 1, 0, D3DFMT_L8, D3DPOOL_MANAGED, &mpD3DImageTexture2a, NULL);
		if (FAILED(hr)) {
			Shutdown();
			return false;
		}

		hr = dev->CreateTexture(subw, subh, 1, 0, D3DFMT_L8, D3DPOOL_MANAGED, &mpD3DImageTexture2b, NULL);
		if (FAILED(hr)) {
			Shutdown();
			return false;
		}
	} else {
		mpVideoManager->DetermineBestTextureFormat(source.format, mTexFmt.format, d3dfmt);

		mUploadMode = kUploadModeNormal;

		if (source.format != mTexFmt.format) {
			if ((source.format == nsVDPixmap::kPixFormat_YUV422_UYVY || source.format == nsVDPixmap::kPixFormat_YUV422_YUYV)
				&& caps.PixelShaderVersion >= D3DPS_VERSION(1,1))
			{
				if (mpManager->IsTextureFormatAvailable(D3DFMT_A8R8G8B8)) {
					mUploadMode = kUploadModeDirect16;
					d3dfmt = D3DFMT_A8R8G8B8;
				}
			} else if (!allowConversion) {
				Shutdown();
				return false;
			}
		}
	}

	mConversionTexW = texw;
	mConversionTexH = texh;
	if (!ReinitVRAMTextures()) {
		Shutdown();
		return false;
	}

	if (mUploadMode == kUploadModeDirect16) {
		texw = (source.w + 1) >> 1;
		texh = source.h;
		mpManager->AdjustTextureSize(texw, texh);
	}

	hr = dev->CreateTexture(texw, texh, 1, 0, d3dfmt, D3DPOOL_MANAGED, &mpD3DImageTextures[0], NULL);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	if (!mpD3DConversionTextures[0]) {
		for(int i=1; i<buffers; ++i) {
			hr = dev->CreateTexture(texw, texh, 1, 0, d3dfmt, D3DPOOL_MANAGED, &mpD3DImageTextures[i], NULL);
			if (FAILED(hr)) {
				Shutdown();
				return false;
			}
		}
	}

	// clear source textures
	for(int i=0; i<buffers; ++i) {
		D3DLOCKED_RECT lr;
		if (mpD3DImageTextures[i] && SUCCEEDED(mpD3DImageTextures[i]->LockRect(0, &lr, NULL, 0))) {
			void *p = lr.pBits;
			for(int h=0; h<texh; ++h) {
				memset(p, 0, 4*texw);
				vdptroffset(p, lr.Pitch);
			}
			mpD3DImageTextures[i]->UnlockRect(0);
		}
	}

	VDDEBUG_DX9DISP("VideoDisplay/DX9: Init successful for %dx%d source image (%s -> %s)\n", source.w, source.h, VDPixmapGetInfo(source.format).name, VDPixmapGetInfo(mTexFmt.format).name);
	return true;
}

void VDVideoUploadContextD3D9::Shutdown() {
	for(int i=0; i<3; ++i) {
		if (mpD3DConversionTextures[i]) {
			mpD3DConversionTextures[i]->Release();
			mpD3DConversionTextures[i] = NULL;
		}
	}
	if (mpD3DImageTexture2b) {
		mpD3DImageTexture2b->Release();
		mpD3DImageTexture2b = NULL;
	}
	if (mpD3DImageTexture2a) {
		mpD3DImageTexture2a->Release();
		mpD3DImageTexture2a = NULL;
	}

	for(int i=0; i<3; ++i) {
		if (mpD3DImageTextures[i]) {
			mpD3DImageTextures[i]->Release();
			mpD3DImageTextures[i] = NULL;
		}
	}

	mpVideoManager = NULL;
	if (mpManager) {
		VDDeinitDirect3D9(mpManager, this);
		mpManager = NULL;
	}
}

bool VDVideoUploadContextD3D9::Update(const VDPixmap& source, int fieldMask) {
	if (mpD3DConversionTextures[1]) {
		for(int i=mBufferCount - 2; i>=0; --i)
			std::swap(mpD3DConversionTextures[i], mpD3DConversionTextures[i+1]);
	}

	if (mpD3DImageTextures[1]) {
		for(int i=mBufferCount - 2; i>=0; --i)
			std::swap(mpD3DImageTextures[i], mpD3DImageTextures[i+1]);
	}

	D3DLOCKED_RECT lr;
	HRESULT hr;
	
	hr = mpD3DImageTextures[0]->LockRect(0, &lr, NULL, 0);
	if (FAILED(hr))
		return false;

	mTexFmt.data		= lr.pBits;
	mTexFmt.pitch		= lr.Pitch;

	VDPixmap dst(mTexFmt);
	VDPixmap src(source);

	if (fieldMask == 1) {
		dst.pitch *= 2;
		dst.h = (dst.h + 1) >> 1;
		src.pitch *= 2;
		src.h = (src.h + 1) >> 1;
	} else if (fieldMask == 2) {
		dst.data = vdptroffset(dst.data, dst.pitch);
		dst.pitch *= 2;
		dst.h >>= 1;
		src.data = vdptroffset(src.data, src.pitch);
		src.pitch *= 2;
		src.h >>= 1;
	}

	if (mUploadMode == kUploadModeDirect16) {
		VDMemcpyRect(dst.data, dst.pitch, src.data, src.pitch, src.w * 2, src.h);
	} else if (mUploadMode == kUploadModeDirect8) {
		VDMemcpyRect(dst.data, dst.pitch, src.data, src.pitch, src.w, src.h);
	} else {
		VDPixmapBlt(dst, src);
	}

	VDVERIFY(SUCCEEDED(mpD3DImageTextures[0]->UnlockRect(0)));

	if (mUploadMode == kUploadModeDirect8) {
		uint32 subw = source.w;
		uint32 subh = source.h;

		switch(source.format) {
			case nsVDPixmap::kPixFormat_YUV410_Planar:
				subw >>= 2;
				subh >>= 2;
				break;
			case nsVDPixmap::kPixFormat_YUV420_Planar:
				subw >>= 1;
				subh >>= 1;
				break;
			case nsVDPixmap::kPixFormat_YUV422_Planar:
				subw >>= 1;
				break;
			case nsVDPixmap::kPixFormat_YUV444_Planar:
				break;
		}

		if (subw < 1)
			subw = 1;
		if (subh < 1)
			subh = 1;

		// upload Cb plane
		hr = mpD3DImageTexture2a->LockRect(0, &lr, NULL, 0);
		if (FAILED(hr))
			return false;

		VDMemcpyRect(lr.pBits, lr.Pitch, source.data2, source.pitch2, subw, subh);

		VDVERIFY(SUCCEEDED(mpD3DImageTexture2a->UnlockRect(0)));

		// upload Cr plane
		hr = mpD3DImageTexture2b->LockRect(0, &lr, NULL, 0);
		if (FAILED(hr))
			return false;

		VDMemcpyRect(lr.pBits, lr.Pitch, source.data3, source.pitch3, subw, subh);

		VDVERIFY(SUCCEEDED(mpD3DImageTexture2b->UnlockRect(0)));
	}

	if (mUploadMode != kUploadModeNormal) {
		IDirect3DDevice9 *dev = mpManager->GetDevice();
		vdrefptr<IDirect3DSurface9> rtsurface;

		hr = mpD3DConversionTextures[0]->GetSurfaceLevel(0, ~rtsurface);
		if (FAILED(hr))
			return false;

		hr = dev->SetStreamSource(0, mpManager->GetVertexBuffer(), 0, sizeof(Vertex));
		if (FAILED(hr))
			return false;

		hr = dev->SetIndices(mpManager->GetIndexBuffer());
		if (FAILED(hr))
			return false;

		hr = dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2);
		if (FAILED(hr))
			return false;

		hr = dev->SetRenderTarget(0, rtsurface);
		if (FAILED(hr))
			return false;

		static const uint32 kRenderStates[][2]={
			{	D3DRS_LIGHTING,			FALSE				},
			{	D3DRS_CULLMODE,			D3DCULL_NONE		},
			{	D3DRS_ZENABLE,			FALSE				},
			{	D3DRS_ALPHATESTENABLE,	FALSE				},
			{	D3DRS_ALPHABLENDENABLE,	FALSE				},
			{	D3DRS_STENCILENABLE,	FALSE				},
		};

		for(int i=0; i<sizeof(kRenderStates)/sizeof(kRenderStates[0]); ++i) {
			const uint32 (&rs)[2] = kRenderStates[i];

			hr = dev->SetRenderState((D3DRENDERSTATETYPE)rs[0], rs[1]);
			if (FAILED(hr))
				return false;
		}

		bool success = false;
		if (mpManager->BeginScene()) {
			success = true;

			D3DVIEWPORT9 vp = { 0, 0, source.w, source.h, 0, 1 };
			hr = dev->SetViewport(&vp);
			if (FAILED(hr))
				success = false;

			RECT r = { 0, 0, source.w, source.h };
			if (success) {
				VDVideoDisplayDX9Manager::EffectContext ctx;

				ctx.mpSourceTexture1 = mpD3DImageTextures[0];
				ctx.mpSourceTexture2 = mpD3DImageTexture2a;
				ctx.mpSourceTexture3 = mpD3DImageTexture2b;
				ctx.mpInterpFilterH = NULL;
				ctx.mpInterpFilterV = NULL;
				ctx.mSourceW = source.w;
				ctx.mSourceH = source.h;
				ctx.mSourceTexW = mTexFmt.w;
				ctx.mSourceTexH = mTexFmt.h;
				ctx.mInterpHTexW = 0;
				ctx.mInterpHTexH = 0;
				ctx.mInterpVTexW = 0;
				ctx.mInterpVTexH = 0;

				switch(source.format) {
					case nsVDPixmap::kPixFormat_YUV422_UYVY:
						if (!mpVideoManager->RunEffect(ctx, r, g_technique_uyvy_to_rgb_1_1, rtsurface))
							success = false;
						break;

					case nsVDPixmap::kPixFormat_YUV422_YUYV:
						if (!mpVideoManager->RunEffect(ctx, r, g_technique_yuy2_to_rgb_1_1, rtsurface))
							success = false;
						break;

					case nsVDPixmap::kPixFormat_YUV444_Planar:
						if (!mpVideoManager->RunEffect(ctx, r, g_technique_yv24_to_rgb_1_1, rtsurface))
							success = false;
						break;

					case nsVDPixmap::kPixFormat_YUV422_Planar:
						if (!mpVideoManager->RunEffect(ctx, r, g_technique_yv16_to_rgb_1_1, rtsurface))
							success = false;
						break;

					case nsVDPixmap::kPixFormat_YUV420_Planar:
						if (!mpVideoManager->RunEffect(ctx, r, g_technique_yv12_to_rgb_1_1, rtsurface))
							success = false;
						break;

					case nsVDPixmap::kPixFormat_YUV410_Planar:
						if (!mpVideoManager->RunEffect(ctx, r, g_technique_yvu9_to_rgb_1_1, rtsurface))
							success = false;
						break;

				}
			}

			if (!mpManager->EndScene())
				success = false;
		}

		dev->SetRenderTarget(0, mpManager->GetRenderTarget());

		return success;
	}

	return true;
}

void VDVideoUploadContextD3D9::OnPreDeviceReset() {
	for(int i=0; i<3; ++i) {
		if (mpD3DConversionTextures[i]) {
			mpD3DConversionTextures[i]->Release();
			mpD3DConversionTextures[i] = NULL;
		}
	}
}

void VDVideoUploadContextD3D9::OnPostDeviceReset() {
	ReinitVRAMTextures();
}

bool VDVideoUploadContextD3D9::ReinitVRAMTextures() {
	if (mUploadMode != kUploadModeNormal) {
		IDirect3DDevice9 *dev = mpManager->GetDevice();

		for(int i=0; i<mBufferCount; ++i) {
			if (mpD3DConversionTextures[i]) {
				mpD3DConversionTextures[i]->Release();
				mpD3DConversionTextures[i] = NULL;
			}

			HRESULT hr = dev->CreateTexture(mConversionTexW, mConversionTexH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &mpD3DConversionTextures[i], NULL);
			if (FAILED(hr))
				return false;

			mpManager->ClearRenderTarget(mpD3DConversionTextures[i]);
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayMinidriverDX9 : public VDVideoDisplayMinidriver, protected VDD3D9Client {
public:
	VDVideoDisplayMinidriverDX9();
	~VDVideoDisplayMinidriverDX9();

protected:
	bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid();
	bool IsFramePending() { return mbSwapChainPresentPending; }
	void SetFilterMode(FilterMode mode);
	void SetFullScreen(bool fs);

	bool Tick(int id);
	void Poll();
	bool Resize();
	bool Update(UpdateMode);
	void Refresh(UpdateMode);
	bool Paint(HDC hdc, const RECT& rClient, UpdateMode mode);

	void SetLogicalPalette(const uint8 *pLogicalPalette);
	float GetSyncDelta() const { return mSyncDelta; }

protected:
	void OnPreDeviceReset();
	void OnPostDeviceReset() {}

	void InitBicubic();
	void ShutdownBicubic();
	bool InitBicubicPS2Filters(int w, int h);
	void ShutdownBicubicPS2Filters();

	bool UpdateBackbuffer(const RECT& rClient);
	bool UpdateScreen(const RECT& rClient, UpdateMode updateMode, bool polling);

	HWND				mhwnd;
	RECT				mrClient;
	VDD3D9Manager		*mpManager;
	vdrefptr<VDVideoDisplayDX9Manager>	mpVideoManager;
	IDirect3DDevice9	*mpD3DDevice;			// weak ref
	vdrefptr<IDirect3DTexture9>	mpD3DInterpFilterTextureH;
	vdrefptr<IDirect3DTexture9>	mpD3DInterpFilterTextureV;
	int					mInterpFilterHSize;
	int					mInterpFilterHTexSize;
	int					mInterpFilterVSize;
	int					mInterpFilterVTexSize;

	vdrefptr<VDVideoUploadContextD3D9>	mpUploadContext;
	vdrefptr<IVDFontRendererD3D9>	mpFontRenderer;

	vdrefptr<IVDD3D9SwapChain>	mpSwapChain;
	int					mSwapChainW;
	int					mSwapChainH;
	bool				mbSwapChainImageValid;
	bool				mbSwapChainPresentPending;
	bool				mbSwapChainPresentPolling;
	bool				mbFirstPresent;
	bool				mbFullScreen;
	bool				mbFullScreenSet;

	VDVideoDisplayDX9Manager::CubicMode	mCubicMode;
	bool				mbCubicInitialized;
	bool				mbCubicAttempted;
	FilterMode			mPreferredFilter;
	float				mSyncDelta;
	VDD3DPresentHistory	mPresentHistory;

	VDPixmap					mTexFmt;
	enum UploadMode {
		kUploadModeNormal,
		kUploadModeDirect8,
		kUploadModeDirect16
	} mUploadMode;

	VDVideoDisplaySourceInfo	mSource;
};

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverDX9() {
	return new VDVideoDisplayMinidriverDX9;
}

VDVideoDisplayMinidriverDX9::VDVideoDisplayMinidriverDX9()
	: mpManager(NULL)
	, mpD3DDevice(NULL)
	, mInterpFilterHSize(0)
	, mInterpFilterHTexSize(0)
	, mInterpFilterVSize(0)
	, mInterpFilterVTexSize(0)
	, mpVideoManager(NULL)
	, mSwapChainW(0)
	, mSwapChainH(0)
	, mbSwapChainImageValid(false)
	, mbSwapChainPresentPending(false)
	, mbSwapChainPresentPolling(false)
	, mbFirstPresent(true)
	, mbFullScreen(false)
	, mbFullScreenSet(false)
	, mbCubicInitialized(false)
	, mbCubicAttempted(false)
	, mPreferredFilter(kFilterAnySuitable)
	, mSyncDelta(0.0f)
{
	mrClient.top = mrClient.left = mrClient.right = mrClient.bottom = 0;
}

VDVideoDisplayMinidriverDX9::~VDVideoDisplayMinidriverDX9() {
}

bool VDVideoDisplayMinidriverDX9::Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) {
	VDASSERT(!mpManager);
	mhwnd = hwnd;
	mSource = info;
	GetClientRect(hwnd, &mrClient);

	// attempt to initialize D3D9
	mbFullScreenSet = false;
	mpManager = VDInitDirect3D9(this);
	if (!mpManager) {
		Shutdown();
		return false;
	}

	if (!VDInitDisplayDX9(~mpVideoManager)) {
		Shutdown();
		return false;
	}

	if (mbFullScreen && !mbFullScreenSet) {
		mbFullScreenSet = true;
		mpManager->AdjustFullScreen(true);
	}

	mpD3DDevice = mpManager->GetDevice();

	// init font renderer
	if (mbDisplayDebugInfo) {
		if (!VDCreateFontRendererD3D9(~mpFontRenderer)) {
			Shutdown();
			return false;
		}

		mpFontRenderer->Init(mpManager);		// we explicitly allow this to fail
	}

	mpUploadContext = new_nothrow VDVideoUploadContextD3D9;
	if (!mpUploadContext || !mpUploadContext->Init(info.pixmap, info.bAllowConversion, 1)) {
		Shutdown();
		return false;
	}

	mSyncDelta = 0.0f;
	mbFirstPresent = true;

	return true;
}

void VDVideoDisplayMinidriverDX9::OnPreDeviceReset() {
	ShutdownBicubic();
	ShutdownBicubicPS2Filters();
	mpSwapChain = NULL;
	mSwapChainW = 0;
	mSwapChainH = 0;
}

void VDVideoDisplayMinidriverDX9::InitBicubic() {
	if (!mbCubicInitialized && !mbCubicAttempted) {
		mbCubicAttempted = true;

		mCubicMode = mpVideoManager->InitBicubic();

		if (mCubicMode != VDVideoDisplayDX9Manager::kCubicNotPossible) {
			if (mCubicMode == VDVideoDisplayDX9Manager::kCubicNotPossible) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Bicubic initialization failed -- falling back to bilinear path.\n");
			} else {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Bicubic initialization complete.\n");
				if (mCubicMode == VDVideoDisplayDX9Manager::kCubicUsePS1_4Path)
					VDDEBUG_DX9DISP("VideoDisplay/DX9: Using pixel shader 1.4, 5 texture (RADEON 8xxx+ / GeForceFX+) pixel path.\n");
				else if (mCubicMode == VDVideoDisplayDX9Manager::kCubicUsePS1_1Path)
					VDDEBUG_DX9DISP("VideoDisplay/DX9: Using pixel shader 1.1, 4 texture (GeForce3/4) pixel path.\n");
				else if (mCubicMode == VDVideoDisplayDX9Manager::kCubicUseFF3Path)
					VDDEBUG_DX9DISP("VideoDisplay/DX9: Using fixed function, 3 texture (RADEON 7xxx) pixel path.\n");
				else
					VDDEBUG_DX9DISP("VideoDisplay/DX9: Using fixed function, 2 texture (GeForce2) pixel path.\n");

				mbCubicInitialized = true;
			}
		}
	}
}

void VDVideoDisplayMinidriverDX9::ShutdownBicubic() {
	if (mbCubicInitialized) {
		mbCubicInitialized = mbCubicAttempted = false;
		mpVideoManager->ShutdownBicubic();
	}
}

///////////////////////////////////////////////////////////////////////////

namespace {
	int GeneratePS2CubicTexture(VDD3D9Manager *pManager, int w, int srcw, vdrefptr<IDirect3DTexture9>& pTexture, int existingTexW, bool mode1_4) {
		IDirect3DDevice9 *dev = pManager->GetDevice();

		// Round up to next multiple of 128 pixels to reduce reallocation.
		int texw = (w + 127) & ~127;
		int texh = 1;
		pManager->AdjustTextureSize(texw, texh);

		// If we can't fit the texture, bail.
		if (texw < w)
			return -1;

		// Check if we need to reallocate the texture.
		if (!pTexture || existingTexW != texw) {
			HRESULT hr = dev->CreateTexture(texw, texh, 1, 0, mode1_4  ? D3DFMT_A8R8G8B8 : D3DFMT_X8L8V8U8, D3DPOOL_MANAGED, ~pTexture, NULL);
			if (FAILED(hr))
				return -1;
		}

		// Fill the texture.
		D3DLOCKED_RECT lr;
		HRESULT hr = pTexture->LockRect(0, &lr, NULL, 0);
		VDASSERT(SUCCEEDED(hr));
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to load bicubic texture.\n");
			return -1;
		}

		double dudx = (double)srcw / (double)w;
		double u = dudx * 0.5;
		double u0 = 0.5;
		double ud0 = 1.5;
		double ud1 = (double)srcw - 1.5;
		double u1 = (double)srcw - 0.5;
		uint32 *p0 = (uint32 *)lr.pBits;

		if (mode1_4) {
			for(int x = 0; x < texw; ++x) {
				double ut = u;
				if (ut < u0)
					ut = u0;
				else if (ut > u1)
					ut = u1;
				int ix = VDFloorToInt(ut - 0.5);
				double d = ut - ((double)ix + 0.5);

				static const double m = -0.75;
				double c0 = (( (m    )*d - 2.0*m    )*d +   m)*d;
				double c1 = (( (m+2.0)*d -     m-3.0)*d      )*d + 1.0;
				double c2 = ((-(m+2.0)*d + 2.0*m+3.0)*d -   m)*d;
				double c3 = ((-(m    )*d +     m    )*d      )*d;

				double c03		= c0+c3;
				double k1 = d < 0.5 ? d < 1e-5 ? -m : c2 / d : d > 1-1e-5 ? -m : c1 / (1-d);
				double kx = d < 0.5 ? c1 - k1*(1-d) : c2 - k1*d;

				if (ut < ud0 || ut > ud1) {
					c0 = 0;
					k1 = 1.0;
					kx = 0.0;
					c3 = 0;
				}

				double blue		= -c0*4;
				double green	= k1 - 1.0 + 128.0f/255.0f;
				double red		= kx * 2;
				double alpha	= -c3*4;

				uint8 ib = VDClampedRoundFixedToUint8Fast((float)blue);
				uint8 ig = VDClampedRoundFixedToUint8Fast((float)green);
				uint8 ir = VDClampedRoundFixedToUint8Fast((float)red);
				uint8 ia = VDClampedRoundFixedToUint8Fast((float)alpha);

				p0[x] = (uint32)ib + ((uint32)ig << 8) + ((uint32)ir << 16) + ((uint32)ia << 24);

				u += dudx;
			}
		} else {
			for(int x = 0; x < texw; ++x) {
				int ix = VDFloorToInt(u - 0.5);
				double d = u - ((double)ix + 0.5);

				static const double m = -0.75;
				double c0 = (( (m    )*d - 2.0*m    )*d +   m)*d;
				double c1 = (( (m+2.0)*d -     m-3.0)*d      )*d + 1.0;
				double c2 = ((-(m+2.0)*d + 2.0*m+3.0)*d -   m)*d;
				double c3 = ((-(m    )*d +     m    )*d      )*d;

				double k0 = d*(1-d)*m;
				double k2 = d*(1-d)*m;

				double c1bi = d*k0;
				double c2bi = (1-d)*k2;
				double c1ex = c1-c1bi;
				double c2ex = c2-c2bi;

				double o1 = c2ex/(c1ex+c2ex)-d;

				double blue		= d;							// bilinear offset - p0 and p3
				double green	= o1*4;							// bilinear offset - p1 and p2
				double red		= (d*(1-d))*4;					// shift factor between the two
				double alpha	= d;							// lerp constant between p0 and p3

				uint8 ib = VDClampedRoundFixedToUint8Fast((float)blue * 127.0f/255.0f + 128.0f/255.0f) ^ 0x80;
				uint8 ig = VDClampedRoundFixedToUint8Fast((float)green * 127.0f/255.0f + 128.0f/255.0f) ^ 0x80;
				uint8 ir = VDClampedRoundFixedToUint8Fast((float)red);
				uint8 ia = VDClampedRoundFixedToUint8Fast((float)alpha);

#if 0
				double fb = (sint8)ib / 127.0f;
				double fg = (sint8)ig / 127.0f;
				double fr = (double)ir / 255.0f;
				double fa = (double)ia / 255.0f;

				double g0 = fr*0.25f*0.75f;
				double g1 = 2*(0.5f + fr*0.25f*0.75f);
				double d1 = 0.25f * fg + d;
				double g2 = fr*0.25f*0.75f;

				double cr0 = -g0*(1-d);
				double cr1 = -g0*d + g1*(1-d1);
				double cr2 = g1*d1 + -g2*(1-d);
				double cr3 = -g2*d;

				if (fabsf(cr0-c0) > 0.01f)
					__debugbreak();
				if (fabsf(cr1-c1) > 0.01f)
					__debugbreak();
				if (fabsf(cr2-c2) > 0.01f)
					__debugbreak();
				if (fabsf(cr3-c3) > 0.01f)
					__debugbreak();
#endif

	//			p0[x] = (uint32)ib + ((uint32)ig << 8);
				p0[x] = (uint32)ib + ((uint32)ig << 8) + ((uint32)ir << 16) + ((uint32)ia << 24);

				u += dudx;
			}
		}

		VDVERIFY(SUCCEEDED(pTexture->UnlockRect(0)));
		return texw;
	}
}

bool VDVideoDisplayMinidriverDX9::InitBicubicPS2Filters(int w, int h) {
	// requires PS2.0 path
	if (mCubicMode != VDVideoDisplayDX9Manager::kCubicUsePS1_1Path && mCubicMode != VDVideoDisplayDX9Manager::kCubicUsePS1_4Path)
		return false;

	bool mode1_4 = (mCubicMode == VDVideoDisplayDX9Manager::kCubicUsePS1_4Path);

	// update horiz filter
	if (!mpD3DInterpFilterTextureH || mInterpFilterHSize != w) {
		int newtexw = GeneratePS2CubicTexture(mpManager, w, mSource.pixmap.w, mpD3DInterpFilterTextureH, mInterpFilterHSize, mode1_4);
		if (newtexw < 0)
			return false;

		mInterpFilterHTexSize = newtexw;
	}

	// update vert filter
	if (!mpD3DInterpFilterTextureV || mInterpFilterVSize != h) {
		int newtexw = GeneratePS2CubicTexture(mpManager, h, mSource.pixmap.h, mpD3DInterpFilterTextureV, mInterpFilterVSize, mode1_4);
		if (newtexw < 0)
			return false;

		mInterpFilterVTexSize = newtexw;
	}
	return true;
}

void VDVideoDisplayMinidriverDX9::ShutdownBicubicPS2Filters() {
	mpD3DInterpFilterTextureH = NULL;
	mpD3DInterpFilterTextureV = NULL;
	mInterpFilterHSize = 0;
	mInterpFilterHTexSize = 0;
	mInterpFilterVSize = 0;
	mInterpFilterVTexSize = 0;
}

void VDVideoDisplayMinidriverDX9::Shutdown() {
	mpUploadContext = NULL;

	if (mpFontRenderer) {
		mpFontRenderer->Shutdown();
		mpFontRenderer = NULL;
	}

	ShutdownBicubic();
	ShutdownBicubicPS2Filters();

	mpSwapChain = NULL;
	mSwapChainW = 0;
	mSwapChainH = 0;

	mpVideoManager = NULL;

	if (mpManager) {
		if (mbFullScreenSet) {
			mbFullScreenSet = false;
			mpManager->AdjustFullScreen(false);
		}

		VDDeinitDirect3D9(mpManager, this);
		mpManager = NULL;
	}

	mbCubicAttempted = false;
}

bool VDVideoDisplayMinidriverDX9::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (mSource.pixmap.w == info.pixmap.w && mSource.pixmap.h == info.pixmap.h && mSource.pixmap.format == info.pixmap.format && mSource.pixmap.pitch == info.pixmap.pitch) {
		mSource = info;
		return true;
	}
	return false;
}

bool VDVideoDisplayMinidriverDX9::IsValid() {
	return mpD3DDevice != 0;
}

void VDVideoDisplayMinidriverDX9::SetFilterMode(FilterMode mode) {
	mPreferredFilter = mode;

	if (mode != kFilterBicubic && mode != kFilterAnySuitable) {
		ShutdownBicubicPS2Filters();

		if (mbCubicInitialized)
			ShutdownBicubic();
	}
}

void VDVideoDisplayMinidriverDX9::SetFullScreen(bool fs) {
	if (mbFullScreen != fs) {
		mbFullScreen = fs;

		if (mpManager) {
			if (mbFullScreenSet != fs) {
				mbFullScreenSet = fs;
				mpManager->AdjustFullScreen(fs);
			}
		}
	}
}

bool VDVideoDisplayMinidriverDX9::Tick(int id) {
	return true;
}

void VDVideoDisplayMinidriverDX9::Poll() {
	if (mbSwapChainPresentPending)
		UpdateScreen(mrClient, kModeVSync, true);
}

bool VDVideoDisplayMinidriverDX9::Resize() {
	mbSwapChainImageValid = false;
	GetClientRect(mhwnd, &mrClient);
	return true;
}

bool VDVideoDisplayMinidriverDX9::Update(UpdateMode mode) {
	int fieldMask = 3;

	switch(mode & kModeFieldMask) {
		case kModeEvenField:
			fieldMask = 1;
			break;

		case kModeOddField:
			fieldMask = 2;
			break;

		case kModeAllFields:
			break;
	}

	if (!mpUploadContext->Update(mSource.pixmap, fieldMask))
		return false;

	mbSwapChainImageValid = false;

	return true;
}

void VDVideoDisplayMinidriverDX9::Refresh(UpdateMode mode) {
	if (mrClient.right > 0 && mrClient.bottom > 0) {
		Paint(NULL, mrClient, mode);
	}
}

bool VDVideoDisplayMinidriverDX9::Paint(HDC, const RECT& rClient, UpdateMode updateMode) {
	return (mbSwapChainImageValid || UpdateBackbuffer(rClient)) && UpdateScreen(rClient, updateMode, 0 != (updateMode & kModeVSync));
}

void VDVideoDisplayMinidriverDX9::SetLogicalPalette(const uint8 *pLogicalPalette) {
}

bool VDVideoDisplayMinidriverDX9::UpdateBackbuffer(const RECT& rClient0) {
	int rtw = mpManager->GetMainRTWidth();
	int rth = mpManager->GetMainRTHeight();
	RECT rClient = rClient0;
	if (mbFullScreen) {
		rClient.right = rtw;
		rClient.bottom = rth;
	}

	RECT rClippedClient={0,0,std::min<int>(rClient.right, rtw), std::min<int>(rClient.bottom, rth)};

	// Make sure the device is sane.
	if (!mpManager->CheckDevice())
		return false;

	// Check if we need to create or resize the swap chain.
	if (mSwapChainW >= rClippedClient.right + 128 || mSwapChainH >= rClippedClient.bottom + 128) {
		mpSwapChain = NULL;
		mSwapChainW = 0;
		mSwapChainH = 0;
	}

	if (!mbFullScreen && (!mpSwapChain || mSwapChainW < rClippedClient.right || mSwapChainH < rClippedClient.bottom)) {
		int scw = std::min<int>((rClippedClient.right + 127) & ~127, rtw);
		int sch = std::min<int>((rClippedClient.bottom + 127) & ~127, rth);

		VDDEBUG("Resizing swap chain to %dx%d\n", scw, sch);

		if (!mpManager->CreateSwapChain(scw, sch, ~mpSwapChain))
			return false;

		mSwapChainW = scw;
		mSwapChainH = sch;
	}

	// Do we need to switch bicubic modes?
	FilterMode mode = mPreferredFilter;

	if (mode == kFilterAnySuitable)
		mode = kFilterBicubic;

	// bicubic modes cannot clip
	if (rClient.right != rClippedClient.right || rClient.bottom != rClippedClient.bottom)
		mode = kFilterBilinear;

	if (mode != kFilterBicubic && mbCubicInitialized)
		ShutdownBicubic();
	else if (mode == kFilterBicubic && !mbCubicInitialized && !mbCubicAttempted)
		InitBicubic();


	const D3DMATRIX ident={
		1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1
	};

	D3D_DO(SetTransform(D3DTS_WORLD, &ident));
	D3D_DO(SetTransform(D3DTS_VIEW, &ident));
	D3D_DO(SetTransform(D3DTS_PROJECTION, &ident));

	D3D_DO(SetStreamSource(0, mpManager->GetVertexBuffer(), 0, sizeof(Vertex)));
	D3D_DO(SetIndices(mpManager->GetIndexBuffer()));
	D3D_DO(SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2));
	D3D_DO(SetRenderState(D3DRS_LIGHTING, FALSE));
	D3D_DO(SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
	D3D_DO(SetRenderState(D3DRS_ZENABLE, FALSE));
	D3D_DO(SetRenderState(D3DRS_ALPHATESTENABLE, FALSE));
	D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
	D3D_DO(SetRenderState(D3DRS_STENCILENABLE, FALSE));
	D3D_DO(SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
	D3D_DO(SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
	D3D_DO(SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2));

	const D3DPRESENT_PARAMETERS& pparms = mpManager->GetPresentParms();

	VDVideoDisplayDX9Manager::EffectContext ctx;

	ctx.mpSourceTexture1 = mpUploadContext->GetD3DTexture();
	ctx.mpSourceTexture2 = NULL;
	ctx.mpSourceTexture3 = NULL;
	ctx.mpInterpFilterH = NULL;
	ctx.mpInterpFilterV = NULL;
	ctx.mSourceW = mSource.pixmap.w;
	ctx.mSourceH = mSource.pixmap.h;

	D3DSURFACE_DESC desc;

	HRESULT hr = ctx.mpSourceTexture1->GetLevelDesc(0, &desc);
	if (FAILED(hr))
		return false;

	ctx.mSourceTexW = desc.Width;
	ctx.mSourceTexH = desc.Height;
	ctx.mInterpHTexW = 1;
	ctx.mInterpHTexH = 1;
	ctx.mInterpVTexW = 1;
	ctx.mInterpVTexH = 1;

	vdrefptr<IDirect3DSurface9> pRTMain;

	mpManager->SetSwapChainActive(NULL);

	if (mpSwapChain) {
		IDirect3DSwapChain9 *sc = mpSwapChain->GetD3DSwapChain();
		hr = sc->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, ~pRTMain);
		if (FAILED(hr))
			return false;
	} else {
		mpManager->SetSwapChainActive(NULL);
		mpD3DDevice->GetRenderTarget(0, ~pRTMain);
	}

	mbSwapChainImageValid = false;

	bool bSuccess = false;

	if (mColorOverride) {
		mpManager->SetSwapChainActive(mpSwapChain);

		D3DRECT rClear;
		rClear.x1 = rClient.left;
		rClear.y1 = rClient.top;
		rClear.x2 = rClient.right;
		rClear.y2 = rClient.bottom;
		HRESULT hr = mpD3DDevice->Clear(1, &rClear, D3DCLEAR_TARGET, mColorOverride, 0.0f, 0);

		bSuccess = SUCCEEDED(hr);
	} else if (mbCubicInitialized &&
		(uint32)rClient.right <= pparms.BackBufferWidth &&
		(uint32)rClient.bottom <= pparms.BackBufferHeight &&
		(uint32)mSource.pixmap.w <= pparms.BackBufferWidth &&
		(uint32)mSource.pixmap.h <= pparms.BackBufferHeight
		)
	{
		int cubicMode = mCubicMode;

		if (cubicMode == VDVideoDisplayDX9Manager::kCubicUsePS1_1Path || cubicMode == VDVideoDisplayDX9Manager::kCubicUsePS1_4Path) {
			if (!InitBicubicPS2Filters(rClient.right, rClient.bottom))
				cubicMode = VDVideoDisplayDX9Manager::kCubicUseFF3Path;
			else {
				ctx.mpInterpFilterH = mpD3DInterpFilterTextureH;
				ctx.mpInterpFilterV = mpD3DInterpFilterTextureV;
				ctx.mInterpHTexW = mInterpFilterHTexSize;
				ctx.mInterpHTexH = 1;
				ctx.mInterpVTexW = mInterpFilterVTexSize;
				ctx.mInterpVTexH = 1;
			}
		}

		switch(cubicMode) {
		case VDVideoDisplayDX9Manager::kCubicUsePS1_4Path:
			bSuccess = mpVideoManager->RunEffect(ctx, rClient, g_technique_bicubic1_4, pRTMain);
			break;
		case VDVideoDisplayDX9Manager::kCubicUsePS1_1Path:
			bSuccess = mpVideoManager->RunEffect(ctx, rClient, g_technique_bicubic1_1, pRTMain);
			break;
		case VDVideoDisplayDX9Manager::kCubicUseFF3Path:
			bSuccess = mpVideoManager->RunEffect(ctx, rClient, g_technique_bicubicFF3, pRTMain);
			break;
		case VDVideoDisplayDX9Manager::kCubicUseFF2Path:
			bSuccess = mpVideoManager->RunEffect(ctx, rClient, g_technique_bicubicFF2, pRTMain);
			break;
		}
	} else {
		if (mPreferredFilter == kFilterPoint)
			bSuccess = mpVideoManager->RunEffect(ctx, rClient, g_technique_point, pRTMain);
		else
			bSuccess = mpVideoManager->RunEffect(ctx, rClient, g_technique_bilinear, pRTMain);
	}

	pRTMain = NULL;

	if (mbDisplayDebugInfo && mpFontRenderer) {
		if (mpFontRenderer->Begin()) {
			VDStringA s;

			const char *modestr = "point";

			switch(mode) {
				case kFilterBilinear:
					modestr = "bilinear";
					break;
				case kFilterBicubic:
					modestr = "bicubic";
					break;
			}

			VDStringA desc;
			GetFormatString(mSource, desc);
			s.sprintf("Direct3D9 minidriver - %s (%s)  Average present time: %6.2fms", desc.c_str(), modestr, mPresentHistory.mAveragePresentTime * 1000.0);
			mpFontRenderer->DrawTextLine(10, rClient.bottom - 40, 0xFFFFFF00, 0, s.c_str());

			s.sprintf("Target scanline: %7.2f  Average bracket [%7.2f,%7.2f]  Last bracket [%4d,%4d]  Poll count %5d"
					, mPresentHistory.mScanlineTarget
					, mPresentHistory.mAverageStartScanline
					, mPresentHistory.mAverageEndScanline
					, mPresentHistory.mLastBracketY1
					, mPresentHistory.mLastBracketY2
					, mPresentHistory.mPollCount);
			mPresentHistory.mPollCount = 0;
			mpFontRenderer->DrawTextLine(10, rClient.bottom - 20, 0xFFFFFF00, 0, s.c_str());

			mpFontRenderer->End();
		}
	}

	if (bSuccess && !mpManager->EndScene())
		bSuccess = false;

	mpManager->Flush();
	mpManager->SetSwapChainActive(NULL);

	hr = E_FAIL;

	if (!bSuccess) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Render failed -- applying boot to the head.\n");

		// TODO: Need to free all DEFAULT textures before proceeding

		if (!mpManager->Reset())
			return false;

	} else {
		mbSwapChainImageValid = true;
		mbSwapChainPresentPending = true;
		mbSwapChainPresentPolling = false;
	}

	return bSuccess;
}

bool VDVideoDisplayMinidriverDX9::UpdateScreen(const RECT& rClient, UpdateMode updateMode, bool polling) {
	if (!mbSwapChainImageValid)
		return false;

	HRESULT hr;
	if (mbFullScreen)
		hr = mpManager->PresentFullScreen(!polling);
	else {
		hr = mpManager->PresentSwapChain(mpSwapChain, &rClient, mhwnd, (updateMode & kModeVSync) != 0, !polling || !mbSwapChainPresentPolling, polling, mSyncDelta, mPresentHistory);
		mbSwapChainPresentPolling = false;
	}

	if (hr == S_FALSE)
		return true;

	// Workaround for Windows Vista DWM composition chain not updating.
	if (!mbFullScreen && mbFirstPresent) {
		SetWindowPos(mhwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
		mbFirstPresent = false;
	}

	mbSwapChainPresentPending = false;
	mbSwapChainPresentPolling = false;
	VDASSERT(!mPresentHistory.mbPresentPending);

	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Render failed -- applying boot to the head.\n");

		// TODO: Need to free all DEFAULT textures before proceeding

		if (!mpManager->Reset())
			return false;
	} else
		mSource.mpCB->RequestNextFrame();

	return true;
}
