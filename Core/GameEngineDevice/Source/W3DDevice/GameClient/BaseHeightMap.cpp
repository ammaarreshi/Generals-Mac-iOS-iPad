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
#include "Common/Xfer.h"

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
#include "W3DDevice/GameClient/W3DPropBuffer.h"
#include "W3DDevice/GameClient/W3DTreeBuffer.h"
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
#include "W3DDevice/GameClient/BaseHeightMap.h"

#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/FlatHeightMap.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DSnow.h"


extern FlatHeightMapRenderObjClass *TheFlatHeightMap;
extern HeightMapRenderObjClass *TheHeightMap;

//-----------------------------------------------------------------------------
//         Private Data
//-----------------------------------------------------------------------------
#define SC_DETAIL_BLEND ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass detailOpaqueShader(SC_DETAIL_BLEND);

//-----------------------------------------------------------------------------
//         Global Functions & Data
//-----------------------------------------------------------------------------
/// The one-of for the terrain rendering object.
BaseHeightMapRenderObjClass *TheTerrainRenderObject=nullptr;

/** Entry point so that trees can be drawn at the appropriate point in the rendering pipe for
    transparent objects. */
void DoTrees(RenderInfoClass & rinfo)
{
	if (TheTerrainRenderObject) {
		TheTerrainRenderObject->renderTrees(&rinfo.Camera);
	}
}

void oversizeTheTerrain(Int amount)
{
	if (TheTerrainRenderObject)
	{
		TheTerrainRenderObject->oversizeTerrain(amount);
	}
}

#define DEFAULT_MAX_BATCH_SHORELINE_TILES		512	//maximum number of terrain tiles rendered per call (must fit in one VB)
#define DEFAULT_MAX_MAP_SHORELINE_TILES		4096	//default size of array allocated to hold all map shoreline tiles.

#define ADJUST_FROM_INDEX_TO_REAL(k) ((k-m_map->getBorderSizeInline())*MAP_XY_FACTOR)
inline Int IABS(Int x) {	if (x>=0) return x; return -x;};

//-----------------------------------------------------------------------------
//         Private Functions
//-----------------------------------------------------------------------------

//=============================================================================
// BaseHeightMapRenderObjClass::freeMapResources
//=============================================================================
/** Frees the w3d resources used to draw the terrain. */
//=============================================================================
Int BaseHeightMapRenderObjClass::freeMapResources()
{
#ifdef DO_SCORCH
	freeScorchBuffers();
#endif
	REF_PTR_RELEASE(m_vertexMaterialClass);
	REF_PTR_RELEASE(m_stageZeroTexture);
	REF_PTR_RELEASE(m_stageOneTexture);
	REF_PTR_RELEASE(m_stageTwoTexture);
	REF_PTR_RELEASE(m_stageThreeTexture);
	REF_PTR_RELEASE(m_destAlphaTexture);
	REF_PTR_RELEASE(m_map);

	return 0;
}

#ifdef DO_SCORCH
//=============================================================================
// BaseHeightMapRenderObjClass::drawScorches
//=============================================================================
/** Draws the scorch marks. */
//=============================================================================
void BaseHeightMapRenderObjClass::drawScorches()
{

	updateScorches();
	if (m_curNumScorchIndices == 0) {
		return;
	}
	DX8Wrapper::Set_Index_Buffer(m_indexScorch,0);
	DX8Wrapper::Set_Vertex_Buffer(m_vertexScorch);
	DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaShader);

	DX8Wrapper::Set_Texture(0,m_scorchTexture);
	if (Is_Hidden() == 0) {
		DX8Wrapper::Draw_Triangles(	0,m_curNumScorchIndices/3, 0,	m_curNumScorchVertices);
	}
}
#endif


//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// BaseHeightMapRenderObjClass::~BaseHeightMapRenderObjClass
//=============================================================================
/** Destructor. Releases w3d assets. */
//=============================================================================
BaseHeightMapRenderObjClass::~BaseHeightMapRenderObjClass()
{
	freeMapResources();

	delete m_treeBuffer;
	m_treeBuffer = nullptr;

	delete m_propBuffer;
	m_propBuffer = nullptr;

	delete m_bibBuffer;
	m_bibBuffer = nullptr;

#ifdef DO_ROADS
	delete m_roadBuffer;
	m_roadBuffer = nullptr;
#endif

	delete m_bridgeBuffer;
	m_bridgeBuffer = nullptr;

	delete m_waypointBuffer;
	m_waypointBuffer = nullptr;

	delete m_shroud;
	m_shroud = nullptr;

	delete [] m_shoreLineTilePositions;
	m_shoreLineTilePositions = nullptr;

	delete [] m_shoreLineSortInfos;
	m_shoreLineSortInfos = nullptr;
}

//=============================================================================
// BaseHeightMapRenderObjClass::BaseHeightMapRenderObjClass
//=============================================================================
/** Constructor. Mostly nulls out the member variables. */
//=============================================================================
BaseHeightMapRenderObjClass::BaseHeightMapRenderObjClass()
{
	m_x=0;
	m_y=0;
	m_needFullUpdate = false;
	m_showImpassableAreas = false;
	m_updating = false;
	//Set height to the maximum value that can be stored.
	//We should refine this with actual value.
	m_maxHeight=(pow(256.0, sizeof(HeightSampleType))-1.0)*MAP_HEIGHT_SCALE;
	m_minHeight=0;
	m_shoreLineTilePositions=nullptr;
	m_numShoreLineTiles=0;
	m_shoreLineSortInfos=nullptr;
	m_shoreLineSortInfosSize=0;
	m_shoreLineSortInfosXMajor=TRUE;
	m_shoreLineTileSortMaxCoordinate=0;
	m_shoreLineTileSortMinCoordinate=0;
	m_numVisibleShoreLineTiles=0;
	m_shoreLineTilePositionsSize=0;
	m_currentMinWaterOpacity = -1.0f;

	m_vertexMaterialClass=nullptr;
	m_stageZeroTexture=nullptr;
	m_stageOneTexture=nullptr;
	m_stageTwoTexture=nullptr;
	m_stageThreeTexture=nullptr;
	m_destAlphaTexture=nullptr;
	m_map=nullptr;
	m_depthFade.X = 0.0f;
	m_depthFade.Y = 0.0f;
	m_depthFade.Z = 0.0f;
	m_useDepthFade = false;
	m_disableTextures = false;
	TheTerrainRenderObject = this;

	m_treeBuffer = nullptr;
	m_propBuffer = nullptr;
	m_bibBuffer = nullptr;
	m_bridgeBuffer = nullptr;
	m_waypointBuffer = nullptr;
#ifdef DO_ROADS
	m_roadBuffer = nullptr;
#endif
#ifdef DO_SCORCH
	m_vertexScorch = nullptr;
	m_indexScorch = nullptr;
	m_scorchTexture = nullptr;
	clearAllScorches();
	m_shroud = nullptr;
#endif
	m_bridgeBuffer = NEW W3DBridgeBuffer;

	if (TheGlobalData->m_headless)
		return;

	m_treeBuffer = NEW W3DTreeBuffer;

	m_propBuffer = NEW W3DPropBuffer;

	m_bibBuffer = NEW W3DBibBuffer;

	m_curImpassableSlope = 45.0f;	// default to 45 degrees.
	m_waypointBuffer = NEW W3DWaypointBuffer;
#ifdef DO_ROADS
	m_roadBuffer = NEW W3DRoadBuffer;
#endif
#if ENABLE_CONFIGURABLE_SHROUD
	if (TheGlobalData->m_shroudOn)
		m_shroud = NEW W3DShroud;
#else
	m_shroud = NEW W3DShroud;
#endif
	DX8Wrapper::SetCleanupHook(this);
}

void BaseHeightMapRenderObjClass::setTextureLOD(Int lod)
{
	if (m_treeBuffer)
		m_treeBuffer->setTextureLOD(lod);
	if (m_map)
		m_map->setTextureLOD(lod);
}

//=============================================================================
// BaseHeightMapRenderObjClass::adjustTerrainLOD
//=============================================================================
/** Adjust the terrain Level Of Detail.  If adj > 0 , increases LOD 1 step, if
adj < 0 decreases it one step, if adj==0, then just sets up for the current LOD */
//=============================================================================
void BaseHeightMapRenderObjClass::adjustTerrainLOD(Int adj)
{
	if (adj>0 && TheGlobalData->m_terrainLOD<TERRAIN_LOD_MAX) TheWritableGlobalData->m_terrainLOD=(TerrainLOD)(TheGlobalData->m_terrainLOD+1);
	if (adj<0 && TheGlobalData->m_terrainLOD>TERRAIN_LOD_MIN) TheWritableGlobalData->m_terrainLOD=(TerrainLOD)(TheGlobalData->m_terrainLOD-1);

	if (TheGlobalData->m_terrainLOD ==TERRAIN_LOD_AUTOMATIC) {
		TheWritableGlobalData->m_terrainLOD=TERRAIN_LOD_MAX;
	}

	if (m_map==nullptr) return;
	if (m_shroud)
		m_shroud->reset();	//need reset here since initHeightData will load new shroud.

	BaseHeightMapRenderObjClass *newROBJ = nullptr;
	if (TheGlobalData->m_terrainLOD==7) {
		newROBJ = TheHeightMap;
		if (newROBJ==nullptr) {
			newROBJ = NEW_REF( HeightMapRenderObjClass, () );
		}
	}	else {
		newROBJ = TheFlatHeightMap;
		if (newROBJ==nullptr) {
			newROBJ = NEW_REF( FlatHeightMapRenderObjClass, () );
		}
	}
	if (TheGlobalData->m_terrainLOD == 5)
		newROBJ = nullptr;
	RTS3DScene *pMyScene = (RTS3DScene *)Scene;
	if (pMyScene) {
		pMyScene->Remove_Render_Object(this);
		pMyScene->Unregister(this, SceneClass::ON_FRAME_UPDATE);
		// add our terrain render object to the scene
		if (newROBJ) {
			pMyScene->Add_Render_Object( newROBJ );
			pMyScene->Register(newROBJ,SceneClass::ON_FRAME_UPDATE);
		}
	}

	if (newROBJ) {
		// apply the heightmap to the terrain render object
		newROBJ->initHeightData( m_map->getDrawWidth(),
																					 m_map->getDrawHeight(),
																					 m_map,
																					 nullptr);
		TheTerrainRenderObject = newROBJ;
		newROBJ->staticLightingChanged();
		newROBJ->m_roadBuffer->loadRoads();
	}
	if (TheTacticalView) {
		TheTacticalView->forceRedraw();
	}
}

//=============================================================================
// BaseHeightMapRenderObjClass::ReleaseResources
//=============================================================================
/** Releases all w3d assets, to prepare for Reset device call. */
//=============================================================================
void BaseHeightMapRenderObjClass::ReleaseResources()
{
	if (m_treeBuffer) {
		m_treeBuffer->freeTreeBuffers();
	}
	if (m_bibBuffer) {
		m_bibBuffer->freeBibBuffers();
	}
	if (m_bridgeBuffer) {
		m_bridgeBuffer->freeBridgeBuffers();
	}

	if( m_waypointBuffer )
	{
		m_waypointBuffer->freeWaypointBuffers();
	}
	// We need to save the map.
	WorldHeightMap *pMap=nullptr;
	REF_PTR_SET(pMap, m_map);
	freeMapResources();
	m_map = pMap; // ref_ptr_set has already incremented the ref count.
	if (TheWaterRenderObj)
		TheWaterRenderObj->ReleaseResources();
	if (TheTerrainTracksRenderObjClassSystem)
		TheTerrainTracksRenderObjClassSystem->ReleaseResources();
	if (TheW3DShadowManager)
		TheW3DShadowManager->ReleaseResources();
	if (m_shroud)
	{	m_shroud->reset();
		m_shroud->ReleaseResources();
	}

	if (TheSmudgeManager)
		TheSmudgeManager->ReleaseResources();

	if (TheSnowManager)
		((W3DSnowManager *)TheSnowManager)->ReleaseResources();

	//Release any resources that may be used by custom pixel/vertex shaders
	W3DShaderManager::shutdown();
#ifdef DO_ROADS
	if (m_roadBuffer) {
		m_roadBuffer->freeRoadBuffers();
	}
#endif
}

//=============================================================================
// BaseHeightMapRenderObjClass::ReAcquireResources
//=============================================================================
/** Reallocates all W3D assets after a reset.. */
//=============================================================================
void BaseHeightMapRenderObjClass::ReAcquireResources()
{
	W3DShaderManager::init();	//reaquire resources which may be needed by custom shaders

	if (TheWaterRenderObj)
		TheWaterRenderObj->ReAcquireResources();

	if (TheTerrainTracksRenderObjClassSystem)
		TheTerrainTracksRenderObjClassSystem->ReAcquireResources();

	if (TheW3DShadowManager)
		TheW3DShadowManager->ReAcquireResources();
	if (m_shroud)
		m_shroud->ReAcquireResources();

	if (m_map)
	{
		this->initHeightData(m_x,m_y,m_map, nullptr);
		// Tell lights to update next time through.
		m_needFullUpdate = true;
	}

	if (m_treeBuffer) {
		m_treeBuffer->allocateTreeBuffers();
	}
	if (m_bibBuffer) {
		m_bibBuffer->allocateBibBuffers();
	}
	if (m_bridgeBuffer) {
		m_bridgeBuffer->allocateBridgeBuffers();
	}

	if (TheSmudgeManager)
		TheSmudgeManager->ReAcquireResources();

	if (TheSnowManager)
		((W3DSnowManager *)TheSnowManager)->ReAcquireResources();

	//Waypoint buffers are done dynamically. One line, one node (just rendered multiple times accessing other data).
	//Internally creates it if needed.

#ifdef DO_ROADS
	if (m_roadBuffer) {
		m_roadBuffer->allocateRoadBuffers();
		m_roadBuffer->loadRoads();
	}
#endif

	if (TheTacticalView) {
		TheTacticalView->forceRedraw(); //force map to update itself for the current camera position.
	}
}

