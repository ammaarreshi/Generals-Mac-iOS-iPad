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

// FILE: Heightmap.cpp ////////////////////////////////////////////////
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
// File name: Heightmap.cpp
//
// Created:   Mark W., John Ahlquist, April/May 2001
//
// Desc:      Draw the terrain and scorchmarks in a scene.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//         Includes
//-----------------------------------------------------------------------------
#include "W3DDevice/GameClient/HeightMap.h"

#ifndef USE_FLAT_HEIGHT_MAP // Flat height map uses flattened textures. jba. [3/20/2003]

#include <stdlib.h>
#include <assetmgr.h>
#include <texture.h>
#include <tri.h>
#include <colmath.h>
#include <coltest.h>
#include <rinfo.h>
#include <camera.h>
#include <d3dx8core.h>
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"

#include "GameClient/TerrainVisual.h"
#include "GameClient/View.h"
#include "GameClient/Water.h"

#include "GameLogic/AIPathfind.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/W3DTerrainTracks.h"
#include "W3DDevice/GameClient/W3DBibBuffer.h"
#include "W3DDevice/GameClient/W3DTreeBuffer.h"
#include "W3DDevice/GameClient/W3DPropBuffer.h"
#include "W3DDevice/GameClient/W3DRoadBuffer.h"
#include "W3DDevice/GameClient/W3DBridgeBuffer.h"
#include "W3DDevice/GameClient/W3DWaypointBuffer.h"
#include "W3DDevice/GameClient/W3DCustomEdging.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/light.h"
#include "WW3D2/scene.h"
#include "W3DDevice/GameClient/W3DPoly.h"
#include "W3DDevice/GameClient/W3DCustomScene.h"

#include "Common/UnitTimings.h" //Contains the DO_UNIT_TIMINGS define jba.


#define no_OPTIMIZED_HEIGHTMAP_LIGHTING	01
// Doesn't work well.  jba.

HeightMapRenderObjClass *TheHeightMap = nullptr;
//-----------------------------------------------------------------------------
//         Private Data
//-----------------------------------------------------------------------------
#define SC_DETAIL_BLEND ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass detailOpaqueShader(SC_DETAIL_BLEND);

#define DEFAULT_MAX_FRAME_EXTRABLEND_TILES		256	//default number of terrain tiles rendered per call (must fit in one VB)
#define DEFAULT_MAX_MAP_EXTRABLEND_TILES		2048	//default size of array allocated to hold all map extra blend tiles.
#define DEFAULT_MAX_BATCH_SHORELINE_TILES		512	//maximum number of terrain tiles rendered per call (must fit in one VB)
#define DEFAULT_MAX_MAP_SHORELINE_TILES		4096	//default size of array allocated to hold all map shoreline tiles.

#define ADJUST_FROM_INDEX_TO_REAL(k) ((k-m_map->getBorderSizeInline())*MAP_XY_FACTOR)
inline Int IABS(Int x) {	if (x>=0) return x; return -x;};

//-----------------------------------------------------------------------------
//         Private Functions
//-----------------------------------------------------------------------------

//=============================================================================
// HeightMapRenderObjClass::freeIndexVertexBuffers
//=============================================================================
/** Frees the w3d resources used to draw the terrain. */
//=============================================================================
void HeightMapRenderObjClass::freeIndexVertexBuffers()
{
	REF_PTR_RELEASE(m_indexBuffer);

	if (m_vertexBufferTiles) {
		for (int i=0; i<m_numVertexBufferTiles; i++)
			REF_PTR_RELEASE(m_vertexBufferTiles[i]);
		delete[] m_vertexBufferTiles;
		m_vertexBufferTiles = nullptr;
	}

	delete[] m_vertexBufferBackup;
	m_vertexBufferBackup = nullptr;

	m_numVertexBufferTiles = 0;
}

//=============================================================================
// HeightMapRenderObjClass::freeMapResources
//=============================================================================
/** Frees the w3d resources used to draw the terrain. */
//=============================================================================
Int HeightMapRenderObjClass::freeMapResources()
{
	BaseHeightMapRenderObjClass::freeMapResources();
	freeIndexVertexBuffers();

	return 0;
}

//=============================================================================

DX8VertexBufferClass *HeightMapRenderObjClass::getVertexBufferTile(Int x, Int y)
{
	return m_vertexBufferTiles[y*m_numVBTilesX+x];
}

//=============================================================================
VERTEX_FORMAT *HeightMapRenderObjClass::getVertexBufferBackup(Int x, Int y)
{
	return m_vertexBufferBackup + y*m_numVBTilesX*HEIGHTMAP_VERTEX_NUM + x*HEIGHTMAP_VERTEX_NUM;
}

//=============================================================================
// HeightMapRenderObjClass::doTheDynamicLight
//=============================================================================
/** Calculates the diffuse lighting as affected by dynamic lighting. */
//=============================================================================
UnsignedInt HeightMapRenderObjClass::doTheDynamicLight(VERTEX_FORMAT *vb, VERTEX_FORMAT *vbMirror, Vector3*light, Vector3*normal,  W3DDynamicLight *pLights[], Int numLights)
{
#ifdef USE_NORMALS
	return;
#else
	Real shadeR, shadeG, shadeB;
	Int diffuse = vbMirror->diffuse;
#ifdef RTS_DEBUG
	//vbMirror->diffuse += 30;	// Shows which vertexes are geting touched by dynamic light. debug only.
#endif

	// (gth) avoiding the extra divides (compiler unfortunately didn't do this automatically...)
	const float oo255 = (1.0f/255.0f);
	shadeR = ((diffuse>>16)&0x00FF) * oo255;
	shadeG = ((diffuse>>8)&0x00FF) * oo255;
	shadeB = (diffuse&0x00FF) * oo255;

	Int alpha = (diffuse>>24)&0x00FF;
	Int k;
	for (k=0; k<numLights; k++) {
		W3DDynamicLight *pLight = pLights[k];
		if (!pLight->isEnabled()) {
			continue; // he is turned off.
		}
		Vector3 lightDirection(vbMirror->x, vbMirror->y, vbMirror->z);
		Real factor = 1.0f;
		switch(pLight->Get_Type()) {
		case LightClass::POINT:
		case LightClass::SPOT: {
				Vector3 lightLoc = pLight->Get_Position();
				lightDirection -= lightLoc;
				double range, midRange;
				pLight->Get_Far_Attenuation_Range(midRange, range);
				Real dist = lightDirection.Length();
				if (dist >= range) continue;
				if (midRange < 0.1) continue;
				factor = 1.0f - (dist - midRange) / (range - midRange);
				factor = WWMath::Clamp(factor,0.0f,1.0f);

				// (gth) normalize here since we have the length
				lightDirection /= dist;
			}
			break;
		case LightClass::DIRECTIONAL:
			pLight->Get_Spot_Direction(lightDirection);
			factor = 1.0;
			break;
		};

		// (gth) unneeded due to above normalization
		//lightDirection.Normalize();

		Vector3 lightRay(-lightDirection.X, -lightDirection.Y, -lightDirection.Z);
		Real shade = Vector3::Dot_Product(lightRay, *normal);
		shade *= factor;
		Vector3 diffuse;
		pLight->Get_Diffuse(&diffuse);
		Vector3 ambient;
		pLight->Get_Ambient(&ambient);
		if (shade > 1.0) shade = 1.0;
		if(shade < 0.0f) shade = 0.0f;
		shadeR += shade*diffuse.X;
		shadeG += shade*diffuse.Y;
		shadeB += shade*diffuse.Z;
		shadeR += factor*ambient.X;
		shadeG += factor*ambient.Y;
		shadeB += factor*ambient.Z;

	}
	if (shadeR > 1.0) shadeR = 1.0;
	if(shadeR < 0.0f) shadeR = 0.0f;
	if (shadeG > 1.0) shadeG = 1.0;
	if(shadeG < 0.0f) shadeG = 0.0f;
	if (shadeB > 1.0) shadeB = 1.0;
	if(shadeB < 0.0f) shadeB = 0.0f;
	shadeR*=255.0f;
	shadeG*=255.0f;
	shadeB*=255.0f;

//	(gth) faster float to int conversion, return the result so we can re-use it.
//	vb->diffuse=REAL_TO_INT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16) | ((int)alpha << 24);
	UnsignedInt light_val = WWMath::Float_To_Int_Chop(shadeB) | (WWMath::Float_To_Int_Chop(shadeG) << 8) | (WWMath::Float_To_Int_Chop(shadeR) << 16) | ((int)alpha << 24);
	vb->diffuse = light_val;
	return light_val;

#endif
}

//=============================================================================
// HeightMapRenderObjClass::getXWithOrigin
//=============================================================================
/** Gets the x index that corresponds to the data.  For example, if the columns
are shifted by 3, index 3 is actually the first row of polygons, or 0.  Yes it
is confusing, but it makes sliding the map 10x faster.  */
//=============================================================================
Int HeightMapRenderObjClass::getXWithOrigin(Int x)
{
	const Int xMax = m_x-1;
	x -= m_originX;
	if (x < 0) x += xMax;
	if (x >= xMax) x -= xMax;
	if (x < 0) { DEBUG_CRASH(("X out of range.")); x = 0; }
	if (x >= xMax) { DEBUG_CRASH(("X out of range.")); x = xMax; }
	return x;
}

//=============================================================================
// HeightMapRenderObjClass::getYWithOrigin
//=============================================================================
/** Gets the y index that corresponds to the data.  For example, if the rows
are shifted by 3, index 3 is actually the first row of polygons, or 0.  Yes it
is confusing, but it makes sliding the map 10x faster.  */
//=============================================================================
Int HeightMapRenderObjClass::getYWithOrigin(Int y)
{
	const Int yMax = m_y-1;
	y -= m_originY;
	if (y < 0) y += yMax;
	if (y >= yMax) y -= yMax;
	if (y < 0) { DEBUG_CRASH(("Y out of range.")); y = 0; }
	if (y >= yMax) { DEBUG_CRASH(("Y out of range.")); y = yMax; }
	return y;
}

