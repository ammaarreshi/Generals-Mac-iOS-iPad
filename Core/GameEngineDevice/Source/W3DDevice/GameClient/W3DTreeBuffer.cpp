/*
**	Command & Conquer Generals Zero Hour(tm)
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

// FILE: W3DTreeBuffer.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: W3DTreeBuffer.cpp
//
// Created:   John Ahlquist, May 2001
//
// Desc:      Draw buffer to handle all the trees in a scene.
//
//-----------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
/** Topple options */
// ------------------------------------------------------------------------------------------------
enum
{
	W3D_TOPPLE_OPTIONS_NONE			 = 0x00000000,
	W3D_TOPPLE_OPTIONS_NO_BOUNCE = 0x00000001,  ///< do not bounce when hit the ground
	W3D_TOPPLE_OPTIONS_NO_FX		 = 0x00000002	///< do not play any FX when hit the ground
};
//-----------------------------------------------------------------------------
//         Includes
//-----------------------------------------------------------------------------

#include "W3DDevice/GameClient/W3DTreeBuffer.h"

#include <assetmgr.h>
#include <texture.h>
#include "Common/FramePacer.h"
#include "Common/GameUtility.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/FileSystem.h"
#include "Common/file.h"
#include "Common/PerfTimer.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameClient/ClientRandomValue.h"
#include "GameClient/FXList.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "W3DDevice/GameClient/Module/W3DTreeDraw.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "W3DDevice/GameClient/W3DProjectedShadow.h"
#include "WW3D2/camera.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/matinfo.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "d3dx8tex.h"


// If TEST_AND_BLEND is defined, it will do an alpha test and blend.  Otherwise just alpha test. jba. [5/30/2003]
#define dontTEST_AND_BLEND 1

#define USE_STATIC 1

#define END_OF_PARTITION (-1)

#define DELETED_TREE_TYPE (-2)

/******************************************************************************
						W3DTreeTextureClass
******************************************************************************/
//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// W3DTreeBuffer::W3DTreeTextureClass::W3DTreeTextureClass
//=============================================================================
/** Constructor. Calls parent constructor to create a 16 bit per pixel D3D
texture of the desired height and mip level. */
//=============================================================================
W3DTreeBuffer::W3DTreeTextureClass::W3DTreeTextureClass(unsigned width, unsigned height) :
	TextureClass(width, height,
		WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_ALL )
{
}

//=============================================================================
// W3DTreeBuffer::W3DTreeTextureClass::update
//=============================================================================
/** Sets the tile bitmap data into the texture.  The tiles are placed with 4
	pixel borders around them, so that when the tiles are scaled and bilinearly
	interpolated, you don't get seams between the tiles.  */
//=============================================================================
int W3DTreeBuffer::W3DTreeTextureClass::update(W3DTreeBuffer *buffer)
{

	//Set to clamp.
	Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);

	IDirect3DSurface8 *surface_level;
	D3DSURFACE_DESC surface_desc;
	D3DLOCKED_RECT locked_rect;
	DX8_ErrorCode(Peek_D3D_Texture()->GetSurfaceLevel(0, &surface_level));
	DX8_ErrorCode(surface_level->GetDesc(&surface_desc));

	DX8_ErrorCode(surface_level->LockRect(&locked_rect, nullptr, 0));

	Int tilePixelExtent = TILE_PIXEL_EXTENT;
//	Int numRows = surface_desc.Height/(tilePixelExtent+TILE_OFFSET);
#ifdef RTS_DEBUG
	//DASSERT_MSG(tilesPerRow*numRows >= htMap->m_numBitmapTiles,Debug::Format ("Too many tiles."));
	//DEBUG_ASSERTCRASH((Int)surface_desc.Width >= tilePixelExtent*tilesPerRow, ("Bitmap too small."));
#endif
	if (surface_desc.Format == D3DFMT_A8R8G8B8) {
		Int tileNdx;
		Int pixelBytes = 4;
#if 0 // Fill unused texture for debug display.
		UnsignedInt cellX, cellY;
		for (cellX = 0; cellX < surface_desc.Width; cellX++) {
			for (cellY = 0; cellY < surface_desc.Height; cellY++) {
				UnsignedByte *pBGR = ((UnsignedByte *)locked_rect.pBits)+(cellY*surface_desc.Width+cellX)*pixelBytes;
				//*((Short*)pBGR) =  0x8000 + (((255-2*cellY)>>3)<<10) + ((4*cellX)>>4);
				*((Int*)pBGR) =  0xFF000000 | ( (((255-cellY))<<16) + ((cellX)) );

			}
		}
#endif
		for (tileNdx=0; tileNdx < buffer->getNumTiles(); tileNdx++) {
			TileData *pTile = buffer->getSourceTile(tileNdx);
			if (!pTile) continue;
			ICoord2D position = pTile->m_tileLocationInTexture;
			if (position.x<0) {
				continue;
			}
			Int i,j;
			for (j=0; j<tilePixelExtent; j++) {
				UnsignedByte *pBGR = pTile->getRGBDataForWidth(tilePixelExtent);
				pBGR += (tilePixelExtent-(1+j))*TILE_BYTES_PER_PIXEL*tilePixelExtent; // invert to match.
				Int row = position.y+j;
				UnsignedByte *pBGRA = ((UnsignedByte*)locked_rect.pBits) +
							(row)*surface_desc.Width*pixelBytes;

				Int column = position.x;
				pBGRA += column*pixelBytes;
				for (i=0; i<tilePixelExtent; i++) {
					// 15 bit color *((Short*)pBGRA) = 0x8000 + ((pBGR[2]>>3)<<10) + ((pBGR[1]>>3)<<5) + (pBGR[0]>>3);
					*((Int *)pBGRA) = (pBGR[3]<<24) + (pBGR[2]<<16) + (pBGR[1]<<8) + (pBGR[0]);
					pBGRA +=pixelBytes;
					pBGR +=TILE_BYTES_PER_PIXEL;
				}
			}
		}

	}
	DX8_ErrorCode(surface_level->UnlockRect());
	surface_level->Release();
	DX8_ErrorCode(D3DXFilterTexture(Peek_D3D_Texture(), nullptr, (UINT)0, D3DX_FILTER_BOX));
	if (WW3D::Get_Texture_Reduction()) {
		DX8_ErrorCode(Peek_D3D_Texture()->SetLOD((DWORD)WW3D::Get_Texture_Reduction()));
	}
	return(surface_desc.Height);
}


//=============================================================================
// W3DTreeBuffer::W3DTreeTextureClass::setLOD
//=============================================================================
/** Sets the lod of the texture to be loaded into the video card.  */
//=============================================================================
void W3DTreeBuffer::W3DTreeTextureClass::setLOD(Int LOD) const
{
	if (Peek_D3D_Texture()) {
		DX8_ErrorCode(Peek_D3D_Texture()->SetLOD((DWORD)LOD));
	}
}
//=============================================================================
// W3DTreeBuffer::W3DTreeTextureClass::Apply
//=============================================================================
/** Sets the texture as the current D3D texture, and does some custom setup
(standard D3D setup, but beyond the scope of W3D).  */
//=============================================================================
void W3DTreeBuffer::W3DTreeTextureClass::Apply(unsigned int stage)
{
	// Do the base apply.
	TextureClass::Apply(stage);
}
//-----------------------------------------------------------------------------
//         Private Data
//-----------------------------------------------------------------------------

#ifdef TEST_AND_BLEND
// A W3D shader that does alpha, texturing, tests zbuffer, doesn't update zbuffer.
#define SC_ALPHA_DETAIL ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_SRC_ALPHA, \
	ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_ENABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

#define SC_ALPHA_DETAIL_2X ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_SRC_ALPHA, \
	ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE2X, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_ENABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

#else
#define SC_ALPHA_DETAIL ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_ENABLE, ShaderClass::CULL_MODE_DISABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

#define SC_ALPHA_DETAIL_2X ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE2X, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_ENABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )
#endif
static ShaderClass detailAlphaShader(SC_ALPHA_DETAIL);
static ShaderClass detailAlphaShader2X(SC_ALPHA_DETAIL_2X);


/*
#define SC_ALPHA_DETAIL ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_ENABLE, ShaderClass::CULL_MODE_DISABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass detailAlphaShader(SC_ALPHA_DETAIL);
*/

/*
#define SC_ALPHA_DETAIL ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_DISABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_SRC_ALPHA, \
	ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_ENABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass detailAlphaShader(SC_ALPHA_DETAIL);
*/

/*
#define SC_ALPHA_MIRROR ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_DISABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_SRC_ALPHA, \
	ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass mirrorAlphaShader(SC_ALPHA_DETAIL);

// ShaderClass::PASS_ALWAYS,

#define SC_ALPHA_2D ( SHADE_CNST(PASS_ALWAYS, DEPTH_WRITE_DISABLE, COLOR_WRITE_ENABLE, \
	SRCBLEND_SRC_ALPHA, DSTBLEND_ONE_MINUS_SRC_ALPHA, FOG_DISABLE, GRADIENT_DISABLE, \
	SECONDARY_GRADIENT_DISABLE, TEXTURING_ENABLE, DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE, \
	ALPHATEST_DISABLE, CULL_MODE_ENABLE, DETAILCOLOR_DISABLE, DETAILALPHA_DISABLE) )
ShaderClass ShaderClass::_PresetAlpha2DShader(SC_ALPHA_2D);
*/
//-----------------------------------------------------------------------------
//         Private Functions
//-----------------------------------------------------------------------------