//=============================================================================
// BaseHeightMapRenderObjClass::doTheLight
//=============================================================================
/** Calculates the diffuse lighting for a vertex in the terrain, taking all of the
static lights into account as well.  It is possible to just use the normal in the
vertex and let D3D do the lighting, but it is slower to render, and can only
handle 4 lights at this point. */
//=============================================================================
void BaseHeightMapRenderObjClass::doTheLight(VERTEX_FORMAT *vb, Vector3*light, Vector3*normal, RefRenderObjListIterator *pLightsIterator, UnsignedByte alpha)
{
#ifdef USE_NORMALS
	vb->nx = normal->X;
	vb->ny = normal->Y;
	vb->nz = normal->Z;
#else
	Real shadeR, shadeG, shadeB;
	Real shade;
	shadeR = TheGlobalData->m_terrainAmbient[0].red;	//only the first terrain light contributes to ambient
	shadeG = TheGlobalData->m_terrainAmbient[0].green;
	shadeB = TheGlobalData->m_terrainAmbient[0].blue;

	if (pLightsIterator) {
		for (pLightsIterator->First(); !pLightsIterator->Is_Done(); pLightsIterator->Next())
		{
			LightClass *pLight = (LightClass*)pLightsIterator->Peek_Obj();
			Vector3 lightDirection(vb->x, vb->y, vb->z);
			Real factor = 1.0f;
			switch(pLight->Get_Type()) {
			case LightClass::POINT:
			case LightClass::SPOT: {
					Vector3 lightLoc = pLight->Get_Position();
					lightDirection -= lightLoc;
					double range, midRange;
					pLight->Get_Far_Attenuation_Range(midRange, range);
					if (vb->x < lightLoc.X-range) continue;
					if (vb->x > lightLoc.X+range) continue;
					if (vb->y < lightLoc.Y-range) continue;
					if (vb->y > lightLoc.Y+range) continue;
					Real dist = lightDirection.Length();
					if (dist >= range) continue;
					if (midRange < 0.1) continue;
#if 1
					factor = 1.0f - (dist - midRange) / (range - midRange);
#else
					// f = 1.0 / (atten0 + d*atten1 + d*d/atten2);
					if (fabs(range-midRange)<1e-5)	{
						// if the attenuation range is too small assume uniform with cutoff
						factor = 1.0;
					}	else  {
						factor = 1.0f/(0.1+dist/midRange + 5.0f*dist*dist/(range*range));
					}
#endif
					factor = WWMath::Clamp(factor,0.0f,1.0f);
				}
				break;
			case LightClass::DIRECTIONAL:
				lightDirection = pLight->Get_Transform().Get_Z_Vector();
				factor = 1.0;
				break;
			};
			lightDirection.Normalize();
			Vector3 lightRay(-lightDirection.X, -lightDirection.Y, -lightDirection.Z);
			shade = Vector3::Dot_Product(lightRay, *normal);
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
	}
	// Add in global diffuse value.
	const RGBColor *terrainDiffuse;
	for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
	{
		shade = Vector3::Dot_Product(light[lightIndex], *normal);
		if (shade > 1.0) shade = 1.0;
		if(shade < 0.0f) shade = 0.0f;
		terrainDiffuse=&TheGlobalData->m_terrainDiffuse[lightIndex];
		shadeR += shade*terrainDiffuse->red;
		shadeG += shade*terrainDiffuse->green;
		shadeB += shade*terrainDiffuse->blue;
	}

	if (shadeR > 1.0) shadeR = 1.0;
	if(shadeR < 0.0f) shadeR = 0.0f;
	if (shadeG > 1.0) shadeG = 1.0;
	if(shadeG < 0.0f) shadeG = 0.0f;
	if (shadeB > 1.0) shadeB = 1.0;
	if(shadeB < 0.0f) shadeB = 0.0f;

	if (m_useDepthFade && vb->z <= TheGlobalData->m_waterPositionZ)
	{	//height is below water level
		//reduce lighting values based on light fall off as it travels through water.
		float depthScale = (1.4f - vb->z)/TheGlobalData->m_waterPositionZ;
		shadeR *= 1.0f - depthScale * (1.0f-m_depthFade.X);
		shadeG *= 1.0f - depthScale * (1.0f-m_depthFade.Y);
		shadeB *= 1.0f - depthScale * (1.0f-m_depthFade.Z);
	}

	shadeR*=255.0f;
	shadeG*=255.0f;
	shadeB*=255.0f;
	vb->diffuse = REAL_TO_INT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16) | ((Int)alpha << 24);
#endif
}

//=============================================================================
// BaseHeightMapRenderObjClass::updateMacroTexture
//=============================================================================
/** Updates the macro noise/lightmap texture (pass 3) */
//=============================================================================
void BaseHeightMapRenderObjClass::updateMacroTexture(AsciiString textureName)
{
	m_macroTextureName = textureName;
	// Release texture.
	REF_PTR_RELEASE(m_stageThreeTexture);
	// Reallocate texture.
	m_stageThreeTexture=NEW LightMapTerrainTextureClass(m_macroTextureName);
}

//=============================================================================
// BaseHeightMapRenderObjClass::reset
//=============================================================================
/** Updates the macro noise/lightmap texture (pass 3) */
//=============================================================================
void BaseHeightMapRenderObjClass::reset()
{
	if (m_treeBuffer) {
		m_treeBuffer->clearAllTrees();
	}
	if (m_propBuffer) {
		m_propBuffer->clearAllProps();
	}
	clearAllScorches();
#ifdef TEST_CUSTOM_EDGING
	m_customEdging ->clearAllEdging();
#endif
#ifdef DO_ROADS
	if (m_roadBuffer) {
		m_roadBuffer->clearAllRoads();
	}
#endif
	if (m_bridgeBuffer) {
		m_bridgeBuffer->clearAllBridges();
	}

	if (m_bibBuffer) {
		m_bibBuffer->clearAllBibs();
	}

	m_showAsVisibleCliff.clear();

	if (m_shroud)
	{	m_shroud->reset();
		m_shroud->setBorderShroudLevel((W3DShroudLevel)TheGlobalData->m_shroudAlpha);	//assume border is always black at start.
	}
}

/**@todo: Ray intersection needs to be optimized with some sort of grid-tracing
(ala line drawing).  We should also try making the search in a front->back order
relative to the ray so we can early exit as soon as we have a hit.
*
//=============================================================================
// BaseHeightMapRenderObjClass::Cast_Ray
//=============================================================================
/** Return intersection of a ray with the heightmap mesh.
This is a quick version that just checks every polygon inside
a 2D bounding rectangle of the ray projected onto the height map plane.
For most of our view-picking cases the ray is almost perpendicular to the
map plane so this is very quick (small bounding box).  But it can become slow
for arbitrary rays such as those used in AI visibility checks(2 units on
opposite corners of the map would check every polygon in the map).
*/
//=============================================================================
bool BaseHeightMapRenderObjClass::Cast_Ray(RayCollisionTestClass & raytest)
{
	TriClass tri;
	Bool hit = false;
	Int X,Y;
	Vector3 normal,P0,P1,P2,P3;
	Bool hasP0 = false;
	Bool hasP1 = false;

	if (!m_map)
		return false;	//need valid pointer to heightmap samples

	//Clip ray to extents of height map
	AABoxClass hbox;
	LineSegClass lineseg,lineseg2;
	CastResultStruct	result;
	Int startCellX = 0;
	Int startCellY = 0;
	Int endCellX = 0;
	Int endCellY = 0;
	const Int borderSize = m_map->getBorderSizeInline();
	const Int overhang = 2*VERTEX_BUFFER_TILE_LENGTH + borderSize; // Allow picking past the edge for scrolling & objects.
 	Vector3 minPt(MAP_XY_FACTOR*(-overhang), MAP_XY_FACTOR*(-overhang), -MAP_XY_FACTOR);
	Vector3 maxPt(MAP_XY_FACTOR*(m_map->getXExtent()+overhang),
		MAP_XY_FACTOR*(m_map->getYExtent()+overhang), MAP_HEIGHT_SCALE*m_map->getMaxHeightValue()+MAP_XY_FACTOR);
	MinMaxAABoxClass mmbox(minPt, maxPt);
	hbox.Init(mmbox);

	lineseg=raytest.Ray;

	Int p;
	for (p=0; p<3; p++) {
		//find intersection point of ray and terrain bounding box
		result.Reset();
		result.ComputeContactPoint=true;
		Bool newP0 = false;
		Bool newP1 = false;

		if (CollisionMath::Collide(lineseg,hbox,&result))
		{
			//ray intersects terrain or starts inside the terrain.
			if (!result.StartBad)	//check if start point inside terrain
			{
				newP0 = P0 != result.ContactPoint;
				hasP0 = true;
				P0 = result.ContactPoint;	//make intersection point the new start of the ray.
			}

			//reverse direction of original ray and clip again to extent of heightmap
			result.Fraction=1.0f;	//reset the result
			result.StartBad=false;
			lineseg2.Set(lineseg.Get_P1(),lineseg.Get_P0());	//reverse line segment
			if (CollisionMath::Collide(lineseg2,hbox,&result))
			{
				if (!result.StartBad)	//check if end point inside terrain
				{
					newP1 = P1 != result.ContactPoint;
					hasP1 = true;
					P1 = result.ContactPoint;	//make intersection point the new end point of ray
				}
			}
		}

		if (!newP0 || !newP1)
			break;

		// Take the 2D bounding box of ray and check heights
		// inside this box for intersection.
		if (P0.X > P1.X) {	//flip start/end points
			startCellX = REAL_TO_INT_FLOOR(P1.X/MAP_XY_FACTOR);
			endCellX = REAL_TO_INT_CEIL(P0.X/MAP_XY_FACTOR);
		}	else {
			startCellX = REAL_TO_INT_FLOOR(P0.X/MAP_XY_FACTOR);
			endCellX = REAL_TO_INT_CEIL(P1.X/MAP_XY_FACTOR);
		}
		if (P0.Y > P1.Y) {	//flip start/end points
			startCellY = REAL_TO_INT_FLOOR(P1.Y/MAP_XY_FACTOR);
			endCellY = REAL_TO_INT_CEIL(P0.Y/MAP_XY_FACTOR);
		}	else {
			startCellY = REAL_TO_INT_FLOOR(P0.Y/MAP_XY_FACTOR);
			endCellY = REAL_TO_INT_CEIL(P1.Y/MAP_XY_FACTOR);
		}

		Int i, j, minHt, maxHt;

		minHt = m_map->getMaxHeightValue();
		maxHt = 0;

		for (j=startCellY; j<=endCellY; j++) {
			for (i=startCellX; i<=endCellX; i++) {
				Short cur = getClipHeight(i+borderSize,j+borderSize);
				if (cur<minHt) minHt = cur;
				if (maxHt<cur) maxHt = cur;
			}
		}
		Vector3 minPt(MAP_XY_FACTOR*(startCellX-1), MAP_XY_FACTOR*(startCellY-1), MAP_HEIGHT_SCALE*(minHt-1));
		Vector3 maxPt(MAP_XY_FACTOR*(endCellX+1), MAP_XY_FACTOR*(endCellY+1), MAP_HEIGHT_SCALE*(maxHt+1));
		MinMaxAABoxClass mmbox(minPt, maxPt);
		hbox.Init(mmbox);
	}

	if (!hasP0 || !hasP1)
		return false;

	raytest.Result->ComputeContactPoint=true;	//tell CollisionMath that we need point.

	// Adjust indexes into the bordered height map.

	startCellX += borderSize;
	endCellX += borderSize;
	startCellY += borderSize;
	endCellY += borderSize;

	Int offset;
	for (offset = 1; offset < 5; offset *= 3) {
		for (Y=startCellY-offset; Y<=endCellY+offset; Y++) {

			for (X=startCellX-offset; X<=endCellX+offset; X++) {
				//test the 2 triangles in this cell
				//	3-----2
				//  |    /|
				//  |  /  |
				//	|/    |
				//  0-----1

				//bottom triangle first
				P0.X=ADJUST_FROM_INDEX_TO_REAL(X);
				P0.Y=ADJUST_FROM_INDEX_TO_REAL(Y);
				P0.Z=MAP_HEIGHT_SCALE*(float)getClipHeight(X, Y);

				P1.X=ADJUST_FROM_INDEX_TO_REAL(X+1);
				P1.Y=ADJUST_FROM_INDEX_TO_REAL(Y);
				P1.Z=MAP_HEIGHT_SCALE*(float)getClipHeight(X+1, Y);

				P2.X=ADJUST_FROM_INDEX_TO_REAL(X+1);
				P2.Y=ADJUST_FROM_INDEX_TO_REAL(Y+1);
				P2.Z=MAP_HEIGHT_SCALE*(float)getClipHeight(X+1, Y+1);

				P3.X=ADJUST_FROM_INDEX_TO_REAL(X);
				P3.Y=ADJUST_FROM_INDEX_TO_REAL(Y+1);
				P3.Z=MAP_HEIGHT_SCALE*(float)getClipHeight(X, Y+1);


				tri.V[0] = &P0;
				tri.V[1] = &P1;
				tri.V[2] = &P2;

				tri.N = &normal;

				tri.Compute_Normal();

				hit = hit || (Bool)CollisionMath::Collide(raytest.Ray, tri, raytest.Result);

				if (raytest.Result->StartBad)
					return true;

				//top triangle
				tri.V[0] = &P2;
				tri.V[1] = &P3;
				tri.V[2] = &P0;

				tri.N = &normal;

				tri.Compute_Normal();

				hit = hit || (Bool)CollisionMath::Collide(raytest.Ray, tri, raytest.Result);

				if (hit)
					raytest.Result->SurfaceType = SURFACE_TYPE_DEFAULT;	///@todo: WW3D uses this to return dirt, grass, etc.  Do we need this?
			}
			// Don't break.  It is possible to intersect 2 triangles, and the second is closer. if (hit) break;
		}
		// Don't break.  It is possible to intersect 2 triangles, and the second is closer. if (hit) break;
	}
	return hit;
}