//=============================================================================
// HeightMapRenderObjClass::updateVB
//=============================================================================
/** Update a rectangular block of the given Vertex Buffer.
data is expected to be an array same dimensions as current heightmap
mapped into this VB.
*/
//=============================================================================
Int HeightMapRenderObjClass::updateVB(DX8VertexBufferClass	*pVB, VERTEX_FORMAT *data, Int x0, Int y0, Int x1, Int y1, Int originX, Int originY, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator)
{
	Int i,j;
	Vector3 lightRay[MAX_GLOBAL_LIGHTS];
	Int xCoord, yCoord;
	Int vn0,un0,vp1,up1;
	Vector3 l2r,n2f,normalAtTexel;
	constexpr const Int vertsPerRow=(VERTEX_BUFFER_TILE_LENGTH)*4;	//vertices per row of VB
	constexpr const Int cellOffset = 1;

	REF_PTR_SET(m_map, pMap);	//update our heightmap pointer in case it changed since last call.
	if (m_vertexBufferTiles && pMap)
	{
#ifdef RTS_DEBUG
		assert(x0 >= originX && y0 >= originY && x1>x0 && y1>y0 && x1<=originX+VERTEX_BUFFER_TILE_LENGTH && y1<=originY+VERTEX_BUFFER_TILE_LENGTH);
#endif
		// GeneralsX @performance Mehmet Bat 01/09/2026 Hoist invariant terrain light-ray setup out of the terrain-cell loop.
		for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
		{
			const Coord3D *lightPos=&TheGlobalData->m_terrainLightPos[lightIndex];
			lightRay[lightIndex].Set(-lightPos->x,-lightPos->y, -lightPos->z);
		}

		DX8VertexBufferClass::WriteLockClass lockVtxBuffer(pVB);
		VERTEX_FORMAT *vbHardware = (VERTEX_FORMAT*)lockVtxBuffer.Get_Vertex_Array();
		VERTEX_FORMAT *vBase = data;
		// Note that we are building the vertex buffer data in the memory buffer, data.
		// At the bottom, we will copy the final vertex data for one cell into the
		// hardware vertex buffer.

		for (j=y0; j<y1; j++)
		{
			VERTEX_FORMAT *vb = vBase;
			vb += (j-originY)*vertsPerRow;	//skip to correct row in vertex buffer
			vb += (x0-originX)*4;		//skip to correct vertex in row.

			const Int mapY = getYWithOrigin(j);
			vn0 = mapY-cellOffset;
			if (vn0 < -pMap->getDrawOrgY())
				vn0=-pMap->getDrawOrgY();
			vp1 = getYWithOrigin(j+cellOffset)+cellOffset;
			if (vp1 >= pMap->getYExtent()-pMap->getDrawOrgY())
				vp1=pMap->getYExtent()-pMap->getDrawOrgY()-1;

			yCoord = mapY+pMap->getDrawOrgY();
			for (i=x0; i<x1; i++)
			{
				const Int mapX = getXWithOrigin(i);
				un0 = mapX-cellOffset;
				if (un0 < -pMap->getDrawOrgX())
					un0=-pMap->getDrawOrgX();
				up1 = getXWithOrigin(i+cellOffset)+cellOffset;
				if (up1 >= pMap->getXExtent()-pMap->getDrawOrgX())
					up1=pMap->getXExtent()-pMap->getDrawOrgX()-1;
				xCoord = mapX+pMap->getDrawOrgX();

				//update the 4 vertices in this block
				float U[4], V[4];
				UnsignedByte alpha[4];
				float UA[4], VA[4];
				Bool flipForBlend = false;			 // True if the blend needs the triangles flipped.

				pMap->getUVData(mapX, mapY, U, V);
				pMap->getAlphaUVData(mapX, mapY, UA, VA, alpha, &flipForBlend);

				//top-left sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX+cellOffset, mapY) - pMap->getDisplayHeight(un0, mapY)));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX, (mapY+cellOffset)) - pMap->getDisplayHeight(mapX, vn0)));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				vb->x=xCoord;
				vb->y=yCoord;
				vb->z=  ((float)pMap->getDisplayHeight(mapX, mapY))*MAP_HEIGHT_SCALE;
				vb->x = ADJUST_FROM_INDEX_TO_REAL(vb->x);
				vb->y = ADJUST_FROM_INDEX_TO_REAL(vb->y);
				vb->u1=U[0];
				vb->v1=V[0];
				vb->u2=UA[0];
				vb->v2=VA[0];
				doTheLight(vb, lightRay, &normalAtTexel, pLightsIterator, alpha[0]);
				vb++;

				//top-right sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(up1 , mapY ) - pMap->getDisplayHeight(mapX , mapY )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX+cellOffset , (mapY+cellOffset) ) - pMap->getDisplayHeight(mapX+cellOffset , vn0 )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				vb->x=xCoord+cellOffset;
				vb->y=yCoord;
				vb->z=  ((float)pMap->getDisplayHeight(mapX+cellOffset, mapY))*MAP_HEIGHT_SCALE;
				vb->x = ADJUST_FROM_INDEX_TO_REAL(vb->x);
				vb->y = ADJUST_FROM_INDEX_TO_REAL(vb->y);
				vb->u1=U[1];
				vb->v1=V[1];
				vb->u2=UA[1];
				vb->v2=VA[1];
				doTheLight(vb, lightRay, &normalAtTexel, pLightsIterator, alpha[1]);
				vb++;

				//bottom-right sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(up1 , (mapY+cellOffset) ) - pMap->getDisplayHeight(mapX , (mapY+cellOffset) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX+cellOffset , vp1 ) - pMap->getDisplayHeight(mapX+cellOffset , mapY )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				vb->x=xCoord+cellOffset;
				if (yCoord + 1 == pMap->getDrawOrgY() + m_y - 1) {
					vb->y=yCoord+1;
				} else {
					vb->y=yCoord+cellOffset;
				}
				vb->z=  ((float)pMap->getDisplayHeight(mapX+cellOffset, mapY+cellOffset))*MAP_HEIGHT_SCALE;
				vb->x = ADJUST_FROM_INDEX_TO_REAL(vb->x);
				vb->y = ADJUST_FROM_INDEX_TO_REAL(vb->y);
				vb->u1=U[2];
				vb->v1=V[2];
				vb->u2=UA[2];
				vb->v2=VA[2];
				doTheLight(vb, lightRay, &normalAtTexel, pLightsIterator, alpha[2]);
				vb++;

				//bottom-left sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX+cellOffset , (mapY+cellOffset) ) - pMap->getDisplayHeight(un0 , (mapY+cellOffset) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX , vp1 ) - pMap->getDisplayHeight(mapX , mapY )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				if (xCoord == pMap->getDrawOrgX()) {
					vb->x=xCoord;
					//if (vb->x < 0) vb->x = 0;
				} else {
					vb->x=xCoord;
				}
				if (yCoord + 1 == pMap->getDrawOrgY() + m_y - 1) {
					vb->y=yCoord+1;
				} else {
					vb->y=yCoord+cellOffset;
				}
				vb->z=  ((float)pMap->getDisplayHeight(mapX, mapY+cellOffset))*MAP_HEIGHT_SCALE;
				vb->x = ADJUST_FROM_INDEX_TO_REAL(vb->x);
				vb->y = ADJUST_FROM_INDEX_TO_REAL(vb->y);
				vb->u1=U[3];
				vb->v1=V[3];
				vb->u2=UA[3];
				vb->v2=VA[3];
				doTheLight(vb, lightRay, &normalAtTexel, pLightsIterator, alpha[3]);
				vb++;

				VERTEX_FORMAT *pCurVertices = vb-4;
#ifdef FLIP_TRIANGLES // jba - reduces "diamonding" in some cases, not others.  Better cliffs, though.
				VERTEX_FORMAT tmpVertex;
				if (flipForBlend) {
					tmpVertex = pCurVertices[0];
					pCurVertices[0] = pCurVertices[1];
					pCurVertices[1] = pCurVertices[2];
					pCurVertices[2] = pCurVertices[3];
					pCurVertices[3] = tmpVertex;
				}
#endif

				if (m_showImpassableAreas) {
					// Color impassable cells "red"
					DEBUG_ASSERTCRASH(PATHFIND_CELL_SIZE_F == MAP_XY_FACTOR, ("Pathfind must be terrain cell size, or this code needs reworking.  John A."));
					Real borderHiX = (pMap->getXExtent()-2*pMap->getBorderSizeInline())*MAP_XY_FACTOR;
					Real borderHiY = (pMap->getYExtent()-2*pMap->getBorderSizeInline())*MAP_XY_FACTOR;
					Bool border = pCurVertices[0].x == -MAP_XY_FACTOR || pCurVertices[0].y == -MAP_XY_FACTOR;
					Bool cliffMapped = pMap->isCliffMappedTexture(mapX, mapY);
					if (pCurVertices[0].x == borderHiX) {
						border = true;
					}
					if (pCurVertices[0].y == borderHiY) {
						border = true;
					}
					Bool isCliff = pMap->getCliffState(xCoord, yCoord) || showAsVisibleCliff(xCoord, yCoord);

					if ( isCliff || border || cliffMapped) {
						Int cellX, cellY;
						for (cellX=0; cellX<2; cellX++) {
							for (cellY=0; cellY<2; cellY++) {
								Int vertex = cellX+2*cellY;
								if (border) {
									Bool doBorder = false;
									if (pCurVertices[vertex].y >= 0 && pCurVertices[vertex].y <= borderHiY) {
										if (pCurVertices[vertex].x == 0 || pCurVertices[vertex].x == borderHiX) {
											doBorder = true;
										}
									}
									if (pCurVertices[vertex].x >= 0 && pCurVertices[vertex].x <= borderHiX) {
										if (pCurVertices[vertex].y == 0 || pCurVertices[vertex].y == borderHiY) {
											doBorder = true;
										}
									}
									if (doBorder) {
										pCurVertices[vertex].diffuse &= 0xFF0000ff; // blue with alpha.
									}
								} else if (isCliff) {
									pCurVertices[vertex].diffuse &= 0xFFFF0000; // red with alpha.
								}
								if (cliffMapped && vertex==0) {
									pCurVertices[vertex].diffuse &= 0xFF000000; // Black.
									pCurVertices[vertex].diffuse |= 0xff00; // Add green.
								}
							}
						}
					}
				}

				// Note - We have been building the vertex buffer in the memory location.
				// Now copy the set of vertices into the hardware buffer.
				// We don't copy the whole vertex buffer because we often update only
				// a couple of rows and its a lot faster to just copy the ones that change.
				Int offset = pCurVertices - vBase;
				memcpy(vbHardware+offset, pCurVertices, 4*sizeof(VERTEX_FORMAT));
			}
		}
		return 0; //success.
	}
	return -1;
}

//=============================================================================
// HeightMapRenderObjClass::updateVBForLight
//=============================================================================
/** Update the dynamic lighting values only in a rectangular block of the given Vertex Buffer.
The vertex locations and texture coords are unchanged.
*/
Int HeightMapRenderObjClass::updateVBForLight(DX8VertexBufferClass	*pVB, VERTEX_FORMAT *data, Int x0, Int y0, Int x1, Int y1, Int originX, Int originY, W3DDynamicLight *pLights[], Int numLights)
{

#if (OPTIMIZED_HEIGHTMAP_LIGHTING)	// (gth) if optimizations are enabled, jump over to the "optimized" version of this function.
	return updateVBForLightOptimized( pVB, data, x0, y0, x1, y1, originX, originY, pLights, numLights );
#endif

	Int i,j,k;
	Int vn0,un0,vp1,up1;
	Vector3 l2r,n2f,normalAtTexel;
	constexpr const Int	vertsPerRow=(VERTEX_BUFFER_TILE_LENGTH)*4;	//vertices per row of VB

	if (m_vertexBufferTiles && m_map)
	{
#ifdef RTS_DEBUG
		assert(x0 >= originX && y0 >= originY && x1>x0 && y1>y0 && x1<=originX+VERTEX_BUFFER_TILE_LENGTH && y1<=originY+VERTEX_BUFFER_TILE_LENGTH);
#endif

		DX8VertexBufferClass::WriteLockClass lockVtxBuffer(pVB);
		VERTEX_FORMAT *vBase = (VERTEX_FORMAT*)lockVtxBuffer.Get_Vertex_Array();
		VERTEX_FORMAT *vb;

		for (j=y0; j<y1; j++)
		{
			const Int mapY = getYWithOrigin(j);
			const Int yCoord = mapY+m_map->getDrawOrgY()-m_map->getBorderSizeInline();
			Bool intersect = false;
			for (k=0; k<numLights; k++) {
				if (pLights[k]->m_minY <= yCoord+1 &&
					pLights[k]->m_maxY >= yCoord) {
					intersect = true;
				}
				if (pLights[k]->m_prevMinY <= yCoord+1 &&
					pLights[k]->m_prevMaxY >= yCoord) {
					intersect = true;
				}
			}
			if (!intersect) {
				continue;
			}
			vn0 = mapY-1;
			if (vn0 < -m_map->getDrawOrgY())
				vn0=-m_map->getDrawOrgY();
			vp1 = getYWithOrigin(j+1)+1;
			if (vp1 >= m_map->getYExtent()-m_map->getDrawOrgY())
				vp1=m_map->getYExtent()-m_map->getDrawOrgY()-1;

			for (i=x0; i<x1; i++)
			{
				const Int mapX = getXWithOrigin(i);
				const Int xCoord = mapX+m_map->getDrawOrgX()-m_map->getBorderSizeInline();
				Bool intersect = false;
				for (k=0; k<numLights; k++) {
					if (pLights[k]->m_minX <= xCoord+1 &&
						pLights[k]->m_maxX >= xCoord &&
						pLights[k]->m_minY <= yCoord+1 &&
						pLights[k]->m_maxY >= yCoord) {
						intersect = true;
					}
					if (pLights[k]->m_prevMinX <= xCoord+1 &&
						pLights[k]->m_prevMaxX >= xCoord &&
						pLights[k]->m_prevMinY <= yCoord+1 &&
						pLights[k]->m_prevMaxY >= yCoord) {
						intersect = true;
					}
				}
				if (!intersect) {
					continue;
				}
				// vb is the pointer to the vertex in the hardware dx8 vertex buffer.
				Int offset = (j-originY)*vertsPerRow+4*(i-originX);
				vb = vBase + offset;	//skip to correct row in vertex buffer
				// vbMirror is the pointer to the vertex in our memory based copy.
				// The important point is that we can read out of our copy to get the original
				// diffuse color, and xyz location.  It is VERY SLOW to read out of the
				// hardware vertex buffer, possibly worse... jba.
				VERTEX_FORMAT *vbMirror = data + offset;
				un0 = mapX-1;
				if (un0 < -m_map->getDrawOrgX())
					un0=-m_map->getDrawOrgX();
				up1 = getXWithOrigin(i+1)+1;
				if (up1 >= m_map->getXExtent()-m_map->getDrawOrgX())
					up1=m_map->getXExtent()-m_map->getDrawOrgX()-1;

				Vector3 lightRay(0,0,0);

				//top-left sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1, mapY) - m_map->getDisplayHeight(un0, mapY)));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX, (mapY+1)) - m_map->getDisplayHeight(mapX, vn0)));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				vb++;	vbMirror++;

				//top-right sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(up1 , mapY ) - m_map->getDisplayHeight(mapX , mapY )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , (mapY+1) ) - m_map->getDisplayHeight(mapX+1 , vn0 )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				vb++;	vbMirror++;

				//bottom-right sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(up1 , (mapY+1) ) - m_map->getDisplayHeight(mapX , (mapY+1) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , vp1 ) - m_map->getDisplayHeight(mapX+1 , mapY )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				vb++;	vbMirror++;

				//bottom-left sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , (mapY+1) ) - m_map->getDisplayHeight(un0 , (mapY+1) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX , vp1 ) - m_map->getDisplayHeight(mapX , mapY )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				vb++;	vbMirror++;
			}
		}
		return 0; //success.
	}
	return -1;
}


