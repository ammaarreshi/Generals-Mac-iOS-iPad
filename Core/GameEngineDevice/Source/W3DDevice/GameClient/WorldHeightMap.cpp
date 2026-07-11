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

// WorldHeightMap.cpp
// Class to encapsulate height map.
// Author: John Ahlquist, April 2001

#define INSTANTIATE_WELL_KNOWN_KEYS

#include "windows.h"
#include "stdlib.h"
#include "Common/STLTypedefs.h"

#include "Common/DataChunk.h"
//#include "Common/GameFileSystem.h"
#include "Common/FileSystem.h" // for LOAD_TEST_ASSETS
#include "Common/GlobalData.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/TerrainTypes.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/WellKnownKeys.h"

#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/SidesList.h"

#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/TileData.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/W3DShadow.h"

#include "Common/file.h"


#define K_OBSOLETE_HEIGHT_MAP_VERSION 8

#define PATHFIND_CLIFF_SLOPE_LIMIT_F	9.8f

// -----------------------------------------------------------
static AsciiString validateName(AsciiString n, Int flags)
{

	return n;

}

/* ********* GDIFileStream class ****************************/
class GDIFileStream : public InputStream
{
protected:
	File* m_file;
public:
	GDIFileStream(File* pFile):m_file(pFile) {};
	virtual Int read(void *pData, Int numBytes) override {
			return(m_file->read(pData, numBytes));
	};
};


/* ********* MapObject class ****************************/
/*static*/ MapObject *MapObject::TheMapObjectListPtr = nullptr;
/*static*/ Dict MapObject::TheWorldDict;

MapObject::MapObject(Coord3D loc, AsciiString name, Real angle, Int flags, const Dict* props,
										 const ThingTemplate *thingTemplate )
{
	m_objectName = validateName( name, flags );
	m_thingTemplate = thingTemplate;
	m_nextMapObject = nullptr;
	m_location = loc;
	m_angle = normalizeAngle(angle);
	m_color = (0xff)<<8; // Bright green.
	m_flags = flags;
	m_renderObj = nullptr;
	m_shadowObj = nullptr;
	m_runtimeFlags = 0;
	// Note - do NOT set TheKey_objectSelectable on creation - allow it to follow the .ini value unless specified by user action.  jba. [3/20/2003]
	if (props)
	{
		m_properties = *props;
	}
	else
	{
		m_properties.setInt(TheKey_objectInitialHealth, 100);
		m_properties.setBool(TheKey_objectEnabled, true);
		m_properties.setBool(TheKey_objectIndestructible, false);
		m_properties.setBool(TheKey_objectUnsellable, false);
		m_properties.setBool(TheKey_objectPowered, true);
		m_properties.setBool(TheKey_objectRecruitableAI, true);
		m_properties.setBool(TheKey_objectTargetable, false );
	}

	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
		setBridgeRenderObject( (BridgeTowerType)i, nullptr );

}


MapObject::~MapObject()
{
	setRenderObj(nullptr);
	setShadowObj(nullptr);
	if (m_nextMapObject) {
		MapObject *cur = m_nextMapObject;
		MapObject *next;
		while (cur) {
			next = cur->getNext();
			cur->setNextMap(nullptr); // prevents recursion.
			deleteInstance(cur);
			cur = next;
		}
	}
	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
		setBridgeRenderObject( (BridgeTowerType)i, nullptr );

}

MapObject *MapObject::duplicate()
{
	MapObject *pObj = newInstance( MapObject)(m_location, m_objectName, m_angle, m_flags, &m_properties, m_thingTemplate);
	pObj->setColor(getColor());
	pObj->m_runtimeFlags = m_runtimeFlags;
	return pObj;
}

void MapObject::setRenderObj(RenderObjClass *pObj)
{
	REF_PTR_SET(m_renderObj, pObj);
}

void MapObject::setBridgeRenderObject( BridgeTowerType type, RenderObjClass* renderObj )
{

	if( type >= 0 && type < BRIDGE_MAX_TOWERS )
		REF_PTR_SET( m_bridgeTowers[ type ], renderObj );

}

RenderObjClass* MapObject::getBridgeRenderObject( BridgeTowerType type )
{

	if( type >= 0 && type < BRIDGE_MAX_TOWERS )
		return m_bridgeTowers[ type ];
	return nullptr;

}

void MapObject::validate()
{
	verifyValidTeam();
	verifyValidUniqueID();
}

void MapObject::verifyValidTeam()
{
	// if this map object has a valid team, then do nothing.
	// if it has an invalid team, the place it on the default neutral team, (by clearing the
	// existing team name.)
	Bool exists;
	AsciiString teamName = getProperties()->getAsciiString(TheKey_originalOwner, &exists);
	if (exists) {
		Bool valid = false;

		int numSides = TheSidesList->getNumTeams();

		for (int i = 0; i < numSides; ++i) {
			TeamsInfo *teamInfo = TheSidesList->getTeamInfo(i);
			if (!teamInfo) {
				continue;
			}

			Bool itBetter;
			AsciiString testAgainstTeamName = teamInfo->getDict()->getAsciiString(TheKey_teamName, &itBetter);
			if (itBetter) {
				if (testAgainstTeamName.compare(teamName) == 0) {
					valid = true;
				}
			}
		}

		if (!valid) {
			getProperties()->remove(TheKey_originalOwner);
		}
	}
}

void MapObject::verifyValidUniqueID()
{
	Bool exists;
	AsciiString uniqueID = getProperties()->getAsciiString(TheKey_uniqueID, &exists);
	MapObject *obj = MapObject::getFirstMapObject();

	// -1 is the sentinel
	int highestIndex = -1;

	while (obj) {
		if (obj == this) {
			// the first object is THIS OBJECT, cause we've already been added.
			obj = obj->getNext();
			continue;
		}

		if (obj->isWaypoint()) {
			// waypoints throw this off. Sad but true. :-(
			obj = obj->getNext();
			continue;
		}

		Bool iterateExists;
		AsciiString tempStr = obj->getProperties()->getAsciiString(TheKey_uniqueID, &iterateExists);
		const char* lastSpace = tempStr.reverseFind(' ');

		int testIndex = -1;
		if (lastSpace) {
			testIndex = atoi(lastSpace);
		}

		if (testIndex > highestIndex) {
			highestIndex = testIndex;
		}
		break;
	}

	int indexOfThisObject = highestIndex + 1;

	const char* thingName;
	if (getThingTemplate()) {
		thingName = getThingTemplate()->getName().str();
	} else if (isWaypoint()) {
		thingName = getWaypointName().str();
	} else {
		thingName = getName().str();
	}
	const char* pName = thingName;

	while (*thingName) {
		if ((*thingName) == '/') {
			pName = thingName + 1;
		}
		++thingName;
	}

	AsciiString newID;
	if (isWaypoint()) {
		newID.format("%s", pName);
	} else {
		newID.format("%s %d", pName, indexOfThisObject);
	}
	getProperties()->setAsciiString(TheKey_uniqueID, newID);
}

void MapObject::fastAssignAllUniqueIDs()
{
	// here's what we do. Take all of them, push them onto a stack. Then, pop each one, setting its id.
	// should be much faster than what we currently do.

	MapObject *pMapObj = getFirstMapObject();

	std::stack<MapObject*> objStack;
	Int actualNumObjects = 0;

	while (pMapObj) {
		++actualNumObjects;
		objStack.push(pMapObj);
		pMapObj = pMapObj->getNext();
	}

	Int indexOfThisObject = 0;
	while (actualNumObjects) {
		MapObject *obj = objStack.top();


		const char* thingName;
		if (obj->getThingTemplate()) {
			thingName = obj->getThingTemplate()->getName().str();
		} else if (obj->isWaypoint()) {
			thingName = obj->getWaypointName().str();
		} else {
			thingName = obj->getName().str();
		}
		const char* pName = thingName;

		while (*thingName) {
			if ((*thingName) == '/') {
				pName = thingName + 1;
			}
			++thingName;
		}

		AsciiString newID;
		if (obj->isWaypoint()) {
			newID.format("%s", pName);
		} else {
			newID.format("%s %d", pName, indexOfThisObject);
		}

		obj->getProperties()->setAsciiString(TheKey_uniqueID, newID);
		objStack.pop();

		++indexOfThisObject;
		--actualNumObjects;
	}
}



void MapObject::setThingTemplate(const ThingTemplate *thing)
{
	m_thingTemplate = thing;
	m_objectName = thing->getName();
}


void MapObject::setName(AsciiString name)
{
	m_objectName = name;
}

WaypointID MapObject::getWaypointID() { return (WaypointID)getProperties()->getInt(TheKey_waypointID); }
AsciiString MapObject::getWaypointName() { return getProperties()->getAsciiString(TheKey_waypointName); }
void MapObject::setWaypointID(Int i) { getProperties()->setInt(TheKey_waypointID, i); }
void MapObject::setWaypointName(AsciiString n) { getProperties()->setAsciiString(TheKey_waypointName, n); }

/*static */ Int MapObject::countMapObjectsWithOwner(const AsciiString& n)
{
	Int count = 0;
	for (MapObject *pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{
		if (pMapObj->getProperties()->getAsciiString(TheKey_originalOwner) == n)
			++count;
	}
	return count;
}

//-------------------------------------------------------------------------------------------------
const ThingTemplate *MapObject::getThingTemplate() const
{
	if (m_thingTemplate)
		return (const ThingTemplate*) m_thingTemplate->getFinalOverride();

	return nullptr;
}


/* ********* WorldHeightMap class ****************************/

TileData *WorldHeightMap::m_alphaTiles[NUM_ALPHA_TILES]={0};

//
// WorldHeightMap destructor .
//
WorldHeightMap::~WorldHeightMap()
{
	delete[](m_data);
	m_data = nullptr;

	delete[](m_tileNdxes);
	m_tileNdxes = nullptr;

	delete[](m_blendTileNdxes);
	m_blendTileNdxes = nullptr;

	delete[](m_extraBlendTileNdxes);
	m_extraBlendTileNdxes = nullptr;

	delete[](m_cliffInfoNdxes);
	m_cliffInfoNdxes = nullptr;

	delete[](m_cellFlipState);
	m_cellFlipState = nullptr;

	delete[](m_seismicUpdateFlag);
	m_seismicUpdateFlag = nullptr;

	delete[](m_seismicZVelocities);
	m_seismicZVelocities = nullptr;

	delete[](m_cellCliffState);
	m_cellCliffState = nullptr;

	int i;
	for (i=0; i<NUM_SOURCE_TILES; i++) {
		REF_PTR_RELEASE(m_sourceTiles[i]);
		REF_PTR_RELEASE(m_edgeTiles[i]);
	}
	for (i=0; i<NUM_ALPHA_TILES; i++) {
		REF_PTR_RELEASE(m_alphaTiles[i]);
	}
	REF_PTR_RELEASE(m_terrainTex);
	REF_PTR_RELEASE(m_alphaTerrainTex);
	REF_PTR_RELEASE(m_alphaEdgeTex);
}

void WorldHeightMap::freeListOfMapObjects()
{
	if (MapObject::TheMapObjectListPtr)
	{
		deleteInstance(MapObject::TheMapObjectListPtr);
		MapObject::TheMapObjectListPtr = nullptr;
	}
	MapObject::getWorldDict()->clear();
}


/**
 WorldHeightMap - create a new height map for class WorldHeightMap.
 Note that there is 1 m_numBlendedTiles, which is the implied
 transparent tile for non-blended tiles.
*/
WorldHeightMap::WorldHeightMap():
	m_width(0), m_height(0),  m_dataSize(0), m_data(nullptr), m_cellFlipState(nullptr), m_seismicUpdateFlag(nullptr), m_seismicZVelocities(nullptr),
	m_drawOriginX(0), m_drawOriginY(0),
	m_numTextureClasses(0),
	m_drawWidthX(NORMAL_DRAW_WIDTH), m_drawHeightY(NORMAL_DRAW_HEIGHT),
	m_tileNdxes(nullptr), m_blendTileNdxes(nullptr), m_extraBlendTileNdxes(nullptr), m_cliffInfoNdxes(nullptr),
	m_terrainTexHeight(1), m_alphaTexHeight(1),	m_cellCliffState(nullptr),
#ifdef EVAL_TILING_MODES
	m_tileMode(TILE_4x4),
#endif
	m_numCliffInfo(1),
	m_terrainTex(nullptr), m_alphaTerrainTex(nullptr), m_numBitmapTiles(0), m_numBlendedTiles(1)
{
	Int i;
	for (i=0; i<NUM_SOURCE_TILES; i++) {
		m_sourceTiles[i] = nullptr;
		m_edgeTiles[i] = nullptr;
	}

	TheSidesList->validateSides();
	setupAlphaTiles();
}

#ifdef EVAL_TILING_MODES
static Bool ParseFunkyTilingDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	WorldHeightMap *pThis = (WorldHeightMap *)userData;
	*((Int *)&pThis->m_tileMode) = file.readInt();
	return true;
}
#endif