//=============================================================================
// BaseHeightMapRenderObjClass::getHeightMapHeight
//=============================================================================
/** return the height and normal of the triangle plane containing given location within heightmap. */
//=============================================================================
Real BaseHeightMapRenderObjClass::getHeightMapHeight(Real x, Real y, Coord3D* normal) const
{

  // SORRY, KIDS
  // Had to make this function logic safe, so,
  // even though this is a renderObject, and is thus classified as client-side
  // it is responsible for reporting height map heights (for reasons I can't say)
  // but to do so safely, I a going to pass it the logical heighmap from the W3dTerrainVisual
  // yes another nosequiter. Ugh!

  // M Lorenzen

  // by doing it this way the compiler won't call getLogicHeightMap twice...
  WorldHeightMap *logicHeightMap = TheTerrainVisual?TheTerrainVisual->getLogicHeightMap():m_map;

  if ( !logicHeightMap )
  {
		if (normal)
		{
			// return a default normal pointing up
			normal->x = 0.0f;
			normal->y = 0.0f;
			normal->z = 1.0f;
		}
		return 0;
  }


	float height;

	//	3-----2
	//  |    /|
	//  |  /  |
	//	|/    |
	//  0-----1
	//Find surrounding grid points

	const Real MAP_XY_FACTOR_INV = 1.0f / MAP_XY_FACTOR;

	float xdiv = x * MAP_XY_FACTOR_INV;
	float ydiv = y * MAP_XY_FACTOR_INV;

	float ixf = FAST_REAL_FLOOR(xdiv);
	float iyf = FAST_REAL_FLOOR(ydiv);

	float fx = xdiv - ixf; //get fraction
	float fy = ydiv - iyf; //get fraction

	// since ixf & iyf are already floor'ed, we can use the fastest f->i conversion we have...
	Int	ix = fast_float2long_round(ixf) + logicHeightMap->getBorderSizeInline();
	Int	iy = fast_float2long_round(iyf) + logicHeightMap->getBorderSizeInline();
	Int xExtent = logicHeightMap->getXExtent();

	// Check for extent-3, not extent-1: we go into the next row/column of data for smoothed triangle points, so extent-1
	// goes off the end...
	if (ix > (xExtent-3) || iy > (logicHeightMap->getYExtent()-3) || iy < 1 || ix < 1)
	{
		// sample point is not on the heightmap
		if (normal)
		{
			// return a default normal pointing up
			normal->x = 0.0f;
			normal->y = 0.0f;
			normal->z = 1.0f;
		}
		return getClipHeight(ix, iy) * MAP_HEIGHT_SCALE;
	}

	const UnsignedByte* data = logicHeightMap->getDataPtr();
	int idx = ix + iy*xExtent;
	float p0 = data[idx];
	float p2 = data[idx + xExtent + 1];
	if (fy > fx) // test if we are in the upper triangle
	{
		float p3 = data[idx + xExtent];
		height = (p3 + (1.0f-fy)*(p0-p3) + fx*(p2-p3)) * MAP_HEIGHT_SCALE;
	}
	else
	{
		// we are in the lower triangle
		float p1 = data[idx + 1];
		height = (p1 + fy*(p2-p1) + (1.0f-fx)*(p0-p1)) * MAP_HEIGHT_SCALE;
	}

//  DEBUG_ASSERTCRASH( height < 30, ("SOMEBODY THINKS THE CLIENT HEIGHTMAP IS GOOD ENOUGH FOR LOGIC SAMPLING."));

	if (normal) {
		//		9		  8
		//
		//10	3-----2		7
		//	  |    /|
		//	  |  /  |
		//		|/    |
		//11	0-----1		6
		//
		//		4			5
		//Find surrounding grid points for smoothed normals.
 		int idx4 = ix + (iy-1)*xExtent;
 		int idx0 = ix + iy*xExtent;
 		int idx3 = ix + iy*xExtent+xExtent;
		int idx9 = ix + (iy+2)*xExtent;
		UnsignedByte d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11;
		d0 = data[idx0];
		d1 = data[idx0+1];
		d2 = data[idx3+1];
		d3 = data[idx3];
		d4 = data[idx4];
		d5 = data[idx4+1];
		d6 = data[idx0+2];
		d7 = data[idx3+2];
		d8 = data[idx9+1];
		d9 = data[idx9];
		d10 = data[idx3-1];
		d11 = data[idx0-1];

		Real deltaZ_X0 = d1-d11;
		Real deltaZ_X1 = d6-d0;
		Real deltaZ_X2 = d7-d3;
		Real deltaZ_X3 = d6-d0;

		Real deltaZ_Y0 = d3-d4;
		Real deltaZ_Y1 = d2-d5;
		Real deltaZ_Y2 = d8-d1;
		Real deltaZ_Y3 = d9-d0;

		// Interpolate to get the smoothed valued.
		Real deltaZ_X_Left = deltaZ_X0*(1.0f-fx) + fx*deltaZ_X3;
		Real deltaZ_X_Right = deltaZ_X1*(1.0f-fx) + fx*deltaZ_X2;
		Real deltaZ_X = deltaZ_X_Left*(1.0-fy) + fy*deltaZ_X_Right;

		Real deltaZ_Y_Left = deltaZ_Y0*(1.0f-fx) + fx*deltaZ_Y3;
		Real deltaZ_Y_Right = deltaZ_Y1*(1.0f-fx) + fx*deltaZ_Y2;
		Real deltaZ_Y = deltaZ_Y_Left*(1.0-fy) + fy*deltaZ_Y_Right;



			Vector3 l2r, n2f, normalAtTexel;
			l2r.Set(2*MAP_XY_FACTOR/MAP_HEIGHT_SCALE, 0, deltaZ_X);
			n2f.Set(0, 2*MAP_XY_FACTOR/MAP_HEIGHT_SCALE, deltaZ_Y);
			Vector3::Normalized_Cross_Product(l2r,n2f, &normalAtTexel);
			normal->x = normalAtTexel.X;
			normal->y = normalAtTexel.Y;
			normal->z = normalAtTexel.Z;

	}


	return height;
}

//=============================================================================
Bool BaseHeightMapRenderObjClass::isClearLineOfSight(const Coord3D& pos, const Coord3D& posOther) const
{
	if (m_map == nullptr)
		return false;	// doh. should not happen.

  WorldHeightMap *logicHeightMap = TheTerrainVisual?TheTerrainVisual->getLogicHeightMap():m_map;

#define DO_BRESENHAM
#ifdef DO_BRESENHAM

	/*
		this is WAY faster, though not quite as accurate... however, the inaccuracy
		is pretty minimal, so we really should force other code to live with it. (srj)
	*/
	const Real MAP_XY_FACTOR_INV = 1.0f / MAP_XY_FACTOR;

	Int borderSize = logicHeightMap->getBorderSizeInline();
	Int start_x = REAL_TO_INT_FLOOR(pos.x * MAP_XY_FACTOR_INV) + borderSize;
	Int start_y = REAL_TO_INT_FLOOR(pos.y * MAP_XY_FACTOR_INV) + borderSize;
	Int end_x = REAL_TO_INT_FLOOR(posOther.x * MAP_XY_FACTOR_INV) + borderSize;
	Int end_y = REAL_TO_INT_FLOOR(posOther.y * MAP_XY_FACTOR_INV) + borderSize;
	Int delta_x = abs(end_x - start_x);			// The difference between the x's
	Int delta_y = abs(end_y - start_y);			// The difference between the y's
	Int x = start_x;												// Start x off at the first pixel
	Int y = start_y;												// Start y off at the first pixel

	Int xinc1, xinc2;
	if (end_x >= start_x)								// The x-values are increasing
	{
		xinc1 = 1;
		xinc2 = 1;
	}
	else																// The x-values are decreasing
	{
		xinc1 = -1;
		xinc2 = -1;
	}

	Int yinc1, yinc2;
	if (end_y >= start_y)               // The y-values are increasing
	{
		yinc1 = 1;
		yinc2 = 1;
	}
	else																// The y-values are decreasing
	{
		yinc1 = -1;
		yinc2 = -1;
	}

	Int den, num, numadd, numpixels;

	Bool checkY = true;
	if (delta_x >= delta_y)							// There is at least one x-value for every y-value
	{
		xinc1 = 0;												// Don't change the x when numerator >= denominator
		yinc2 = 0;												// Don't change the y for every iteration
		den = delta_x;
		num = delta_x / 2;
		numadd = delta_y;
		numpixels = delta_x;							// There are more x-values than y-values
	}
	else																// There is at least one y-value for every x-value
	{
		checkY = false;
		xinc2 = 0;												// Don't change the x for every iteration
		yinc1 = 0;												// Don't change the y when numerator >= denominator
		den = delta_y;
		num = delta_y / 2;
		numadd = delta_x;
		numpixels = delta_y;							// There are more y-values than x-values
	}

	Real nsInv = 1.0f / numpixels;
	Real z = pos.z;
	Real dz = posOther.z - z;
	Real zinc = dz * nsInv;

	Bool result = true;
	const UnsignedByte* data = logicHeightMap->getDataPtr();
	Int xExtent = logicHeightMap->getXExtent();
	Int yExtent = logicHeightMap->getYExtent();
	for (Int curpixel = 0; curpixel < numpixels; curpixel++)
	{
		if (x < 0 ||
				y < 0 ||
				x >= xExtent-1 ||
				y >= yExtent-1)
		{
			// once we go off the map, we're done
			break;
		}

		Int idx = x + y*xExtent;
		float height = data[idx];
		height = MAX(height, (float)data[idx + 1]);
		height = MAX(height, (float)data[idx + xExtent]);
		height = MAX(height, (float)data[idx + xExtent + 1]);
		// @fix Convert raw heightmap byte to world-space Z before comparing with LOS line.
		// Without this, raw values (0-255) are compared against world Z, breaking LOS.
		height *= MAP_HEIGHT_SCALE;
		// if terrainHeight > z, we can't see, so punt.
		// add a little fudge to account for slop.
		const Real LOS_FUDGE = 0.5f;
		if (height > z + LOS_FUDGE)
		{
			result = false;
			break;
		}

		// we're above the max height of the terrain and still looking up, so we're done.
		// (don't bother for reverse test, since that doesn't generally happen)
		if (z >= getMaxHeight() && zinc > 0.0f)
		{
			break;
		}

		z += zinc;

		// continue with the maintenance.
		num += numadd;										// Increase the numerator by the top of the fraction
		if (num >= den)										// Check if numerator >= denominator
		{
			num -= den;											// Calculate the new numerator value
			x += xinc1;											// Change the x as appropriate
			y += yinc1;											// Change the y as appropriate
		}
		x += xinc2;												// Change the x as appropriate
		y += yinc2;												// Change the y as appropriate
	}

	return result;

#else

	// walk a line from obj to objOther and
	// find the highest point in between 'em. while
	// we're doing this, also estimate the point on the
	// line at the same x,y as the high-terrain-point.

	Real fx = pos.x;
	Real fy = pos.y;
	Real fz = pos.z;
	Real fdx = posOther.x - fx;
	Real fdy = posOther.y - fy;
	Real fdz = posOther.z - fz;

	// What's the largest step size that will be accurate enough?
	// Currently we use a step size of about 2 "feet", which
	// seems acceptable accuracy. If performance here is inadequate,
	// we can try increasing the step size, but be sure to retest
	// accuracy.
	Real len = ceilf(sqrtf(fdx*fdx + fdy*fdy));
	const Real STEP_LEN = 2.0f;
	Int numSteps = REAL_TO_INT_CEIL(len / STEP_LEN);
	if (numSteps < 1) numSteps = 1;
	Real fnsInv = 1.0f / numSteps;
	Real fxinc = fdx * fnsInv;
	Real fyinc = fdy * fnsInv;
	Real fzinc = fdz * fnsInv;
	while (numSteps--)
	{
		Real terrainHeight = getHeightMapHeight( fx, fy, nullptr );

		// if terrainHeight > fz, we can't see, so punt.
		// add a little fudge to account for slop.
		const Real LOS_FUDGE = 0.5f;
		if (terrainHeight > fz + LOS_FUDGE)
		{
			return false;
		}

		// we're above the max height of the terrain and still looking up, so we're done.
		// (don't bother for reverse test, since that doesn't generally happen)
		if (fz >= getMaxHeight() && fzinc > 0.0f)
		{
			return true;
		}

		fx += fxinc;
		fy += fyinc;
		fz += fzinc;

	}

	return true;
#endif
}

