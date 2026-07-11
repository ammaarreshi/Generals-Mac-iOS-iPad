// GLM_ENABLE_EXPERIMENTAL is passed via target_compile_definitions in CMakeLists.txt.
// Do NOT redefine it here -- that causes -Wmacro-redefined with Clang.

#include "d3dx8core.h"

// GeneralsX @build felipebraz 20/06/2025 GLI causes make_vec4 ambiguity with Apple Clang (GLM version mismatch).
// On macOS, exclude GLI and use stub implementations for the surface scaling path.
#ifndef __APPLE__
#include <gli/gli.hpp>
#include <gli/generate_mipmaps.hpp>
#endif

HRESULT WINAPID3DXGetErrorStringA(HRESULT hr, LPSTR pBuffer, UINT BufferLen)
{
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI
D3DXCreateTexture(LPDIRECT3DDEVICE8 pDevice,
	UINT Width,
	UINT Height,
	UINT MipLevels,
	DWORD Usage,
	D3DFORMAT Format,
	D3DPOOL Pool,
	LPDIRECT3DTEXTURE8 *ppTexture)
{
	// GeneralsX @bugfix fbraz 04/05/2026 Headless replay can request texture creation before DX8 device initialization.
	if (ppTexture == nullptr || pDevice == nullptr)
	{
		if (ppTexture != nullptr)
		{
			*ppTexture = nullptr;
		}

		return D3DERR_INVALIDCALL;
	}

	return pDevice->CreateTexture(Width, Height, MipLevels, Usage, Format, Pool, ppTexture);
}

#include <iostream>

HRESULT WINAPI
D3DXCreateTextureFromFileExA(
	LPDIRECT3DDEVICE8 pDevice,
	LPCSTR pSrcFile,
	UINT Width,
	UINT Height,
	UINT MipLevels,
	DWORD Usage,
	D3DFORMAT Format,
	D3DPOOL Pool,

	DWORD Filter,
	DWORD MipFilter,
	D3DCOLOR ColorKey,
	D3DXIMAGE_INFO *pSrcInfo,
	PALETTEENTRY *pPalette,

	LPDIRECT3DTEXTURE8 *ppTexture)
{
	HRESULT hr;

	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI
D3DXLoadSurfaceFromSurface(
	LPDIRECT3DSURFACE8 pDestSurface,
	CONST PALETTEENTRY *pDestPalette,
	CONST RECT *pDestRect,
	LPDIRECT3DSURFACE8 pSrcSurface,
	CONST PALETTEENTRY *pSrcPalette,
	CONST RECT *pSrcRect,
	DWORD Filter,
	D3DCOLOR ColorKey)
{
	D3DSURFACE_DESC descSrc;
	D3DSURFACE_DESC descDest;

	//DEBUG_ASSERTCRASH(pDestPalette == NULL);

	pSrcSurface->GetDesc(&descSrc);
	pDestSurface->GetDesc(&descDest);

	if (descSrc.Format != descDest.Format)
	{
		// Currently we only support scaling between formats of the same type
		return D3DERR_INVALIDCALL;
	}

	D3DLOCKED_RECT srcRect;
	pSrcSurface->LockRect(&srcRect, NULL, 0);

	D3DLOCKED_RECT destRect;
	pDestSurface->LockRect(&destRect, NULL, 0);

	// Fast path: No scaling needs to be done if the dimensions are the same
	if (descDest.Width == descSrc.Width && descDest.Height == descSrc.Height)
	{
		// Copy the data directly
		memcpy(destRect.pBits, srcRect.pBits, srcRect.Pitch * descSrc.Height);
		pDestSurface->UnlockRect();
		pSrcSurface->UnlockRect();
		return D3D_OK;
	}

#ifndef __APPLE__
	// GeneralsX @bugfix Antigravity 26/06/2026 Linux: GLI lacks support for A4R4G4B4/R5G6B5 formats.
	// We use manual box filter downsampling for these formats to fix black infantry rendering.
	// GeneralsX @bugfix Codex 10/07/2026 Guard typed pixel writes against a mismatched destination format.
	if (descDest.Format == descSrc.Format &&
		descDest.Width == descSrc.Width / 2 && descDest.Height == descSrc.Height / 2)
	{
		if (descSrc.Format == D3DFMT_A4R4G4B4)
		{
			// Box filter for A4R4G4B4: average 2x2 blocks
			const uint16_t *src = (const uint16_t *)srcRect.pBits;
			uint16_t *dst = (uint16_t *)destRect.pBits;
			uint32_t srcPitch16 = srcRect.Pitch / 2;
			uint32_t dstPitch16 = destRect.Pitch / 2;

			for (uint32_t y = 0; y < descDest.Height; y++)
			{
				for (uint32_t x = 0; x < descDest.Width; x++)
				{
					uint32_t sx = x * 2;
					uint32_t sy = y * 2;
					uint16_t p00 = src[sy * srcPitch16 + sx];
					uint16_t p10 = src[sy * srcPitch16 + sx + 1];
					uint16_t p01 = src[(sy + 1) * srcPitch16 + sx];
					uint16_t p11 = src[(sy + 1) * srcPitch16 + sx + 1];

					// Extract and average each channel (A4 R4 G4 B4)
					uint32_t a = (((p00 >> 12) & 0x0F) + ((p10 >> 12) & 0x0F) +
					              ((p01 >> 12) & 0x0F) + ((p11 >> 12) & 0x0F) + 2) >> 2;
					uint32_t r = (((p00 >> 8) & 0x0F) + ((p10 >> 8) & 0x0F) +
					              ((p01 >> 8) & 0x0F) + ((p11 >> 8) & 0x0F) + 2) >> 2;
					uint32_t g = (((p00 >> 4) & 0x0F) + ((p10 >> 4) & 0x0F) +
					              ((p01 >> 4) & 0x0F) + ((p11 >> 4) & 0x0F) + 2) >> 2;
					uint32_t b = ((p00 & 0x0F) + (p10 & 0x0F) +
					              (p01 & 0x0F) + (p11 & 0x0F) + 2) >> 2;

					dst[y * dstPitch16 + x] = (uint16_t)((a << 12) | (r << 8) | (g << 4) | b);
				}
			}
			pDestSurface->UnlockRect();
			pSrcSurface->UnlockRect();
			return D3D_OK;
		}
		else if (descSrc.Format == D3DFMT_R5G6B5)
		{
			// Box filter for R5G6B5: average 2x2 blocks
			const uint16_t *src = (const uint16_t *)srcRect.pBits;
			uint16_t *dst = (uint16_t *)destRect.pBits;
			uint32_t srcPitch16 = srcRect.Pitch / 2;
			uint32_t dstPitch16 = destRect.Pitch / 2;

			for (uint32_t y = 0; y < descDest.Height; y++)
			{
				for (uint32_t x = 0; x < descDest.Width; x++)
				{
					uint32_t sx = x * 2;
					uint32_t sy = y * 2;
					uint16_t p00 = src[sy * srcPitch16 + sx];
					uint16_t p10 = src[sy * srcPitch16 + sx + 1];
					uint16_t p01 = src[(sy + 1) * srcPitch16 + sx];
					uint16_t p11 = src[(sy + 1) * srcPitch16 + sx + 1];

					// Extract and average each channel (R5 G6 B5)
					uint32_t r = (((p00 >> 11) & 0x1F) + ((p11 >> 11) & 0x1F) +
					              ((p01 >> 11) & 0x1F) + ((p10 >> 11) & 0x1F) + 2) >> 2;
					uint32_t g = (((p00 >> 5) & 0x3F) + ((p11 >> 5) & 0x3F) +
					              ((p01 >> 5) & 0x3F) + ((p10 >> 5) & 0x3F) + 2) >> 2;
					uint32_t b = ((p00 & 0x1F) + (p11 & 0x1F) +
					              (p01 & 0x1F) + (p10 & 0x1F) + 2) >> 2;

					dst[y * dstPitch16 + x] = (uint16_t)((r << 11) | (g << 5) | b);
				}
			}
			pDestSurface->UnlockRect();
			pSrcSurface->UnlockRect();
			return D3D_OK;
		}
	}
	// Pick a compatible format
	gli::format imageFormat = gli::format::FORMAT_RGBA8_UNORM_PACK8;

	assert(descSrc.Format == D3DFMT_A8R8G8B8 || descSrc.Format == D3DFMT_A1R5G5B5 || descSrc.Format == D3DFMT_X8R8G8B8);
	if (descSrc.Format == D3DFMT_A8R8G8B8 || descSrc.Format == D3DFMT_X8R8G8B8)
	{
		imageFormat = gli::format::FORMAT_RGBA8_UNORM_PACK8;	
	}
	else if (descSrc.Format == D3DFMT_A1R5G5B5)
	{
		imageFormat = gli::format::FORMAT_A1RGB5_UNORM_PACK16;
	}
	else
	{
		return D3DERR_INVALIDCALL;
	}

	// Create two levels of mips, 0 and 1
	gli::texture2d texSrc(imageFormat, gli::extent2d(descSrc.Width, descSrc.Height), 2);

	// Copy the data to level 0
	memcpy(texSrc.data(), srcRect.pBits, texSrc.size());
	// Generate mip 1 from level 0
	gli::texture2d mipMap = gli::generate_mipmaps(texSrc, gli::filter::FILTER_LINEAR);

	if (mipMap.size(1) != destRect.Pitch * descDest.Height)
	{
		// The generated dimension would not be the same as the destination
		// This does not happen in the game, yet let's not allow it
		pDestSurface->UnlockRect();
		pSrcSurface->UnlockRect();
		return D3DERR_INVALIDCALL;
	}

	// Copy mip level 1 to the destination
	memcpy(destRect.pBits, mipMap.data(0,0,1), destRect.Pitch * descDest.Height);

	pDestSurface->UnlockRect();
	pSrcSurface->UnlockRect();

	return D3D_OK;
#else
	// GeneralsX @bugfix BenderAI 07/03/2026 macOS: GLI not available due to Apple Clang ambiguity.
	// Implement manual box filter downsampling for mipmap generation.
	// This is critical for terrain textures - without mipmaps, terrain renders black.
	// GeneralsX @bugfix Codex 10/07/2026 Guard typed pixel writes against a mismatched destination format.
	if (descDest.Format == descSrc.Format &&
		descDest.Width == descSrc.Width / 2 && descDest.Height == descSrc.Height / 2)
	{
		if (descSrc.Format == D3DFMT_A1R5G5B5)
		{
			// Box filter for A1R5G5B5: average 2x2 blocks
			const uint16_t *src = (const uint16_t *)srcRect.pBits;
			uint16_t *dst = (uint16_t *)destRect.pBits;
			uint32_t srcPitch16 = srcRect.Pitch / 2;
			uint32_t dstPitch16 = destRect.Pitch / 2;

			for (uint32_t y = 0; y < descDest.Height; y++)
			{
				for (uint32_t x = 0; x < descDest.Width; x++)
				{
					uint32_t sx = x * 2;
					uint32_t sy = y * 2;
					uint16_t p00 = src[sy * srcPitch16 + sx];
					uint16_t p10 = src[sy * srcPitch16 + sx + 1];
					uint16_t p01 = src[(sy + 1) * srcPitch16 + sx];
					uint16_t p11 = src[(sy + 1) * srcPitch16 + sx + 1];

					// Extract and average each channel (A1 R5 G5 B5)
					uint32_t r = (((p00 >> 10) & 0x1F) + ((p10 >> 10) & 0x1F) +
					              ((p01 >> 10) & 0x1F) + ((p11 >> 10) & 0x1F) + 2) >> 2;
					uint32_t g = (((p00 >> 5) & 0x1F) + ((p10 >> 5) & 0x1F) +
					              ((p01 >> 5) & 0x1F) + ((p11 >> 5) & 0x1F) + 2) >> 2;
					uint32_t b = ((p00 & 0x1F) + (p10 & 0x1F) +
					              (p01 & 0x1F) + (p11 & 0x1F) + 2) >> 2;
					// Alpha: majority vote (set if 2+ pixels have alpha set)
					uint32_t a = ((p00 >> 15) + (p10 >> 15) + (p01 >> 15) + (p11 >> 15)) >= 2 ? 1 : 0;

					dst[y * dstPitch16 + x] = (uint16_t)((a << 15) | (r << 10) | (g << 5) | b);
				}
			}
		}
		else if (descSrc.Format == D3DFMT_A8R8G8B8 || descSrc.Format == D3DFMT_X8R8G8B8)
		{
			// Box filter for A8R8G8B8/X8R8G8B8: average 2x2 blocks
			const uint32_t *src = (const uint32_t *)srcRect.pBits;
			uint32_t *dst = (uint32_t *)destRect.pBits;
			uint32_t srcPitch32 = srcRect.Pitch / 4;
			uint32_t dstPitch32 = destRect.Pitch / 4;

			for (uint32_t y = 0; y < descDest.Height; y++)
			{
				for (uint32_t x = 0; x < descDest.Width; x++)
				{
					uint32_t sx = x * 2;
					uint32_t sy = y * 2;
					uint32_t p00 = src[sy * srcPitch32 + sx];
					uint32_t p10 = src[sy * srcPitch32 + sx + 1];
					uint32_t p01 = src[(sy + 1) * srcPitch32 + sx];
					uint32_t p11 = src[(sy + 1) * srcPitch32 + sx + 1];

					uint32_t a = (((p00 >> 24) & 0xFF) + ((p10 >> 24) & 0xFF) +
					              ((p01 >> 24) & 0xFF) + ((p11 >> 24) & 0xFF) + 2) >> 2;
					uint32_t r = (((p00 >> 16) & 0xFF) + ((p10 >> 16) & 0xFF) +
					              ((p01 >> 16) & 0xFF) + ((p11 >> 16) & 0xFF) + 2) >> 2;
					uint32_t g = (((p00 >> 8) & 0xFF) + ((p10 >> 8) & 0xFF) +
					              ((p01 >> 8) & 0xFF) + ((p11 >> 8) & 0xFF) + 2) >> 2;
					uint32_t b = ((p00 & 0xFF) + (p10 & 0xFF) +
					              (p01 & 0xFF) + (p11 & 0xFF) + 2) >> 2;

					dst[y * dstPitch32 + x] = (a << 24) | (r << 16) | (g << 8) | b;
				}
			}
		}
		else if (descSrc.Format == D3DFMT_A4R4G4B4)
		{
			// Box filter for A4R4G4B4: average 2x2 blocks
			const uint16_t *src = (const uint16_t *)srcRect.pBits;
			uint16_t *dst = (uint16_t *)destRect.pBits;
			uint32_t srcPitch16 = srcRect.Pitch / 2;
			uint32_t dstPitch16 = destRect.Pitch / 2;

			for (uint32_t y = 0; y < descDest.Height; y++)
			{
				for (uint32_t x = 0; x < descDest.Width; x++)
				{
					uint32_t sx = x * 2;
					uint32_t sy = y * 2;
					uint16_t p00 = src[sy * srcPitch16 + sx];
					uint16_t p10 = src[sy * srcPitch16 + sx + 1];
					uint16_t p01 = src[(sy + 1) * srcPitch16 + sx];
					uint16_t p11 = src[(sy + 1) * srcPitch16 + sx + 1];

					// Extract and average each channel (A4 R4 G4 B4)
					uint32_t a = (((p00 >> 12) & 0x0F) + ((p10 >> 12) & 0x0F) +
					              ((p01 >> 12) & 0x0F) + ((p11 >> 12) & 0x0F) + 2) >> 2;
					uint32_t r = (((p00 >> 8) & 0x0F) + ((p10 >> 8) & 0x0F) +
					              ((p01 >> 8) & 0x0F) + ((p11 >> 8) & 0x0F) + 2) >> 2;
					uint32_t g = (((p00 >> 4) & 0x0F) + ((p10 >> 4) & 0x0F) +
					              ((p01 >> 4) & 0x0F) + ((p11 >> 4) & 0x0F) + 2) >> 2;
					uint32_t b = ((p00 & 0x0F) + (p10 & 0x0F) +
					              (p01 & 0x0F) + (p11 & 0x0F) + 2) >> 2;

					dst[y * dstPitch16 + x] = (uint16_t)((a << 12) | (r << 8) | (g << 4) | b);
				}
			}
		}
		else if (descSrc.Format == D3DFMT_R5G6B5)
		{
			// Box filter for R5G6B5: average 2x2 blocks
			const uint16_t *src = (const uint16_t *)srcRect.pBits;
			uint16_t *dst = (uint16_t *)destRect.pBits;
			uint32_t srcPitch16 = srcRect.Pitch / 2;
			uint32_t dstPitch16 = destRect.Pitch / 2;

			for (uint32_t y = 0; y < descDest.Height; y++)
			{
				for (uint32_t x = 0; x < descDest.Width; x++)
				{
					uint32_t sx = x * 2;
					uint32_t sy = y * 2;
					uint16_t p00 = src[sy * srcPitch16 + sx];
					uint16_t p10 = src[sy * srcPitch16 + sx + 1];
					uint16_t p01 = src[(sy + 1) * srcPitch16 + sx];
					uint16_t p11 = src[(sy + 1) * srcPitch16 + sx + 1];

					// Extract and average each channel (R5 G6 B5)
					uint32_t r = (((p00 >> 11) & 0x1F) + ((p11 >> 11) & 0x1F) +
					              ((p01 >> 11) & 0x1F) + ((p10 >> 11) & 0x1F) + 2) >> 2;
					uint32_t g = (((p00 >> 5) & 0x3F) + ((p11 >> 5) & 0x3F) +
					              ((p01 >> 5) & 0x3F) + ((p10 >> 5) & 0x3F) + 2) >> 2;
					uint32_t b = ((p00 & 0x1F) + (p11 & 0x1F) +
					              (p01 & 0x1F) + (p10 & 0x1F) + 2) >> 2;

					dst[y * dstPitch16 + x] = (uint16_t)((r << 11) | (g << 5) | b);
				}
			}
		}
		else
		{
			pDestSurface->UnlockRect();
			pSrcSurface->UnlockRect();
			return D3DERR_INVALIDCALL;
		}

		pDestSurface->UnlockRect();
		pSrcSurface->UnlockRect();
		return D3D_OK;
	}

	// Non-power-of-two scaling not supported
	pDestSurface->UnlockRect();
	pSrcSurface->UnlockRect();
	return D3DERR_INVALIDCALL;
#endif
}

HRESULT WINAPI
D3DXGetErrorStringA(
	HRESULT hr,
	LPSTR pBuffer,
	UINT BufferLen)
{
	return D3DERR_INVALIDCALL;
}

// Taken and adopted from Wine 3.21
HRESULT WINAPI
D3DXFilterTexture(
	LPDIRECT3DBASETEXTURE8 pBaseTexture,
	CONST PALETTEENTRY *pPalette,
	UINT SrcLevel,
	DWORD Filter)
{
	HRESULT hr = D3DERR_INVALIDCALL;
	if (SrcLevel == D3DX_DEFAULT)
	{
		SrcLevel = 0;
	}
	else if (SrcLevel >= pBaseTexture->GetLevelCount())
	{
		return D3DERR_INVALIDCALL;
	}

	D3DRESOURCETYPE type;
	switch (type = pBaseTexture->GetType())
	{
		case D3DRTYPE_TEXTURE:
		{
			IDirect3DTexture8 *tex = (IDirect3DTexture8 *)pBaseTexture;
			IDirect3DSurface8 *topsurf, *mipsurf;
			D3DSURFACE_DESC desc;
			int i;

			tex->GetLevelDesc(SrcLevel, &desc);
			if (Filter == D3DX_DEFAULT)
			{
				Filter = D3DX_FILTER_BOX;
			}

			int Level = SrcLevel + 1;
			hr = tex->GetSurfaceLevel(SrcLevel, &topsurf);
			if (FAILED(hr))
			{
				return hr;
			}

			while (tex->GetSurfaceLevel(Level, &mipsurf) == D3D_OK)
			{
				// Copy the data
				D3DXLoadSurfaceFromSurface(mipsurf, NULL, NULL, topsurf, NULL, NULL, Filter, 0);

				// Release the surface
				mipsurf->Release();
				topsurf = mipsurf;

				Level++;
			}

			topsurf->Release();
			if (FAILED(hr))
			{
				return hr;
			}
		}
		return D3D_OK;
	}

	return D3D_OK;
}

HRESULT WINAPI
D3DXCreateCubeTexture(
	LPDIRECT3DDEVICE8 pDevice,
	UINT Size,
	UINT MipLevels,
	DWORD Usage,
	D3DFORMAT Format,
	D3DPOOL Pool,
	LPDIRECT3DCUBETEXTURE8 *ppCubeTexture)
{
	// Also unused by the game
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI
D3DXCreateVolumeTexture(
	LPDIRECT3DDEVICE8 pDevice,
	UINT Width,
	UINT Height,
	UINT Depth,
	UINT MipLevels,
	DWORD Usage,
	D3DFORMAT Format,
	D3DPOOL Pool,
	LPDIRECT3DVOLUMETEXTURE8 *ppVolumeTexture)
{
	// Used to create volume textures by the texture loader
	// Actually not used in the game
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI
D3DXAssembleShader(
	LPCVOID pSrcData,
	UINT SrcDataLen,
	DWORD Flags,
	LPD3DXBUFFER *ppConstants,
	LPD3DXBUFFER *ppCompiledShader,
	LPD3DXBUFFER *ppCompilationErrors)
{
	// Called to create water shader amongst other things
	// Code seems to be handle assembly failing
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI
D3DXAssembleShaderFromFileA(
	LPCSTR pSrcFile,
	DWORD Flags,
	LPD3DXBUFFER *ppConstants,
	LPD3DXBUFFER *ppCompiledShader,
	LPD3DXBUFFER *ppCompilationErrors)
{
	// Not required by the game
	return D3DERR_INVALIDCALL;
}

// Taken from Wine at ca46949
static UINT Get_TexCoord_Size_From_FVF(DWORD FVF, int tex_num)
{
    return (((((FVF) >> (16 + (2 * (tex_num)))) + 1) & 0x03) + 1);
}

UINT WINAPI D3DXGetFVFVertexSize(DWORD FVF)
{
    DWORD size = 0;
    UINT i;
    UINT numTextures = (FVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

    if (FVF & D3DFVF_NORMAL) size += sizeof(D3DVECTOR);
    if (FVF & D3DFVF_DIFFUSE) size += sizeof(DWORD);
    if (FVF & D3DFVF_SPECULAR) size += sizeof(DWORD);
    if (FVF & D3DFVF_PSIZE) size += sizeof(DWORD);

    switch (FVF & D3DFVF_POSITION_MASK)
    {
        case D3DFVF_XYZ:    size += sizeof(D3DVECTOR); break;
        case D3DFVF_XYZRHW: size += 4 * sizeof(FLOAT); break;
        case D3DFVF_XYZB1:  size += 4 * sizeof(FLOAT); break;
        case D3DFVF_XYZB2:  size += 5 * sizeof(FLOAT); break;
        case D3DFVF_XYZB3:  size += 6 * sizeof(FLOAT); break;
        case D3DFVF_XYZB4:  size += 7 * sizeof(FLOAT); break;
        case D3DFVF_XYZB5:  size += 8 * sizeof(FLOAT); break;
		// Not present in D3D8, thus uncommented
        //case D3DFVF_XYZW:   size += 4 * sizeof(FLOAT); break;
    }

    for (i = 0; i < numTextures; i++)
    {
        size += Get_TexCoord_Size_From_FVF(FVF, i) * sizeof(FLOAT);
    }

    return size;
}