/**
* WorldHeightMap - read a height map from a file.
* Format is  Chunky.
*
*	Input: ChunkInputStream,
*
*/
WorldHeightMap::WorldHeightMap(ChunkInputStream *pStrm, Bool logicalDataOnly):
	m_width(0), m_height(0),  m_dataSize(0), m_data(nullptr), m_cellFlipState(nullptr), m_seismicUpdateFlag(nullptr), m_seismicZVelocities(nullptr),
	m_drawOriginX(0),	m_cellCliffState(nullptr), m_drawOriginY(0),
	m_numTextureClasses(0),
	m_drawWidthX(NORMAL_DRAW_WIDTH), m_drawHeightY(NORMAL_DRAW_HEIGHT),
	m_tileNdxes(nullptr), m_blendTileNdxes(nullptr), m_extraBlendTileNdxes(nullptr), m_cliffInfoNdxes(nullptr),
	m_terrainTexHeight(1), m_alphaTexHeight(1),
#ifdef EVAL_TILING_MODES
	m_tileMode(TILE_4x4),
#endif
	m_numCliffInfo(1),
	m_terrainTex(nullptr), m_alphaTerrainTex(nullptr), m_numBitmapTiles(0), m_numBlendedTiles(1)
{

	int i;
	for (i=0; i<NUM_SOURCE_TILES; i++) {
		m_sourceTiles[i]=nullptr;
		m_edgeTiles[i]=nullptr;
	}
	if (TheGlobalData && TheGlobalData->m_stretchTerrain) {
		m_drawWidthX=STRETCH_DRAW_WIDTH;
		m_drawHeightY=STRETCH_DRAW_HEIGHT;
	}

	DataChunkInput file( pStrm );

	if (logicalDataOnly) {
		file.registerParser( "HeightMapData", AsciiString::TheEmptyString, ParseSizeOnlyInChunk );
		file.registerParser( "WorldInfo", AsciiString::TheEmptyString, ParseWorldDictDataChunk );
		file.registerParser( "ObjectsList", AsciiString::TheEmptyString, ParseObjectsDataChunk );
		freeListOfMapObjects(); // just in case.
		file.registerParser( "PolygonTriggers", AsciiString::TheEmptyString, PolygonTrigger::ParsePolygonTriggersDataChunk );
		PolygonTrigger::deleteTriggers(); // just in case.
		TheSidesList->emptySides();
		file.registerParser("SidesList", AsciiString::TheEmptyString,	SidesList::ParseSidesDataChunk );
	}	else {
		file.registerParser( "HeightMapData", AsciiString::TheEmptyString, ParseHeightMapDataChunk );
		file.registerParser( "BlendTileData", AsciiString::TheEmptyString, ParseBlendTileDataChunk );
#ifdef EVAL_TILING_MODES
		file.registerParser( "FUNKY_TILING", AsciiString::TheEmptyString, ParseFunkyTilingDataChunk );
#endif
		file.registerParser( "GlobalLighting", AsciiString::TheEmptyString, ParseLightingDataChunk );
	}
	if (!file.parse(this)) {

		throw(ERROR_CORRUPT_FILE_FORMAT);
	}
	// patch bad maps.
	if (!logicalDataOnly) {
		for(i=0; i<m_dataSize; i++) {
			if (m_cliffInfoNdxes[i]<0 || m_cliffInfoNdxes[i]>= m_numCliffInfo) {
				m_cliffInfoNdxes[i] = 0;
			}
			if (m_blendTileNdxes[i]<0 || m_blendTileNdxes[i]>= m_numBlendedTiles) {
				m_blendTileNdxes[i] = 0;
			}
			if (m_extraBlendTileNdxes[i]<0 || m_extraBlendTileNdxes[i]>= m_numBlendedTiles) {
				m_extraBlendTileNdxes[i] = 0;
			}
		}
	}
	if (TheGlobalData && TheGlobalData->m_drawEntireTerrain) {
		m_drawWidthX=m_width;
		m_drawHeightY=m_height;
	}
	if (m_drawWidthX > m_width) {
		m_drawWidthX = m_width;
	}
	if (m_drawHeightY > m_height) {
		m_drawHeightY = m_height;
	}

	TheSidesList->validateSides();
	setupAlphaTiles();
}

/** Optimized version of method to get triangle flip state of a terrain cell.  Use this
*	instead of getAlphaUVData() whenever possible.
*/
Bool WorldHeightMap::getFlipState(Int xIndex, Int yIndex) const
{
	if (xIndex<0 || yIndex<0) return false;
	if (yIndex>=m_height) return false;
	if (xIndex>=m_width) return false;
	if (!m_cellFlipState) return false;
	return m_cellFlipState[yIndex*m_flipStateWidth + (xIndex >> 3)] & (1<<(xIndex&0x7));
}

/** Sets the value of the flip state bit.
*/
void WorldHeightMap::setFlipState(Int xIndex, Int yIndex, Bool value)
{
	if (xIndex<0 || yIndex<0) return ;
	if (yIndex>=m_height) return ;
	if (xIndex>=m_width) return ;
	if (!m_cellFlipState) return ;
	UnsignedByte *curVal = &m_cellFlipState[yIndex*m_flipStateWidth + (xIndex >> 3)];
	if (value) {
		*curVal |= (1<<(xIndex&0x7));
	}	else {
		*curVal &= ~(1<<(xIndex&0x7));
	}
}

/** Clears all flip state bits.
*/
void WorldHeightMap::clearFlipStates() {
	if (m_cellFlipState) {
		memset(m_cellFlipState,0,m_flipStateWidth*m_height);	//clear all flags
	}
}

//////////////////////////////////////////////////////////////////////////////m_SeismicUpdateFlag
Bool WorldHeightMap::getSeismicUpdateFlag(Int xIndex, Int yIndex) const
{
	if (xIndex<0 || yIndex<0) return false;
	if (yIndex>=m_height) return false;
	if (xIndex>=m_width) return false;
	if (!m_seismicUpdateFlag) return false;
	return m_seismicUpdateFlag[yIndex*m_seismicUpdateWidth + (xIndex >> 3)] & (1<<(xIndex&0x7));
}
void WorldHeightMap::setSeismicUpdateFlag(Int xIndex, Int yIndex, Bool value)
{
	if (xIndex<0 || yIndex<0) return ;
	if (yIndex>=m_height) return ;
	if (xIndex>=m_width) return ;
	if (!m_seismicUpdateFlag) return ;
	UnsignedByte *curVal = &m_seismicUpdateFlag[yIndex*m_seismicUpdateWidth + (xIndex >> 3)];
	if (value) {
		*curVal |= (1<<(xIndex&0x7));
	}	else {
		*curVal &= ~(1<<(xIndex&0x7));
	}
}
void WorldHeightMap::clearSeismicUpdateFlags()
{
	if (m_seismicUpdateFlag) {
		memset(m_seismicUpdateFlag,0,m_seismicUpdateWidth*m_height);	//clear all flags
	}
}

///////////////////////////////////////////////m_SeismicZVelocities
Real WorldHeightMap::getSeismicZVelocity(Int xIndex, Int yIndex) const
{
	if (xIndex<0 || yIndex<0) return false;
	if (yIndex>=m_height) return false;
	if (xIndex>=m_width) return false;
	if (!m_seismicZVelocities) return false;
	return m_seismicZVelocities[yIndex*m_width + xIndex];
}
void WorldHeightMap::setSeismicZVelocity(Int xIndex, Int yIndex, Real value)
{
	if (xIndex<0 || yIndex<0) return ;
	if (yIndex>=m_height) return ;
	if (xIndex>=m_width) return ;
	if (!m_seismicZVelocities) return ;
	m_seismicZVelocities[yIndex*m_width + xIndex] = value;
}
void WorldHeightMap::fillSeismicZVelocities( Real value )
{
	if (!m_seismicZVelocities) return ;
  for (Int idx = 0; idx < m_width*m_height; ++idx)
    m_seismicZVelocities[idx] = value;
}

Real WorldHeightMap::getBilinearSampleSeismicZVelocity( Int x, Int y)
{
	if ( x < 0 || y < 0 ) return 0;
	if ( y >= m_height ) return 0;
	if ( x >= m_width ) return 0;
	if (!m_seismicZVelocities) return 0;

  Real collector = 0.0f;
  Real divisor = 0.0f;

  collector += m_seismicZVelocities[ y * m_width + x ];
  ++divisor;

  if ( y > 0 )
  {
    collector += m_seismicZVelocities[ (y-1) * m_width + x ];//bottom
    ++divisor;

    if( x > 0 )
    {
      collector += m_seismicZVelocities[ (y-1) * m_width + (x-1) ];//lower left
      ++divisor;
    }
    if ( x < m_width-1 )
    {
      collector += m_seismicZVelocities[ (y-1) * m_width + (x+1) ];//lower right
      ++divisor;
    }
  }
  if ( y < m_height-1 )
  {
    collector += m_seismicZVelocities[ (y+1) * m_width + x ];//top
    ++divisor;

    if( x > 0 )
    {
      collector += m_seismicZVelocities[ (y+1) * m_width + (x-1) ];//upper left
      ++divisor;
    }
    if ( x < m_width-1 )
    {
      collector += m_seismicZVelocities[ (y+1) * m_width + (x+1) ];//upper right
      ++divisor;
    }
  }
  if( x > 0 )
  {
    collector += m_seismicZVelocities[ y * m_width + (x-1) ];//left
    ++divisor;
  }
  if ( x < m_width-1 )
  {
    collector += m_seismicZVelocities[ y * m_width + (x+1) ];//right
    ++divisor;
  }

  collector /= divisor;

  return collector;

}

/** Get whether the cell is a cliff cell (impassable to ground vehicles).
*/
Bool WorldHeightMap::getCliffState(Int xIndex, Int yIndex) const
{
	if (xIndex<0 || yIndex<0) return false;
	if (yIndex>=m_height) return false;
	if (xIndex>=m_width) return false;
	if (!m_cellCliffState) return false;
	return m_cellCliffState[yIndex*m_flipStateWidth + (xIndex >> 3)] & (1<<(xIndex&0x7));
}

//=============================================================================
// setCliffState
//=============================================================================
/** Sets the cliff state for a given cell. */
//=============================================================================
void WorldHeightMap::setCliffState(Int xIndex, Int yIndex, Bool state)
{
	if (xIndex<0 || yIndex<0) return;
	if (yIndex>=m_height) return;
	if (xIndex>=m_width) return;
	if (!m_cellCliffState) return;
	UnsignedByte	flagByte = m_cellCliffState[yIndex*m_flipStateWidth + (xIndex >> 3)];
	UnsignedByte flagMask = (1<<(xIndex&0x7));
	if (state) {
		flagByte |= flagMask;
	} else {
		flagByte &= (~flagMask);
	}
	m_cellCliffState[yIndex*m_flipStateWidth + (xIndex >> 3)] = flagByte;
}