Int HeightMapRenderObjClass::updateVBForLightOptimized(DX8VertexBufferClass	*pVB, VERTEX_FORMAT *data, Int x0, Int y0, Int x1, Int y1, Int originX, Int originY, W3DDynamicLight *pLights[], Int numLights)
{
	Int i,j,k;
	Int vn0,un0,vp1,up1;
	Vector3 l2r,n2f,normalAtTexel;
	constexpr const Int vertsPerRow=(VERTEX_BUFFER_TILE_LENGTH)*4;	//vertices per row of VB

	if (m_vertexBufferTiles && m_map)
	{
#ifdef RTS_DEBUG
		assert(x0 >= originX && y0 >= originY && x1>x0 && y1>y0 && x1<=originX+VERTEX_BUFFER_TILE_LENGTH && y1<=originY+VERTEX_BUFFER_TILE_LENGTH);
#endif

		DX8VertexBufferClass::WriteLockClass lockVtxBuffer(pVB);
		VERTEX_FORMAT *vBase = (VERTEX_FORMAT*)lockVtxBuffer.Get_Vertex_Array();
		VERTEX_FORMAT *vb;

		//
		// (gth) the optimization in this function is to take advantage of verts in the same
		// x,y position who have already computed their lighting.  To do this, we need to set up
		// some offsets in the vertex buffer.  I've computed these offsets to be consistent with
		// the formula's that Generals is using but in the case of the "half-res-mesh" I'm not
		// sure things are correct...
		//
		constexpr const Int quad_right_offset = 4;
		constexpr const Int quad_below_offset = vertsPerRow;
		//constexpr const Int quad_below_right_offset = vertsPerRow + 4;

		//
		// i,j loop over the quads affected by the light.  Each quad has its *own* 4 vertices.  This
		// means that for any vertex position on the map, there are actually 4 copies of the vertex.
		//
		for (j=y0; j<y1; j++)
		{
			const Int mapY = getYWithOrigin(j);
			const Int yCoord = mapY+m_map->getDrawOrgY()-m_map->getBorderSizeInline();
			Bool intersect = false;
			for (k=0; k<numLights; k++) {
				if (pLights[k]->m_minY <= yCoord+1 &&
					pLights[k]->m_maxY >= yCoord) {
					intersect = true;
				}
				if (pLights[k]->m_prevMinY <= yCoord+1 &&
					pLights[k]->m_prevMaxY >= yCoord) {
					intersect = true;
				}
			}
			if (!intersect) {
				continue;
			}
			vn0 = mapY-1;
			if (vn0 < -m_map->getDrawOrgY())
				vn0=-m_map->getDrawOrgY();
			vp1 = getYWithOrigin(j+1)+1;
			if (vp1 >= m_map->getYExtent()-m_map->getDrawOrgY())
				vp1=m_map->getYExtent()-m_map->getDrawOrgY()-1;

			for (i=x0; i<x1; i++)
			{
				const Int mapX = getXWithOrigin(i);
				const Int xCoord = mapX+m_map->getDrawOrgX()-m_map->getBorderSizeInline();
				Bool intersect = false;
				for (k=0; k<numLights; k++) {
					if (pLights[k]->m_minX <= xCoord+1 &&
						pLights[k]->m_maxX >= xCoord &&
						pLights[k]->m_minY <= yCoord+1 &&
						pLights[k]->m_maxY >= yCoord) {
						intersect = true;
					}
					if (pLights[k]->m_prevMinX <= xCoord+1 &&
						pLights[k]->m_prevMaxX >= xCoord &&
						pLights[k]->m_prevMinY <= yCoord+1 &&
						pLights[k]->m_prevMaxY >= yCoord) {
						intersect = true;
					}
				}
				if (!intersect) {
					continue;
				}
				// vb is the pointer to the vertex in the hardware dx8 vertex buffer.
				Int offset = (j-originY)*vertsPerRow+4*(i-originX);
				vb = vBase + offset;	//skip to correct row in vertex buffer
				// vbMirror is the pointer to the vertex in our memory based copy.
				// The important point is that we can read out of our copy to get the original
				// diffuse color, and xyz location.  It is VERY SLOW to read out of the
				// hardware vertex buffer, possibly worse... jba.
				VERTEX_FORMAT *vbMirror = data + offset;
				VERTEX_FORMAT *vbaseMirror = data;
				un0 = mapX-1;
				if (un0 < -m_map->getDrawOrgX())
					un0=-m_map->getDrawOrgX();
				up1 = getXWithOrigin(i+1)+1;
				if (up1 >= m_map->getXExtent()-m_map->getDrawOrgX())
					up1=m_map->getXExtent()-m_map->getDrawOrgX()-1;

				Vector3 lightRay(0,0,0);

				//
				// (gth) Following the set of rules below lets us take advantage of lighting values that have
				// been previously computed.  The idea is to copy them ahead to future quads that will need them
				// and then not compute them when we get to those quads.  This also avoids having to read-back
				// from the vertex buffer but we do jump around in memory... probably bad anyway, maybe we should
				// compute into a temporary buffer and copy all at once...
				//
				unsigned long light_copy;

				// top-left sample -> only compute when i==0 and j==0
				if ((i==x0) && (j==y0)) {
					l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1, mapY) - m_map->getDisplayHeight(un0, mapY)));
					n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX, (mapY+1)) - m_map->getDisplayHeight(mapX, vn0)));
					Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
					doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				}
				vb++;	vbMirror++;

				//top-right sample -> compute when j==0, then copy to (right,0)
				if (j==y0) {
					l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(up1 , mapY ) - m_map->getDisplayHeight(mapX , mapY )));
					n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , (mapY+1) ) - m_map->getDisplayHeight(mapX+1 , vn0 )));
					Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
					light_copy = doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);

					if (i < x1-1) {
						// copy light to (right,0)
						(vBase + offset + quad_right_offset)->diffuse = (light_copy&0x00FFFFFF) | ((vbaseMirror + offset + quad_right_offset)->diffuse&0xff000000) ;
					}
				}
				vb++;	vbMirror++;

				//bottom-right sample -> always compute, then copy to (right,3), (down,1), (down+right,0)
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(up1 , (mapY+1) ) - m_map->getDisplayHeight(mapX , (mapY+1) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , vp1 ) - m_map->getDisplayHeight(mapX+1 , mapY )));
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
				light_copy = doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);

				if (i < x1-1) {
					// copy light to (right,3)
					//(vBase + offset + quad_right_offset + 3)->diffuse = light_copy;
					(vBase + offset + quad_right_offset + 3)->diffuse = (light_copy&0x00FFFFFF) | ((vbaseMirror + offset + quad_right_offset + 3)->diffuse&0xff000000) ;
				}
				if (j < y1-1) {
					// copy light to (down,1)
					//(vBase + offset + quad_below_offset + 1)->diffuse = light_copy;
					(vBase + offset + quad_right_offset + 1)->diffuse = (light_copy&0x00FFFFFF) | ((vbaseMirror + offset + quad_right_offset + 1)->diffuse&0xff000000) ;
				}
				if ((i < x1-1) && (j < y1-1)) {
					// copy light to (right+down,0)
					//(vBase + offset + quad_below_right_offset)->diffuse = light_copy;
					(vBase + offset + quad_right_offset)->diffuse = (light_copy&0x00FFFFFF) | ((vbaseMirror + offset + quad_right_offset)->diffuse&0xff000000) ;
				}
				vb++;	vbMirror++;

				//bottom-left sample -> compute when i==0, otherwise copy from (left,2)
				if (i==x0) {
					l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , (mapY+1) ) - m_map->getDisplayHeight(un0 , (mapY+1) )));
					n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX , vp1 ) - m_map->getDisplayHeight(mapX , mapY )));
					Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
					light_copy = doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);

					if (j < y1-1) {
						// copy light to (down,0)
						//(vBase + offset + quad_below_offset)->diffuse = light_copy;
						(vBase + offset + quad_below_offset)->diffuse = (light_copy&0x00FFFFFF) | ((vbaseMirror + offset + quad_below_offset)->diffuse&0xff000000) ;
					}
				}
				vb++;	vbMirror++;
			}
		}
		return 0; //success.
	}
	return -1;
}



//=============================================================================
// HeightMapRenderObjClass::doPartialUpdate
//=============================================================================
/** Updates a partial block of vertices from [x0,y0 to x1,y1]
The coordinates in partialRange are map cell coordinates, relative to the entire map.
The vertex coordinates and texture coordinates, as well as static lighting are updated.
*/
void HeightMapRenderObjClass::doPartialUpdate(const IRegion2D &partialRange, WorldHeightMap *htMap, RefRenderObjListIterator *pLightsIterator)
{
	// Adjust range into the current drawn map range.
	Int minX = partialRange.lo.x - htMap->getDrawOrgX();
	Int maxX = partialRange.hi.x - htMap->getDrawOrgX();
	Int minY = partialRange.lo.y - htMap->getDrawOrgY();
	Int maxY = partialRange.hi.y - htMap->getDrawOrgY();
	if (minX<0) minX = 0;
	if (minY<0) minY = 0;
	if (maxX > m_x-1) maxX = m_x-1;
	if (maxY > m_y-1) maxY = m_y-1;
	if (maxX < minX) return;
	if (maxY < minY) return;
	if (m_originX == 0 && m_originY == 0) {
		// simple case.
		updateBlock(minX, minY, maxX, maxY,
								htMap, pLightsIterator);
	}
	else
	{
		minY = minY+m_originY;
		maxY = maxY+m_originY;

		if (minY> m_y-1) {
			minY -= m_y-1;
 			maxY -= m_y-1;
		}
		if (maxY > m_y-1) {
			maxY -= m_y-1;
			updateBlock(0, minY, m_x-1, m_y-1, htMap, pLightsIterator);
			updateBlock(0, 0, m_x-1, maxY, htMap, pLightsIterator);
		} else {
			updateBlock(0, minY, m_x-1, maxY, htMap, pLightsIterator);
		}
	}

	if (!m_extraBlendTilePositions)
	{	//Need to allocate memory
		m_extraBlendTilePositions = NEW Int[DEFAULT_MAX_MAP_EXTRABLEND_TILES];
		m_extraBlendTilePositionsSize = DEFAULT_MAX_MAP_EXTRABLEND_TILES;
	}

	//Find list of all extra blend tiles used on map.  These are tiles with 3 materials/textures
	//over the same tile and require an extra render pass.

	Int i, j;
	//First remove any existing extra blend tiles within this partial region
	for (j=0; j<m_numExtraBlendTiles; j++)
	{	Int x = m_extraBlendTilePositions[j] & 0xffff;
		Int y = m_extraBlendTilePositions[j] >> 16;
		if (x >= partialRange.lo.x && x < partialRange.hi.x &&
			y >= partialRange.lo.y && y < partialRange.hi.y)
		{	//this tile is inside region being updated so remove it by shifting tile array
			memcpy(m_extraBlendTilePositions+j,m_extraBlendTilePositions+j+1,(m_numExtraBlendTiles-1-j)*sizeof(Int));
			m_numExtraBlendTiles--;
			j--;	//need to look at index j again because this tile was removed
		}
	}

	for (j=partialRange.lo.y; j<partialRange.hi.y; j++)
		for (i=partialRange.lo.x; i<partialRange.hi.x; i++)
		{
			if (j<0 || i<0) continue;
			Real U[4],V[4];
			UnsignedByte alpha[4];
			Bool flipState,cliffState;
			if (htMap->getExtraAlphaUVData(i,j,U,V,alpha,&flipState, &cliffState))
			{	if (m_numExtraBlendTiles >= m_extraBlendTilePositionsSize)
				{	//no more room to store extra blend tiles so enlarge the buffer.
					Int *tempPositions=NEW Int[m_extraBlendTilePositionsSize+512];
					memcpy(tempPositions, m_extraBlendTilePositions, m_extraBlendTilePositionsSize*sizeof(Int));
					delete [] m_extraBlendTilePositions;
					//enlarge by more tiles to reduce memory trashing
					m_extraBlendTilePositions = tempPositions;
					m_extraBlendTilePositionsSize += 512;
				}
				//Pack x and y position into single integer since maps are limited in size
				m_extraBlendTilePositions[m_numExtraBlendTiles]=i | (j <<16);
				m_numExtraBlendTiles++;
			}
		}
	updateShorelineTiles(partialRange.lo.x,partialRange.lo.y,partialRange.hi.x,partialRange.hi.y,htMap);

	updateViewImpassableAreas(TRUE, minX, maxX, minY, maxY);
}

