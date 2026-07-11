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

// FILE: GlobalData.cpp ///////////////////////////////////////////////////////////////////////////
// The GameLogicData object
// Author: trolfs, Michael Booth, Colin Day, April 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

//#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#ifndef _WIN32
#include <filesystem>
#endif
#include "ww3d.h"
#include "texturefilter.h"

#include "Common/GlobalData.h"

#define DEFINE_TERRAIN_LOD_NAMES
#define DEFINE_TIME_OF_DAY_NAMES
#define DEFINE_WEATHER_NAMES
#define DEFINE_BODYDAMAGETYPE_NAMES
#define DEFINE_PANNING_NAMES

#include "Common/AddonCompat.h"
#include "Common/crc.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/GameAudio.h"
#include "Common/INI.h"
#include "Common/OptionPreferences.h"
#include "Common/version.h"

#include "GameLogic/AI.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/BodyModule.h"

#include "GameClient/Color.h"
#include "GameClient/TerrainVisual.h"

#include "GameNetwork/FirewallHelper.h"

// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
GlobalData* TheWritableGlobalData = nullptr;				///< The global data singleton

//-------------------------------------------------------------------------------------------------
GlobalData* GlobalData::m_theOriginal = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
/*static*/ const FieldParse GlobalData::s_GlobalDataFieldParseTable[] =
{
	// GeneralsX @feature BenderAI 21/04/2026 Opt-out flag for in-game update checker
	{ "CheckForUpdates",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_checkForUpdates ) },
	{ "Windowed",									INI::parseBool,				nullptr,			offsetof( GlobalData, m_windowed ) },
	{ "XResolution",							INI::parseInt,				nullptr,			offsetof( GlobalData, m_xResolution ) },
	{ "YResolution",							INI::parseInt,				nullptr,			offsetof( GlobalData, m_yResolution ) },
	{ "MapName",									INI::parseAsciiString,nullptr,			offsetof( GlobalData, m_mapName ) },
	{ "MoveHintName",							INI::parseAsciiString,nullptr,			offsetof( GlobalData, m_moveHintName ) },
	{ "UseTrees",									INI::parseBool,				nullptr,			offsetof( GlobalData, m_useTrees ) },
	{ "UseFPSLimit",							INI::parseBool,				nullptr,			offsetof( GlobalData, m_useFpsLimit ) },
	{ "DumpAssetUsage",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_dumpAssetUsage ) },
	{ "FramesPerSecondLimit",			INI::parseInt,				nullptr,			offsetof( GlobalData, m_framesPerSecondLimit ) },
	{ "ChipsetType",							INI::parseInt,				nullptr,			offsetof( GlobalData, m_chipSetType ) },
	{ "MaxShellScreens",					INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxShellScreens ) },
	{ "UseCloudMap",							INI::parseBool,				nullptr,			offsetof( GlobalData, m_useCloudMap ) },
	{ "UseLightMap",							INI::parseBool,				nullptr,			offsetof( GlobalData, m_useLightMap ) },
	{ "BilinearTerrainTex",				INI::parseBool,				nullptr,			offsetof( GlobalData, m_bilinearTerrainTex ) },
	{ "TrilinearTerrainTex",			INI::parseBool,				nullptr,			offsetof( GlobalData, m_trilinearTerrainTex ) },
	{ "MultiPassTerrain",					INI::parseBool,				nullptr,			offsetof( GlobalData, m_multiPassTerrain ) },
	{ "AdjustCliffTextures",			INI::parseBool,				nullptr,			offsetof( GlobalData, m_adjustCliffTextures ) },
	{ "Use3WayTerrainBlends",			INI::parseInt,				nullptr,			offsetof( GlobalData, m_use3WayTerrainBlends ) },
	{ "StretchTerrain",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_stretchTerrain ) },
	{ "UseHalfHeightMap",					INI::parseBool,				nullptr,			offsetof( GlobalData, m_useHalfHeightMap ) },
	{ "UserDataLeafName",					INI::parseQuotedAsciiString,	nullptr,			offsetof( GlobalData, m_userDataLeafName ) },


	{ "DrawEntireTerrain",					INI::parseBool,				nullptr,			offsetof( GlobalData, m_drawEntireTerrain ) },
	{ "TerrainLOD",									INI::parseIndexList,	TerrainLODNames,	offsetof( GlobalData, m_terrainLOD ) },
	{ "TerrainLODTargetTimeMS",			INI::parseInt,				nullptr,			offsetof( GlobalData, m_terrainLODTargetTimeMS ) },
	{ "RightMouseAlwaysScrolls",		INI::parseBool,				nullptr,			offsetof( GlobalData, m_rightMouseAlwaysScrolls ) },
	{ "UseWaterPlane",							INI::parseBool,				nullptr,			offsetof( GlobalData, m_useWaterPlane ) },
	{ "UseCloudPlane",							INI::parseBool,				nullptr,			offsetof( GlobalData, m_useCloudPlane ) },
	{ "DownwindAngle",							INI::parseReal,				nullptr,			offsetof( GlobalData, m_downwindAngle ) },
	{ "UseShadowVolumes",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_useShadowVolumes ) },
	{ "UseShadowDecals",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_useShadowDecals ) },
	{ "TextureReductionFactor",			INI::parseInt,				nullptr,			offsetof( GlobalData, m_textureReductionFactor ) },
	{ "UseBehindBuildingMarker",		INI::parseBool,				nullptr,			offsetof( GlobalData, m_enableBehindBuildingMarkers ) },
	{ "WaterPositionX",							INI::parseReal,				nullptr,			offsetof( GlobalData, m_waterPositionX ) },
	{ "WaterPositionY",							INI::parseReal,				nullptr,			offsetof( GlobalData, m_waterPositionY ) },
	{ "WaterPositionZ",							INI::parseReal,				nullptr,			offsetof( GlobalData, m_waterPositionZ ) },
	{ "WaterExtentX",								INI::parseReal,				nullptr,			offsetof( GlobalData, m_waterExtentX ) },
	{ "WaterExtentY",								INI::parseReal,				nullptr,			offsetof( GlobalData, m_waterExtentY ) },
	{ "WaterType",									INI::parseInt,				nullptr,			offsetof( GlobalData, m_waterType ) },
	{ "FeatherWater",						  	INI::parseInt,				nullptr,			offsetof( GlobalData, m_featherWater ) },
	{ "ShowSoftWaterEdge",					INI::parseBool,				nullptr,			offsetof( GlobalData, m_showSoftWaterEdge ) },

	// nasty ick, we need to save this data with a map and not hard code INI values
	{ "VertexWaterAvailableMaps1",		INI::parseAsciiString,	nullptr,		offsetof( GlobalData, m_vertexWaterAvailableMaps[ 0 ] ) },
	{ "VertexWaterHeightClampLow1",		INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterHeightClampLow[ 0 ] ) },
	{ "VertexWaterHeightClampHi1",		INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterHeightClampHi[ 0 ] ) },
	{ "VertexWaterAngle1",						INI::parseAngleReal,		nullptr,		offsetof( GlobalData, m_vertexWaterAngle[ 0 ] ) },
	{ "VertexWaterXPosition1",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterXPosition[ 0 ] ) },
	{ "VertexWaterYPosition1",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterYPosition[ 0 ] ) },
	{ "VertexWaterZPosition1",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterZPosition[ 0 ] ) },
	{ "VertexWaterXGridCells1",				INI::parseInt,					nullptr,		offsetof( GlobalData, m_vertexWaterXGridCells[ 0 ] ) },
	{ "VertexWaterYGridCells1",				INI::parseInt,					nullptr,		offsetof( GlobalData, m_vertexWaterYGridCells[ 0 ] ) },
	{ "VertexWaterGridSize1",					INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterGridSize[ 0 ] ) },
	{ "VertexWaterAttenuationA1",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationA[ 0 ] ) },
	{ "VertexWaterAttenuationB1",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationB[ 0 ] ) },
	{ "VertexWaterAttenuationC1",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationC[ 0 ] ) },
	{ "VertexWaterAttenuationRange1",	INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationRange[ 0 ] ) },

	// nasty ick, we need to save this data with a map and not hard code INI values
	{ "VertexWaterAvailableMaps2",		INI::parseAsciiString,	nullptr,		offsetof( GlobalData, m_vertexWaterAvailableMaps[ 1 ] ) },
	{ "VertexWaterHeightClampLow2",		INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterHeightClampLow[ 1 ] ) },
	{ "VertexWaterHeightClampHi2",		INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterHeightClampHi[ 1 ] ) },
	{ "VertexWaterAngle2",						INI::parseAngleReal,		nullptr,		offsetof( GlobalData, m_vertexWaterAngle[ 1 ] ) },
	{ "VertexWaterXPosition2",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterXPosition[ 1 ] ) },
	{ "VertexWaterYPosition2",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterYPosition[ 1 ] ) },
	{ "VertexWaterZPosition2",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterZPosition[ 1 ] ) },
	{ "VertexWaterXGridCells2",				INI::parseInt,					nullptr,		offsetof( GlobalData, m_vertexWaterXGridCells[ 1 ] ) },
	{ "VertexWaterYGridCells2",				INI::parseInt,					nullptr,		offsetof( GlobalData, m_vertexWaterYGridCells[ 1 ] ) },
	{ "VertexWaterGridSize2",					INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterGridSize[ 1 ] ) },
	{ "VertexWaterAttenuationA2",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationA[ 1 ] ) },
	{ "VertexWaterAttenuationB2",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationB[ 1 ] ) },
	{ "VertexWaterAttenuationC2",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationC[ 1 ] ) },
	{ "VertexWaterAttenuationRange2",	INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationRange[ 1 ] ) },

	// nasty ick, we need to save this data with a map and not hard code INI values
	{ "VertexWaterAvailableMaps3",		INI::parseAsciiString,	nullptr,		offsetof( GlobalData, m_vertexWaterAvailableMaps[ 2 ] ) },
	{ "VertexWaterHeightClampLow3",		INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterHeightClampLow[ 2 ] ) },
	{ "VertexWaterHeightClampHi3",		INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterHeightClampHi[ 2 ] ) },
	{ "VertexWaterAngle3",						INI::parseAngleReal,		nullptr,		offsetof( GlobalData, m_vertexWaterAngle[ 2 ] ) },
	{ "VertexWaterXPosition3",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterXPosition[ 2 ] ) },
	{ "VertexWaterYPosition3",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterYPosition[ 2 ] ) },
	{ "VertexWaterZPosition3",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterZPosition[ 2 ] ) },
	{ "VertexWaterXGridCells3",				INI::parseInt,					nullptr,		offsetof( GlobalData, m_vertexWaterXGridCells[ 2 ] ) },
	{ "VertexWaterYGridCells3",				INI::parseInt,					nullptr,		offsetof( GlobalData, m_vertexWaterYGridCells[ 2 ] ) },
	{ "VertexWaterGridSize3",					INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterGridSize[ 2 ] ) },
	{ "VertexWaterAttenuationA3",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationA[ 2 ] ) },
	{ "VertexWaterAttenuationB3",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationB[ 2 ] ) },
	{ "VertexWaterAttenuationC3",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationC[ 2 ] ) },
	{ "VertexWaterAttenuationRange3",	INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationRange[ 2 ] ) },

	// nasty ick, we need to save this data with a map and not hard code INI values
	{ "VertexWaterAvailableMaps4",		INI::parseAsciiString,	nullptr,		offsetof( GlobalData, m_vertexWaterAvailableMaps[ 3 ] ) },
	{ "VertexWaterHeightClampLow4",		INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterHeightClampLow[ 3 ] ) },
	{ "VertexWaterHeightClampHi4",		INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterHeightClampHi[ 3 ] ) },
	{ "VertexWaterAngle4",						INI::parseAngleReal,		nullptr,		offsetof( GlobalData, m_vertexWaterAngle[ 3 ] ) },
	{ "VertexWaterXPosition4",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterXPosition[ 3 ] ) },
	{ "VertexWaterYPosition4",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterYPosition[ 3 ] ) },
	{ "VertexWaterZPosition4",				INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterZPosition[ 3 ] ) },
	{ "VertexWaterXGridCells4",				INI::parseInt,					nullptr,		offsetof( GlobalData, m_vertexWaterXGridCells[ 3 ] ) },
	{ "VertexWaterYGridCells4",				INI::parseInt,					nullptr,		offsetof( GlobalData, m_vertexWaterYGridCells[ 3 ] ) },
	{ "VertexWaterGridSize4",					INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterGridSize[ 3 ] ) },
	{ "VertexWaterAttenuationA4",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationA[ 3 ] ) },
	{ "VertexWaterAttenuationB4",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationB[ 3 ] ) },
	{ "VertexWaterAttenuationC4",			INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationC[ 3 ] ) },
	{ "VertexWaterAttenuationRange4",	INI::parseReal,					nullptr,		offsetof( GlobalData, m_vertexWaterAttenuationRange[ 3 ] ) },

	{ "SkyBoxPositionZ",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_skyBoxPositionZ ) },
	{ "SkyBoxScale",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_skyBoxScale ) },
	{ "DrawSkyBox",				INI::parseBool,				nullptr,			offsetof( GlobalData, m_drawSkyBox ) },
	{ "ViewportHeightScale", INI::parseReal, nullptr, offsetof( GlobalData, m_viewportHeightScale ) },
	{ "CameraPitch",								INI::parseReal,				nullptr,			offsetof( GlobalData, m_cameraPitch ) },
	{ "CameraYaw",									INI::parseReal,				nullptr,			offsetof( GlobalData, m_cameraYaw ) },