Bool WorldHeightMap::ParseWorldDictDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	Dict d = file.readDict();
	*MapObject::getWorldDict() = d;
	Bool exists;
	Int theWeather = MapObject::getWorldDict()->getInt(TheKey_weather, &exists);
	if (exists) {
		TheWritableGlobalData->m_weather = (Weather) theWeather;
	}
	return true;
}

/**
* WorldHeightMap::ParseLightingDataChunk - read a global lights chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseLightingDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
		TheWritableGlobalData->m_timeOfDay = (TimeOfDay)file.readInt();
		Int i;
		GlobalData::TerrainLighting	initLightValues	= { { 0,0,0},{0,0,0},{0,0,-1.0f}};

		// initialize the directions of the lights to not be totally invalid, in case old maps are read
		for (i=0; i<4; i++) {
			for (Int j=0;j<MAX_GLOBAL_LIGHTS; j++) {
				TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j]=initLightValues;
				TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j]=initLightValues;
			}
		}

		for (i=0; i<4; i++) {
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].ambient.red = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].ambient.green = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].ambient.blue = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].diffuse.red = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].diffuse.green = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].diffuse.blue = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].lightPos.x = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].lightPos.y = file.readReal();
			TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][0].lightPos.z = file.readReal();

			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].ambient.red = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].ambient.green = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].ambient.blue = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].diffuse.red = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].diffuse.green = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].diffuse.blue = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].lightPos.x = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].lightPos.y = file.readReal();
			TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][0].lightPos.z = file.readReal();

			if (info->version >= K_LIGHTING_VERSION_2) {
				for (Int j=1; j<3; j++)	//added support for 2 extra object lights
				{
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].ambient.red = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].ambient.green = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].ambient.blue = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].diffuse.red = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].diffuse.green = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].diffuse.blue = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].lightPos.x = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].lightPos.y = file.readReal();
					TheWritableGlobalData->m_terrainObjectsLighting[i+TIME_OF_DAY_FIRST][j].lightPos.z = file.readReal();
				}
			}
			if (info->version >= K_LIGHTING_VERSION_3) {
				for (Int j=1; j<3; j++)	//added support for 2 extra terrain lights
				{
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].ambient.red = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].ambient.green = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].ambient.blue = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].diffuse.red = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].diffuse.green = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].diffuse.blue = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].lightPos.x = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].lightPos.y = file.readReal();
					TheWritableGlobalData->m_terrainLighting[i+TIME_OF_DAY_FIRST][j].lightPos.z = file.readReal();
				}
			}
		}
		if (!file.atEndOfChunk()) {
			UnsignedInt shadowColor = file.readInt();
			if (TheW3DShadowManager) {
				TheW3DShadowManager->setShadowColor(shadowColor);
			}
		}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

/**
* WorldHeightMap::ParseObjectsDataChunk - read a height map chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseObjectsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	file.m_currentObject = nullptr;
	file.registerParser( "Object", info->label, ParseObjectDataChunk );
	return (file.parse(userData));
}

/**
* WorldHeightMap::ParseHeightMapData - read a height map chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseHeightMapDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	WorldHeightMap *pThis = (WorldHeightMap *)userData;
	return pThis->ParseHeightMapData(file, info, userData);
}

/**
* WorldHeightMap::ParseHeightMapData - read a height map chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseHeightMapData(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	m_width = file.readInt();
	m_height = file.readInt();
	if (info->version >= K_HEIGHT_MAP_VERSION_3) {
		m_borderSize = file.readInt();
	} else {
		m_borderSize = 0;
	}

	if (info->version >= K_HEIGHT_MAP_VERSION_4) {
		Int numBorders = file.readInt();
		m_boundaries.resize(numBorders);
		for (int i = 0; i < numBorders; ++i) {
			m_boundaries[i].x = file.readInt();
			m_boundaries[i].y = file.readInt();
		}
	} else {
		m_boundaries.resize(1);
		m_boundaries[0].x = m_width - 2 * m_borderSize;
		m_boundaries[0].y = m_height - 2 * m_borderSize;
	}

	m_dataSize = file.readInt();
	m_data = MSGNEW("WorldHeightMap_ParseHeightMapData") UnsignedByte[m_dataSize];
	if (m_dataSize <= 0 || (m_dataSize != (m_width*m_height))) {
		throw ERROR_CORRUPT_FILE_FORMAT	;
	}

	Int numBytesX = (m_width+7)/8;	//how many bytes to fit all bitflags
	Int numBytesY = m_height;
	m_seismicUpdateWidth=numBytesX;
	m_seismicUpdateFlag	= MSGNEW("WorldHeightMap::ParseHeightMapData _ m_seismicUpdateFlag allocated") UnsignedByte[numBytesX*numBytesY];
  clearSeismicUpdateFlags();
  m_seismicZVelocities = MSGNEW("WorldHeightMap_ParseHeightMapData _ zvelocities allocated") Real[m_dataSize];
  fillSeismicZVelocities( 0 );


	file.readArrayOfBytes((char *)m_data, m_dataSize);
	// Resize me.
	if (info->version == K_HEIGHT_MAP_VERSION_1) {
		Int newWidth = (m_width+1)/2;
		Int newHeight = (m_height+1)/2;
		Int i, j;
		for (i=0; i<newHeight; i++) {
			for (j=0; j<newWidth; j++) {
				m_data[i*newWidth+j] = m_data[2*i*m_width+2*j];
			}
		}
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

/**
* WorldHeightMap::ParseHeightMapData - read a height map chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseSizeOnlyInChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	WorldHeightMap *pThis = (WorldHeightMap *)userData;
	return pThis->ParseSizeOnly(file, info, userData);
}

/**
* WorldHeightMap::ParseHeightMapData - read a height map chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseSizeOnly(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	m_width = file.readInt();
	m_height = file.readInt();
	if (info->version >= K_HEIGHT_MAP_VERSION_3) {
		m_borderSize = file.readInt();
	} else {
		m_borderSize = 0;
	}

	if (info->version >= K_HEIGHT_MAP_VERSION_4) {
		Int numBorders = file.readInt();
		m_boundaries.resize(numBorders);
		for (int i = 0; i < numBorders; ++i) {
			m_boundaries[i].x = file.readInt();
			m_boundaries[i].y = file.readInt();
		}
	} else {
		m_boundaries.resize(1);
		m_boundaries[0].x = m_width - 2 * m_borderSize;
		m_boundaries[0].y = m_height - 2 * m_borderSize;
	}

	m_dataSize = file.readInt();
	m_data = MSGNEW("WorldHeightMap_ParseSizeOnly") UnsignedByte[m_dataSize];
	if (m_dataSize <= 0 || (m_dataSize != (m_width*m_height))) {
		throw ERROR_CORRUPT_FILE_FORMAT	;
	}
	file.readArrayOfBytes((char *)m_data, m_dataSize);
	// Resize me.
	if (info->version == K_HEIGHT_MAP_VERSION_1) {
		Int newWidth = (m_width+1)/2;
		Int newHeight = (m_height+1)/2;
		Int i, j;
		for (i=0; i<newHeight; i++) {
			for (j=0; j<newWidth; j++) {
				m_data[i*newWidth+j] = m_data[2*i*m_width+2*j];
			}
		}
		m_width = newWidth;
		m_height = newHeight;
	}
	return true;
}

/**
* WorldHeightMap::ParseBlendTileDataChunk - read a blend tile info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseBlendTileDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	WorldHeightMap *pThis = (WorldHeightMap *)userData;
	return pThis->ParseBlendTileData(file, info, userData);
}

/** Function to read in the tiles for a texture class. */
void WorldHeightMap::readTexClass(TXTextureClass *texClass, TileData **tileData)
{
	char path[_MAX_PATH];
	path[0] = 0;
	File *theFile = nullptr;

	// get the file from the description in TheTerrainTypes
	TerrainType *terrain = TheTerrainTypes->findTerrain( texClass->name );
	char texturePath[ _MAX_PATH ];
	if (terrain==nullptr)
	{
#ifdef LOAD_TEST_ASSETS
		theFile = TheFileSystem->openFile( texClass->name.str(), File::READ|File::BINARY);
#endif
	}
	else
	{
		snprintf( texturePath, ARRAY_SIZE(texturePath), "%s%s", TERRAIN_TGA_DIR_PATH, terrain->getTexture().str() );
		theFile = TheFileSystem->openFile( texturePath, File::READ|File::BINARY);
	}

	if (theFile != nullptr) {
		GDIFileStream theStream(theFile);
		InputStream *pStr = &theStream;
		Int numTiles = WorldHeightMap::countTiles(pStr);
		theFile->seek(0, File::START);
		if (numTiles >= texClass->numTiles) {
			numTiles = texClass->numTiles;
			Int width;
			for (width = 10; width >= 1; width--) {
				if (numTiles >= width*width) {
					numTiles = width*width;
					break;
				}
			}
			WorldHeightMap::readTiles(pStr, tileData+texClass->firstTile, width);
		}
		theFile->close();
	}
}

