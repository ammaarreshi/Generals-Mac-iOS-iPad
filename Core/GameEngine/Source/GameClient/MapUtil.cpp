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

// FILE: MapUtil.cpp //////////////////////////////////////////////////////////////////////////////
// Author: Matt Campbell, December 2001
// Description: Map utility/convenience routines
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include <cstdint>

#include "Common/crc.h"
#include "Common/FileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/file.h"
#include "Common/GlobalData.h"
#include "Common/GameState.h"
#include "Common/GameEngine.h"
#include "Common/NameKeyGenerator.h"
#include "Common/DataChunk.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/MessageStream.h"
#include "Common/WellKnownKeys.h"
#include "Common/INI.h"
#include "Common/QuotedPrintable.h"
#include "Common/SkirmishBattleHonors.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/MapObject.h"
#include "GameClient/GameText.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Gadget.h"
#include "GameClient/Image.h"
#include "GameClient/Shell.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/FPUControl.h"
#include "GameNetwork/GameInfo.h"
#include "GameNetwork/NetworkDefs.h"


//-------------------------------------------------------------------------------
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
static const char *mapExtension = ".map";

// GeneralsX @bugfix Copilot 27/04/2026 Accept both '\' and '/' separators so user custom maps work on macOS/Linux paths.
static const char *findLastPathSeparator(const AsciiString& path)
{
	const char *backslash = path.reverseFind('\\');
	const char *slash = path.reverseFind('/');

	if (!backslash)
	{
		return slash;
	}

	if (!slash)
	{
		return backslash;
	}

	return slash > backslash ? slash : backslash;
}

static Bool hasValidMapLeafLayout(const AsciiString& filepathLower, const AsciiString& filenameLower)
{
	AsciiString endingStrBackslash;
	endingStrBackslash.format("%s\\%s%s", filenameLower.str(), filenameLower.str(), mapExtension);

	AsciiString endingStrSlash;
	endingStrSlash.format("%s/%s%s", filenameLower.str(), filenameLower.str(), mapExtension);

	return filepathLower.endsWithNoCase(endingStrBackslash.str()) || filepathLower.endsWithNoCase(endingStrSlash.str());
}

static Int m_width = 0;						///< Height map width.
static Int m_height = 0;					///< Height map height (y size of array).
static Int m_borderSize = 0;			///< Non-playable border area.
static std::vector<ICoord2D> m_boundaries;	///< All the boundaries we use for the map
static Int m_dataSize = 0;				///< size of m_data.
static UnsignedByte *m_data = nullptr;	///< array of z(height) values in the height map.
static Dict worldDict = 0;

static WaypointMap *m_waypoints = nullptr;
static Coord3DList	m_supplyPositions;
static Coord3DList	m_techPositions;

static Int m_mapDX = 0;
static Int m_mapDY = 0;

static UnsignedInt calcCRC( AsciiString fname )
{
	CRC theCRC;
	theCRC.clear();

	File *fp = TheFileSystem->openFile(fname.str(), File::READ);
	if( !fp )
	{
		DEBUG_CRASH(("Couldn't open '%s'", fname.str()));
		return 0;
	}

	UnsignedByte buf[4096];
	Int num;
	while ( (num=fp->read(buf, 4096)) > 0 )
	{
		theCRC.computeCRC(buf, num);
	}

	fp->close();
	fp = nullptr;

	return theCRC.get();
}

static Bool ParseObjectDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	Bool readDict = info->version >= K_OBJECTS_VERSION_2;

	Coord3D loc;
	loc.x = file.readReal();
	loc.y = file.readReal();
	loc.z = file.readReal();
	if (info->version <= K_OBJECTS_VERSION_2)
	{
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
	MapObject *pThisOne;

	// create the map object
	pThisOne = newInstance( MapObject )( loc, name, angle, flags, &d,
														TheThingFactory->findTemplate( name, FALSE ) );

//DEBUG_LOG(("obj %s owner %s",name.str(),d.getAsciiString(TheKey_originalOwner).str()));

	if (pThisOne->getProperties()->getType(TheKey_waypointID) == Dict::DICT_INT)
	{
		pThisOne->setIsWaypoint();

		// grab useful info
		(*m_waypoints)[pThisOne->getWaypointName()] = loc;
	}
	else if (pThisOne->getThingTemplate() && pThisOne->getThingTemplate()->isKindOf(KINDOF_TECH_BUILDING))
	{
		m_techPositions.push_back(loc);
	}
	else if (pThisOne->getThingTemplate() && pThisOne->getThingTemplate()->isKindOf(KINDOF_SUPPLY_SOURCE_ON_PREVIEW))
	{
		m_supplyPositions.push_back(loc);
	}

	deleteInstance(pThisOne);
	return TRUE;
}

static Bool ParseObjectsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	file.m_currentObject = nullptr;
	file.registerParser( "Object", info->label, ParseObjectDataChunk );
	return (file.parse(userData));
}

static Bool ParseWorldDictDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	worldDict = file.readDict();
	return true;
}

static Bool ParseSizeOnly(DataChunkInput &file, DataChunkInfo *info, void *userData)
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
	}
	return true;

	m_dataSize = file.readInt();
	m_data = NEW UnsignedByte[m_dataSize];	// pool[]ify
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

static Bool ParseSizeOnlyInChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	return ParseSizeOnly(file, info, userData);
}

