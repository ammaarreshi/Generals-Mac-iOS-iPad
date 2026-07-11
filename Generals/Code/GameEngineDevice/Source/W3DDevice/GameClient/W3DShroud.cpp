/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: W3DShroud.cpp /////////////////////////////////////////////////////////////////////////////
// Created:   Mark Wilczynski, Jan 2002
// Desc:      Code to support rendering of shrouded units/terrain.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/BaseType.h"
#include "camera.h"
#include "simplevec.h"
#include "dx8wrapper.h"
#include "Common/MapObject.h"
#include "Common/PerfTimer.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DPoly.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "assetmgr.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "WW3D2/textureloader.h"
#include "Common/GlobalData.h"
#include "GameLogic/PartitionManager.h"


//-----------------------------------------------------------------------------

// In Global Data now

//#define SHROUD_COLOR	0x00ffffff //temporary test of gray shroud instead of pure black.
//#define MIN_SHROUD_LEVEL	0		//for gray fog

//Int SHROUD_COLOR=0x00808080; //temporary test of gray shroud instead of pure black.
//Int MIN_SHROUD_LEVEL=50;		//for gray fog

//#define SHROUD_COLOR	0x00eeeebff //temporary test of gray shroud instead of pure black.
//#define MIN_SHROUD_LEVEL	254		//for gray fog

//#define SHROUD_COLOR	0x00bbbbbb //temporary test of gray shroud instead of pure black.
//#define SHROUD_COLOR	0x00004080 //temporary test of blue shroud instead of pure black.

//#define MIN_SHROUD_LEVEL	100		//for black fog

//#define MAX_MAP_SHROUDSIZE	1024	//maximum number of shroud cells across entire map.
//#define MAX_VISIBLE_SHROUDSIZE	(MAX_MAP_SHROUDSIZE+1)	//maximum number of shroud vertices visible at any given time
#define DEFAULT_SHROUD_CELL_SIZE	MAP_XY_FACTOR	//assume shroud at same resolution as terrain cells.
#define DEFAULT_TERRAIN_SIZE 1024 //assumed size of largest terrain possible (in vertices)
#define DEFAULT_VISIBLE_TERRAIN 96	//assumed size of visible terrain cells.

//-----------------------------------------------------------------------------
W3DShroud::W3DShroud()
{
	m_finalFogData=nullptr;
	m_currentFogData=nullptr;
	m_pSrcTexture=nullptr;
	m_pDstTexture=nullptr;
	m_srcTextureData=nullptr;
	m_srcTexturePitch=0;
	m_dstTextureWidth=m_numMaxVisibleCellsX=0;
	m_dstTextureHeight=m_numMaxVisibleCellsY=0;
	m_boderShroudLevel = (W3DShroudLevel)TheGlobalData->m_shroudAlpha;	//assume border is black
	m_clearDstTexture = TRUE;	//force clearing of destination texture;

	m_cellWidth=DEFAULT_SHROUD_CELL_SIZE;
	m_cellHeight=DEFAULT_SHROUD_CELL_SIZE;
	m_numCellsX=0;
	m_numCellsY=0;
	m_shroudFilter=TextureFilterClass::FILTER_TYPE_DEFAULT;
}

//-----------------------------------------------------------------------------
W3DShroud::~W3DShroud()
{
	ReleaseResources();

	if (m_pSrcTexture)
		m_pSrcTexture->Release();

	delete [] m_finalFogData;
	delete [] m_currentFogData;

	m_drawFogOfWar=FALSE;
}