//=============================================================================
// BaseHeightMapRenderObjClass::getMaxCellHeight
//=============================================================================
/** Returns maximum height of the 4 corners containing the given point */
//=============================================================================
Real BaseHeightMapRenderObjClass::getMaxCellHeight(Real x, Real y) const
{
	float p0,p1,p2,p3;
	float height;

	//	3-----2
	//  |    /|
	//  |  /  |
	//	|/    |
	//  0-----1
	//Find surrounding grid points

	if (m_map == nullptr)
	{	//sample point is not on the heightmap
		return 0.0f;	//return default height
	}

  WorldHeightMap *logicHeightMap = TheTerrainVisual?TheTerrainVisual->getLogicHeightMap():m_map;


	Int offset = 1;
	Int iX = x/MAP_XY_FACTOR;
	Int iY = y/MAP_XY_FACTOR;
	iX += logicHeightMap->getBorderSizeInline();
	iY += logicHeightMap->getBorderSizeInline();
	if (iX<0) iX = 0;
	if (iY<0) iY = 0;
	if (iX >= (logicHeightMap->getXExtent()-1)) {
		iX = logicHeightMap->getXExtent()-2;
	}
	if (iY >= (logicHeightMap->getYExtent()-1)) {
		iY = logicHeightMap->getYExtent()-2;
	}
	UnsignedByte *data = logicHeightMap->getDataPtr();
	p0=data[iX+iY*logicHeightMap->getXExtent()]*MAP_HEIGHT_SCALE;
	p1=data[(iX+offset)+iY*logicHeightMap->getXExtent()]*MAP_HEIGHT_SCALE;
	p2=data[(iX+offset)+(iY+offset)*logicHeightMap->getXExtent()]*MAP_HEIGHT_SCALE;
	p3=data[iX+(iY+offset)*logicHeightMap->getXExtent()]*MAP_HEIGHT_SCALE;

	height=p0;
	// GeneralsX @bugfix BenderAI 13/02/2026 Replace __max with MAX (fighter19 pattern)
	height=MAX(height,p1);
	height=MAX(height,p2);
	height=MAX(height,p3);

	return height;
}

//=============================================================================
// BaseHeightMapRenderObjClass::isCliffCell
//=============================================================================
/** Returns true if the cell containing the point is a cliff cell */
//=============================================================================
Bool BaseHeightMapRenderObjClass::isCliffCell(Real x, Real y)
{

	if (m_map == nullptr)
	{	//sample point is not on the heightmap
		return false;
	}

  WorldHeightMap *logicHeightMap = TheTerrainVisual?TheTerrainVisual->getLogicHeightMap():m_map;

	Int iX = x/MAP_XY_FACTOR;
	Int iY = y/MAP_XY_FACTOR;
	iX += logicHeightMap->getBorderSizeInline();
	iY += logicHeightMap->getBorderSizeInline();
	if (iX<0) iX = 0;
	if (iY<0) iY = 0;
	if (iX >= (logicHeightMap->getXExtent()-1)) {
		iX = logicHeightMap->getXExtent()-2;
	}
	if (iY >= (logicHeightMap->getYExtent()-1)) {
		iY = logicHeightMap->getYExtent()-2;
	}
	return logicHeightMap->getCliffState(iX, iY);
}

//=============================================================================
//=============================================================================
Bool BaseHeightMapRenderObjClass::showAsVisibleCliff(Int xIndex, Int yIndex) const
{
	if (m_map == nullptr)
	{
		return false;
	}

	Int xSize = m_map->getXExtent();

	return m_showAsVisibleCliff[xIndex + yIndex * xSize];
}

//=============================================================================
//=============================================================================
Bool BaseHeightMapRenderObjClass::evaluateAsVisibleCliff(Int xIndex, Int yIndex, Real valuesGreaterThanRad)
{
	// This one never changes, so don't bother recomputing it.
	static const Real distance[4] =
	{
		0.0f,
		1.0 * MAP_XY_FACTOR,
		sqrt(2.0f) * MAP_XY_FACTOR,
		1.0 * MAP_XY_FACTOR,
	};

	// Note: getHeight will protect us from going out of bounds by returning 0 if we give it
	// a value outside of its bounds.
	UnsignedByte bytes[4] =
	{
		m_map->getHeight(xIndex + 0, yIndex + 0),
		m_map->getHeight(xIndex + 1, yIndex + 0),
		m_map->getHeight(xIndex + 1, yIndex + 1),
		m_map->getHeight(xIndex + 0, yIndex + 1),
	};

	Real heights[4] =
	{
		INT_TO_REAL(bytes[0]) * MAP_HEIGHT_SCALE,
		INT_TO_REAL(bytes[1]) * MAP_HEIGHT_SCALE,
		INT_TO_REAL(bytes[2]) * MAP_HEIGHT_SCALE,
		INT_TO_REAL(bytes[3]) * MAP_HEIGHT_SCALE,
	};

	Bool anyImpassable = FALSE;

	for (Int i = 1; i < 4 && !anyImpassable; ++i) {
		if (fabs((heights[i] - heights[0]) / distance[i]) > valuesGreaterThanRad) {
			anyImpassable = TRUE;
		}
	}

	return anyImpassable;
}

//=============================================================================
// BaseHeightMapRenderObjClass::Get_Obj_Space_Bounding_Sphere
//=============================================================================
/** WW3D method that returns object bounding sphere used in frustum culling*/
//=============================================================================
void BaseHeightMapRenderObjClass::Get_Obj_Space_Bounding_Sphere(SphereClass & sphere) const
{
	Int x = 0; Int y = 0;
	if (m_map) {
		x = m_map->getXExtent();
		y = m_map->getYExtent();
	}
	Vector3	ObjSpaceCenter((float)x*0.5f*MAP_XY_FACTOR,(float)y*0.5f*MAP_XY_FACTOR,(float)m_minHeight+(m_maxHeight-m_minHeight)*0.5f);
	float length = ObjSpaceCenter.Length();

	if (m_map) {
		ObjSpaceCenter.X += m_map->getDrawOrgX()*MAP_XY_FACTOR;
		ObjSpaceCenter.Y += m_map->getDrawOrgY()*MAP_XY_FACTOR;
	}
	sphere.Init(ObjSpaceCenter, length);
}

//=============================================================================
// BaseHeightMapRenderObjClass::Get_Obj_Space_Bounding_Box
//=============================================================================
/** WW3D method that returns object bounding box used in collision detection*/
//=============================================================================
void BaseHeightMapRenderObjClass::Get_Obj_Space_Bounding_Box(AABoxClass & box) const
{
	Int x = 0; Int y = 0;
	if (m_map) {
		x = m_map->getXExtent();
		y = m_map->getYExtent();
	}
	Vector3	minPt(0,0,m_minHeight);
	Vector3	maxPt((float)x*MAP_XY_FACTOR,(float)y*MAP_XY_FACTOR,(float)m_maxHeight);
	MinMaxAABoxClass minMaxBox(minPt, maxPt);

	box.Init(minMaxBox);
}

//-------------------------------------------------------------------------------------------------
/** Get the 3D extent of the terrain visible through the camera.  Return value
	is false if no part of terrain is visible.  This function returns a worse
	case bounding volume based on lowest/highest points in entire terrain.  It
	does not optimize the volume to heights actually visible.  Unlike some of
	the other methods, this function is guaranteed not to miss any visible
	polygons.  The ignoreMaxHeight flag is used to return a box that uses the
	camera position as the maximum height instead of the terrain - good for getting
	a volume enclosing things that can float above terrain.
 */
//-------------------------------------------------------------------------------------------------
Bool BaseHeightMapRenderObjClass::getMaximumVisibleBox(const FrustumClass &frustum, AABoxClass *box, Bool ignoreMaxHeight)
{
	//create a plane from the lowest point on the terrain
	PlaneClass	groundPlane(Vector3(0,0,1), m_minHeight);

	//clip each side of the view frustum to ground plane
	float clipFraction;
	Vector3 ClippedCorners[8];
	ClippedCorners[0]=frustum.Corners[0];
	for (Int i=0; i<4; i++)
	{	ClippedCorners[i]=frustum.Corners[i];
		if (groundPlane.Compute_Intersection(frustum.Corners[i],frustum.Corners[i+4],&clipFraction) == PlaneClass::INSIDE_SEGMENT)
		{	//edge intersects the terrain
			ClippedCorners[i+4]=frustum.Corners[i]+(frustum.Corners[i+4]-frustum.Corners[i])*clipFraction;
		}
		else
			ClippedCorners[i+4]=frustum.Corners[i+4];
	}

	if (box)
		box->Init(ClippedCorners,8);

	return TRUE;
}

//=============================================================================
// BaseHeightMapRenderObjClass::Class_ID
//=============================================================================
/** returns the class id, so the scene can tell what kind of render object it has. */
//=============================================================================
Int BaseHeightMapRenderObjClass::Class_ID() const
{
	return RenderObjClass::CLASSID_TILEMAP;
}

//=============================================================================
// BaseHeightMapRenderObjClass::Clone
//=============================================================================
/** Not used, but required virtual method. */
//=============================================================================
RenderObjClass *	 BaseHeightMapRenderObjClass::Clone() const
{
	assert(false);
	return nullptr;
}

//=============================================================================
// BaseHeightMapRenderObjClass::loadRoadsAndBridges
//=============================================================================
/** Loads the roads from the map objects. */
//=============================================================================
void BaseHeightMapRenderObjClass::loadRoadsAndBridges(W3DTerrainLogic *pTerrainLogic, Bool saveGame)
{
	if (DX8Wrapper::_Get_D3D_Device8() && (DX8Wrapper::_Get_D3D_Device8()->TestCooperativeLevel()) != D3D_OK)
		return;	//device not ready to render anything

#ifdef DO_ROADS
	if (m_roadBuffer) {
		m_roadBuffer->loadRoads();
	}
#endif
	if (m_bridgeBuffer) {
		m_bridgeBuffer->loadBridges(pTerrainLogic, saveGame);
	}
}

// ============================================================================
// BaseHeightMapRenderObjClass::worldBuilderUpdateBridgeTowers
// ============================================================================
/** The worldbuilder has it's own method here to update the visual representation
	* of the bridge towers */
// ============================================================================
void BaseHeightMapRenderObjClass::worldBuilderUpdateBridgeTowers( W3DAssetManager *assetManager,
																															SimpleSceneClass *scene )
{

	if( m_bridgeBuffer )
		m_bridgeBuffer->worldBuilderUpdateBridgeTowers( assetManager, scene );

}

void BaseHeightMapRenderObjClass::setShoreLineDetail()
{
	if (!m_map)
		return;

	Int m_mapDX=m_map->getXExtent();
	Int m_mapDY=m_map->getYExtent();

	//Find all shoreline tiles so they can get extra alpha blend
	updateShorelineTiles(0,0,m_mapDX-1,m_mapDY-1,m_map);
}

/** This is an extra pre-process of shoreline data to make it more efficient for runtime culling.  It is not
used by the world builder since it modifies the terrain too frequently.  The function also assumes that updateShoreLineTiles()
was previously called and inserted the tiles in the correctly sorted order.
WARNING!!! Current version assumes we always sort the entire map!  So don't set parameters to partial updates! */
void BaseHeightMapRenderObjClass::recordShoreLineSortInfos()
{
	if (TheGlobalData->m_isWorldBuilder || !m_shoreLineTilePositions || !m_map)
		return;	//we must be in the builder so don't sort.

	//Find how many sortinfos we need.
	Int shoreLineSortInfosSize = m_map->getXExtent()-1;
	Bool shoreLineSortInfosXMajor=TRUE;

	if (shoreLineSortInfosSize <= (m_map->getYExtent()-1))
	{	shoreLineSortInfosSize = m_map->getYExtent()-1;
		shoreLineSortInfosXMajor=FALSE;
	}

	m_shoreLineSortInfosXMajor= shoreLineSortInfosXMajor;

	//Check if we need to allocate memory
	if (!m_shoreLineSortInfos || shoreLineSortInfosSize > m_shoreLineSortInfosSize)
	{
		delete [] m_shoreLineSortInfos;	//old buffer was too small.

		//Find the major map axis (having the most tiles).
		m_shoreLineSortInfosSize = shoreLineSortInfosSize;

		m_shoreLineSortInfos = NEW shoreLineTileSortInfo[m_shoreLineSortInfosSize];
	}

	//Clear the sort infos
	memset(m_shoreLineSortInfos,0,sizeof(shoreLineTileSortInfo)*m_shoreLineSortInfosSize);

	if (m_shoreLineSortInfosXMajor)	//map is wider than taller, so tiles are already sorted by x
	{
		//scan all the tiles and record batches of tiles using same x value.
		m_shoreLineTileSortMaxCoordinate=m_shoreLineTileSortMinCoordinate=(m_shoreLineTilePositions[0].m_xy & 0xffff);
		for (Int i=0; i<m_numShoreLineTiles; i++)
		{
			Int x=(m_shoreLineTilePositions[i].m_xy & 0xffff);

			shoreLineTileSortInfo *sortInfo=&m_shoreLineSortInfos[x];

			if (x > m_shoreLineTileSortMaxCoordinate)
				m_shoreLineTileSortMaxCoordinate=x;

			//find number of tiles in a row using same y coordinate.
			Int j=i+1;
			Int minY,maxY;
			minY=maxY=m_shoreLineTilePositions[i].m_xy >> 16;

			while ((m_shoreLineTilePositions[j].m_xy & 0xffff) == x && j < m_numShoreLineTiles)
			{	//keep track of highest y coordinate.
				Int y = m_shoreLineTilePositions[j].m_xy >> 16;
				if (y > maxY)
					maxY=y;
				j++;
			}

			sortInfo->tileStartIndex=i;
			sortInfo->numTiles=j-i;
			sortInfo->minTileCoordinate=minY;
			sortInfo->maxTileCoordinate=maxY;
			i += sortInfo->numTiles-1;	//skip tiles we just scanned.
		}
	}
	else	//map is taller than wider so tiles are already sorted by y
	{
		//scan all the tiles and record batches of tiles using same y value.
		m_shoreLineTileSortMaxCoordinate=m_shoreLineTileSortMinCoordinate=(m_shoreLineTilePositions[0].m_xy >> 16);
		for (Int i=0; i<m_numShoreLineTiles; i++)
		{
			Int y=(m_shoreLineTilePositions[i].m_xy >> 16);

			shoreLineTileSortInfo *sortInfo=&m_shoreLineSortInfos[y];

			if (y > m_shoreLineTileSortMaxCoordinate)
				m_shoreLineTileSortMaxCoordinate=y;

			//find number of tiles in a row using same y coordinate.
			Int j=i+1;
			Int minX,maxX;
			minX=maxX=m_shoreLineTilePositions[i].m_xy & 0xffff;

			while ((m_shoreLineTilePositions[j].m_xy >> 16) == y && j < m_numShoreLineTiles)
			{	//keep track of highest x coordinate.
				Int x = m_shoreLineTilePositions[j].m_xy & 0xffff;
				if (x > maxX)
					maxX=x;
				j++;
			}

			sortInfo->tileStartIndex=i;
			sortInfo->numTiles=j-i;
			sortInfo->minTileCoordinate=minX;
			sortInfo->maxTileCoordinate=maxX;
			i += sortInfo->numTiles-1;	//skip tiles we just scanned.
		}
	}
}