static Bool loadMap( AsciiString filename )
{
	CachedFileInputStream fileStrm;

	if( !fileStrm.open(filename) )
	{
		return FALSE;
	}

	ChunkInputStream *pStrm = &fileStrm;

	DataChunkInput file( pStrm );

	m_waypoints = NEW WaypointMap;

	file.registerParser( "HeightMapData", AsciiString::TheEmptyString, ParseSizeOnlyInChunk );
	file.registerParser( "WorldInfo", AsciiString::TheEmptyString, ParseWorldDictDataChunk );
	file.registerParser( "ObjectsList", AsciiString::TheEmptyString, ParseObjectsDataChunk );
	if (!file.parse(nullptr)) {
		throw(ERROR_CORRUPT_FILE_FORMAT);
	}

	m_mapDX = m_width  - 2*m_borderSize;
	m_mapDY = m_height - 2*m_borderSize;

	return TRUE;
}

static void resetMap()
{
	delete[] m_data;
	m_data = nullptr;

	delete m_waypoints;
	m_waypoints = nullptr;

	m_techPositions.clear();
	m_supplyPositions.clear();
}

static void getExtent( Region3D *extent )
{
	extent->lo.x = 0.0f;

	extent->lo.y = 0.0f;

	// Note - m_mapDX & Y are the number of height map grids wide, so we have to
	// multiply by the grid width.
	extent->hi.x = m_mapDX*MAP_XY_FACTOR;
	extent->hi.y = m_mapDY*MAP_XY_FACTOR;

	extent->lo.z = 0;
	extent->hi.z = 0;
}

//-------------------------------------------------------------------------------

void WaypointMap::update()
{
	if (!m_waypoints)
	{
		m_numStartSpots = 1;
		return;
	}

	this->clear();

	AsciiString startingCamName = TheNameKeyGenerator->keyToName(TheKey_InitialCameraPosition);
	WaypointMap::const_iterator it;

	it = m_waypoints->find(startingCamName);
	if (it != m_waypoints->end())
	{
		(*this)[startingCamName] = it->second;
	}

	m_numStartSpots = 0;
	for (Int i=0; i<MAX_SLOTS; ++i)
	{
		startingCamName.format("Player_%d_Start", i+1); // start pos waypoints are 1-based
		it = m_waypoints->find(startingCamName);
		if (it != m_waypoints->end())
		{
			(*this)[startingCamName] = it->second;
			++m_numStartSpots;
		}
		else
		{
			break;
		}
	}

	m_numStartSpots = max(1, m_numStartSpots);
}

const char *const MapCache::m_mapCacheName = "MapCache.ini";

AsciiString MapCache::getMapDir() const
{
	return "Maps";
}

AsciiString MapCache::getUserMapDir() const
{
	AsciiString tmp = TheGlobalData->getPath_UserData();
	tmp.concat(getMapDir());
	return tmp;
}

AsciiString MapCache::getMapExtension() const
{
	return "map";
}

void MapCache::writeCacheINI( const AsciiString &mapDir )
{
	AsciiString filepath = mapDir;
	// GeneralsX @bugfix fbraz 06/05/2026 Use portable separator for host filesystem paths to avoid creating literal "\\" filenames on POSIX.
	filepath.concat('/');

	TheFileSystem->createDirectory(mapDir);

	filepath.concat(m_mapCacheName);
	FILE *fp = fopen(filepath.str(), "w");
	DEBUG_ASSERTCRASH(fp != nullptr, ("Failed to create %s", filepath.str()));
	if (fp == nullptr) {
		return;
	}

	fprintf(fp, "; FILE: %s /////////////////////////////////////////////////////////////\n", filepath.str());
	fprintf(fp, "; This INI file is auto-generated - do not modify\n");
	fprintf(fp, "; /////////////////////////////////////////////////////////////////////////////\n");

	MapCache::iterator it = begin();
	for (; it != end(); ++it)
	{
		if (it->first.startsWithNoCase(mapDir.str()))
		{
			const MapMetaData &md = it->second;
			fprintf(fp, "\nMapCache %s\n", AsciiStringToQuotedPrintable(it->first.str()).str());
			fprintf(fp, "  fileSize = %u\n", md.m_filesize);
			fprintf(fp, "  fileCRC = %u\n", md.m_CRC);
			fprintf(fp, "  timestampLo = %d\n", md.m_timestamp.m_lowTimeStamp);
			fprintf(fp, "  timestampHi = %d\n", md.m_timestamp.m_highTimeStamp);
			fprintf(fp, "  isOfficial = %s\n", (md.m_isOfficial)?"yes":"no");

			fprintf(fp, "  isMultiplayer = %s\n", (md.m_isMultiplayer)?"yes":"no");
			fprintf(fp, "  numPlayers = %d\n", md.m_numPlayers);

			fprintf(fp, "  extentMin = X:%2.2f Y:%2.2f Z:%2.2f\n", md.m_extent.lo.x, md.m_extent.lo.y, md.m_extent.lo.z);
			fprintf(fp, "  extentMax = X:%2.2f Y:%2.2f Z:%2.2f\n", md.m_extent.hi.x, md.m_extent.hi.y, md.m_extent.hi.z);

// BAD AND NOW UNUSED:  the mapcache.ini should not contain localized data... using the lookup tag instead
#if RTS_GENERALS
			fprintf(fp, "  displayName = %s\n", UnicodeStringToQuotedPrintable(md.m_displayName).str());
#else
			fprintf(fp, "  nameLookupTag = %s\n", md.m_nameLookupTag.str());
#endif

			Coord3D pos;
			WaypointMap::const_iterator itw = md.m_waypoints.begin();
			for (; itw != md.m_waypoints.end(); ++itw)
			{
				pos = itw->second;
				fprintf(fp, "  %s = X:%2.2f Y:%2.2f Z:%2.2f\n", itw->first.str(), pos.x, pos.y, pos.z);
			}
			Coord3DList::const_iterator itc3d = md.m_techPositions.begin();
			for (; itc3d != md.m_techPositions.end(); ++itc3d)
			{
				pos = *itc3d;
				fprintf(fp, "  techPosition = X:%2.2f Y:%2.2f Z:%2.2f\n", pos.x, pos.y, pos.z);
			}

			itc3d = md.m_supplyPositions.begin();
			for (; itc3d != md.m_supplyPositions.end(); ++itc3d)
			{
				pos = *itc3d;
				fprintf(fp, "  supplyPosition = X:%2.2f Y:%2.2f Z:%2.2f\n", pos.x, pos.y, pos.z);
			}
			fprintf(fp, "END\n\n");
		}
		else
		{
			//DEBUG_LOG(("%s does not start %s", mapDir.str(), it->first.str()));
		}
	}

	fclose(fp);
}