//-----------------------------------------------------------------------------
/**Called to initialize a new shroud for a new map.  Should be done after the map is loaded
   into the terrain object.  worldCellSize is the world-space dimensions of each shroud cell.
   The system will generate enough cells to cover the full map.
*/
void W3DShroud::init(WorldHeightMap *pMap, Real worldCellSizeX, Real worldCellSizeY)
{
	DEBUG_ASSERTCRASH( m_pSrcTexture == nullptr, ("ReAcquire of existing shroud textures"));
	DEBUG_ASSERTCRASH( pMap != nullptr, ("Shroud init with null WorldHeightMap"));

	Int dstTextureWidth=0;
	Int dstTextureHeight=0;
	m_cellWidth=worldCellSizeX;
	m_cellHeight=worldCellSizeY;

	//Precompute a bounding box for entire shroud layer
	if (pMap)
	{
		m_numCellsX = REAL_TO_INT_CEIL((Real)(pMap->getXExtent() - 1 - pMap->getBorderSizeInline()*2)*MAP_XY_FACTOR/m_cellWidth);
		m_numCellsY = REAL_TO_INT_CEIL((Real)(pMap->getYExtent() - 1 - pMap->getBorderSizeInline()*2)*MAP_XY_FACTOR/m_cellHeight);

		//Maximum visible cells will depend on maximum drawable terrain size plus 1 for partial cells (since
		//shroud cells are larger than terrain cells).
		dstTextureWidth=m_numMaxVisibleCellsX=REAL_TO_INT_FLOOR((Real)(pMap->getDrawWidth()-1)*MAP_XY_FACTOR/m_cellWidth)+1;
		dstTextureHeight=m_numMaxVisibleCellsY=REAL_TO_INT_FLOOR((Real)(pMap->getDrawHeight()-1)*MAP_XY_FACTOR/m_cellHeight)+1;

		dstTextureWidth = m_numCellsX;
		dstTextureHeight = m_numCellsY;

		dstTextureWidth += 2;	//enlarge by 2 pixels so we can have a border color all the way around.
		unsigned int depth = 1;
		dstTextureHeight += 2;	//enlarge by 2 pixels so we can have border color all the way around.
		TextureLoader::Validate_Texture_Size((unsigned int &)dstTextureWidth,(unsigned int &)dstTextureHeight, depth);
	}

	UnsignedInt srcWidth,srcHeight;

	srcWidth=m_numCellsX;
	//vertical size is larger by 1 pixel so that we have some unused pixels to use in clearing the video texture.
	//To clear the video texture, I will copy pixels from this unused area.  There is no other way to clear a video
  //memory texture to a known value because you can't lock it - only copy into it.
	srcHeight=m_numCellsY;
	srcHeight += 1;

#ifdef DO_FOG_INTERPOLATION
	m_finalFogData = new W3DShroudLevel[srcWidth*srcHeight];
	m_currentFogData = new W3DShroudLevel[srcWidth*srcHeight];
	//Clear the fog to black
	memset(m_currentFogData,0,srcWidth*srcHeight);
 	memset(m_finalFogData,0,srcWidth*srcHeight);
#endif

#if defined(RTS_DEBUG)
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
		m_pSrcTexture = DX8Wrapper::_Create_DX8_Surface(srcWidth,srcHeight, WW3D_FORMAT_A4R4G4B4);
	else
#endif
		m_pSrcTexture = DX8Wrapper::_Create_DX8_Surface(srcWidth,srcHeight, WW3D_FORMAT_R5G6B5);

	DEBUG_ASSERTCRASH( m_pSrcTexture != nullptr, ("Failed to Allocate Shroud Src Surface"));

	D3DLOCKED_RECT rect;

	//Get a pointer to source surface pixels.
	HRESULT res = m_pSrcTexture->LockRect(&rect,nullptr,D3DLOCK_NO_DIRTY_UPDATE);
	m_pSrcTexture->UnlockRect();

	DEBUG_ASSERTCRASH( res == D3D_OK, ("Failed to lock shroud src surface"));
	res = 0;// just to avoid compiler warnings

	m_srcTextureData=rect.pBits;
	m_srcTexturePitch=rect.Pitch;

	//clear entire texture to black
	memset(m_srcTextureData,0,m_srcTexturePitch*srcHeight);

#if defined(RTS_DEBUG)
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
		fillShroudData(TheGlobalData->m_shroudAlpha);	//initialize shroud to a known value
#endif

	if (dstTextureWidth != m_dstTextureWidth || dstTextureHeight != m_dstTextureHeight )	///@todo: Check if size has changed - probably never
		ReleaseResources();	//need a new sized shroud

	if (!m_pDstTexture )
	{	m_dstTextureWidth = dstTextureWidth;
		m_dstTextureHeight = dstTextureHeight;
		ReAcquireResources();	//allocate video memory surface
	}

	//Force a refresh of shroud data since we just created a new source texture.
	if (ThePartitionManager)
		ThePartitionManager->refreshShroudForLocalPlayer();
}