//=============================================================================
// HeightMapRenderObjClass::updateBlock
//=============================================================================
/** Updates a block of vertices from [x0,y0 to x1,y1]
The vertex coordinates and texture coordinates, as well as static lighting are updated.
*/
Int HeightMapRenderObjClass::updateBlock(Int x0, Int y0, Int x1, Int y1,  WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator)
{
#ifdef RTS_DEBUG
	DEBUG_ASSERTCRASH(x0>=0,  ("HeightMapRenderObjClass::UpdateBlock parameters extend beyond left edge."));
	DEBUG_ASSERTCRASH(y0>=0,  ("HeightMapRenderObjClass::UpdateBlock parameters extend beyond bottom edge."));
	DEBUG_ASSERTCRASH(x1<m_x, ("HeightMapRenderObjClass::UpdateBlock parameters extend beyond right edge."));
	DEBUG_ASSERTCRASH(y1<m_y, ("HeightMapRenderObjClass::UpdateBlock parameters extend beyond top edge."));
	DEBUG_ASSERTCRASH(x0<=x1, ("HeightMapRenderObjClass::UpdateBlock parameters have inside-out rectangle (on X)."));
	DEBUG_ASSERTCRASH(y0<=y1, ("HeightMapRenderObjClass::UpdateBlock parameters have inside-out rectangle (on Y)."));
#endif
	Invalidate_Cached_Bounding_Volumes();
	if (pMap && m_treeBuffer != nullptr) {
		REF_PTR_SET(m_stageZeroTexture, pMap->getTerrainTexture());
		REF_PTR_SET(m_stageOneTexture, pMap->getAlphaTerrainTexture());
	}

	Int i,j;
	Int originX,originY;
	//step through each vertex buffer that needs updating
	for (j=0; j<m_numVBTilesY; j++)
	{
		originY=j*VERTEX_BUFFER_TILE_LENGTH;	//location of this VB on the large full-size heightmap
		Int yMin, yMax;
		yMin = originY;
		if (y0>yMin) yMin = y0;
		yMax = originY+VERTEX_BUFFER_TILE_LENGTH;
		if (y1<yMax) yMax = y1;
		if (yMin >= yMax) {
			continue;
		}
		for (i=0; i<m_numVBTilesX; i++)
		{
			originX=i*VERTEX_BUFFER_TILE_LENGTH;	//location of this VB on the large full-size heightmap
			Int xMin, xMax;
			xMin = originX;
			if (xMin<x0) xMin = x0;
			xMax = originX+VERTEX_BUFFER_TILE_LENGTH;
			if (xMax>x1) xMax = x1;
			if (xMin >= xMax) {
				continue;
			}
			DX8VertexBufferClass *pVB = getVertexBufferTile(i, j);
			VERTEX_FORMAT *pData = getVertexBufferBackup(i, j);
			updateVB(pVB, pData, xMin, yMin, xMax, yMax, originX, originY, pMap, pLightsIterator);
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// HeightMapRenderObjClass::~HeightMapRenderObjClass
//=============================================================================
/** Destructor. Releases w3d assets. */
//=============================================================================
HeightMapRenderObjClass::~HeightMapRenderObjClass()
{
	freeMapResources();

	delete [] m_extraBlendTilePositions;
	m_extraBlendTilePositions = nullptr;
}

//=============================================================================
// HeightMapRenderObjClass::HeightMapRenderObjClass
//=============================================================================
/** Constructor. Mostly nulls out the member variables. */
//=============================================================================
HeightMapRenderObjClass::HeightMapRenderObjClass():
m_extraBlendTilePositions(nullptr),
m_numExtraBlendTiles(0),
m_numVisibleExtraBlendTiles(0),
m_extraBlendTilePositionsSize(0),
m_vertexBufferTiles(nullptr),
m_vertexBufferBackup(nullptr),
m_originX(0),
m_originY(0),
m_oversizeDrawWidth(0),
m_oversizeDrawHeight(0),
m_indexBuffer(nullptr),
m_numVBTilesX(0),
m_numVBTilesY(0),
m_numVertexBufferTiles(0),
m_numBlockColumnsInLastVB(0),
m_numBlockRowsInLastVB(0)
{
	TheHeightMap = this;
}


//=============================================================================
// HeightMapRenderObjClass::adjustTerrainLOD
//=============================================================================
/** Adjust the terrain Level Of Detail.  If adj > 0 , increases LOD 1 step, if
adj < 0 decreases it one step, if adj==0, then just sets up for the current LOD */
//=============================================================================
void HeightMapRenderObjClass::adjustTerrainLOD(Int adj)
{
	BaseHeightMapRenderObjClass::adjustTerrainLOD(adj);

	return;

#if 0
	if (adj>0 && TheGlobalData->m_terrainLOD<TERRAIN_LOD_MAX) TheWritableGlobalData->m_terrainLOD=(TerrainLOD)(TheGlobalData->m_terrainLOD+1);
	if (adj<0 && TheGlobalData->m_terrainLOD>TERRAIN_LOD_MIN) TheWritableGlobalData->m_terrainLOD=(TerrainLOD)(TheGlobalData->m_terrainLOD-1);

	switch (TheGlobalData->m_terrainLOD) {
		case	TERRAIN_LOD_MIN: TheWritableGlobalData->m_useCloudMap = false;
									TheWritableGlobalData->m_useLightMap = false ;
									TheWritableGlobalData->m_useWaterPlane = false;
									TheWritableGlobalData->m_stretchTerrain = false;
									TheWritableGlobalData->m_useHalfHeightMap = true;
									break;
		case TERRAIN_LOD_HALF_CLOUDS: TheWritableGlobalData->m_useCloudMap = true;
									TheWritableGlobalData->m_useLightMap = true;
									TheWritableGlobalData->m_useWaterPlane = false;
									TheWritableGlobalData->m_stretchTerrain = false;
									TheWritableGlobalData->m_useHalfHeightMap = true;
									break;
		case TERRAIN_LOD_STRETCH_NO_CLOUDS: TheWritableGlobalData->m_useCloudMap = false;
									TheWritableGlobalData->m_useLightMap = false;
									TheWritableGlobalData->m_useWaterPlane = false;
									TheWritableGlobalData->m_stretchTerrain = true;
									TheWritableGlobalData->m_useHalfHeightMap = false;
									break;
		case TERRAIN_LOD_STRETCH_CLOUDS: TheWritableGlobalData->m_useCloudMap = true;
									TheWritableGlobalData->m_useLightMap = true;
									TheWritableGlobalData->m_useWaterPlane = false;
									TheWritableGlobalData->m_stretchTerrain = true;
									TheWritableGlobalData->m_useHalfHeightMap = false;
									break;
		case TERRAIN_LOD_NO_CLOUDS: TheWritableGlobalData->m_useCloudMap = false;
									TheWritableGlobalData->m_useLightMap = false;
									TheWritableGlobalData->m_useWaterPlane = false;
									TheWritableGlobalData->m_stretchTerrain = false;
									TheWritableGlobalData->m_useHalfHeightMap = false;
									break;
		default:
		case TERRAIN_LOD_NO_WATER: TheWritableGlobalData->m_useCloudMap = true;
									TheWritableGlobalData->m_useLightMap = true;
									TheWritableGlobalData->m_useWaterPlane = false;
									TheWritableGlobalData->m_stretchTerrain = false;
									TheWritableGlobalData->m_useHalfHeightMap = false;
									break;
		case TERRAIN_LOD_MAX: TheWritableGlobalData->m_useCloudMap = true;
									TheWritableGlobalData->m_useLightMap = true;
									TheWritableGlobalData->m_useWaterPlane = true;
									TheWritableGlobalData->m_stretchTerrain = false;
									TheWritableGlobalData->m_useHalfHeightMap = false;
									break;
	}
	if (m_map==nullptr) return;
	m_map->setDrawOrg(m_map->getDrawOrgX(), m_map->getDrawOrgX());
	if (m_shroud)
		m_shroud->reset();	//need reset here since initHeightData will load new shroud.
	this->initHeightData(m_map->getDrawWidth(),
											m_map->getDrawHeight(), m_map, nullptr);
	staticLightingChanged();
	if (TheTacticalView) {
		TheTacticalView->forceRedraw();
	}
#endif
}

//=============================================================================
// HeightMapRenderObjClass::ReleaseResources
//=============================================================================
/** Releases all w3d assets, to prepare for Reset device call. */
//=============================================================================
void HeightMapRenderObjClass::ReleaseResources()
{
	BaseHeightMapRenderObjClass::ReleaseResources();
}

//=============================================================================
// HeightMapRenderObjClass::ReAcquireResources
//=============================================================================
/** Reallocates all W3D assets after a reset.. */
//=============================================================================
void HeightMapRenderObjClass::ReAcquireResources()
{
	BaseHeightMapRenderObjClass::ReAcquireResources();
}

//=============================================================================
// HeightMapRenderObjClass::reset
//=============================================================================
/** Updates the macro noise/lightmap texture (pass 3) */
//=============================================================================
void HeightMapRenderObjClass::reset()
{
	BaseHeightMapRenderObjClass::reset();
	m_oversizeDrawWidth = 0;
	m_oversizeDrawHeight = 0;
}

//=============================================================================
// HeightMapRenderObjClass::oversizeTerrain
//=============================================================================
/** Sets the terrain oversize amount. */
//=============================================================================
void HeightMapRenderObjClass::oversizeTerrain(Int tilesToOversize)
{
	if (tilesToOversize>0)
	{
		m_oversizeDrawWidth = WorldHeightMap::NORMAL_DRAW_WIDTH + VERTEX_BUFFER_TILE_LENGTH * tilesToOversize;
		m_oversizeDrawHeight = WorldHeightMap::NORMAL_DRAW_HEIGHT + VERTEX_BUFFER_TILE_LENGTH * tilesToOversize;
		m_oversizeDrawWidth = std::min(m_oversizeDrawWidth, m_map->getXExtent());
		m_oversizeDrawHeight = std::min(m_oversizeDrawHeight, m_map->getYExtent());
		setTerrainDrawSize(m_oversizeDrawWidth, m_oversizeDrawHeight);
	}
	else
	{
		m_oversizeDrawWidth = 0;
		m_oversizeDrawHeight = 0;
		Int width = std::min((Int)WorldHeightMap::NORMAL_DRAW_WIDTH, m_map->getXExtent());
		Int height = std::min((Int)WorldHeightMap::NORMAL_DRAW_HEIGHT, m_map->getYExtent());
		setTerrainDrawSize(width, height);
	}
}

void HeightMapRenderObjClass::setTerrainDrawSize(Int width, Int height)
{
	if (m_map == nullptr)
		return;

	if (m_oversizeDrawWidth != 0)
		width = m_oversizeDrawWidth;
	else
		width = std::min(width, m_map->getXExtent());

	if (m_oversizeDrawHeight != 0)
		height = m_oversizeDrawHeight;
	else
		height = std::min(height, m_map->getYExtent());

	if (width == m_map->getDrawWidth() && height == m_map->getDrawHeight())
		return;

	Int dx = width-m_map->getDrawWidth();
	Int dy = height-m_map->getDrawHeight();
	m_map->setDrawWidth(width);
	m_map->setDrawHeight(height);
	dx /= 2;
	dy /= 2;
	Int newOrgX = m_map->getDrawOrgX()-dx;
	Int newOrgY = m_map->getDrawOrgY()-dy;
	m_map->setDrawOrg(newOrgX,newOrgY);
	m_originX = 0;
	m_originY = 0;
	if (m_shroud)
		m_shroud->reset();
	//delete m_shroud;
	//m_shroud = nullptr;
	initHeightData(m_map->getDrawWidth(), m_map->getDrawHeight(), m_map, nullptr, FALSE);
	m_needFullUpdate = true;
}




//=============================================================================
// HeightMapRenderObjClass::initHeightData
//=============================================================================
/** Allocate a heightmap of x by y vertices and fill with initial height values.
Also allocates all rendering resources such as vertex buffers, index buffers,
shaders, and materials.*/
//=============================================================================
Int HeightMapRenderObjClass::initHeightData(Int x, Int y, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator, Bool updateExtraPassTiles)
{
	BaseHeightMapRenderObjClass::initHeightData(x, y, pMap, pLightsIterator, updateExtraPassTiles);
	Int i,j;
//	Int	vertsPerRow=x*2-2;
//	Int	vertsPerColumn=y*2-2;

	HeightSampleType *data = nullptr;
	if (pMap) {
		data = pMap->getDataPtr();
	}

	if (updateExtraPassTiles)
	{
		m_numExtraBlendTiles = 0;
		//Do some preprocessing on map to extract useful data
		if (pMap)
		{
			Int m_mapDX=pMap->getXExtent();
			Int m_mapDY=pMap->getYExtent();
			if (!m_extraBlendTilePositions)
			{	//Need to allocate memory
				m_extraBlendTilePositions = NEW Int[DEFAULT_MAX_MAP_EXTRABLEND_TILES];
				m_extraBlendTilePositionsSize = DEFAULT_MAX_MAP_EXTRABLEND_TILES;
			}

			//Find list of all extra blend tiles used on map.  These are tiles with 3 materials/textures
			//over the same tile and require an extra render pass.
			for (j=0; j<(m_mapDY-1); j++)
				for (i=0; i<(m_mapDX-1); i++)
				{
					Real U[4],V[4];
					UnsignedByte alpha[4];
					Bool flipState,cliffState;
					if (pMap->getExtraAlphaUVData(i,j,U,V,alpha,&flipState, &cliffState))
					{	if (m_numExtraBlendTiles >= m_extraBlendTilePositionsSize)
						{	//no more room to store extra blend tiles so enlarge the buffer.
							Int *tempPositions=NEW Int[m_extraBlendTilePositionsSize+512];
							memcpy(tempPositions, m_extraBlendTilePositions, m_extraBlendTilePositionsSize*sizeof(Int));
							delete [] m_extraBlendTilePositions;
							//enlarge by more tiles to reduce memory trashing
							m_extraBlendTilePositions = tempPositions;
							m_extraBlendTilePositionsSize += 512;
						}
						//Pack x and y position into single integer since maps are limited in size
						m_extraBlendTilePositions[m_numExtraBlendTiles]=i | (j <<16);
						m_numExtraBlendTiles++;
					}
				}
		}
	}

	m_originX = 0;
	m_originY = 0;
	m_needFullUpdate = true;

	// If the size changed, we need to allocate.
	Bool needToAllocate = (x != m_x || y != m_y);
	// If the textures aren't allocated (usually because of a hardware reset) need to allocate.
	if (m_stageOneTexture == nullptr) {
		needToAllocate = true;
	}
	if (data && needToAllocate && m_treeBuffer != nullptr)
	{	//requested heightmap different from old one.
		freeIndexVertexBuffers();
		//Create static index buffers.  These will index the vertex buffers holding the map.
		m_indexBuffer=NEW_REF(DX8IndexBufferClass,(VERTEX_BUFFER_TILE_LENGTH*VERTEX_BUFFER_TILE_LENGTH*2*3));

		// Fill up the IB
		DX8IndexBufferClass::WriteLockClass lockIdxBuffer(m_indexBuffer);
		UnsignedShort *ib=lockIdxBuffer.Get_Index_Array();

		for (j=0; j<(VERTEX_BUFFER_TILE_LENGTH*VERTEX_BUFFER_TILE_LENGTH*4); j+=VERTEX_BUFFER_TILE_LENGTH*4)
		{
			for (i=j; i<(j+VERTEX_BUFFER_TILE_LENGTH*4); i+=4)	//4 vertices per 2x2 block
			{
				ib[0]=i;
				ib[1]=i+2;
				ib[2]=i+3;

				ib[3]=i;
				ib[4]=i+1;
				ib[5]=i+2;

				ib+=6;	//skip the 6 indices we just filled
			}
		}

		//Get number of vertex buffers needed to hold current map
		//First round dimensions to next multiple of VERTEX_BUFFER_TILE_LENGTH since that's our block size
		m_numVBTilesX=1;
		for (i=VERTEX_BUFFER_TILE_LENGTH+1; i<x;)
		{	i+=VERTEX_BUFFER_TILE_LENGTH;
			m_numVBTilesX++;
		}
		m_numVBTilesY=1;
		for (j=VERTEX_BUFFER_TILE_LENGTH+1; j<y;)
		{	j+=VERTEX_BUFFER_TILE_LENGTH;
			m_numVBTilesY++;
		}

		m_numBlockColumnsInLastVB=(x-1)%VERTEX_BUFFER_TILE_LENGTH;	//right border within last VB
		m_numBlockRowsInLastVB=(y-1)%VERTEX_BUFFER_TILE_LENGTH;	//bottom border within last VB

		m_numVertexBufferTiles=m_numVBTilesX*m_numVBTilesY;
		m_x=x;
		m_y=y;

		m_vertexBufferTiles = NEW DX8VertexBufferClass*[m_numVertexBufferTiles];
		m_vertexBufferBackup = NEW VERTEX_FORMAT [m_numVertexBufferTiles * HEIGHTMAP_VERTEX_NUM];

		for (i=0; i<m_numVertexBufferTiles; i++) {
#ifdef USE_NORMALS
			m_vertexBufferTiles[i] = NEW_REF(DX8VertexBufferClass,(DX8_FVF_XYZNUV2,HEIGHTMAP_VERTEX_NUM,DX8VertexBufferClass::USAGE_DEFAULT));
#else
			m_vertexBufferTiles[i] = NEW_REF(DX8VertexBufferClass,(DX8_VERTEX_FORMAT,HEIGHTMAP_VERTEX_NUM,DX8VertexBufferClass::USAGE_DEFAULT));
#endif
		}

		//go with a preset material for now.
	}

	updateBlock(0,0,x-1,y-1,pMap,pLightsIterator);

	return 0;
}


//=============================================================================
// HeightMapRenderObjClass::On_Frame_Update
//=============================================================================
// Updates the diffuse color values in the vertices as affected by the dynamic
// lights.
// TheSuperHackers @bugfix xezon 15/12/2025 Now draws the dynamic lights
// properly on the entirety of the drawable map region.
//=============================================================================
void HeightMapRenderObjClass::On_Frame_Update()
{
	BaseHeightMapRenderObjClass::On_Frame_Update();
	Int i,j,k;
	Int originX,originY;
	if (Scene==nullptr) return;
	RTS3DScene *pMyScene = (RTS3DScene *)Scene;


	RefRenderObjListIterator pDynamicLightsIterator(pMyScene->getDynamicLights());
	if (m_map == nullptr) {
		return;
	}

#ifdef DO_UNIT_TIMINGS
#pragma MESSAGE("*** WARNING *** DOING DO_UNIT_TIMINGS!!!!")
	return;
#endif

#ifdef EXTENDED_STATS
	if (DX8Wrapper::stats.m_disableTerrain) {
		return;
	}
#endif

	Int numDynaLights=0;
	W3DDynamicLight *enabledLights[MAX_ENABLED_DYNAMIC_LIGHTS];

	const Int xCoordMin = m_map->getDrawOrgX() - m_map->getBorderSizeInline();
	const Int yCoordMin = m_map->getDrawOrgY() - m_map->getBorderSizeInline();
	const Int xCoordMax = xCoordMin + m_map->getDrawWidth();
	const Int yCoordMax = yCoordMin + m_map->getDrawHeight();

	for (pDynamicLightsIterator.First(); !pDynamicLightsIterator.Is_Done(); pDynamicLightsIterator.Next())
	{
		W3DDynamicLight *pLight = (W3DDynamicLight*)pDynamicLightsIterator.Peek_Obj();
		pLight->m_processMe = false;
		if (pLight->m_enabled || pLight->m_priorEnable) {
			Real range = pLight->Get_Attenuation_Range();
			if (pLight->m_priorEnable) {
				pLight->m_prevMinX = pLight->m_minX;
				pLight->m_prevMinY = pLight->m_minY;
				pLight->m_prevMaxX = pLight->m_maxX;
				pLight->m_prevMaxY = pLight->m_maxY;
			}
			Vector3	pos = pLight->Get_Position();
			pLight->m_minX = (pos.X-range)/MAP_XY_FACTOR;
			pLight->m_maxX = (pos.X+range)/MAP_XY_FACTOR+1.0f;
			pLight->m_minY = (pos.Y-range)/MAP_XY_FACTOR;
			pLight->m_maxY = (pos.Y+range)/MAP_XY_FACTOR+1.0f;
			if (!pLight->m_priorEnable) {
				pLight->m_prevMinX = pLight->m_minX;
				pLight->m_prevMinY = pLight->m_minY;
				pLight->m_prevMaxX = pLight->m_maxX;
				pLight->m_prevMaxY = pLight->m_maxY;
			}

			if (pLight->m_minX < xCoordMax &&
					pLight->m_minY < yCoordMax &&
					pLight->m_maxX > xCoordMin &&
					pLight->m_maxY > yCoordMin) {
				pLight->m_processMe = TRUE;
			} else if (pLight->m_prevMinX < xCoordMax &&
					pLight->m_prevMinY < yCoordMax &&
					pLight->m_prevMaxX > xCoordMin &&
					pLight->m_prevMaxY > yCoordMin) {
				pLight->m_processMe = TRUE;
			} else {
				pLight->m_processMe = false;
			}
			if (pLight->m_processMe) {
				enabledLights[numDynaLights] = pLight;
				numDynaLights++;
				if (numDynaLights == MAX_ENABLED_DYNAMIC_LIGHTS) {
					break;
				}
			}
		}
		pLight->m_priorEnable = pLight->m_enabled;
	}
	if (numDynaLights > 0) {
		//step through each vertex buffer that needs updating
		for (j=0; j<m_numVBTilesY; j++)
		{
			originY=j*VERTEX_BUFFER_TILE_LENGTH;	//location of this VB on the large full-size heightmap
			Int yMin, yMax;
			yMin = originY;
			yMax = originY+VERTEX_BUFFER_TILE_LENGTH;
			Bool intersect = false;
			Int yCoordMin = getYWithOrigin(yMin)+m_map->getDrawOrgY()-m_map->getBorderSizeInline();
			Int yCoordMax = getYWithOrigin(yMax-1)+m_map->getDrawOrgY()+1-m_map->getBorderSizeInline();
			if (yCoordMax>yCoordMin) {
				// no wrap occurred.
				for (k=0; k<numDynaLights; k++) {
					if (enabledLights[k]->m_minY < yCoordMax &&
						enabledLights[k]->m_maxY > yCoordMin) {
						intersect = true;
						break;
					}
					if (enabledLights[k]->m_prevMinY < yCoordMax &&
						enabledLights[k]->m_prevMaxY > yCoordMin) {
						intersect = true;
						break;
					}
				}
			} else {
				// wrap occurred, so we are outside of this range.
				int tmp=yCoordMin;
				yCoordMin = yCoordMax;
				yCoordMax = tmp;
				for (k=0; k<numDynaLights; k++) {
					if (enabledLights[k]->m_minY <=  yCoordMin ||
						enabledLights[k]->m_maxY >= yCoordMax) {
						intersect = true;
						break;
					}
					if (enabledLights[k]->m_prevMinY <=  yCoordMin ||
						enabledLights[k]->m_prevMaxY >= yCoordMax) {
						intersect = true;
						break;
					}
				}
			}
			if (!intersect) {
				continue;
			}

			for (i=0; i<m_numVBTilesX; i++)
			{
				originX=i*VERTEX_BUFFER_TILE_LENGTH;	//location of this VB on the large full-size heightmap
				Int xMin, xMax;
				xMin = originX;
				xMax = originX+VERTEX_BUFFER_TILE_LENGTH;

				Bool intersect = false;
				Int xCoordMin = getXWithOrigin(xMin)+m_map->getDrawOrgX()-m_map->getBorderSizeInline();
				Int xCoordMax = getXWithOrigin(xMax-1)+m_map->getDrawOrgX()+1-m_map->getBorderSizeInline();
				if (xCoordMax>xCoordMin) {
					// no wrap occurred.
					for (k=0; k<numDynaLights; k++) {
						if (enabledLights[k]->m_minX < xCoordMax &&
							enabledLights[k]->m_maxX > xCoordMin) {
							intersect = true;
							break;
						}
						if (enabledLights[k]->m_prevMinX < xCoordMax &&
							enabledLights[k]->m_prevMaxX > xCoordMin) {
							intersect = true;
							break;
						}
					}
				} else {
					// wrap occurred, so we are outside of this range.
					int tmp=xCoordMin;
					xCoordMin = xCoordMax;
					xCoordMax = tmp;
					for (k=0; k<numDynaLights; k++) {
						if (enabledLights[k]->m_minX <=  xCoordMin ||
							enabledLights[k]->m_maxX >= xCoordMax) {
							intersect = true;
							break;
						}
						if (enabledLights[k]->m_prevMinX <=  xCoordMin ||
							enabledLights[k]->m_prevMaxX >= xCoordMax) {
							intersect = true;
							break;
						}
					}
				}
				if (!intersect) {
					continue;
				}
				DX8VertexBufferClass *pVB = getVertexBufferTile(i, j);
				VERTEX_FORMAT *pData = getVertexBufferBackup(i, j);
				updateVBForLight(pVB, pData, xMin, yMin, xMax, yMax, originX,originY, enabledLights, numDynaLights);
			}
		}
	}
}

//=============================================================================
// HeightMapRenderObjClass::staticLightingChanged
//=============================================================================
/** Notification that all lighting needs to be recalculated. */
//=============================================================================
void HeightMapRenderObjClass::staticLightingChanged()
{
	BaseHeightMapRenderObjClass::staticLightingChanged();
}

#define CENTER_LIMIT 2
#define BIG_JUMP 16
#define WIDE_STEP 32

static Int visMinX, visMinY, visMaxX, visMaxY;
static Bool check(const FrustumClass & frustum, WorldHeightMap *pMap, Int x, Int y)
{
	if (x<0 || y<0) return(false);
	if (x>= pMap->getXExtent() || y>= pMap->getYExtent()) return(false);
	if (x >= visMinX && y >= visMinY && x <=visMaxX && y <= visMaxY) {
		return(true);
	}
	Int height = pMap->getHeight(x, y);
	Vector3 loc((x-pMap->getBorderSizeInline())*MAP_XY_FACTOR, (y-pMap->getBorderSizeInline())*MAP_XY_FACTOR, height*MAP_HEIGHT_SCALE);
	if (CollisionMath::Overlap_Test(frustum,loc) == CollisionMath::INSIDE) {
		if (x<visMinX) visMinX=x;
		if (x>visMaxX) visMaxX=x;
		if (y<visMinY) visMinY=y;
		if (y>visMaxY) visMaxY=y;
		return(true);
	}
	return(false);
}

static void calcVis(const FrustumClass & frustum, WorldHeightMap *pMap, Int minX, Int minY, Int maxX, Int maxY, Int limit)
{
	if (maxX-minX<2) return;
	if (maxY-minY<2) return;
	if (minX >=visMinX && minY >= visMinY && maxX <=visMaxX && maxY <= visMaxY) {
		return;
	}
	Int midX = (minX+maxX)/2;
	Int midY = (minY+maxY)/2;
	Bool recurse1 = maxX-minX>=limit;
	Bool recurse2 = recurse1;
	Bool recurse3 = recurse1;
	Bool recurse4 = recurse1;
	/* boxes are:

			1     2


			3			4 */

	if (check(frustum, pMap, midX, maxY)) {
		recurse1=true;
		recurse2=true;
	}
	if (check(frustum, pMap, midX, minY)) {
		recurse3=true;
		recurse4=true;
	}
	if (check(frustum, pMap, midX, midY)) {
		recurse1=true;
		recurse2=true;
		recurse3=true;
		recurse4=true;
	}
	if (check(frustum, pMap, minX, midY)) {
		recurse1=true;
		recurse3=true;
	}
	if (check(frustum, pMap, maxX, midY)) {
		recurse2=true;
		recurse4=true;
	}
	if (recurse1) {
		calcVis(frustum, pMap, minX, midY, midX, maxY, limit);
	}
	if (recurse2) {
		calcVis(frustum, pMap, midX, midY, maxX, maxY, limit);
	}
	if (recurse3) {
		calcVis(frustum, pMap, minX, minY, midX, midY, limit);
	}
	if (recurse4) {
		calcVis(frustum, pMap, midX, minY, maxX, midY, limit);
	}
}





//=============================================================================
// HeightMapRenderObjClass::updateCenter
//=============================================================================
/** Updates the positioning of the drawn portion of the height map in the
heightmap.  As the view slides around, this determines what is the actually
rendered portion of the terrain. Only a small section is rendered at any time.
*/
//=============================================================================
void HeightMapRenderObjClass::updateCenter(CameraClass *camera, const Vector3 *cameraPivot, RefRenderObjListIterator *pLightsIterator)
{
	if (m_map==nullptr) {
		return;
	}
	if (m_updating) {
		return;
	}
	if (m_vertexBufferTiles ==nullptr)
		return;		//did not initialize resources yet.

	BaseHeightMapRenderObjClass::updateCenter(camera, cameraPivot, pLightsIterator);

	if (m_x >= m_map->getXExtent() && m_y >= m_map->getYExtent())
  {
		return; // no need to center.
	}

	const Real cameraPitch = asin(fabs(camera->Get_Forward_Dir().Z));
	Int newOrgX;
	Int newOrgY;

	if (cameraPitch > ViewDefaultLowPitchRadians)
	{
		// TheSuperHackers @info This is the original code to determine the center position for the visible terrain area.
		// It is relatively expensive and breaks when the frustum planes can no longer intersect with the terrain at low camera
		// pitch or when the camera is too far from the terrain, but it is very accurate when the camera is close to the
		// terrain. For now, we prefer to keep this code for the original camera pitch and above.

		// determine the ray corresponding to the camera and distance to projection plane
		const Matrix3D& camera_matrix = camera->Get_Transform();
		Vector3 camera_location  = camera->Get_Position();
		Vector3 rayLocation;
		Vector3 rayDirection;
		Vector3 rayDirectionPt;
		// the projected ray has the same origin as the camera
		rayLocation = camera_location;
		// determine the location of the screen coordinate in camera-model space
		const ViewportClass &viewport = camera->Get_Viewport();
		Int i, j, minHt;

		Real intersectionZ;
		minHt = m_map->getMaxHeightValue();
		for (j=0; j<m_y; j+=4) {
			for (i=0; i<m_x; i+=4) {
				Short cur = m_map->getDisplayHeight(i,j);
				if (cur<minHt) minHt = cur;
			}
		}
		intersectionZ = (float)minHt;
	//	float aspect = camera->Get_Aspect_Ratio();

		Vector2 min,max;
		camera->Get_View_Plane(min,max);
		float xscale = (max.X - min.X);
		float yscale = (max.Y - min.Y);

		float zmod = -1.0; // Scene->vpd; // Note: view plane distance is now always 1.0 from the camera
		float minX = 200000;
		float maxX = -minX;
		float minY = 200000;
		float maxY = -minY;
		for (i=0; i<2; i++) {
			for (j=0; j<2; j++) {
				float xmod = (-i + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
				float ymod = (j - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

				// transform the screen coordinates by the camera's matrix into world coordinates.
				float x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
				float y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
				float z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];

				rayDirection.Set(x,y,z);
				rayDirection.Normalize();
				rayDirectionPt = rayLocation+rayDirection;

				x = Vector3::Find_X_At_Z(intersectionZ, rayLocation, rayDirectionPt);
				y = Vector3::Find_Y_At_Z(intersectionZ, rayLocation, rayDirectionPt);
				if (x<minX) minX = x;
				if (x>maxX) maxX = x;
				if (y<minY) minY = y;
				if (y>maxY) maxY = y;
			}
		}

		// convert back to cell indexes.
		minX /= MAP_XY_FACTOR;
		maxX /= MAP_XY_FACTOR;
		minY /= MAP_XY_FACTOR;
		maxY /= MAP_XY_FACTOR;

		minX += m_map->getBorderSizeInline();
		maxX += m_map->getBorderSizeInline();
		minY += m_map->getBorderSizeInline();
		maxY += m_map->getBorderSizeInline();

		visMinX = m_map->getXExtent();
		visMinY = m_map->getYExtent();
		visMaxX = 0;
		visMaxY = 0;

		///< @todo find out why values go out of range
		if (minX<0) minX=0;
		if (minY<0) minY=0;
		if (maxX > visMinX) maxX = visMinX;
		if (maxY > visMinY) maxY = visMinY;

		const FrustumClass & frustum = camera->Get_Frustum();
		Int limit = (maxX-minX)/2;
		if (limit > WIDE_STEP/2) {
			limit=WIDE_STEP/2;
		}
		calcVis(frustum, m_map, minX-WIDE_STEP/2, minY-WIDE_STEP/2, maxX+WIDE_STEP/2, maxY+WIDE_STEP/2, limit);

		newOrgX = (visMaxX+visMinX)/2 - m_x/2;
		newOrgY = (visMaxY+visMinY)/2 - m_y/2;
	}
	else
	{
		// TheSuperHackers @fix Very fast approximation. Works well for all camera pitches, but is less accurate
		// than the original implementation. Using this method for higher camera pitch would require to increase
		// the normal draw width by at least one tile length.
		const Real visibleTerrainEdgeLen = (m_x+m_y)/2 * MAP_XY_FACTOR;
		const Real magicEdgeLenScale = 0.25f * visibleTerrainEdgeLen;
		Vector3 viewDir = camera->Get_Forward_Dir();
		Vector2 shiftPivot;
		shiftPivot.X = viewDir.X * magicEdgeLenScale;
		shiftPivot.Y = viewDir.Y * magicEdgeLenScale;

		newOrgX = WWMath::Round((cameraPivot->X + shiftPivot.X)/MAP_XY_FACTOR) - m_x/2 + m_map->getBorderSizeInline();
		newOrgY = WWMath::Round((cameraPivot->Y + shiftPivot.Y)/MAP_XY_FACTOR) - m_y/2 + m_map->getBorderSizeInline();
	}

	WorldHeightMap::DrawArea newDrawArea = m_map->createDrawArea(newOrgX, newOrgY);

	m_updating = true;
	if (m_needFullUpdate)
	{
		m_needFullUpdate = false;
		m_map->setDrawArea(newDrawArea);
		updateBlock(0, 0, m_x-1, m_y-1, m_map, pLightsIterator);
		m_updating = false;
		return;
	}
	else
	{
		constexpr const Int cellOffset = 1;
		const Int deltaX = newDrawArea.originX - m_map->getDrawOrgX();
		const Int deltaY = newDrawArea.originY - m_map->getDrawOrgY();

		if (IABS(deltaX) > m_x/2 || IABS(deltaY)>m_x/2) {
			if (m_map->setDrawArea(newDrawArea)) {
				m_originY = 0;
				m_originX = 0;
				updateBlock(0, 0, m_x-1, m_y-1, m_map, pLightsIterator);
			}
			m_updating = false;
			return;
		}

		if (abs(deltaY) > CENTER_LIMIT) {
			if (m_map->setDrawOrg(m_map->getDrawOrgX(), newOrgY)) {
				Int minY = 0;
				Int maxY = 0;
				m_originY += deltaY;
				if (m_originY >= m_y-1) m_originY -= m_y-1;
				if (deltaY<0) {
					minY = m_originY;
					maxY = m_originY-deltaY;
				} else {
					minY = m_originY - deltaY;
					maxY = m_originY;
				}
				minY-=cellOffset;
				if (m_originY < 0) m_originY += m_y-1;
				if (minY<0) {
					minY += m_y-1;
					if (minY<0) minY = 0;
					updateBlock(0, minY, m_x-1, m_y-1, m_map, pLightsIterator);
					updateBlock(0, 0, m_x-1, maxY, m_map, pLightsIterator);
				} else {
					updateBlock(0, minY, m_x-1, maxY, m_map, pLightsIterator);
				}
			}
			// It is much more efficient to update a couple of columns one frame, and then
			// a couple of rows.  So if we aren't "jumping" to a new view, and have done X
			// recently, return.
			if (abs(deltaX) < BIG_JUMP && !m_doXNextTime) {
				m_updating = false;
				m_doXNextTime = true;
				return;	// Only do the y this frame.  Do x next frame.  jba.
			}
		}
		if (abs(deltaX) > CENTER_LIMIT) {
			m_doXNextTime = false;
			newOrgX = m_map->getDrawOrgX() + deltaX;
			if (m_map->setDrawOrg(newOrgX, m_map->getDrawOrgY())) {
				Int minX = 0;
				Int maxX = 0;
				m_originX += deltaX;
				if (m_originX >= m_x-1) m_originX -= m_x-1;
				if (deltaX<0) {
					minX = m_originX;
					maxX = m_originX-deltaX;
				} else {
					minX = m_originX - deltaX;
					maxX = m_originX;
				}
				minX-=cellOffset;
				maxX+=cellOffset;
				if (m_originX < 0) m_originX += m_x-1;
				if (minX<0) {
					minX += m_x-1;
					if (minX<0) minX = 0;
					updateBlock(minX,0,m_x-1, m_y-1, m_map, pLightsIterator);
					updateBlock(0,0,maxX, m_y-1, m_map, pLightsIterator);
				} else {
					updateBlock(minX,0,maxX, m_y-1, m_map, pLightsIterator);
				}
			}
		}
	}
	m_updating = false;
}

//=============================================================================
// HeightMapRenderObjClass::Render
//=============================================================================
/** Renders (draws) the terrain. */
//=============================================================================
//DECLARE_PERF_TIMER(Terrain_Render)

void HeightMapRenderObjClass::Render(RenderInfoClass & rinfo)
{
	//USE_PERF_TIMER(Terrain_Render)

	Int i,j,devicePasses;
	W3DShaderManager::ShaderTypes st;
	const Bool doCloud = useCloud();

	if (doCloud)
	{
		// TheSuperHackers @tweak Updates the cloud movement before applying it to the world.
		// Is now decoupled from logic step.
		W3DShaderManager::updateCloud();
	}

	Matrix3D tm(Transform);
#if 0 // There is some weirdness sometimes with the dx8 static buffers.
			// This usually fixes terrain flashing.  jba.
	static Int delay = 1;
	delay --;
	if (delay<1) {
		delay = 1;
		static Int ndx = -1;
		ndx++;
		if (ndx>=m_numVertexBufferTiles) {
			ndx = 0;
		}
		DX8VertexBufferClass::WriteLockClass lockVtxBuffer(m_vertexBufferTiles + ndx);
		VERTEX_FORMAT *vb = (VERTEX_FORMAT*)lockVtxBuffer.Get_Vertex_Array();
		vb = 0;
	}
#endif
	// If there are trees, tell them to draw at the transparent time to draw.
	if (m_treeBuffer) {
		m_treeBuffer->setIsTerrain();
	}


#ifdef DO_UNIT_TIMINGS
#pragma MESSAGE("*** WARNING *** DOING DO_UNIT_TIMINGS!!!!")
	return;
#endif

#ifdef EXTENDED_STATS
	if (DX8Wrapper::stats.m_disableTerrain) {
		return;
	}
#endif

	DX8Wrapper::Set_Light_Environment(rinfo.light_environment);

	// Force shaders to update.
	m_stageTwoTexture->restore();
	DX8Wrapper::Set_Texture(0,nullptr);
	DX8Wrapper::Set_Texture(1,nullptr);
	ShaderClass::Invalidate();

	//	tm.Scale(ObjSpaceExtent);
	DX8Wrapper::Set_Transform(D3DTS_WORLD,tm);

	//Apply the shader and material

	DX8Wrapper::Set_Index_Buffer(m_indexBuffer,0);

	Bool doMultiPassWireFrame=FALSE;

	if (((RTS3DScene *)rinfo.Camera.Get_User_Data())->getCustomPassMode() == SCENE_PASS_ALPHA_MASK ||
		((SceneClass *)rinfo.Camera.Get_User_Data())->Get_Extra_Pass_Polygon_Mode() == SceneClass::EXTRA_PASS_CLEAR_LINE)
	{
			if (WW3D::Is_Texturing_Enabled())
			{	//first pass where we just fill the z-buffer

				devicePasses=1;	//one pass solid, next in wireframe.
				doMultiPassWireFrame=TRUE;

				if (rinfo.Additional_Pass_Count())
				{
					rinfo.Peek_Additional_Pass(0)->Install_Materials();
					renderTerrainPass(&rinfo.Camera);
					rinfo.Peek_Additional_Pass(0)->UnInstall_Materials();
					return;
				}
			}
			else
			{	//wireframe pass
				//Set to vertex diffuse lighting
				DX8Wrapper::Set_Material(m_vertexMaterialClass);
				//Set shader to non-textured solid color from vertex
				DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueSolidShader);
				devicePasses=1;	//one pass solid, next in wireframe.
				DX8Wrapper::Apply_Render_State_Changes();
				DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
				DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR,0xff808080);
				doMultiPassWireFrame=TRUE;
				renderTerrainPass(&rinfo.Camera);
				DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR,0xff008000);
				return;
			}
	}
	else
	{
		DX8Wrapper::Set_Material(m_vertexMaterialClass);
		DX8Wrapper::Set_Shader(m_shaderClass);

 		st=W3DShaderManager::ST_TERRAIN_BASE; //set default shader

 		//set correct shader based on current settings
 		if (!ShaderClass::Is_Backface_Culling_Inverted())
 		{	//not reflection pass
 			if (TheGlobalData->m_useLightMap && doCloud)
 			{	st=W3DShaderManager::ST_TERRAIN_BASE_NOISE12;
 			}
 			else
 			if (TheGlobalData->m_useLightMap)
 			{	//lightmap only
 				st=W3DShaderManager::ST_TERRAIN_BASE_NOISE2;
 			}
 			else
 			if (doCloud)
 			{	//cloudmap only
 				st=W3DShaderManager::ST_TERRAIN_BASE_NOISE1;
 			}
 		}
 		else
 		{	//reflection pass, just do base texture
 			st=W3DShaderManager::ST_TERRAIN_BASE;
 		}

 		//Find number of passes required to render current shader
 		devicePasses=W3DShaderManager::getShaderPasses(st);

 		if (m_disableTextures)
 			devicePasses=1;	//force to 1 lighting-only pass

 		//Specify all textures that this shader may need.
 		W3DShaderManager::setTexture(0,m_stageZeroTexture);
 		W3DShaderManager::setTexture(1,m_stageZeroTexture);
 		W3DShaderManager::setTexture(2,m_stageTwoTexture);	//cloud
 		W3DShaderManager::setTexture(3,m_stageThreeTexture);//noise
		//Disable writes to destination alpha channel (if there is one)
		if (DX8Wrapper::getBackBufferFormat() == WW3D_FORMAT_A8R8G8B8)
			DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_RED);
	}

	Int pass;
 	for (pass=0; pass<devicePasses; pass++) {
#ifdef TIMING_TESTS
#endif
		if (!doMultiPassWireFrame)	//multi-pass wireframe doesn't use regular shaders.
		{
 			if (m_disableTextures ) {
 				DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaque2DShader);
 				DX8Wrapper::Set_Texture(0,nullptr);
   			} else {
 				W3DShaderManager::setShader(st, pass);
			}
		}

		for (j=0; j<m_numVBTilesY; j++)
			for (i=0; i<m_numVBTilesX; i++)
			{
				DX8Wrapper::Set_Vertex_Buffer(getVertexBufferTile(i, j));
#ifdef PRE_TRANSFORM_VERTEX
				if (m_xformedVertexBuffer && pass==0) {
					// Note - m_xformedVertexBuffer should only be used for non T&L hardware.  jba.
					DX8Wrapper::Apply_Render_State_Changes();
					int code = DX8Wrapper::_Get_D3D_Device8()->ProcessVertices(0, 0, numVertex, m_xformedVertexBuffer[j*m_numVBTilesX+i], 0);
					::OutputDebugString("did process vertex\n");
				}
				if (m_xformedVertexBuffer) {
					// Note - m_xformedVertexBuffer should only be used for non T&L hardware.  jba.
					DX8Wrapper::Apply_Render_State_Changes();
					DX8Wrapper::_Get_D3D_Device8()->SetStreamSource(
						0,
						m_xformedVertexBuffer[j*m_numVBTilesX+i],
						D3DXGetFVFVertexSize(D3DFVF_XYZRHW |D3DFVF_DIFFUSE|D3DFVF_TEX2));
					DX8Wrapper::_Get_D3D_Device8()->SetVertexShader(D3DFVF_XYZRHW |D3DFVF_DIFFUSE|D3DFVF_TEX2);
				}
#endif
				if (Is_Hidden() == 0) {
					DX8Wrapper::Draw_Triangles(0, HEIGHTMAP_POLYGON_NUM, 0, HEIGHTMAP_VERTEX_NUM);
				}

			}
	}

	if (!doMultiPassWireFrame)
	{
		if (pass)	//shader was applied at least once?
 			W3DShaderManager::resetShader(st);

		//Draw feathered shorelines
		renderShoreLines(&rinfo.Camera);

		//Do additional pass over any tiles that have 3 textures blended together.
		if (TheGlobalData->m_use3WayTerrainBlends)
			renderExtraBlendTiles();

		Int yCoordMin = m_map->getDrawOrgY();
		Int yCoordMax = m_y+m_map->getDrawOrgY()-1;
		Int xCoordMin = m_map->getDrawOrgX();
		Int xCoordMax = m_x+m_map->getDrawOrgX()-1;
	#ifdef TEST_CUSTOM_EDGING
		// Draw edging just before last pass.
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Set_Texture(1,nullptr);
		m_stageTwoTexture->restore();
		m_customEdging->drawEdging(m_map, xCoordMin, xCoordMax, yCoordMin, yCoordMax,
			m_stageZeroTexture, doCloud?m_stageTwoTexture: nullptr, TheGlobalData->m_useLightMap?m_stageThreeTexture: nullptr);
	#endif
	#ifdef DO_ROADS
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Set_Texture(1,nullptr);
		m_stageTwoTexture->restore();

		ShaderClass::Invalidate();
		if (!ShaderClass::Is_Backface_Culling_Inverted()) {
			DX8Wrapper::Set_Material(m_vertexMaterialClass);
			if (Scene) {
				RTS3DScene *pMyScene = (RTS3DScene *)Scene;
				RefRenderObjListIterator pDynamicLightsIterator(pMyScene->getDynamicLights());
				m_roadBuffer->drawRoads(&rinfo.Camera, doCloud?m_stageTwoTexture:nullptr, TheGlobalData->m_useLightMap?m_stageThreeTexture:nullptr,
					m_disableTextures,xCoordMin-m_map->getBorderSizeInline(), xCoordMax-m_map->getBorderSizeInline(), yCoordMin-m_map->getBorderSizeInline(), yCoordMax-m_map->getBorderSizeInline(), &pDynamicLightsIterator);
			}
		}
	#endif
	if (m_propBuffer) {
		m_propBuffer->drawProps(rinfo);
	}
	#ifdef DO_SCORCH
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Set_Texture(1,nullptr);
		m_stageTwoTexture->restore();

		ShaderClass::Invalidate();
		if (!ShaderClass::Is_Backface_Culling_Inverted()) {
			drawScorches();
		}
	#endif
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Set_Texture(1,nullptr);
		m_stageTwoTexture->restore();
		ShaderClass::Invalidate();
		DX8Wrapper::Apply_Render_State_Changes();

		m_bridgeBuffer->drawBridges(&rinfo.Camera, m_disableTextures, doCloud?m_stageTwoTexture:nullptr);

		if (TheTerrainTracksRenderObjClassSystem)
			TheTerrainTracksRenderObjClassSystem->flush();

		if (m_shroud && rinfo.Additional_Pass_Count())
		{
			rinfo.Peek_Additional_Pass(0)->Install_Materials();
			renderTerrainPass(&rinfo.Camera);
			rinfo.Peek_Additional_Pass(0)->UnInstall_Materials();
		}

		ShaderClass::Invalidate();
		DX8Wrapper::Apply_Render_State_Changes();
	}
	else
			m_bridgeBuffer->drawBridges(&rinfo.Camera, m_disableTextures, m_stageTwoTexture);

  if ( m_waypointBuffer )
	  m_waypointBuffer->drawWaypoints(rinfo);

	m_bibBuffer->renderBibs();

	// We do some custom blending, so tell the shader class to reset everything.
	DX8Wrapper::Set_Texture(0,nullptr);
	DX8Wrapper::Set_Texture(1,nullptr);
	m_stageTwoTexture->restore();
	ShaderClass::Invalidate();
	DX8Wrapper::Set_Material(nullptr);

}