void MapCache::updateCache()
{
	setFPMode();

	const AsciiString mapDir = getMapDir();
	const AsciiString userMapDir = getUserMapDir();

	// Create the standard map cache if required. Is only relevant for Mod developers.
	// TheSuperHackers @tweak This step is done before loading any other map caches to not poison the cached state.
	if (m_doCreateStandardMapCacheINI)
	{
#if defined(RTS_DEBUG)
		// only create the map cache file if "Maps" folder exists
		const Bool buildMapCache = TheLocalFileSystem->doesFileExist(mapDir.str());
#else
		const Bool buildMapCache = TheGlobalData->m_buildMapCache;
#endif
		if (buildMapCache)
		{
			const Bool isOfficial = TRUE;
			const Bool filterByAllowedMaps = !m_allowedMaps.empty();

			if (loadMapsFromDisk(mapDir, isOfficial, filterByAllowedMaps))
			{
				writeCacheINI(mapDir);
			}
		}
		m_doCreateStandardMapCacheINI = FALSE;
	}

	// Load user map cache first.
	if (m_doLoadUserMapCacheINI)
	{
		loadMapsFromMapCacheINI(userMapDir);
		m_doLoadUserMapCacheINI = FALSE;
	}

	// Load user maps from disk and update any discrepancies from the map cache.
	if (loadMapsFromDisk(userMapDir, FALSE))
	{
		writeCacheINI(userMapDir);
		m_doLoadStandardMapCacheINI = TRUE;
	}

	// Load standard maps from map cache last.
	// This overwrites matching user maps to prevent munkees getting rowdy :)
	if (m_doLoadStandardMapCacheINI)
	{
		loadMapsFromMapCacheINI(mapDir);
		m_doLoadStandardMapCacheINI = FALSE;
	}
}

void MapCache::prepareUnseenMaps( const AsciiString &mapDir )
{
	MapCache::iterator it = begin();
	for (; it != end(); ++it)
	{
		const AsciiString &mapName = it->first;

		if (mapName.startsWithNoCase(mapDir.str()))
		{
			it->second.m_doesExist = FALSE;
		}
	}
}

Bool MapCache::clearUnseenMaps( const AsciiString &mapDir )
{
	Bool erasedSomething = FALSE;

	MapCache::iterator it = begin();
	while (it != end())
	{
		MapCache::iterator next = it;
		++next;

		const AsciiString &mapName = it->first;
		const MapMetaData &mapData = it->second;

		if (mapName.startsWithNoCase(mapDir.str()) && !mapData.m_doesExist)
		{
			erase(it);
			erasedSomething = TRUE;
		}

		it = next;
	}

	return erasedSomething;
}

void MapCache::loadMapsFromMapCacheINI( const AsciiString &mapDir )
{
	INI ini;
	AsciiString fname;
	// GeneralsX @bugfix fbraz 06/05/2026 Keep MapCache INI path consistent with writeCacheINI() on POSIX hosts.
	fname.format("%s/%s", mapDir.str(), m_mapCacheName);

	if (TheFileSystem->doesFileExist(fname.str()))
	{
		ini.load( fname, INI_LOAD_OVERWRITE, nullptr );
	}
}

Bool MapCache::loadMapsFromDisk( const AsciiString &mapDir, Bool isOfficial, Bool filterByAllowedMaps )
{
	prepareUnseenMaps(mapDir);

	FilenameList filepathList;
	FilenameListIter filepathIt;
	AsciiString toplevelPattern;
	toplevelPattern.format("%s\\", mapDir.str());
	Bool mapListChanged = FALSE;
	AsciiString filenamepattern;
	filenamepattern.format("*.%s", getMapExtension().str());

	TheFileSystem->getFileListInDirectory(toplevelPattern, filenamepattern, filepathList, TRUE);

	filepathIt = filepathList.begin();

	for (; filepathIt != filepathList.end(); ++filepathIt)
	{
		FileInfo fileInfo;
		AsciiString filepathLower = *filepathIt;
		filepathLower.toLower();

		const char *szFilenameLower = findLastPathSeparator(filepathLower);
		if (!szFilenameLower)
		{
			DEBUG_CRASH(("Couldn't find path separator in map name!"));
			continue;
		}

		AsciiString filenameLower = szFilenameLower+1;
		filenameLower.truncateBy(strlen(mapExtension));

		if (filterByAllowedMaps && m_allowedMaps.find(filenameLower) == m_allowedMaps.end())
		{
			DEBUG_CRASH(("Map '%s' has been filtered out", filenameLower.str()));
			continue;
		}

		if (!hasValidMapLeafLayout(filepathLower, filenameLower))
		{
			DEBUG_CRASH(("Found map '%s' in wrong spot (%s)", filenameLower.str(), filepathLower.str()));
			continue;
		}

		if (!TheFileSystem->getFileInfo(*filepathIt, &fileInfo))
		{
			DEBUG_CRASH(("Could not get file info for map %s", filepathIt->str()));
			continue;
		}

		mapListChanged |= addMap(mapDir, *filepathIt, filepathLower, fileInfo, isOfficial);
	}

	if (clearUnseenMaps(mapDir))
	{
		mapListChanged = TRUE;
	}

	return mapListChanged;
}