//-----------------------------------------------------------------------------
///Called on map reset.
void W3DShroud::reset()
{
	//Free old shroud data since it may no longer fit new map.
	if (m_pSrcTexture)
	{
		m_pSrcTexture->Release();
		m_pSrcTexture=nullptr;
	}

	delete [] m_finalFogData;
	m_finalFogData=nullptr;

	delete [] m_currentFogData;
	m_currentFogData=nullptr;

	m_clearDstTexture = TRUE;	//always refill the destination texture after a reset
}

//-----------------------------------------------------------------------------
///Release any resources that can't survive a D3D device reset.
void W3DShroud::ReleaseResources()
{
	REF_PTR_RELEASE (m_pDstTexture);
}

//-----------------------------------------------------------------------------
///Restore resources that are lost on D3D device reset.
Bool W3DShroud::ReAcquireResources()
{
		if (!m_dstTextureWidth)
			return TRUE;	//nothing to reaquire since shroud was never initialized with valid data

		DEBUG_ASSERTCRASH( m_pDstTexture == nullptr, ("ReAcquire of existing shroud texture"));

		// Create destination texture (stored in video memory).
		// Since we control the video memory copy, we can do partial updates more efficiently. Or do shift blits.
#if defined(RTS_DEBUG)
		if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
			m_pDstTexture = MSGNEW("TextureClass") TextureClass(m_dstTextureWidth,m_dstTextureHeight,WW3D_FORMAT_A4R4G4B4,MIP_LEVELS_1, TextureClass::POOL_DEFAULT);
		else
#endif
			m_pDstTexture = MSGNEW("TextureClass") TextureClass(m_dstTextureWidth,m_dstTextureHeight,WW3D_FORMAT_R5G6B5,MIP_LEVELS_1, TextureClass::POOL_DEFAULT);

		DEBUG_ASSERTCRASH( m_pDstTexture != nullptr, ("Failed ReAcquire of shroud texture"));

		if (!m_pDstTexture)
		{	//could not create a valid texture
			m_dstTextureWidth = 0;
			m_dstTextureHeight = 0;
			return FALSE;
		}
		m_pDstTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		m_pDstTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		m_pDstTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		m_clearDstTexture = TRUE;	//force clearing of destination texture first time it's used.

		return TRUE;
}

//-----------------------------------------------------------------------------
W3DShroudLevel W3DShroud::getShroudLevel(Int x, Int y)
{
	DEBUG_ASSERTCRASH( m_pSrcTexture != nullptr, ("Reading empty shroud"));
	// GeneralsX @bugfix Copilot 23/04/2026 Prevent invalid shroud sampling during campaign water rendering.
	if (!m_pSrcTexture || !m_srcTextureData || x < 0 || y < 0)
		return 0;

	if (x < m_numCellsX && y < m_numCellsY)
	{
		UnsignedShort pixel=*(UnsignedShort *)((Byte *)m_srcTextureData + x*2 + y*m_srcTexturePitch);

#if defined(RTS_DEBUG)
		if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
			//in this mode, alpha channel holds intensity
			return (W3DShroudLevel)((1.0f-((Real)(pixel >> 12)/15.0f))*255.0f);
		else
#endif
			//in this mode, green has the best precision at 6 bits.
			return (W3DShroudLevel)((Real)((pixel >> 5)&0x3f)/63.0f*255.0f);
	}
	return 0;
}