void BaseHeightMapRenderObjClass::updateShorelineTile(Int i, Int j, Int border, WorldHeightMap *pMap)
{
	Int waterSide;
	Real waterZ0,waterZ1,waterZ2,waterZ3;
	Real terrainZ0, terrainZ1, terrainZ2, terrainZ3;

	//Figure out maximum depth of water before we reach the m_minWaterOpacity value.  Depths greater than this don't need
	//custom shoreline tiles because they will get their opacity from the default value stored in the frame buffer during
	//a screen clear operation.
	Real transparentDepth=TheWaterTransparency->m_transparentWaterDepth*TheWaterTransparency->m_minWaterOpacity;
	Real depthScaleFactor = 1.0f/transparentDepth;

	Real X0=(i-border)*MAP_XY_FACTOR;
	Real Y0=(j-border)*MAP_XY_FACTOR;
	waterSide=(waterZ0=TheWaterRenderObj->getWaterHeight(X0,Y0)) > ((terrainZ0=MAP_HEIGHT_SCALE*pMap->getHeight(i,j)));
	Real X1=(i-border+1)*MAP_XY_FACTOR;
	Real Y1=(j-border+1)*MAP_XY_FACTOR;
	waterSide |=((waterZ1=TheWaterRenderObj->getWaterHeight(X1,Y0)) > ((terrainZ1=MAP_HEIGHT_SCALE*pMap->getHeight(i+1,j)))) << 1;
	waterSide |=((waterZ2=TheWaterRenderObj->getWaterHeight(X1,Y1)) > ((terrainZ2=MAP_HEIGHT_SCALE*pMap->getHeight(i+1,j+1)))) << 2;
	waterSide |=((waterZ3=TheWaterRenderObj->getWaterHeight(X0,Y1)) > ((terrainZ3=MAP_HEIGHT_SCALE*pMap->getHeight(i,j+1)))) << 3;

	if (!waterSide || (waterZ0*waterZ1*waterZ2*waterZ3) <= 0)
		return;	//all verts are on positive (surface) side of water so don't need blending.  Or one of them is outside the water plane bounds (waterHeight <= 0!)

	//Check if mix of under/over water vertices or some vertices within depth fade region.
	if (waterSide < 0xf || (waterZ0 - terrainZ0) < transparentDepth ||
		(waterZ1 - terrainZ1) < transparentDepth || (waterZ2 - terrainZ2) < transparentDepth
		|| (waterZ3 - terrainZ3) < transparentDepth)
	{	//add tile to set that needs shoreline blending.
		if (m_numShoreLineTiles >= m_shoreLineTilePositionsSize)
		{	//no more room to store extra blend tiles so enlarge the buffer.
			shoreLineTileInfo *tempPositions=NEW shoreLineTileInfo[m_shoreLineTilePositionsSize+512];
			memcpy(tempPositions, m_shoreLineTilePositions, m_shoreLineTilePositionsSize*sizeof(shoreLineTileInfo));
			delete [] m_shoreLineTilePositions;
			//enlarge by more tiles to reduce memory trashing
			m_shoreLineTilePositions = tempPositions;
			m_shoreLineTilePositionsSize += 512;
		}
		//Pack x and y position into single integer since maps are limited in size
		shoreLineTileInfo *shoreInfo=&m_shoreLineTilePositions[m_numShoreLineTiles];
		shoreInfo->m_xy=i | (j <<16);
		shoreInfo->verts[0]=X0;	shoreInfo->verts[1]=Y0; shoreInfo->verts[2]=terrainZ0;
		shoreInfo->t0=(waterZ0 - terrainZ0)*depthScaleFactor;
		shoreInfo->verts[3]=X1;	shoreInfo->verts[4]=Y0; shoreInfo->verts[5]=terrainZ1;
		shoreInfo->t1=(waterZ1 - terrainZ1)*depthScaleFactor;
		shoreInfo->verts[6]=X1;	shoreInfo->verts[7]=Y1; shoreInfo->verts[8]=terrainZ2;
		shoreInfo->t2=(waterZ2 - terrainZ2)*depthScaleFactor;
		shoreInfo->verts[9]=X0;	shoreInfo->verts[10]=Y1; shoreInfo->verts[11]=terrainZ3;
		shoreInfo->t3=(waterZ3 - terrainZ3)*depthScaleFactor;

		m_numShoreLineTiles++;
	}
}

/**Scan through our map and record all tiles which cross a water plane and are within visible depth under
water.*/
void BaseHeightMapRenderObjClass::updateShorelineTiles(Int minX, Int minY, Int maxX, Int maxY, WorldHeightMap *pMap)
{
	Int border = pMap->getBorderSizeInline();

	//Clamp region to valid terrain tiles
	if (minX<0)
		minX = 0;
	if (minY<0)
		minY = 0;
	if (maxX > (pMap->getXExtent() - 1))
		maxX = (pMap->getXExtent() - 1);
	if (maxY > (pMap->getYExtent() - 1))
		maxY = (pMap->getYExtent() - 1);

	if (!m_shoreLineTilePositions)
	{	//Need to allocate memory
		m_shoreLineTilePositions = NEW shoreLineTileInfo[DEFAULT_MAX_MAP_SHORELINE_TILES];
		m_shoreLineTilePositionsSize = DEFAULT_MAX_MAP_SHORELINE_TILES;
	}

	//First remove any existing extra blend tiles within this partial region
	Int j=0;
	for (; j<m_numShoreLineTiles; j++)
	{	Int x = m_shoreLineTilePositions[j].m_xy & 0xffff;
		Int y = m_shoreLineTilePositions[j].m_xy >> 16;
		if (x >= minX && x < maxX &&
			y >= minY && y < maxY)
		{	//this tile is inside region being updated so remove it by shifting tile array
			memcpy(m_shoreLineTilePositions+j,m_shoreLineTilePositions+j+1,(m_numShoreLineTiles-1-j)*sizeof(shoreLineTileInfo));
			m_numShoreLineTiles--;
			j--;	//look at current tile again since it was removed.
		}
	}

	if (TheWaterTransparency->m_transparentWaterDepth == 0 || !TheGlobalData->m_showSoftWaterEdge)
		return;

	//we want to add the tiles in a certain order to make culling faster
	//by default they are inserted in x-order
	Bool shoreLineSortInfosXMajor=FALSE;
	if (!TheGlobalData->m_isWorldBuilder)	//we're in the game, not the builder
	{
		if ((m_map->getXExtent()-1) > (m_map->getYExtent()-1))
			shoreLineSortInfosXMajor=TRUE;
	}

	if (shoreLineSortInfosXMajor)
		for (Int i=minX; i<maxX; i++)
			for (j=minY; j<maxY; j++)
			{
				updateShorelineTile(i,j,border,pMap);
			}
	else
		for (j=minY; j<maxY; j++)
			for (Int i=minX; i<maxX; i++)
			{
				updateShorelineTile(i,j,border,pMap);
			}

	recordShoreLineSortInfos();
}

/** Generate a lookup table for arbitrary angled impassable area viewing. */
void BaseHeightMapRenderObjClass::updateViewImpassableAreas(Bool partial, Int minX, Int maxX, Int minY, Int maxY)
{
	Int xSize = m_map->getXExtent();
	Int ySize = m_map->getYExtent();
	if (m_showAsVisibleCliff.size() != xSize * ySize) {
		m_showAsVisibleCliff.resize(xSize * ySize);
	}

	if (!partial) {
		minX = 0;
		minY = 0;
		maxX = xSize;
		maxY = ySize;
	}

	// save calculating the tangent over and over again.
	Real tanImpassableRad = tan(m_curImpassableSlope / 360.f * 2 * PI);
	for (Int j = minY; j < maxY; ++j) {
		for (Int i = minX; i < maxX; ++i) {
			m_showAsVisibleCliff[i + j * xSize] = evaluateAsVisibleCliff(i, j, tanImpassableRad);
		}
	}
}

/** Generate a lookup table which can be used to generate an
alpha value from a given set of uv coordinates.  Currently used
for smoothing water/terrain border*/
void BaseHeightMapRenderObjClass::initDestAlphaLUT()
{
	if (!m_destAlphaTexture)
		return;

	SurfaceClass *surf=m_destAlphaTexture->Get_Surface_Level();

	if (surf)
	{
		Int pitch;
		UnsignedInt *pData=(UnsignedInt*)surf->Lock(&pitch);

		Int maxOpacity=(Int)(TheWaterTransparency->m_minWaterOpacity * 255.0f);
		Int alpha;

		if (pData)
		{
			//Fill texture with alpha gradient
			for (Int x=0; x<256; x++)
			{
				alpha = x;
				if (alpha > maxOpacity)
					alpha = maxOpacity;
				*pData=(alpha<<24)|0x00ffffff;
				pData++;
			}
			surf->Unlock();
		}

		m_destAlphaTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		m_destAlphaTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		REF_PTR_RELEASE(surf);
		m_currentMinWaterOpacity = TheWaterTransparency->m_minWaterOpacity;
	}
}

//=============================================================================
// BaseHeightMapRenderObjClass::initHeightData
//=============================================================================
/** Allocate a heightmap of x by y vertices and fill with initial height values.
Also allocates all rendering resources such as vertex buffers, index buffers,
shaders, and materials.*/
//=============================================================================
Int BaseHeightMapRenderObjClass::initHeightData(Int x, Int y, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIteratork, Bool updateExtraPassTiles)
{

	REF_PTR_SET(m_map, pMap);	//update our heightmap pointer in case it changed since last call.

	if (m_shroud)
		m_shroud->init(m_map,TheGlobalData->m_partitionCellSize,TheGlobalData->m_partitionCellSize);
#ifdef DO_ROADS
	if (m_roadBuffer)
		m_roadBuffer->setMap(m_map);
#endif
	HeightSampleType *data = nullptr;
	if (pMap) {
		data = pMap->getDataPtr();
	}

	if (m_treeBuffer) {
		Region2D bounds;
		bounds.lo.x = 0;
		bounds.lo.y = 0;
		bounds.hi.x = (pMap->getXExtent() - 2*pMap->getBorderSize()) *MAP_XY_FACTOR;
		bounds.hi.y = (pMap->getYExtent() - 2*pMap->getBorderSize()) *MAP_XY_FACTOR;
		m_treeBuffer->setBounds(bounds);
	}

	if (updateExtraPassTiles)
	{
		m_numShoreLineTiles = 0;
		//Do some preprocessing on map to extract useful data
		if (pMap)
		{
			//Find min/max values for all terrain heights, useful for rendering optimization
			Int m_mapDX=pMap->getXExtent();
			Int m_mapDY=pMap->getYExtent();
			Int i, j, minHt, maxHt;

			minHt = pMap->getMaxHeightValue();
			maxHt = 0;

			for (j=0; j<m_mapDY; j++) {
				for (i=0; i<m_mapDX; i++) {
					Short cur = pMap->getHeight(i,j);
					if (cur<minHt) minHt = cur;
					if (maxHt<cur) maxHt = cur;
				}
			}
			m_minHeight = minHt * MAP_HEIGHT_SCALE;
			m_maxHeight = maxHt * MAP_HEIGHT_SCALE;

			//Find all shoreline tiles so they can get extra alpha blend
			updateShorelineTiles(0,0,m_mapDX-1,m_mapDY-1,pMap);
			if (TheWaterTransparency->m_minWaterOpacity != m_currentMinWaterOpacity)
				initDestAlphaLUT();
		}
	}

	Set_Force_Visible(TRUE);	//terrain is always visible.
	m_needFullUpdate = true;

	m_scorchesInBuffer = 0;
	m_curNumScorchVertices=0;
	m_curNumScorchIndices=0;
	// If the textures aren't allocated (usually because of a hardware reset) need to allocate.
	Bool needToAllocate = false;
	if (m_stageTwoTexture == nullptr && m_treeBuffer) {
		needToAllocate = true;
	}
	if (data && needToAllocate)
	{	//requested heightmap different from old one.
		//allocate a new one.
		freeMapResources();	//free old data and ib/vb
		REF_PTR_SET(m_map,pMap);	//update our heightmap pointer in case it changed since last call.
		m_stageTwoTexture=NEW CloudMapTerrainTextureClass;
		m_stageThreeTexture=NEW LightMapTerrainTextureClass(m_macroTextureName);
		m_destAlphaTexture=MSGNEW("TextureClass") TextureClass(256,1,WW3D_FORMAT_A8R8G8B8,MIP_LEVELS_1);
		initDestAlphaLUT();
#ifdef DO_SCORCH
		allocateScorchBuffers();
#endif

		m_vertexMaterialClass=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);

		m_shaderClass = detailOpaqueShader;	//		ShaderClass::_PresetOpaqueShader;
	}

	return 0;
}