#if PRESERVE_RETAIL_SCRIPTED_CAMERA
	{ "CameraHeight",								INI::parseReal,				nullptr,			offsetof( GlobalData, m_cameraHeight ) },
#endif
	{ "MaxCameraHeight",						INI::parseReal,				nullptr,			offsetof( GlobalData, m_maxCameraHeight ) },
	{ "MinCameraHeight",						INI::parseReal,				nullptr,			offsetof( GlobalData, m_minCameraHeight ) },
	{ "TerrainDrawDistanceScale",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_terrainDrawDistanceScale ) },
	{ "TerrainHeightAtEdgeOfMap",					INI::parseReal,				nullptr,			offsetof( GlobalData, m_terrainHeightAtEdgeOfMap ) },
	{ "UnitDamagedThreshold",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_unitDamagedThresh ) },
	{ "UnitReallyDamagedThreshold",	INI::parseReal,				nullptr,			offsetof( GlobalData, m_unitReallyDamagedThresh ) },
	{ "GroundStiffness",					INI::parseReal,				nullptr,				offsetof( GlobalData, m_groundStiffness ) },
	{ "StructureStiffness",					INI::parseReal,				nullptr,				offsetof( GlobalData, m_structureStiffness ) },
	{ "Gravity",									INI::parseAccelerationReal,				nullptr,				offsetof( GlobalData, m_gravity ) },
	{ "StealthFriendlyOpacity",		INI::parsePercentToReal,				nullptr,				offsetof( GlobalData, m_stealthFriendlyOpacity ) },
	{ "DefaultOcclusionDelay",				INI::parseDurationUnsignedInt,				nullptr,			offsetof( GlobalData, m_defaultOcclusionDelay ) },

	{ "PartitionCellSize",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_partitionCellSize ) },

	{ "AmmoPipScaleFactor",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_ammoPipScaleFactor ) },
	{ "ContainerPipScaleFactor",	INI::parseReal,				nullptr,			offsetof( GlobalData, m_containerPipScaleFactor ) },
	{ "AmmoPipWorldOffset",						INI::parseCoord3D,				nullptr,			offsetof( GlobalData, m_ammoPipWorldOffset ) },
	{ "ContainerPipWorldOffset",				INI::parseCoord3D,				nullptr,			offsetof( GlobalData, m_containerPipWorldOffset ) },
	{ "AmmoPipScreenOffset",						INI::parseCoord2D,				nullptr,			offsetof( GlobalData, m_ammoPipScreenOffset ) },
	{ "ContainerPipScreenOffset",				INI::parseCoord2D,				nullptr,			offsetof( GlobalData, m_containerPipScreenOffset ) },

	{ "HistoricDamageLimit",				INI::parseDurationUnsignedInt,				nullptr,			offsetof( GlobalData, m_historicDamageLimit ) },

	{ "MaxTerrainTracks",					INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxTerrainTracks ) },
	{ "TimeOfDay",								INI::parseIndexList,	TimeOfDayNames,			offsetof( GlobalData, m_timeOfDay ) },
	{ "Weather",									INI::parseIndexList,	WeatherNames,			offsetof( GlobalData, m_weather ) },
	{ "MakeTrackMarks",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_makeTrackMarks ) },
	{ "HideGarrisonFlags",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_hideGarrisonFlags ) },
	{ "ForceModelsToFollowTimeOfDay",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_forceModelsToFollowTimeOfDay ) },
	{ "ForceModelsToFollowWeather",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_forceModelsToFollowWeather ) },

	{ "LevelGainAnimationName",		INI::parseAsciiString,	nullptr,	offsetof( GlobalData, m_levelGainAnimationName ) },
	{ "LevelGainAnimationTime",		INI::parseReal,					nullptr, offsetof( GlobalData, m_levelGainAnimationDisplayTimeInSeconds ) },
	{ "LevelGainAnimationZRise",	INI::parseReal,					nullptr, offsetof( GlobalData, m_levelGainAnimationZRisePerSecond ) },

	{ "GetHealedAnimationName",		INI::parseAsciiString,	nullptr,	offsetof( GlobalData, m_getHealedAnimationName ) },
	{ "GetHealedAnimationTime",		INI::parseReal,					nullptr, offsetof( GlobalData, m_getHealedAnimationDisplayTimeInSeconds ) },
	{ "GetHealedAnimationZRise",	INI::parseReal,					nullptr, offsetof( GlobalData, m_getHealedAnimationZRisePerSecond ) },

	{ "TerrainLightingMorningAmbient",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][0].ambient ) },
	{ "TerrainLightingMorningDiffuse",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][0].diffuse ) },
	{ "TerrainLightingMorningLightPos",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][0].lightPos ) },
	{ "TerrainLightingAfternoonAmbient",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][0].ambient ) },
	{ "TerrainLightingAfternoonDiffuse",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][0].diffuse ) },
	{ "TerrainLightingAfternoonLightPos",	INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][0].lightPos ) },
	{ "TerrainLightingEveningAmbient",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][0].ambient ) },
	{ "TerrainLightingEveningDiffuse",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][0].diffuse ) },
	{ "TerrainLightingEveningLightPos",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][0].lightPos ) },
	{ "TerrainLightingNightAmbient",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][0].ambient ) },
	{ "TerrainLightingNightDiffuse",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][0].diffuse ) },
	{ "TerrainLightingNightLightPos",			INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][0].lightPos ) },

	{ "TerrainObjectsLightingMorningAmbient",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][0].ambient ) },
	{ "TerrainObjectsLightingMorningDiffuse",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][0].diffuse ) },
	{ "TerrainObjectsLightingMorningLightPos",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][0].lightPos ) },
	{ "TerrainObjectsLightingAfternoonAmbient",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][0].ambient ) },
	{ "TerrainObjectsLightingAfternoonDiffuse",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][0].diffuse ) },
	{ "TerrainObjectsLightingAfternoonLightPos",	INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][0].lightPos ) },
	{ "TerrainObjectsLightingEveningAmbient",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][0].ambient ) },
	{ "TerrainObjectsLightingEveningDiffuse",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][0].diffuse ) },
	{ "TerrainObjectsLightingEveningLightPos",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][0].lightPos ) },
	{ "TerrainObjectsLightingNightAmbient",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][0].ambient ) },
	{ "TerrainObjectsLightingNightDiffuse",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][0].diffuse ) },
	{ "TerrainObjectsLightingNightLightPos",			INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][0].lightPos ) },

	//Secondary global light
	{ "TerrainLightingMorningAmbient2",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][1].ambient ) },
	{ "TerrainLightingMorningDiffuse2",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][1].diffuse ) },
	{ "TerrainLightingMorningLightPos2",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][1].lightPos ) },
	{ "TerrainLightingAfternoonAmbient2",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][1].ambient ) },
	{ "TerrainLightingAfternoonDiffuse2",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][1].diffuse ) },
	{ "TerrainLightingAfternoonLightPos2",	INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][1].lightPos ) },
	{ "TerrainLightingEveningAmbient2",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][1].ambient ) },
	{ "TerrainLightingEveningDiffuse2",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][1].diffuse ) },
	{ "TerrainLightingEveningLightPos2",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][1].lightPos ) },
	{ "TerrainLightingNightAmbient2",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][1].ambient ) },
	{ "TerrainLightingNightDiffuse2",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][1].diffuse ) },
	{ "TerrainLightingNightLightPos2",			INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][1].lightPos ) },

	{ "TerrainObjectsLightingMorningAmbient2",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][1].ambient ) },
	{ "TerrainObjectsLightingMorningDiffuse2",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][1].diffuse ) },
	{ "TerrainObjectsLightingMorningLightPos2",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][1].lightPos ) },
	{ "TerrainObjectsLightingAfternoonAmbient2",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][1].ambient ) },
	{ "TerrainObjectsLightingAfternoonDiffuse2",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][1].diffuse ) },
	{ "TerrainObjectsLightingAfternoonLightPos2",	INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][1].lightPos ) },
	{ "TerrainObjectsLightingEveningAmbient2",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][1].ambient ) },
	{ "TerrainObjectsLightingEveningDiffuse2",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][1].diffuse ) },
	{ "TerrainObjectsLightingEveningLightPos2",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][1].lightPos ) },
	{ "TerrainObjectsLightingNightAmbient2",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][1].ambient ) },
	{ "TerrainObjectsLightingNightDiffuse2",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][1].diffuse ) },
	{ "TerrainObjectsLightingNightLightPos2",			INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][1].lightPos ) },

	//Third global light
	{ "TerrainLightingMorningAmbient3",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][2].ambient ) },
	{ "TerrainLightingMorningDiffuse3",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][2].diffuse ) },
	{ "TerrainLightingMorningLightPos3",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_MORNING ][2].lightPos ) },
	{ "TerrainLightingAfternoonAmbient3",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][2].ambient ) },
	{ "TerrainLightingAfternoonDiffuse3",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][2].diffuse ) },
	{ "TerrainLightingAfternoonLightPos3",	INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_AFTERNOON ][2].lightPos ) },
	{ "TerrainLightingEveningAmbient3",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][2].ambient ) },
	{ "TerrainLightingEveningDiffuse3",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][2].diffuse ) },
	{ "TerrainLightingEveningLightPos3",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_EVENING ][2].lightPos ) },
	{ "TerrainLightingNightAmbient3",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][2].ambient ) },
	{ "TerrainLightingNightDiffuse3",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][2].diffuse ) },
	{ "TerrainLightingNightLightPos3",			INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainLighting[ TIME_OF_DAY_NIGHT ][2].lightPos ) },

	{ "TerrainObjectsLightingMorningAmbient3",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][2].ambient ) },
	{ "TerrainObjectsLightingMorningDiffuse3",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][2].diffuse ) },
	{ "TerrainObjectsLightingMorningLightPos3",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_MORNING ][2].lightPos ) },
	{ "TerrainObjectsLightingAfternoonAmbient3",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][2].ambient ) },
	{ "TerrainObjectsLightingAfternoonDiffuse3",		INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][2].diffuse ) },
	{ "TerrainObjectsLightingAfternoonLightPos3",	INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_AFTERNOON ][2].lightPos ) },
	{ "TerrainObjectsLightingEveningAmbient3",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][2].ambient ) },
	{ "TerrainObjectsLightingEveningDiffuse3",			INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][2].diffuse ) },
	{ "TerrainObjectsLightingEveningLightPos3",		INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_EVENING ][2].lightPos ) },
	{ "TerrainObjectsLightingNightAmbient3",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][2].ambient ) },
	{ "TerrainObjectsLightingNightDiffuse3",				INI::parseRGBColor,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][2].diffuse ) },
	{ "TerrainObjectsLightingNightLightPos3",			INI::parseCoord3D,			nullptr,			offsetof( GlobalData, m_terrainObjectsLighting[ TIME_OF_DAY_NIGHT ][2].lightPos ) },


	{ "NumberGlobalLights",								INI::parseInt,				nullptr,			offsetof( GlobalData, m_numGlobalLights)},
	{ "InfantryLightMorningScale",				INI::parseReal,			nullptr,			offsetof( GlobalData, m_infantryLightScale[TIME_OF_DAY_MORNING] ) },
	{ "InfantryLightAfternoonScale",				INI::parseReal,			nullptr,			offsetof( GlobalData, m_infantryLightScale[TIME_OF_DAY_AFTERNOON] ) },
	{ "InfantryLightEveningScale",				INI::parseReal,			nullptr,			offsetof( GlobalData, m_infantryLightScale[TIME_OF_DAY_EVENING] ) },
	{ "InfantryLightNightScale",				INI::parseReal,			nullptr,			offsetof( GlobalData, m_infantryLightScale[TIME_OF_DAY_NIGHT] ) },

	{ "MaxTranslucentObjects",						INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxVisibleTranslucentObjects) },
	{ "OccludedColorLuminanceScale",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_occludedLuminanceScale) },