//-----------------------------------------------------------------------------
void W3DShroud::setShroudLevel(Int x, Int y, W3DShroudLevel level, Bool textureOnly)
{
	DEBUG_ASSERTCRASH( m_pSrcTexture != nullptr, ("Writing empty shroud.  Usually means that map failed to load."));

	if (!m_pSrcTexture)
		return;

	if (x < m_numCellsX && y < m_numCellsY)
	{
		if (level < TheGlobalData->m_shroudAlpha)
			level = TheGlobalData->m_shroudAlpha;

#if defined(RTS_DEBUG)
		if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
		{
			///@todo: optimize this case if we end up using fog shroud.
			Int redVal = TheGlobalData->m_shroudColor.red;
			Int greenVal = TheGlobalData->m_shroudColor.green;
			Int blueVal = TheGlobalData->m_shroudColor.blue;
//			Int redVal = (SHROUD_COLOR >> 16) & 0xff;
//			Int greenVal = (SHROUD_COLOR >> 8) & 0xff;
//			Int blueVal = SHROUD_COLOR & 0xff;
			Int alphaVal = 255 - level;

			UnsignedShort pixel=((blueVal>>4)&0xf) | (((greenVal>>4)&0xf)<<4) | (((redVal>>4)&0xf)<<8) | (((alphaVal>>4)&0xf)<<12);
			*(UnsignedShort *)((Byte *)m_srcTextureData + x*2 + y*m_srcTexturePitch)=pixel;
		}
		else
#endif
		{
#ifdef DO_FOG_INTERPOLATION
			if (!textureOnly)
				m_finalFogData[x+y*m_numCellsX]=level;
#endif
			UnsignedInt bluepixel = (UnsignedInt)((Real)level*((Real)(TheGlobalData->m_shroudColor.getAsInt()&0xff)/255.0f));
			UnsignedInt greenpixel = (UnsignedInt)((Real)level*((Real)((TheGlobalData->m_shroudColor.getAsInt()&0xff00)>>8)/255.0f));
			UnsignedInt redpixel = (UnsignedInt)((Real)level*((Real)((TheGlobalData->m_shroudColor.getAsInt()&0xff0000)>>16)/255.0f));
//			UnsignedInt bluepixel = (UnsignedInt)((Real)level*((Real)(SHROUD_COLOR&0xff)/255.0f));
//			UnsignedInt greenpixel = (UnsignedInt)((Real)level*((Real)((SHROUD_COLOR&0xff00)>>8)/255.0f));
//			UnsignedInt redpixel = (UnsignedInt)((Real)level*((Real)((SHROUD_COLOR&0xff0000)>>16)/255.0f));
			if (level == 255)
			{	//unshrouded pixels should be fully lit
				redpixel = 255;
				greenpixel = 255;
				bluepixel = 255;
			}

			UnsignedShort *texel = (UnsignedShort *)((Byte *)m_srcTextureData + x*2 + y*m_srcTexturePitch);

//      For those interested, MLorenzen has this bock commented out until he gets back on Mon, Sept. 30 2002
			// If this code is still here by mid october, nuke it!
//			UnsignedInt texelRed =  ((*texel >> 8 ) & 0xf8);
//			UnsignedInt texelGreen =((*texel >> 3 ) & 0xfc);
//			UnsignedInt texelBlue = ((*texel << 3 ) & 0xf8);
//			if (texelRed < texelGreen && texelGreen > texelBlue)
//			{
//				bluepixel += redpixel;
//				bluepixel -= redpixel;
//			}

			*texel = ( ((bluepixel&0xf8) >> 3) | ((greenpixel&0xfc)<<3) | ((redpixel&0xf8)<<8));
		}
		return;
	}
}

//-----------------------------------------------------------------------------
///Quickly sets the shroud level of entire map to a single value
void W3DShroud::fillShroudData(W3DShroudLevel level)
{

	Int x,y;
	UnsignedShort pixel;

	if (level < TheGlobalData->m_shroudAlpha)
		level = TheGlobalData->m_shroudAlpha;

#if defined(RTS_DEBUG)
	//convert value to pixel format
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
	{
		Int redVal = TheGlobalData->m_shroudColor.red;
		Int greenVal = TheGlobalData->m_shroudColor.green;
		Int blueVal = TheGlobalData->m_shroudColor.blue;
//		Int redVal = (SHROUD_COLOR >> 16) & 0xff;
//		Int greenVal = (SHROUD_COLOR >> 8) & 0xff;
//		Int blueVal = SHROUD_COLOR & 0xff;
		Int alphaVal = 255 - level;

		pixel=((blueVal>>4)&0xf) | (((greenVal>>4)&0xf)<<4) | (((redVal>>4)&0xf)<<8) | (((alphaVal>>4)&0xf)<<12);
	}
	else
#endif
	{
		UnsignedInt bluepixel = (UnsignedInt)((Real)level*((Real)(TheGlobalData->m_shroudColor.getAsInt()&0xff)/255.0f));
		UnsignedInt greenpixel = (UnsignedInt)((Real)level*((Real)((TheGlobalData->m_shroudColor.getAsInt()&0xff00)>>8)/255.0f));
		UnsignedInt redpixel = (UnsignedInt)((Real)level*((Real)((TheGlobalData->m_shroudColor.getAsInt()&0xff0000)>>16)/255.0f));
//		UnsignedInt bluepixel = (UnsignedInt)((Real)level*((Real)(SHROUD_COLOR&0xff)/255.0f));
//		UnsignedInt greenpixel = (UnsignedInt)((Real)level*((Real)((SHROUD_COLOR&0xff00)>>8)/255.0f));
//		UnsignedInt redpixel = (UnsignedInt)((Real)level*((Real)((SHROUD_COLOR&0xff0000)>>16)/255.0f));

		if (level == 255)
		{	//unshrouded pixels should be fully lit
			redpixel = 255;
			greenpixel = 255;
			bluepixel = 255;
		}
		pixel=( ((bluepixel&0xf8) >> 3) | ((greenpixel&0xfc)<<3) | ((redpixel&0xf8)<<8));
	}

	UnsignedShort *ptr=(UnsignedShort *)m_srcTextureData;
	Int pitch = m_srcTexturePitch >> 1;	//2 bytes per pointer increment
	for (y=0; y<m_numCellsY; y++)
	{
		for (x=0; x<m_numCellsX; x++)
			ptr[x]=pixel;
		ptr	+= pitch;
	}

#ifdef DO_FOG_INTERPOLATION
	//Set the final shroud state.  May differe from current state because of time interpolation.
	W3DShroudLevel *cptr=m_finalFogData;
	pitch = m_numCellsX;
	for (y=0; y<m_numCellsY; y++)
	{
		for (x=0; x<m_numCellsX; x++)
			cptr[x]=level;
		ptr	+= pitch;
	}
#endif
}