/**
* WorldHeightMap::ParseBlendTileData - read a blend tile info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseBlendTileData(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	int i, j;
	Int len = file.readInt();
	if (m_dataSize != len) {
		throw ERROR_CORRUPT_FILE_FORMAT	;
	}
	m_tileNdxes = MSGNEW("WorldHeightMap_ParseBlendTileData") Short[m_dataSize];
	m_cliffInfoNdxes = MSGNEW("WorldHeightMap_ParseBlendTileData") Short[m_dataSize];
	m_blendTileNdxes = MSGNEW("WorldHeightMap_ParseBlendTileData") Short[m_dataSize];
	m_extraBlendTileNdxes = MSGNEW("WorldHeightMap_ParseBlendTileData") Short[m_dataSize];
	// Note - we have one less cell than the width & height. But for paranoia, allocate
	// extra row. jba.
	//
	Int numBytesX = (m_width+7)/8;	//how many bytes to fit all bitflags
	Int numBytesY = m_height;

	m_flipStateWidth=numBytesX;

	m_cellFlipState	= MSGNEW("WorldHeightMap_getTerrainTexture") UnsignedByte[numBytesX*numBytesY];
	m_cellCliffState	= MSGNEW("WorldHeightMap_getTerrainTexture") UnsignedByte[numBytesX*numBytesY];
	memset(m_cellFlipState,0,numBytesX*numBytesY);	//clear all flags
	memset(m_cellCliffState,0,numBytesX*numBytesY);	//clear all flags

	file.readArrayOfBytes((char*)m_tileNdxes, m_dataSize*sizeof(Short));
	file.readArrayOfBytes((char*)m_blendTileNdxes, m_dataSize*sizeof(Short));
	if (info->version >= K_BLEND_TILE_VERSION_6) {
		file.readArrayOfBytes((char*)m_extraBlendTileNdxes, m_dataSize*sizeof(Short));
		//Allow clearing of extra blend tiles via ini and resaving of map.
		//Useful for flushing out initial maps made with buggy 3-way blending.
		if (!TheGlobalData->m_use3WayTerrainBlends)
			memset(m_extraBlendTileNdxes,0,m_dataSize*sizeof(Short));
	}
	if (info->version >= K_BLEND_TILE_VERSION_5) {
		file.readArrayOfBytes((char*)m_cliffInfoNdxes, m_dataSize*sizeof(Short));
	}
	if (info->version >= K_BLEND_TILE_VERSION_7) {
		if (info->version==K_BLEND_TILE_VERSION_7) {
			Int byteWidth = (m_width+1)/8; // previous incorrect length that got used to save the file.  jba. [4/3/2003]
			UnsignedByte *data = new UnsignedByte[m_height*byteWidth];
			file.readArrayOfBytes((char*)data, m_height*byteWidth);
			for (j=0; j<m_height; j++) {
				for (i=0; i<byteWidth; i++) {
					m_cellCliffState[j*m_flipStateWidth + i] = data[j*byteWidth + i];
				}
			}
		} else {
			file.readArrayOfBytes((char*)m_cellCliffState, m_height*m_flipStateWidth);
		}
	} else {
		initCliffFlagsFromHeights();
	}
	m_numBitmapTiles = file.readInt();
	DEBUG_ASSERTCRASH(m_numBitmapTiles>0 && m_numBitmapTiles<2048, ("Unlikely numBitmapTiles."));
	m_numBlendedTiles = file.readInt();
	DEBUG_ASSERTCRASH(m_numBlendedTiles>0 && m_numBlendedTiles<NUM_BLEND_TILES+1, ("Unlikely numBlendedTiles."));
	if (info->version >= K_BLEND_TILE_VERSION_5) {
		m_numCliffInfo = file.readInt();
	} else {
		m_numCliffInfo = 1;	// cliffInfo[0] is the default info.
	}
// --> file loading here
	m_numTextureClasses	= file.readInt();
	DEBUG_ASSERTCRASH(m_numTextureClasses>0 && m_numTextureClasses<200, ("Unlikely m_numTextureClasses."));
	for (i=0; i<m_numTextureClasses; i++) {
		m_textureClasses[i].globalTextureClass = -1;
		m_textureClasses[i].firstTile = file.readInt();
		m_textureClasses[i].numTiles = file.readInt();
		m_textureClasses[i].width = file.readInt();

		// legacy GDF data
		// used to read "m_textureClasses[i].isGDF = file.readInt();"
	/*	Int legacy = */ file.readInt();

		m_textureClasses[i].name = file.readAsciiString();
		readTexClass(&m_textureClasses[i], m_sourceTiles);
	}
	m_numEdgeTextureClasses = 0;
	m_numEdgeTiles = 0;
	if (info->version >= K_BLEND_TILE_VERSION_4) {
		m_numEdgeTiles	= file.readInt();
		m_numEdgeTextureClasses	= file.readInt();
		for (i=0; i<m_numEdgeTextureClasses; i++) {
			m_edgeTextureClasses[i].globalTextureClass = -1;
			m_edgeTextureClasses[i].firstTile = file.readInt();
			m_edgeTextureClasses[i].numTiles = file.readInt();
			m_edgeTextureClasses[i].width = file.readInt();
			m_edgeTextureClasses[i].name = file.readAsciiString();
			readTexClass(&m_edgeTextureClasses[i], m_edgeTiles);
		}
	}
	for (i=1; i<m_numBlendedTiles; i++) {
		Int flag;
		m_blendedTiles[i].blendNdx = file.readInt();
		m_blendedTiles[i].horiz = file.readByte();
		m_blendedTiles[i].vert = file.readByte();
		m_blendedTiles[i].rightDiagonal = file.readByte();
		m_blendedTiles[i].leftDiagonal = file.readByte();
		m_blendedTiles[i].inverted = file.readByte();
		//Allow clearing of extra blend tiles via ini and resaving of map.
		//Useful for flushing out initial maps made with buggy 3-way blending.
		if (!TheGlobalData->m_use3WayTerrainBlends)
			m_blendedTiles[i].inverted &= ~FLIPPED_MASK;	//filter out extra flips from 3-way

		if (info->version >= K_BLEND_TILE_VERSION_3) {
			m_blendedTiles[i].longDiagonal = file.readByte();
		} else {
			m_blendedTiles[i].longDiagonal = false;
		}
		if (info->version >= K_BLEND_TILE_VERSION_4) {
			m_blendedTiles[i].customBlendEdgeClass = file.readInt();
		} else {
			m_blendedTiles[i].customBlendEdgeClass = -1;
		}

		flag = file.readInt();
		DEBUG_ASSERTCRASH(flag==FLAG_VAL, ("Invalid format."));
		if (flag != FLAG_VAL) {
			throw ERROR_CORRUPT_FILE_FORMAT;
		}
	}
	if (info->version >= K_BLEND_TILE_VERSION_5) {
		for (i=1; i<m_numCliffInfo; i++) {
			m_cliffInfo[i].tileIndex = file.readInt();
			m_cliffInfo[i].u0 = file.readReal();
			m_cliffInfo[i].v0 = file.readReal();
			m_cliffInfo[i].u1 = file.readReal();
			m_cliffInfo[i].v1 = file.readReal();
			m_cliffInfo[i].u2 = file.readReal();
			m_cliffInfo[i].v2 = file.readReal();
			m_cliffInfo[i].u3 = file.readReal();
			m_cliffInfo[i].v3 = file.readReal();
			m_cliffInfo[i].flip = file.readByte();
			m_cliffInfo[i].mutant = file.readByte();
		}
	}
	// Resize me.
	if (info->version == K_BLEND_TILE_VERSION_1) {
		Int newWidth = (m_width+1)/2;
		Int newHeight = (m_height+1)/2;
		Int i, j;
		for (i=0; i<newHeight; i++) {
			for (j=0; j<newWidth; j++) {
				m_tileNdxes[i*newWidth+j] = m_tileNdxes[2*i*m_width+2*j];
				m_blendTileNdxes[i*newWidth+j] = 0;
				m_extraBlendTileNdxes[i*newWidth+j] = 0;
				m_cliffInfoNdxes[i*newWidth+j] = 0;
			}
		}
		m_numBlendedTiles = 1;
		m_numCliffInfo = 1;
		m_width= newWidth;
		m_height = newHeight;
		m_dataSize = m_width*m_height;
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}


/**
* WorldHeightMap::ParseObjectData - read a object info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseObjectDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	WorldHeightMap *pThis = (WorldHeightMap *)file.m_userData;
	return pThis->ParseObjectData(file, info, userData, info->version >= K_OBJECTS_VERSION_2);
}

/**
* WorldHeightMap::ParseObjectData - read a object info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput
*
*/
Bool WorldHeightMap::ParseObjectData(DataChunkInput &file, DataChunkInfo *info, void *userData, Bool readDict)
{
	MapObject *pPrevious = (MapObject *)file.m_currentObject;

	Coord3D loc;
	loc.x = file.readReal();
	loc.y = file.readReal();
	loc.z = file.readReal();

	Real minZ = -100*MAP_XY_FACTOR;
	Real maxZ = (255*10)*MAP_HEIGHT_SCALE;

	if (info->version <= K_OBJECTS_VERSION_2) {
		loc.z = 0;
	}

	Real angle = file.readReal();
	Int flags = file.readInt();
	AsciiString name = file.readAsciiString();
	Dict d;
	if (readDict)
	{
		d = file.readDict();
	}

	if (loc.z<minZ || loc.z>maxZ) {
		DEBUG_LOG(("Removing object at z height %f", loc.z));
		return true;
	}

	MapObject *pThisOne;

	// create the map object
	pThisOne = newInstance( MapObject )( loc, name, angle, flags, &d,
														TheThingFactory->findTemplate( name, FALSE ) );

//DEBUG_LOG(("obj %s owner %s",name.str(),d.getAsciiString(TheKey_originalOwner).str()));

	if (pThisOne->getProperties()->getType(TheKey_waypointID) == Dict::DICT_INT)
		pThisOne->setIsWaypoint();

	if (pThisOne->getProperties()->getType(TheKey_lightHeightAboveTerrain) == Dict::DICT_REAL)
		pThisOne->setIsLight();

	if (pThisOne->getProperties()->getType(TheKey_scorchType) == Dict::DICT_INT)
		pThisOne->setIsScorch();


	if (pPrevious) {
		DEBUG_ASSERTCRASH(MapObject::TheMapObjectListPtr != nullptr && pPrevious->getNext() == nullptr, ("Bad linkage."));
		pPrevious->setNextMap(pThisOne);
	}	else {
		DEBUG_ASSERTCRASH(MapObject::TheMapObjectListPtr == nullptr, ("Bad linkage."));
		MapObject::TheMapObjectListPtr = pThisOne;
	}
	file.m_currentObject = pThisOne;
	return true;
}



// Targa format:  Header

typedef struct {
	UnsignedByte	idLength;
	UnsignedByte	colorMapType; // 0 = rgb, 1 = indexed.
	UnsignedByte	imageType; //0x1 = indexed, 0x2 = rgb, 0x8 = rle.
	UnsignedByte	colorMapInfo[5]; // we ignore, only do rgb.
	Short			xOrigin;
	Short			yOrigin;
	Short			imageWidth;
	Short			imageHeight;
	UnsignedByte	pixelDepth;
	UnsignedByte	flags; //  &0x0F = alpha channel bits, &0x10 is right to left flag,
						   // 0x20 is top to bottom flag.  (0x0? is left to right, bottom to top)
						   // 0x3? is top to bottom, right to left.
} TTargaHeader;

// followed by idLength bytes of ascii data

// followed by pixel data

// followed by optional data.



/// Count how many tiles come in from a targa file.
Int WorldHeightMap::countTiles(InputStream *pStr, Bool *halfTile)
{
	TTargaHeader hdr;
	if (halfTile) {
		*halfTile = false;
	}
	Int len = pStr->read(&hdr,sizeof(hdr));
	if (len!=sizeof(hdr)) return(0);
	Int tileWidth = hdr.imageWidth/TILE_PIXEL_EXTENT;
	Int tileHeight = hdr.imageHeight/TILE_PIXEL_EXTENT;

	if (hdr.colorMapType != 0) {
		return(0); // we don't do indexed at this time. jba.
	}
	if (hdr.imageType != 0x2 && hdr.imageType != 0xA) {
		return(0); // we don't do indexed at this time. jba.
	}

	if (hdr.pixelDepth < 24) return(false);
	if (hdr.pixelDepth > 32) return(false);
	// 3x3 gives 9,
	// 2x2 gives 4,
	// 1x1 gives 1,
	// else 0;
	if (tileWidth>10 || tileHeight>10) return(0);  // don't do huge images, or bad files.
	if (tileWidth>=10 && tileHeight >=10) return(100);
	if (tileWidth>=9 && tileHeight >=9) return(81);
	if (tileWidth>=8 && tileHeight >=8) return(64);
	if (tileWidth>=7 && tileHeight >=7) return(49);
	if (tileWidth>=6 && tileHeight >=6) return(36);
	if (tileWidth>=5 && tileHeight >=5) return(25);
	if (tileWidth>=4 && tileHeight >=4) return(16);
	if (tileWidth>=3 && tileHeight >=3) return(9);
	if (tileWidth>=2 && tileHeight >=2) return(4);
	if (tileWidth>=1 && tileHeight >=1) return(1);
	if (halfTile && hdr.imageHeight==TILE_PIXEL_EXTENT/2 && hdr.imageWidth==TILE_PIXEL_EXTENT/2) {
		*halfTile = true;
		return 1;
	}
	return(0);
}
/*Break down a .tga file into a collection of tiles.  numRows * numRows total tiles.*/
Bool WorldHeightMap::readTiles(InputStream *pStr, TileData **tiles, Int numRows)
{
	TTargaHeader hdr;
	pStr->read(&hdr, sizeof(hdr));
	Int tileWidth = hdr.imageWidth/TILE_PIXEL_EXTENT;
	Int tileHeight = hdr.imageHeight/TILE_PIXEL_EXTENT;

	if (hdr.imageHeight==TILE_PIXEL_EXTENT/2) {
		tileHeight = 1;
	}
	if (hdr.imageWidth==TILE_PIXEL_EXTENT/2) {
		tileWidth = 1;
	}

	if (tileWidth<numRows && tileHeight<numRows) {
		return(false);
	}
	Bool compressed = false;
	if (hdr.imageType & 0x08) {
		compressed = true;
	}
	int row = 0;
	int column = 0;
	int bytesPerPixel = (hdr.pixelDepth+7)/8;
	if (bytesPerPixel < 3) return(false);
	if (bytesPerPixel > 4) return(false);
	int i;
	for (i=0; i<numRows*numRows; i++) {
		if (tiles[i] == nullptr)
			tiles[i] = MSGNEW("WorldHeightMap_readTiles") TileData;
	}
	UnsignedByte buf[4];
	int repeatCount = 0;
//	Bool read = false;
	Bool running = false;
	for (row = 0; row < numRows*TILE_PIXEL_EXTENT; row++) {
		for (column=0; column<hdr.imageWidth; column++) {
			UnsignedByte r, g, b, a;
			if (row < hdr.imageHeight) {
				if (compressed && repeatCount==0) {
					UnsignedByte flag;
					pStr->read(&flag, 1);
					repeatCount = flag&0x7f;
					repeatCount++;
					if (flag&0x80) {
						running = true;
						pStr->read(buf, bytesPerPixel);
					} else {
						running = false;
					}
				}
				if (compressed) repeatCount--;
				if (!running) {
					pStr->read(buf, bytesPerPixel);
				}
				r = buf[2]; g = buf[1]; b = buf[0];
				if (bytesPerPixel==4) {
					a = buf[3];
				} else {
					a = 255;// solid alpha.
				}
			} else {
				r = g = b = a = 0;
			}
			if (column >= (numRows*TILE_PIXEL_EXTENT)) continue;
			int tileNdx = (column/TILE_PIXEL_EXTENT) + numRows*(row/TILE_PIXEL_EXTENT);
			int pixelNdx = (column%TILE_PIXEL_EXTENT) + TILE_PIXEL_EXTENT*(row%TILE_PIXEL_EXTENT);

			UnsignedByte *pixel = tiles[tileNdx]->getDataPtr();

			pixel += pixelNdx*TILE_BYTES_PER_PIXEL;
			*pixel++ = b;
			*pixel++ = g;
			*pixel++ = r;
			*pixel = a;

		}
		DEBUG_ASSERTCRASH(repeatCount==0, ("Invalid tga."));
	}
	for (i=0; i<numRows*numRows; i++) {
		tiles[i]->updateMips();
	}
	return(true);
}