/* These are internal use only, they do not need file definitions
	{ "TerrainAmbientRGB",				INI::parseRGBColor,		nullptr,			offsetof( GlobalData, m_terrainAmbient ) },
	{ "TerrainDiffuseRGB",				INI::parseRGBColor,		nullptr,			offsetof( GlobalData, m_terrainDiffuse ) },
	{ "TerrainLightPos",					INI::parseCoord3D,		nullptr,			offsetof( GlobalData, m_terrainLightPos ) },
*/
	{ "MaxRoadSegments",						INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxRoadSegments ) },
	{ "MaxRoadVertex",							INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxRoadVertex ) },
	{ "MaxRoadIndex",								INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxRoadIndex ) },
	{ "MaxRoadTypes",								INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxRoadTypes ) },

	{ "ValuePerSupplyBox",					INI::parseInt,				nullptr,			offsetof( GlobalData, m_baseValuePerSupplyBox ) },

	{ "AudioOn",										INI::parseBool,				nullptr,			offsetof( GlobalData, m_audioOn ) },
	{ "MusicOn",										INI::parseBool,				nullptr,			offsetof( GlobalData, m_musicOn ) },
	{ "SoundsOn",										INI::parseBool,				nullptr,			offsetof( GlobalData, m_soundsOn ) },
	{ "Sounds3DOn",									INI::parseBool,				nullptr,			offsetof( GlobalData, m_sounds3DOn ) },
	{ "SpeechOn",										INI::parseBool,				nullptr,			offsetof( GlobalData, m_speechOn ) },
	{ "VideoOn",										INI::parseBool,				nullptr,			offsetof( GlobalData, m_videoOn ) },
	{ "DisableCameraMovements",			INI::parseBool,				nullptr,			offsetof( GlobalData, m_disableCameraMovement ) },

/* These are internal use only, they do not need file definitions
	/// @todo remove this hack
	{ "InGame",											INI::parseBool,				nullptr,			offsetof( GlobalData, m_inGame ) },
*/

	{ "DebugAI",										INI::parseBool,				nullptr,			offsetof( GlobalData, m_debugAI ) },
	{ "DebugAIObstacles",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_debugAIObstacles ) },
	{ "ShowClientPhysics",				INI::parseBool,				nullptr,			offsetof( GlobalData, m_showClientPhysics ) },
	{ "ShowTerrainNormals",				INI::parseBool,				nullptr,			offsetof( GlobalData, m_showTerrainNormals ) },
	{ "ShowObjectHealth",						INI::parseBool,				nullptr,			offsetof( GlobalData, m_showObjectHealth ) },

	{ "ParticleScale",										INI::parseReal,					nullptr,	 offsetof( GlobalData, m_particleScale ) },
	{ "AutoFireParticleSmallPrefix",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoFireParticleSmallPrefix ) },
	{ "AutoFireParticleSmallSystem",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoFireParticleSmallSystem ) },
	{ "AutoFireParticleSmallMax",					INI::parseInt,					nullptr,	 offsetof( GlobalData, m_autoFireParticleSmallMax ) },
	{ "AutoFireParticleMediumPrefix",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoFireParticleMediumPrefix ) },
	{ "AutoFireParticleMediumSystem",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoFireParticleMediumSystem ) },
	{ "AutoFireParticleMediumMax",				INI::parseInt,					nullptr,	 offsetof( GlobalData, m_autoFireParticleMediumMax ) },
	{ "AutoFireParticleLargePrefix",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoFireParticleLargePrefix ) },
	{ "AutoFireParticleLargeSystem",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoFireParticleLargeSystem ) },
	{ "AutoFireParticleLargeMax",					INI::parseInt,					nullptr,	 offsetof( GlobalData, m_autoFireParticleLargeMax ) },
	{ "AutoSmokeParticleSmallPrefix",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoSmokeParticleSmallPrefix ) },
	{ "AutoSmokeParticleSmallSystem",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoSmokeParticleSmallSystem ) },
	{ "AutoSmokeParticleSmallMax",				INI::parseInt,					nullptr,	 offsetof( GlobalData, m_autoSmokeParticleSmallMax ) },
	{ "AutoSmokeParticleMediumPrefix",		INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoSmokeParticleMediumPrefix ) },
	{ "AutoSmokeParticleMediumSystem",		INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoSmokeParticleMediumSystem ) },
	{ "AutoSmokeParticleMediumMax",				INI::parseInt,					nullptr,	 offsetof( GlobalData, m_autoSmokeParticleMediumMax ) },
	{ "AutoSmokeParticleLargePrefix",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoSmokeParticleLargePrefix ) },
	{ "AutoSmokeParticleLargeSystem",			INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoSmokeParticleLargeSystem ) },
	{ "AutoSmokeParticleLargeMax",				INI::parseInt,					nullptr,	 offsetof( GlobalData, m_autoSmokeParticleLargeMax ) },
	{ "AutoAflameParticlePrefix",					INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoAflameParticlePrefix ) },
	{ "AutoAflameParticleSystem",					INI::parseAsciiString,  nullptr,  offsetof( GlobalData, m_autoAflameParticleSystem ) },
	{ "AutoAflameParticleMax",						INI::parseInt,					nullptr,	 offsetof( GlobalData, m_autoAflameParticleMax ) },