void W3DShroud::fillBorderShroudData(W3DShroudLevel level, SurfaceClass* pDestSurface)
{
	Int x,y;
	UnsignedShort pixel;

	if (level < TheGlobalData->m_shroudAlpha)
		level = TheGlobalData->m_shroudAlpha;

#if defined(RTS_DEBUG)
	//convert value to pixel format
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
	{
		Int redVal = TheGlobalData->m_shroudColor.red;
		Int greenVal = TheGlobalData->m_shroudColor.green;
		Int blueVal = TheGlobalData->m_shroudColor.blue;
		Int alphaVal = 255 - level;

		pixel=((blueVal>>4)&0xf) | (((greenVal>>4)&0xf)<<4) | (((redVal>>4)&0xf)<<8) | (((alphaVal>>4)&0xf)<<12);
	}
	else
#endif
	{
		UnsignedInt bluepixel = (UnsignedInt)((Real)level*((Real)(TheGlobalData->m_shroudColor.getAsInt()&0xff)/255.0f));
		UnsignedInt greenpixel = (UnsignedInt)((Real)level*((Real)((TheGlobalData->m_shroudColor.getAsInt()&0xff00)>>8)/255.0f));
		UnsignedInt redpixel = (UnsignedInt)((Real)level*((Real)((TheGlobalData->m_shroudColor.getAsInt()&0xff0000)>>16)/255.0f));

		if (level == 255)
		{	//unshrouded pixels should be fully lit
			redpixel = 255;
			greenpixel = 255;
			bluepixel = 255;
		}
		pixel=( ((bluepixel&0xf8) >> 3) | ((greenpixel&0xfc)<<3) | ((redpixel&0xf8)<<8));
	}

	//Skip to unused texels within the shroud data
	UnsignedShort *ptr=(UnsignedShort *)m_srcTextureData + m_numCellsY*(m_srcTexturePitch >> 1);

	//Fill unused texels with border color
	for (x=0; x<m_numCellsX; x++)
			ptr[x]=pixel;

	//Fill destination texture with border color

	RECT	srcRect;

	//create a rectangle enclosing bottom row of unused pixels long enough
	//to cover destination width.
	srcRect.left=0;
	srcRect.top=m_numCellsY;
	srcRect.right= m_numCellsX;
	srcRect.bottom= m_numCellsY+1;

	POINT	dstPoint={0,0};

	Int numFullCopies = m_dstTextureWidth/srcRect.right;
	Int numExtraPixels = m_dstTextureWidth%srcRect.right;

	for (y=0; y<m_dstTextureHeight; y++)
	{
		dstPoint.y=y;
		dstPoint.x=0;

		for (x=0; x<numFullCopies; x++)
		{
			dstPoint.x = x * srcRect.right;	//advance to next set of pixel in row.

			DX8Wrapper::_Copy_DX8_Rects(
				m_pSrcTexture,
				&srcRect,
				1,
				pDestSurface->Peek_D3D_Surface(),
				&dstPoint);
		}
		if (numExtraPixels)
		{	Int oldVal=srcRect.right;
			dstPoint.x = numFullCopies * oldVal;
			srcRect.right = numExtraPixels;
			DX8Wrapper::_Copy_DX8_Rects(
				m_pSrcTexture,
				&srcRect,
				1,
				pDestSurface->Peek_D3D_Surface(),
				&dstPoint);
			srcRect.right = oldVal;
		}
	}

}