//=============================================================================
// W3DTreeBuffer::cull
//=============================================================================
/** Culls the trees, marking the visible flag.  If a tree becomes visible, it sets
it's sortKey */
//=============================================================================
void W3DTreeBuffer::cull(const CameraClass * camera)
{
	Int curTree;

	// Calculate the vector direction that the camera is looking at.
	Matrix3D camera_matrix = camera->Get_Transform();
	float zmod = -1;
	float x = zmod * camera_matrix[0][2] ;
	float y = zmod * camera_matrix[1][2] ;
	float z = zmod * camera_matrix[2][2] ;
	m_cameraLookAtVector.Set(x,y,z);

	for (curTree=0; curTree<m_numTrees; curTree++) {
		Bool doKey = false;	// We calculate the key when a tree becomes visible.
		Bool visible = !camera->Cull_Sphere(m_trees[curTree].bounds);
		if (visible != m_trees[curTree].visible) {
			m_trees[curTree].visible=visible;
			m_anythingChanged = true;
			if (visible) {
				doKey = true;
			}
		}
		// Also calculate sort key if a tree is visible, and the view changed setting m_updateAllKeys to true.
		if (doKey || (visible&&m_updateAllKeys)) {
			// The sort key is essentially the distance of location in the direction of the
			// camera look at.
			m_trees[curTree].sortKey = Vector3::Dot_Product(m_trees[curTree].location, m_cameraLookAtVector);
		}
	}
	m_updateAllKeys = false;
}
//=============================================================================
// W3DTreeBuffer::getPartitionBucket
//=============================================================================
/** Returns the bucket index into m_areaPartition for a given location. */
//=============================================================================
Int W3DTreeBuffer::getPartitionBucket(const Coord3D &pos) const
{
	Real x = pos.x;
	Real y = pos.y;
	if (x<m_bounds.lo.x) x = m_bounds.lo.x;
	if (y<m_bounds.lo.y) y = m_bounds.lo.y;
	if (x>m_bounds.hi.x) x = m_bounds.hi.x;
	if (y>m_bounds.hi.y) y = m_bounds.hi.y;
	Int xIndex = REAL_TO_INT_FLOOR ( (x/(m_bounds.hi.x-m_bounds.lo.x)) * (static_cast<float>(PARTITION_WIDTH_HEIGHT)-0.1f) );
	Int yIndex = REAL_TO_INT_FLOOR ( (y/(m_bounds.hi.y-m_bounds.lo.y)) * (static_cast<float>(PARTITION_WIDTH_HEIGHT)-0.1f) );
	DEBUG_ASSERTCRASH(xIndex>=0 && yIndex>=0 && xIndex<PARTITION_WIDTH_HEIGHT && yIndex<PARTITION_WIDTH_HEIGHT, ("Invalid range."));
	return yIndex*PARTITION_WIDTH_HEIGHT + xIndex;
}

//=============================================================================
// W3DTreeBuffer::updateSway
//=============================================================================
void W3DTreeBuffer::updateSway(const BreezeInfo& info)
{
	Int i;
	for	(i=0; i<NUM_SWAY_ENTRIES; i++) {
		Real factor = Cos(i*2.0f*PI/(static_cast<float>(NUM_SWAY_ENTRIES)+1.0f));
		Real angle = info.m_lean + (info.m_intensity  * factor);
		Real S = Sin(angle);
		Real C = Cos(angle);
		m_swayOffsets[i].X = info.m_directionVec.x * S;
		m_swayOffsets[i].Y = info.m_directionVec.y * S;
		m_swayOffsets[i].Z = C - 1.0f;
	}

	Real delta = info.m_randomness * 0.5f;

	for (i=0; i<MAX_SWAY_TYPES; i++) {
		m_curSwayStep[i] = static_cast<float>(NUM_SWAY_ENTRIES) / (Real)info.m_breezePeriod;
		m_curSwayStep[i]	*= GameClientRandomValueReal(1.0f-delta, 1.0f+delta);
		if (m_curSwayStep[i]<0.0f) {
			m_curSwayStep[i] = 0.0f;
		}
		m_curSwayOffset[i] = 0;
		m_curSwayFactor[i] = GameClientRandomValueReal(1.0f-delta, 1.0f+delta);
	}
	m_curSwayVersion = info.m_breezeVersion;
}

#if 0 // sort is not used, and messes up the order jba. [6/6/2003]
//=============================================================================
// W3DTreeBuffer::sort
//=============================================================================
/** Sorts the trees.  Does num_iterations of a bubble sort.  This is good because
it ends immediately if the trees are already sorted (which is most of the time)
and will perform a fixed amount of work each frame until it becomes sorted. */
//=============================================================================
void W3DTreeBuffer::sort(Int numIterations)
{
	// sort in descending order.
	Int iter;
	Bool swap = false;
	for (iter = 0; iter<numIterations; iter++) {
		Int cur = 0;
		// Note - only sorts the visible trees.
		while (cur<m_numTrees-iter && !m_trees[cur].visible) {
			cur++;
		}
		Int i;
		for (i=cur+1; i<m_numTrees-iter; i++) {
			if (m_trees[i].visible) {
				if (m_trees[cur].sortKey > m_trees[i].sortKey) {
					TTree tmp = m_trees[cur];
					m_trees[cur] = m_trees[i];
					m_trees[i] = tmp;
					swap = true;
				}
				cur = i;
			}
		}
		if (!swap) {
			return;
		}
		m_anythingChanged = true;
	}
}
#endif

/********** GDIFileStream2 class ****************************/
class GDIFileStream2 : public InputStream
{
protected:
	File* m_file;
public:
	GDIFileStream2():m_file(nullptr) {};
	GDIFileStream2(File* pFile):m_file(pFile) {};
	virtual Int read(void *pData, Int numBytes) override {
		return(m_file?m_file->read(pData, numBytes):0);
	};
};

//=============================================================================
// W3DTreeBuffer::updateTexture
//=============================================================================
/** Creates a new texture. */
//=============================================================================
void W3DTreeBuffer::updateTexture()
{

	const Int MAX_TEX_WIDTH = 2048;

	Int i, j;
	Int maxHeight = 0;
	const Int maxTilesPerRow = MAX_TEX_WIDTH/(TILE_PIXEL_EXTENT);

	REF_PTR_RELEASE(m_treeTexture);

	Bool availableGrid[maxTilesPerRow][maxTilesPerRow];
	Int row, column;
	for (row=0; row<maxTilesPerRow; row++) {
		for (column=0; column<maxTilesPerRow; column++) {
			availableGrid[row][column] = true;
		}
	}

	for (i=0; i<m_numTiles; i++) {
		REF_PTR_RELEASE (m_sourceTiles[i]);
	}
	m_numTiles = 0;
	File *theFile = nullptr;
	for (i=0; i<m_numTreeTypes; i++) {
		char texturePath[ _MAX_PATH ];
		m_treeTypes[i].m_numTiles = 0;
		snprintf( texturePath, ARRAY_SIZE(texturePath), "%s%s", TERRAIN_TGA_DIR_PATH, m_treeTypes[i].m_data->m_textureName.str() );
		theFile = TheFileSystem->openFile( texturePath, File::READ|File::BINARY);
		if (theFile==nullptr) {
			snprintf( texturePath, ARRAY_SIZE(texturePath), "%s%s", TGA_DIR_PATH, m_treeTypes[i].m_data->m_textureName.str() );
			theFile = TheFileSystem->openFile( texturePath, File::READ|File::BINARY);
		}
		if (theFile != nullptr) {
			GDIFileStream2 theStream(theFile);
			InputStream *pStr = &theStream;
			Bool halfTile;
			Int numTiles = WorldHeightMap::countTiles(pStr, &halfTile);
			Int width;
			for (width = 10; width >= 1; width--) {
				if (numTiles >= width*width) {
					numTiles = width*width;
					break;
				}
			}
			Bool texFound = false;
			for	(j=0; j<i; j++) {
				if (m_treeTypes[j].m_data->m_textureName.compareNoCase(m_treeTypes[i].m_data->m_textureName)==0) {
					m_treeTypes[i].m_firstTile = 0;
					m_treeTypes[i].m_tileWidth = width;
					m_treeTypes[i].m_numTiles = 0;
					texFound = true;
					break;
				}
			}
			if (texFound) {
				theFile->close();
				continue;
			}
			if (m_numTiles+numTiles<=MAX_TILES) {
				theFile->seek(0, File::START);
				m_treeTypes[i].m_firstTile = m_numTiles;
				m_treeTypes[i].m_tileWidth = width;
				m_treeTypes[i].m_numTiles = numTiles;
				m_treeTypes[i].m_halfTile = halfTile;
				WorldHeightMap::readTiles(pStr, m_sourceTiles+m_treeTypes[i].m_firstTile, width);
				m_numTiles += numTiles;
			} else {
				m_treeTypes[i].m_firstTile = 0;
				m_treeTypes[i].m_tileWidth = 0;
				m_treeTypes[i].m_numTiles = 0;
			}
			theFile->close();
		} else {
			DEBUG_CRASH(("Could not find texture %s", m_treeTypes[i].m_data->m_textureName.str()));
			m_treeTypes[i].m_firstTile = 0;
			m_treeTypes[i].m_tileWidth = 0;
			m_treeTypes[i].m_numTiles = 0;
		}
	}

	Int tmpWidth = 8;
	while (tmpWidth*tmpWidth<m_numTiles) {
		tmpWidth*=2;
	}
	Int tilesPerRow = tmpWidth;
	m_textureWidth = tmpWidth*TILE_PIXEL_EXTENT;

	if (m_textureWidth>MAX_TEX_WIDTH) {
		m_textureWidth = 64;
		m_textureHeight = 64;
		if (m_treeTexture==nullptr) {
			m_treeTexture = new TextureClass("missing.tga");
		}
		DEBUG_CRASH(("Too many trees in a scene."));
		return;
	}

	for (i=0; i<m_numTiles; i++) {
		if (m_sourceTiles[i]) {
			m_sourceTiles[i]->m_tileLocationInTexture.x = -1;
			m_sourceTiles[i]->m_tileLocationInTexture.y = -1;
		}
	}

	/* put the tree tiles into the texture */
	Int texClass;
	Int tileWidth;
	for (tileWidth = tilesPerRow; tileWidth>0; tileWidth--) {
		for (texClass=0; texClass<m_numTreeTypes; texClass++) {
			Int width = m_treeTypes[texClass].m_tileWidth;
			if (width != tileWidth) continue;
			Bool texFound = false;
			for	(i=0; i<texClass; i++) {
				if (m_treeTypes[i].m_data->m_textureName.compareNoCase(m_treeTypes[texClass].m_data->m_textureName)==0) {
					m_treeTypes[texClass].m_textureOrigin.x = m_treeTypes[i].m_textureOrigin.x;
					m_treeTypes[texClass].m_textureOrigin.y = m_treeTypes[i].m_textureOrigin.y;
					texFound = true;
					break;
				}
			}
			if (texFound) {
				continue;
			}

			// Find an available block of space.
			Bool found = false;
			for (row=0; row<(tilesPerRow-width)+1 && !found; row++) {
				for (column=0; column<(tilesPerRow-width)+1 && !found; column++) {
					if (availableGrid[row][column]) {
						Bool open = true;
						for (i=0; i<width && open; i++) {
							for (j=0; j<width&&open; j++) {
								if (!availableGrid[row+j][column+i]) {
									open = false;
								}
							}
						}
						if (open) found = true;
						break;
					}
				}
				if (found) break;
			}
			if (!found) {
				m_treeTypes[texClass].m_textureOrigin.x = 0;
				m_treeTypes[texClass].m_textureOrigin.y = 0;
				continue;
			}

			Int xOrigin = column*(TILE_PIXEL_EXTENT);
			Int yOrigin = row*(TILE_PIXEL_EXTENT);
			m_treeTypes[texClass].m_textureOrigin.x = xOrigin;
			m_treeTypes[texClass].m_textureOrigin.y = yOrigin;
			Int classHeight = yOrigin + width*TILE_PIXEL_EXTENT;
			if (maxHeight < classHeight) maxHeight = classHeight;

			for (i=0; i<width; i++) {
				for (j=0; j<width; j++) {
					availableGrid[row+j][column+i] = false;
					Int baseNdx = m_treeTypes[texClass].m_firstTile + i + j*width;
					Int x = xOrigin + i*TILE_PIXEL_EXTENT;
					Int y = yOrigin + ((width-j)-1)*TILE_PIXEL_EXTENT;
					m_sourceTiles[baseNdx]->m_tileLocationInTexture.x = x;
					m_sourceTiles[baseNdx]->m_tileLocationInTexture.y = y;
				}
			}
		}
	}
	DEBUG_ASSERTCRASH(maxHeight<=m_textureWidth, ("Bad max height."));
	W3DTreeTextureClass *tex = new W3DTreeTextureClass((DWORD)m_textureWidth, (DWORD)m_textureWidth);
	m_textureHeight = tex->update(this);

	m_treeTexture = tex;

	for (i=0; i<m_numTiles; i++) {
		REF_PTR_RELEASE (m_sourceTiles[i]);
	}
	//	m_treeTexture = NEW_REF (TextureClass, (m_treeTypes[0].m_textureName.str()));

}