/* These are internal use only, they do not need file definitions
	{ "LatencyAverage",							INI::parseInt,				nullptr,			offsetof( GlobalData, m_latencyAverage ) },
	{ "LatencyAmplitude",						INI::parseInt,				nullptr,			offsetof( GlobalData, m_latencyAmplitude ) },
	{ "LatencyPeriod",							INI::parseInt,				nullptr,			offsetof( GlobalData, m_latencyPeriod ) },
	{ "LatencyNoise",								INI::parseInt,				nullptr,			offsetof( GlobalData, m_latencyNoise ) },
	{ "PacketLoss",									INI::parseInt,				nullptr,			offsetof( GlobalData, m_packetLoss ) },
*/

	{ "BuildSpeed",									INI::parseReal,				nullptr,			offsetof( GlobalData, m_BuildSpeed ) },
	{ "MinDistFromEdgeOfMapForBuild",	 INI::parseReal,				nullptr,			offsetof( GlobalData, m_MinDistFromEdgeOfMapForBuild ) },
	{ "SupplyBuildBorder",	 INI::parseReal,				nullptr,			offsetof( GlobalData, m_SupplyBuildBorder ) },
	{ "AllowedHeightVariationForBuilding", INI::parseReal,nullptr,			offsetof( GlobalData, m_allowedHeightVariationForBuilding ) },
	{ "MinLowEnergyProductionSpeed",INI::parseReal,				nullptr,			offsetof( GlobalData, m_MinLowEnergyProductionSpeed ) },
	{ "MaxLowEnergyProductionSpeed",INI::parseReal,				nullptr,			offsetof( GlobalData, m_MaxLowEnergyProductionSpeed ) },
	{ "LowEnergyPenaltyModifier",		INI::parseReal,				nullptr,			offsetof( GlobalData, m_LowEnergyPenaltyModifier ) },
	{ "MultipleFactory",						INI::parseReal,				nullptr,			offsetof( GlobalData, m_MultipleFactory ) },
	{ "RefundPercent",							INI::parsePercentToReal,			nullptr,			offsetof( GlobalData, m_RefundPercent ) },

	{ "CommandCenterHealRange",			INI::parseReal,				nullptr,			offsetof( GlobalData, m_commandCenterHealRange ) },
	{ "CommandCenterHealAmount",		INI::parseReal,				nullptr,			offsetof( GlobalData, m_commandCenterHealAmount ) },

	{ "StandardMinefieldDensity",		INI::parseReal,				nullptr,			offsetof( GlobalData, m_standardMinefieldDensity ) },
	{ "StandardMinefieldDistance",		INI::parseReal,				nullptr,			offsetof( GlobalData, m_standardMinefieldDistance ) },

	{ "MaxLineBuildObjects",				INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxLineBuildObjects ) },
	{ "MaxTunnelCapacity",					INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxTunnelCapacity ) },

	{ "MaxParticleCount",						INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxParticleCount ) },
	{ "MaxFieldParticleCount",						INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxFieldParticleCount ) },
	{ "HorizontalScrollSpeedFactor",INI::parseReal,				nullptr,			offsetof( GlobalData, m_horizontalScrollSpeedFactor ) },
	{ "VerticalScrollSpeedFactor",	INI::parseReal,				nullptr,			offsetof( GlobalData, m_verticalScrollSpeedFactor ) },
	{ "ScrollAmountCutoff",					INI::parseReal,				nullptr,			offsetof( GlobalData, m_scrollAmountCutoff ) },
	{ "CameraAdjustSpeed",					INI::parseReal,				nullptr,			offsetof( GlobalData, m_cameraAdjustSpeed ) },
	{ "EnforceMaxCameraHeight",			INI::parseBool,				nullptr,			offsetof( GlobalData, m_enforceMaxCameraHeight ) },
	{ "KeyboardScrollSpeedFactor",	INI::parseReal,				nullptr,			offsetof( GlobalData, m_keyboardScrollFactor ) },
	{ "KeyboardDefaultScrollSpeedFactor",	INI::parseReal,				nullptr,			offsetof( GlobalData, m_keyboardDefaultScrollFactor ) },
	{ "KeyboardCameraRotateSpeed", INI::parseReal, nullptr, offsetof( GlobalData, m_keyboardCameraRotateSpeed ) },
	{ "MovementPenaltyDamageState",	INI::parseIndexList,	TheBodyDamageTypeNames,	 offsetof( GlobalData, m_movementPenaltyDamageState ) },

// you cannot set this; it always has a value of 100%.
//{ "HealthBonus_Regular",				INI::parsePercentToReal, nullptr,	offsetof( GlobalData, m_healthBonus[LEVEL_REGULAR]) },
	{ "HealthBonus_Veteran",				INI::parsePercentToReal, nullptr,	offsetof( GlobalData, m_healthBonus[LEVEL_VETERAN]) },
	{ "HealthBonus_Elite",					INI::parsePercentToReal, nullptr,	offsetof( GlobalData, m_healthBonus[LEVEL_ELITE]) },
	{ "HealthBonus_Heroic",					INI::parsePercentToReal, nullptr,	offsetof( GlobalData, m_healthBonus[LEVEL_HEROIC]) },

	{ "HumanSoloPlayerHealthBonus_Easy",					INI::parsePercentToReal,			nullptr,			offsetof( GlobalData, m_soloPlayerHealthBonusForDifficulty[PLAYER_HUMAN][DIFFICULTY_EASY] ) },
	{ "HumanSoloPlayerHealthBonus_Normal",				INI::parsePercentToReal,			nullptr,			offsetof( GlobalData, m_soloPlayerHealthBonusForDifficulty[PLAYER_HUMAN][DIFFICULTY_NORMAL] ) },
	{ "HumanSoloPlayerHealthBonus_Hard",				INI::parsePercentToReal,			nullptr,			offsetof( GlobalData, m_soloPlayerHealthBonusForDifficulty[PLAYER_HUMAN][DIFFICULTY_HARD] ) },

	{ "AISoloPlayerHealthBonus_Easy",					INI::parsePercentToReal,			nullptr,			offsetof( GlobalData, m_soloPlayerHealthBonusForDifficulty[PLAYER_COMPUTER][DIFFICULTY_EASY] ) },
	{ "AISoloPlayerHealthBonus_Normal",				INI::parsePercentToReal,			nullptr,			offsetof( GlobalData, m_soloPlayerHealthBonusForDifficulty[PLAYER_COMPUTER][DIFFICULTY_NORMAL] ) },
	{ "AISoloPlayerHealthBonus_Hard",				INI::parsePercentToReal,			nullptr,			offsetof( GlobalData, m_soloPlayerHealthBonusForDifficulty[PLAYER_COMPUTER][DIFFICULTY_HARD] ) },

	{ "WeaponBonus",								WeaponBonusSet::parseWeaponBonusSetPtr,	nullptr,	offsetof( GlobalData, m_weaponBonusSet ) },

	{ "DefaultStructureRubbleHeight",	INI::parseReal,			nullptr,			offsetof( GlobalData, m_defaultStructureRubbleHeight ) },

	{ "FixedSeed",									INI::parseInt,				nullptr,			offsetof( GlobalData, m_fixedSeed ) },

	{ "ShellMapName",								INI::parseAsciiString,nullptr,			offsetof( GlobalData, m_shellMapName ) },
	{ "ShellMapOn",									INI::parseBool,				nullptr,			offsetof( GlobalData, m_shellMapOn ) },
	{	"PlayIntro",									INI::parseBool,				nullptr,			offsetof( GlobalData, m_playIntro ) },

	{ "FirewallBehavior",						INI::parseInt,				nullptr,			offsetof( GlobalData, m_firewallBehavior ) },
	{ "FirewallPortOverride",				INI::parseInt,				nullptr,			offsetof( GlobalData, m_firewallPortOverride ) },
	{	"FirewallPortAllocationDelta",INI::parseInt,				nullptr,			offsetof( GlobalData, m_firewallPortAllocationDelta) },

	{	"GroupSelectMinSelectSize",		INI::parseInt,				nullptr,			offsetof( GlobalData, m_groupSelectMinSelectSize ) },
	{	"GroupSelectVolumeBase",			INI::parseReal,				nullptr,			offsetof( GlobalData, m_groupSelectVolumeBase ) },
	{	"GroupSelectVolumeIncrement",	INI::parseReal,				nullptr,			offsetof( GlobalData, m_groupSelectVolumeIncrement ) },
	{	"MaxUnitSelectSounds",				INI::parseInt,				nullptr,			offsetof( GlobalData, m_maxUnitSelectSounds ) },

	{	"SelectionFlashSaturationFactor",	INI::parseReal,		nullptr,			offsetof( GlobalData, m_selectionFlashSaturationFactor ) },
	{	"SelectionFlashHouseColor",	      INI::parseBool,		nullptr,			offsetof( GlobalData, m_selectionFlashHouseColor ) },

	{	"CameraAudibleRadius",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_cameraAudibleRadius ) },
	{ "GroupMoveClickToGatherAreaFactor", INI::parseReal,	nullptr,			offsetof( GlobalData, m_groupMoveClickToGatherFactor ) },

#if ALLOW_MONEY_PER_MINUTE_FOR_PLAYER
	{ "AllowMoneyPerMinuteForPlayer",	INI::parseBool,			nullptr,			offsetof( GlobalData, m_allowMoneyPerMinuteForPlayer ) },
#endif

	{ "ShakeSubtleIntensity",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_shakeSubtleIntensity ) },
	{ "ShakeNormalIntensity",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_shakeNormalIntensity ) },
	{ "ShakeStrongIntensity",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_shakeStrongIntensity ) },
	{ "ShakeSevereIntensity",				INI::parseReal,				nullptr,			offsetof( GlobalData, m_shakeSevereIntensity ) },
	{ "ShakeCineExtremeIntensity",	INI::parseReal,				nullptr,			offsetof( GlobalData, m_shakeCineExtremeIntensity ) },
	{ "ShakeCineInsaneIntensity",		INI::parseReal,				nullptr,			offsetof( GlobalData, m_shakeCineInsaneIntensity ) },
	{ "MaxShakeIntensity",					INI::parseReal,				nullptr,			offsetof( GlobalData, m_maxShakeIntensity ) },
	{ "MaxShakeRange",							INI::parseReal,				nullptr,			offsetof( GlobalData, m_maxShakeRange) },
	{ "SellPercentage",							INI::parsePercentToReal,	nullptr,			offsetof( GlobalData, m_sellPercentage ) },
	{ "BaseRegenHealthPercentPerSecond", INI::parsePercentToReal, nullptr,	offsetof( GlobalData, m_baseRegenHealthPercentPerSecond ) },
	{ "BaseRegenDelay",							INI::parseDurationUnsignedInt, nullptr,offsetof( GlobalData, m_baseRegenDelay ) },

#ifdef ALLOW_SURRENDER
	{ "PrisonBountyMultiplier",			INI::parseReal,				nullptr,			offsetof( GlobalData, m_prisonBountyMultiplier ) },
	{ "PrisonBountyTextColor",			INI::parseColorInt,		nullptr,			offsetof( GlobalData, m_prisonBountyTextColor ) },
#endif

	{ "SpecialPowerViewObject",			INI::parseAsciiString,	nullptr,			offsetof( GlobalData, m_specialPowerViewObjectName ) },

	// TheSuperHackers @feature Customize the opacity (0..1) and shadows of build preview objects. Shadows are enabled by default.
	// Note that disabling shadows loses a fair bit of contrast visually and warrants raising the opacity.
	{ "ObjectPlacementOpacity", INI::parseReal, nullptr, offsetof( GlobalData, m_objectPlacementOpacity ) },
	{ "ObjectPlacementShadows", INI::parseBool, nullptr, offsetof( GlobalData, m_objectPlacementShadows ) },

	{ "StandardPublicBone", INI::parseAsciiStringVectorAppend, nullptr, offsetof(GlobalData, m_standardPublicBones) },
	{ "ShowMetrics",								INI::parseBool,				   nullptr,		offsetof( GlobalData, m_showMetrics ) },
  { "DefaultStartingCash",				Money::parseMoneyAmount, nullptr,		offsetof( GlobalData, m_defaultStartingCash ) },