/**Set the shroud color within the border area of the map*/
void W3DShroud::setBorderShroudLevel(W3DShroudLevel level)
{
	m_boderShroudLevel = level;
	m_clearDstTexture = TRUE;
}

//-----------------------------------------------------------------------------
///@todo: remove this
TextureClass *DummyTexture=nullptr;

//#define LOAD_DUMMY_SHROUD

//-----------------------------------------------------------------------------
//DECLARE_PERF_TIMER(shroudCopy)

//-----------------------------------------------------------------------------
/** Updates video memory surface with currently visible shroud data */
void W3DShroud::render(CameraClass *cam)
{
	if (!m_pSrcTexture)
		return; //nothing to update from.  Must be in reset state.

	if (DX8Wrapper::_Get_D3D_Device8() && (DX8Wrapper::_Get_D3D_Device8()->TestCooperativeLevel()) != D3D_OK)
		return;	//device not ready to render anything

#if defined(RTS_DEBUG)
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn != m_drawFogOfWar)
	{
		//fog state has changed since last time shroud system was initialized
		reset();
		ReleaseResources();
		init(TheTerrainRenderObject->getMap(),m_cellWidth,m_cellHeight);
		ThePartitionManager->refreshShroudForLocalPlayer();
		m_drawFogOfWar=TheGlobalData->m_fogOfWarOn;
		m_clearDstTexture=TRUE;
	}
#endif

	DEBUG_ASSERTCRASH( m_pSrcTexture != nullptr, ("Updating unallocated shroud texture"));

#ifdef LOAD_DUMMY_SHROUD

	static doInit=1;
	if (doInit)
	{
		//some temporary code here to debug the shroud.

		///@todo: remove this debug buffer fill.
		fillShroudData(1.0f);	//force to all shrouded

		Short *src=(Short *)m_srcTextureData;
		//fill with some dummy values
		src[0]=(char)0xff;
		src[1]=(char)0xff;
		src[m_numCellsX]=(char)0xff;
		src[m_numCellsX+1]=(char)0xff;
		src[m_numCellsX*2]=(char)0xff;

		src[m_numCellsX*9+8]=(char)0xff;
		src[m_numCellsX*8+8]=(char)0xff;
		src[m_numCellsX*7+8]=(char)0xff;
		src[m_numCellsX*8+9]=(char)0xff;
		src[m_numCellsX*8+7]=(char)0xff;

		DummyTexture=WW3DAssetManager::Get_Instance()->Get_Texture("shroud1024.tga");

		Short *dataDest=(Short *)((char *)m_srcTextureData);	//offset to correct row of full sysmem shroud
		Int pitchDest = m_srcTexturePitch >> 1;	//2 bytes per pixel so divide byte count by 2.

		//Copy the dummy shroud into our game shroud.
		SurfaceClass *pSurface=DummyTexture->Get_Surface_Level(0);
		Int pitch;
		Int *dataSrc=(Int *)((char *)pSurface->Lock(&pitch));	//offset to correct row of full sysmem shroud
		pitch >>= 2;	//4 bytes per pixel so divide byte count by 4.
		SurfaceClass::SurfaceDescription desc;
		pSurface->Get_Description(desc);

		//Check if source data is larger than our current shroud
		desc.Width = __min(desc.Width,m_numCellsX);
		desc.Height = __min(desc.Height,m_numCellsY);

		for (Int y=0; y<desc.Height; y++)
		{
			for (Int x=0; x<desc.Width; x++)
			{
#if defined(RTS_DEBUG)
				if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
				{
					dataDest[x]=((TheGlobalData->m_shroudColor.getAsInt()>>4)&0xf) | (((TheGlobalData->m_shroudColor.getAsInt()>>12)&0xf)<<4) | (((TheGlobalData->m_shroudColor.getAsInt()>>20)&0xf)<<8) | ((((255-(dataSrc[x] & 0xff))>>4)&0xf)<<12);
//					dataDest[x]=((SHROUD_COLOR>>4)&0xf) | (((SHROUD_COLOR>>12)&0xf)<<4) | (((SHROUD_COLOR>>20)&0xf)<<8) | ((((255-(dataSrc[x] & 0xff))>>4)&0xf)<<12);
				}
				else
#endif
				{
					dataDest[x]=((dataSrc[x]>>3)&0x1f) | (((dataSrc[x]>>10)&0x3f)<<5) | (((dataSrc[x]>>19)&0x1f)<<11);
				}
			}

			dataDest += pitchDest;	//skip to next row.
			dataSrc += pitch;	//skip to next row of shroud
		}

		pSurface->Unlock();

		REF_PTR_RELEASE (pSurface);
		REF_PTR_RELEASE (DummyTexture);

		doInit=0;
	}