/** updateTileTexturePositions - assigns each tile a location in the texture.
*/
Int WorldHeightMap::updateTileTexturePositions(Int *edgeHeight)
{
	Int i, j;
	Int maxHeight = 0;
	const Int tilesPerRow = TEXTURE_WIDTH/(TILE_PIXEL_EXTENT+TILE_OFFSET);

	Bool availableGrid[tilesPerRow][tilesPerRow];
	Int row, column;
	for (row=0; row<tilesPerRow; row++) {
		for (column=0; column<tilesPerRow; column++) {
			availableGrid[row][column] = true;
		}
	}

	for (i=0; i<m_numBitmapTiles; i++) {
		if (m_sourceTiles[i]) {
			m_sourceTiles[i]->m_tileLocationInTexture.x = 0;
			m_sourceTiles[i]->m_tileLocationInTexture.y = 0;
		}
	}

	/* put the normal tiles into the terrain texture */
	Int texClass;
	Int tileWidth;
	for (tileWidth = tilesPerRow; tileWidth>0; tileWidth--) {
		for (texClass=0; texClass<m_numTextureClasses; texClass++) {
			Int width = m_textureClasses[texClass].width;
			if (width != tileWidth) continue;
			// Find an available block of space.
			Bool found = false;
			for (row=0; row<tilesPerRow-width+1 && !found; row++) {
				for (column=0; column<tilesPerRow-width+1 && !found; column++) {
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
				m_textureClasses[texClass].positionInTexture.x = 0;
				m_textureClasses[texClass].positionInTexture.y = 0;
				continue;
			}

			Int xOrigin = TILE_OFFSET/2 + column*(TILE_PIXEL_EXTENT+TILE_OFFSET);
			Int yOrigin = TILE_OFFSET/2 + row*(TILE_PIXEL_EXTENT+TILE_OFFSET);
			m_textureClasses[texClass].positionInTexture.x = xOrigin;
			m_textureClasses[texClass].positionInTexture.y = yOrigin;
			Int classHeight = yOrigin + width*TILE_PIXEL_EXTENT+ TILE_OFFSET/2;
			if (maxHeight < classHeight) maxHeight = classHeight;

			for (i=0; i<width; i++) {
				for (j=0; j<width; j++) {
					availableGrid[row+j][column+i] = false;
					Int baseNdx = m_textureClasses[texClass].firstTile + i + j*width;
					// In case we are just checking for room...
					if (m_sourceTiles[baseNdx] == nullptr) continue;
					Int x = xOrigin + i*TILE_PIXEL_EXTENT;
					Int y = yOrigin + (width-j-1)*TILE_PIXEL_EXTENT;
					m_sourceTiles[baseNdx]->m_tileLocationInTexture.x = x;
					m_sourceTiles[baseNdx]->m_tileLocationInTexture.y = y;
				}
			}
		}
	}

	for (i=0; i<m_numBitmapTiles; i++) {
		if (m_edgeTiles[i]) {
			m_edgeTiles[i]->m_tileLocationInTexture.x = 0;
			m_edgeTiles[i]->m_tileLocationInTexture.y = 0;
		}
	}

	/* put the blend edge tiles into the blend edges texture */
	Int maxEdgeHeight = 0;
	// Reset the grid, cause we're using a different texture now.
	for (row=0; row<tilesPerRow; row++) {
		for (column=0; column<tilesPerRow; column++) {
			availableGrid[row][column] = true;
		}
	}
	for (texClass=0; texClass<m_numEdgeTextureClasses; texClass++) {
		Int width = m_edgeTextureClasses[texClass].width;
		// Find an available block of space.
		Bool found = false;
		for (row=0; row<tilesPerRow-width+1 && !found; row++) {
			for (column=0; column<tilesPerRow-width+1 && !found; column++) {
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
			m_edgeTextureClasses[texClass].positionInTexture.x = 0;
			m_edgeTextureClasses[texClass].positionInTexture.y = 0;
			continue;
		}

		Int xOrigin = TILE_OFFSET/2 + column*(TILE_PIXEL_EXTENT+TILE_OFFSET);
		Int yOrigin = TILE_OFFSET/2 + row*(TILE_PIXEL_EXTENT+TILE_OFFSET);
		m_edgeTextureClasses[texClass].positionInTexture.x = xOrigin;
		m_edgeTextureClasses[texClass].positionInTexture.y = yOrigin;
		Int classHeight = yOrigin + width*TILE_PIXEL_EXTENT+ TILE_OFFSET/2;
		if (maxEdgeHeight < classHeight) maxEdgeHeight = classHeight;

		for (i=0; i<width; i++) {
			for (j=0; j<width; j++) {
				availableGrid[row+j][column+i] = false;
				Int baseNdx = m_edgeTextureClasses[texClass].firstTile + i + j*width;
				// In case we are just checking for room...
				if (m_edgeTiles[baseNdx] == nullptr) continue;
				Int x = xOrigin + i*TILE_PIXEL_EXTENT;
				Int y = yOrigin + (width-j-1)*TILE_PIXEL_EXTENT;
				// Use negative offsets to differentiate between tiles & edges.
				m_edgeTiles[baseNdx]->m_tileLocationInTexture.x = x;
				m_edgeTiles[baseNdx]->m_tileLocationInTexture.y = y;
			}
		}
	}
	if (edgeHeight) *edgeHeight = maxEdgeHeight;
	return maxHeight;
}

/** getUVData - Gets the texture coordinates to use.  See getTerrainTexture.
*/
void WorldHeightMap::getUVForNdx(Int tileNdx, float *minU, float *minV, float *maxU, float*maxV)
{
	Short baseNdx = tileNdx>>2;
	if (m_sourceTiles[baseNdx] == nullptr) {
		// Missing texture.
		*minU = *minV = *maxU = *maxV = 0.0f;
		return;
	}
	ICoord2D pos = m_sourceTiles[baseNdx]->m_tileLocationInTexture;
	*minU = pos.x;
	*minV = pos.y;
	*maxU = *minU+TILE_PIXEL_EXTENT;
	*maxV = *minV+TILE_PIXEL_EXTENT;
#ifdef EVAL_TILING_MODES
	if (m_tileMode == TILE_8x8) {
		*maxU = *minU+TILE_PIXEL_EXTENT/2.0f;
		*maxV = *minV+TILE_PIXEL_EXTENT/2.0f;
	} else if (m_tileMode == TILE_6x6) {
		*maxU = *minU+2.0f*TILE_PIXEL_EXTENT/3.0f;
		*maxV = *minV+2.0f*TILE_PIXEL_EXTENT/3.0f;
	} else {
		*maxU = *minU+TILE_PIXEL_EXTENT;
		*maxV = *minV+TILE_PIXEL_EXTENT;
	}
#endif
	*minU/=TEXTURE_WIDTH;
	*minV/=m_terrainTexHeight;
	*maxU/=TEXTURE_WIDTH;
	*maxV/=m_terrainTexHeight;

	// Tiles are 64x64 pixels, height grids map to 32x32.
	// So get the proper quadrant of the tile.
	Real midX = (*minU+*maxU)/2;
	Real midY = (*minV+*maxV)/2;
	if (tileNdx&2) {		// y's are flipped.
		*maxV = midY;
	} else {
		*minV = midY;
	}
	if (tileNdx&1) {
		*minU = midX;
	} else {
		*maxU = midX;
	}
}

/** getUVData - Gets the texture coordinates to use.  See getTerrainTexture.
*/
void WorldHeightMap::getUVForBlend(Int edgeClass, Region2D *range)
{
	ICoord2D pos = m_edgeTextureClasses[edgeClass].positionInTexture;
	Int width = m_edgeTextureClasses[edgeClass].width;
	range->lo.x = (Real)pos.x/TEXTURE_WIDTH;
	range->lo.y = (Real)pos.y/m_alphaEdgeHeight;
	range->hi.x = ((Real)pos.x + width*TILE_PIXEL_EXTENT)/TEXTURE_WIDTH;
	range->hi.y = ((Real)pos.y + width*TILE_PIXEL_EXTENT)/m_alphaEdgeHeight;
}

/// Get whether something is cliff indexed with the offset that HeightMapRenderObjClass uses built in.
Bool WorldHeightMap::isCliffMappedTexture(Int x, Int y) {
	Int ndx = x+m_drawOriginX+m_width*(y+m_drawOriginY);
	if (ndx>=0 && ndx<m_dataSize) {
		return m_cliffInfoNdxes[ndx] != 0;
	}
	return false;
};

/** getUVData - Gets the texture coordinates to use.  See getTerrainTexture.
		xIndex and yIndex are the integer coordinates into the height map.
		U and V are the texture coordinates for the 4 corners of a height map cell.
*/
Bool WorldHeightMap::getUVData(Int xIndex, Int yIndex, float U[4], float V[4])
{
#define dont_SHOW_THE_TEXTURE_FOR_DEBUG 1
#if SHOW_THE_TEXTURE_FOR_DEBUG
		// This is debug code that just shows the generated texture laid on the terrain.
		// For debugging ;) jba.
		xIndex += m_drawOriginX;
		yIndex += m_drawOriginY;
		float nU= xIndex;
		float xU = xIndex+1;
		float nV = 48-yIndex-1;
		float xV = 48-yIndex;
		float k = 48;
		nU /= k;
		xU /= k;
		k = k*m_terrainTexHeight/TEXTURE_WIDTH;
		nV /= k;
		xV /= k;
		U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
		V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
		return(true);
#else
	xIndex += m_drawOriginX;
	yIndex += m_drawOriginY;
	Int ndx = (yIndex*m_width)+xIndex;
	if ((ndx<m_dataSize) && m_tileNdxes) {
		Short tileNdx = m_tileNdxes[ndx];
		return getUVForTileIndex(ndx, tileNdx, U, V);
	}
	return false;
#endif
}

/** getUVForTileIndex - Gets the texture coordinates to use.  See getTerrainTexture.
		ndx is the index into the linear height array.
		tileNdx is the index into the texture tiles array.
		U and V are the texture coordinates for the 4 corners of a height map cell.
*/

Bool WorldHeightMap::getUVForTileIndex(Int ndx, Short tileNdx, float U[4], float V[4])
{
	Real nU, nV, xU, xV;
	nU=nV=xU=xV = 0.0f;
	Int tilesPerRow = TEXTURE_WIDTH/(2*TILE_PIXEL_EXTENT+TILE_OFFSET);
	tilesPerRow *= 4;

	if ((ndx<m_dataSize) && m_tileNdxes) {
		getUVForNdx(tileNdx, &nU, &nV, &xU, &xV);
		U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
		V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
		if (TheGlobalData && !TheGlobalData->m_adjustCliffTextures) {
			return false;
		}
		if (nU==0.0) {
			return false; // missing texture.
		}
		if (m_cliffInfoNdxes[ndx]) {
			TCliffInfo info = m_cliffInfo[m_cliffInfoNdxes[ndx]];
			Bool tilesMatch = false;
			Int ndx1 = tileNdx>>2;
			Int ndx2 = info.tileIndex>>2;
			Int i;
			for (i=0; i<this->m_numTextureClasses; i++) {
				if (ndx1 >= m_textureClasses[i].firstTile && ndx1 < m_textureClasses[i].firstTile + m_textureClasses[i].numTiles) {
					tilesMatch = ndx2 >= m_textureClasses[i].firstTile && ndx2 < m_textureClasses[i].firstTile + m_textureClasses[i].numTiles;
					//tilesMatch = true;
					break;
				}
			}
			if (tilesMatch) {
				Real minU = m_textureClasses[i].positionInTexture.x;
				Real maxV = m_textureClasses[i].positionInTexture.y + m_textureClasses[i].width*TILE_PIXEL_EXTENT;
				minU/=TEXTURE_WIDTH;
				maxV/=m_terrainTexHeight;
				Real vFactor = TEXTURE_WIDTH/m_terrainTexHeight;
				U[0] = info.u0+minU;
				U[1] = info.u1+minU;
				U[2] = info.u2+minU;
				U[3] = info.u3+minU;
				V[0] = info.v0*vFactor+maxV;
				V[1] = info.v1*vFactor+maxV;
				V[2] = info.v2*vFactor+maxV;
				V[3] = info.v3*vFactor+maxV;
				return info.flip;
			}
		}

// TheSuperHackers @info xezon 11/12/2025 The old uv adjustment for cliffs produces bad uv tiles on steep terrain
// and is also not helping performance. But we cannot just remove it, because it is required to render smooth
// steep diagonal slopes.
#define DO_OLD_UV
#ifdef DO_OLD_UV
// old uv adjustment for cliffs
		static Real STRETCH_LIMIT = 1.5f;	 // If it is stretching less than this, don't adjust.
		static Real TILE_LIMIT = 4.0;			// Our tiles are currently 4 cells wide & tall, so dont'
																			// adjust to more than 4.0.

		static Real TALL_STRETCH_LIMIT = 2.0f;
		static Real DIAMOND_STRETCH_LIMIT = 2.4f;
		static Real HEIGHT_SCALE = MAP_HEIGHT_SCALE / MAP_XY_FACTOR;

		Real nU, nV, xU, xV;
		nU=nV=xU=xV = 0.0f;
		Int tilesPerRow = TEXTURE_WIDTH/(2*TILE_PIXEL_EXTENT+TILE_OFFSET);
		tilesPerRow *= 4;


		getUVForNdx(tileNdx, &nU, &nV, &xU, &xV);
		U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
		V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
		if (TheGlobalData && !TheGlobalData->m_adjustCliffTextures) {
			return false;
		}
		if (nU==0.0) {
			return false; // missing texture.
		}
		// check for excessive heights.
		if (ndx < this->m_dataSize - m_width - 1) {
			Int h0 = m_data[ndx];
			Int h1 = m_data[ndx+1];
			Int h2 = m_data[ndx+m_width+1];
			Int h3 = m_data[ndx+m_width];
			Int minH, maxH;
			minH = maxH = h0;
			if (minH>h1) minH = h1;
			if (maxH<h1) maxH = h1;
			if (minH>h2) minH = h2;
			if (maxH<h2) maxH = h2;
			if (minH>h3) minH = h3;
			if (maxH<h3) maxH = h3;
//			Int avgH = (h1+h2+h3+h0+2)/4;
			Int deltaH = maxH-minH;
			Int below = 0;
			Int above = 0;
			Int belowLimit = minH+(2*deltaH+1)/3;
			Int aboveLimit = minH+(deltaH+1)/3;
			if (h0<belowLimit) below++;
			if (h1<belowLimit) below++;
			if (h2<belowLimit) below++;
			if (h3<belowLimit) below++;
			if (h0>aboveLimit) above++;
			if (h1>aboveLimit) above++;
			if (h2>aboveLimit) above++;
			if (h3>aboveLimit) above++;
			if (deltaH*HEIGHT_SCALE < STRETCH_LIMIT) {
				return false;
			}

			Short baseNdx = tileNdx>>2;
			Short texClass;
			for (texClass=0; texClass<m_numTextureClasses; texClass++) {
				if (m_textureClasses[texClass].firstTile<0) {
					continue;
				}
				// see if the blend tile is in a texture class, and get the right tile for xIndex, yIndex.
				if (baseNdx >= m_textureClasses[texClass].firstTile &&
					baseNdx < m_textureClasses[texClass].firstTile+m_textureClasses[texClass].numTiles) {
					break;
				}
			}
			if (texClass>= m_numTextureClasses) return false;
			Real nUb, nVb, xUb, xVb;
			nUb = m_textureClasses[texClass].positionInTexture.x;
			nVb = m_textureClasses[texClass].positionInTexture.y;
			xUb = nUb+m_textureClasses[texClass].width*TILE_PIXEL_EXTENT;
			xVb = nVb+m_textureClasses[texClass].width*TILE_PIXEL_EXTENT;
			nUb/=TEXTURE_WIDTH;
			nVb/=m_terrainTexHeight;
			xUb/=TEXTURE_WIDTH;
			xVb/=m_terrainTexHeight;
			// Now covers texture bounds.
			// too much stretch.
			Real divisor = TILE_LIMIT/(deltaH*HEIGHT_SCALE);
			if (divisor > TILE_LIMIT) divisor = TILE_LIMIT;
			if (divisor < 1.0f) divisor = 1.0f;
			Real deltaV = (xVb-nVb);
//			Real deltaU = (xUb-nUb);

			if (above != 1 && below != 1 && (above!=2 || below != 2)) {
				// diamond shaped.  Use default if it is not too stretched, as
				// the fix is not that appealing either.
				if (deltaH*HEIGHT_SCALE < DIAMOND_STRETCH_LIMIT) {
					return false;
				}
			}

			if (below==1 || above>below) { //(avgH > minH + (2*deltaH+2)/3)
				// we got one low guy.
				if (h0==minH) {
					V[0] = nV+deltaV/divisor;
				} else if (h1 == minH) {
					V[1] = nV+deltaV/divisor;
				}	else if (h2 == minH) {
					V[2] = xV-deltaV/divisor;
				} else if (h3 == minH) {
					V[3] = xV-deltaV/divisor;
				}
#if 0
				nU = nV = xU = xV = 1.0f;
				U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
				V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
				return false;
#endif
			}	else if (above==1 || below>above) { //(avgH < minH + (deltaH+1)/3)
				// we got one high guy
				if (h0==maxH) {
					V[0] = nV+deltaV/divisor;
				} else if (h1 == maxH) {
					V[1] = nV+deltaV/divisor;
				}	else if (h2 == maxH) {
					V[2] = xV-deltaV/divisor;
				} else if (h3 == maxH) {
					V[3] = xV-deltaV/divisor;
				}
#if 0
				nU = nV = xU = xV = 0.0f;
				U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
				V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
				return false;
#endif
			} else {
				// we got two up and two down.
				if (deltaH*HEIGHT_SCALE < TALL_STRETCH_LIMIT) {
					return false;
				}
#if 0
				nU = nV = xU = xV = 0.0f;
				U[0] = nU; U[1] = xU; U[2] = xU; U[3] = nU;
				V[0] = xV; V[1] = xV; V[2] = nV; V[3] = nV;
				return;
#endif
				Real dx = (h3-h2)*HEIGHT_SCALE;
				dx = sqrt(1+dx*dx); // length of the bottom of the cell
				Real dy =	(h3-h0)*HEIGHT_SCALE;
				dy = sqrt(1+dy*dy); // length of the left side.
				if (dx<STRETCH_LIMIT) dx = 1.0f; // don't make a seam unless there is great stretch.
				if (dy<STRETCH_LIMIT) dy = 1.0f; // don't make a seam unless there is great stretch.
				if (dx>TILE_LIMIT) dx = TILE_LIMIT; // don't tile past the texture's edge.
				if (dy>TILE_LIMIT) dy = TILE_LIMIT; // don't tile past the texture's edge.
				dx *= xU-nU;
				dy *= xV-nV;
				U[0] = nU; U[1] = nU+dx; U[2] = nU+dx; U[3] = nU;
				V[0] = nV+dy; V[1] = nV+dy; V[2] = nV; V[3] = nV;
				if (below==1) {
					below = 1;
				}
				// recalc for point 1.
				dx = (h1-h0)*HEIGHT_SCALE;
				dx = sqrt(1+dx*dx); // length of the bottom of the cell
				dy =	(h2-h1)*HEIGHT_SCALE;
				dy = sqrt(1+dy*dy); // length of the left side.
				if (dx<STRETCH_LIMIT) dx = 1.0f; // don't make a seam unless there is great stretch.
				if (dy<STRETCH_LIMIT) dy = 1.0f; // don't make a seam unless there is great stretch.
				if (dx>TILE_LIMIT) dx = TILE_LIMIT; // don't tile past the texture's edge.
				if (dy>TILE_LIMIT) dy = TILE_LIMIT; // don't tile past the texture's edge.
				dx *= xU-nU;
				dy *= xV-nV;
				U[1] = U[0]+dx;
				V[1] = V[3] + dy;
			}
			// Make sure we are within the texture;
			Real adjU = 0;
			Real adjV = 0;
			Int i;
			for (i=0; i<4; i++) {
				if (nVb - V[i] > adjV) adjV = nVb - V[i];
			}
			for (i=0; i<4; i++) {
				V[i] += adjV;
			}
			adjV = 0;
			for (i=0; i<4; i++) {
				if (U[i] - xUb > adjU) adjU = U[i]-xUb;
				if (V[i] - xVb > adjV) adjV = V[i]-xVb;
			}
			for (i=0; i<4; i++) {
				U[i] -= adjU;
				V[i] -= adjV;
			}
		}
		return true;
//
#endif

	}
	return false;
}

///@todo: Are the different "if" cases mutually exclusive?  If so, should add else statements.
Bool WorldHeightMap::getExtraAlphaUVData(Int xIndex, Int yIndex, float U[4], float V[4], UnsignedByte alpha[4], Bool *needFlip, Bool *cliff)
{
	Int ndx = (yIndex*m_width)+xIndex;
	*needFlip = FALSE;
	*cliff = FALSE;

	if ( (ndx>=0) && (ndx<m_dataSize) && m_tileNdxes) {
		Short blendNdx = m_extraBlendTileNdxes[ndx];
		if (blendNdx == 0) {
			return FALSE;
		} else {
			*cliff = getUVForTileIndex(ndx, m_blendedTiles[blendNdx].blendNdx, U, V);
			alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
			if (m_blendedTiles[blendNdx].horiz) {
				// Horizontals don't need flipping unless forced because of 3way blend
				// and a diagonal in base blend layer.
				*needFlip = m_blendedTiles[blendNdx].inverted & FLIPPED_MASK;
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[0] = alpha[3] = 255;
				} else {
					alpha[1] = alpha[2] = 255;
				}
			}
			if (m_blendedTiles[blendNdx].vert) {
				// Verticals don't need flipping unless forced because of 3way blend
				// and a diagonal in base blend layer.
				*needFlip = m_blendedTiles[blendNdx].inverted & FLIPPED_MASK;
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[0] = alpha[1] = 255;
				} else {
					alpha[2] = alpha[3]  = 255;
				}
			}
			if (m_blendedTiles[blendNdx].rightDiagonal) {
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[1] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[0] = 255;
						alpha[2] = 255;
					}
				} else {
					// Uninverted right diagonals need flipping.
					*needFlip = TRUE;
					alpha[2] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[1] = 255;
						alpha[3] = 255;
					}
				}
			}
			if (m_blendedTiles[blendNdx].leftDiagonal) {
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					// Inverted left diagonals need flipping.
					*needFlip = TRUE;
					alpha[0] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[1] = 255;
						alpha[3] = 255;
					}
				} else {
					alpha[3] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[0] = 255;
						alpha[2] = 255;
					}
				}
			}
			if (m_blendedTiles[blendNdx].customBlendEdgeClass>=0) {
				alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
				// No alpha blend, so never need to flip.
				*needFlip = FALSE;
			}
		}
	}

	return TRUE;
}