Bool MapCache::addMap(
	const AsciiString &/*mapDir*/,
	const AsciiString &fname,
	const AsciiString &lowerFname,
	FileInfo &fileInfo,
	Bool isOfficial)
{
	MapCache::iterator it = find(lowerFname);
	if (it != end())
	{
		// Found the map in our cache. Check to see if it has changed.
		const MapMetaData& md = it->second;

		if (md.m_filesize == fileInfo.sizeLow && md.m_CRC != 0)
		{
			// Force a lookup so that we don't display the English localization in all builds.
			if (md.m_nameLookupTag.isEmpty())
			{
				// unofficial maps or maps without names
				AsciiString tempdisplayname;
				const char *displayNameStart = findLastPathSeparator(fname);
				tempdisplayname = displayNameStart ? displayNameStart + 1 : fname;
				(*this)[lowerFname].m_displayName.translate(tempdisplayname);
				if (md.m_numPlayers >= 2)
				{
					UnicodeString extension;
					extension.format(L" (%d)", md.m_numPlayers);
					(*this)[lowerFname].m_displayName.concat(extension);
				}
			}
			else
			{
				// official maps with name tags
				(*this)[lowerFname].m_displayName = TheGameText->fetch(md.m_nameLookupTag);
				if (md.m_numPlayers >= 2)
				{
					UnicodeString extension;
					extension.format(L" (%d)", md.m_numPlayers);
					(*this)[lowerFname].m_displayName.concat(extension);
				}
			}

			it->second.m_doesExist = TRUE;

//			DEBUG_LOG(("MapCache::addMap - found match for map %s", lowerFname.str()));
			return FALSE;	// OK, it checks out.
		}
		DEBUG_LOG(("%s didn't match file in MapCache", fname.str()));
		DEBUG_LOG(("size: %d / %d", fileInfo.sizeLow, md.m_filesize));
		DEBUG_LOG(("time1: %d / %d", fileInfo.timestampHigh, md.m_timestamp.m_highTimeStamp));
		DEBUG_LOG(("time2: %d / %d", fileInfo.timestampLow, md.m_timestamp.m_lowTimeStamp));
//		DEBUG_LOG(("size: %d / %d", filesize, md.m_filesize));
//		DEBUG_LOG(("time1: %d / %d", timestamp.m_highTimeStamp, md.m_timestamp.m_highTimeStamp));
//		DEBUG_LOG(("time2: %d / %d", timestamp.m_lowTimeStamp, md.m_timestamp.m_lowTimeStamp));
	}

	DEBUG_LOG(("MapCache::addMap(): caching '%s' because '%s' was not found", fname.str(), lowerFname.str()));

	loadMap(fname); // Just load for querying the data, since we aren't playing this map.

	// The map is now loaded.  Pick out what we need.
	MapMetaData md;
	md.m_fileName = lowerFname;
	md.m_filesize = fileInfo.sizeLow;
	md.m_isOfficial = isOfficial;
	md.m_doesExist = TRUE;
	md.m_waypoints.update();
	md.m_numPlayers = md.m_waypoints.m_numStartSpots;
	md.m_isMultiplayer = (md.m_numPlayers >= 2);
	md.m_timestamp.m_highTimeStamp = fileInfo.timestampHigh;
	md.m_timestamp.m_lowTimeStamp = fileInfo.timestampLow;
	md.m_supplyPositions = m_supplyPositions;
	md.m_techPositions = m_techPositions;
	md.m_CRC = calcCRC(fname);

	Bool exists = false;
	AsciiString nameLookupTag = worldDict.getAsciiString(TheKey_mapName, &exists);
	md.m_nameLookupTag = nameLookupTag;

	if (!exists || nameLookupTag.isEmpty())
	{
		DEBUG_LOG(("Missing TheKey_mapName!"));
		AsciiString tempdisplayname;
		const char *displayNameStart = findLastPathSeparator(fname);
		tempdisplayname = displayNameStart ? displayNameStart + 1 : fname;
		md.m_displayName.translate(tempdisplayname);
		if (md.m_numPlayers >= 2)
		{
			UnicodeString extension;
			extension.format(L" (%d)", md.m_numPlayers);
			md.m_displayName.concat(extension);
		}
		TheGameText->reset();
	}
	else
	{
		AsciiString stringFileName = fname;
		stringFileName.truncateBy(4);
		const char *stringFileSeparator = findLastPathSeparator(stringFileName);
		if (stringFileSeparator && *stringFileSeparator == '/')
		{
			stringFileName.concat("/map.str");
		}
		else
		{
			stringFileName.concat("\\map.str");
		}
		TheGameText->initMapStringFile(stringFileName);
		md.m_displayName = TheGameText->fetch(nameLookupTag);
		if (md.m_numPlayers >= 2)
		{
			UnicodeString extension;
			extension.format(L" (%d)", md.m_numPlayers);
			md.m_displayName.concat(extension);
		}
		DEBUG_LOG(("Map name is now '%ls'", md.m_displayName.str()));
		TheGameText->reset();
	}

	getExtent(&(md.m_extent));

	(*this)[lowerFname] = md;

	DEBUG_LOG(("  filesize = %d bytes", md.m_filesize));
	DEBUG_LOG(("  displayName = %ls", md.m_displayName.str()));
	DEBUG_LOG(("  CRC = %X", md.m_CRC));
	DEBUG_LOG(("  timestamp = %d", md.m_timestamp));
	DEBUG_LOG(("  isOfficial = %s", (md.m_isOfficial)?"yes":"no"));

	DEBUG_LOG(("  isMultiplayer = %s", (md.m_isMultiplayer)?"yes":"no"));
	DEBUG_LOG(("  numPlayers = %d", md.m_numPlayers));

	DEBUG_LOG(("  extent = (%2.2f,%2.2f) -> (%2.2f,%2.2f)",
		md.m_extent.lo.x, md.m_extent.lo.y,
		md.m_extent.hi.x, md.m_extent.hi.y));

	Coord3D pos;
	WaypointMap::iterator itw = md.m_waypoints.begin();
	for (; itw != md.m_waypoints.end(); ++itw)
	{
		pos = itw->second;
		DEBUG_LOG(("    waypoint %s: (%2.2f,%2.2f)", itw->first.str(), pos.x, pos.y));
	}

	resetMap();

	return TRUE;
}