///Performs additional terrain rendering pass, blending in the black shroud texture.
void HeightMapRenderObjClass::renderTerrainPass(CameraClass *pCamera)
{
	DX8Wrapper::Set_Transform(D3DTS_WORLD,Matrix3D(true));

	//Apply the shader and material

	DX8Wrapper::Set_Index_Buffer(m_indexBuffer,0);

	for (Int j=0; j<m_numVBTilesY; j++)
		for (Int i=0; i<m_numVBTilesX; i++)
		{
			DX8Wrapper::Set_Vertex_Buffer(getVertexBufferTile(i, j));
#ifdef PRE_TRANSFORM_VERTEX
			if (m_xformedVertexBuffer && pass==0) {
				// Note - m_xformedVertexBuffer should only be used for non T&L hardware.  jba.
				DX8Wrapper::Apply_Render_State_Changes();
				int code = DX8Wrapper::_Get_D3D_Device8()->ProcessVertices(0, 0, numVertex, m_xformedVertexBuffer[j*m_numVBTilesX+i], 0);
				::OutputDebugString("did process vertex\n");
			}
			if (m_xformedVertexBuffer) {
				// Note - m_xformedVertexBuffer should only be used for non T&L hardware.  jba.
				DX8Wrapper::Apply_Render_State_Changes();
				DX8Wrapper::_Get_D3D_Device8()->SetStreamSource(
					0,
					m_xformedVertexBuffer[j*m_numVBTilesX+i],
					D3DXGetFVFVertexSize(D3DFVF_XYZRHW |D3DFVF_DIFFUSE|D3DFVF_TEX2));
				DX8Wrapper::_Get_D3D_Device8()->SetVertexShader(D3DFVF_XYZRHW |D3DFVF_DIFFUSE|D3DFVF_TEX2);
			}
#endif
			if (Is_Hidden() == 0) {
				DX8Wrapper::Draw_Triangles(0, HEIGHTMAP_POLYGON_NUM, 0, HEIGHTMAP_VERTEX_NUM);
			}
		}
}