#endif //LOAD_DUMMY_SHROUD


	WorldHeightMap *hm=TheTerrainRenderObject->getMap();
	Int visStartX=REAL_TO_INT_FLOOR((Real)(hm->getDrawOrgX()-hm->getBorderSizeInline())*MAP_XY_FACTOR/m_cellWidth);	//start of rendered heightmap rectangle
	if (visStartX < 0)
		visStartX = 0;	//no shroud is applied in border area so it always starts at > 0
	Int visStartY=REAL_TO_INT_FLOOR((Real)(hm->getDrawOrgY()-hm->getBorderSizeInline())*MAP_XY_FACTOR/m_cellHeight);
	if (visStartY < 0)
		visStartY = 0;	//no shroud is applied in border area so it always starts at > 0

	// Do it all [3/11/2003]
	visStartX = 0;
	visStartY = 0;

	Int visEndX=visStartX+REAL_TO_INT_FLOOR((Real)(hm->getDrawWidth()-1)*MAP_XY_FACTOR/m_cellWidth)+1;	//size of rendered heightmap rectangle
	Int visEndY=visStartY+REAL_TO_INT_FLOOR((Real)(hm->getDrawHeight()-1)*MAP_XY_FACTOR/m_cellHeight)+1;

	// Do it all [3/11/2003]
	visEndX = m_numCellsX;
	visEndY = m_numCellsY;

	if (visEndX > m_numCellsX)
	{
		visStartX -= visEndX - m_numCellsX;	//shift visible rectangle to fall within terrain bounds
		if (visStartX < 0)
			visStartX = 0;
		visEndX = m_numCellsX;
	}

	if (visEndY > m_numCellsY)
	{
		visStartY -= visEndY - m_numCellsY;	//shift visible rectangle to fall within terrain bounds
		if (visStartY < 0)
			visStartY = 0;
		visEndY = m_numCellsY;
	}

	m_drawOriginX = (Real)visStartX * m_cellWidth;
	m_drawOriginY	= (Real)visStartY * m_cellHeight;

	//If system memory usage becomes too large, we should store shroud as Bytes.  Update
	//a system memory texture with data.  Then copy this to video memory.  For now we're
	//holding shroud data directly in a texture to avoid the extra copy.
/*	pSurface=m_pSrcTexture->Get_Surface_Level(0);
	data=(Short *)((char *)pSurface->Lock(&pitch) + visStartY*pitch);	//offset to correct row of full sysmem shroud
	pitch >>= 1;	//we have 2 bytes per pixel, so divide pitch by 2

	Byte *sd=shroudData+visStartY*MAX_MAP_SHROUDSIZE;	//pointer to first shroud row

	for (Int y=visStartY; y<(visStartY+visSizeY); y++)
	{
		for (Int x=visStartX; x<(visStartX+visSizeX); x++)
		{
			data[x]=sd[x]|(sd[x]<<8);
		}
			data += pitch;	//skip to next row.
		sd += MAX_MAP_SHROUDSIZE;	//skip to next row of shroud
	}

	pSurface->Unlock();
*/
	if (m_pDstTexture->Get_Filter().Get_Mag_Filter() != m_shroudFilter)
	{
		m_pDstTexture->Get_Filter().Set_Mag_Filter(m_shroudFilter);
		m_pDstTexture->Get_Filter().Set_Min_Filter(m_shroudFilter);
	}

	//Update video memory texture with sysmem copy
	SurfaceClass* pDestSurface;
	{
		pDestSurface=m_pDstTexture->Get_Surface_Level(0);
	}

	RECT	srcRect;
	POINT	dstPoint={1,1};	//first row/column is reserved for border.

	srcRect.left=visStartX;
	srcRect.top=visStartY;
	srcRect.right=visEndX;
	srcRect.bottom=visEndY;