/** getAlphaUVData - Gets the texture coordinates to use with the alpha texture.
		xIndex and yIndex are the integer coordinates into the height map.
		U and V are the texture coordinates for the 4 corners of a height map cell.
		flip is set if we need to flip the diagonal across the cell to make the
		alpha coordinates blend properly.  Filling a square with 2 triangles is not symmetrical :)
*/
void WorldHeightMap::getAlphaUVData(Int xIndex, Int yIndex, float U[4], float V[4], UnsignedByte alpha[4], Bool *flip)
{
	xIndex += m_drawOriginX;
	yIndex += m_drawOriginY;
	Int ndx = (yIndex*m_width)+xIndex;
	Bool stretchedForCliff = false;
	Bool needFlip = false;

	if ((ndx<m_dataSize) && m_tileNdxes) {
		Short blendNdx = m_blendTileNdxes[ndx];
		if (blendNdx == 0) {
			stretchedForCliff = getUVForTileIndex(ndx, m_tileNdxes[ndx], U, V);
			alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
			// No alpha blend, so never need to flip.
			needFlip = false;
		} else {
			stretchedForCliff = getUVForTileIndex(ndx, m_blendedTiles[blendNdx].blendNdx, U, V);
			alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
			if (m_blendedTiles[blendNdx].horiz) {
				// Horizontals don't need flipping unless forced because of 3way blend.
				needFlip = m_blendedTiles[blendNdx].inverted & FLIPPED_MASK;
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[0] = alpha[3] = 255;
				} else {
					alpha[1] = alpha[2] = 255;
				}
			}
			if (m_blendedTiles[blendNdx].vert) {
				// Verticals don't need flipping unless forced because of 3way blend.
				needFlip = m_blendedTiles[blendNdx].inverted & FLIPPED_MASK;
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[0] = alpha[1] = 255;
				} else {
					alpha[2] = alpha[3]  = 255;
				}
			}
			if (m_blendedTiles[blendNdx].rightDiagonal) {
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					alpha[1] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[0] = 255;
						alpha[2] = 255;
					}
				} else {
					// Uninverted right diagonals need flipping.
					needFlip = true;
					alpha[2] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[1] = 255;
						alpha[3] = 255;
					}
				}
			}
			if (m_blendedTiles[blendNdx].leftDiagonal) {
				if (m_blendedTiles[blendNdx].inverted & INVERTED_MASK) {
					// Inverted left diagonals need flipping.
					needFlip = true;
					alpha[0] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[1] = 255;
						alpha[3] = 255;
					}
				} else {
					alpha[3] = 255;
					if (m_blendedTiles[blendNdx].longDiagonal) {
						alpha[0] = 255;
						alpha[2] = 255;
					}
				}
			}
			if (m_blendedTiles[blendNdx].customBlendEdgeClass>=0) {
				alpha[0] = alpha[1] = alpha[2] = alpha[3] = 0;
				// No alpha blend, so never need to flip.
				needFlip = false;
			}
		}
	}
	if (stretchedForCliff) {
		// If we had to stretch for clif, check heights.
		Int p0=getHeight(xIndex, yIndex);
		Int p1=getHeight(xIndex+1, yIndex);
		Int p2=getHeight(xIndex+1, yIndex+1);
		Int p3=getHeight(xIndex, yIndex+1);
		Int dz1 = abs(p0-p2);
		Int dz2 = abs(p1-p3);
		needFlip = dz1>dz2;
	}