#ifdef DO_SCORCH
//=============================================================================
// BaseHeightMapRenderObjClass::freeScorchBuffers
//=============================================================================
/** Frees the vertex buffers for scorches.*/
//=============================================================================
void BaseHeightMapRenderObjClass::freeScorchBuffers()
{
	REF_PTR_RELEASE(m_vertexScorch);
	REF_PTR_RELEASE(m_indexScorch);
	REF_PTR_RELEASE(m_scorchTexture);
}

//=============================================================================
// BaseHeightMapRenderObjClass::allocateScorchBuffers
//=============================================================================
/** Allocates the vertex buffer and texture for scorches.*/
//=============================================================================
void BaseHeightMapRenderObjClass::allocateScorchBuffers()
{
	m_vertexScorch=NEW_REF(DX8VertexBufferClass,(DX8_FVF_XYZDUV1,MAX_SCORCH_VERTEX,DX8VertexBufferClass::USAGE_DEFAULT));
	m_indexScorch=NEW_REF(DX8IndexBufferClass,(MAX_SCORCH_INDEX));
	m_scorchTexture=NEW ScorchTextureClass;
	m_scorchesInBuffer = 0; // If we just allocated the buffers, we got no scorches in the buffer.
	m_curNumScorchVertices=0;
	m_curNumScorchIndices=0;
#ifdef RTS_DEBUG
	Vector3 loc(4*MAP_XY_FACTOR,4*MAP_XY_FACTOR,0);
	addScorch(loc, 1*MAP_XY_FACTOR, SCORCH_1);
	loc.Y += 10*MAP_XY_FACTOR;
	loc.X += 5*MAP_XY_FACTOR;
	addScorch(loc, 3*MAP_XY_FACTOR, SCORCH_1);
#endif

}

//=============================================================================
// BaseHeightMapRenderObjClass::updateScorches
//=============================================================================
/** Builds the vertex buffer data for drawing the scorches.*/
//=============================================================================
void BaseHeightMapRenderObjClass::updateScorches()
{
	if (m_scorchesInBuffer > 1) {
		return;
	}
	if (m_numScorches==0) {
		return;
	}
	if (!m_indexScorch || !m_vertexScorch) {
		return;
	}
	m_curNumScorchVertices = 0;
	m_curNumScorchIndices = 0;
	DX8IndexBufferClass::WriteLockClass lockIdxBuffer(m_indexScorch);
	UnsignedShort *ib=lockIdxBuffer.Get_Index_Array();
	UnsignedShort *curIb = ib;

	DX8VertexBufferClass::WriteLockClass lockVtxBuffer(m_vertexScorch);
	VertexFormatXYZDUV1 *vb = (VertexFormatXYZDUV1*)lockVtxBuffer.Get_Vertex_Array();
	VertexFormatXYZDUV1 *curVb = vb;

	Int curScorch;
	Real shadeR, shadeG, shadeB;
	shadeR = TheGlobalData->m_terrainAmbient[0].red;
	shadeG = TheGlobalData->m_terrainAmbient[0].green;
	shadeB = TheGlobalData->m_terrainAmbient[0].blue;
	shadeR += TheGlobalData->m_terrainDiffuse[0].red/2;
	shadeG += TheGlobalData->m_terrainDiffuse[0].green/2;
	shadeB += TheGlobalData->m_terrainDiffuse[0].blue/2;
	shadeR*=255.0f;
	shadeG*=255.0f;
	shadeB*=255.0f;
	Int diffuse=REAL_TO_INT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16) | ((int)255 << 24);
	m_scorchesInBuffer = 0;
	for (curScorch=m_numScorches-1; curScorch>=0; curScorch--) {
		m_scorchesInBuffer++;
		Real radius = m_scorches[curScorch].radius;
		Vector3 loc = m_scorches[curScorch].location;
		Int type = m_scorches[curScorch].scorchType;
		if (type<0) {
			type = 0;
		}
		if (type >= SCORCH_MARKS_IN_TEXTURE) {
			type = 0;
		}
		Real amtToFloat = 0;
		amtToFloat = MAP_HEIGHT_SCALE/10;

		Int minX = REAL_TO_INT_FLOOR((loc.X-radius)/MAP_XY_FACTOR);
		Int minY = REAL_TO_INT_FLOOR((loc.Y-radius)/MAP_XY_FACTOR);
		if (minX<-m_map->getBorderSizeInline()) minX=-m_map->getBorderSizeInline();
		if (minY<-m_map->getBorderSizeInline()) minY=-m_map->getBorderSizeInline();
		Int maxX = REAL_TO_INT_CEIL((loc.X+radius)/MAP_XY_FACTOR);
		Int maxY = REAL_TO_INT_CEIL((loc.Y+radius)/MAP_XY_FACTOR);
		maxX++; maxY++;
		if (maxX > m_map->getXExtent()-m_map->getBorderSizeInline()) {
			maxX = m_map->getXExtent()-m_map->getBorderSizeInline();
		}
		if (maxY > m_map->getYExtent()-m_map->getBorderSizeInline()) {
			maxY = m_map->getYExtent()-m_map->getBorderSizeInline();
		}
		Int startVertex = m_curNumScorchVertices;
		Int i, j;
		for (j=minY; j<maxY; j++) {
			for (i=minX; i<maxX; i++) {
				if (m_curNumScorchVertices >= MAX_SCORCH_VERTEX) return;
				curVb->diffuse = diffuse;
				Real theZ;
				theZ = amtToFloat+((float)getClipHeight(i+m_map->getBorderSizeInline(),j+m_map->getBorderSizeInline())*MAP_HEIGHT_SCALE);
				// The scorchmarks are spaced out by 1.5 in the texture.
				Real uOffset = (type%SCORCH_PER_ROW) * 1.5f;
				Real vOffset = (type/SCORCH_PER_ROW) * 1.5f;
				Real X = i*MAP_XY_FACTOR;
				Real Y = j*MAP_XY_FACTOR;
				curVb->u1 = (uOffset + 0.5f + (X - loc.X)/(2*radius)) / (SCORCH_PER_ROW+1);
				curVb->v1 = (vOffset + 0.5f + (Y - loc.Y)/(2*radius)) / (SCORCH_PER_ROW+1);
				curVb->x = X;
				curVb->y = Y;
				curVb->z = theZ;
				curVb++;
				m_curNumScorchVertices++;
			}
		}
		Int yOffset = maxX-minX;
		for (j=0; j<maxY-minY-1; j++) {
			for (i=0; i<maxX-minX-1; i++) {
				if (m_curNumScorchIndices+6 > MAX_SCORCH_INDEX) return;
				Int xNdx = i+minX+m_map->getBorderSizeInline();
				Int yNdx = j+minY+m_map->getBorderSizeInline();
				Bool flipForBlend = m_map->getFlipState(xNdx, yNdx);
#if 0
				UnsignedByte alpha[4];
				float UA[4], VA[4];
				m_map->getAlphaUVData(xNdx, yNdx, UA, VA, alpha, &flipForBlend);
#endif
				if (flipForBlend) {
					*curIb++ = startVertex + j*yOffset + i+1;
 					*curIb++ = startVertex + j*yOffset + i+yOffset;
					*curIb++ = startVertex + j*yOffset + i;
 					*curIb++ = startVertex + j*yOffset + i+1;
 					*curIb++ = startVertex + j*yOffset + i+1+yOffset;
					*curIb++ = startVertex + j*yOffset + i+yOffset;
				}
				else
				{
					*curIb++ = startVertex + j*yOffset + i;
					*curIb++ = startVertex + j*yOffset + i+1+yOffset;
					*curIb++ = startVertex + j*yOffset + i+yOffset;
					*curIb++ = startVertex + j*yOffset + i;
					*curIb++ = startVertex + j*yOffset + i+1;
					*curIb++ = startVertex + j*yOffset + i+1+yOffset;
				}
				m_curNumScorchIndices+=6;
			}
		}
	}

}

#endif

//=============================================================================
// BaseHeightMapRenderObjClass::clearAllScorches
//=============================================================================
/** Removes all scorches. */
//=============================================================================
void BaseHeightMapRenderObjClass::clearAllScorches()
{
#ifdef DO_SCORCH
	m_numScorches=0;
	m_scorchesInBuffer=0;
#endif
}

//=============================================================================
// BaseHeightMapRenderObjClass::addScorch
//=============================================================================
/** Adds a scorch mark. */
//=============================================================================
void BaseHeightMapRenderObjClass::addScorch(Vector3 location, Real radius, Scorches type)
{
#ifdef DO_SCORCH
	if (m_numScorches >= MAX_SCORCH_MARKS) {
		Int i;
		for (i=0; i<MAX_SCORCH_MARKS-1; i++) {
			m_scorches[i] = m_scorches[i+1];
		}
		m_numScorches--;
	}

	Int i;
	Real limit = radius/4;
	for (i=0; i<m_numScorches; i++) {
		if ( abs(location.X-m_scorches[i].location.X) < limit &&
				 abs(location.Y-m_scorches[i].location.Y) < limit &&
				 abs(radius - m_scorches[i].radius) < limit &&
				 m_scorches[i].scorchType == type) {
			return; // basically a duplicate.
		}
	}

	m_scorches[m_numScorches].location = location;
	m_scorches[m_numScorches].radius = radius;
	m_scorches[m_numScorches].scorchType = type;
	m_numScorches++;
	m_scorchesInBuffer = 0; // force buffer regenerations.
#endif
}


//=============================================================================
// BaseHeightMapRenderObjClass::getStaticDiffuse
//=============================================================================
/** Gets the static diffuse color value for a terrain vertex.*/
//=============================================================================
Int BaseHeightMapRenderObjClass::getStaticDiffuse(Int x, Int y)
{

	if (x<0) x = 0;
	if (y<0) y = 0;
	if (x >= m_map->getXExtent())
		x=m_map->getXExtent()-1;
	if (y >= m_map->getYExtent())
		y=m_map->getYExtent()-1;

	if (m_map == nullptr) {
		return(0);
	}

	Vector3 l2r,n2f,normalAtTexel;
	Int vn0,un0,vp1,up1;
	constexpr const Int cellOffset = 1;

	vn0 = y-cellOffset;
	vp1 = y+cellOffset;
	if (vp1 >= m_map->getYExtent())
		vp1=m_map->getYExtent()-1;
	if (vn0<0) vn0 = 0;
	un0 = x-cellOffset;
	up1 = x+cellOffset;
	if (un0 < 0)
		un0=0;
	if (up1 >= m_map->getXExtent())
		up1=m_map->getXExtent()-1;

	Vector3 lightRay[MAX_GLOBAL_LIGHTS];
	const Coord3D *lightPos;

	for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
	{
		lightPos=&TheGlobalData->m_terrainLightPos[lightIndex];
		lightRay[lightIndex].Set(-lightPos->x,-lightPos->y,	-lightPos->z);
	}

	//top-left sample
	l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getHeight(up1, y) - m_map->getHeight(un0, y)));
	n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getHeight(x, vp1) - m_map->getHeight(x, vn0)));

	Vector3::Normalized_Cross_Product(l2r,n2f, &normalAtTexel);

	VERTEX_FORMAT vertex;
	vertex.x=ADJUST_FROM_INDEX_TO_REAL(x);
	vertex.y=ADJUST_FROM_INDEX_TO_REAL(y);

	vertex.z=  ((float)m_map->getHeight(x,y))*MAP_HEIGHT_SCALE;
	vertex.u1=0;
	vertex.v1=0;
	vertex.u2=1;
	vertex.v2=1;

	RTS3DScene *pMyScene = (RTS3DScene *)Scene;
	if (pMyScene) {
		RefRenderObjListIterator *it = pMyScene->createLightsIterator();
		doTheLight(&vertex, lightRay, &normalAtTexel, it, 1.0f);
		if (it) {
		 pMyScene->destroyLightsIterator(it);
		 it = nullptr;
		}
	} else {
		doTheLight(&vertex, lightRay, &normalAtTexel, nullptr, 1.0f);
	}
	return vertex.diffuse;
}

//=============================================================================
// BaseHeightMapRenderObjClass::On_Frame_Update
//=============================================================================
/** Updates the diffuse color values in the vertices as affected by the dynamic lights.*/
//=============================================================================
void BaseHeightMapRenderObjClass::On_Frame_Update()
{

}