/**Adjust the resolution of tree texture uploaded to the video card.  The system memory
version always remains at full resolution.  Someone should probably optimize this at
some point since it wastes a lot of system memory on low-end systems. -MW
*/
void W3DTreeBuffer::setTextureLOD(Int lod)
{
	if (m_treeTexture)
		((W3DTreeTextureClass*)m_treeTexture)->setLOD(lod);
}

//=============================================================================
// W3DTreeBuffer::doLighting
//=============================================================================
/** Calculates the diffuse lighting as affected by dynamic lighting. */
//=============================================================================
UnsignedInt W3DTreeBuffer::doLighting(const Vector3 *normal,
															const GlobalData::TerrainLighting	*objectLighting,
															const Vector3 *emissive, UnsignedInt vertDiffuse, Real scale) const
{

	Real shadeR, shadeG, shadeB;
	Real shade;
	shadeR = objectLighting[0].ambient.red+emissive->X;	//only the first light contributes to ambient
	shadeG = objectLighting[0].ambient.green+emissive->Y;
	shadeB = objectLighting[0].ambient.blue+emissive->Z;

	Int i;
	for	(i=0; i<MAX_GLOBAL_LIGHTS; i++) {
		Vector3 lightDirection(objectLighting[i].lightPos.x, objectLighting[i].lightPos.y, objectLighting[i].lightPos.z);
		lightDirection.Normalize();
		Vector3 lightRay(-lightDirection.X, -lightDirection.Y, -lightDirection.Z);
		shade = Vector3::Dot_Product(lightRay, *normal);

		if (shade > 1.0) shade = 1.0;
		if(shade < 0.0f) shade = 0.0f;
		shadeR += shade*objectLighting[i].diffuse.red;
		shadeG += shade*objectLighting[i].diffuse.green;
		shadeB += shade*objectLighting[i].diffuse.blue;
	}

	shadeR *= scale;
	shadeG *= scale;
	shadeB *= scale;

	if (shadeR > 1.0) shadeR = 1.0;
	if(shadeR < 0.0f) shadeR = 0.0f;
	if (shadeG > 1.0) shadeG = 1.0;
	if(shadeG < 0.0f) shadeG = 0.0f;
	if (shadeB > 1.0) shadeB = 1.0;
	if(shadeB < 0.0f) shadeB = 0.0f;

	if (vertDiffuse!=0xFFFFFFFF) {
		shade = vertDiffuse&0xff; //blue;
		shadeB *= shade/255.0f;
		shade = (vertDiffuse>>8)&0xFF; // green;
		shadeG *= shade/255.0f;
		shade = (vertDiffuse>>16)&0xFF; // red;
		shadeR *= shade/255.0f;
	}

	shadeR*=255.0f;
	shadeG*=255.0f;
	shadeB*=255.0f;
	const Real alpha = 255.0;
	return REAL_TO_UNSIGNEDINT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16) | ((Int)alpha << 24);

}