#ifdef FLIP_TRIANGLES
	*flip = needFlip;
#endif
}

void WorldHeightMap::setTextureLOD(Int lod)
{
	if (m_terrainTex)
		m_terrainTex->setLOD(lod);
}

TextureClass *WorldHeightMap::getTerrainTexture()
{
	if (m_terrainTex == nullptr) {
		Int edgeHeight;
		Int height = updateTileTexturePositions(&edgeHeight);
		Int pow2Height = 1;
		while (pow2Height<height) {
			pow2Height *=2;
		}
		REF_PTR_RELEASE(m_terrainTex);
		m_terrainTex = MSGNEW("WorldHeightMap_getTerrainTexture") TerrainTextureClass(pow2Height);
		m_terrainTexHeight = m_terrainTex->update(this);
		char buf[64];
		sprintf(buf, "Base tex height %d", pow2Height);
		DEBUG_LOG((buf));
		REF_PTR_RELEASE(m_alphaTerrainTex);
		m_alphaTerrainTex = MSGNEW("WorldHeightMap_getTerrainTexture") AlphaTerrainTextureClass(m_terrainTex);

		pow2Height = 1;
		while (pow2Height<edgeHeight) {
			pow2Height *=2;
		}
		REF_PTR_RELEASE(m_alphaEdgeTex);
		m_alphaEdgeTex = MSGNEW("WorldHeightMap_getTerrainTexture") AlphaEdgeTextureClass(pow2Height);
		m_alphaEdgeHeight = m_alphaEdgeTex->update(this);

		//Generate lookup table for determining triangle order in each terrain cell.
		//Not the best place to put this but getAlphaUVData() requires a valid terrain
		//texture to return valid values.

		for (Int y=0; y<(m_height-1); y++)
			for (Int x=0; x<(m_width-1); x++)
			{
				UnsignedByte alpha[4];
				float UA[4], VA[4];
				Bool flipForBlend;

				getAlphaUVData(x, y, UA, VA, alpha, &flipForBlend);

				m_cellFlipState[y*m_flipStateWidth+(x>>3)] |= flipForBlend << (x & 0x7);
				DEBUG_ASSERTCRASH ((y*m_flipStateWidth+(x>>3)) < (m_flipStateWidth * m_height), ("Bad range"));
			}
	}

	return m_terrainTex;
}

TextureClass *WorldHeightMap::getAlphaTerrainTexture()
{
	if (m_alphaTerrainTex == nullptr) {
		getTerrainTexture();
	}
	return m_alphaTerrainTex;
}

TextureClass *WorldHeightMap::getEdgeTerrainTexture()
{
	if (m_alphaEdgeTex == nullptr) {
		getTerrainTexture();
	}
	return m_alphaEdgeTex;
}

TerrainTextureClass *WorldHeightMap::getFlatTexture(Int xCell, Int yCell, Int cellWidth, Int pixelsPerCell)
{
	if (WW3D::Get_Texture_Reduction()) {
		if (WW3D::Get_Texture_Reduction()>1) {
			pixelsPerCell /= 4;
		} else {
			pixelsPerCell /= 2;
		}
	}
	Int pow2Height = 1;
	while (pow2Height<cellWidth*pixelsPerCell) {
		pow2Height *=2;
	}
	TerrainTextureClass *newTexture = MSGNEW("WorldHeightMap_getTerrainTexture") TerrainTextureClass(pow2Height, pow2Height);
	newTexture->updateFlat(this, xCell, yCell, cellWidth, pixelsPerCell);
	return newTexture;
}

Region2D WorldHeightMap::getDrawRegion2D()
{
	// Get region in heightmap space
	const Int loX = getDrawOrgX() - getBorderSize();
	const Int loY = getDrawOrgY() - getBorderSize();
	const Int hiX = loX + getDrawWidth();
	const Int hiY = loY + getDrawHeight();

	// Convert to world space
	Region2D region;
	region.lo.x = loX * MAP_XY_FACTOR;
	region.lo.y = loY * MAP_XY_FACTOR;
	region.hi.x = hiX * MAP_XY_FACTOR;
	region.hi.y = hiY * MAP_XY_FACTOR;

	return region;
}

WorldHeightMap::DrawArea WorldHeightMap::createDrawArea(Int xOrg, Int yOrg)
{
	DrawArea area;
	area.sizeX = m_drawWidthX;
	area.sizeY = m_drawHeightY;

	if (TheGlobalData && TheGlobalData->m_stretchTerrain) {
		area.sizeX = STRETCH_DRAW_WIDTH;
		area.sizeY = STRETCH_DRAW_HEIGHT;
	}
	if (TheGlobalData && TheGlobalData->m_drawEntireTerrain) {
		area.sizeX = m_width;
		area.sizeY = m_height;
	}
	area.sizeX = std::min(area.sizeX, m_width);
	area.sizeY = std::min(area.sizeY, m_height);
	area.originX = clamp(0, xOrg, m_width - area.sizeX);
	area.originY = clamp(0, yOrg, m_height - area.sizeY);

	return area;
}

Bool WorldHeightMap::setDrawArea(const DrawArea& area)
{
	Bool anythingDifferent =
		m_drawOriginX != area.originX ||
		m_drawOriginY != area.originY ||
		m_drawWidthX != area.sizeX ||
		m_drawHeightY != area.sizeY;

	if (anythingDifferent) {
		m_drawOriginX = area.originX;
		m_drawOriginY = area.originY;
		m_drawWidthX = area.sizeX;
		m_drawHeightY = area.sizeY;
		return true;
	}
	return false;
}

Bool WorldHeightMap::setDrawOrg(Int xOrg, Int yOrg)
{
	return setDrawArea(createDrawArea(xOrg, yOrg));
}


/** Gets global texture class. */
Int WorldHeightMap::getTextureClass(Int xIndex, Int yIndex, Bool baseClass)
{
	Int ndx = (yIndex*m_width)+xIndex;
	DEBUG_ASSERTCRASH((ndx>=0 && ndx<this->m_dataSize),("oops"));
	if (ndx<0 || ndx >= this->m_dataSize) return(-1);
	Int textureNdx = m_tileNdxes[ndx];
	if (!baseClass && (m_blendTileNdxes[ndx] != 0 || m_extraBlendTileNdxes[ndx] != 0)) {
		return(-1);  // blended, so not of the original class.
	}
	return getTextureClassFromNdx(textureNdx);
}


/** Sets all the cliff flags in map based on height. */
void WorldHeightMap::initCliffFlagsFromHeights()
{
	Int xIndex, yIndex;

	for (xIndex=0; xIndex<m_width-1; xIndex++) {
		for (yIndex=0; yIndex<m_height-1; yIndex++) {
			setCellCliffFlagFromHeights(xIndex, yIndex);
		}
	}
}