MapCache *TheMapCache = nullptr;

// PUBLIC FUNCTIONS //////////////////////////////////////////////////////////////////////////////

Bool WouldMapTransfer( const AsciiString& mapName )
{
	return mapName.startsWithNoCase(TheMapCache->getUserMapDir());
}

//-------------------------------------------------------------------------------------------------
typedef std::set<UnicodeString, rts::less_than_nocase<UnicodeString>/**/> MapNameList;
typedef std::map<UnicodeString, AsciiString> MapDisplayToFileNameList;

static void buildMapListForNumPlayers(MapNameList &outMapNames, MapDisplayToFileNameList &outFileNames, Int numPlayers)
{
	MapCache::iterator it = TheMapCache->begin();

	for (; it != TheMapCache->end(); ++it)
	{
		const MapMetaData &mapData = it->second;
		if (mapData.m_numPlayers == numPlayers)
		{
			outMapNames.insert(it->second.m_displayName);
			outFileNames[it->second.m_displayName] = it->first;
		}
	}
}

//-------------------------------------------------------------------------------------------------
struct MapListBoxData
{
	MapListBoxData()
		: listbox(nullptr)
		, numLength(0)
		, numColumns(0)
		, w(10)
		, h(10)
		, color(GameMakeColor(255, 255, 255, 255))
		, battleHonors(nullptr)
		, easyImage(nullptr)
		, mediumImage(nullptr)
		, brutalImage(nullptr)
		, maxBrutalImage(nullptr)
		, mapToSelect()
		, selectionIndex(0) // always select *something*
		, isMultiplayer(false)
	{
	}

	GameWindow *listbox;
	Int numLength;
	Int numColumns;
	Int w;
	Int h;
	Color color;
	const SkirmishBattleHonors *battleHonors;
	const Image *easyImage;
	const Image *mediumImage;
	const Image *brutalImage;
	const Image *maxBrutalImage;
	AsciiString mapToSelect;
	Int selectionIndex;
	Bool isMultiplayer;
};

//-------------------------------------------------------------------------------------------------
static Bool addMapToMapListbox(
	MapListBoxData& lbData,
	const AsciiString& mapDir,
	const AsciiString& mapName,
	const MapMetaData& mapMetaData)
{
	const Bool mapOk = mapName.startsWithNoCase(mapDir.str()) && lbData.isMultiplayer == mapMetaData.m_isMultiplayer && !mapMetaData.m_displayName.isEmpty();

	if (mapOk)
	{
		UnicodeString mapDisplayName;
		/// @todo: mapDisplayName = TheGameText->fetch(mapMetaData.m_displayName.str());
		mapDisplayName = mapMetaData.m_displayName;

		Int index = -1;
		Int imageItemData = -1;
		if (lbData.numColumns > 1 && mapMetaData.m_isMultiplayer)
		{
			const Int numEasy = lbData.battleHonors->getEnduranceMedal(mapName.str(), SLOT_EASY_AI);
			const Int numMedium = lbData.battleHonors->getEnduranceMedal(mapName.str(), SLOT_MED_AI);
			const Int numBrutal = lbData.battleHonors->getEnduranceMedal(mapName.str(), SLOT_BRUTAL_AI);
			if (numBrutal)
			{
				const Int maxBrutalSlots = mapMetaData.m_numPlayers - 1;
				if (lbData.maxBrutalImage != nullptr && numBrutal == maxBrutalSlots)
				{
					index = GadgetListBoxAddEntryImage( lbData.listbox, lbData.maxBrutalImage, index, 0, lbData.w, lbData.h, TRUE);
					imageItemData = 4;
				}
				else
				{
					index = GadgetListBoxAddEntryImage( lbData.listbox, lbData.brutalImage, index, 0, lbData.w, lbData.h, TRUE);
					imageItemData = 3;
				}
			}
			else if (numMedium)
			{
				imageItemData = 2;
				index = GadgetListBoxAddEntryImage( lbData.listbox, lbData.mediumImage, index, 0, lbData.w, lbData.h, TRUE);
			}
			else if (numEasy)
			{
				imageItemData = 1;
				index = GadgetListBoxAddEntryImage( lbData.listbox, lbData.easyImage, index, 0, lbData.w, lbData.h, TRUE);
			}
			else
			{
				imageItemData = 0;
				index = GadgetListBoxAddEntryImage( lbData.listbox, nullptr, index, 0, lbData.w, lbData.h, TRUE);
			}
		}

		index = GadgetListBoxAddEntryText( lbData.listbox, mapDisplayName, lbData.color, index, lbData.numColumns-1 );
		DEBUG_ASSERTCRASH(index >= 0, ("Expects valid index"));

		if (mapName == lbData.mapToSelect)
		{
			lbData.selectionIndex = index;
		}

		// now set the char* as the item data.  this works because the map cache isn't being
		// modified while a map listbox is up.
		GadgetListBoxSetItemData( lbData.listbox, (void *)(mapName.str()), index );

		if (lbData.numColumns > 1)
		{
			GadgetListBoxSetItemData( lbData.listbox, reinterpret_cast<void*>(std::uintptr_t(imageItemData)), index, 1 );
		}

		// TheSuperHackers @performance Now stops processing when the list is full.
		if (index == lbData.numLength - 1)
		{
			return false;
		}
	}

	return true;
}