//=============================================================================
// HeightMapRenderObjClass::renderExtraBlendTiles
//=============================================================================
/** Renders an additional terrain pass including only those tiles which have more than 2 textures
blended together.  Used primarily for corner cases where 3 different textures meet.*/
void HeightMapRenderObjClass::renderExtraBlendTiles()
{
	Int vertexCount = 0;
	Int indexCount = 0;
	Int xExtent = m_map->getXExtent();
	Int border = m_map->getBorderSizeInline();
	static Int maxBlendTiles = DEFAULT_MAX_FRAME_EXTRABLEND_TILES;

	m_numVisibleExtraBlendTiles = 0;

	if (!m_numExtraBlendTiles)
		return;	//nothing to draw

	if (maxBlendTiles > 10000)	//we can only fit about 10000 tiles into a single VB.
		maxBlendTiles = 10000;

	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,DX8_FVF_XYZNDUV2,maxBlendTiles*4);
	DynamicIBAccessClass ib_access(BUFFER_TYPE_DYNAMIC_DX8,maxBlendTiles*6);
	{

		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* vb= lock.Get_Formatted_Vertex_Array();
		DynamicIBAccessClass::WriteLockClass lockib(&ib_access);
		UnsignedShort *ib=lockib.Get_Index_Array();

		if (!vb || !ib) return;

		const UnsignedByte* data = m_map->getDataPtr();

		//Loop over visible terrain and extract all the tiles that need extra blend
		Int drawEdgeY=m_map->getDrawOrgY()+m_map->getDrawHeight()-1;
		Int drawEdgeX=m_map->getDrawOrgX()+m_map->getDrawWidth()-1;
		if (drawEdgeX > (m_map->getXExtent()-1))
			drawEdgeX = m_map->getXExtent()-1;
		if (drawEdgeY > (m_map->getYExtent()-1))
			drawEdgeY = m_map->getYExtent()-1;
		Int drawStartX=m_map->getDrawOrgX();
		Int drawStartY=m_map->getDrawOrgY();

		for (Int j=0; j<m_numExtraBlendTiles; j++)
		{
			if (vertexCount >= (maxBlendTiles*4))
				break;	//no room in vertex buffer

			Real U[4],V[4];
			UnsignedByte alpha[4];
			Bool flipState,cliffState;
			Int x = m_extraBlendTilePositions[j] & 0xffff;
			Int y = m_extraBlendTilePositions[j] >> 16;

			if (x >= drawStartX && x < drawEdgeX &&
				y >= drawStartY && y < drawEdgeY &&
				m_map->getExtraAlphaUVData(x,y,U,V,alpha,&flipState, &cliffState))
			{	//this tile is inside visible region and has 3rd blend layer.

				Int idx = x+y*xExtent;

				Real p0=data[idx]*MAP_HEIGHT_SCALE;
				Real p1=data[idx+1]*MAP_HEIGHT_SCALE;
				Real p2=data[idx + 1 + xExtent]*MAP_HEIGHT_SCALE;
				Real p3=data[idx + xExtent]*MAP_HEIGHT_SCALE;
				if (cliffState && abs(p0-p2) > abs(p1-p3))	//cliffs sometimes force a flip
					flipState = TRUE;

				vb->x=(x-border)*MAP_XY_FACTOR;
				vb->y=(y-border)*MAP_XY_FACTOR;
				vb->z=p0;
				vb->nx=0;
				vb->ny=0;
				vb->nz=0;
				vb->diffuse=(alpha[0]<<24)|(getStaticDiffuse(x,y) & 0x00ffffff);
				vb->u1=U[0];
				vb->v1=V[0];
				vb->u2=0;
				vb->v2=0;
				vb++;

				vb->x=(x+1-border)*MAP_XY_FACTOR;
				vb->y=(y-border)*MAP_XY_FACTOR;
				vb->z=p1;
				vb->nx=0;
				vb->ny=0;
				vb->nz=0;
				vb->diffuse=(alpha[1]<<24)|(getStaticDiffuse(x+1,y) & 0x00ffffff);
				vb->u1=U[1];
				vb->v1=V[1];
				vb->u2=0;
				vb->v2=0;
				vb++;

				vb->x=(x+1-border)*MAP_XY_FACTOR;
				vb->y=(y+1-border)*MAP_XY_FACTOR;
				vb->z=p2;
				vb->nx=0;
				vb->ny=0;
				vb->nz=0;
				vb->diffuse=(alpha[2]<<24)|(getStaticDiffuse(x+1,y+1) & 0x00ffffff);
				vb->u1=U[2];
				vb->v1=V[2];
				vb->u2=0;
				vb->v2=0;
				vb++;

				vb->x=(x-border)*MAP_XY_FACTOR;
				vb->y=(y+1-border)*MAP_XY_FACTOR;
				vb->z=p3;
				vb->nx=0;
				vb->ny=0;
				vb->nz=0;
				vb->diffuse=(alpha[3]<<24)|(getStaticDiffuse(x,y+1) & 0x00ffffff);
				vb->u1=U[3];
				vb->v1=V[3];
				vb->u2=0;
				vb->v2=0;
				vb++;

				if (flipState)
				{
					ib[0]=1+vertexCount;
					ib[1]=3+vertexCount;
					ib[2]=0+vertexCount;
					ib[3]=1+vertexCount;
					ib[4]=2+vertexCount;
					ib[5]=3+vertexCount;
				}
				else
				{
					ib[0]=0+vertexCount;
					ib[1]=2+vertexCount;
					ib[2]=3+vertexCount;
					ib[3]=0+vertexCount;
					ib[4]=1+vertexCount;
					ib[5]=2+vertexCount;
				}
				ib += 6;
				vertexCount +=4;
				indexCount +=6;
			}
		}
	}

	if (vertexCount)
	{
		//Check if we couldn't fit all blend tiles into vertex buffer so we can enlarge it for next frame.
		if (vertexCount == (maxBlendTiles*4))
			maxBlendTiles += 16;	//enlarge by 16 to reduce trashing.

		ShaderClass::Invalidate();	//invalidate to force shader to reset since we directly changed states
		DX8Wrapper::Set_Index_Buffer(ib_access,0);
		DX8Wrapper::Set_Vertex_Buffer(vb_access);
		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);
		ShaderClass shader=ShaderClass::_PresetOpaqueShader;
		shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//disable writes to z
		DX8Wrapper::Set_Shader(shader);

		if (TheGlobalData->m_use3WayTerrainBlends == 2)
		{
			shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);	//disable lighting.
			shader.Set_Texturing(ShaderClass::TEXTURING_DISABLE);		//disable texturing.
			DX8Wrapper::Set_Shader(shader);
			DX8Wrapper::Set_Texture(0,nullptr);	//debug mode which draws terrain tiles in white.
			if (Is_Hidden() == 0) {
				DX8Wrapper::Draw_Triangles(	0,indexCount/3, 0,	vertexCount);	//draw a quad, 2 triangles, 4 verts
				m_numVisibleExtraBlendTiles += indexCount/6;
			}
		}
		else
		{
			W3DShaderManager::setTexture(0,m_stageOneTexture);
			W3DShaderManager::setTexture(1,m_stageTwoTexture);	//cloud
			W3DShaderManager::setTexture(2,m_stageThreeTexture);	//noise/lightmap

			W3DShaderManager::ShaderTypes st = W3DShaderManager::ST_ROAD_BASE;

			const Bool doCloud = useCloud();

			if (TheGlobalData->m_useLightMap && doCloud)
 			{
				st = W3DShaderManager::ST_ROAD_BASE_NOISE12;
 			}
 			else if (TheGlobalData->m_useLightMap)
 			{	//lightmap only
 				st = W3DShaderManager::ST_ROAD_BASE_NOISE2;
 			}
 			else if (doCloud)
 			{	//cloudmap only
 				st = W3DShaderManager::ST_ROAD_BASE_NOISE1;
 			}

			Int devicePasses=W3DShaderManager::getShaderPasses(st);

			for (Int pass=0; pass < devicePasses; pass++)
			{
				W3DShaderManager::setShader(st, pass);
				if (Is_Hidden() == 0) {
					DX8Wrapper::Draw_Triangles(	0,indexCount/3, 0,	vertexCount);	//draw a quad, 2 triangles, 4 verts
					m_numVisibleExtraBlendTiles += indexCount/6;
				}
			}
			W3DShaderManager::resetShader(st);
		}
  }
}
#endif