#ifdef DO_FOG_INTERPOLATION
	//interpolate current shroud state to the final one
	interpolateFogLevels(&srcRect);
#endif

	if (m_clearDstTexture)
	{	//we need to clear unused parts of the destination texture to a known
		//color in order to keep map border in the state we want.
		m_clearDstTexture=FALSE;

		fillBorderShroudData(m_boderShroudLevel, pDestSurface);
	}

	{
		//USE_PERF_TIMER(shroudCopy)
		DX8Wrapper::_Copy_DX8_Rects(
				m_pSrcTexture,
				&srcRect,
				1,
				pDestSurface->Peek_D3D_Surface(),
				&dstPoint);
	}

	REF_PTR_RELEASE (pDestSurface);
}

#define FOG_INTERPOLATION_RATE	(255.0f/1000.0f)	//take one second to go from black to fully lit.
//-----------------------------------------------------------------------------
void W3DShroud::interpolateFogLevels(RECT *rect)
{
	static UnsignedInt prevTime = timeGetTime();

	UnsignedInt timeDiff=timeGetTime()-prevTime;

	if (!timeDiff)
		return;	//no time has elapsed

	prevTime +=timeDiff;	//update for next frame

	Int maxFogChange=FOG_INTERPOLATION_RATE * (Real)timeDiff;	//maximum amount of fog change allowed in frame.
	if (maxFogChange > 255)
		maxFogChange = 255;
	W3DShroudLevel levelDelta = maxFogChange;

	W3DShroudLevel *startLevel=m_currentFogData;
	W3DShroudLevel *finalLevel=m_finalFogData;

	for (Int j=0; j<m_numCellsY; j++)
	{
		for (Int i=0; i<m_numCellsX; i++,startLevel++,finalLevel++)
			if (*startLevel != *finalLevel)
			{	//fog needs fading.
				if (*startLevel == *finalLevel)
					continue;
				else
				if (*finalLevel < *startLevel)
				{
					if ((*startLevel - *finalLevel) < levelDelta)
						*startLevel = *finalLevel;	//change too large so clamp to final value.
					else
						*startLevel -= levelDelta;
				}
				else
				if (*finalLevel > *startLevel)
				{
					if ((*finalLevel - *startLevel) < levelDelta)
						*startLevel = *finalLevel;	//change too large so clamp to final value.
					else
						*startLevel += levelDelta;
				}
				setShroudLevel(i,j,*startLevel,TRUE);
			}
	}
}

//-----------------------------------------------------------------------------
void W3DShroud::setShroudFilter(Bool enable)
{
	if (enable)
		m_shroudFilter=TextureFilterClass::FILTER_TYPE_DEFAULT;
	else
		m_shroudFilter=TextureFilterClass::FILTER_TYPE_NONE;
}

//-----------------------------------------------------------------------------
///Set render states required to draw shroud pass.
void W3DShroudMaterialPassClass::Install_Materials() const
{
	if (TheTerrainRenderObject->getShroud())
	{
 		W3DShaderManager::setTexture(0,TheTerrainRenderObject->getShroud()->getShroudTexture());
		W3DShaderManager::setShader(W3DShaderManager::ST_SHROUD_TEXTURE, 0);
	}
}

//-----------------------------------------------------------------------------
///Restore render states that W3D doesn't know about.
void W3DShroudMaterialPassClass::UnInstall_Materials() const
{
	W3DShaderManager::resetShader(W3DShaderManager::ST_SHROUD_TEXTURE);
}

//-----------------------------------------------------------------------------
///Set render states required to draw shroud pass.
void W3DMaskMaterialPassClass::Install_Materials() const
{
	W3DShaderManager::setShader(W3DShaderManager::ST_MASK_TEXTURE, 0);
}

//-----------------------------------------------------------------------------
///Restore render states that W3D doesn't know about.
void W3DMaskMaterialPassClass::UnInstall_Materials() const
{
	if (m_allowUninstall)
		W3DShaderManager::resetShader(W3DShaderManager::ST_MASK_TEXTURE);
}