/** Sets the cliff flag for a cell based on height. */
void WorldHeightMap::setCellCliffFlagFromHeights(Int xIndex, Int yIndex)
{
	Real height1 = getHeight(xIndex, yIndex)*MAP_HEIGHT_SCALE;
	Real height2 = getHeight(xIndex+1, yIndex)*MAP_HEIGHT_SCALE;
	Real height3 = getHeight(xIndex, yIndex+1)*MAP_HEIGHT_SCALE;
	Real height4 = getHeight(xIndex+1, yIndex+1)*MAP_HEIGHT_SCALE;
	Real minZ = height1;
	if (minZ > height2) minZ = height2;
	if (minZ > height3) minZ = height3;
	if (minZ > height4) minZ = height4;
	Real maxZ = height1;
	if (maxZ < height2) maxZ = height2;
	if (maxZ < height3) maxZ = height3;
	if (maxZ < height4) maxZ = height4;
	const Real cliffRange = PATHFIND_CLIFF_SLOPE_LIMIT_F;
	Bool isCliff = (maxZ-minZ > cliffRange);
	setCliffState(xIndex, yIndex, isCliff);

}

/** Gets global texture class. */
Int WorldHeightMap::getTextureClassFromNdx(Int tileNdx)
{
	Int i;
	tileNdx = tileNdx>>2;
	for (i=0; i<m_numTextureClasses; i++) {
		if (m_textureClasses[i].firstTile<0) {
			continue;
		}
		// see if the blend tile is in a texture class, and get the right tile for xIndex, yIndex.
		if (tileNdx >= m_textureClasses[i].firstTile &&
			tileNdx < m_textureClasses[i].firstTile+m_textureClasses[i].numTiles) {
			return(m_textureClasses[i].globalTextureClass);
		}
	}
	return(-1);
}

TXTextureClass WorldHeightMap::getTextureFromIndex( Int textureIndex )
{
	return m_textureClasses[textureIndex];
}

void WorldHeightMap::getTerrainColorAt(Real x, Real y, RGBColor *pColor)
{
	Int xIndex = REAL_TO_INT_FLOOR(x/MAP_XY_FACTOR);
	Int yIndex = REAL_TO_INT_FLOOR(y/MAP_XY_FACTOR);
	xIndex += m_borderSize;
	yIndex += m_borderSize;
	pColor->red = pColor->green = pColor->blue = 0;
	if (xIndex<0) xIndex = 0;
	if (yIndex<0) yIndex = 0;
	if (xIndex >= m_width) xIndex = m_width-1;
	if (yIndex >= m_height) yIndex = m_height-1;
	Int ndx = (yIndex*m_width)+xIndex;
	if (ndx<0 || ndx >= this->m_dataSize) return;
	Int tileNdx = m_tileNdxes[ndx];
	tileNdx = tileNdx>>2;	 // We pack 4 grids into a tile.

	TileData *pTile = getSourceTile(tileNdx);
	if (pTile) {
		// pTile contains the bitmap data for 4 squares.
		// Get the data mipped down to one pixel for the tile.
		UnsignedByte *pData = pTile->getRGBDataForWidth(1);
		// Data is in microsoft bgra format.
		pColor->red = pData[2]/255.0;
		pColor->green = pData[1]/255.0;
		pColor->blue = pData[0]/255.0;
	}
}

AsciiString WorldHeightMap::getTerrainNameAt(Real x, Real y)
{
	Int xIndex = REAL_TO_INT_FLOOR(x/MAP_XY_FACTOR);
	Int yIndex = REAL_TO_INT_FLOOR(y/MAP_XY_FACTOR);
	xIndex += m_borderSize;
	yIndex += m_borderSize;
	if (xIndex<0) xIndex = 0;
	if (yIndex<0) yIndex = 0;
	if (xIndex >= m_width) xIndex = m_width-1;
	if (yIndex >= m_height) yIndex = m_height-1;
	Int ndx = (yIndex*m_width)+xIndex;
	if (ndx<0 || ndx >= this->m_dataSize) return AsciiString::TheEmptyString;
	Int tileNdx = m_tileNdxes[ndx];
	tileNdx = tileNdx>>2;	 // We pack 4 grids into a tile.

	Int i;
	for (i=0; i<this->m_numTextureClasses; i++) {
		if (tileNdx >= m_textureClasses[i].firstTile && tileNdx < m_textureClasses[i].firstTile + m_textureClasses[i].numTiles) {
			return(m_textureClasses[i].name);
		}
	}
	return AsciiString::TheEmptyString;
}


static UnsignedByte s_buffer[DATA_LEN_BYTES];
static UnsignedByte s_blendBuffer[DATA_LEN_BYTES];

UnsignedByte * WorldHeightMap::getPointerToTileData(Int xIndex, Int yIndex, Int width)
{
	Int ndx = (yIndex*m_width)+xIndex;
	if (yIndex<0 || xIndex<0 || xIndex>=m_width || yIndex>=m_height) {
		return nullptr;
	}
	if (ndx<0 || ndx>=m_dataSize) {
		return nullptr;
	}
	TBlendTileInfo *pBlend = nullptr;
	Short tileNdx = m_tileNdxes[ndx];
	if (getRawTileData(tileNdx, width, s_buffer, DATA_LEN_BYTES)) {
		Short blendTileNdx = m_blendTileNdxes[ndx];
		if (blendTileNdx>0 && blendTileNdx < NUM_BLEND_TILES) {
			pBlend = &m_blendedTiles[blendTileNdx];
			if (getRawTileData(pBlend->blendNdx, width, s_blendBuffer, DATA_LEN_BYTES)) {
				UnsignedByte *pAlpha = getRGBAlphaDataForWidth(width, pBlend);
				pAlpha += 3;  // skip over the rgb to the a.
				Int i, limit;
				limit = width*width;
				UnsignedByte *pBlendData = s_blendBuffer;
				UnsignedByte *pDestData = s_buffer;
				for (i=0; i<limit; i++) {
					Int r,g,b,a;
					b = *pBlendData++;
					g = *pBlendData++;
					r = *pBlendData++; pBlendData++;
					a = *pAlpha; pAlpha += 4;
					*pDestData++ = ((b*a)/255) + (((*pDestData)*(255-a))/255);
					*pDestData++ = ((g*a)/255) + (((*pDestData)*(255-a))/255);
					*pDestData++ = ((r*a)/255) + (((*pDestData)*(255-a))/255);
					*pDestData++ = 255; // just skip alpha.  not really used. jba.
				}
			}
		}
		return(s_buffer);
	}

	return(nullptr);
}

#define K_HORIZ 0
#define K_VERT 1
#define K_LDIAG 2
#define K_RDIAG 3
#define K_LLDIAG 4
#define K_LRDIAG 5
#define K_DIR_MOD 0x05
#define K_INV 6

UnsignedByte *WorldHeightMap::getRGBAlphaDataForWidth(Int width, TBlendTileInfo *pBlend)
{
	Int alphaTileNdx = 0;
	if (pBlend->horiz) {
		alphaTileNdx = K_HORIZ;
	} else if (pBlend->vert) {
		alphaTileNdx = K_VERT;
	} else if (pBlend->rightDiagonal) {
		alphaTileNdx = K_RDIAG;
		if (pBlend->longDiagonal) alphaTileNdx=K_LRDIAG;
	} else if (pBlend->leftDiagonal) {
		alphaTileNdx = K_LDIAG;
		if (pBlend->longDiagonal) alphaTileNdx=K_LLDIAG;
	}
	if (pBlend->inverted) {
		alphaTileNdx += K_INV;
	}
	return m_alphaTiles[alphaTileNdx]->getRGBDataForWidth(width);
}

void WorldHeightMap::setupAlphaTiles()
{
	TBlendTileInfo blendInfo;
	if (m_alphaTiles[0] != nullptr) return;
	Int k;
	for (k=0; k<NUM_ALPHA_TILES; k++) {
		memset(&blendInfo, 0, sizeof(blendInfo));
		Int baseK = k;
		if (k>=K_INV) {
			blendInfo.inverted = true;
			baseK -= K_INV;
		}
		switch(baseK) {
			case K_HORIZ : blendInfo.horiz = true; break;
			case K_VERT : blendInfo.vert = true; break;
			case K_LDIAG : blendInfo.leftDiagonal = true; break;
			case K_RDIAG : blendInfo.rightDiagonal = true; break;
			case K_LLDIAG : blendInfo.leftDiagonal = true; blendInfo.longDiagonal = true; break;
			case K_LRDIAG : blendInfo.rightDiagonal = true; blendInfo.longDiagonal = true; break;
		}
		m_alphaTiles[k] = new TileData;
		TileData *pTile = m_alphaTiles[k];

		Int i, j;
		UnsignedByte *pDest = pTile->getDataPtr();
		for (j=0; j<TILE_PIXEL_EXTENT; j++) {
			for (i=0; i<TILE_PIXEL_EXTENT; i++) {
				Int h = i;
				Int v = j;
				Int alpha = 255;  // 0 - 255.
				if (blendInfo.horiz) {
					if (!blendInfo.inverted) h = TILE_PIXEL_EXTENT-h-1;
					alpha = (alpha*h)/(TILE_PIXEL_EXTENT-1);
				} else if (blendInfo.vert) {
					if (!blendInfo.inverted) v = TILE_PIXEL_EXTENT-v-1;
					alpha = (alpha*v)/(TILE_PIXEL_EXTENT-1);
				} else if (blendInfo.rightDiagonal) {
					h = TILE_PIXEL_EXTENT-h-1;
					if (!blendInfo.inverted) v = TILE_PIXEL_EXTENT-v-1;
					v += h;				// angled
					if (blendInfo.longDiagonal) {
						v -= TILE_PIXEL_EXTENT;
					}
					alpha = (alpha*v)/(TILE_PIXEL_EXTENT-1);
				} else if (blendInfo.leftDiagonal) {
					if (!blendInfo.inverted) v = TILE_PIXEL_EXTENT-v-1;
					v += h;				// angled
					if (blendInfo.longDiagonal) {
						v -= TILE_PIXEL_EXTENT;
					}
					alpha = (alpha*v)/(TILE_PIXEL_EXTENT-1);
				}

				if (alpha > 255) alpha = 255;
				if (alpha<0) alpha = 0;
				alpha = 255-alpha;

				pDest += 3; // skip blue, green & red bytes.
				*pDest = alpha;		// alpha.
				//*pDest = 255;
				pDest++;
			}
		}
		pTile->updateMips();
	}
}


Bool  WorldHeightMap::getRawTileData(Short tileNdx, Int width,
																				 UnsignedByte *buffer, Int bufLen)
{
	TileData *pSrc = nullptr;
	if (tileNdx/4 < NUM_SOURCE_TILES) {
		pSrc = m_sourceTiles[tileNdx/4];
	}
	if (bufLen < (width*width*TILE_BYTES_PER_PIXEL)) {
		return(false);
	}
	if (pSrc && pSrc->hasRGBDataForWidth(2*width)) {
		Int j;
		UnsignedByte *pSrcData = pSrc->getRGBDataForWidth(2*width);
		Int xOffset=0;
		Int yOffset=0;
		if (tileNdx & 1) xOffset = width;
		if (tileNdx & 2) yOffset = width;
		for (j=0; j<width; j++) {
			UnsignedByte *pDestData = buffer;
			pDestData += j*(width)*TILE_BYTES_PER_PIXEL;
			UnsignedByte *pSrc = pSrcData;
			pSrc += (j+yOffset)*width*TILE_BYTES_PER_PIXEL*2;
			pSrc += xOffset*TILE_BYTES_PER_PIXEL;
			memcpy(pDestData, pSrc, width*TILE_BYTES_PER_PIXEL);
		}
		return(true);
	}
	return(false);
}