// NOTE: m_doubleClickTimeMS is still in use, but we disallow setting it from the GameData.ini file. It is now set in the constructor according to the windows parameter.
//	{ "DoubleClickTimeMS",									INI::parseUnsignedInt,			nullptr, offsetof( GlobalData, m_doubleClickTimeMS ) },

	{ "ShroudColor",		INI::parseRGBColor,						nullptr,	offsetof( GlobalData, m_shroudColor) },
	{ "ClearAlpha",			INI::parseUnsignedByte,				nullptr,	offsetof( GlobalData, m_clearAlpha) },
	{ "FogAlpha",				INI::parseUnsignedByte,				nullptr,	offsetof( GlobalData, m_fogAlpha) },
	{ "ShroudAlpha",		INI::parseUnsignedByte,				nullptr,	offsetof( GlobalData, m_shroudAlpha) },

	{ "HotKeyTextColor",										INI::parseColorInt,					nullptr,	offsetof( GlobalData, m_hotKeyTextColor ) },

	{ "PowerBarBase",												INI::parseInt,							nullptr,	offsetof( GlobalData, m_powerBarBase) },
	{ "PowerBarIntervals",									INI::parseReal,							nullptr,	offsetof( GlobalData, m_powerBarIntervals) },
	{ "PowerBarYellowRange",								INI::parseInt,							nullptr,	offsetof( GlobalData, m_powerBarYellowRange) },
	{ "UnlookPersistDuration",							INI::parseDurationUnsignedInt, nullptr, offsetof( GlobalData, m_unlookPersistDuration) },

	{ "NetworkFPSHistoryLength", INI::parseInt, nullptr, offsetof(GlobalData, m_networkFPSHistoryLength) },
	{ "NetworkLatencyHistoryLength", INI::parseInt, nullptr, offsetof(GlobalData, m_networkLatencyHistoryLength) },
	{ "NetworkRunAheadMetricsTime", INI::parseInt, nullptr, offsetof(GlobalData, m_networkRunAheadMetricsTime) },
	{ "NetworkCushionHistoryLength", INI::parseInt, nullptr, offsetof(GlobalData, m_networkCushionHistoryLength) },
	{ "NetworkRunAheadSlack", INI::parseInt, nullptr, offsetof(GlobalData, m_networkRunAheadSlack) },
	{ "NetworkKeepAliveDelay", INI::parseInt, nullptr, offsetof(GlobalData, m_networkKeepAliveDelay) },
	{ "NetworkDisconnectTime", INI::parseInt, nullptr, offsetof(GlobalData, m_networkDisconnectTime) },
	{ "NetworkPlayerTimeoutTime", INI::parseInt, nullptr, offsetof(GlobalData, m_networkPlayerTimeoutTime) },
	{ "NetworkDisconnectScreenNotifyTime", INI::parseInt, nullptr, offsetof(GlobalData, m_networkDisconnectScreenNotifyTime) },

	{ "PlayStats",									INI::parseInt,				nullptr,			offsetof( GlobalData, m_playStats ) },

#if defined(RTS_DEBUG)
	{ "DisableCameraFade",			INI::parseBool,				nullptr,			offsetof( GlobalData, m_disableCameraFade ) },
	{ "DisableScriptedInputDisabling",			INI::parseBool,		nullptr,			offsetof( GlobalData, m_disableScriptedInputDisabling ) },
	{ "DisableMilitaryCaption",			INI::parseBool,				nullptr,			offsetof( GlobalData, m_disableMilitaryCaption ) },
	{ "BenchmarkTimer",			INI::parseInt,				nullptr,			offsetof( GlobalData, m_benchmarkTimer ) },
	{ "CheckMemoryLeaks", INI::parseBool, nullptr, offsetof(GlobalData, m_checkForLeaks) },
	{ "Wireframe",								INI::parseBool,				nullptr,			offsetof( GlobalData, m_wireframe ) },
	{ "StateMachineDebug",				INI::parseBool,				nullptr,			offsetof( GlobalData, m_stateMachineDebug ) },
	{ "UseCameraConstraints",				INI::parseBool,				nullptr,			offsetof( GlobalData, m_useCameraConstraints ) },
	{ "ShroudOn",										INI::parseBool,				nullptr,			offsetof( GlobalData, m_shroudOn ) },
	{ "FogOfWarOn",										INI::parseBool,				nullptr,			offsetof( GlobalData, m_fogOfWarOn ) },
	{ "ShowCollisionExtents",				INI::parseBool,				nullptr,			offsetof( GlobalData, m_showCollisionExtents ) },
  { "ShowAudioLocations",  				INI::parseBool,				nullptr,			offsetof( GlobalData, m_showAudioLocations ) },
	{ "DebugProjectileTileWidth",		INI::parseReal,				nullptr,			offsetof( GlobalData, m_debugProjectileTileWidth) },
	{ "DebugProjectileTileDuration",INI::parseInt,				nullptr,			offsetof( GlobalData, m_debugProjectileTileDuration) },
	{ "DebugProjectileTileColor",		INI::parseRGBColor,		nullptr,			offsetof( GlobalData, m_debugProjectileTileColor) },
	{ "DebugVisibilityTileCount",		INI::parseInt,				nullptr,			offsetof( GlobalData, m_debugVisibilityTileCount) },
	{ "DebugVisibilityTileWidth",		INI::parseReal,				nullptr,			offsetof( GlobalData, m_debugVisibilityTileWidth) },
	{ "DebugVisibilityTileDuration",INI::parseInt,				nullptr,			offsetof( GlobalData, m_debugVisibilityTileDuration) },
	{ "DebugVisibilityTileTargettableColor",INI::parseRGBColor, nullptr,	offsetof( GlobalData, m_debugVisibilityTargettableColor) },
	{ "DebugVisibilityTileDeshroudColor",		INI::parseRGBColor,	nullptr,	offsetof( GlobalData, m_debugVisibilityDeshroudColor) },
	{ "DebugVisibilityTileGapColor",				INI::parseRGBColor,	nullptr,	offsetof( GlobalData, m_debugVisibilityGapColor) },
	{ "DebugThreatMapTileDuration",					INI::parseInt,							nullptr,	offsetof( GlobalData, m_debugThreatMapTileDuration) },
	{ "MaxDebugThreatMapValue",							INI::parseUnsignedInt,			nullptr,	offsetof( GlobalData, m_maxDebugThreat) },
	{ "DebugCashValueMapTileDuration",			INI::parseInt,							nullptr,	offsetof( GlobalData, m_debugCashValueMapTileDuration) },
	{ "MaxDebugCashValueMapValue",					INI::parseUnsignedInt,			nullptr,	offsetof( GlobalData, m_maxDebugValue) },
	{ "VTune", INI::parseBool,	nullptr,			offsetof( GlobalData, m_vTune ) },
	{ "SaveStats",									INI::parseBool,				nullptr,			offsetof( GlobalData, m_saveStats ) },
	{ "UseLocalMOTD",								INI::parseBool,				nullptr,			offsetof( GlobalData, m_useLocalMOTD ) },
	{ "BaseStatsDir",								INI::parseAsciiString,nullptr,			offsetof( GlobalData, m_baseStatsDir ) },
	{ "LocalMOTDPath",							INI::parseAsciiString,nullptr,			offsetof( GlobalData, m_MOTDPath ) },
	{ "ExtraLogging",								INI::parseBool,				nullptr,			offsetof( GlobalData, m_extraLogging ) },