//=============================================================================
// BaseHeightMapRenderObjClass::unitMoved
//=============================================================================
/** Tell that a unit moved.*/
//=============================================================================
void BaseHeightMapRenderObjClass::unitMoved( Object *unit )
{
	if (m_treeBuffer) {
		m_treeBuffer->unitMoved(unit);
	}
}

//=============================================================================
// BaseHeightMapRenderObjClass::removeTreesAndPropsForConstruction
//=============================================================================
/** Tell that a unit moved.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeTreesAndPropsForConstruction(const Coord3D* pos, const GeometryInfo& geom, Real angle )
{
	if (m_treeBuffer) {
		m_treeBuffer->removeTreesForConstruction(pos, geom, angle);
	}
	if (m_propBuffer) {
		m_propBuffer->removePropsForConstruction(pos, geom, angle);
	}
}

//=============================================================================
// BaseHeightMapRenderObjClass::addTree
//=============================================================================
/** Adds a tree to the tree buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::addTree(DrawableID id, Coord3D location, Real scale, Real angle,
								Real randomScaleAmount,  const W3DTreeDrawModuleData *data)
{
	if (m_treeBuffer) {
		m_treeBuffer->addTree(id, location, scale, angle, randomScaleAmount, data);
	}
};

//=============================================================================
// BaseHeightMapRenderObjClass::removeTree
//=============================================================================
/** Adds a tree to the tree buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeTree(DrawableID id)
{
	if (m_treeBuffer) {
		m_treeBuffer->removeTree(id);
	}
};

//=============================================================================
// BaseHeightMapRenderObjClass::removeAllTrees
//=============================================================================
/** Adds a tree to the tree buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeAllTrees()
{
	if (m_treeBuffer) {
		m_treeBuffer->clearAllTrees();
	}
};

//=============================================================================
// BaseHeightMapRenderObjClass::updateTreePosition
//=============================================================================
/** Updates a tree's position and angle in the tree buffer.*/
//=============================================================================
Bool BaseHeightMapRenderObjClass::updateTreePosition(DrawableID id, Coord3D location, Real angle)
{
	if (m_treeBuffer) {
		return m_treeBuffer->updateTreePosition(id, location, angle);
	}
	return false;
};

//=============================================================================
// BaseHeightMapRenderObjClass::addProp
//=============================================================================
/** Adds a prop to the prop buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::addProp(Int id, Coord3D location, Real angle, Real scale,
																					const AsciiString &modelName)
{
	if (m_propBuffer) {
		m_propBuffer->addProp(id, location, angle, scale, modelName);
	}
};


//=============================================================================
// BaseHeightMapRenderObjClass::removeProp
//=============================================================================
/** Removes a prop from the prop buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeProp(Int id)
{
	if (m_propBuffer) {
		m_propBuffer->removeProp(id);
	}
};

//=============================================================================
// BaseHeightMapRenderObjClass::removeAllProps
//=============================================================================
/** Removes all props from the prop buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeAllProps()
{
	if (m_propBuffer) {
		m_propBuffer->clearAllProps();
	}
};

//=============================================================================
// BaseHeightMapRenderObjClass::notifyShroudChanged
//=============================================================================
/** Notifies that the local shroud changed.*/
//=============================================================================
void BaseHeightMapRenderObjClass::notifyShroudChanged()
{
	if (m_propBuffer) {
		m_propBuffer->notifyShroudChanged();
	}
};

//=============================================================================
// BaseHeightMapRenderObjClass::addTerrainBib
//=============================================================================
/** Adds a terrainBib to the bib buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::addTerrainBib(Vector3 corners[4],
																						ObjectID id, Bool highlight)
{
	if (m_bibBuffer)
		m_bibBuffer->addBib(corners, id, highlight);
};

//=============================================================================
// BaseHeightMapRenderObjClass::addTerrainBib
//=============================================================================
/** Adds a terrainBib to the bib buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::addTerrainBibDrawable(Vector3 corners[4],
																						DrawableID id, Bool highlight)
{
	if (m_bibBuffer)
		m_bibBuffer->addBibDrawable(corners, id, highlight);
};

//=============================================================================
// BaseHeightMapRenderObjClass::removeAllTerrainBibs
//=============================================================================
/** Removes all terrainBib highlighting from the bib buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeTerrainBibHighlighting()
{
	if (m_bibBuffer)
		m_bibBuffer->removeHighlighting(  );
};

//=============================================================================
// BaseHeightMapRenderObjClass::removeAllTerrainBibs
//=============================================================================
/** Removes all terrainBibs from the bib buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeAllTerrainBibs()
{
	if (m_bibBuffer)
		m_bibBuffer->clearAllBibs(  );
};

//=============================================================================
// BaseHeightMapRenderObjClass::removeTerrainBib
//=============================================================================
/** Removes a terrainBib from the bib buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeTerrainBib(ObjectID id)
{
	if (m_bibBuffer)
		m_bibBuffer->removeBib( id );
};

//=============================================================================
// BaseHeightMapRenderObjClass::removeTerrainBib
//=============================================================================
/** Removes a terrainBib from the bib buffer.*/
//=============================================================================
void BaseHeightMapRenderObjClass::removeTerrainBibDrawable(DrawableID id)
{
	if (m_bibBuffer)
		m_bibBuffer->removeBibDrawable( id );
};

//=============================================================================
// BaseHeightMapRenderObjClass::staticLightingChanged
//=============================================================================
/** Notification that all lighting needs to be recalculated. */
//=============================================================================
void BaseHeightMapRenderObjClass::staticLightingChanged()
{
	// Cause the terrain to get updated with new lighting.
	m_needFullUpdate = true;

	// Cause the scorches to get updated with new lighting.
	m_scorchesInBuffer = 0; // If we just allocated the buffers, we got no scorches in the buffer.
	m_curNumScorchVertices=0;
	m_curNumScorchIndices=0;
	if (m_roadBuffer)
		m_roadBuffer->updateLighting();

}

//=============================================================================
// BaseHeightMapRenderObjClass::setTimeOfDay
//=============================================================================
/** When the time of day changes, the lighting changes and we need to update. */
//=============================================================================
void BaseHeightMapRenderObjClass::setTimeOfDay( TimeOfDay tod )
{
	staticLightingChanged();
}

//=============================================================================
// BaseHeightMapRenderObjClass::Notify_Added
//=============================================================================
/** W3D render object method, we use it to add ourselves to tthe update
list, so On_Frame_Update gets called. */
//=============================================================================
void BaseHeightMapRenderObjClass::Notify_Added(SceneClass * scene)
{
	RenderObjClass::Notify_Added(scene);
	scene->Register(this,SceneClass::ON_FRAME_UPDATE);
}

//=============================================================================
// BaseHeightMapRenderObjClass::updateCenter
//=============================================================================
/** Updates the positioning of the drawn portion of the height map in the
heightmap.  As the view slides around, this determines what is the actually
rendered portion of the terrain.  Only a 96x96 section is rendered at any time,
even though maps can be up to 1024x1024.  This function determines which subset
is rendered. */
//=============================================================================
void BaseHeightMapRenderObjClass::updateCenter(CameraClass *camera, const Vector3 *cameraPivot, RefRenderObjListIterator *pLightsIterator)
{
	if (m_map==nullptr) {
		return;
	}
	if (m_updating) {
		return;
	}

	if (m_treeBuffer) {
		m_treeBuffer->doFullUpdate();	// Tell the trees to update for view change.
	}
	if (m_propBuffer) {
		m_propBuffer->doFullUpdate();	// Tell the trees to update for view change.
	}
	m_updating = true;
#ifdef DO_ROADS
	if (m_roadBuffer) {
		m_roadBuffer->updateCenter();
	}
#endif
	if (m_needFullUpdate) {
		m_bridgeBuffer->doFullUpdate();
		m_bridgeBuffer->updateCenter(camera, pLightsIterator);
		m_updating = false;
		return;
	}
	m_bridgeBuffer->updateCenter(camera, pLightsIterator);
	m_updating = false;
}

//=============================================================================
// BaseHeightMapRenderObjClass::Render
//=============================================================================
/** Renders (draws) the terrain. */
//=============================================================================
//DECLARE_PERF_TIMER(Terrain_Render)

void BaseHeightMapRenderObjClass::Render(RenderInfoClass & rinfo)
{

}

/**Render parts of terrain that are along the coast line and have vertices directly under the
water plane.  Applying a custom render to these polygons allows for a smoother land->water
transition*/
void BaseHeightMapRenderObjClass::renderShoreLines(CameraClass *pCamera)
{
	if (!TheGlobalData->m_isWorldBuilder)	//use faster version optimized for game and not world builder?
	{	renderShoreLinesSorted(pCamera);
		return;
	}

	m_numVisibleShoreLineTiles=0;

	if (!TheGlobalData->m_showSoftWaterEdge || TheWaterTransparency->m_transparentWaterDepth==0 || m_numShoreLineTiles == 0)
		return;

	//Check if video card is capable of using this effect
	if (DX8Wrapper::getBackBufferFormat() != WW3D_FORMAT_A8R8G8B8)
		return;	//can't apply effect on cards without destination alpha

	Int vertexCount = 0;
	Int indexCount = 0;
	Int drawEdgeY=m_map->getDrawOrgY()+m_map->getDrawHeight()-1;
	Int drawEdgeX=m_map->getDrawOrgX()+m_map->getDrawWidth()-1;
	if (drawEdgeX > (m_map->getXExtent()-1))
		drawEdgeX = m_map->getXExtent()-1;
	if (drawEdgeY > (m_map->getYExtent()-1))
		drawEdgeY = m_map->getYExtent()-1;
	Int drawStartX=m_map->getDrawOrgX();
	Int drawStartY=m_map->getDrawOrgY();
	Int j=0;

	ShaderClass unlitShader=ShaderClass::_PresetOpaque2DShader;
	unlitShader.Set_Depth_Compare(ShaderClass::PASS_LEQUAL);
	DX8Wrapper::Set_Shader(unlitShader);
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	DX8Wrapper::Set_Texture(0,m_destAlphaTexture);
	DX8Wrapper::Set_Transform(D3DTS_WORLD,Matrix3D(true));
	//Enabled writes to destination alpha only
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_ALPHA);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXCOORDINDEX, 0);


	while (j != m_numShoreLineTiles)
	{
		DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,DEFAULT_MAX_BATCH_SHORELINE_TILES*4);
		DynamicIBAccessClass ib_access(BUFFER_TYPE_DYNAMIC_DX8,DEFAULT_MAX_BATCH_SHORELINE_TILES*6);

		{	//Need to put this in another code block so vb/ib gets automatically locked/unlocked by destructors
			DynamicVBAccessClass::WriteLockClass lock(&vb_access);
			VertexFormatXYZNDUV2 *vb= lock.Get_Formatted_Vertex_Array();
			DynamicIBAccessClass::WriteLockClass lockib(&ib_access);
			UnsignedShort *ib=lockib.Get_Index_Array();
			if (!ib || !vb)
			{	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_RED);
				return;
			}

			//Loop over visible terrain and extract all the tiles that need shoreline blend
			for (; j<m_numShoreLineTiles; j++)
			{
				if (vertexCount >= (DEFAULT_MAX_BATCH_SHORELINE_TILES*4))
					break;	//no room in vertex buffer

				shoreLineTileInfo *shoreInfo=&m_shoreLineTilePositions[j];

				Int x = shoreInfo->m_xy & 0xffff;
				Int y = shoreInfo->m_xy >> 16;

				if (x >= drawStartX && x < drawEdgeX &&	y >= drawStartY && y < drawEdgeY)
				{	//this tile is inside visible region

					vb->x = shoreInfo->verts[0];
					vb->y = shoreInfo->verts[1];
					vb->z = shoreInfo->verts[2];
					vb->nx=0;	//filling these to keep AGP write buffer happy.
					vb->ny=0;
					vb->nz=0;
					vb->diffuse=0;
					vb->u1=shoreInfo->t0;
					vb->v1=0;
					vb->u2=0;
					vb->v2=0;
					vb++;

					vb->x = shoreInfo->verts[3];
					vb->y = shoreInfo->verts[4];
					vb->z = shoreInfo->verts[5];
					vb->nx=0;	//filling these to keep AGP write buffer happy.
					vb->ny=0;
					vb->nz=0;
					vb->diffuse=0;
					vb->u1=shoreInfo->t1;
					vb->v1=0;
					vb->u2=0;
					vb->v2=0;
					vb++;

					vb->x = shoreInfo->verts[6];
					vb->y = shoreInfo->verts[7];
					vb->z = shoreInfo->verts[8];
					vb->nx=0;	//filling these to keep AGP write buffer happy.
					vb->ny=0;
					vb->nz=0;
					vb->diffuse=0;
					vb->u1=shoreInfo->t2;
					vb->v1=0;
					vb->u2=0;
					vb->v2=0;
					vb++;

					vb->x = shoreInfo->verts[9];
					vb->y = shoreInfo->verts[10];
					vb->z = shoreInfo->verts[11];
					vb->nx=0;	//filling these to keep AGP write buffer happy.
					vb->ny=0;
					vb->nz=0;
					vb->diffuse=0;
					vb->u1=shoreInfo->t3;
					vb->v1=0;
					vb->u2=0;
					vb->v2=0;
					vb++;

					if (m_map->getQuickFlipState(x,y))
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

		if (indexCount > 0 && vertexCount > 0)
		{
			DX8Wrapper::Set_Index_Buffer(ib_access,0);
			DX8Wrapper::Set_Vertex_Buffer(vb_access);
			DX8Wrapper::Draw_Triangles(	0,indexCount/3, 0,	vertexCount);	//draw a quad, 2 triangles, 4 verts
			m_numVisibleShoreLineTiles += indexCount/6;
		}

		vertexCount=0;
		indexCount=0;
	}

	//Disable writes to destination alpha
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_RED);
	ShaderClass::Invalidate();
}