//=============================================================================
// W3DTreeBuffer::loadTreesInVertexAndIndexBuffers
//=============================================================================
/** Loads the trees into the vertex buffer for drawing. */
//=============================================================================
void W3DTreeBuffer::loadTreesInVertexAndIndexBuffers(RefRenderObjListIterator *pDynamicLightsIterator)
{
	if (!m_indexTree[0] || !m_vertexTree[0] || !m_initialized) {
		return;
	}
	if (!m_anythingChanged) {
		return;
	}

	if (m_shadow == nullptr && TheW3DProjectedShadowManager) {
		Shadow::ShadowTypeInfo shadowInfo;
		shadowInfo.allowUpdates=FALSE;	//shadow image will never update
		shadowInfo.allowWorldAlign=TRUE;	//shadow image will wrap around world objects
		shadowInfo.m_type = (ShadowType)SHADOW_DECAL;
		shadowInfo.m_sizeX=20;
		shadowInfo.m_sizeY=20;
		m_shadow = TheW3DProjectedShadowManager->createDecalShadow(&shadowInfo);
	}

	// TheSuperHackers @bugfix Reset bufferNdx so updateVertexBuffer skips trees absent from this rebuild.
	for (Int t = 0; t < m_numTrees; t++) {
		m_trees[t].bufferNdx = -1;
	}
	m_anythingChanged = false;
	Int curTree=0;
	Int bNdx;
	const GlobalData::TerrainLighting *objectLighting = TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay];
	for (bNdx=0; bNdx<MAX_BUFFERS; bNdx++) {
		m_curNumTreeVertices[bNdx] = 0;
		m_curNumTreeIndices[bNdx] = 0;
		if (curTree >= m_numTrees) {
			break;
		}
		VertexFormatXYZNDUV1 *vb;
		UnsignedShort *ib;
		// Lock the buffers.
	#ifdef USE_STATIC
		DX8IndexBufferClass::WriteLockClass lockIdxBuffer(m_indexTree[bNdx], 0);
		DX8VertexBufferClass::WriteLockClass lockVtxBuffer(m_vertexTree[bNdx], 0);
	#else
		DX8IndexBufferClass::WriteLockClass lockIdxBuffer(m_indexTree[bNdx], D3DLOCK_DISCARD);
		DX8VertexBufferClass::WriteLockClass lockVtxBuffer(m_vertexTree[bNdx], D3DLOCK_DISCARD);
	#endif
		vb=(VertexFormatXYZNDUV1*)lockVtxBuffer.Get_Vertex_Array();
		ib = lockIdxBuffer.Get_Index_Array();
		// Add to the index buffer & vertex buffer.
		Vector2 lookAtVector(m_cameraLookAtVector.X, m_cameraLookAtVector.Y);
		lookAtVector.Normalize();
		// We draw from back to front, so we put the indexes in the buffer
		// from back to front.
		UnsignedShort *curIb = ib;

		VertexFormatXYZNDUV1 *curVb = vb;




		for ( ;curTree<m_numTrees;curTree++) {
			Int type = m_trees[curTree].treeType;
			if (type<0 || m_treeTypes[type].m_mesh == nullptr) {
				continue; // Deleted tree or missing mesh. [6/9/2003]
			}
			if (!m_trees[curTree].visible) continue;
			Real scale = m_trees[curTree].scale;
			Vector3 loc = m_trees[curTree].location;
			Real theSin = m_trees[curTree].sin;
			Real theCos = m_trees[curTree].cos;

			Bool doVertexLighting = true;

	#if 0 // no dynamic lighting.
			for (pDynamicLightsIterator->First(); !pDynamicLightsIterator->Is_Done(); pDynamicLightsIterator->Next())
			{
				W3DDynamicLight *pLight = (W3DDynamicLight*)pDynamicLightsIterator->Peek_Obj();
				if (!pLight->isEnabled()) {
					continue; // he is turned off.
				}
				if (CollisionMath::Overlap_Test(m_trees[curTree].bounds, pLight->Get_Bounding_Sphere()) == CollisionMath::OUTSIDE) {
					continue; // this tree is outside of the light's influence.
				}
				doVertexLighting = true;
			}
	#endif
			Vector3 emissive(0.0f,0.0f,0.0f);
			MaterialInfoClass * matInfo = m_treeTypes[type].m_mesh->Get_Material_Info();
			if (matInfo) {
				VertexMaterialClass *vertMat = matInfo->Peek_Vertex_Material(0);
				if (vertMat) {
					vertMat->Get_Emissive(&emissive);
				}
			}
			REF_PTR_RELEASE(matInfo);


			Int startVertex = m_curNumTreeVertices[bNdx];
			m_trees[curTree].firstIndex = startVertex;
			m_trees[curTree].bufferNdx = bNdx;
			Int i;
			Int numVertex = m_treeTypes[type].m_mesh->Peek_Model()->Get_Vertex_Count();
			Vector3 *pVert = m_treeTypes[type].m_mesh->Peek_Model()->Get_Vertex_Array();



			// If we happen to have too many trees, stop.
			if (m_curNumTreeVertices[bNdx]+numVertex+2>= MAX_TREE_VERTEX) {
				break;
			}
			Int numIndex = m_treeTypes[type].m_mesh->Peek_Model()->Get_Polygon_Count();
			const TriIndex *pPoly = m_treeTypes[type].m_mesh->Peek_Model()->Get_Polygon_Array();
			if (m_curNumTreeIndices[bNdx]+3*numIndex+6 >= MAX_TREE_INDEX) {
				break;
			}

			const Vector2*uvs=m_treeTypes[type].m_mesh->Peek_Model()->Get_UV_Array_By_Index(0);

			const Vector3*normals = m_treeTypes[type].m_mesh->Peek_Model()->Get_Vertex_Normal_Array();
			const unsigned *vecDiffuse = m_treeTypes[type].m_mesh->Peek_Model()->Get_Color_Array(0, false);

			Int diffuse = 0;
			if (normals == nullptr) {
				doVertexLighting = false;
				Vector3 normal(0.0f,0.0f,1.0f);
				diffuse = doLighting(&normal, objectLighting, &emissive, 0xFFFFFFFF, 1.0f);
			}
	/*
	 *
			// If we are doing reduced resolution terrain, do reduced
			// poly trees.
			Bool doPanel = (TheGlobalData->m_useHalfHeightMap || TheGlobalData->m_stretchTerrain);

			if (doPanel) {
				if (m_trees[curTree].rotates) {
					theSin = -lookAtVector.X;
					theCos = lookAtVector.Y;
				}
				// panel start is index offset, there are 3 index per triangle.
				if (m_trees[curTree].panelStart/3 + 2 > numIndex) {
					continue; // not enough polygons for the offset.  jba.
				}
				for (j=0; j<6; j++) {
					i = ((Int *)pPoly)[j+m_trees[curTree].panelStart];
					if (m_curNumTreeVertices >= MAX_TREE_VERTEX)
						break;

					// Update the uv values.  The W3D models each have their own texture, and
					// we use one texture with all images in one, so we have to change the uvs to
					// match.
					Real U, V;
					if (type==SHRUB) {
						// shrub texture is tucked in the corner
						U = ((512-64)+uvs[i].U*64.0f)/512.0f;
						V = ((256-64)+uvs[i].V*64.0f)/256.0f;
					} else if (type==FENCE) {
						U = uvs[i].U*0.5f;
						V = 1.0f + uvs[i].V;
					} else {
						U = typeOffset+uvs[i].U*0.5f;
						V = uvs[i].V;
					}

					curVb->u1 = U;
					curVb->v1 = V/2.0;
					Vector3 vLoc;
					vLoc.X = pVert[i].X*scale*theCos - pVert[i].Y*scale*theSin;
					vLoc.Y = pVert[i].Y*scale*theCos + pVert[i].X*scale*theSin;

					vLoc.X += loc.X;
					vLoc.Y += loc.Y;
					vLoc.Z = loc.Z + pVert[i].Z*scale;

					curVb->x = vLoc.X;
					curVb->y = vLoc.Y;
					curVb->z = vLoc.Z;
					if (doVertexLighting) {
						curVb->diffuse = doLighting(&vLoc, shadeR, shadeG, shadeB, m_trees[curTree].bounds, pDynamicLightsIterator);
					} else {
						curVb->diffuse = diffuse;
					}
					curVb++;
					m_curNumTreeVertices++;
				}

				for (i=0; i<6; i++) {
					if (m_curNumTreeIndices+4 > MAX_TREE_INDEX)
						break;
					curIb--;
					*curIb = startVertex + i;
					m_curNumTreeIndices++;
				}
			} else {
	 */
			Real Uscale = m_treeTypes[type].m_tileWidth * (Real)TILE_PIXEL_EXTENT / (Real)m_textureWidth;
			Real Vscale = m_treeTypes[type].m_tileWidth * (Real)TILE_PIXEL_EXTENT / (Real)m_textureHeight;
			Real UOffset = m_treeTypes[type].m_textureOrigin.x/(Real)m_textureWidth;
			Real VOffset = m_treeTypes[type].m_textureOrigin.y/(Real)m_textureHeight;
			if (m_treeTypes[type].m_halfTile) {
				Uscale *= 0.5f;
				Vscale *= 0.5f;
				VOffset += (TILE_PIXEL_EXTENT/2) / (Real)m_textureHeight;
			}
			for (i=0; i<numVertex; i++) {
				if (m_curNumTreeVertices[bNdx] >= MAX_TREE_VERTEX)
					break;

				// Update the uv values.  The W3D models each have their own texture, and
				// we use one texture with all images in one, so we have to change the uvs to
				// match.
				Real U, V;
				U = uvs[i].U;
				V = uvs[i].V;

				if (U>1.0f) U=1.0f;
				if (U<0.0f) U=0.0f;
				if (V>1.0f) V=1.0f;
				if (V<0.0f) V=0.0f;

				curVb->u1 = U*Uscale + UOffset;
				curVb->v1 = V*Vscale + VOffset;
				Real x = pVert[i].X;
				Real y = pVert[i].Y;


				Vector3 vLoc;
				x += m_treeTypes[type].m_offset.X;
				y += m_treeTypes[type].m_offset.Y;
				vLoc.X = x*scale*theCos - y*scale*theSin;
				vLoc.Y = y*scale*theCos + x*scale*theSin;
				vLoc.Z = pVert[i].Z*scale;
				vLoc.Z += m_treeTypes[type].m_offset.Z;

				if (m_trees[curTree].m_toppleState != TOPPLE_UPRIGHT) {
					Matrix3D::Transform_Vector(m_trees[curTree].m_mtx, vLoc, &vLoc);
				} else {
					if (m_trees[curTree].pushAside>0.0f) {
						vLoc.X += pVert[i].Z * m_trees[curTree].pushAside * m_trees[curTree].pushAsideCos * m_treeTypes[type].m_data->m_maxOutwardMovement;
						vLoc.Y += pVert[i].Z * m_trees[curTree].pushAside * m_trees[curTree].pushAsideSin* m_treeTypes[type].m_data->m_maxOutwardMovement;
					}
					vLoc.X += loc.X;
					vLoc.Y += loc.Y;
					vLoc.Z += loc.Z;
				}


				curVb->x = vLoc.X;
				curVb->y = vLoc.Y;
				curVb->z = vLoc.Z;
				curVb->nx = m_trees[curTree].swayType;
				curVb->ny = 1.0f - m_treeTypes[type].m_data->m_darkening*m_trees[curTree].pushAside;
				curVb->nz = loc.Z;
				if (doVertexLighting) {
					Vector3 normal(0.0f, 0.0f, 1.0f);
					if (normals) {
						normal.X = normals[i].X*theCos - normals[i].Y*theSin;
						normal.Y = normals[i].Y*theCos + normals[i].X*theSin;
						normal.Z = normals[i].Z;
					}
					UnsignedInt vertexDiffuse;
					if (vecDiffuse) {
						vertexDiffuse = vecDiffuse[i];
					} else {
						vertexDiffuse = 0xffffffff;
					}
					curVb->diffuse = doLighting(&normal, objectLighting, &emissive,
														vertexDiffuse, 1.0f);
				} else {
					curVb->diffuse = diffuse;
				}
				curVb++;
				m_curNumTreeVertices[bNdx]++;
			}

			for (i=0; i<numIndex; i++) {
				if (m_curNumTreeIndices[bNdx]+4 > MAX_TREE_INDEX)
					break;
				*curIb++ = startVertex + pPoly[i].I;
				*curIb++ = startVertex + pPoly[i].J;
				*curIb++ = startVertex + pPoly[i].K;
				m_curNumTreeIndices[bNdx]+=3;
			}
		}
	}

}
//=============================================================================
// W3DTreeBuffer::updateVertexBuffer
//=============================================================================
/** Updates the push aside offset in vertex buffer. */
//=============================================================================
void W3DTreeBuffer::updateVertexBuffer()
{
	if (!m_indexTree[0] || !m_vertexTree[0] || !m_initialized) {
		return;
	}
	Int bNdx;
	for	(bNdx = 0; bNdx<MAX_BUFFERS; bNdx++) {
		if (m_curNumTreeIndices[bNdx]==0) {
			break;
		}
		VertexFormatXYZNDUV1 *vb;
		// Lock the buffers.
	#ifdef USE_STATIC
		DX8VertexBufferClass::WriteLockClass lockVtxBuffer(m_vertexTree[bNdx], 0);
	#else
		DX8VertexBufferClass::WriteLockClass lockVtxBuffer(m_vertexTree[bNdx], D3DLOCK_DISCARD);
	#endif
		vb=(VertexFormatXYZNDUV1*)lockVtxBuffer.Get_Vertex_Array();
		if (!vb) {
			continue;
		}

		VertexFormatXYZNDUV1 *curVb;

		Int curTree;
		for (curTree=0; curTree<m_numTrees; curTree++) {
			if (m_trees[curTree].bufferNdx!=bNdx) {
				continue;
			}
			Int type = m_trees[curTree].treeType;
			if (m_trees[curTree].pushAsideDelta==0.0f && m_trees[curTree].m_toppleState == TOPPLE_UPRIGHT) {
				continue; // not toppling or pushed, no need to update. jba [7/11/2003]
			}
			m_anyPushChanged = true;
			if (!m_trees[curTree].visible) continue;
			Real scale = m_trees[curTree].scale;
			Vector3 loc = m_trees[curTree].location;
			Real theSin = m_trees[curTree].sin;
			Real theCos = m_trees[curTree].cos;
			DEBUG_ASSERTCRASH(type>=0 && m_treeTypes[type].m_mesh!=nullptr, ("Invalid tree type or mesh."));

			Int startVertex = m_trees[curTree].firstIndex;
			curVb = vb+startVertex;
			Int i;
			Int numVertex = m_treeTypes[type].m_mesh->Peek_Model()->Get_Vertex_Count();
			Vector3 *pVert = m_treeTypes[type].m_mesh->Peek_Model()->Get_Vertex_Array();

			for (i=0; i<numVertex; i++) {
				Real x = pVert[i].X;
				Real y = pVert[i].Y;


				Vector3 vLoc;
				x += m_treeTypes[type].m_offset.X;
				y += m_treeTypes[type].m_offset.Y;
				vLoc.X = x*scale*theCos - y*scale*theSin;
				vLoc.Y = y*scale*theCos + x*scale*theSin;
				vLoc.Z = pVert[i].Z*scale;
				vLoc.Z += m_treeTypes[type].m_offset.Z;

				if (m_trees[curTree].m_toppleState != TOPPLE_UPRIGHT) {
					m_trees[curTree].m_mtx.Transform_Vector(m_trees[curTree].m_mtx, vLoc, &vLoc);
				} else {
					if (m_trees[curTree].pushAside>0.0f) {
						vLoc.X += pVert[i].Z * m_trees[curTree].pushAside * m_trees[curTree].pushAsideCos * m_treeTypes[type].m_data->m_maxOutwardMovement;
						vLoc.Y += pVert[i].Z * m_trees[curTree].pushAside * m_trees[curTree].pushAsideSin* m_treeTypes[type].m_data->m_maxOutwardMovement;
					}
					vLoc.X += loc.X;
					vLoc.Y += loc.Y;
					vLoc.Z += loc.Z;
				}

				curVb->x = vLoc.X;
				curVb->y = vLoc.Y;
				curVb->z = vLoc.Z;
				curVb->ny = 1.0f - m_treeTypes[type].m_data->m_darkening*m_trees[curTree].pushAside;
				curVb++;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// W3DTreeBuffer::~W3DTreeBuffer
//=============================================================================
/** Destructor. Releases w3d assets. */
//=============================================================================
W3DTreeBuffer::~W3DTreeBuffer()
{
	freeTreeBuffers();
	REF_PTR_RELEASE(m_treeTexture);
	Int i;
	for (i=0; i<MAX_TYPES; i++) {
		REF_PTR_RELEASE(m_treeTypes[i].m_mesh);
	}

	delete m_shadow;
	m_shadow = nullptr;
}

//=============================================================================
// W3DTreeBuffer::W3DTreeBuffer
//=============================================================================
/** Constructor. Sets m_initialized to true if it finds the w3d models it needs
for the trees. */
//=============================================================================
W3DTreeBuffer::W3DTreeBuffer()
{
	m_initialized = false;
	Int i;
	for	(i=0; i<MAX_BUFFERS; i++) {
		m_vertexTree[i] = nullptr;
		m_indexTree[i] = nullptr;
		m_curNumTreeVertices[i]=0;
		m_curNumTreeIndices[i]=0;
	}
	m_treeTexture = nullptr;
	m_dwTreeVertexShader = 0;
	m_dwTreePixelShader = 0;
	clearAllTrees();
	allocateTreeBuffers();
	m_initialized = true;
	m_curSwayVersion = -1;

	m_shadow = nullptr;

}


//=============================================================================
// W3DTreeBuffer::freeTreeBuffers
//=============================================================================
/** Frees the index and vertex buffers. */
//=============================================================================
void W3DTreeBuffer::freeTreeBuffers()
{
	Int i;
	for	(i=0; i<MAX_BUFFERS; i++) {
		REF_PTR_RELEASE(m_vertexTree[i]);
		REF_PTR_RELEASE(m_indexTree[i]);
	}

	if (m_dwTreePixelShader)
		DX8Wrapper::_Get_D3D_Device8()->DeletePixelShader(m_dwTreePixelShader);
	m_dwTreePixelShader = 0;

	if (m_dwTreeVertexShader)
		DX8Wrapper::_Get_D3D_Device8()->DeleteVertexShader(m_dwTreeVertexShader);
	m_dwTreeVertexShader = 0;
}

//=============================================================================
// W3DTreeBuffer::unitMoved
//=============================================================================
/** Check to see if a unit collided with a tree/grass/bush. */
//=============================================================================
void W3DTreeBuffer::unitMoved(Object *unit)
{
	if (unit->isKindOf(KINDOF_IMMOBILE)) {
		// This is the initial positioning of the object, and we don't care. jba. [6/5/2003]
		return;
	}
	Real radius = unit->getGeometryInfo().getMajorRadius();
	if (unit->getGeometryInfo().getGeomType()==GEOMETRY_BOX) {
		if (radius>unit->getGeometryInfo().getMinorRadius()) {
			radius = unit->getGeometryInfo().getMinorRadius();
		}
	}
	// Value to assume for the tree radius.
#define TREE_RADIUS_APPROX 7.0f
	radius += TREE_RADIUS_APPROX;

	Coord3D pos = *unit->getPosition();
	Real x = pos.x-radius;
	Real y = pos.y-radius;
	if (x<m_bounds.lo.x) x = m_bounds.lo.x;
	if (y<m_bounds.lo.y) y = m_bounds.lo.y;
	if (x>m_bounds.hi.x) x = m_bounds.hi.x;
	if (y>m_bounds.hi.y) y = m_bounds.hi.y;
	Int xIndex = REAL_TO_INT_FLOOR ( (x/(m_bounds.hi.x-m_bounds.lo.x)) * (static_cast<float>(PARTITION_WIDTH_HEIGHT)-0.1f) );
	Int yIndex = REAL_TO_INT_FLOOR ( (y/(m_bounds.hi.y-m_bounds.lo.y)) * (static_cast<float>(PARTITION_WIDTH_HEIGHT)-0.1f) );
	DEBUG_ASSERTCRASH(xIndex>=0 && yIndex>=0 && xIndex<PARTITION_WIDTH_HEIGHT && yIndex<PARTITION_WIDTH_HEIGHT, ("Invalid range."));

	x = pos.x+radius;
	y = pos.y+radius;
	if (x<m_bounds.lo.x) x = m_bounds.lo.x;
	if (y<m_bounds.lo.y) y = m_bounds.lo.y;
	if (x>m_bounds.hi.x) x = m_bounds.hi.x;
	if (y>m_bounds.hi.y) y = m_bounds.hi.y;
	Int xMax = REAL_TO_INT_CEIL ( (x/(m_bounds.hi.x-m_bounds.lo.x)) * (static_cast<float>(PARTITION_WIDTH_HEIGHT)-0.1f) );
	Int yMax = REAL_TO_INT_CEIL ( (y/(m_bounds.hi.y-m_bounds.lo.y)) * (static_cast<float>(PARTITION_WIDTH_HEIGHT)-0.1f) );
	DEBUG_ASSERTCRASH(xMax>=0 && yMax>=0 && xMax<=PARTITION_WIDTH_HEIGHT && yMax<=PARTITION_WIDTH_HEIGHT, ("Invalid range."));
	Int i, j;
	for (i=xIndex; i<xMax; i++) {
		for (j=yIndex; j<yMax; j++) {
			Int treeNdx = m_areaPartition[i + PARTITION_WIDTH_HEIGHT*j];
			while (treeNdx != END_OF_PARTITION) {
				// paranoia [7/7/2003]
				if (treeNdx<0 || treeNdx>=m_numTrees) {
					DEBUG_CRASH(("Invalid index."));
					break;
				}
				if (m_trees[treeNdx].treeType<0) {
					treeNdx = m_trees[treeNdx].nextInPartition;
					continue;	//  Tree is deleted. [7/11/2003]
				}
				Coord3D delta;
				delta.set(m_trees[treeNdx].location.X, m_trees[treeNdx].location.Y, m_trees[treeNdx].location.Z );
				delta.sub(pos);
				if (radius*radius>delta.lengthSqr()) {
					bool canTopple = unit->getCrusherLevel() > 1;
					if (canTopple && m_treeTypes[m_trees[treeNdx].treeType].m_data->m_doTopple) {
						// Give a vector with direction to thing.
						Coord3D toppleVector;
						toppleVector.set(m_trees[treeNdx].location.X, m_trees[treeNdx].location.Y, 0);
						toppleVector.x -= unit->getPosition()->x;
						toppleVector.y -= unit->getPosition()->y;
						applyTopplingForce(m_trees+treeNdx, &toppleVector, 0, W3D_TOPPLE_OPTIONS_NONE);
					} else if (m_treeTypes[m_trees[treeNdx].treeType].m_data->m_framesToMoveOutward>1) {
						pushAsideTree(m_trees[treeNdx].drawableID, &pos, unit->getUnitDirectionVector2D(), unit->getID());
					}
				}
				treeNdx = m_trees[treeNdx].nextInPartition;
			}
		}
	}


}

//=============================================================================
// W3DTreeBuffer::allocateTreeBuffers
//=============================================================================
/** Allocates the index and vertex buffers. */
//=============================================================================
void W3DTreeBuffer::allocateTreeBuffers()
{
	Int i;
	for	(i=0; i<MAX_BUFFERS; i++) {
	#ifdef USE_STATIC
		m_vertexTree[i]=NEW_REF(DX8VertexBufferClass,(DX8_FVF_XYZNDUV1,MAX_TREE_VERTEX+4,DX8VertexBufferClass::USAGE_DEFAULT));
		m_indexTree[i]=NEW_REF(DX8IndexBufferClass,(MAX_TREE_INDEX+4, DX8IndexBufferClass::USAGE_DEFAULT));
	#else
		m_vertexTree[i]=NEW_REF(DX8VertexBufferClass,(DX8_FVF_XYZNDUV1,MAX_TREE_VERTEX+4,DX8VertexBufferClass::USAGE_DYNAMIC));
		m_indexTree[i]=NEW_REF(DX8IndexBufferClass,(MAX_TREE_INDEX+4, DX8IndexBufferClass::USAGE_DYNAMIC));
	#endif
		m_curNumTreeVertices[i]=0;
		m_curNumTreeIndices[i]=0;
	}

		//shader decleration
	// DX8_FVF_XYZNDUV1
	DWORD Declaration[] =
	{
		D3DVSD_STREAM( 0 ),
		D3DVSD_REG( 0, D3DVSDT_FLOAT3 ),  // Position
		D3DVSD_REG( 1, D3DVSDT_FLOAT3 ),  // Normal
		D3DVSD_REG( 2, D3DVSDT_D3DCOLOR), // Diffuse color
		D3DVSD_REG( 7, D3DVSDT_FLOAT2 ),  // Tex coord
		D3DVSD_END()
	};

	HRESULT hr;
	hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\Trees.vso", &Declaration[0], 0, true, &m_dwTreeVertexShader);
	if (FAILED(hr))
		return;

	hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\Trees.pso", &Declaration[0], 0, false, &m_dwTreePixelShader);
	if (FAILED(hr))
		return;
}

//=============================================================================
// W3DTreeBuffer::clearAllTrees
//=============================================================================
/** Removes all trees. */
//=============================================================================
void W3DTreeBuffer::clearAllTrees()
{
	m_numTrees=0;
	m_bounds.lo.x = m_bounds.lo.y = 0;
	m_bounds.hi.x = m_bounds.hi.y = 1;
	REF_PTR_RELEASE(m_treeTexture);
	m_curNumTreeIndices[0]=0;
	m_anythingChanged = true;
	Int i;
	for (i=0; i<MAX_TYPES; i++) {
		REF_PTR_RELEASE(m_treeTypes[i].m_mesh);
	}
	for (i=0; i<PARTITION_WIDTH_HEIGHT*PARTITION_WIDTH_HEIGHT; i++) {
		m_areaPartition[i] = END_OF_PARTITION;
	}
	m_numTreeTypes = 0;
}

//=============================================================================
// W3DTreeBuffer::removeTree
//=============================================================================
/** Removes a tree.  */
//=============================================================================
void W3DTreeBuffer::removeTree(DrawableID id)
{
	Int i;
	for (i=0; i<m_numTrees; i++) {
		if (m_trees[i].drawableID == id) {
			m_trees[i].location = Vector3(0,0,0);
			m_trees[i].treeType = DELETED_TREE_TYPE;
			// Translate the bounding sphere of the model.
			m_trees[i].bounds.Center = Vector3(0,0,0);
			m_trees[i].bounds.Radius = 1;
			m_anythingChanged = true;
		}
	}
}

//=============================================================================
// W3DTreeBuffer::removeTree
//=============================================================================
/** Removes any trees that would be under a building.  */
//=============================================================================
void W3DTreeBuffer::removeTreesForConstruction(const Coord3D* pos, const GeometryInfo& geom, Real angle )
{
	// Just iterate all trees, as even non-collidable ones get removed. jba. [7/11/2003]
	Int i;
	for (i=0; i<m_numTrees; i++) {				// small, height,							radius,									minor radius
		if (m_trees[i].treeType < 0) {
			continue; // already deleted. jba [7/11/2003]
		}
		GeometryInfo info(GEOMETRY_CYLINDER, false, 5*TREE_RADIUS_APPROX, 2*TREE_RADIUS_APPROX, 2*TREE_RADIUS_APPROX);
		Coord3D treePos;
		treePos.set(m_trees[i].location.X, m_trees[i].location.Y, m_trees[i].location.Z);
		if (ThePartitionManager->geomCollidesWithGeom( pos, geom, angle, &treePos, info, 0.0f)) {
			// remove it [7/11/2003]
			m_trees[i].treeType = DELETED_TREE_TYPE;
			m_anythingChanged = true;
		}
	}
}


//=============================================================================
// W3DTreeBuffer::addTreeTypes
//=============================================================================
/** Adds a type of tree (model & texture). */
//=============================================================================
Int W3DTreeBuffer::addTreeType(const W3DTreeDrawModuleData *data)
{
	if (m_numTreeTypes>=MAX_TYPES) {
		DEBUG_CRASH(("Too many kinds of trees in map.  Reduce kinds of trees, or raise tree limit. jba."));
		return 0;
	}
	m_needToUpdateTexture = true;

	m_treeTypes[m_numTreeTypes].m_mesh = nullptr;

	RenderObjClass *robj=WW3DAssetManager::Get_Instance()->Create_Render_Obj(data->m_modelName.str());

	if (robj==nullptr) {
		DEBUG_CRASH(("Unable to find model for tree %s", data->m_modelName.str()));
		return 0;
	}
	AABoxClass box;

	robj->Get_Obj_Space_Bounding_Box(box);
	Vector3 offset(0,0,0);
	if (robj->Class_ID() == RenderObjClass::CLASSID_HLOD) {
		RenderObjClass *hlod = robj;
		robj = hlod->Get_Sub_Object(0);
		const Matrix3D xfm = robj->Get_Bone_Transform(0);
		xfm.Get_Translation(&offset);
		REF_PTR_RELEASE(hlod);
	}

	if (robj->Class_ID() == RenderObjClass::CLASSID_MESH)
		m_treeTypes[m_numTreeTypes].m_mesh = (MeshClass*)robj;

	if (m_treeTypes[m_numTreeTypes].m_mesh==nullptr) {
		DEBUG_CRASH(("Tree %s is not simple mesh. Tell artist to re-export. Don't Ignore!!!", data->m_modelName.str()));
		return 0;
	}

	Int numVertex = m_treeTypes[m_numTreeTypes].m_mesh->Peek_Model()->Get_Vertex_Count();
	Vector3 *pVert = m_treeTypes[m_numTreeTypes].m_mesh->Peek_Model()->Get_Vertex_Array();

	const Matrix3D xfm = m_treeTypes[m_numTreeTypes].m_mesh->Get_Transform();
	SphereClass bounds(pVert, numVertex);
	bounds.Center += offset;
	m_treeTypes[m_numTreeTypes].m_bounds = bounds;
	m_treeTypes[m_numTreeTypes].m_textureOrigin.x = 0;
	m_treeTypes[m_numTreeTypes].m_textureOrigin.y = 0;
	m_treeTypes[m_numTreeTypes].m_data = data;
	m_treeTypes[m_numTreeTypes].m_offset = offset;
	m_treeTypes[m_numTreeTypes].m_shadowSize = (box.Extent.X + box.Extent.Y); // Average extent * 2. jba.
	m_treeTypes[m_numTreeTypes].m_doShadow = data->m_doShadow;
	m_numTreeTypes++;
	return m_numTreeTypes-1;
}

//=============================================================================
// W3DTreeBuffer::addTree
//=============================================================================
/** Adds a tree.  Name is the W3D model name, supported models are
ALPINE, DECIDUOUS and SHRUB. */
//=============================================================================
void W3DTreeBuffer::addTree(DrawableID id, Coord3D location, Real scale, Real angle,
								Real randomScaleAmount, const W3DTreeDrawModuleData *data)
{
	if (m_numTrees >= MAX_TREES) {
		return;
	}
	if (!m_initialized) {
		return;
	}
	Int treeType = DELETED_TREE_TYPE;
	Int i;
	for (i=0; i<m_numTreeTypes; i++) {
		if (m_treeTypes[i].m_data->m_modelName.compareNoCase(data->m_modelName)==0 &&
				m_treeTypes[i].m_data->m_textureName.compareNoCase(data->m_textureName)==0) {
			treeType = i;
			break;
		}
	}
	if (treeType<0) {
		treeType = addTreeType(data);
		if (treeType<0) {
			return;
		}
		m_needToUpdateTexture = true;
	}
	if (data->m_framesToMoveOutward > 2 || data->m_doTopple) {
		// Trees/grass that topples or gets pushed aside (outward) gets put in the area partition. jba [7/7/2003]
		Short bucket = getPartitionBucket(location);
		m_trees[m_numTrees].nextInPartition = m_areaPartition[bucket];
		m_areaPartition[bucket] = m_numTrees;
	} else {
		m_trees[m_numTrees].nextInPartition = END_OF_PARTITION;
	}

	Real randomScale = GameClientRandomValueReal( 1.0f - randomScaleAmount, 1.0f+ randomScaleAmount );
	m_trees[m_numTrees].sin = WWMath::Sin(angle);
	m_trees[m_numTrees].cos = WWMath::Cos(angle);
	if (randomScaleAmount>0.0f) {
		// Randomizes the scale and orientation of trees.
		m_trees[m_numTrees].scale = scale*randomScale;
	} else {
		// Don't randomly scale & orient
		m_trees[m_numTrees].scale = scale;
	}
	m_trees[m_numTrees].location = Vector3(location.x, location.y, location.z);
	m_trees[m_numTrees].treeType = treeType;
	// Translate the bounding sphere of the model.
	m_trees[m_numTrees].bounds = m_treeTypes[treeType].m_bounds;
	m_trees[m_numTrees].bounds.Center *= m_trees[m_numTrees].scale;
	m_trees[m_numTrees].bounds.Radius *= m_trees[m_numTrees].scale;
	m_trees[m_numTrees].bounds.Center += m_trees[m_numTrees].location;
	// Initially set it invisible.  cull will update it's visibility flag.
	m_trees[m_numTrees].visible = false;
	m_trees[m_numTrees].drawableID = id;

	m_trees[m_numTrees].firstIndex = 0;
	m_trees[m_numTrees].bufferNdx = -1;

	m_trees[m_numTrees].swayType = GameClientRandomValue(1, MAX_SWAY_TYPES);
	m_trees[m_numTrees].pushAside = 0;
	m_trees[m_numTrees].lastFrameUpdated = 0;
	m_trees[m_numTrees].pushAsideSource = INVALID_ID;
	m_trees[m_numTrees].pushAsideDelta = 0;
	m_trees[m_numTrees].pushAsideCos = 1;
	m_trees[m_numTrees].pushAsideSin = 1;
	m_trees[m_numTrees].m_toppleState = TOPPLE_UPRIGHT;
	m_numTrees++;
}

//=============================================================================
// W3DTreeBuffer::updateTreePosition
//=============================================================================
/** Updates a tree's position */
//=============================================================================
Bool W3DTreeBuffer::updateTreePosition(DrawableID id, Coord3D location, Real angle)
{
	Int i;
	for (i=0; i<m_numTrees; i++) {
		if (m_trees[i].drawableID == id) {
			m_trees[i].location = Vector3(location.x, location.y, location.z);
			m_trees[i].sin = WWMath::Sin(angle);
			m_trees[i].cos = WWMath::Cos(angle);
			// Translate the bounding sphere of the model.
			m_trees[i].bounds = m_treeTypes[m_trees[i].treeType].m_bounds;
			m_trees[i].bounds.Center *= m_trees[i].scale;
			m_trees[i].bounds.Radius *= m_trees[i].scale;
			m_trees[i].bounds.Center += m_trees[i].location;
			m_anythingChanged = true;
			return true;
		}
	}
	return false;
}

//=============================================================================
// W3DTreeBuffer::pushAsideTree
//=============================================================================
/** Push sideways tree or grass. */
//=============================================================================
void W3DTreeBuffer::pushAsideTree(DrawableID id, const Coord3D *pusherPos,
																	const Coord3D *pusherDirection, ObjectID pusherID )
{
	Int i;
	for (i=0; i<m_numTrees; i++) {
		if (m_trees[i].drawableID == id) {
			UnsignedInt lastFrame = m_trees[i].lastFrameUpdated;
			m_trees[i].lastFrameUpdated = TheGameLogic->getFrame();
			if(m_trees[i].pushAsideSource == pusherID) {
				if (m_trees[i].lastFrameUpdated - lastFrame < 3)
					return; // already pushing. [5/28/2003]
			}

			if(m_trees[i].pushAside != 0.0f) {
				return; // already pushing. [5/28/2003]
			}
			m_trees[i].pushAsideSource = pusherID;
			Coord3D delta;
			delta.set(m_trees[i].location.X, m_trees[i].location.Y, m_trees[i].location.Z);
			delta.sub(*pusherPos);

			if (pusherDirection->x*delta.y - pusherDirection->y*delta.x > 0.0f) {
				m_trees[i].pushAsideCos = -pusherDirection->y;
				m_trees[i].pushAsideSin = pusherDirection->x;
			} else {
				m_trees[i].pushAsideCos = pusherDirection->y;
				m_trees[i].pushAsideSin = -pusherDirection->x;
			}
			m_anyPushChanged = true;
			m_trees[i].pushAsideDelta = 1.0f/(Real)m_treeTypes[m_trees[i].treeType].m_data->m_framesToMoveOutward;
		}
	}
}

DECLARE_PERF_TIMER(Tree_Render)

//=============================================================================
// W3DTreeBuffer::drawTrees
//=============================================================================
/** Draws the trees.  Uses camera to cull. */
//=============================================================================
void W3DTreeBuffer::drawTrees(CameraClass * camera, RefRenderObjListIterator *pDynamicLightsIterator)
{
	USE_PERF_TIMER(Tree_Render)
	if (!m_isTerrainPass) {
		return;
	}

	// if breeze changes, always process the full update, even if not visible,
	// so that things offscreen won't 'pop' when first viewed
	const BreezeInfo& info = TheScriptEngine->getBreezeInfo();
	if (info.m_breezeVersion != m_curSwayVersion)
	{
		updateSway(info);
	}

	// TheSuperHackers @tweak The tree sway, topple and sink time steps are now decoupled from the render update.
	const Real timeScale = TheFramePacer->getActualLogicTimeScaleOverFpsRatio();
	Vector3 swayFactor[MAX_SWAY_TYPES];
	Int i;
	for (i=0; i<MAX_SWAY_TYPES; i++)
	{
		m_curSwayOffset[i] += m_curSwayStep[i] * timeScale;
		if (m_curSwayOffset[i] > NUM_SWAY_ENTRIES-1) {
			m_curSwayOffset[i] -= NUM_SWAY_ENTRIES-1;
		}
		Int minOffset = REAL_TO_INT_FLOOR(m_curSwayOffset[i]);
		if (minOffset>=0 && minOffset+1<NUM_SWAY_ENTRIES) {
			Real f2 = m_curSwayOffset[i] - minOffset;
			Real f1 = 1.0f - f2;
			swayFactor[i] = f1*m_swayOffsets[minOffset] + f2*m_swayOffsets[minOffset+1];
			swayFactor[i] *= m_curSwayFactor[i];
		}
	}

	m_isTerrainPass = false;

	if (m_needToUpdateTexture) {
		m_needToUpdateTexture = false;
		updateTexture();
	}
	if (m_treeTexture==nullptr) {
		return;
	}
	if (m_updateAllKeys) {
		cull(camera);
	}

	Int curTree;
	// Draw tree shadows.
	if (m_shadow && TheW3DProjectedShadowManager && TheGlobalData->m_useShadowDecals) {
		for (curTree=0; curTree<m_numTrees; curTree++) {
			Int type = m_trees[curTree].treeType;
			if (type<0) { // deleted.
				continue;
			}
			if (!m_trees[curTree].visible || !m_treeTypes[type].m_doShadow) {
				continue;
			}

			if (m_trees[curTree].m_toppleState == TOPPLE_FALLING ||
					m_trees[curTree].m_toppleState == TOPPLE_DOWN) {
				continue;
			}
			m_shadow->setSize(m_treeTypes[type].m_shadowSize, m_treeTypes[type].m_shadowSize);
			m_shadow->setPosition(m_trees[curTree].location.X, m_trees[curTree].location.Y, m_trees[curTree].location.Z);
			TheW3DProjectedShadowManager->queueDecal(m_shadow);
		}
		TheW3DProjectedShadowManager->flushDecals(m_shadow->getTexture(0), SHADOW_DECAL);
	}

	// Update pushed aside and toppling trees.
	for (curTree=0; curTree<m_numTrees; curTree++) {
		Int type = m_trees[curTree].treeType;
		if (type<0) { // deleted.
			continue;
		}
		const W3DTreeDrawModuleData *moduleData = m_treeTypes[type].m_data;
		if(m_trees[curTree].m_toppleState == TOPPLE_FALLING ||
			 m_trees[curTree].m_toppleState == TOPPLE_FOGGED) {
			updateTopplingTree(m_trees+curTree, timeScale);
		} else if(m_trees[curTree].m_toppleState == TOPPLE_DOWN) {
			if (moduleData->m_killWhenToppled) {
				if (m_trees[curTree].m_sinkFramesLeft <= 0.0f) {
					m_trees[curTree].treeType = DELETED_TREE_TYPE; // delete it. [7/11/2003]
					m_anythingChanged = true; // need to regenerate trees. [7/11/2003]
				}
				const Real sinkDistancePerFrame = moduleData->m_sinkDistance / moduleData->m_sinkFrames;
				m_trees[curTree].m_sinkFramesLeft -= timeScale;
				m_trees[curTree].location.Z -= sinkDistancePerFrame * timeScale;
				m_trees[curTree].m_mtx.Set_Translation(m_trees[curTree].location);
			}
		} else if (m_trees[curTree].pushAsideDelta!=0.0f) {
			m_trees[curTree].pushAside += m_trees[curTree].pushAsideDelta;
			if (m_trees[curTree].pushAside>=1.0f) {
				m_trees[curTree].pushAsideDelta = -1.0f/(Real)moduleData->m_framesToMoveInward;
			} else if (m_trees[curTree].pushAside<=0.0f) {
				m_trees[curTree].pushAsideDelta = 0.0f;
				m_trees[curTree].pushAside = 0.0f;
			}
		}
	}

	if (m_anythingChanged) {
		loadTreesInVertexAndIndexBuffers(pDynamicLightsIterator);
		m_anythingChanged = false;
	} else if (m_anyPushChanged) {
		m_anyPushChanged = false;
		updateVertexBuffer();
	}

//#define DEBUG_TEXTURE 1
#ifdef DEBUG_TEXTURE // Draw the combined texture for debugging. jba. [4/21/2003]
	// Setup the vertex buffer, shader & texture.
	DX8Wrapper::Set_Shader(detailAlphaShader);
	DX8Wrapper::Set_Texture(0,m_treeTexture);
	DynamicIBAccessClass ib_access(BUFFER_TYPE_DYNAMIC_DX8, 6);
	//draw an infinite sky plane
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8, DX8_FVF_XYZNDUV2, 4);
	{
		DynamicIBAccessClass::WriteLockClass ibLock(&ib_access);
		UnsignedShort *ndx = ibLock.Get_Index_Array();

		if (ndx) {
			ndx[0] = 0;
			ndx[1] = 1;
			ndx[2] = 2;
			ndx[3] = 1;
			ndx[4] = 3;
			ndx[5] = 2;
		}
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();
		if(verts)
		{
			Real width = 300;
			Real origin = 40;
			verts[0].x=origin;
			verts[0].y=origin;
			verts[0].z=15;
			verts[0].u1=0;
			verts[0].v1=0;
			verts[0].diffuse=0xffffffff;

			verts[1].x=origin+width;
			verts[1].y=origin;
			verts[1].z=15;
			verts[1].u1=1;
			verts[1].v1=0;
			verts[1].diffuse=0xffffffff;

			verts[2].x=origin;
			verts[2].y=origin+width;
			verts[2].z=15;
			verts[2].u1=0;
			verts[2].v1=1;
			verts[2].diffuse=0xffffffff;

			verts[3].x=origin+width;
			verts[3].y=origin+width;
			verts[3].z=15;
			verts[3].u1=1;
			verts[3].v1=1;
			verts[3].diffuse=0xffffffff;
		}
	}

	DX8Wrapper::Set_Index_Buffer(ib_access,0);
	DX8Wrapper::Set_Vertex_Buffer(vb_access);

	Matrix3D tm(1);
	DX8Wrapper::Set_Transform(D3DTS_WORLD,tm);

	DX8Wrapper::Draw_Triangles(	0,2, 0,	4);	//draw a quad, 2 triangles, 4 verts
#endif


	if (m_curNumTreeIndices[0] == 0) {
		return;
	}
	DX8Wrapper::Set_Shader(detailAlphaShader);

	DX8Wrapper::Set_Texture(0,m_treeTexture);
	DX8Wrapper::Set_Texture(1,nullptr);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(1,  D3DTSS_TEXCOORDINDEX, 1);
	// Draw all the trees.
	DX8Wrapper::Apply_Render_State_Changes();
	W3DShaderManager::setShroudTex(1);
	DX8Wrapper::Apply_Render_State_Changes();

	if (m_dwTreeVertexShader) {
		// GeneralsX @bugfix BenderAI 13/02/2026 Matrix4x4 for GLM compatibility (fighter19 pattern)
		Matrix4x4 matProj, matView, matWorld;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_WORLD, matWorld);
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, matView);
		DX8Wrapper::_Get_DX8_Transform(D3DTS_PROJECTION, matProj);
		Matrix4x4 mat;
		D3DXMatrixMultiply( (D3DXMATRIX*)&mat, (D3DXMATRIX*)&matView, (D3DXMATRIX*)&matProj );
		D3DXMatrixMultiply( (D3DXMATRIX*)&mat, (D3DXMATRIX*)&matWorld, (D3DXMATRIX*)&mat );
		D3DXMatrixTranspose( (D3DXMATRIX*)&mat, (D3DXMATRIX*)&mat );

		// c4  - Composite World-View-Projection Matrix
		DX8Wrapper::_Get_D3D_Device8()->SetVertexShaderConstant(  4, &mat,  4 );
		Vector4 noSway(0,0,0,0);
		DX8Wrapper::_Get_D3D_Device8()->SetVertexShaderConstant(  8, &noSway,  1 );

		// c8 - c8+MAX_SWAY_TYPES - the sway amount.
		for	(i=0; i<MAX_SWAY_TYPES; i++) {
			Vector4 sway4(swayFactor[i].X, swayFactor[i].Y, swayFactor[i].Z, 0);
			DX8Wrapper::_Get_D3D_Device8()->SetVertexShaderConstant(  9+i, &sway4,  1 );
		}

		W3DShroud *shroud;
		if ((shroud=TheTerrainRenderObject->getShroud()) != nullptr) {
			// Setup shroud texture info [6/6/2003]
			float xoffset = 0;
			float yoffset = 0;
			Real width=shroud->getCellWidth();
			Real height=shroud->getCellHeight();

			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
			Vector4 offset(xoffset, yoffset, 0, 0);
			DX8Wrapper::_Get_D3D_Device8()->SetVertexShaderConstant(  32, &offset,  1 );
			width = 1.0f/(width*shroud->getTextureWidth());
			height = 1.0f/(height*shroud->getTextureHeight());
			offset.Set(width, height, 1, 1);
			DX8Wrapper::_Get_D3D_Device8()->SetVertexShaderConstant(  33, &offset,  1 );

		} else {
			Vector4 offset(0,0,0,0);
			DX8Wrapper::_Get_D3D_Device8()->SetVertexShaderConstant(  32, &offset,  1 );
			DX8Wrapper::_Get_D3D_Device8()->SetVertexShaderConstant(  33, &offset,  1 );
		}

		DX8Wrapper::Set_Vertex_Shader(m_dwTreeVertexShader);
#if 0
		DX8Wrapper::Set_Pixel_Shader(m_dwTreePixelShader);
		// a.c. 6/16 - allow switching between normal and 2X mode for terrain
		Real mulTwoX = 0.5f;
		if(TheGlobalData && TheGlobalData->m_useOverbright)
			mulTwoX = 1.0f;
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstant(1, D3DXVECTOR4(mulTwoX, mulTwoX, mulTwoX, mulTwoX), 1);
#endif

	} else {
		DX8Wrapper::Set_Vertex_Shader(DX8_FVF_XYZNDUV1);
	}


	Int bNdx;
	for (bNdx=0;bNdx<MAX_BUFFERS; bNdx++) {
		if (m_curNumTreeIndices[bNdx]==0) {
			break;
		}
		DX8Wrapper::Set_Index_Buffer(m_indexTree[bNdx],0);
		DX8Wrapper::Set_Vertex_Buffer(m_vertexTree[bNdx]);
		// Render the waving grass
		DX8Wrapper::Apply_Render_State_Changes();
		if (m_dwTreeVertexShader) {
			DX8Wrapper::_Get_D3D_Device8()->SetVertexShader(m_dwTreeVertexShader);
			DX8Wrapper::_Get_D3D_Device8()->SetTextureStageState(0,  D3DTSS_TEXCOORDINDEX, 0);
			DX8Wrapper::_Get_D3D_Device8()->SetTextureStageState(1,  D3DTSS_TEXCOORDINDEX, 1);
			DX8Wrapper::_Get_D3D_Device8()->SetTextureStageState(1,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		}
		DX8Wrapper::Draw_Triangles(	0, m_curNumTreeIndices[bNdx]/3, 0,	m_curNumTreeVertices[bNdx]);
	}

	DX8Wrapper::Set_Vertex_Shader(DX8_FVF_XYZNDUV1);
	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Invalidate_Cached_Render_States();	//code above mucks around with W3D states so make sure we reset

}

//-------------------------------------------------------------------------------------------------
///< Start the toppling process by giving a force vector
//-------------------------------------------------------------------------------------------------
void W3DTreeBuffer::applyTopplingForce( TTree *tree, const Coord3D* toppleDirection, Real toppleSpeed,
																			 UnsignedInt options )
{
	if (tree->m_toppleState != TOPPLE_UPRIGHT) {
		return;
	}
	const W3DTreeDrawModuleData* d = m_treeTypes[tree->treeType].m_data;
  // Having a low toppleSpeed is BAD. In particular, if the toppleSpeed is exactly 0, the
  // tree will stay upright forever, frozen in place (because the sway update is dead)
  // but never dying
  if ( toppleSpeed < d->m_minimumToppleSpeed )
  {
    toppleSpeed = d->m_minimumToppleSpeed;
  }

	tree->m_toppleDirection = *toppleDirection;
	tree->m_toppleDirection.normalize();
	tree->m_angularAccumulation = 0;

	tree->m_angularVelocity = toppleSpeed * d->m_initialVelocityPercent;
	tree->m_angularAcceleration = toppleSpeed * d->m_initialAccelPercent;
	tree->m_toppleState = TOPPLE_FALLING;
	tree->m_options = options;
	Coord3D pos;
	pos.set(tree->location.X, tree->location.Y, tree->location.Z);
	FXList::doFXPos(d->m_toppleFX, &pos);
	m_anyPushChanged = true;
	tree->m_mtx.Make_Identity();
	tree->m_mtx.Set_Translation(tree->location);

}

// this is our "bounce" limit -- slightly less that 90 degrees, to account for slop.
static const Real ANGULAR_LIMIT = PI/2 - PI/64;

//-------------------------------------------------------------------------------------------------
///< Keep track of rotational fall distance, bounce and/or stop when needed.
//-------------------------------------------------------------------------------------------------
void W3DTreeBuffer::updateTopplingTree(TTree *tree, Real timeScale)
{
	//DLOG(Debug::Format("updating W3DTreeBuffer %08lx\n",this));
	DEBUG_ASSERTCRASH(tree->m_toppleState != TOPPLE_UPRIGHT, ("hmm, we should be sleeping here"));
	if ( (tree->m_toppleState == TOPPLE_UPRIGHT)  ||  (tree->m_toppleState == TOPPLE_DOWN) )
		return;

	const W3DTreeDrawModuleData* d = m_treeTypes[tree->treeType].m_data;
	const Int localPlayerIndex = rts::getObservedOrLocalPlayerIndex_Safe();
	Coord3D pos;
	pos.set(tree->location.X, tree->location.Y, tree->location.Z);
	ObjectShroudStatus ss = ThePartitionManager->getPropShroudStatusForPlayer(localPlayerIndex, &pos);
	if (ss==OBJECTSHROUD_FOGGED) {
		// Don't update fogged trees. [8/11/2003]
		tree->m_toppleState = TOPPLE_FOGGED;
		return;
	} else if (tree->m_toppleState == TOPPLE_FOGGED) {
		// was fogged, now isn't.
		tree->m_angularVelocity = 0;
		tree->m_toppleState = TOPPLE_DOWN;
		tree->m_mtx.In_Place_Pre_Rotate_X(-ANGULAR_LIMIT * tree->m_toppleDirection.y);
		tree->m_mtx.In_Place_Pre_Rotate_Y(ANGULAR_LIMIT * tree->m_toppleDirection.x);
		if (d->m_killWhenToppled) {
			// If got killed in the fog, just remove. jba [8/11/2003]
			tree->m_sinkFramesLeft = 0.0f;
		}
		return;
	}
	const Real VELOCITY_BOUNCE_LIMIT = 0.01f;				// if the velocity after a bounce will be this or lower, just stop at zero
	const Real VELOCITY_BOUNCE_SOUND_LIMIT = 0.03f;	// and if this low, then skip the bounce sound

	Real curVelToUse = tree->m_angularVelocity * timeScale;
	if (tree->m_angularAccumulation + curVelToUse > ANGULAR_LIMIT)
		curVelToUse = ANGULAR_LIMIT - tree->m_angularAccumulation;

	tree->m_mtx.In_Place_Pre_Rotate_X(-curVelToUse * tree->m_toppleDirection.y);
	tree->m_mtx.In_Place_Pre_Rotate_Y(curVelToUse * tree->m_toppleDirection.x);

	tree->m_angularAccumulation += curVelToUse;
	if ((tree->m_angularAccumulation >= ANGULAR_LIMIT) && (tree->m_angularVelocity > 0))
	{
		// Hit so either bounce or stop if too little remaining velocity.
		tree->m_angularVelocity *= -d->m_bounceVelocityPercent;

		if( BitIsSet( tree->m_options, W3D_TOPPLE_OPTIONS_NO_BOUNCE ) == TRUE ||
				fabs(tree->m_angularVelocity) < VELOCITY_BOUNCE_LIMIT )
		{
			// too slow, just stop
			tree->m_angularVelocity = 0;
			tree->m_toppleState = TOPPLE_DOWN;
			if (d->m_killWhenToppled) {
				tree->m_sinkFramesLeft = d->m_sinkFrames;
			}
		}
		else if( fabs(tree->m_angularVelocity) >= VELOCITY_BOUNCE_SOUND_LIMIT )
		{
			// fast enough bounce to warrant the bounce fx
			if( BitIsSet( tree->m_options, W3D_TOPPLE_OPTIONS_NO_FX ) == FALSE ) {
				Vector3 loc(0, 0, 3*TREE_RADIUS_APPROX); // Kinda towards the top of the tree. jba. [7/11/2003]
				Vector3 xloc;
				tree->m_mtx.Transform_Vector(tree->m_mtx, loc, &xloc);
				Coord3D pos;
				pos.set(xloc.X, xloc.Y, xloc.Z);
				FXList::doFXPos(d->m_bounceFX, &pos);
			}
		}
	}
	else
	{
		tree->m_angularVelocity += tree->m_angularAcceleration * timeScale;
	}

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void W3DTreeBuffer::crc( Xfer *xfer )
{
	// empty. jba [8/11/2003]
}

// ------------------------------------------------------------------------------------------------
/** Xfer
	* Version Info:
	* 1: Initial version
	* 2: TheSuperHackers @tweak Serialize sink frames as float instead of integer
	*/
// ------------------------------------------------------------------------------------------------
void W3DTreeBuffer::xfer( Xfer *xfer )
{

	// version
#if RETAIL_COMPATIBLE_XFER_SAVE
	XferVersion currentVersion = 1;
#else
	XferVersion currentVersion = 2;
#endif
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	Int i;
	Int numTrees = m_numTrees;
	xfer->xferInt(&numTrees);
	if (xfer->getXferMode() == XFER_LOAD)	{
		m_numTrees = 0;
		for (i=0; i<PARTITION_WIDTH_HEIGHT*PARTITION_WIDTH_HEIGHT; i++) {
			m_areaPartition[i] = END_OF_PARTITION;
		}
	}

	// Save trees. [8/11/2003]
	for	(i=0; i<numTrees; i++) {
		TTree tree;
		memset(&tree, 0, sizeof(tree));
		AsciiString modelName;
		AsciiString modelTexture;
		Int treeType = DELETED_TREE_TYPE;
		if (xfer->getXferMode() != XFER_LOAD) {
			tree = m_trees[i];
			treeType = m_trees[i].treeType;
			if (treeType != DELETED_TREE_TYPE) {
				modelName = m_treeTypes[treeType].m_data->m_modelName;
				modelTexture = m_treeTypes[treeType].m_data->m_textureName;
			}
		}
		xfer->xferAsciiString(&modelName);
		xfer->xferAsciiString(&modelTexture);
		if (xfer->getXferMode() == XFER_LOAD) {
			Int j;
			for (j=0; j<m_numTreeTypes; j++) {
				if (m_treeTypes[j].m_data->m_modelName.compareNoCase(modelName)==0 &&
						m_treeTypes[j].m_data->m_textureName.compareNoCase(modelTexture)==0) {
					treeType = j;
					break;
				}
			}
		}

		xfer->xferReal(&tree.location.X);
		xfer->xferReal(&tree.location.Y);
		xfer->xferReal(&tree.location.Z);

		xfer->xferReal(&tree.scale);	///< Scale at location.
		xfer->xferReal(&tree.sin);	///< Sine of the rotation angle at location.
		xfer->xferReal(&tree.cos);	///< Cosine of the rotation angle at location.

		xfer->xferDrawableID(&tree.drawableID);	///< Drawable this tree corresponds to.

		// Topple parameters. [7/7/2003]
		xfer->xferReal(&tree.m_angularVelocity);	///< Velocity in degrees per frame (or is it radians per frame?)
		xfer->xferReal(&tree.m_angularAcceleration);	///< Acceleration angularVelocity is increasing
		xfer->xferCoord3D(&tree.m_toppleDirection);	///< Z-less direction we are toppling
		xfer->xferUser(&tree.m_toppleState, sizeof(tree.m_toppleState));	///< Stage this module is in.
		xfer->xferReal(&tree.m_angularAccumulation);	///< How much have I rotated so I know when to bounce.
		xfer->xferUnsignedInt(&tree.m_options);	///< topple options
		xfer->xferMatrix3D(&tree.m_mtx);

		if (version <= 1)
		{
			UnsignedInt sinkFramesLeft = (UnsignedInt)tree.m_sinkFramesLeft;
			xfer->xferUnsignedInt(&sinkFramesLeft);	///< Toppled trees sink into the terrain & disappear, how many frames left.
			tree.m_sinkFramesLeft = (Real)sinkFramesLeft;
		}
		else
		{
			xfer->xferReal(&tree.m_sinkFramesLeft);	///< Toppled trees sink into the terrain & disappear, how many frames left.
		}

		if (xfer->getXferMode() == XFER_LOAD && treeType != DELETED_TREE_TYPE && treeType < m_numTreeTypes) {
			Coord3D pos;
			pos.set(tree.location.X, tree.location.Y, tree.location.Z);
			Real angle = 0;
			addTree(tree.drawableID, pos, tree.scale, angle, 0, m_treeTypes[treeType].m_data);
			if (m_numTrees) {
				TTree *curTree = &m_trees[m_numTrees-1];
				curTree->m_angularAcceleration = tree.m_angularAcceleration;
				curTree->m_angularVelocity = tree.m_angularVelocity;
				curTree->m_toppleDirection = tree.m_toppleDirection;
				curTree->m_toppleState = tree.m_toppleState;
				curTree->m_options = tree.m_options;
				curTree->m_mtx = tree.m_mtx;
				curTree->m_sinkFramesLeft = tree.m_sinkFramesLeft;
			}
		}
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void W3DTreeBuffer::loadPostProcess()
{
	// empty. jba [8/11/2003]
}