//-------------------------------------------------------------------------------------------------
static Bool addMapCollectionToMapListbox(
	MapListBoxData& lbData,
	const AsciiString& mapDir,
	const MapNameList& mapNames,
	const MapDisplayToFileNameList& fileNames)
{
	MapNameList::const_iterator mapNameIt = mapNames.begin();

	for (; mapNameIt != mapNames.end(); ++mapNameIt)
	{
		MapDisplayToFileNameList::const_iterator fileNameIt = fileNames.find(*mapNameIt);
		DEBUG_ASSERTCRASH(fileNameIt != fileNames.end(), ("Map '%s' not found in file names map", mapNameIt->str()));

		const AsciiString& asciiMapName = fileNameIt->second;

#if RTS_ZEROHOUR
		//Patch 1.03 -- Purposely filter out these broken maps that exist in Generals.
		if( !asciiMapName.compare( "maps\\armored fury\\armored fury.map" ) ||
			!asciiMapName.compare( "maps\\scorched earth\\scorched earth.map" ) )
		{
			continue;
		}
#endif

		MapCache::iterator mapCacheIt = TheMapCache->find(asciiMapName);
		DEBUG_ASSERTCRASH(mapCacheIt != TheMapCache->end(), ("Map '%s' not found in map cache.", mapNameIt->str()));
		/*
		if (it != TheMapCache->end())
		{
			DEBUG_LOG(("populateMapListbox(): looking at %s (displayName = %ls), mp = %d (== %d?) mapDir=%s (ok=%d)",
				it->first.str(), it->second.m_displayName.str(), it->second.m_isMultiplayer, isMultiplayer,
				mapDir.str(), it->first.startsWith(mapDir.str())));
		}
		*/

		const Bool ok = addMapToMapListbox(lbData, mapDir, mapCacheIt->first, mapCacheIt->second);

		if (!ok)
			return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------------------
/** Load the listbox with all the map files available to play */
//-------------------------------------------------------------------------------------------------
Int populateMapListboxNoReset( GameWindow *listbox, Bool useSystemMaps, Bool isMultiplayer, AsciiString mapToSelect )
{
	if(!TheMapCache)
		return -1;

	if (!listbox)
		return -1;

	MapListBoxData lbData;
	lbData.listbox = listbox;
	lbData.numLength = GadgetListBoxGetListLength( listbox );
	lbData.numColumns = GadgetListBoxGetNumColumns( listbox );
	lbData.mapToSelect = mapToSelect;
	lbData.isMultiplayer = isMultiplayer;

	if (lbData.numColumns > 1)
	{
		lbData.easyImage = TheMappedImageCollection->findImageByName("Star-Bronze");
		lbData.mediumImage = TheMappedImageCollection->findImageByName("Star-Silver");
		lbData.brutalImage = TheMappedImageCollection->findImageByName("Star-Gold");
		lbData.maxBrutalImage = TheMappedImageCollection->findImageByName("RedYell_Star");
		lbData.battleHonors = new SkirmishBattleHonors;

		lbData.w = lbData.brutalImage ? lbData.brutalImage->getImageWidth() : 10;
		lbData.w = min(GadgetListBoxGetColumnWidth(listbox, 0), lbData.w);
		lbData.h = lbData.w;
	}

	AsciiString mapDir;
	if (useSystemMaps)
	{
		mapDir = TheMapCache->getMapDir();
	}
	else
	{
		mapDir = TheGlobalData->getPath_UserData();
		mapDir.concat(TheMapCache->getMapDir());
	}
	mapDir.toLower();

	MapNameList mapNames;
	MapDisplayToFileNameList fileNames;
	Int curNumPlayersInMap = 1;

	for (; curNumPlayersInMap <= MAX_SLOTS; ++curNumPlayersInMap)
	{
		buildMapListForNumPlayers(mapNames, fileNames, curNumPlayersInMap);

		const Bool ok = addMapCollectionToMapListbox(lbData, mapDir, mapNames, fileNames);

		mapNames.clear();
		fileNames.clear();

		if (!ok)
			break;
	}

	delete lbData.battleHonors;
	lbData.battleHonors = nullptr;

	GadgetListBoxSetSelected(listbox, &lbData.selectionIndex, 1);

	if (lbData.selectionIndex >= 0)
	{
		Int topIndex = GadgetListBoxGetTopVisibleEntry(listbox);
		Int bottomIndex = GadgetListBoxGetBottomVisibleEntry(listbox);
		Int rowsOnScreen = bottomIndex - topIndex;

		if (lbData.selectionIndex >= bottomIndex)
		{
			Int newTop = max( 0, lbData.selectionIndex - max( 1, rowsOnScreen / 2 ) );
			//The trouble is that rowsOnScreen/2 can be zero if bottom is 1 and top is zero
			GadgetListBoxSetTopVisibleEntry( listbox, newTop );
		}
	}

	return lbData.selectionIndex;
}

//-------------------------------------------------------------------------------------------------
/** Load the listbox with all the map files available to play */
//-------------------------------------------------------------------------------------------------
Int populateMapListbox( GameWindow *listbox, Bool useSystemMaps, Bool isMultiplayer, AsciiString mapToSelect )
{
	if(!TheMapCache)
		return -1;

	if (!listbox)
		return -1;

	// reset the listbox content
	GadgetListBoxReset( listbox );

	return populateMapListboxNoReset( listbox, useSystemMaps, isMultiplayer, mapToSelect );
}



//-------------------------------------------------------------------------------------------------
/** Validate a map */
//-------------------------------------------------------------------------------------------------
Bool isValidMap( AsciiString mapName, Bool isMultiplayer )
{
	if(!TheMapCache || mapName.isEmpty())
		return FALSE;
	TheMapCache->updateCache();

	mapName.toLower();
	MapCache::iterator it = TheMapCache->find(mapName);
	if (it != TheMapCache->end())
	{
		if (isMultiplayer == it->second.m_isMultiplayer)
		{
			return TRUE;
		}
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Find a valid map */
//-------------------------------------------------------------------------------------------------
AsciiString getDefaultMap( Bool isMultiplayer )
{
	if(!TheMapCache)
		return AsciiString::TheEmptyString;
	TheMapCache->updateCache();

	MapCache::iterator it = TheMapCache->begin();
	for (; it != TheMapCache->end(); ++it)
	{
		if (isMultiplayer == it->second.m_isMultiplayer)
		{
			return it->first;
		}
	}

	return AsciiString::TheEmptyString;
}


AsciiString getDefaultOfficialMap()
{
	if(!TheMapCache)
		return AsciiString::TheEmptyString;
	TheMapCache->updateCache();

	MapCache::iterator it = TheMapCache->begin();
	for (; it != TheMapCache->end(); ++it)
	{
		if (it->second.m_isMultiplayer && it->second.m_isOfficial)
		{
			return it->first;
		}
	}
	return AsciiString::TheEmptyString;
}


Bool isOfficialMap( AsciiString mapName )
{
	if(!TheMapCache || mapName.isEmpty())
		return FALSE;
	TheMapCache->updateCache();
	mapName.toLower();
	MapCache::iterator it = TheMapCache->find(mapName);
	if (it != TheMapCache->end())
		return it->second.m_isOfficial;
	return FALSE;
}


const MapMetaData *MapCache::findMap(AsciiString mapName)
{
	mapName.toLower();
	MapCache::iterator it = find(mapName);
	if (it == end())
		return nullptr;
	return &(it->second);
}

// ------------------------------------------------------------------------------------------------
/** Embed the pristine map into the xfer stream */
// ------------------------------------------------------------------------------------------------
static void copyFromBigToDir( const AsciiString& infile, const AsciiString& outfile )
{
	// open the map file

	File *file = TheFileSystem->openFile( infile.str(), File::READ | File::BINARY );
	if( file == nullptr )
	{
		DEBUG_CRASH(( "copyFromBigToDir - Error opening source file '%s'", infile.str() ));
		throw SC_INVALID_DATA;
	}

	// how big is the map file
	Int fileSize = file->seek( 0, File::END );


	// rewind to beginning of file
	file->seek( 0, File::START );

	// allocate buffer big enough to hold the entire map file
	char *buffer = NEW char[ fileSize ];
	if( buffer == nullptr )
	{
		DEBUG_CRASH(( "copyFromBigToDir - Unable to allocate buffer for file '%s'", infile.str() ));
		throw SC_INVALID_DATA;
	}

	// copy the file to the buffer
	if( file->read( buffer, fileSize ) < fileSize )
	{
		DEBUG_CRASH(( "copyFromBigToDir - Error reading from file '%s'", infile.str() ));
		throw SC_INVALID_DATA;
	}
	// close the BIG file
	file->close();

	File *filenew = TheFileSystem->openFile( outfile.str(), File::WRITE | File::CREATE | File::BINARY );

	if( !filenew || filenew->write(buffer, fileSize) < fileSize)
	{
		DEBUG_CRASH(( "copyFromBigToDir - Error writing to file '%s'", outfile.str() ));
		throw SC_INVALID_DATA;
	}

	filenew->close();

	// delete the buffer
	delete [] buffer;
}

Image *getMapPreviewImage( AsciiString mapName )
{
	if(!TheGlobalData)
		return nullptr;
	DEBUG_LOG(("%s Map Name", mapName.str()));
	AsciiString tgaName = mapName;
	AsciiString name;
	AsciiString tempName;
	tgaName.truncateBy(4); // ".map"
	name = tgaName;
	tgaName.concat(".tga");

	AsciiString portableName = TheGameState->realMapPathToPortableMapPath(name);
	tempName.set(AsciiString::TheEmptyString);
	for(Int i = 0; i < portableName.getLength(); ++i)
	{
		char c = portableName.getCharAt(i);
		if (c == '\\' || c == ':')
			tempName.concat('_');
		else
			tempName.concat(c);
	}

	name = tempName;
	name.concat(".tga");


	// copy file over
	// copy source tgaName, to name

	Image *image = (Image *)TheMappedImageCollection->findImageByName(tempName);
	if(!image)
	{

		if(!TheFileSystem->doesFileExist(tgaName.str()))
			return nullptr;
		AsciiString mapPreviewDir;
		mapPreviewDir.format(MAP_PREVIEW_DIR_PATH, TheGlobalData->getPath_UserData().str());
		TheFileSystem->createDirectory(mapPreviewDir);

		mapPreviewDir.concat(name);

		Bool success = false;
		try
		{
			copyFromBigToDir(tgaName, mapPreviewDir);
			success = true;
		}
		catch (...)
		{
			success = false;	// no rethrow
		}

		if (success)
		{
    	image = newInstance(Image);
			image->setName(tempName);
			//image->setFullPath("mission.tga");
			image->setFilename(name);
			image->setStatus(IMAGE_STATUS_NONE);
			Region2D uv;
			uv.hi.x = 1.0f;
			uv.hi.y = 1.0f;
			uv.lo.x	= 0.0f;
			uv.lo.y = 0.0f;
			image->setUV(&uv);
			image->setTextureHeight(128);
			image->setTextureWidth(128);
			TheMappedImageCollection->addImage(image);
		}
		else
		{
			image = nullptr;
		}
	}

	return image;



/*
	// sanity
	if( mapName.isEmpty() )
		return nullptr;
	Region2D uv;
	mapPreviewImage = TheMappedImageCollection->findImageByName("MapPreview");
	if(mapPreviewImage)
		deleteInstance(mapPreviewImage);

	mapPreviewImage = TheMappedImageCollection->newImage();
	mapPreviewImage->setName("MapPreview");
	mapPreviewImage->setStatus(IMAGE_STATUS_RAW_TEXTURE);
// allocate our terrain texture
	TextureClass * texture = new TextureClass( size.x, size.y,
																			 WW3D_FORMAT_X8R8G8B8, MIP_LEVELS_1 );
	uv.lo.x = 0.0f;
	uv.lo.y = 1.0f;
	uv.hi.x = 1.0f;
	uv.hi.y = 0.0f;
	mapPreviewImage->setStatus( IMAGE_STATUS_RAW_TEXTURE );
	mapPreviewImage->setRawTextureData( texture );
	mapPreviewImage->setUV( &uv );
	mapPreviewImage->setTextureWidth( size.x );
	mapPreviewImage->setTextureHeight( size.y );
	mapPreviewImage->setImageSize( &size );


	CachedFileInputStream theInputStream;
	if (theInputStream.open(AsciiString(mapName.str())))
	{
		ChunkInputStream *pStrm = &theInputStream;
		pStrm->absoluteSeek(0);
		DataChunkInput file( pStrm );
		if (file.isValidFileType()) {	// Backwards compatible files aren't valid data chunk files.
			// Read the waypoints.
			file.registerParser( "MapPreview", AsciiString::TheEmptyString, parseMapPreviewChunk );
			if (!file.parse(nullptr)) {
				DEBUG_CRASH(("Unable to read MapPreview info."));
				deleteInstance(mapPreviewImage);
				return nullptr;
			}
		}
		theInputStream.close();
	}
	else
	{
		deleteInstance(mapPreviewImage);
		return nullptr;
	}


	return mapPreviewImage;

*/
	return nullptr;
}

Bool parseMapPreviewChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
/*
	ICoord2D size;

	SurfaceClass *surface;
	size.x = file.readInt();
	size.y = file.readInt();


	surface = (TextureClass *)mapPreviewImage->getRawTextureData()->Get_Surface_Level();
	//texture->Get_Surface_Level();

	DEBUG_LOG(("BeginMapPreviewInfo"));
	int pitch;
	void *pBits = surface->Lock(&pitch);
	const unsigned int bytesPerPixel = surface->Get_Bytes_Per_Pixel();
	UnsignedInt *buffer = new UnsignedInt[size.x * size.y];
	Int x,y;
	for (y=0; y<size.y; y++) {
		for(x = 0; x< size.x; x++)
		{
			surface->Draw_Pixel( x, y, file.readInt(), bytesPerPixel, pBits, pitch );
			buffer[y + x] = file.readInt();
			DEBUG_LOG(("x:%d, y:%d, %X", x, y, buffer[y + x]));
		}
	}
	mapPreviewImage->setRawTextureData(buffer);
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	DEBUG_LOG(("EndMapPreviewInfo"));
	surface->Unlock();
	REF_PTR_RELEASE(surface);
	return true;
*/
	return FALSE;
}

void findDrawPositions( Int startX, Int startY, Int width, Int height, Region3D extent,
															 ICoord2D *ul, ICoord2D *lr )
{

	Real ratioWidth;
	Real ratioHeight;
	Coord2D radar;
	ratioWidth = extent.width()/(width * 1.0f);
	ratioHeight = extent.height()/(height* 1.0f);

	if( ratioWidth >= ratioHeight)
	{
		radar.x = extent.width() / ratioWidth;
		radar.y = extent.height()/ ratioWidth;
		ul->x = 0;
		ul->y = (height - radar.y) / 2.0f;
		lr->x = radar.x;
		lr->y = height - ul->y;
	}
	else
	{
		radar.x = extent.width() / ratioHeight;
		radar.y = extent.height()/ ratioHeight;
		ul->x = (width - radar.x ) / 2.0f;
		ul->y = 0;
		lr->x = width - ul->x;
		lr->y = radar.y;
	}

	// make them pixel positions
	ul->x += startX;
	ul->y += startY;
	lr->x += startX;
	lr->y += startY;

}