#endif

	{ nullptr,					nullptr,						nullptr,						0 }

};


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GlobalData::GlobalData()
{
	Int i, j;

	//
	// we have now instanced a global data instance, if theOriginal is null, this is
	// *the* very first instance and it shall be recorded.  This way, when we load
	// overrides of the global data, we can revert to the common, original data
	// in m_theOriginal
	//
	if( m_theOriginal == nullptr )
		m_theOriginal = this;
	m_next = nullptr;

  m_TiVOFastMode = FALSE;

#if defined(RTS_DEBUG) || ENABLE_CONFIGURABLE_SHROUD
	m_shroudOn = TRUE;
#endif

#if defined(RTS_DEBUG)
	m_wireframe = 0;
	m_stateMachineDebug = FALSE;
	m_useCameraConstraints = TRUE;
	m_fogOfWarOn = FALSE;
	m_jabberOn = FALSE;
	m_munkeeOn = FALSE;
	m_showCollisionExtents = FALSE;
  m_showAudioLocations = FALSE;
	m_debugCamera = FALSE;
	m_specialPowerUsesDelay = TRUE;
	m_debugVisibility = FALSE;
	m_debugVisibilityTileCount = 32;	// default to 32.
	m_debugVisibilityTileDuration = LOGICFRAMES_PER_SECOND;
	m_debugProjectilePath = FALSE;
	m_debugProjectileTileWidth = 10;
	m_debugProjectileTileDuration = LOGICFRAMES_PER_SECOND;
	m_debugThreatMap = FALSE;
	m_maxDebugThreat = 5000;
	m_debugThreatMapTileDuration = LOGICFRAMES_PER_SECOND;
	m_debugCashValueMap = FALSE;
	m_maxDebugValue = 10000;
	m_debugCashValueMapTileDuration = LOGICFRAMES_PER_SECOND;
	m_vTune = false;
	m_checkForLeaks = TRUE;
	m_benchmarkTimer = -1;
	m_allowUnselectableSelection = FALSE;
	m_disableCameraFade = false;
	m_disableScriptedInputDisabling = false;
	m_disableMilitaryCaption = false;
	m_latencyAverage = 0;
	m_latencyAmplitude = 0;
	m_latencyPeriod = 0;
	m_latencyNoise = 0;
	m_packetLoss = 0;
	m_saveStats = FALSE;
	m_saveAllStats = FALSE;
	m_useLocalMOTD = FALSE;
	m_baseStatsDir = ".\\";
	m_MOTDPath = "MOTD.txt";
	m_extraLogging = FALSE;
#endif

#ifdef DEBUG_CRASHING
	m_debugIgnoreAsserts = FALSE;
#endif

#ifdef DEBUG_STACKTRACE
	m_debugIgnoreStackTrace = FALSE;
#endif

	m_playStats = -1;
	m_incrementalAGPBuf = FALSE;
	m_mapName.clear();
	m_moveHintName.clear();
	m_useTrees = 0;
	m_useTreeSway = TRUE;
	m_useDrawModuleLOD = FALSE;
	m_useHeatEffects = TRUE;
	// GeneralsX @tweak felipebraz 20/06/2026 Default render FPS limit to 60 FPS instead of uncapped/0.
	m_useFpsLimit = TRUE;
	m_dumpAssetUsage = FALSE;
	m_framesPerSecondLimit = 60;
	m_chipSetType = 0;
	m_headless = FALSE;
	// GeneralsX @feature BenderAI 21/04/2026 Default to TRUE; user can override via Options.ini
	m_checkForUpdates = TRUE;
	m_windowed = 0;
	m_xResolution = DEFAULT_DISPLAY_WIDTH;
	m_yResolution = DEFAULT_DISPLAY_HEIGHT;
	m_maxShellScreens = 0;
	m_useCloudMap = FALSE;
	m_use3WayTerrainBlends = 1;
	m_useLightMap = FALSE;
	m_bilinearTerrainTex = FALSE;
	m_trilinearTerrainTex = FALSE;
	m_multiPassTerrain = FALSE;
	m_adjustCliffTextures = FALSE;
	m_stretchTerrain = FALSE;
	m_useHalfHeightMap = FALSE;
	m_terrainLOD = TERRAIN_LOD_AUTOMATIC;
	m_terrainLODTargetTimeMS = 0;
	m_enableDynamicLOD = TRUE;
	m_enableStaticLOD = TRUE;
	m_rightMouseAlwaysScrolls = FALSE;
	m_useWaterPlane = FALSE;
	m_useCloudPlane = FALSE;
	m_downwindAngle = ( -0.785f );//Northeast!
	m_useShadowVolumes = FALSE;
	m_useShadowDecals = FALSE;
	m_textureReductionFactor = -1;
	m_enableBehindBuildingMarkers = TRUE;
	m_scriptDebug = FALSE;
	m_particleEdit = FALSE;
	m_displayDebug = FALSE;
	m_winCursors = TRUE;
	m_constantDebugUpdate = FALSE;
	m_showTeamDot = FALSE;
	m_fixedSeed = -1; // disabled
	m_horizontalScrollSpeedFactor = 1.0;
	m_verticalScrollSpeedFactor = 1.0;

	m_waterPositionX = 0.0f;
	m_waterPositionY = 0.0f;
	m_waterPositionZ = 0.0f;
	m_waterExtentX = 0.0f;
	m_waterExtentY = 0.0f;
	m_waterType = 0;
	m_featherWater = FALSE;
	m_showSoftWaterEdge = TRUE;	//display soft water edge
	m_usingWaterTrackEditor = FALSE;
	m_isWorldBuilder = FALSE;

	m_showMetrics = false;

	for( i = 0; i < MAX_WATER_GRID_SETTINGS; i++ )
	{

		m_vertexWaterHeightClampLow[ i ] = 0.0f;
		m_vertexWaterHeightClampHi[ i ] = 0.0f;
		m_vertexWaterAngle[ i ] = 0.0f;
		m_vertexWaterXPosition[ i ] = 0.0f;
		m_vertexWaterYPosition[ i ] = 0.0f;
		m_vertexWaterZPosition[ i ] = 0.0f;
		m_vertexWaterXGridCells[ i ] = 0;
		m_vertexWaterYGridCells[ i ] = 0;
		m_vertexWaterGridSize[ i ] = 0.0f;
		m_vertexWaterAttenuationA[ i ] = 0.0f;
		m_vertexWaterAttenuationB[ i ] = 0.0f;
		m_vertexWaterAttenuationC[ i ] = 0.0f;
		m_vertexWaterAttenuationRange[ i ] = 0.0f;
		m_vertexWaterAvailableMaps[i].clear();
	}

	m_skyBoxPositionZ = 0.0f;
	m_drawSkyBox = FALSE;
	m_skyBoxScale = 4.5f;

	m_historicDamageLimit = 0;
	m_maxTerrainTracks = 0;

	m_levelGainAnimationDisplayTimeInSeconds = 0.0f;
	m_levelGainAnimationZRisePerSecond = 0.0f;

	m_getHealedAnimationDisplayTimeInSeconds = 0.0f;
	m_getHealedAnimationZRisePerSecond = 0.0f;

	m_maxTankTrackEdges=100;
	m_maxTankTrackOpaqueEdges=25;
	m_maxTankTrackFadeDelay=300000;

	m_timeOfDay = TIME_OF_DAY_AFTERNOON;
	m_weather = WEATHER_NORMAL;
	m_makeTrackMarks = FALSE;
	m_hideGarrisonFlags = FALSE;
	m_forceModelsToFollowTimeOfDay = true;
	m_forceModelsToFollowWeather = true;

	m_partitionCellSize = 0.0f;
	m_ammoPipScaleFactor = 1.0f;
	m_containerPipScaleFactor = 1.0f;
	m_ammoPipWorldOffset.zero();
	m_containerPipWorldOffset.zero();
	m_ammoPipScreenOffset.x = m_ammoPipScreenOffset.y = 0;
	m_containerPipScreenOffset.x = m_containerPipScreenOffset.y = 0;

	for (i=0; i<MAX_GLOBAL_LIGHTS; i++)
	{
		m_terrainAmbient[i].red = 0.0f;
		m_terrainAmbient[i].green = 0.0f;
		m_terrainAmbient[i].blue = 0.0f;
		m_terrainDiffuse[i].red = 0.0f;
		m_terrainDiffuse[i].green = 0.0f;
		m_terrainDiffuse[i].blue = 0.0f;
		m_terrainLightPos[i].x = 0.0f;
		m_terrainLightPos[i].y = 0.0f;
		m_terrainLightPos[i].z = -1.0f;

		for (j=0; j<TIME_OF_DAY_COUNT; j++)
		{	m_terrainLighting[ j ][i].ambient.red=0;
			m_terrainLighting[ j ][i].ambient.green=0;
			m_terrainLighting[ j ][i].ambient.blue=0;
			m_terrainLighting[ j ][i].diffuse.red=0;
			m_terrainLighting[ j ][i].diffuse.green=0;
			m_terrainLighting[ j ][i].diffuse.blue=0;
			m_terrainLighting[ j ][i].lightPos.x=0;
			m_terrainLighting[ j ][i].lightPos.y=0;
			m_terrainLighting[ j ][i].lightPos.z=-1.0f;

			m_terrainObjectsLighting[ j ][i].ambient.red=0;
			m_terrainObjectsLighting[ j ][i].ambient.green=0;
			m_terrainObjectsLighting[ j ][i].ambient.blue=0;
			m_terrainObjectsLighting[ j ][i].diffuse.red=0;
			m_terrainObjectsLighting[ j ][i].diffuse.green=0;
			m_terrainObjectsLighting[ j ][i].diffuse.blue=0;
			m_terrainObjectsLighting[ j ][i].lightPos.x=0;
			m_terrainObjectsLighting[ j ][i].lightPos.y=0;
			m_terrainObjectsLighting[ j ][i].lightPos.z=-1.0f;
		}
	}

	for (j=TIME_OF_DAY_FIRST; j<TIME_OF_DAY_COUNT; j++)
		m_infantryLightScale[j] = 1.5f;

	m_scriptOverrideInfantryLightScale = -1.0f;

	m_numGlobalLights = 3;
	m_maxRoadSegments = 0;
	m_maxRoadVertex = 0;
	m_maxRoadIndex = 0;
	m_maxRoadTypes = 0;

	m_baseValuePerSupplyBox = 100;

	m_audioOn = TRUE;
	m_musicOn = TRUE;
	m_soundsOn = TRUE;
	m_sounds3DOn = TRUE;
	m_speechOn = TRUE;
	m_videoOn = TRUE;
	m_disableCameraMovement = FALSE;
	m_maxVisibleTranslucentObjects = 512;
	m_maxVisibleOccluderObjects = 512;
	m_maxVisibleOccludeeObjects = 512;
	m_maxVisibleNonOccluderOrOccludeeObjects = 512;
	m_occludedLuminanceScale = 0.5f;

	m_useFX = TRUE;

//	m_inGame = FALSE;

	m_noDraw = 0;
	m_particleScale = 1.0f;

	m_autoFireParticleSmallMax = 0;
	m_autoFireParticleMediumMax = 0;
	m_autoFireParticleLargeMax = 0;
	m_autoSmokeParticleSmallMax = 0;
	m_autoSmokeParticleMediumMax = 0;
	m_autoSmokeParticleLargeMax = 0;
	m_autoAflameParticleMax = 0;

	m_autoFireParticleSmallPrefix.clear();
	m_autoFireParticleMediumPrefix.clear();
	m_autoFireParticleLargePrefix.clear();
	m_autoSmokeParticleSmallPrefix.clear();
	m_autoSmokeParticleMediumPrefix.clear();
	m_autoSmokeParticleLargePrefix.clear();
	m_autoAflameParticlePrefix.clear();

	m_autoFireParticleSmallSystem.clear();
	m_autoFireParticleMediumSystem.clear();
	m_autoFireParticleLargeSystem.clear();
	m_autoSmokeParticleSmallSystem.clear();
	m_autoSmokeParticleMediumSystem.clear();
	m_autoSmokeParticleLargeSystem.clear();
	m_autoAflameParticleSystem.clear();
	m_levelGainAnimationName.clear();
	m_getHealedAnimationName.clear();
	m_specialPowerViewObjectName.clear();

	m_drawEntireTerrain = FALSE;
	m_maxParticleCount = 0;
	m_maxFieldParticleCount = 30;

	m_debugAI = AI_DEBUG_NONE;
	m_debugSupplyCenterPlacement = FALSE;
	m_debugAIObstacles = FALSE;
	m_showClientPhysics = TRUE;
	m_showTerrainNormals = FALSE;
	m_showObjectHealth = FALSE;

	m_particleEdit = FALSE;

	m_viewportHeightScale = 0.80f; // Default value for the original Control Bar.

	m_cameraPitch = 0.0f;
	m_cameraYaw = 0.0f;
#if PRESERVE_RETAIL_SCRIPTED_CAMERA
	m_cameraHeight = 0.0f;
#endif
	m_minCameraHeight = 100.0f;
	m_maxCameraHeight = 300.0f;
	m_terrainDrawDistanceScale = 1.0f;
	m_terrainHeightAtEdgeOfMap = 0.0f;

	m_unitDamagedThresh = 0.5f;
	m_unitReallyDamagedThresh = 0.1f;
	m_groundStiffness = 0.5f;
	m_structureStiffness = 0.5f;
	m_gravity = -1.0f;
	m_stealthFriendlyOpacity = 0.5f;
	m_defaultOcclusionDelay = LOGICFRAMES_PER_SECOND * 3;	//default to 3 seconds

	m_preloadAssets = FALSE;
	m_preloadEverything = FALSE;
	m_preloadReport = FALSE;

	m_netMinPlayers = 1; // allowing sandbox mode

	m_defaultIP = 0;

	m_BuildSpeed = 0.0f;
	m_MinDistFromEdgeOfMapForBuild = 0.0f;
	m_SupplyBuildBorder = 0.0f;
	m_allowedHeightVariationForBuilding = 0.0f;
	m_MinLowEnergyProductionSpeed = 0.0f;
	m_MaxLowEnergyProductionSpeed = 0.0f;
	m_LowEnergyPenaltyModifier = 0.0f;
	m_MultipleFactory = 0.0f;
	m_RefundPercent = 0.0f;

	m_commandCenterHealRange = 0.0f;
	m_commandCenterHealAmount = 0.0f;
	m_maxTunnelCapacity = 0;
	m_maxLineBuildObjects = 0;

	m_standardMinefieldDensity = 0.01f;
	m_standardMinefieldDistance = 40.0f;

	m_objectPlacementOpacity = 0.45f;
	m_objectPlacementShadows = TRUE;

	m_groupSelectMinSelectSize = 5;
	m_groupSelectVolumeBase = 0.5f;
	m_groupSelectVolumeIncrement = 0.02f;
	m_maxUnitSelectSounds = 8;

	m_selectionFlashSaturationFactor = 0.5f; /// how colorful should the selection flash be? 0-4
	m_selectionFlashHouseColor = FALSE;  /// skip the house color and just use white.

	m_cameraAudibleRadius = 500.0;
	m_groupMoveClickToGatherFactor = 1.0f;

	m_shakeSubtleIntensity = 0.5f;
	m_shakeNormalIntensity = 1.0f;
	m_shakeStrongIntensity = 2.5f;
	m_shakeSevereIntensity = 5.0f;
	m_shakeCineExtremeIntensity = 8.0f;
	m_shakeCineInsaneIntensity = 12.0f;
	m_maxShakeIntensity = 10.0f;
	m_maxShakeRange = 150.f;

	m_sellPercentage = 1.0f;
	m_baseRegenHealthPercentPerSecond = 0.0f;
	m_baseRegenDelay = 0;

#ifdef ALLOW_SURRENDER
	m_prisonBountyMultiplier = 1.0f;
	m_prisonBountyTextColor = GameMakeColor( 255, 255, 255, 255 );
#endif

	m_hotKeyTextColor = GameMakeColor(255,255,0,255);

	m_shroudColor.setFromInt( 0x00FFFFFF ) ;
	m_clearAlpha = 255;
	m_fogAlpha = 127;
	m_shroudAlpha = 0;

	m_powerBarBase = 7;
	m_powerBarIntervals = 3;
	m_powerBarYellowRange = 5;
	m_displayGamma = 1.0f;	//ramp that does nothing

	m_standardPublicBones.clear();

	m_antiAliasLevel = WW3D::MultiSampleModeEnum::MULTISAMPLE_MODE_NONE;
	m_textureFilteringMode = TextureFilterClass::TextureFilterMode::TEXTURE_FILTER_BILINEAR;
	m_textureAnisotropyLevel = TextureFilterClass::AnisotropicFilterMode::TEXTURE_FILTER_ANISOTROPIC_2X;

//	m_languageFilterPref = false;
	m_languageFilterPref = true;
	m_firewallBehavior = FirewallHelperClass::FIREWALL_TYPE_UNKNOWN;
	m_firewallSendDelay = FALSE;
	m_firewallPortOverride = 0;
	m_firewallPortAllocationDelta = 0;
	m_loadScreenDemo = FALSE;
	m_disableRender = false;

	m_saveCameraInReplay = FALSE;
	m_useCameraInReplay = FALSE;
	m_enablePlayerObserver = FALSE;

	m_networkLatencyFontSize = 8;
	m_renderFpsFontSize = 8;
	m_systemTimeFontSize = 8;
	m_gameTimeFontSize = 8;
	m_playerInfoListFontSize = 8;

	m_showMoneyPerMinute = FALSE;
	m_allowMoneyPerMinuteForPlayer = FALSE;

	m_gameWindowTransitionSpeedMultiplier = 1.0f;

	m_debugShowGraphicalFramerate = FALSE;

	// By default, show all asserts.

	m_unlookPersistDuration		= 30;

	//-----------------------------------------------------------------------------------------------

	// network timing values.  Having these default to 0 would be bad. - BGC
	m_networkFPSHistoryLength = 30;
	m_networkLatencyHistoryLength = 200;
	m_networkRunAheadMetricsTime = 500;
	m_networkCushionHistoryLength = 10;
	m_networkRunAheadSlack = 10;
	m_networkKeepAliveDelay = 20;
	m_networkDisconnectTime = 5000;
	m_networkPlayerTimeoutTime = 60000;
	m_networkDisconnectScreenNotifyTime = 15000;

	m_isBreakableMovie = FALSE;
	m_breakTheMovie = FALSE;

	setTimeOfDay( m_timeOfDay );

	m_buildMapCache = FALSE;
	m_initialFile.clear();
	m_pendingFile.clear();

	m_simulateReplays.clear();
	m_simulateReplayJobs = SIMULATE_REPLAYS_SEQUENTIAL;

	for (i = LEVEL_FIRST; i <= LEVEL_LAST; ++i)
		m_healthBonus[i] = 1.0f;

	for (i = 0; i < PLAYERTYPE_COUNT; ++i)
	{
		for (j = 0; j < DIFFICULTY_COUNT; ++j)
		{
			m_soloPlayerHealthBonusForDifficulty[i][j] = 1.0f;
		}
	}

	m_defaultStructureRubbleHeight = 1.0f;
	m_weaponBonusSet = newInstance(WeaponBonusSet);

	m_shellMapName.set("Maps\\ShellMap1\\ShellMap1.map");
	m_shellMapOn =TRUE;
	m_playIntro = TRUE;
	m_playSizzle = TRUE;
	m_afterIntro = FALSE;
	m_allowExitOutOfMovies = FALSE;
	m_loadScreenRender = FALSE;

	m_keyboardDefaultScrollFactor = m_keyboardScrollFactor = 0.5f;
	m_drawScrollAnchor = FALSE;
	m_moveScrollAnchor = FALSE;
	m_scrollAmountCutoff = 10.0f;
	m_cameraAdjustSpeed = 0.1f;
	m_enforceMaxCameraHeight = TRUE;

	m_animateWindows = TRUE;

	m_iniCRC = 0;
	m_exeCRC = 0;

	m_movementPenaltyDamageState = BODY_REALLYDAMAGED;

	m_shouldUpdateTGAToDDS = FALSE;

	// Default DoubleClickTime to System double click time.
	m_doubleClickTimeMS = GetDoubleClickTime(); // Note: This is actual MS, not frames.

#ifdef DUMP_PERF_STATS
	m_dumpPerformanceStatistics = FALSE;
  m_dumpStatsAtInterval = FALSE;
  m_statsInterval = 30;
#endif

	m_forceBenchmark = FALSE;	///<forces running of CPU detection benchmark, even on known cpu's.

	m_keyboardCameraRotateSpeed = 0.1f;

	m_useAlternateMouse = FALSE;
	// GeneralsX @bugfix Meeseeks 18/06/2026 Default to TRUE to allow right-click drag scrolling under alternate controls by default
	m_useRightMouseScrollWithAlternateMouse = TRUE;
	m_clientRetaliationModeEnabled = TRUE; //On by default.
	m_doubleClickAttackMove = FALSE;

}