/**Render parts of terrain that are along the coast line and have vertices directly under the
water plane.  Applying a custom render to these polygons allows for a smoother land->water
transition.  This version is exactly like the one above but optimized for the case where tiles
are assumed to be sorted.  Not used by World Builder. */
void BaseHeightMapRenderObjClass::renderShoreLinesSorted(CameraClass *pCamera)
{
	m_numVisibleShoreLineTiles=0;

	if (!TheGlobalData->m_showSoftWaterEdge || TheWaterTransparency->m_transparentWaterDepth==0 || m_numShoreLineTiles == 0)
		return;

	//Check if video card is capable of using this effect
	if (DX8Wrapper::getBackBufferFormat() != WW3D_FORMAT_A8R8G8B8)
		return;	//can't apply effect on cards without destination alpha

	Int vertexCount = 0;
	Int indexCount = 0;
	Int drawEdgeY=m_map->getDrawOrgY()+m_map->getDrawHeight()-1;
	Int drawEdgeX=m_map->getDrawOrgX()+m_map->getDrawWidth()-1;
	if (drawEdgeX > (m_map->getXExtent()-1))
		drawEdgeX = m_map->getXExtent()-1;
	if (drawEdgeY > (m_map->getYExtent()-1))
		drawEdgeY = m_map->getYExtent()-1;
	Int drawStartX=m_map->getDrawOrgX();
	Int drawStartY=m_map->getDrawOrgY();

	if (m_shoreLineSortInfosXMajor)	//map is wider than taller.
	{
		//Clamp the major map axis to available shoreline tiles.
		if (m_shoreLineTileSortMinCoordinate > drawStartX)
			drawStartX=m_shoreLineTileSortMinCoordinate;
		if ((m_shoreLineTileSortMaxCoordinate+1) < drawEdgeX)
			drawEdgeX=(m_shoreLineTileSortMaxCoordinate+1);
		if ((drawEdgeX-drawStartX) <= 0)
			return;	//nothing to draw
	}
	else
	{
		//Clamp the major map axis to available shoreline tiles.
		if (m_shoreLineTileSortMinCoordinate > drawStartY)
			drawStartY=m_shoreLineTileSortMinCoordinate;
		if ((m_shoreLineTileSortMaxCoordinate+1) < drawEdgeY)
			drawEdgeY=(m_shoreLineTileSortMaxCoordinate+1);

		if ((drawEdgeY-drawStartY) <= 0)
			return;	//nothing to draw
	}

	ShaderClass unlitShader=ShaderClass::_PresetOpaque2DShader;
	unlitShader.Set_Depth_Compare(ShaderClass::PASS_LEQUAL);
	DX8Wrapper::Set_Shader(unlitShader);
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	DX8Wrapper::Set_Texture(0,m_destAlphaTexture);
	DX8Wrapper::Set_Transform(D3DTS_WORLD,Matrix3D(true));
	//Enabled writes to destination alpha only
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_ALPHA);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_TEXCOORDINDEX, 0);

	Bool isDone=FALSE;
	Int lastRenderedTile=0;

	while (!isDone)
	{
		DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,DEFAULT_MAX_BATCH_SHORELINE_TILES*4);
		DynamicIBAccessClass ib_access(BUFFER_TYPE_DYNAMIC_DX8,DEFAULT_MAX_BATCH_SHORELINE_TILES*6);

		{	//Need to put this in another code block so vb/ib gets automatically locked/unlocked by destructors
			DynamicVBAccessClass::WriteLockClass lock(&vb_access);
			VertexFormatXYZNDUV2 *vb= lock.Get_Formatted_Vertex_Array();
			DynamicIBAccessClass::WriteLockClass lockib(&ib_access);
			UnsignedShort *ib=lockib.Get_Index_Array();
			if (!ib || !vb)
			{	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_RED);
				return;
			}

			//Loop over visible terrain and extract all the tiles that need shoreline blend
			if (m_shoreLineSortInfosXMajor)	//map is wider than taller.
			{
				Int x=drawStartX;
				for (; x<drawEdgeX; x++)
				{	//figure out how many tiles are available in this column
					shoreLineTileSortInfo *sortInfo=&m_shoreLineSortInfos[x];

					if (!sortInfo->numTiles)
						continue;	//no tiles in this column.

					//Clamp visible area to actual tiles in this column
					Int startY=drawStartY;
					if (sortInfo->minTileCoordinate > startY)
						startY = sortInfo->minTileCoordinate;
					Int edgeY=drawEdgeY;
					if ((sortInfo->maxTileCoordinate+1) < edgeY)
						edgeY = sortInfo->maxTileCoordinate+1;

					if ((edgeY-startY) <= 0)
						continue;	//no tiles visible in this column.

					//Pointer to first tile in this column
					shoreLineTileInfo *shoreInfo=m_shoreLineTilePositions+sortInfo->tileStartIndex+lastRenderedTile;
					//Loop over tiles in this column and render visible ones
					for (Int k=lastRenderedTile; k<sortInfo->numTiles; k++)
					{
						Int tileY = shoreInfo->m_xy >> 16;
						if (tileY < startY)
						{	shoreInfo++;	//advance to next tile.
							continue;	//this tile is not visible
						}

						if (tileY >= edgeY)
							break;	//since tiles are x-sorted, there will not be any visible ones after this one.

						if (vertexCount >= (DEFAULT_MAX_BATCH_SHORELINE_TILES*4))
						{	lastRenderedTile=k;
							goto flushVertexBuffer0;
						}

						vb->x = shoreInfo->verts[0];
						vb->y = shoreInfo->verts[1];
						vb->z = shoreInfo->verts[2];
						vb->nx=0;	//filling these to keep AGP write buffer happy.
						vb->ny=0;
						vb->nz=0;
						vb->diffuse=0;
						vb->u1=shoreInfo->t0;
						vb->v1=0;
						vb->u2=0;
						vb->v2=0;
						vb++;

						vb->x = shoreInfo->verts[3];
						vb->y = shoreInfo->verts[4];
						vb->z = shoreInfo->verts[5];
						vb->nx=0;	//filling these to keep AGP write buffer happy.
						vb->ny=0;
						vb->nz=0;
						vb->diffuse=0;
						vb->u1=shoreInfo->t1;
						vb->v1=0;
						vb->u2=0;
						vb->v2=0;
						vb++;

						vb->x = shoreInfo->verts[6];
						vb->y = shoreInfo->verts[7];
						vb->z = shoreInfo->verts[8];
						vb->nx=0;	//filling these to keep AGP write buffer happy.
						vb->ny=0;
						vb->nz=0;
						vb->diffuse=0;
						vb->u1=shoreInfo->t2;
						vb->v1=0;
						vb->u2=0;
						vb->v2=0;
						vb++;

						vb->x = shoreInfo->verts[9];
						vb->y = shoreInfo->verts[10];
						vb->z = shoreInfo->verts[11];
						vb->nx=0;	//filling these to keep AGP write buffer happy.
						vb->ny=0;
						vb->nz=0;
						vb->diffuse=0;
						vb->u1=shoreInfo->t3;
						vb->v1=0;
						vb->u2=0;
						vb->v2=0;
						vb++;

						if (m_map->getQuickFlipState(x,tileY))
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
						shoreInfo++;	//advance to next tile.
					}
					lastRenderedTile=0;
				}
flushVertexBuffer0:
				drawStartX = x;	//record how far we've moved so far
				isDone = x >= drawEdgeX;
			}
			else
			{
				Int y=drawStartY;
				for (; y<drawEdgeY; y++)
				{	//figure out how many tiles are available in this row
					shoreLineTileSortInfo *sortInfo=&m_shoreLineSortInfos[y];

					if (!sortInfo->numTiles)
						continue;	//no tiles in this row.

					//Clamp visible area to actual tiles in this row
					Int startX=drawStartX;
					if (sortInfo->minTileCoordinate > startX)
						startX = sortInfo->minTileCoordinate;
					Int edgeX=drawEdgeX;
					if ((sortInfo->maxTileCoordinate+1) < edgeX)
						edgeX = sortInfo->maxTileCoordinate+1;

					if ((edgeX-startX) <= 0)
						continue;	//no tiles visible in this row.

					//Pointer to first tile in this row
					shoreLineTileInfo *shoreInfo=m_shoreLineTilePositions+sortInfo->tileStartIndex+lastRenderedTile;
					//Loop over tiles in this row and render visible ones
					for (Int k=lastRenderedTile; k<sortInfo->numTiles; k++)
					{
						Int tileX = shoreInfo->m_xy & 0xffff;
						if (tileX < startX)
						{	shoreInfo++;	//advance to next tile.
							continue;	//this tile is not visible
						}

						if (tileX >= edgeX)
							break;	//since tiles are x-sorted, there will not be any visible ones after this one.

						if (vertexCount >= (DEFAULT_MAX_BATCH_SHORELINE_TILES*4))
						{	lastRenderedTile=k;
							goto flushVertexBuffer1;
						}

						vb->x = shoreInfo->verts[0];
						vb->y = shoreInfo->verts[1];
						vb->z = shoreInfo->verts[2];
						vb->nx=0;	//filling these to keep AGP write buffer happy.
						vb->ny=0;
						vb->nz=0;
						vb->diffuse=0;
						vb->u1=shoreInfo->t0;
						vb->v1=0;
						vb->u2=0;
						vb->v2=0;
						vb++;

						vb->x = shoreInfo->verts[3];
						vb->y = shoreInfo->verts[4];
						vb->z = shoreInfo->verts[5];
						vb->nx=0;	//filling these to keep AGP write buffer happy.
						vb->ny=0;
						vb->nz=0;
						vb->diffuse=0;
						vb->u1=shoreInfo->t1;
						vb->v1=0;
						vb->u2=0;
						vb->v2=0;
						vb++;

						vb->x = shoreInfo->verts[6];
						vb->y = shoreInfo->verts[7];
						vb->z = shoreInfo->verts[8];
						vb->nx=0;	//filling these to keep AGP write buffer happy.
						vb->ny=0;
						vb->nz=0;
						vb->diffuse=0;
						vb->u1=shoreInfo->t2;
						vb->v1=0;
						vb->u2=0;
						vb->v2=0;
						vb++;

						vb->x = shoreInfo->verts[9];
						vb->y = shoreInfo->verts[10];
						vb->z = shoreInfo->verts[11];
						vb->nx=0;	//filling these to keep AGP write buffer happy.
						vb->ny=0;
						vb->nz=0;
						vb->diffuse=0;
						vb->u1=shoreInfo->t3;
						vb->v1=0;
						vb->u2=0;
						vb->v2=0;
						vb++;

						if (m_map->getQuickFlipState(tileX,y))
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
						shoreInfo++;	//advance to next tile.
					}
					lastRenderedTile=0;
				}
flushVertexBuffer1:
				drawStartY = y;	//record how far we've moved so far
				isDone = y >= drawEdgeY;
			}
		}

		if (indexCount > 0 && vertexCount > 0)
		{
			DX8Wrapper::Set_Index_Buffer(ib_access,0);
			DX8Wrapper::Set_Vertex_Buffer(vb_access);
			DX8Wrapper::Draw_Triangles(	0,indexCount/3, 0,	vertexCount);	//draw a quad, 2 triangles, 4 verts
			m_numVisibleShoreLineTiles += indexCount/6;
		}

		vertexCount=0;
		indexCount=0;
	}

	//Disable writes to destination alpha
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_RED);
	ShaderClass::Invalidate();
}

//=============================================================================
// BaseHeightMapRenderObjClass::renderTrees
//=============================================================================
/** Renders (draws) the trees. Since the trees are transparent, this has to be
called after flush. */
//=============================================================================
void BaseHeightMapRenderObjClass::renderTrees(CameraClass * camera)
{
#ifdef EXTENDED_STATS
	if (DX8Wrapper::stats.m_disableObjects) {
		return;
	}
#endif
	if (m_map==nullptr) return;
	if (Scene==nullptr) return;
	if (m_treeBuffer) {
		DX8Wrapper::Set_Transform(D3DTS_WORLD,Transform);
		DX8Wrapper::Set_Material(m_vertexMaterialClass);
		RTS3DScene *pMyScene = (RTS3DScene *)Scene;
		RefRenderObjListIterator pDynamicLightsIterator(pMyScene->getDynamicLights());
		m_treeBuffer->drawTrees(camera, &pDynamicLightsIterator);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void BaseHeightMapRenderObjClass::crc( Xfer *xfer )
{
	// empty. jba [8/11/2003]
}

// ------------------------------------------------------------------------------------------------
/** Xfer
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void BaseHeightMapRenderObjClass::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	xfer->xferSnapshot( m_treeBuffer );
	xfer->xferSnapshot( m_propBuffer );


}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void BaseHeightMapRenderObjClass::loadPostProcess()
{
	// empty. jba [8/11/2003]
}

//=============================================================================
Bool BaseHeightMapRenderObjClass::useCloud()
{
	return TheGlobalData->m_useCloudMap && TheGlobalData->m_timeOfDay != TIME_OF_DAY_NIGHT;
}