//-------------------------------------------------------------------------------------------------
AsciiString GlobalData::getPath_UserData() const
{
	return m_userDataDir;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GlobalData::~GlobalData()
{
	DEBUG_ASSERTCRASH( TheWritableGlobalData->m_next == nullptr, ("~GlobalData: theOriginal is not original") );

	deleteInstance(m_weaponBonusSet);

	if( m_theOriginal == this )	{
		m_theOriginal = nullptr;
		TheWritableGlobalData = nullptr;
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool GlobalData::setTimeOfDay( TimeOfDay tod )
{
	if( tod >= TIME_OF_DAY_COUNT || tod < TIME_OF_DAY_FIRST )
	{
		return FALSE;
	}

	m_timeOfDay = tod;
	for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
	{	m_terrainAmbient[i] = m_terrainLighting[ tod ][i].ambient;
		m_terrainDiffuse[i] = m_terrainLighting[ tod ][i].diffuse;
		m_terrainLightPos[i] = m_terrainLighting[ tod ][i].lightPos;
	}

	return TRUE;

}

//-------------------------------------------------------------------------------------------------
/** Create a new global data instance to override the existing data set.  The
	* initial values of the newly created instance will be a copy of the current
	* data (or the most recently created override) */
//-------------------------------------------------------------------------------------------------
GlobalData *GlobalData::newOverride()
{
	// TheSuperHackers @info This copy is not implemented in VS6 builds
	GlobalData *overrideData = NEW GlobalData;

	// copy the data from the latest override (TheWritableGlobalData) to the newly created instance
	DEBUG_ASSERTCRASH( TheWritableGlobalData, ("GlobalData::newOverride() - no existing data") );
	*overrideData = *TheWritableGlobalData;

	//
	// link the override to the previously created one, the link order is important here
	// for the reset function, if you change the way things are linked
	// for overrides make sure you update the reset function
	//
	overrideData->m_next = TheWritableGlobalData;

	// set this new instance as the 'most current override' where we will access all data from
	TheWritableGlobalData = overrideData;

	return overrideData;

}

//-------------------------------------------------------------------------------------------------
void GlobalData::init()
{
	m_exeCRC = generateExeCRC();
}

//-------------------------------------------------------------------------------------------------
/** Reset, remove any override data instances and return to just the initial one
	*/
//-------------------------------------------------------------------------------------------------
void GlobalData::reset()
{
	DEBUG_ASSERTCRASH(this == TheWritableGlobalData, ("calling reset on wrong GlobalData"));

	//
	// delete	any data instances that were loaded as an override and set the original
	// global data instance as the singleton TheWritableGlobalData once again
	//
	while (TheWritableGlobalData != GlobalData::m_theOriginal)
	{

		// get next instance
		GlobalData* next = m_next;

		// delete the head of the global data list (the latest override)
		delete TheWritableGlobalData;

		// set next as top
		TheWritableGlobalData = next;

	}

	//
	// we now have the one single global data in TheWritableGlobalData singleton, lets sanity check
	// some of all that
	//
	DEBUG_ASSERTCRASH( TheWritableGlobalData->m_next == nullptr, ("ResetGlobalData: theOriginal is not original") );
	DEBUG_ASSERTCRASH( TheWritableGlobalData == GlobalData::m_theOriginal, ("ResetGlobalData: oops") );

}

//-------------------------------------------------------------------------------------------------
/** Parse GameData entry */
//-------------------------------------------------------------------------------------------------
void GlobalData::parseGameDataDefinition( INI* ini )
{
	if( TheWritableGlobalData && ini->getLoadType() != INI_LOAD_MULTIFILE)
	{

		//
		// if the type of loading we're doing creates override data, we need to
		// be loading into a new override item
		//
		if( ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES )
			TheWritableGlobalData->newOverride();

	}
	else if (!TheWritableGlobalData)
	{

		// we don't have any global data instance at all yet, create one
		TheWritableGlobalData = NEW GlobalData;

	}
	// If we're multifile, then continue loading stuff into the Global Data as normal.

	// parse the ini weapon definition
	ini->initFromINI( TheWritableGlobalData, s_GlobalDataFieldParseTable );

	TheWritableGlobalData->m_userDataDir.clear();

	// GeneralsX @feature Bender 01/04/2026 Cross-platform user data directory handling
	// Adopts upstream refactoring with extended cross-platform support
	TheWritableGlobalData->m_userDataDir = BuildUserDataPathFromIni();
	CreateDirectory(TheWritableGlobalData->m_userDataDir.str(), nullptr);

	// override INI values with user preferences
	OptionPreferences optionPref;
	TheWritableGlobalData->m_useAlternateMouse = optionPref.getAlternateMouseModeEnabled();
	TheWritableGlobalData->m_useRightMouseScrollWithAlternateMouse = optionPref.getRightMouseScrollWithAlternateMouseEnabled();
	TheWritableGlobalData->m_clientRetaliationModeEnabled = optionPref.getRetaliationModeEnabled();
	TheWritableGlobalData->m_doubleClickAttackMove = optionPref.getDoubleClickAttackMoveEnabled();
	TheWritableGlobalData->m_keyboardScrollFactor = optionPref.getScrollFactor();
	TheWritableGlobalData->m_drawScrollAnchor = optionPref.getDrawScrollAnchor();
	TheWritableGlobalData->m_moveScrollAnchor = optionPref.getMoveScrollAnchor();
	TheWritableGlobalData->m_defaultIP = optionPref.getLANIPAddress();
	TheWritableGlobalData->m_firewallSendDelay = optionPref.getSendDelay();
	TheWritableGlobalData->m_firewallBehavior = optionPref.getFirewallBehavior();
	TheWritableGlobalData->m_firewallPortAllocationDelta = optionPref.getFirewallPortAllocationDelta();
	TheWritableGlobalData->m_firewallPortOverride = optionPref.getFirewallPortOverride();

	TheWritableGlobalData->m_saveCameraInReplay = optionPref.saveCameraInReplays();
	TheWritableGlobalData->m_useCameraInReplay = optionPref.useCameraInReplays();
	TheWritableGlobalData->m_enablePlayerObserver = optionPref.getPlayerObserverEnabled();

	TheWritableGlobalData->m_networkLatencyFontSize = optionPref.getNetworkLatencyFontSize();
	TheWritableGlobalData->m_renderFpsFontSize = optionPref.getRenderFpsFontSize();
	TheWritableGlobalData->m_systemTimeFontSize = optionPref.getSystemTimeFontSize();
	TheWritableGlobalData->m_gameTimeFontSize = optionPref.getGameTimeFontSize();
	TheWritableGlobalData->m_playerInfoListFontSize = optionPref.getPlayerInfoListFontSize();
	TheWritableGlobalData->m_showMoneyPerMinute = optionPref.getShowMoneyPerMinute();
	TheWritableGlobalData->m_gameWindowTransitionSpeedMultiplier = optionPref.getGameWindowTransitionSpeedMultiplier();

	TheWritableGlobalData->m_antiAliasLevel = optionPref.getAntiAliasing();
	TheWritableGlobalData->m_textureFilteringMode = optionPref.getTextureFilterMode();
	TheWritableGlobalData->m_textureAnisotropyLevel = optionPref.getTextureAnisotropyLevel();

	Int val=optionPref.getGammaValue();
	//generate a value between 0.6 and 2.0.
	if (val < 50)
	{	//darker gamma
		if (val <= 0)
			TheWritableGlobalData->m_displayGamma = 0.6f;
		else
			TheWritableGlobalData->m_displayGamma=1.0f-(0.4f) * (Real)(50-val)/50.0f;
	}
	else
	if (val > 50)
		TheWritableGlobalData->m_displayGamma=1.0f+(1.0f) * (Real)(val-50)/50.0f;

	Int xres,yres;
	optionPref.getResolution(&xres, &yres);

	TheWritableGlobalData->m_xResolution = xres;
	TheWritableGlobalData->m_yResolution = yres;
}

void GlobalData::parseCustomDefinition()
{
	if (addon::HasFullviewportDat())
	{
		// TheSuperHackers @tweak xezon 03/08/2025 Force full viewport for 'Control Bar Pro' Addons like GenTool did it.
		m_viewportHeightScale = 1.0f;
	}
}

UnsignedInt GlobalData::generateExeCRC()
{
	DEBUG_ASSERTCRASH(TheFileSystem != nullptr, ("TheFileSystem is null"));

	// lets CRC the executable!  Whee!
	const Int blockSize = 65536;
	CRC exeCRC;
	File *fp;
	// TheSuperHackers @tweak SkyAero/xezon 27/05/2025
	// Simulate the EXE's CRC value to force Network and Replay compatibility with another build.
#if (defined(_MSC_VER) && _MSC_VER < 1300) && RETAIL_COMPATIBLE_CRC

#define GENERALS_108_CD_EXE_CRC    0x93d1eab4u
#define GENERALS_108_STEAM_EXE_CRC 0x8d6e4dd7u
#define GENERALS_108_EAAPP_EXE_CRC 0xb07fbd50u

	exeCRC.set(GENERALS_108_CD_EXE_CRC);
	DEBUG_LOG(("Fake EXE CRC is 0x%8.8X", exeCRC.get()));

#else
	{
		Char buffer[ _MAX_PATH ];
		GetModuleFileName( nullptr, buffer, sizeof( buffer ) );
		fp = TheFileSystem->openFile(buffer, File::READ | File::BINARY);
		if (fp != nullptr) {
			unsigned char crcBlock[blockSize];
			Int amtRead = 0;
			while ( (amtRead=fp->read(crcBlock, blockSize)) > 0 )
			{
				exeCRC.computeCRC(crcBlock, amtRead);
			}
			DEBUG_LOG(("EXE CRC is 0x%8.8X", exeCRC.get()));
			fp->close();
			fp = nullptr;
		}
		else {
			DEBUG_CRASH(("Executable file has failed to open"));
		}
	}
#endif

	UnsignedInt version = 0;
	if (TheVersion)
	{
		version = TheVersion->getVersionNumber();
		exeCRC.computeCRC( &version, sizeof(UnsignedInt) );
	}
	// GeneralsX @bugfix Copilot 22/03/2026 Load shared script CRC inputs relative to the configured asset root.
	// Add in MP scripts to the EXE CRC, since the game will go out of sync if they change
	fp = TheFileSystem->openFile("Data\\Scripts\\SkirmishScripts.scb", File::READ | File::BINARY);
	if (fp != nullptr) {
		unsigned char crcBlock[blockSize];
		Int amtRead = 0;
		while ( (amtRead=fp->read(crcBlock, blockSize)) > 0 )
		{
			exeCRC.computeCRC(crcBlock, amtRead);
		}
		fp->close();
		fp = nullptr;
	}
	fp = TheFileSystem->openFile("Data\\Scripts\\MultiplayerScripts.scb", File::READ | File::BINARY);
	if (fp != nullptr) {
		unsigned char crcBlock[blockSize];
		Int amtRead = 0;
		while ( (amtRead=fp->read(crcBlock, blockSize)) > 0 )
		{
			exeCRC.computeCRC(crcBlock, amtRead);
		}
		fp->close();
		fp = nullptr;
	}

	DEBUG_LOG(("EXE+Version(%d.%d)+SCB CRC is 0x%8.8X", version >> 16, version & 0xffff, exeCRC.get()));

	return exeCRC.get();
}

AsciiString GlobalData::BuildUserDataPathFromIni()
{
	AsciiString userDataDir;

#ifdef _WIN32
	// GeneralsX @refactor Bender 01/04/2026 Windows-specific path handling
	// Integrates upstream bug-fix for OneDrive/Group Policy folder redirection
#if defined(_MSC_VER) && (_MSC_VER < 1300)
	// VC6 lacks FOLDERID_Documents and KF_FLAG_DEFAULT
	const GUID FOLDERID_Documents = { 0xFDD39AD0, 0x238F, 0x46AF, 0xAD, 0xB4, 0x6C, 0x85, 0x48, 0x03, 0x69, 0xC7 };
	const DWORD KF_FLAG_DEFAULT = 0;
#endif

	typedef HRESULT(WINAPI* PFN_SHGetKnownFolderPath)(const GUID& rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath);

	AsciiString myDocumentsDirectory;
	HMODULE shell32module = GetModuleHandleA("shell32.dll");
	PFN_SHGetKnownFolderPath pSHGetKnownFolderPath = nullptr;

	// TheSuperHackers @bugfix Mauller 20/03/2026 Fix the handling of folder redirection
	// OneDrive and Group Policy folder redirection is better supported by SHGetKnownFolderPath()
	// SHGetKnownFolderPath() is only supported in windows Vista onwards so we check for it being available
	if (shell32module) {
		pSHGetKnownFolderPath = (PFN_SHGetKnownFolderPath)GetProcAddress(shell32module, "SHGetKnownFolderPath");
	}

	if (pSHGetKnownFolderPath) {
		PWSTR pszPath = nullptr;
		HRESULT hr = pSHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &pszPath);

		if (SUCCEEDED(hr) && pszPath) {
			myDocumentsDirectory.translate(pszPath);
			CoTaskMemFree(pszPath);
		}
	}
	else {
		char temp[_MAX_PATH + 1];
		if (SHGetSpecialFolderPath(nullptr, temp, CSIDL_PERSONAL, true)) {
			myDocumentsDirectory = temp;
		}
	}

	if (!myDocumentsDirectory.isEmpty()) {
		// Now build the full path string
		if (!myDocumentsDirectory.endsWith("\\"))
			myDocumentsDirectory.concat('\\');

		myDocumentsDirectory.concat(TheWritableGlobalData->m_userDataLeafName.str());

		if (!myDocumentsDirectory.endsWith("\\"))
			myDocumentsDirectory.concat('\\');
	}

	userDataDir = myDocumentsDirectory;

#elif defined(__APPLE__)
	// GeneralsX @feature Bender 01/04/2026 macOS user data directory
	// Uses ~/Library/Application Support as standard macOS location
	{
		const char* home = getenv("HOME");
		if (home) {
			std::filesystem::path path = std::filesystem::path(home) / "Library" / "Application Support" / "GeneralsX" / "Generals" / "";
			std::filesystem::create_directories(path);
			userDataDir = path.string().c_str();
		} else {
			userDataDir = "./";
		}
	}

#else
	// GeneralsX @feature Bender 01/04/2026 Linux user data directory
	// Uses XDG Base Directory specification
	{
		std::filesystem::path path;
		const char* xdgDataHome = getenv("XDG_DATA_HOME");
		const char* home = getenv("HOME");
		
		if (xdgDataHome) {
			path = std::filesystem::path(xdgDataHome);
		} else if (home) {
			path = std::filesystem::path(home) / ".local" / "share";
		} else {
			path = ".";
		}

		path = path / "GeneralsX" / "Generals" / "";
		std::filesystem::create_directories(path);
		userDataDir = path.string().c_str();
	}
#endif

	return userDataDir;
}
