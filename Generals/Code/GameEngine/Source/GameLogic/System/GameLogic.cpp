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

// FILE: GameLogic.cpp ////////////////////////////////////////////////////////////////////////////
// GameLogic class implementation
// Author: Michael S. Booth, October 2000
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#ifndef _WIN32
#include <fenv.h>
#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif
#endif

#include "Common/AudioAffect.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/BuildAssistant.h"
#include "Common/CRCDebug.h"
#include "Common/FramePacer.h"
#include "Common/GameAudio.h"
#include "Common/GameEngine.h"
#include "Common/GameState.h"
#include "Common/GameUtility.h"
#include "Common/INI.h"
#include "Common/LatchRestore.h"
#include "Common/MapObject.h"
#include "Common/MultiplayerSettings.h"
#include "Common/OSDisplay.h"
#include "Common/PerfTimer.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Radar.h"
#include "Common/RandomValue.h"
#include "Common/Recorder.h"
#include "Common/StatsCollector.h"
#include "Common/ThingFactory.h"
#include "Common/Team.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Water.h"
#include "Common/WellKnownKeys.h"
#include "Common/Xfer.h"
#include "Common/XferCRC.h"
#include "Common/XferDeepCRC.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameText.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/InGameUI.h"
#include "GameClient/LoadScreen.h"
#include "GameClient/MapUtil.h"
#include "GameClient/Mouse.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/View.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/GameWindowTransitions.h"

#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/CaveSystem.h"
#include "GameLogic/CrateSystem.h"
#include "GameLogic/FPUControl.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/DestroyModule.h"
#include "GameLogic/Module/OpenContain.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/VictoryConditions.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/GhostObject.h"

#include "Common/DataChunk.h"
#include "GameLogic/Scripts.h"

#include "GameNetwork/GameSpy/BuddyThread.h"
#include "GameNetwork/GameSpy/PeerDefs.h"
#include "GameNetwork/GameSpy/ThreadUtils.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "GameNetwork/NetworkInterface.h"

struct QuitGameException {};

DECLARE_PERF_TIMER(SleepyMaintenance)

#include "Common/UnitTimings.h" //Contains the DO_UNIT_TIMINGS define jba.
// If defined, the game times various units.
#ifdef DO_UNIT_TIMINGS
#pragma MESSAGE("*** WARNING *** DOING DO_UNIT_TIMINGS!!!!")
Bool g_UT_gotUnit = false;
const ThingTemplate *g_UT_curThing = nullptr;
Bool g_UT_startTiming = false;
FILE *g_UT_timingLog=nullptr;
FILE *g_UT_commaLog=nullptr;
// Note - this is only for gathering timing data!  DO NOT DO THIS IN REGULAR CODE!!!  JBA
#define BRUTAL_TIMING_HACK
#include "../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"
#endif



// I'm making this larger now that we know how big our maps are going to be.
enum { OBJ_HASH_SIZE	= 8192 };

/// The GameLogic singleton instance
GameLogic *TheGameLogic = nullptr;

static void findAndSelectCommandCenter(Object *obj, void* alreadyFound);


// ------------------------------------------------------------------------------------------------
/** This enum is for loading screen bar progress */
// ------------------------------------------------------------------------------------------------
enum
{
	LOAD_PROGRESS_START =0,
	LOAD_PROGRESS_POST_PARTICLE_INI_LOAD = LOAD_PROGRESS_START + 1,
	LOAD_PROGRESS_POST_LOAD_MAP = LOAD_PROGRESS_POST_PARTICLE_INI_LOAD + 1,
	LOAD_PROGRESS_SIDE_POPULATION = LOAD_PROGRESS_POST_LOAD_MAP + 1,  // Increment the next one by at least MAX_SLOTS
	LOAD_PROGRESS_POST_SIDE_LIST_INIT = LOAD_PROGRESS_SIDE_POPULATION + 1 + MAX_SLOTS,
	LOAD_PROGRESS_POST_PLAYER_LIST_RESET = LOAD_PROGRESS_POST_SIDE_LIST_INIT + 1,
	LOAD_PROGRESS_POST_SCRIPT_ENGINE_NEW_MAP = LOAD_PROGRESS_POST_PLAYER_LIST_RESET + 1,
	LOAD_PROGRESS_POST_VICTORY_CONDITION_SETUP = LOAD_PROGRESS_POST_SCRIPT_ENGINE_NEW_MAP + 2,
	LOAD_PROGRESS_POST_VICTORY_CONDITION_SET_VICTORY_CONDITION = LOAD_PROGRESS_POST_VICTORY_CONDITION_SETUP + 1,
	LOAD_PROGRESS_POST_GHOST_OBJECT_MANAGER_RESET = LOAD_PROGRESS_POST_VICTORY_CONDITION_SET_VICTORY_CONDITION + 1,
	LOAD_PROGRESS_POST_TERRAIN_LOGIC_NEW_MAP = LOAD_PROGRESS_POST_GHOST_OBJECT_MANAGER_RESET + 1,
	LOAD_PROGRESS_POST_BRIDGE_LOAD = LOAD_PROGRESS_POST_TERRAIN_LOGIC_NEW_MAP + 1,
	LOAD_PROGRESS_POST_PATHFINDER_NEW_MAP = LOAD_PROGRESS_POST_BRIDGE_LOAD + 1,

	LOAD_PROGRESS_LOOP_ALL_THE_FREAKN_OBJECTS = LOAD_PROGRESS_POST_BRIDGE_LOAD + 1,
	LOAD_PROGRESS_MAX_ALL_THE_FREAKN_OBJECTS = 80,// THE END OF THE BIG ASS PROGRESS PAUSE

	LOAD_PROGRESS_LOOP_INITIAL_NETWORK_BUILDINGS = LOAD_PROGRESS_MAX_ALL_THE_FREAKN_OBJECTS + 1,  // Increment the next one by at least MAX_SLOTS
	LOAD_PROGRESS_POST_INITIAL_NETWORK_BUILDINGS = LOAD_PROGRESS_LOOP_INITIAL_NETWORK_BUILDINGS + MAX_SLOTS + 1,

	LOAD_PROGRESS_POST_PRELOAD_ASSETS = LOAD_PROGRESS_POST_INITIAL_NETWORK_BUILDINGS + 1,
	LOAD_PROGRESS_POST_STARTING_CAMERA = LOAD_PROGRESS_POST_PRELOAD_ASSETS + 1,
	LOAD_PROGRESS_POST_STARTING_CAMERA_2 = LOAD_PROGRESS_POST_STARTING_CAMERA + 1,
	LOAD_PROGRESS_END = 100,
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static Waypoint * findNamedWaypoint(AsciiString name)
{
	for (Waypoint *way = TheTerrainLogic->getFirstWaypoint(); way; way = way->getNext())
	{
		if (way->getName() == name)
		{
			return way;
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void setFPMode()
{
  // Set floating point round mode to CHOP, which only comes
  // into play when precision is exceeded.  This is necessary
  // for the fast float to int routines used elsewhere in the
  // system.
	//
	// Also set floating point precision to low.  It could be
	// anything as long as it is consistent, really, but this
	// is in the (vain?) hope of any slight speed boost.
	//
	// GeneralsX @bugfix BenderAI 14/03/2026 Align non-Windows x86 FPU control with the Windows replay determinism baseline.
	#ifdef _WIN32
	_fpreset();

	UnsignedInt curVal = _statusfp();
	UnsignedInt newVal = curVal;
	newVal = (newVal & ~_MCW_RC) | (_RC_NEAR & _MCW_RC);
	//newVal = (newVal & ~_MCW_RC) | (_RC_CHOP & _MCW_RC);
	newVal = (newVal & ~_MCW_PC) | (_PC_24   & _MCW_PC);

	_controlfp(newVal, _MCW_PC | _MCW_RC);
	#else
	fesetenv(FE_DFL_ENV);
	feclearexcept(FE_ALL_EXCEPT);
	fesetround(FE_TONEAREST);

	#if defined(__i386__) || defined(__x86_64__)
	unsigned short controlWord = 0;
	__asm__ __volatile__("fnstcw %0" : "=m" (controlWord));
	controlWord = static_cast<unsigned short>(controlWord & ~0x0F00u);
	__asm__ __volatile__("fldcw %0" : : "m" (controlWord));
	#endif

	#if defined(__SSE__) || defined(__x86_64__)
	UnsignedInt mxcsr = static_cast<UnsignedInt>(_mm_getcsr());
	mxcsr = (mxcsr & ~static_cast<UnsignedInt>(_MM_ROUND_MASK)) | static_cast<UnsignedInt>(_MM_ROUND_NEAREST);
	_mm_setcsr(mxcsr);
	#endif
	#endif
}

//-------------------------------------------------------------------------------------------------
const char* toString(GameMode mode)
{
	switch (mode)
	{
		case GAME_SINGLE_PLAYER:
			return "GAME_SINGLE_PLAYER";
		case GAME_LAN:
			return "GAME_LAN";
		case GAME_SKIRMISH:
			return "GAME_SKIRMISH";
		case GAME_REPLAY:
			return "GAME_REPLAY";
		case GAME_SHELL:
			return "GAME_SHELL";
		case GAME_INTERNET:
			return "GAME_INTERNET";
		case GAME_NONE:
			return "GAME_NONE";
		default:
			return "GAME_UNKNOWN";
	}
}

// ------------------------------------------------------------------------------------------------
/** GameLogic class constructor */
// ------------------------------------------------------------------------------------------------
GameLogic::GameLogic()
{
	m_background = nullptr;
	m_CRC = 0;
	m_isInUpdate = FALSE;

	m_rankPointsToAddAtGameStart = 0;

	for(Int i = 0; i < MAX_SLOTS; i++)
	{
		m_progressComplete[i] = FALSE;
		m_progressCompleteTimeout[i] = 0;
	}

	m_shouldValidateCRCs = FALSE;

	m_startNewGame = FALSE;

	m_frame = 0;
	m_hasUpdated = FALSE;
	m_frameObjectsChangedTriggerAreas = 0;
	m_width = 0;
	m_height = 0;
	m_objList = nullptr;
	m_curUpdateModule = nullptr;
	m_nextObjID = INVALID_ID;
	m_startNewGame = FALSE;
	m_gameMode = GAME_NONE;
	m_rankLevelLimit = 1000;
	m_pauseFrame = 0;
	m_gamePaused = FALSE;
	m_pauseSound = FALSE;
	m_pauseMusic = FALSE;
	m_pauseInput = FALSE;
	m_inputEnabledMemory = TRUE;
	m_mouseVisibleMemory = TRUE;
	m_logicTimeScaleEnabledMemory = FALSE;
	m_loadScreen = nullptr;
	m_forceGameStartByTimeOut = FALSE;
	m_quitToDesktopAfterMatch = FALSE;
#ifdef DUMP_PERF_STATS
	m_overallFailedPathfinds = 0;
#endif

	m_loadingMap = FALSE;
	m_loadingSave = FALSE;
	m_clearingGameData = FALSE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool GameLogic::isInSinglePlayerGame()
{
	return (m_gameMode == GAME_SINGLE_PLAYER ||
		(TheRecorder && TheRecorder->isPlaybackMode() && TheRecorder->getGameMode() == GAME_SINGLE_PLAYER));
}


//-------------------------------------------------------------------------------------------------
/** Destroy all objects immediately */
//-------------------------------------------------------------------------------------------------
void GameLogic::destroyAllObjectsImmediate()
{
	Object *obj;
	Object *nextObj;

	// TheSuperHackers @bugfix xezon 22/05/2025 Set all remaining objects effectively dead to avoid triggering their
	// death modules that eventually would spawn new objects, such as debris, which could then crash the game.
	// See https://github.com/TheSuperHackers/GeneralsGameCode/issues/896
	for( obj = m_objList; obj; obj = obj->getNextObject() )
	{
		obj->setEffectivelyDead(true);
	}

	// destroy all remaining objects
	for( obj = m_objList; obj; obj = nextObj )
	{
		nextObj = obj->getNextObject();
		destroyObject( obj );
	}

	// process the destroy list immediately
	processDestroyList();
	DEBUG_ASSERTCRASH( m_objList == nullptr, ("destroyAllObjectsImmediate: Object list not cleared") );

}

//-------------------------------------------------------------------------------------------------
/**GameLogic class destructor, the destruction order should mirror the
 * initialization order */
// ------------------------------------------------------------------------------------------------
GameLogic::~GameLogic()
{

	// clear any object TOC we might have
	m_objectTOC.clear();

	if (m_background)
	{
		m_background->destroyWindows();
		deleteInstance(m_background);
		m_background = nullptr;
	}

	// destroy all remaining objects
	destroyAllObjectsImmediate();

	// delete the logical terrain
	delete TheTerrainLogic;
	TheTerrainLogic = nullptr;

	delete TheGhostObjectManager;
	TheGhostObjectManager=nullptr;

	// delete the partition manager
	delete ThePartitionManager;
	ThePartitionManager = nullptr;

	delete TheScriptActions;
	TheScriptActions = nullptr;

	delete TheScriptConditions;
	TheScriptConditions = nullptr;

	// delete the Script Engine
	delete TheScriptEngine;
	TheScriptEngine = nullptr;

	// Null out TheGameLogic
	TheGameLogic = nullptr;
}

// ------------------------------------------------------------------------------------------------
/** (re)initialize the instance. */
// ------------------------------------------------------------------------------------------------
void GameLogic::init()
{

	setFPMode();

	// create the partition manager
	ThePartitionManager = NEW PartitionManager;
	ThePartitionManager->init();
	ThePartitionManager->setName("ThePartitionManager");


	// Create system for holding deleted objects that are
	// still in the partition manager because player has a fogged
	// view of them.
	TheGhostObjectManager = createGhostObjectManager(TheGlobalData->m_headless);

	// create the terrain logic
	TheTerrainLogic = createTerrainLogic();
	TheTerrainLogic->init();
	TheTerrainLogic->setName("TheTerrainLogic");

	// Create script engine system.
	TheScriptActions = NEW ScriptActions;		 // Basically, a subsystem of TheScriptEngine.
	TheScriptConditions = NEW ScriptConditions;	 // Basically, a subsystem of TheScriptEngine.
	TheScriptEngine = NEW ScriptEngine;
	TheScriptEngine->init();
	TheScriptEngine->setName("TheScriptEngine");

	// create a team for the player
	//DEBUG_ASSERTCRASH(ThePlayerList, ("null ThePlayerList"));
	//ThePlayerList->setLocalPlayer(0);

	reset();
	m_isInUpdate = FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Reset the game logic systems */
//-------------------------------------------------------------------------------------------------
void GameLogic::reset()
{
	m_thingTemplateBuildableOverrides.clear();
	m_controlBarOverrides.clear();

	// set the hash to be rather large. We need to optimize this value later.
	m_objHash.clear();
#if USING_STLPORT
	m_objHash.resize(OBJ_HASH_SIZE);
#else
	m_objHash.reserve(OBJ_HASH_SIZE);
#endif

	m_pauseFrame = 0;
	m_pauseSound = FALSE;
	m_pauseMusic = FALSE;
	m_inputEnabledMemory = TRUE;
	m_mouseVisibleMemory = TRUE;
	m_logicTimeScaleEnabledMemory = FALSE;
	pauseGameLogic(FALSE);
	pauseGameInput(FALSE);

	setFPMode();

	// destroy all objects
	destroyAllObjectsImmediate();

	m_nextObjID = (ObjectID)1;

	m_frameObjectsChangedTriggerAreas = 0;

	TheGhostObjectManager->reset();
	ThePartitionManager->reset();
	TheTerrainLogic->reset();
	TheAI->reset();
	TheScriptEngine->reset();

	m_CRC = 0;
	for(Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_progressComplete[i] = FALSE;
		m_progressCompleteTimeout[i] = 0;
	}
	m_forceGameStartByTimeOut = FALSE;

	delete TheStatsCollector;
	TheStatsCollector = nullptr;

	// clear any table of contents we have
	m_objectTOC.clear();

	m_frame = 0;
	m_hasUpdated = FALSE;
	m_width = DEFAULT_WORLD_WIDTH;
	m_height = DEFAULT_WORLD_HEIGHT;
	m_objList = nullptr;
#ifdef ALLOW_NONSLEEPY_UPDATES
	m_normalUpdates.clear();
#endif
	for (std::vector<UpdateModulePtr>::iterator it = m_sleepyUpdates.begin(); it != m_sleepyUpdates.end(); ++it)
	{
		(*it)->friend_setIndexInLogic(-1);
	}
	m_sleepyUpdates.clear();
	m_curUpdateModule = nullptr;

	m_isScoringEnabled = TRUE;
	m_showBehindBuildingMarkers = TRUE;
	m_drawIconUI = TRUE;
	m_showDynamicLOD = TRUE;
	m_scriptHulkMaxLifetimeOverride = -1;

	// Clean up any water transparency overrides that were generated for this map.
	WaterTransparencySetting *wt = (WaterTransparencySetting*) TheWaterTransparency.getNonOverloadedPointer();
	TheWaterTransparency = (WaterTransparencySetting*) wt->deleteOverrides();

	m_rankPointsToAddAtGameStart = 0;
}

static Object * placeObjectAtPosition(Int slotNum, AsciiString objectTemplateName, Coord3D& pos, Player *pPlayer,
																	const PlayerTemplate *pTemplate)
{
	const ThingTemplate* btt = TheThingFactory->findTemplate(objectTemplateName);
	Object *obj = TheThingFactory->newObject( btt, pPlayer->getDefaultTeam() );
	DEBUG_ASSERTCRASH(obj, ("TheThingFactory didn't give me a valid Object for player %d's (%ls) starting building",
		slotNum, pTemplate->getDisplayName().str()));
	if (obj)
	{
		obj->setOrientation(obj->getTemplate()->getPlacementViewAngle());
		obj->setPosition( &pos );

		//DEBUG_LOG(("Placed a starting building for %s at waypoint %s", playerName.str(), waypointName.str()));
		CRCDEBUG_LOG(("Placed an object for %ls at pos (%g,%g,%g)", pPlayer->getPlayerDisplayName().str(),
			pos.x, pos.y, pos.z));
		DUMPCOORD3D(&pos);

		Team *team = pPlayer->getDefaultTeam();
		// Now onCreates were called at the constructor.  This magically created
		// thing needs to be considered as Built for Game specific stuff.
		for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
		{
			CreateModuleInterface* create = (*m)->getCreate();
			if (!create)
				continue;
			create->onBuildComplete();
		}

		// Since the team now has members, activate it.
		// srj sez: team should not be null, but could be for ill-formed user maps. so don't crash.
		if (team)
			team->setActive();
		TheAI->pathfinder()->addObjectToPathfindMap(obj);
		if (obj->getAIUpdateInterface() && !obj->isKindOf(KINDOF_IMMOBILE)) {
			CRCDEBUG_LOG(("Not immobile - adjusting dest"));
			if (TheAI->pathfinder()->adjustDestination(obj, obj->getAIUpdateInterface()->getLocomotorSet(), &pos)) {
				DUMPCOORD3D(&pos);
				TheAI->pathfinder()->updateGoal(obj, &pos, LAYER_GROUND);	// Units always start on the ground for now.  jba.
				obj->setPosition( &pos );
			}
		}
	}

	return obj;
}

static void placeNetworkBuildingsForPlayer(Int slotNum, const GameSlot *pSlot, Player *pPlayer, const PlayerTemplate *pTemplate)
{
	Int startPos = pSlot->getStartPos();
	AsciiString waypointName;
	waypointName.format("Player_%d_Start", startPos+1); // start pos waypoints are 1-based

	AsciiString rallyWaypointName;
	rallyWaypointName.format("Player_%d_Rally", startPos+1); // start pos waypoints are 1-based

	Waypoint *waypoint = findNamedWaypoint(waypointName);
	Waypoint *rallyWaypoint = findNamedWaypoint(rallyWaypointName);
	DEBUG_ASSERTCRASH(waypoint, ("Player %d has no starting waypoint (Player_%d_Start)", slotNum, startPos));
	if (!waypoint)
		return;

	Coord3D pos = *waypoint->getLocation();
	pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );

	AsciiString buildingTemplateName = pTemplate->getStartingBuilding();

	DEBUG_ASSERTCRASH(!buildingTemplateName.isEmpty(), ("no starting building type for player %d (playertemplate %ls)",
		slotNum, pTemplate->getDisplayName().str()));
	if (buildingTemplateName.isEmpty())
		return;

	DEBUG_LOG(("Placing starting building at waypoint %s", waypointName.str()));
	Object *conYard = placeObjectAtPosition(slotNum, buildingTemplateName, pos, pPlayer, pTemplate);

	if (!conYard)
		return;

	pPlayer->onStructureCreated(nullptr, conYard);
	pPlayer->onStructureConstructionComplete(nullptr, conYard, FALSE);

	//pos.x -= conYard->getGeometryInfo().getBoundingSphereRadius()/2;
	pos.y -= conYard->getGeometryInfo().getBoundingSphereRadius()/2;
	pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );

	if (rallyWaypoint)
	{
		pos = *rallyWaypoint->getLocation();
		pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );
	}

	for (Int i=0; i<MAX_MP_STARTING_UNITS; ++i)
	{
		AsciiString objName = pTemplate->getStartingUnit(i);
		if (objName.isNotEmpty())
		{
			Coord3D objPos = pos;
			FindPositionOptions options;
			options.minRadius = conYard->getGeometryInfo().getBoundingSphereRadius() * 0.7f;
			options.maxRadius = conYard->getGeometryInfo().getBoundingSphereRadius() * 1.3f;
			DEBUG_LOG(("Placing starting object %d (%s)", i, objName.str()));
			ThePartitionManager->update();
			Bool foundPos = ThePartitionManager->findPositionAround(&pos, &options, &objPos);
			if (foundPos)
			{
				Object *unit = placeObjectAtPosition(slotNum, objName, objPos, pPlayer, pTemplate);
				if (unit) {
					pPlayer->onUnitCreated(nullptr, unit);
				}
			}
			else
			{
				DEBUG_LOG(("Could not find position"));
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
LoadScreen *GameLogic::getLoadScreen( Bool saveGame )
{
	switch (m_gameMode)
	{
	case GAME_SHELL:
		return NEW ShellGameLoadScreen;
		break;
	case GAME_SINGLE_PLAYER:
		if(TheCampaignManager->getCurrentMission() && saveGame == FALSE )
			return NEW SinglePlayerLoadScreen;
		else
			return NEW ShellGameLoadScreen;
		break;
	case GAME_SKIRMISH:
		return NEW MultiPlayerLoadScreen;
		break;
	case GAME_LAN:
		return NEW MultiPlayerLoadScreen;
		break;
	case GAME_REPLAY:
		return NEW ShellGameLoadScreen;
		break;
	case GAME_INTERNET:
		return NEW GameSpyLoadScreen;
		break;
	case GAME_NONE:
	default:
		return nullptr;
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void handleNameChange( MapObject *mapObj )
{
	if( !mapObj->getName().compare( "AmericaTankLeopard" ) )
	{
		mapObj->setName( "AmericaTankCrusader" );
		const ThingTemplate *thingTemplate = TheThingFactory->findTemplate( mapObj->getName() );
		mapObj->setThingTemplate( thingTemplate );
	}
	if( !mapObj->getName().compare( "AmericaVehicleHumVee" ) )
	{
		mapObj->setName( "AmericaVehicleHumvee" );
		const ThingTemplate *thingTemplate = TheThingFactory->findTemplate( mapObj->getName() );
		mapObj->setThingTemplate( thingTemplate );
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static void checkForDuplicateColors( GameInfo *game )
{
	if(!game)
		return;
	Int i;

	// In QuickMatch, people can possibly set preferred color and house.
	// Here, we check for collisions in the color choice.  If there is a
	// collision, the first player will get the color, and the other(s)
	// will map to random.
	for (i=MAX_SLOTS-1; i>=0; --i)
	{
		GameSlot *slot = game->getSlot(i);

		if (!slot || !slot->isOccupied())
			continue;

		Int colorIdx = slot->getColor();
		if (colorIdx < 0 || colorIdx >= TheMultiplayerSettings->getNumColors())
			continue; // don't need to fix up random ones

		slot->setColor(-1);
		if (!game->isColorTaken(colorIdx))
		{
			// we were the only one using the color.  it is safe to keep using it.
			slot->setColor(colorIdx);
		}
		else if (colorIdx >= 0)
		{
			DEBUG_LOG(("Clearing color %d for player %d", colorIdx, i));
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static void populateRandomSideAndColor( GameInfo *game )
{
	if(!game)
		return;
	Int i;

#define MORE_RANDOM
#ifdef MORE_RANDOM
	std::vector<Int> startSlots;
	for (i = 0; i < ThePlayerTemplateStore->getPlayerTemplateCount(); ++i)
	{
		const PlayerTemplate* ptTest = ThePlayerTemplateStore->getNthPlayerTemplate(i);
		if (!ptTest || ptTest->getStartingBuilding().isEmpty())
			continue;
		startSlots.push_back(i);
	}
#endif

	for (i=0; i<MAX_SLOTS; ++i)
	{
		GameSlot *slot = game->getSlot(i);

		if (!slot || !slot->isOccupied())
			continue;

		// clean up random factions
		Int playerTemplateIdx = slot->getPlayerTemplate();
		DEBUG_LOG(("Player %d has playerTemplate index %d", i, playerTemplateIdx));
		while (playerTemplateIdx != PLAYERTEMPLATE_OBSERVER && (playerTemplateIdx < 0 || playerTemplateIdx >= ThePlayerTemplateStore->getPlayerTemplateCount()))
		{
			DEBUG_ASSERTCRASH(playerTemplateIdx == -1, ("Non-random bad playerTemplate %d in slot %d", playerTemplateIdx, i));
#ifdef MORE_RANDOM
			// our RNG is basically shit -- horribly nonrandom at the start of the sequence.
			// get a few values at random to get rid of the dreck.
			// there's no mathematical basis for this, but empirically, it helps a lot.
			UnsignedInt silly = GetGameLogicRandomSeed() % 7;
			for (UnsignedInt poo = 0; poo < silly; ++poo)
			{
				GameLogicRandomValue(0, 1);	// ignore result
			}
			Int idxIdx = GameLogicRandomValue(0, 1000) % startSlots.size();
			playerTemplateIdx = startSlots[idxIdx];
#else
			playerTemplateIdx = GameLogicRandomValue(0, ThePlayerTemplateStore->getPlayerTemplateCount()-1);
#endif
			const PlayerTemplate* pt = ThePlayerTemplateStore->getNthPlayerTemplate(playerTemplateIdx);
			if (!pt || pt->getStartingBuilding().isEmpty())
			{
#ifdef MORE_RANDOM
				DEBUG_CRASH(("should not be possible"));
#endif
				playerTemplateIdx = -1; // only pick playable factions
			}
			else
			{
				DEBUG_LOG(("Setting playerTemplateIdx %d to %d", i, playerTemplateIdx));
				slot->setPlayerTemplate(playerTemplateIdx);
			}
		}

		Int colorIdx = slot->getColor();
		if (colorIdx < 0 || colorIdx >= TheMultiplayerSettings->getNumColors())
		{
			DEBUG_ASSERTCRASH(colorIdx == -1, ("Non-random bad color %d in slot %d", colorIdx, i));
			while (colorIdx == -1)
			{
				colorIdx = GameLogicRandomValue(0, TheMultiplayerSettings->getNumColors()-1);
				if (game->isColorTaken(colorIdx))
					colorIdx = -1;
			}
			DEBUG_LOG(("Setting color %d to %d", i, colorIdx));
			slot->setColor(colorIdx);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static const WaypointMap s_emptyWaypoints = WaypointMap();

static void populateRandomStartPosition( GameInfo *game )
{
	if(!game)
		return;

	Int i;
	Int numPlayers = MAX_SLOTS;
	const MapMetaData *md = TheMapCache->findMap( game->getMap() );
	if (md)
		numPlayers = md->m_numPlayers;
	else
		printf("Could not find map \"%s\"\n", game->getMap().str());
	DEBUG_ASSERTCRASH( md , ("Could not find map %s in the mapcache", game->getMap().str()));

	// generate a map of start spot distances
	Real startSpotDistance[MAX_SLOTS][MAX_SLOTS];
	for (i=0; i<MAX_SLOTS; ++i)
	{
		for (Int j=0; j<MAX_SLOTS; ++j)
		{
			if (i != j && (i<numPlayers && j<numPlayers))
			{
				const WaypointMap& waypoints = md ? md->m_waypoints : s_emptyWaypoints;
				AsciiString w1, w2;
				w1.format("Player_%d_Start", i+1);
				w2.format("Player_%d_Start", j+1);
				WaypointMap::const_iterator c1 = waypoints.find(w1);
				WaypointMap::const_iterator c2 = waypoints.find(w2);
				if (c1 == waypoints.end() || c2 == waypoints.end())
				{
					// couldn't find a waypoint.  must be kinda far away.
					startSpotDistance[i][j] = 1000000.0f;
				}
				else
				{
					Coord3D p1 = c1->second;
					Coord3D p2 = c2->second;
					startSpotDistance[i][j] = WWMath::SqrtOrigin( sqr(p1.x-p2.x) + sqr(p1.y-p2.y) );
				}
			}
			else
			{
				startSpotDistance[i][j] = 0.0f; // not gonna need this
			}
		}
	}

	// see if a start spot has been chosen at all yet
	Bool hasStartSpotBeenPicked = FALSE;
	Bool taken[MAX_SLOTS];
	for (i=0; i<MAX_SLOTS; ++i)
	{
		taken[i] = (i<numPlayers)?FALSE:TRUE;
	}
	for (i=0; i<MAX_SLOTS; ++i)
	{
		GameSlot *slot = game->getSlot(i);

		if (!slot || !slot->isOccupied() || slot->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER)
			continue;

		Int posIdx = slot->getStartPos();
		if (posIdx >= 0 || posIdx >= numPlayers)
		{
			hasStartSpotBeenPicked = TRUE;
			taken[posIdx] = TRUE;
		}
	}

	// now pick non-observer spots
	for (i=0; i<MAX_SLOTS; ++i)
	{
		GameSlot *slot = game->getSlot(i);

		if (!slot || !slot->isOccupied() || slot->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER)
			continue;

		// clean up random start spots
		Int posIdx = slot->getStartPos();
		if (posIdx < 0 || posIdx >= numPlayers)
		{
			DEBUG_ASSERTCRASH(posIdx == -1, ("Non-random bad start position %d in slot %d", posIdx, i));
			if (hasStartSpotBeenPicked)
			{
				// pick the farthest spot away
				Real farthestDistance = 0.0f;
				Int farthestIndex = -1;
				for (posIdx = 0; posIdx < numPlayers; ++posIdx)
				{
					if (!taken[posIdx])
					{
						if (farthestIndex < 0)
						{
							farthestIndex = posIdx; // take this one as best if none else
							for (Int n=0; n<numPlayers; ++n)
							{
								if (taken[n] && n != posIdx)
									farthestDistance += startSpotDistance[posIdx][n];
							}
						}
						else
						{
							Real dist = 0.0f;
							for (Int n=0; n<numPlayers; ++n)
							{
								if (taken[n] && n != posIdx)
									dist += startSpotDistance[posIdx][n];
							}
							if (dist > farthestDistance)
							{
								farthestDistance = dist;
								farthestIndex = posIdx;
							}
						}
					}
				}
				DEBUG_ASSERTCRASH(farthestIndex >= 0, ("Couldn't find a farthest spot!"));
				slot->setStartPos(farthestIndex);
				taken[farthestIndex] = TRUE;
			}
			else
			{
				// We're the first real spot.  Pick randomly.
				// This while loop shouldn't be necessary, since we're first.  Why not, though?
				while (posIdx == -1)
				{
					posIdx = GameLogicRandomValue(0, numPlayers-1);
					if (game->isStartPositionTaken(posIdx))
						posIdx = -1;
				}
				DEBUG_LOG(("Setting start position %d to %d (random choice)", i, posIdx));
				slot->setStartPos(posIdx);
				taken[posIdx] = TRUE;
				hasStartSpotBeenPicked = TRUE;
			}
		}
	}

	// now go back & assign observer spots
	Int numPlayersInGame = 0;
	for (i=0; i<MAX_SLOTS; ++i)
	{
		const GameSlot *slot = game->getConstSlot(i);
		if (slot->isOccupied() && slot->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER)
			++numPlayersInGame;
	}
	for (i=0; i<MAX_SLOTS; ++i)
	{
		GameSlot *slot = game->getSlot(i);

		if (!slot || !slot->isOccupied())
			continue;

		if (slot->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER)
			continue;

		// clean up random start spots
		Int posIdx = -1;
		if (numPlayersInGame == 0)
			posIdx = 0;
		while (posIdx == -1)
		{
			posIdx = GameLogicRandomValue(0, numPlayers-1);
			if (!game->isStartPositionTaken(posIdx))
				posIdx = -1;
		}
		DEBUG_LOG(("Setting observer start position %d to %d", i, posIdx));
		slot->setStartPos(posIdx);
	}
}

// ------------------------------------------------------------------------------------------------
/** Update the load screen progress */
// ------------------------------------------------------------------------------------------------
void GameLogic::updateLoadProgress( Int progress )
{

	if( m_loadScreen )
		m_loadScreen->update( progress );

	if (TheGameEngine->getQuitting() || m_quitToDesktopAfterMatch)
	{
		throw QuitGameException();
	}
}

// ------------------------------------------------------------------------------------------------
/** Delete the load screen */
// ------------------------------------------------------------------------------------------------
void GameLogic::deleteLoadScreen()
{

	delete m_loadScreen;
	m_loadScreen = nullptr;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::updateDisplayBusyState()
{
	const Bool busySystem = isInInteractiveGame() && !isGamePaused();
	const Bool busyDisplay = busySystem && !TheGlobalData->m_headless;

	OSDisplaySetBusyState(busyDisplay, busySystem);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::setGameMode( GameMode mode )
{
	GameMode prev = m_gameMode;
	m_gameMode = mode;

	TheMouse->onGameModeChanged(prev, mode);

	updateDisplayBusyState();
}

// ------------------------------------------------------------------------------------------------
/** Entry point for starting a new game, the engine is already in clean state at this
	* point and ready to load up with all the data */
// ------------------------------------------------------------------------------------------------
void GameLogic::startNewGame( Bool loadingSaveGame )
{
	try
	{
		tryStartNewGame(loadingSaveGame);
	}
	catch (QuitGameException&)
	{
		if (m_quitToDesktopAfterMatch && TheGameEngine)
		{
			TheGameEngine->setQuitting(TRUE);
		}
	}
}

void GameLogic::tryStartNewGame( Bool loadingSaveGame )
{
	#ifdef DUMP_PERF_STATS
	__int64 startTime64;
	__int64 endTime64,freq64;
	GetPrecisionTimerTicksPerSec(&freq64);
	GetPrecisionTimer(&startTime64);
	#endif

	// reset the frame counter
	m_frame = 0;
	m_hasUpdated = FALSE;

#ifdef DEBUG_CRC
	// TheSuperHackers @info helmutbuhler 04/09/2025
	// Let CRC Logger know that a new game was started.
	CRCDebugStartNewGame();
#endif

	setLoadingMap( TRUE );

	if( loadingSaveGame == FALSE )
	{

		// record pristine map name when we're loading from the map (not a save game)
		TheGameState->setPristineMapName( TheGlobalData->m_mapName );

		//
		// sanity, the pristine map should never start with "Save", otherwise that would be
		// a map from the save directory
		//
		if (TheGameState->isInSaveDirectory(TheGlobalData->m_mapName))
		{

			DEBUG_CRASH(( "FATAL SAVE/LOAD ERROR! - Setting a pristine map name that refers to a map in the save directory.  The pristine map should always refer to the ORIGINAL map in the Maps directory, if the pristine map string is corrupt then map.ini files will not load correctly." ));

		}

		if( m_startNewGame == FALSE )
		{
			/// @todo: Here is where we would look at the game mode & play an intro movie or something.
			// Failing that, we just set the flag so the actual game can start from a uniform
			// entry point (startNewGame() called from update()).
			if( m_gameMode == GAME_SINGLE_PLAYER )
			{

				if(m_background)
				{
					m_background->destroyWindows();
					deleteInstance(m_background);
					m_background = nullptr;
				}
				m_loadScreen = getLoadScreen( loadingSaveGame );
				if(m_loadScreen)
				{
					TheWritableGlobalData->m_loadScreenRender = TRUE;	///< mark it so only a few select things are rendered during load
					m_loadScreen->init(nullptr);
				}

			}

			m_startNewGame = TRUE;
			return;

		}

	}

	m_rankLevelLimit = 1000;	// this is reset every game.

	//
	// only reset the next object ID allocator counter when we're not loading a save game.
	// for save games, we read this value out of the save game file and it is important
	// that we preserve it as we load and execute the game
	//
	if( loadingSaveGame == FALSE )
		m_nextObjID = (ObjectID)1;

	TheWritableGlobalData->m_loadScreenRender = TRUE;	///< mark it so only a few select things are rendered during load
	TheWritableGlobalData->m_TiVOFastMode = FALSE;	//always disable the TIVO fast-forward mode at the start of a new game.

	// Fill in the game color and Factions before we do the Load Screen
	TheGameInfo = nullptr;
	if (TheNetwork)
	{
		if (TheLAN)
		{
			DEBUG_LOG(("Starting network game"));
			TheGameInfo = TheLAN->GetMyGame();
		}
		else
		{
			DEBUG_LOG(("Starting gamespy game"));
			TheGameInfo = TheGameSpyGame;	/// @todo: MDC add back in after demo
		}
	}
	else
	{
		if (TheRecorder && TheRecorder->isPlaybackMode())
		{
			TheGameInfo = TheRecorder->getGameInfo();
		}
		else if(m_gameMode == GAME_SKIRMISH)
		{
		  TheGameInfo = TheSkirmishGameInfo;
		}
	}

	checkForDuplicateColors( TheGameInfo );

	Bool isSkirmishOrSkirmishReplay = FALSE;
	if (TheGameInfo)
	{
		for (Int i=0; i<MAX_SLOTS; ++i)
		{
			GameSlot *slot = TheGameInfo->getSlot(i);
			if (!loadingSaveGame) {
				if (slot->hasSavedOriginalSetup())
				{
					DEBUG_ASSERTCRASH(m_gameMode == GAME_SKIRMISH, ("Expected GAME_SKIRMISH but got %s", toString(m_gameMode)));

					// TheSuperHackers @fix Caball009 19/03/2026 Random color, position and faction are based on the logical seed. For improved determinism,
					// restarted games now set the original values so that the games start with the exact same logical seed values as the first time.
					slot->setColor(slot->getOriginalColor());
					slot->setStartPos(slot->getOriginalStartPos());
					slot->setPlayerTemplate(slot->getOriginalPlayerTemplate());
				}
				else
				{
					slot->saveOriginalSetup();
				}
			}
			if (slot->isAI())
			{
				isSkirmishOrSkirmishReplay = TRUE;
			}
		}
	} else {
		if (m_gameMode == GAME_SINGLE_PLAYER)	{
			delete TheSkirmishGameInfo;
			TheSkirmishGameInfo = nullptr;
		}
	}

	populateRandomSideAndColor( TheGameInfo );
	populateRandomStartPosition( TheGameInfo );

	//****************************//
	// Start the LoadScreen Now!	//
	//****************************//

	// Get the m_loadScreen for this kind of game
	if(!m_loadScreen && !(TheRecorder && TheRecorder->getMode() == RECORDERMODETYPE_SIMULATION_PLAYBACK))
	{
		m_loadScreen = getLoadScreen( loadingSaveGame );
		if(m_loadScreen && !TheGlobalData->m_headless)
		{
			TheMouse->setVisibility(FALSE);
			m_loadScreen->init(TheGameInfo);

			updateLoadProgress( LOAD_PROGRESS_START );
		}
	}
	if(m_background)
	{
		m_background->destroyWindows();
		deleteInstance(m_background);
		m_background = nullptr;
	}
	setFPMode();
	if(TheCampaignManager)
		TheCampaignManager->SetVictorious(FALSE);
	m_startNewGame = FALSE;

	// update the loadscreen
	if(m_loadScreen)
		updateLoadProgress(LOAD_PROGRESS_POST_PARTICLE_INI_LOAD);

	DEBUG_ASSERTCRASH(m_frame == 0, ("framecounter expected to be 0 here"));

	// before loading the map, load the map.ini file in the same directory.
	loadMapINI( TheGlobalData->m_mapName );

	// load a map
	TheTerrainLogic->loadMap( TheGlobalData->m_mapName, false );
	// anytime the world's size changes, must reset the partition mgr
	//ThePartitionManager->init();

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_LOAD_MAP);

	#ifdef DUMP_PERF_STATS
	GetPrecisionTimer(&endTime64);
	char Buf[256];
	sprintf(Buf,"After terrainlogic->loadmap=%f",((double)(endTime64-startTime64)/(double)(freq64)*1000.0));
		//DEBUG_LOG(("Placed a starting building for %s at waypoint %s", playerName.str(), waypointName.str()));
	DEBUG_LOG(("%s", Buf));
	#endif

	Int localSlot = 0;
	Int progressCount = LOAD_PROGRESS_SIDE_POPULATION;
	if (TheGameInfo)
	{

		if (TheGameEngine->isMultiplayerSession() || isSkirmishOrSkirmishReplay)
		{
			// Saves off any player, and resets the sides to 0 players so we can add the skirmish players.
			TheSidesList->prepareForMP_or_Skirmish();
		}

		//DEBUG_LOG(("Starting LAN game with %d players", TheGameInfo->getNumPlayers()));
		for (int i=0; i<MAX_SLOTS; ++i)
		{
			// Add a Side to TheSidesList
			GameSlot *slot = TheGameInfo->getSlot(i);

			if (!slot || !slot->isHuman())
			{
				m_progressComplete[i] = TRUE;
				lastHeardFrom(i);
			}

			if (!slot || !slot->isOccupied())
				continue;

			Dict d;
			d.clear();
			AsciiString playerName;
			playerName.format("player%d", i);
			d.setAsciiString(TheKey_playerName, playerName);
			d.setBool(TheKey_playerIsHuman, slot->isHuman());
			d.setUnicodeString(TheKey_playerDisplayName, slot->getName());
			const PlayerTemplate* pt;
			if (slot->getPlayerTemplate() >= 0)
				pt = ThePlayerTemplateStore->getNthPlayerTemplate(slot->getPlayerTemplate());
			else
				pt = ThePlayerTemplateStore->findPlayerTemplate( TheNameKeyGenerator->nameToKey("FactionObserver") );
			if (pt)
			{
				d.setAsciiString(TheKey_playerFaction, KEYNAME(pt->getNameKey()));
			}

			if (TheGameInfo->isPlayerPreorder(i))
			{
				d.setBool(TheKey_playerIsPreorder, TRUE);
			}

			AsciiString enemiesString, alliesString;
			Int team = slot->getTeamNumber();
			DEBUG_LOG(("Looking for allies of player %d, team %d", i, team));
			for(int j=0; j < MAX_SLOTS; ++j)
			{
				GameSlot *teamSlot = TheGameInfo->getSlot(j);
				// for check to see if we're trying to add ourselves
				if(i == j || !teamSlot->isOccupied())
					continue;

				DEBUG_LOG(("Player %d is team %d", j, teamSlot->getTeamNumber()));

				AsciiString teamPlayerName;
				teamPlayerName.format("player%d", j);
				// if our team is None, or our team is not equal to their team,
				// then their our enemy
				Bool isEnemy = FALSE;
				if(team == -1 || teamSlot->getTeamNumber() != team ) isEnemy = TRUE;
				DEBUG_LOG(("Player %d is %s", j, (isEnemy)?"enemy":"ally"));

				if (isEnemy)
				{
					if(!enemiesString.isEmpty())
						enemiesString.concat(" ");
					enemiesString.concat(teamPlayerName);
				}
				else
				{
					// he's on our team, add him!
					if(!alliesString.isEmpty())
						alliesString.concat(" ");
					alliesString.concat(teamPlayerName);
				}
			}
			d.setAsciiString(TheKey_playerAllies, alliesString);
			d.setAsciiString(TheKey_playerEnemies, enemiesString);
			DEBUG_LOG(("Player %d's teams are: allies=%s, enemies=%s", i,alliesString.str(),enemiesString.str()));
/*

			Int colorIdx = slot->getColor();
			if (colorIdx < 0 || colorIdx >= TheMultiplayerSettings->getNumColors())
			{
				DEBUG_ASSERTCRASH(colorIdx == -1, ("Non-random bad color %d in slot %d", colorIdx, i));
				while (colorIdx == -1)
				{
					colorIdx = GameLogicRandomValue(0, TheMultiplayerSettings->getNumColors()-1);
					if (game->isColorTaken(colorIdx))
						colorIdx = -1;
				}
				DEBUG_LOG(("Setting color %d to %d", i, colorIdx));
				slot->setColor(colorIdx);
			}
			*/

			d.setInt(TheKey_playerColor, TheMultiplayerSettings->getColor(slot->getColor())->getColor());
			d.setInt(TheKey_playerNightColor, TheMultiplayerSettings->getColor(slot->getColor())->getNightColor());
			d.setInt(TheKey_multiplayerStartIndex, slot->getStartPos());
//			d.setBool(TheKey_multiplayerIsLocal, slot->isLocalPlayer());
//			d.setBool(TheKey_multiplayerIsLocal, slot->getIP() == game->getLocalIP());
			d.setBool(TheKey_multiplayerIsLocal, slot->isHuman() && (slot->getName().compare(TheGameInfo->getSlot(TheGameInfo->getLocalSlotNum())->getName().str()) == 0));

/*
			if (slot->getIP() == game->getLocalIP())
			{
				localSlot = i;
				DEBUG_LOG(("GameLogic::StartNewGame - local slot is %d", localSlot));
			}
*/

			if (isSkirmishOrSkirmishReplay)
			{
				d.setBool(TheKey_playerIsSkirmish, true);
				switch (slot->getState()) {
					case SLOT_EASY_AI : d.setInt(TheKey_skirmishDifficulty, DIFFICULTY_EASY); break;
					case SLOT_MED_AI : d.setInt(TheKey_skirmishDifficulty, DIFFICULTY_NORMAL); break;
					case SLOT_BRUTAL_AI : d.setInt(TheKey_skirmishDifficulty, DIFFICULTY_HARD); break;
					default: break;	 // no setting.
				}
			}

			AsciiString slotNameAscii;
			slotNameAscii.translate(slot->getName());
			if (slot->isHuman() && TheGameInfo->getSlotNum(slotNameAscii) == TheGameInfo->getLocalSlotNum()) {
				localSlot = i;
			}
			TheSidesList->addSide(&d);

			AsciiString playerTeamName;
			playerTeamName.set("team");
			playerTeamName.concat(playerName);

			d.clear();
			d.setAsciiString(TheKey_teamName, playerTeamName);
			d.setAsciiString(TheKey_teamOwner, playerName);
			d.setBool(TheKey_teamIsSingleton, true);
			TheSidesList->addTeam(&d);

			DEBUG_LOG(("Added side %d", i));
			updateLoadProgress(progressCount + i);
		}
	}
	//if(m_gameMode != GAME_REPLAY)
	//{

		// Always add in an observer Player
		Dict d;
		d.setAsciiString(TheKey_playerName, "ReplayObserver");
		d.setBool(TheKey_playerIsHuman, TRUE);
		d.setUnicodeString(TheKey_playerDisplayName, L"Observer");
		const PlayerTemplate* pt;
		pt = ThePlayerTemplateStore->findPlayerTemplate( TheNameKeyGenerator->nameToKey("FactionObserver") );
		if (pt)
		{
			d.setAsciiString(TheKey_playerFaction, KEYNAME(pt->getNameKey()));
		}
		d.setAsciiString(TheKey_playerAllies, AsciiString::TheEmptyString);
		d.setAsciiString(TheKey_playerEnemies, AsciiString::TheEmptyString);
		d.setInt(TheKey_playerColor, TheMultiplayerSettings->getColor(0)->getColor());
		d.setInt(TheKey_playerNightColor, TheMultiplayerSettings->getColor(0)->getNightColor());
		d.setInt(TheKey_multiplayerStartIndex, 0);
		d.setBool(TheKey_multiplayerIsLocal, FALSE);

		TheSidesList->addSide(&d);
		d.clear();
		d.setAsciiString(TheKey_teamName, "teamReplayObserver");
		d.setAsciiString(TheKey_teamOwner, "ReplayObserver");
		d.setBool(TheKey_teamIsSingleton, true);
		TheSidesList->addTeam(&d);
	//}
	TheSidesList->validateSides();

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_SIDE_LIST_INIT);

	// update the player list to match the new map.
	TheTeamFactory->reset();
	ThePlayerList->newGame();

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_PLAYER_LIST_RESET);

	// Tell the script engine that a newe set of scripts is loaded.
	TheScriptEngine->newMap();

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_SCRIPT_ENGINE_NEW_MAP);

	if (TheGameEngine->isMultiplayerSession() || isSkirmishOrSkirmishReplay)
	{
		// if there are no other teams (happens for debugging) don't end the game immediately
		Int numTeams = 0; // this can be higher than expected, but is accurate for determining 0, 1, 2+
		Int lastTeam = -1;
		if (TheGameInfo)
		{
			for (int i=0; i<MAX_SLOTS; ++i)
			{
				const GameSlot *slot = TheGameInfo->getConstSlot(i);
				if (slot->isOccupied() && slot->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER)
				{
					if (slot->getTeamNumber() == -1 || slot->getTeamNumber() != lastTeam)
					{
						++numTeams;
						lastTeam = slot->getTeamNumber();
					}
				}
			}
		}

		if (numTeams > 1)
		{
			// add in the multiplayer victory/defeat scripts
			// GeneralsX @bugfix Copilot 22/03/2026 Load shared multiplayer scripts relative to the asset root on Linux.
			AsciiString path = "Data\\Scripts\\MultiplayerScripts.scb";
			CachedFileInputStream theInputStream;
			if (theInputStream.open(path))
			{
				ChunkInputStream *pStrm = &theInputStream;
				DataChunkInput file( pStrm );
				file.registerParser( "PlayerScriptsList", AsciiString::TheEmptyString, ScriptList::ParseScriptsDataChunk );
				if (!file.parse(nullptr)) {
					DEBUG_LOG(("ERROR - Unable to read in multiplayer scripts."));
					return;
				}
				ScriptList *scripts[MAX_PLAYER_COUNT];
				Int count = ScriptList::getReadScripts(scripts);
				if (count)
				{
					ScriptList *pSL = TheSidesList->getSideInfo(0)->getScriptList();
					if (pSL != nullptr)
					{
						Script *next = scripts[0]->getScript();
						while (next)
						{
							Script *dupe = next->duplicate();
							pSL->addScript(dupe, 0);
							next = next->getNext();
						}
					}
				}
				for (Int i=0; i<count; ++i)
				{
					deleteInstance(scripts[i]);
				}
			}
		}

		/*
		// add multiplayer single-player defeat script
		if (numTeams > 1)
		{
			AsciiString name;
			name.format("MultiplayerPlayerDefeat"); // name is for the ui in the editor
			Script *pNewScript = newInstance(Script);
			pNewScript->setName(name);

			Condition *pFalse1 = newInstance(Condition)(Condition::MULTIPLAYER_PLAYER_DEFEAT);
			OrCondition *pOr = newInstance(OrCondition);
			pOr->setFirstAndCondition(pFalse1);
			pNewScript->setOrCondition(pOr);

			ScriptAction *action = newInstance(ScriptAction)(ScriptAction::LOCALDEFEAT);
			pNewScript->setAction(action);

			ScriptList *pSL = TheSidesList->getSideInfo(0)->getScriptList();
			pSL->addScript(pNewScript, 0); // adds this as the player's first script (index 0)
		}

		// add multiplayer victory scripts
		if (numTeams > 1)
		{
			AsciiString name;
			name.format("MultiplayerVictory"); // name is for the ui in the editor
			Script *pNewScript = newInstance( Script );
			pNewScript->setName(name);

			Condition *pFalse1 = newInstance(Condition)(Condition::MULTIPLAYER_ALLIED_VICTORY);
			OrCondition *pOr = newInstance(OrCondition);
			pOr->setFirstAndCondition(pFalse1);
			pNewScript->setOrCondition(pOr);

			ScriptAction *action = newInstance(ScriptAction)(ScriptAction::VICTORY);
			pNewScript->setAction(action);

			ScriptList *pSL = TheSidesList->getSideInfo(0)->getScriptList();
			pSL->addScript(pNewScript, 0); // adds this as the player's first script (index 0)
		}

		// add multiplayer defeat scripts
		if (numTeams > 1)
		{
			AsciiString name;
			name.format("MultiplayerDefeat"); // name is for the ui in the editor
			Script *pNewScript = newInstance(Script);
			pNewScript->setName(name);

			Condition *pFalse1 = newInstance(Condition)(Condition::MULTIPLAYER_ALLIED_DEFEAT);
			OrCondition *pOr = newInstance(OrCondition);
			pOr->setFirstAndCondition(pFalse1);
			pNewScript->setOrCondition(pOr);

			ScriptAction *action = newInstance(ScriptAction)(ScriptAction::DEFEAT);
			pNewScript->setAction(action);

			ScriptList *pSL = TheSidesList->getSideInfo(0)->getScriptList();
			pSL->addScript(pNewScript, 0); // adds this as the player's first script (index 0)
		}
		*/
	}

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_VICTORY_CONDITION_SETUP);

	Player *localPlayer = ThePlayerList->getLocalPlayer();

	// set the radar as on a new map
	TheRadar->newMap( TheTerrainLogic );

	// TheSuperHackers @tweak Force on radar for all observers.
	for (Int i = 0; i < MAX_PLAYER_COUNT; ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (player->isPlayerObserver())
		{
			TheRadar->forceOn(i, TRUE);
		}
	}

	TheInGameUI->setClientQuiet( FALSE ); // okay to start beeping and stuff

	// Tell the multiplayer victory condition singleton that the players are created
	TheVictoryConditions->cachePlayerPtrs();
	TheVictoryConditions->setVictoryConditions(VICTORY_NOBUILDINGS);

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_VICTORY_CONDITION_SET_VICTORY_CONDITION);

	// set the world extents to that of the map
	Region3D extent;
	TheTerrainLogic->getExtent( &extent );

	setWidth( extent.hi.x - extent.lo.x );
	setHeight( extent.hi.y - extent.lo.y );

	// anytime the world's size changes, must reset the partition mgr
	ThePartitionManager->init();
	ThePartitionManager->refreshShroudForLocalPlayer();// Can't do this until after init, and doesn't seem right to do in init

	TheGhostObjectManager->setLocalPlayerIndex(localPlayer->getPlayerIndex());
	TheGhostObjectManager->reset();

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_GHOST_OBJECT_MANAGER_RESET);

	// update the terrain logic now that all is loaded
	TheTerrainLogic->newMap( loadingSaveGame );

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_TERRAIN_LOGIC_NEW_MAP);

	#ifdef DUMP_PERF_STATS
	GetPrecisionTimer(&endTime64);
	sprintf(Buf,"After terrainlogic->newmap=%f",((double)(endTime64-startTime64)/(double)(freq64)*1000.0));
	DEBUG_LOG(("%s", Buf));
	#endif

		// Special case, load any bridge map objects.
	for (MapObject *pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{

		if (pMapObj->getFlag(FLAG_BRIDGE_FLAGS) || pMapObj->getFlag(FLAG_ROAD_FLAGS))
			continue;	// these roads & bridges are special cased in the terrain side.

		// get thing template based from map object name
		const ThingTemplate *thingTemplate = pMapObj->getThingTemplate();
		if( thingTemplate == nullptr )
			continue;

		if (!thingTemplate->isBridgeLike())
			continue;

		Team *team = ThePlayerList->getNeutralPlayer()->getDefaultTeam();
		// create new object in the world
		Object *obj = TheThingFactory->newObject( thingTemplate, team ); //, OBJECT_STATUS_LOADING_FROM_MAP );
		if( obj )
		{
			Coord3D pos = *pMapObj->getLocation();
			pos.z += TheTerrainLogic->getGroundHeight( pos.x, pos.y );

			Real angle = normalizeAngle(pMapObj->getAngle());
			obj->setOrientation(angle);
			obj->setPosition( &pos );
			if (thingTemplate->isBridge()) {
				TheTerrainLogic->addLandmarkBridgeToLogic(obj);
			}
			if (thingTemplate->isKindOf(KINDOF_WALK_ON_TOP_OF_WALL)) {
				TheAI->pathfinder()->addWallPiece(obj);
			}

			// Do this after positioning the object, because we may place objects in response to keys
			// in the map object properties.
			// update this object instance with properties from the map object
			obj->updateObjValuesFromMapProperties( pMapObj->getProperties() );
		}

	}

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_BRIDGE_LOAD);

	// refresh the radar to reflect loaded bridges
	TheRadar->refreshTerrain( TheTerrainLogic );

	// tell the AI about it
	// Note that it is important that the pathfinder be called before the map objects are loaded.
	TheAI->pathfinder()->newMap();

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_PATHFINDER_NEW_MAP);

	// reveal the map for the permanent observer
	Player *observerPlayer = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey("ReplayObserver"));
	ThePartitionManager->revealMapForPlayerPermanently( observerPlayer->getPlayerIndex() );
	DEBUG_LOG(("Reveal shroud for %ls whose index is %d", observerPlayer->getPlayerDisplayName().str(), observerPlayer->getPlayerIndex()));

	if (TheGameInfo)
	{
		for (int i=0; i<MAX_SLOTS; ++i)
		{
			GameSlot *slot = TheGameInfo->getSlot(i);

			if (!slot || !slot->isOccupied())
				continue;

			AsciiString playerName;
			playerName.format("player%d", i);
			Player *player = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(playerName));

			if (slot->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER)
			{
				DEBUG_LOG(("Clearing shroud for observer %s in playerList slot %d", playerName.str(), player->getPlayerIndex()));
				ThePartitionManager->revealMapForPlayerPermanently( player->getPlayerIndex() );
			}
			else
			{
				// remove shroud for the player in MP games
				if (!TheMultiplayerSettings->isShroudInMultiplayer())
					ThePartitionManager->revealMapForPlayer( player->getPlayerIndex() );
			}
		}
	}

	#ifdef DUMP_PERF_STATS
	GetPrecisionTimer(&endTime64);
	sprintf(Buf,"Before loading objects=%f",((double)(endTime64-startTime64)/(double)(freq64)*1000.0));
	DEBUG_LOG(("%s", Buf));
	#endif

	if( loadingSaveGame == FALSE )
	{
		Int progressCount = LOAD_PROGRESS_LOOP_ALL_THE_FREAKN_OBJECTS;
		Int timer = timeGetTime();
		for (MapObject *pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
		{

			if (pMapObj->getFlag(FLAG_BRIDGE_FLAGS) || pMapObj->getFlag(FLAG_ROAD_FLAGS)) {
				continue;	// roads & bridges are special cased in the terrain side.
			}
			//Kris: Added this function so that we can rename objects and preserve them. If the patch
			//      entry is missing for a particular unit, it doesn't get added to the world.
			handleNameChange( pMapObj );

			// get thing template based from map object name
			const ThingTemplate *thingTemplate = pMapObj->getThingTemplate();

			//
			// if no template continue, some map objects don't have thing templates like
			// lights (handled in the device).  Objects that are test objects have a
			// test string (designated with *** and the define TEST_STRING) and will
			// have temporary templates created 'on the fly' during a
			// ThingFactory->findTemplate() call when loading from the map file
			//
			if( thingTemplate == nullptr )
				continue;

			if (thingTemplate->isBridgeLike())
				continue; // bridges have to be added earlier.

			Bool useTrees = TheGlobalData->m_useTrees;
			if (TheRecorder && TheRecorder->isMultiplayer()) {
				useTrees = TRUE; // Always use trees in multiplayer, cause we want it to sync properly. jba.
			}
			// don't create trees and shrubs if this is one and we have that option off
			if( thingTemplate->isKindOf( KINDOF_SHRUBBERY ) && !useTrees )
				continue;

			// Get the team information
			DEBUG_ASSERTCRASH(pMapObj->getProperties()->getType(TheKey_originalOwner) == Dict::DICT_ASCIISTRING, ("unit %s has no original owner specified (obsolete map file)",pMapObj->getName().str()));
			AsciiString originalOwner = pMapObj->getProperties()->getAsciiString(TheKey_originalOwner);
			Team *team = ThePlayerList->validateTeam(originalOwner);

			// create new object in the world
			Object *obj = TheThingFactory->newObject( thingTemplate, team ); //, OBJECT_STATUS_LOADING_FROM_MAP );
			if( obj )
			{
				Coord3D pos = *pMapObj->getLocation();
				pos.z += TheTerrainLogic->getGroundHeight( pos.x, pos.y );

				if(pMapObj->getFlag(FLAG_DRAWS_IN_MIRROR) || obj->isKindOf(KINDOF_CAN_CAST_REFLECTIONS))
				{
					Drawable* draw = obj->getDrawable();
					if(draw)
						draw->setDrawableStatus( DRAWABLE_STATUS_DRAWS_IN_MIRROR );
				}

				Real angle = normalizeAngle(pMapObj->getAngle());
				obj->setOrientation(angle);
				obj->setPosition( &pos );

				// Do this after positioning the object, because we may place objects in response to keys
				// in the map object properties.
				// update this object instance with properties from the map object
				obj->updateObjValuesFromMapProperties( pMapObj->getProperties() );

				PathfindLayerEnum	layer = TheTerrainLogic->getLayerForDestination(&pos);
				obj->setLayer(layer);
				// Now onCreates were called at the constructor.  This magically created
				// thing needs to be considered as Built for Game specific stuff.
				for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
				{
					CreateModuleInterface* create = (*m)->getCreate();
					if (!create)
						continue;
					create->onBuildComplete();
				}

				// Since the team now has members, activate it.
				team->setActive();
				TheAI->pathfinder()->addObjectToPathfindMap( obj );

			}

			if(timeGetTime() > timer + 500)
			{
				if(progressCount < LOAD_PROGRESS_MAX_ALL_THE_FREAKN_OBJECTS)
					progressCount ++;
				updateLoadProgress(progressCount);
				timer = timeGetTime();
			}

		}

	}

	#ifdef DUMP_PERF_STATS
	GetPrecisionTimer(&endTime64);
	sprintf(Buf,"After loading objects=%f",((double)(endTime64-startTime64)/(double)(freq64)*1000.0));
	DEBUG_LOG(("%s", Buf));
	#endif

	// place initial network buildings/units
	if (TheGameInfo && !loadingSaveGame)
	{
		Int progressCount = LOAD_PROGRESS_LOOP_INITIAL_NETWORK_BUILDINGS;
		for (int i=0; i<MAX_SLOTS; ++i)
		{
			GameSlot *slot = TheGameInfo->getSlot(i);

			if (!slot || !slot->isOccupied())
				continue;

			AsciiString playerName;
			playerName.format("player%d", i);
			Player *player = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(playerName));

			if (slot->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER)
			{
				slot->setPlayerTemplate(0);
				const PlayerTemplate *pt = ThePlayerTemplateStore->findPlayerTemplate( TheNameKeyGenerator->nameToKey("FactionObserver") );
				if (pt)
				{
					for (Int j=0; j<ThePlayerTemplateStore->getPlayerTemplateCount(); ++j)
					{
						if (pt == ThePlayerTemplateStore->getNthPlayerTemplate(j))
						{
							slot->setPlayerTemplate(j);
							break;
						}
					}
				}
				DEBUG_LOG(("Setting observer's playerTemplate to %d in slot %d", slot->getPlayerTemplate(), i));
			}
			else
			{
				const PlayerTemplate* pt = nullptr;
				pt = ThePlayerTemplateStore->getNthPlayerTemplate(slot->getPlayerTemplate());

				placeNetworkBuildingsForPlayer(i, slot, player, pt);
			}

			updateLoadProgress(progressCount++);

		}
	}
	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_INITIAL_NETWORK_BUILDINGS);

	//
	// tell the client to pre-load some assets that we will use such as faction things we
	// will build and various damage states for all the structures on the map so that we
	// don't have big pauses when building those objects or switching to those states
	//
	if( TheGlobalData->m_preloadAssets )
	{
		if (TheGlobalData->m_preloadEverything)
		{
			for (Int td = TIME_OF_DAY_FIRST; td < TIME_OF_DAY_COUNT; ++td)
				TheGameClient->preloadAssets((TimeOfDay)td);
		}
		else
		{
			TheGameClient->preloadAssets( TheGlobalData->m_timeOfDay );
		}
	}

	//put this here somewhat randomly.
	TheControlBar->hideCommunicator( FALSE );

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_PRELOAD_ASSETS);

	// GeneralsX @tweak Copilot 23/03/2026 Keep camera defaults aligned after resolution/aspect changes.
	TheTacticalView->setCameraHeightAboveGroundLimitsToDefault();
	TheTacticalView->setAngleToDefault();
	TheTacticalView->setPitchToDefault();
	TheTacticalView->setZoomToDefault();
	TheTacticalView->setZoomToMax();

	if( TheRecorder )
		TheRecorder->initControls();

	// Note - WorldBuilderDoc.cpp also uses initial camera position, so if changed, update both.  jba
	// Note - We construct the multiplayer start spot name manually here, so change this if you
	//        change TheKey_Player_1_Start etc.  mdc
	AsciiString startingCamName = TheNameKeyGenerator->keyToName(TheKey_InitialCameraPosition);
	if (TheGameInfo)
	{
		GameSlot *slot = TheGameInfo->getSlot(localSlot);
		DEBUG_ASSERTCRASH(slot, ("Starting a LAN game without ourselves!"));

		if (slot->isHuman())
		{
			Int startPos = slot->getStartPos();
			startingCamName.format("Player_%d_Start", startPos+1); // start pos waypoints are 1-based
			DEBUG_LOG(("Using %s as the multiplayer initial camera position", startingCamName.str()));
		}
	}

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_STARTING_CAMERA);

	Waypoint *way = findNamedWaypoint(startingCamName);
	if (way)
	{
		Coord3D pos = *way->getLocation();
		TheTacticalView->lookAt( &pos );
	}
	else
	{
		// Just look somewhere.  lookAt does some terrain specific setup, so it is good
		// to call it.  jba
		Coord3D pos;
		pos.x = 50;
		pos.y = 50;
		pos.z = 0;
		TheTacticalView->lookAt( &pos );
		DEBUG_LOG(("Failed to find initial camera position waypoint %s", startingCamName.str()));
	}

	// Set up the camera height based on the map height & globalData.
	TheTacticalView->initHeightForMap();
	TheTacticalView->setAngleToDefault();
	TheTacticalView->setPitchToDefault();
	TheTacticalView->setZoomToDefault();

	// update the loadscreen
	updateLoadProgress(LOAD_PROGRESS_POST_STARTING_CAMERA_2);

	// update partition info - We need to do the initial update so that it can be queried
	// during the first frame.  jba.
	ThePartitionManager->update();

	#ifdef DUMP_PERF_STATS
	GetPrecisionTimer(&endTime64);
	sprintf(Buf,"After partition manager update=%f",((double)(endTime64-startTime64)/(double)(freq64)*1000.0));
	DEBUG_LOG(("%s", Buf));
	#endif


	// final step, run newMap for all players
	if( loadingSaveGame == FALSE )
		ThePlayerList->newMap();

	// reset all the skill points in a single player game
	if(loadingSaveGame == FALSE && isInSinglePlayerGame())
	{
		for (Int i=0; i<MAX_PLAYER_COUNT; ++i)
		{
			Player *pPlayer = ThePlayerList->getNthPlayer(i);
			if (pPlayer && pPlayer->getPlayerType() != PLAYER_HUMAN)
				pPlayer = nullptr;

			if (pPlayer)
			{
				pPlayer->addSkillPoints(m_rankPointsToAddAtGameStart);
				DEBUG_LOG(("GameLogic::startNewGame() - adding m_rankPointsToAddAtGameStart==%d to player %d(%ls)",
					m_rankPointsToAddAtGameStart, i, pPlayer->getPlayerDisplayName().str()));
			}
		}
	}

	updateLoadProgress(LOAD_PROGRESS_END);

	if(isInMultiplayerGame() && TheNetwork)
	{
		TheNetwork->loadProgressComplete();
		TheNetwork->liteupdate();
	}

	while(!isProgressComplete())
	{
		updateLoadProgress(101); // keep greater then 100
		testTimeOut();
		Sleep(100);
	}

	// if we're in a load game, don't fade yet
	if(loadingSaveGame == FALSE && TheTransitionHandler != nullptr && m_loadScreen)
	{
		TheTransitionHandler->setGroup("FadeWholeScreen");
		while(!TheTransitionHandler->isFinished())
		{
			TheWindowManager->update();
			if(!TheTransitionHandler->isFinished())
			{
				TheDisplay->draw();
				setFPMode();
				TheFramePacer->update();
				// GeneralsX @bugfix Codex 10/07/2026 Yield during startup fades when frame limiting is disabled.
				if (!TheFramePacer->isActualFramesPerSecondLimitEnabled())
				{
					Sleep(1);
				}
			}

		}
	}

	if(m_loadScreen)
	{
		TheMouse->setVisibility(TRUE);

/*
		//
		// delete load screen only when not loading a save game, for save games we still
		// have more work to do and the load screen will be deleted elsewhere after
		// we're all done with the load game progress
		//
		if( loadingSaveGame == FALSE )
*/
			deleteLoadScreen();

	}

	#ifdef DUMP_PERF_STATS
	GetPrecisionTimer(&endTime64);
	sprintf(Buf,"After delete load screen=%f",((double)(endTime64-startTime64)/(double)(freq64)*1000.0));
	DEBUG_LOG(("%s", Buf));
	#endif

	if(m_gameMode == GAME_SHELL)
	{
		if (!TheGlobalData->m_headless)
		{
			if(TheShell->getScreenCount() == 0)
				TheShell->push( "Menus/MainMenu.wnd" );
			else if (TheShell->top())
			{
				TheShell->top()->hide(FALSE);
				TheShell->top()->bringForward();
			}
			HideControlBar();
		}
	}
	else
	{

//		TheShell->hideShell();
		if(TheStatsCollector)
		{
			TheStatsCollector->reset();
		}
		else if( TheGlobalData->m_playStats > 0)
		{
			TheStatsCollector = NEW StatsCollector;
			TheStatsCollector->reset();
		}

///		ShowControlBar(FALSE);

		// explicitly set the Control bar to Observer Mode
		if(m_gameMode == GAME_REPLAY )
		{
			rts::changeLocalPlayer(observerPlayer);

			DEBUG_LOG(("Start of a replay game %ls, %d", localPlayer->getPlayerDisplayName().str(), localPlayer->getPlayerIndex()));
		}
		else
		{
			TheControlBar->setControlBarSchemeByPlayer(localPlayer);
			TheControlBar->initSpecialPowershortcutBar(localPlayer);
		}
//		ShowControlBar();

	}
	TheTacticalView->setOkToAdjustHeight(TRUE);

// If defined, the game times various units.
#ifdef DO_UNIT_TIMINGS
	g_UT_curThing = TheThingFactory->firstTemplate();
	g_UT_startTiming = true;
	g_UT_gotUnit = false;
	g_UT_timingLog = fopen("TimingLog.txt", "w");
	g_UT_commaLog = fopen("TimingCDL.txt", "w");
	fputs("Full,100*ms,NoPart-NoSpawn,,No Spawn,100*ms,Logic,100*ms,Thing,Model,Kind,Side,DrawCalls All,DrawCalls NoPart-NoSpawn,DrawCalls NoSpawn\n", g_UT_commaLog);

	// Turn off shadows
	TheWritableGlobalData->m_useShadowVolumes = false;
#ifdef DEBUG_CRASHING
	TheWritableGlobalData->m_debugIgnoreAsserts = TRUE;
#endif

	// Just look somewhere.  lookAt does some terrain specific setup, so it is good
	// to call it.  jba
	Coord3D thePos;
	thePos.x = 50;
	thePos.y = 50;
	thePos.z = 0;
		TheTacticalView->lookAt( &thePos );
#endif


	// @todo remove this hack
//	TheGlobalData->m_inGame = TRUE;

	// If we are now starting a multiplayer or skirmish game, let us set the local players selectionto be the command center
	// We'll ask the Recorder, so we survive replays
	if( TheRecorder->isMultiplayer() )
	{
		// Iterate through each player's objects, and ask if the object
		//is a command center, and if so, select it for that player
		for (Int i = 0; i < MAX_PLAYER_COUNT; ++i)
		{
			Player *player = ThePlayerList->getNthPlayer(i);
			if (player && player->isPlayerActive())
			{
				// we need to iterate their objects, and select the first Command Center we find
				Bool alreadyFound = FALSE;
				player->iterateObjects(findAndSelectCommandCenter, &alreadyFound);
			}
		}
	}

	if(m_gameMode == GAME_SHELL)
	{
		HideControlBar();
	}
	else
	{
		ShowControlBar(FALSE);
	}

#ifdef DO_UNIT_TIMINGS
	// Turn off the UI
	HideControlBar();
#endif
	TheWritableGlobalData->m_loadScreenRender = FALSE;	///< mark to resume rendering as normal

	// if we're in a gamespy game, mark us as playing
	if (TheGameSpyBuddyMessageQueue && TheGameSpyGame && isInInternetGame())
	{
		BuddyRequest req;
		req.buddyRequestType = BuddyRequest::BUDDYREQUEST_SETSTATUS;
		req.arg.status.status = GP_PLAYING;
		strcpy(req.arg.status.statusString, "Playing");
		strlcpy(req.arg.status.locationString, WideCharStringToMultiByte(TheGameSpyGame->getGameName().str()).c_str(),
			ARRAY_SIZE(req.arg.status.locationString));
		TheGameSpyBuddyMessageQueue->addRequest(req);
	}

	//ReAllows quit menu to work during loading scene
	//setGameLoading(FALSE);
	setLoadingMap( FALSE );

#ifdef DUMP_PERF_STATS
	GetPrecisionTimer(&endTime64);
	sprintf(Buf,"Total startnewgame=%f",((double)(endTime64-startTime64)/(double)(freq64)*1000.0));
	DEBUG_LOG(("%s", Buf));
#endif

	//Assume that getting this far means we've successfully entered an online game.
	//Add an additional disconnection to player stats in case he doesn't complete this game. -MW
	if (TheGameSpyInfo)
		TheGameSpyInfo->updateAdditionalGameSpyDisconnections(1);


  if ( isInReplayGame() && TheInGameUI && TheGameText )
  {
		TheInGameUI->messageNoFormat( TheGameText->FETCH_OR_SUBSTITUTE( "GUI:FastForwardInstructions", L"Press F to toggle Fast Forward" ) );
  }

#ifdef PROFILER_ENABLED
	AsciiString message;
	message.format("GameStart: %s", TheGlobalData->m_mapName.str());
	PROFILER_MSG(message.str(), message.getLength());
#endif
}

//-----------------------------------------------------------------------------------------
static void findAndSelectCommandCenter(Object *obj, void* alreadyFound)
{
	if (!((*(Bool*)alreadyFound)) && obj->isKindOf(KINDOF_COMMANDCENTER) )
	{
		((*(Bool*)alreadyFound)) = TRUE;
		TheGameLogic->selectObject(obj, TRUE, obj->getControllingPlayer()->getPlayerMask(), obj->isLocallyControlled());

	}
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------





// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::loadMapINI( AsciiString mapName )
{

	if (!TheMapCache) {
		// Need the map cache to get the map and user map directories.
		return;
	}

	//
	// if map name begins with a "SAVE_DIRECTORY\", then the map refers to a map
	// that has been extracted from a save game file ... in that case we need to get
	// the pristine map name string in order to manipulate and load the right map.ini
	// for that map from it's original location
	//
	const char* pristineMapName = TheGameState->isInSaveDirectory(mapName.str())
		? TheGameState->getSaveGameInfo()->pristineMapName.str()
		: mapName.str();

	char filename[_MAX_PATH];
	strlcpy(filename, pristineMapName, ARRAY_SIZE(filename));

	// sanity
	int length = strlen(filename);
	if (length < 4) {
		return;
	}

	// back up over the ".map" extension and to the first directory separator
	char *extension = filename + length - 4;
	while ((extension > filename) && (*extension != '\\') && (*extension != '/')) {
		--extension;
	}
	*extension = 0;


	char fullFledgeFilename[_MAX_PATH];
	snprintf(fullFledgeFilename, ARRAY_SIZE(fullFledgeFilename), "%s\\map.ini", filename);
	if (TheFileSystem->doesFileExist(fullFledgeFilename)) {
		DEBUG_LOG(("Loading map.ini"));
		INI ini;
		ini.load( AsciiString(fullFledgeFilename), INI_LOAD_CREATE_OVERRIDES, nullptr );
	}

	// TheSuperHackers @todo Implement ini load directory for map folder.
	// Requires adjustments in map transfer.

	snprintf(fullFledgeFilename, ARRAY_SIZE(fullFledgeFilename), "%s\\solo.ini", filename);
	if (TheFileSystem->doesFileExist(fullFledgeFilename)) {
		DEBUG_LOG(("Loading solo.ini"));
		INI ini;
		ini.load( AsciiString(fullFledgeFilename), INI_LOAD_CREATE_OVERRIDES, nullptr );
	}

	// No error here. There could've just *not* been a map.ini file.

	// now look for a string file
	snprintf(fullFledgeFilename, ARRAY_SIZE(fullFledgeFilename), "%s\\map.str", filename);

	if (TheFileSystem->doesFileExist(fullFledgeFilename)) {
		TheGameText->initMapStringFile(fullFledgeFilename);
	}

	// we want to do this before doing the actual map load!
	if (TheDisplay)
	{
		const char* ASSET_USAGE_FILE_NAME = "AssetUsage.txt";
		snprintf(fullFledgeFilename, ARRAY_SIZE(fullFledgeFilename), "%s\\%s", filename, ASSET_USAGE_FILE_NAME);
		// note: call this EVEN IF THE FILE IN QUESTION DOES NOT EXIST.
		TheDisplay->doSmartAssetPurgeAndPreload(fullFledgeFilename);
	}

}

// ------------------------------------------------------------------------------------------------
/** Process the destroy list, destroying all pending objects.
 * The destroy list exists to ensure that all objects have a chance to
 * see each other at each simulation frame - the object list is the
 * same at the start of the update as it is at the end of the update. */
// ------------------------------------------------------------------------------------------------
//DECLARE_PERF_TIMER(processDestroyList)
void GameLogic::processDestroyList()
{
	//USE_PERF_TIMER(processDestroyList)

	for( ObjectPointerListIterator iterator = m_objectsToDestroy.begin(); iterator != m_objectsToDestroy.end(); iterator++ )
	{
		Object* currentObject = (*iterator);

#ifdef ALLOW_NONSLEEPY_UPDATES
		for (std::list<UpdateModulePtr>::iterator it = m_normalUpdates.begin(); it != m_normalUpdates.end(); /* nothing */)
		{
			if ((*it)->friend_getObject() == currentObject)
			{
				it = m_normalUpdates.erase(it);
			}
			else
			{
				++it;
			}
		}
#endif

		/*
			this looks odd, but is necessary; since erasing a single entry can shuffle others in the list
			(in order to maintain its heap-ness), we must do two passes: one to find the updates for this
			object, another to actually erase 'em.

			(in case you're wondering: yes, this is still more efficient than just deleting them
			and rebalancing the entire heap afterwards, at least for real-world maps, since an individual
			rebalance is O(log N) and a full rebalance is O(N)... so unless you are deleting the majority
			of the objects in the world every frame, we come out well ahead this way.)
		*/

		const Int MAX_SUO = 256;
		UpdateModulePtr sleepyUpdatesForThisObject[MAX_SUO];
		Int numSUO = 0;

		for (std::vector<UpdateModulePtr>::iterator it2 = m_sleepyUpdates.begin(); it2 != m_sleepyUpdates.end(); ++it2)
		{
			UpdateModulePtr u = *it2;
			if (u->friend_getObject() == currentObject && numSUO < MAX_SUO)
			{
				sleepyUpdatesForThisObject[numSUO++] = u;
			}
		}

		for (--numSUO; numSUO >= 0; --numSUO)
		{
			// have to re-get idx each time since each call to erase might change others.
			Int idx = sleepyUpdatesForThisObject[numSUO]->friend_getIndexInLogic();
			DEBUG_ASSERTCRASH(m_sleepyUpdates[idx] == sleepyUpdatesForThisObject[numSUO], ("Hmm, expected update mismatch here"));
			eraseSleepyUpdate(idx);
			DEBUG_ASSERTCRASH(sleepyUpdatesForThisObject[numSUO]->friend_getIndexInLogic() == -1, ("Hmm, expected index to be -1 here"));
		}

		currentObject->removeFromList(&m_objList);//remove from object list

		// remove object from lookup table
		removeObjectFromLookupTable( currentObject );

		Object::friend_deleteInstance(currentObject);//actual delete
	}

	m_objectsToDestroy.clear();//list full of bad pointers now, clear it.  If anyone's deletion resulted
	//in the request for a new deletion (sub-object), the new object was added to the end of this list.
}

//-------------------------------------------------------------------------------------------------
/** Process the command list passed to the logic from the network */
//-------------------------------------------------------------------------------------------------
void GameLogic::processCommandList( CommandList *list )
{
	m_cachedCRCs.clear();
	m_shouldValidateCRCs = FALSE;

	GameMessage* msg;

	for( msg = list->getFirstMessage(); msg; msg = msg->next() )
	{
#ifdef RTS_DEBUG
		DEBUG_ASSERTCRASH(msg != nullptr && msg != (GameMessage*)0xdeadbeef, ("bad msg"));
#endif
		logicMessageDispatcher( msg, nullptr );
	}

	if (m_shouldValidateCRCs && !TheNetwork->sawCRCMismatch())
	{
		Bool sawCRCMismatch = FALSE;
		Int numPlayers = 0;
		DEBUG_ASSERTCRASH(TheNetwork, ("No Network!"));
		if (TheNetwork)
		{
			for (Int i=0; i<MAX_SLOTS; ++i)
			{
				if (TheNetwork->isPlayerConnected(i))
					++numPlayers;
			}

			if (m_cachedCRCs.size() < numPlayers)
			{
				DEBUG_CRASH(("Not enough CRCs!"));
				sawCRCMismatch = TRUE;
			}
			else
			{
				Bool hasReferenceCRC = FALSE;
				UnsignedInt referenceCRC = 0;

				for (CachedCRCMap::const_iterator it = m_cachedCRCs.begin(); it != m_cachedCRCs.end(); ++it)
				{
					// TheSuperHackers @bugfix Caball009 14/06/2026 Check if player is still connected,
					// to avoid spurious mismatches at low CRC intervals, e.g. every frame.
					// GeneralsX @bugfix felipebraz 05/07/2026 Use pre-computed slot index.
					const Int slotIndex = ThePlayerList->getSlotIndex(it->first);
					if (slotIndex >= 0 && !TheNetwork->isPlayerConnected(slotIndex))
						continue;
					const UnsignedInt crc = it->second;

					if (!hasReferenceCRC)
					{
						hasReferenceCRC = TRUE;
						referenceCRC = crc;
						continue;
					}

					if (referenceCRC != crc)
					{
						DEBUG_CRASH(("CRC mismatch!"));
						sawCRCMismatch = TRUE;
					}
				}
			}
		}

		if (sawCRCMismatch)
		{
#ifdef DEBUG_LOGGING
			DEBUG_LOG(("CRC Mismatch - saw %d CRCs from %d players", m_cachedCRCs.size(), numPlayers));
			for (CachedCRCMap::const_iterator crcIt = m_cachedCRCs.begin(); crcIt != m_cachedCRCs.end(); ++crcIt)
			{
				Player *player = ThePlayerList->getNthPlayer(crcIt->first);
				DEBUG_LOG(("CRC from player %d (%ls) = %X", crcIt->first,
					player?player->getPlayerDisplayName().str():L"<NONE>", crcIt->second));
			}
#endif // DEBUG_LOGGING
			TheNetwork->setSawCRCMismatch();
		}
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool GameLogic::isIntroMoviePlaying()
{
	/// @todo remove this hack
	return m_startNewGame && TheDisplay->isMoviePlaying();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::selectObject(Object *obj, Bool createNewSelection, PlayerMaskType playerMask, Bool affectClient)
{
	if (!obj) {
		return;
	}

	if (!obj->isMassSelectable() && !createNewSelection) {
		DEBUG_LOG(("GameLogic::selectObject() - Object attempted to be added to selection, but isn't mass-selectable."));
		return;
	}

	while (playerMask) {
		Player *player = ThePlayerList->getEachPlayerFromMask(playerMask);
		if (!player) {
			return;
		}

		CRCGEN_LOG(( "Creating AIGroup in GameLogic::selectObject()" ));
		AIGroupPtr group = TheAI->createGroup();
		group->add(obj);

		// add all selected agents to the AI group
		if (createNewSelection)	{
#if RETAIL_COMPATIBLE_AIGROUP
			player->setCurrentlySelectedAIGroup(group);
#else
			player->setCurrentlySelectedAIGroup(group.Peek());
#endif
		} else {
#if RETAIL_COMPATIBLE_AIGROUP
			player->addAIGroupToCurrentSelection(group);
#else
			player->addAIGroupToCurrentSelection(group.Peek());
#endif
		}

#if RETAIL_COMPATIBLE_AIGROUP
		TheAI->destroyGroup(group);
#else
		group->removeAll();
#endif

		if (affectClient) {
			Drawable *draw = obj->getDrawable();
			if (draw) {
				TheInGameUI->selectDrawable(draw);
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::deselectObject(Object *obj, PlayerMaskType playerMask, Bool affectClient)
{
	if (!obj) {
		return;
	}

	while (playerMask) {
		Player *player = ThePlayerList->getEachPlayerFromMask(playerMask);
		if (!player) {
			return;
		}

		CRCGEN_LOG(( "Removing a unit from a selected group in GameLogic::deselectObject()" ));
		AIGroupPtr group = TheAI->createGroup();
#if RETAIL_COMPATIBLE_AIGROUP
		player->getCurrentSelectionAsAIGroup(group);
#else
		player->getCurrentSelectionAsAIGroup(group.Peek());
#endif

		if (group) {
			Bool emptied = group->remove(obj);

			// Set this to be the currently selected group.
			if (!emptied) {
#if RETAIL_COMPATIBLE_AIGROUP
				player->setCurrentlySelectedAIGroup(group);
				// Then, cleanup the group.
				TheAI->destroyGroup(group);
#else
				player->setCurrentlySelectedAIGroup(group.Peek());
				// Then, cleanup the group.
				group->removeAll();
#endif
			} else {
				// nullptr will clear the group.
				player->setCurrentlySelectedAIGroup(nullptr);
			}

			if (affectClient) {
				Drawable *draw = obj->getDrawable();
				if (draw) {
					TheInGameUI->deselectDrawable(draw);
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
inline void GameLogic::validateSleepyUpdate() const
{
// pretty slow, so do only for DEBUG_CRASHING for now. turn on if you suspect wonkiness.
#ifdef DEBUG_CRASHING
	#define SLEEPY_DEBUG
#endif
#ifdef SLEEPY_DEBUG
	int sz = m_sleepyUpdates.size();
	if (sz == 0)
		return;

	int i;
	//DEBUG_LOG(("\n"));
	//for (i = 0; i < sz; ++i)
	//{
	//	DEBUG_LOG(("u %04d: %08lx %08lx",i,m_sleepyUpdates[i],m_sleepyUpdates[i]->friend_getNextCallFrame()));
	//}
	for (i = 0; i < sz; ++i)
	{
		DEBUG_ASSERTCRASH(m_sleepyUpdates[i]->friend_getIndexInLogic() == i, ("index mismatch: expected %d, got %d",i,m_sleepyUpdates[i]->friend_getIndexInLogic()));
		UnsignedInt pri = m_sleepyUpdates[i]->friend_getPriority();
		if (i > 0)
		{
			Int i0 = (i+1)/2-1;
			UnsignedInt pri0 = m_sleepyUpdates[i0]->friend_getPriority();
			DEBUG_ASSERTCRASH(pri >= pri0, ("sleepyUpdates are munged (0)"));
		}
		Int i1 = 2*(i+1)-1;
		Int i2 = 2*(i+1);
		if (i1 < sz)
		{
			UnsignedInt pri1 = m_sleepyUpdates[i1]->friend_getPriority();
			DEBUG_ASSERTCRASH(pri <= pri1, ("sleepyUpdates are munged (1)"));
		}
		if (i2 < sz)
		{
			UnsignedInt pri2 = m_sleepyUpdates[i2]->friend_getPriority();
			DEBUG_ASSERTCRASH(pri <= pri2, ("sleepyUpdates are munged (2)"));
		}
	}
#endif
}

// ------------------------------------------------------------------------------------------------
void GameLogic::eraseSleepyUpdate(Int i)
{
	USE_PERF_TIMER(SleepyMaintenance)

	DEBUG_ASSERTCRASH(i >= 0 && i < m_sleepyUpdates.size(), ("bad sleepy idx"));

	// swap with the final item, toss the final item, then rebalance
	m_sleepyUpdates[i]->friend_setIndexInLogic(-1);

	Int last = m_sleepyUpdates.size() - 1;
	if (i < last)
	{
		m_sleepyUpdates[i] = m_sleepyUpdates[last];
		m_sleepyUpdates[i]->friend_setIndexInLogic(i);
		m_sleepyUpdates.pop_back();
		rebalanceSleepyUpdate(i);
	}
	else
	{
		m_sleepyUpdates.pop_back();
	}
}

// ------------------------------------------------------------------------------------------------
inline Bool isLowerPriority(const UpdateModulePtr a, const UpdateModulePtr b)
{
	// return true iff a is lower pri than b.
	// remember: lower ordinal value means higher priority.
	// therefore, higher ordinal value means lower priority.
	DEBUG_ASSERTCRASH(a && b, ("these may no longer be null"));
	UnsignedInt f1 = a->friend_getPriority();
	UnsignedInt f2 = b->friend_getPriority();
	return f1 > f2;
}

// ------------------------------------------------------------------------------------------------
Int GameLogic::rebalanceParentSleepyUpdate(Int i)
{
	USE_PERF_TIMER(SleepyMaintenance)

	DEBUG_ASSERTCRASH(i >= 0 && i < m_sleepyUpdates.size(), ("bad sleepy idx"));

	Int parent = ((i+1)>>1)-1;
	while (parent >= 0 && isLowerPriority(m_sleepyUpdates[parent], m_sleepyUpdates[i]))
	{
		UpdateModulePtr a = m_sleepyUpdates[parent];
		UpdateModulePtr b = m_sleepyUpdates[i];

		m_sleepyUpdates[i] = a;
		m_sleepyUpdates[parent] = b;

		a->friend_setIndexInLogic(i);
		b->friend_setIndexInLogic(parent);

		i = parent;
		parent = ((parent+1)>>1)-1;
	}

	return i;
}

// ------------------------------------------------------------------------------------------------
Int GameLogic::rebalanceChildSleepyUpdate(Int i)
{
	USE_PERF_TIMER(SleepyMaintenance)

	DEBUG_ASSERTCRASH(i >= 0 && i < m_sleepyUpdates.size(), ("bad sleepy idx"));

// this function gets the brunt of the work (we frequently
// balance down, not up), so this one is hand-unrolled for
// max efficiency. I have left the pristine non-unrolled
// version present for clarity. (Yes, this is worth doing.) (srj)
#if 1
	UpdateModulePtr* pI = &m_sleepyUpdates[i];

	// our children are i*2 and i*2+1
  Int child = ((i)<<1)+1;
	UpdateModulePtr* pChild = &m_sleepyUpdates[0] + child;
	UpdateModulePtr* pSZ = &m_sleepyUpdates[0] + m_sleepyUpdates.size();	// yes, this is off the end.

  while (pChild < pSZ)
	{
		// choose the higher-priority of the two children; we must be higher-pri than that.
		if (pChild < pSZ-1 && isLowerPriority(*pChild, *(pChild+1)))
		{
      ++pChild;
			++child;
		}

		// if we're higher-pri than our children, we're done.
		if (!isLowerPriority(*pI, *pChild))
		{
			break;
		}

		// doh. swap with the highest-pri child we have.
		UpdateModulePtr a = *pChild;
		UpdateModulePtr b = *pI;

		*pI = a;
		*pChild = b;

		a->friend_setIndexInLogic(i);
		b->friend_setIndexInLogic(child);

		i = child;
		pI = pChild;

		child = ((i)<<1)+1;
		pChild = &m_sleepyUpdates[0] + child;
  }
#else
	// our children are i*2 and i*2+1
	Int sz = m_sleepyUpdates.size();
  Int child = ((i)<<1)+1;
  while (child < sz)
	{
		// choose the higher-priority of the two children; we must be higher-pri than that.
		if (child < sz-1 && isLowerPriority(m_sleepyUpdates[child], m_sleepyUpdates[child+1]))
      ++child;

		// if we're higher-pri than our children, we're done.
		if (!isLowerPriority(m_sleepyUpdates[i], m_sleepyUpdates[child]))
		{
			break;
		}

		// doh. swap with the highest-pri child we have.
		UpdateModulePtr a = m_sleepyUpdates[child];
		UpdateModulePtr b = m_sleepyUpdates[i];

		m_sleepyUpdates[i] = a;
		m_sleepyUpdates[child] = b;

		a->friend_setIndexInLogic(i);
		b->friend_setIndexInLogic(child);
		i = child;
		child = ((i)<<1)+1;
  }
#endif
	return i;
}

// ------------------------------------------------------------------------------------------------
void GameLogic::rebalanceSleepyUpdate(Int i)
{
	USE_PERF_TIMER(SleepyMaintenance)

	i = rebalanceParentSleepyUpdate(i);
	i = rebalanceChildSleepyUpdate(i);
}

// ------------------------------------------------------------------------------------------------
void GameLogic::remakeSleepyUpdate()
{
	USE_PERF_TIMER(SleepyMaintenance)

	Int parent = m_sleepyUpdates.size() / 2;
  while (true)
	{
    rebalanceChildSleepyUpdate(parent);
    if (parent == 0)
			break;
    --parent;
  }

	validateSleepyUpdate();
}

// ------------------------------------------------------------------------------------------------
void GameLogic::pushSleepyUpdate(UpdateModulePtr u)
{
	USE_PERF_TIMER(SleepyMaintenance)

	DEBUG_ASSERTCRASH(u != nullptr, ("You may not pass null for sleepy update info"));

	m_sleepyUpdates.push_back(u);
	u->friend_setIndexInLogic(m_sleepyUpdates.size() - 1);

	rebalanceParentSleepyUpdate(m_sleepyUpdates.size()-1);
}

// ------------------------------------------------------------------------------------------------
UpdateModulePtr GameLogic::peekSleepyUpdate() const
{
	USE_PERF_TIMER(SleepyMaintenance)

	UpdateModulePtr u = m_sleepyUpdates.front();
	DEBUG_ASSERTCRASH(u->friend_getIndexInLogic() == 0, ("index mismatch: expected %d, got %d",0,u->friend_getIndexInLogic()));
	return u;
}

// ------------------------------------------------------------------------------------------------
void GameLogic::popSleepyUpdate()
{
	USE_PERF_TIMER(SleepyMaintenance)

	Int sz = m_sleepyUpdates.size();
	if (sz == 0)
	{
		DEBUG_CRASH(("should not happen"));
		return;
	}

	m_sleepyUpdates[0]->friend_setIndexInLogic(-1);
	if (sz > 1)
	{
		m_sleepyUpdates[0] = m_sleepyUpdates[sz-1];
		m_sleepyUpdates[0]->friend_setIndexInLogic(0);
		m_sleepyUpdates.pop_back();
		rebalanceChildSleepyUpdate(0);
	}
	else
	{
		m_sleepyUpdates.pop_back();
	}
}

// ------------------------------------------------------------------------------------------------
// this should be called only by UpdateModule, thanks.
// ------------------------------------------------------------------------------------------------
//DECLARE_PERF_TIMER(friend_awakenUpdateModule)
void GameLogic::friend_awakenUpdateModule(Object* obj, UpdateModulePtr u, UnsignedInt whenToWakeUp)
{
	//USE_PERF_TIMER(friend_awakenUpdateModule)
	UnsignedInt now = getFrame();
	DEBUG_ASSERTCRASH(whenToWakeUp >= now, ("setWakeFrame frame is in the past... are you sure this is what you want?"));

	if (u == m_curUpdateModule)
	{
		DEBUG_CRASH(("You should not call setWakeFrame() from inside your update(), because it will be ignored, in favor of the return code from update."));
		return;
	}

	if (whenToWakeUp == u->friend_getNextCallFrame())
		return;	// my, that was easy

	if ((now > 0) && (u->friend_getNextCallFrame() == now) && (whenToWakeUp == now + 1))
	{
		// subtle but important case: if we already awake, and someone calls
		// setWakeFrame(self, UPDATE_SLEEP_NONE), we don't want to reset our wake frame,
		// since that would prevent us from getting called THIS frame. since UPDATE_SLEEP_NONE
		// really means "wake up as soon as possible", we don't want to change our status
		// if we are already awake. (srj)
		return;
	}

	Int idx = u->friend_getIndexInLogic();
	if (obj->isInList(&m_objList))
	{
		if (idx < 0 || idx >= m_sleepyUpdates.size())
		{
			RELEASE_CRASH("fatal error! sleepy update module illegal index.");
			return;
		}

		if (m_sleepyUpdates[idx] != u)
		{
			RELEASE_CRASH("fatal error! sleepy update module index mismatch.");
			return;
		}

		// update the value.
		u->friend_setNextCallFrame(whenToWakeUp);

		// rebalance.
		rebalanceSleepyUpdate(idx);

		// validate. (harmless except in debug mode)
		validateSleepyUpdate();

		return;
	}
	else
	{
		if (idx != -1)
		{
			RELEASE_CRASH("fatal error! sleepy update module index mismatch.");
			return;
		}

		// this can happen if stuff happens during object initialization. fortunately,
		// it's easy to deal with:
		u->friend_setNextCallFrame(whenToWakeUp);
		return;
	}
}

// ------------------------------------------------------------------------------------------------
#ifdef DO_UNIT_TIMINGS
	enum {TIME_FRAMES=100};
static void unitTimings()
{
	static Int settleFrames = 0;
	static Int timeFrames = 0;
	enum { INFANTRY, VEHICLE, STRUCTURE, OTHER, END};
	static Int unitTypes = INFANTRY;
	AsciiString sides[8];


	Int sideCount = 0;
  #define SINGLE_UNIT "OxFooble."

#define DO_FACTION
#ifdef DO_FACTION
	sides[sideCount++] = "America";
	sides[sideCount++] = "China";
	sides[sideCount++] = "GLA";
#endif

#define DO_CIVILIAN
#ifdef DO_CIVILIAN
	sides[sideCount++] = "Civilian";
#endif

	sides[sideCount] = "";
	static bool veryFirstTime = true;


	static Int side = 0;

	const Int FACTOR = 5; // run at TIME_FRAMES/FACTOR so we dont' die of boredom.  jba.

	static __int64 startTime64;
	static __int64 endTime64,freq64;
	static int drawCallTotal;
	static enum { LOGIC, NO_PARTICLES, NO_SPAWN, ALL} mode;
	static double timeAll, timeAllNoAnim, timeNoPart, timeNoSpawn, timeLogic, timeLogicNoAnim;
	static float drawCallAll,drawCallNoPart,drawCallNoSpawn,drawCallLogic;

	if (settleFrames>0) {
		settleFrames--;
		if (settleFrames>0) return;

		QueryPerformanceCounter((LARGE_INTEGER *)&startTime64);
		QueryPerformanceFrequency((LARGE_INTEGER *)&freq64);
		timeFrames = TIME_FRAMES/FACTOR;

		// reset the draw counter
		drawCallTotal = 0;
		return;
	}
	if (timeFrames>0) {
		drawCallTotal += TheDisplay->getLastFrameDrawCalls();
		timeFrames--;
		if (timeFrames>0) return;

		QueryPerformanceCounter((LARGE_INTEGER *)&endTime64);
		double timeToUpdate = ((double)(endTime64-startTime64) / (double)(freq64));
		timeToUpdate *= FACTOR;



		if (mode == LOGIC) {
			timeLogic = timeToUpdate;
			drawCallLogic = (float)drawCallTotal / (float)(TIME_FRAMES * 100);  // 100 units for TIME_FRAMES

			mode = ALL;
			settleFrames = 10;
			//g_timing_no_anim = true;
			Coord3D thePos;
			thePos.x = 50;
			thePos.y = 50;
			thePos.z = 0;
			TheTacticalView->lookAt( &thePos );
			return;
		}
		if (mode == ALL) {
			timeAll = timeToUpdate;
			drawCallAll = (float)drawCallTotal / (float)(TIME_FRAMES * 100);  // 100 units for TIME_FRAMES

			mode = NO_PARTICLES;
			settleFrames = 10;
			if (TheParticleSystemManager->getParticleCount()>1) {
				TheParticleSystemManager->reset();
				DEBUG_LOG(("Starting noPart - "));
			}
			return;
		}
		if (mode == NO_PARTICLES) {
			timeNoPart = timeToUpdate;
			drawCallNoPart = (float)drawCallTotal / (float)(TIME_FRAMES * 100);  // 100 units for TIME_FRAMES

			mode = NO_SPAWN;
			Object *obj = TheGameLogic->getFirstObject();
			Bool gotSpawn;
			while (obj) {
				if (obj->getTemplate() != g_UT_curThing) {
					TheGameLogic->destroyObject(obj);
					gotSpawn = true;
				}
				obj = obj->getNextObject();
			}
			if (gotSpawn) {
				DEBUG_LOG(("Starting noSpawn - "));
				settleFrames = 10;
				return;
			}
		}
		if (mode==NO_SPAWN) {
			timeNoSpawn = timeToUpdate;
			drawCallNoSpawn = (float)drawCallTotal / (float)(TIME_FRAMES * 100);  // 100 units for TIME_FRAMES
		}
		if (g_UT_curThing==nullptr) return;



		char foo[1024];
		AsciiString thingName = g_UT_curThing->getName();
		if (veryFirstTime) {
			thingName = "No Object";
		}
		sprintf(foo, "\nTime %f, %d ms for 100 %s , noPart %f, noSpawn %f logic %f \n", timeAll,
			(Int)(timeAll*1000/TIME_FRAMES), thingName.str(), timeNoPart,
			timeNoSpawn, timeLogic);
		DEBUG_LOG((foo));
		sprintf(foo, "\nDrawCalls for 100 %s , all %d, noPart %d, noSpawn %d logic %d \n", thingName.str(),
			drawCallAll,drawCallNoPart,drawCallNoSpawn, drawCallLogic);
		DEBUG_LOG((foo));

		if (g_UT_timingLog) {
			fputs(foo, g_UT_timingLog);
		}
		if (g_UT_commaLog) {
			AsciiString type;
			if (unitTypes == INFANTRY) {
				type="Infantry";
			}	else if (unitTypes == VEHICLE) {
				type="Vehicle";
			}	else if (unitTypes == STRUCTURE) {
				type="Structure";
			}	else {
				type="Other";
			}
			AsciiString modelName;
			ModelConditionFlags state;
			state.clear();
			const ModuleInfo& mi = g_UT_curThing->getDrawModuleInfo();
			if (mi.getCount() > 0)
			{
				const ModuleData* mdd = mi.getNthData(0);
				const W3DModelDrawModuleData* md = mdd ? mdd->getAsW3DModelDrawModuleData() : nullptr;
				if (md)
				{
					modelName = md->getBestModelNameForWB(state);
				}
			}
			if (veryFirstTime) {
				modelName = "**NO MODEL**";
				veryFirstTime = false;
			}
			sprintf(foo, "%f,%d,%f,%d,%f,%d,%f,%d,%s,%s,%s,%s,%f,%f,%f\n", timeAll,
			(Int)(timeAll*1000/TIME_FRAMES),timeNoPart,
			(Int)(timeNoPart*1000/TIME_FRAMES),timeNoSpawn,
			(Int)(timeNoSpawn*1000/TIME_FRAMES),timeLogic,
			(Int)(timeLogic*1000/TIME_FRAMES), thingName.str(), modelName.str(), type.str(),
			sides[side].str(),
			drawCallAll,drawCallNoPart,drawCallNoSpawn);
			fputs(foo, g_UT_commaLog);
		}
		TheParticleSystemManager->reset();
		g_UT_gotUnit = false;
	}

#ifdef SINGLE_UNIT
	if (g_UT_curThing && g_UT_startTiming) {
		if (g_UT_curThing->getName()==SINGLE_UNIT) {
			return;
		}
		while (g_UT_curThing->friend_getNextTemplate()
			&& g_UT_curThing->friend_getNextTemplate()->getName()!=SINGLE_UNIT)
			g_UT_curThing = g_UT_curThing->friend_getNextTemplate();

	}
#endif

	Object *obj = TheGameLogic->getFirstObject();
	while (obj) {
		TheGameLogic->destroyObject(obj);
		obj = obj->getNextObject();
	}

	if (g_UT_startTiming && g_UT_curThing && !g_UT_gotUnit) {
		TheWritableGlobalData->m_framesPerSecondLimit = 10000;
		TheWritableGlobalData->m_useFpsLimit = false;
		while (!g_UT_gotUnit) {
			if (veryFirstTime) {
				g_UT_gotUnit = true;
				break;
			}
			g_UT_curThing = g_UT_curThing->friend_getNextTemplate();

			if (g_UT_curThing == nullptr) {
				unitTypes++;
				if (unitTypes==END) {
					side++;
					unitTypes = INFANTRY;
					if (sides[side].isEmpty()) {
						g_UT_startTiming = false;
						if (g_UT_timingLog) {
							fclose(g_UT_timingLog);
							g_UT_timingLog = nullptr;
						}
						if (g_UT_commaLog) {
							fclose(g_UT_commaLog);
							g_UT_commaLog = nullptr;
						}
						break;
					}
				}
				g_UT_curThing = TheThingFactory->firstTemplate();
			}

			const ThingTemplate* btt = g_UT_curThing;
			if (btt->getDefaultOwningSide() != sides[side]) {
				continue;
			}
			if (unitTypes == INFANTRY) {
				if (!btt->isKindOf(KINDOF_INFANTRY)) continue;
			}	else if (unitTypes == VEHICLE) {
				if (!btt->isKindOf(KINDOF_VEHICLE)) continue;
			}	else if (unitTypes == STRUCTURE) {
				if (!btt->isKindOf(KINDOF_STRUCTURE)) continue;
			}	else {
				if (btt->isKindOf(KINDOF_INFANTRY)) continue;
				if (btt->isKindOf(KINDOF_VEHICLE)) continue;
				if (btt->isKindOf(KINDOF_STRUCTURE)) continue;
			}
			if (btt->getName() == "EMPPulseBomb") continue; // 100 overloads system.
			if (btt->getName() == "GLAAngryMobRockProjectileObject") continue; // 100 overloads system.
			if (btt->getName() == "ClusterMinesBomb") continue; // 100 overloads system.
			if (btt->getName() == "BlackNapalmFirestormSmall") continue; // 100 overloads system.
			if (btt->getName() == "CabooseFullOfTerrorists") continue; // 100 overloads system.
			if (btt->getName() == "GLAAngryMobMolotovCocktailProjectileObject") continue; // 100 overloads system.
			if (btt->getName().startsWith("Firestorm"))	continue;	// 100 crashes
			if (btt->getName().startsWith("Avalanche"))	continue;	// 100 crashes
			if (btt->getName().startsWith("InfernoTankShell"))	continue;	// 100 crashes

			if (btt->getName() == "ChinaArtilleryBarrageShell") continue; // 100 takes really, freaking long. Doesn't crash jba.
			if (btt->getName() == "ChinaTankOverlordBattleBunker") continue; // 100 seems to hang gth.
			if (btt->getName() == "ChinaTankOverlordPropagandaTower") continue; // 100 seems to hang gth.
			if (btt->getName() == "ChinaTankOverlordGattlingCannon") continue; // 100 seems to hang gth.
			if (btt->getName().startsWith("CINE")) continue;
			if (btt->getName() == "GLAInfantryAngryMobNexus") continue;

#ifdef SINGLE_UNIT
			if (btt->getName()!=SINGLE_UNIT) {
				DEBUG_LOG(("Skipping %s", btt->getName().str()));
				continue;
			}
#endif
			DEBUG_LOG(("Doing thing %s -", btt->getName().str()));
			Int i, j;
			for (i=0; i<10; i++) {
				for (j=0; j<10; j++) {
					Team *team = ThePlayerList->getNthPlayer(1)->getDefaultTeam();
					Object *obj = TheThingFactory->newObject( btt, team );
					if (obj==nullptr) break;
					if (obj)
					{
						g_UT_gotUnit = true;
						Coord3D pos;
						pos.x = 20*i+30;
						pos.y = 20*j+30;
						pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );

						obj->setOrientation(0);
						obj->setPosition( &pos );

						// Now onCreates were called at the constructor.  This magically created
						// thing needs to be considered as Built for Game specific stuff.
						for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
						{
							CreateModuleInterface* create = (*m)->getCreate();
							if (!create)
								continue;
							create->onBuildComplete();
						}
						// Since the team now has members, activate it.
						team->setActive();
						TheAI->pathfinder()->addObjectToPathfindMap(obj);
					}
				}
			}
		}
		if (g_UT_gotUnit) {
			settleFrames = TIME_FRAMES/2;
			Coord3D thePos;
			thePos.x = 5000;
			thePos.y = 50;
			thePos.z = 0;
			TheTacticalView->lookAt( &thePos );
			mode = LOGIC;
			return;
		}
	}
}
#endif

DECLARE_PERF_TIMER(GameLogic_update)
DECLARE_PERF_TIMER(GameLogic_update_normal)
DECLARE_PERF_TIMER(GameLogic_update_sleepy)

#ifdef DUMP_PERF_STATS
extern __int64 Total_Get_Texture_Time;
extern __int64 Total_Get_HAnim_Time;
extern __int64 Total_Create_Render_Obj_Time;
extern __int64 Total_Load_3D_Assets;
#endif

// ------------------------------------------------------------------------------------------------
/** Update all objects in the world by invoking their update() methods. */
// ------------------------------------------------------------------------------------------------
void GameLogic::update()
{
	USE_PERF_TIMER(GameLogic_update)
	PROFILER_SECTION_COLOR(0x4CAF50);

	LatchRestore<Bool> inUpdateLatch(m_isInUpdate, TRUE);
#ifdef DO_UNIT_TIMINGS
	unitTimings();
#endif

	setFPMode();

	/// @todo remove this hack
	if ( m_startNewGame && !TheDisplay->isMoviePlaying())
	{
	#ifdef DUMP_PERF_STATS
		Total_Get_Texture_Time=0;
		Total_Get_HAnim_Time=0;
		Total_Create_Render_Obj_Time=0;
		Total_Load_3D_Assets=0;
	#endif

		startNewGame( FALSE );
		m_startNewGame = FALSE;

	#ifdef DUMP_PERF_STATS
		char Buf[1024];
		__int64 freq64;
		GetPrecisionTimerTicksPerSec(&freq64);

		sprintf(Buf,"Texture=%f, Anim=%f, CreateRobj=%f, Load3DAssets=%f",
			((double)Total_Get_Texture_Time/(double)(freq64)*1000.0),
			((double)Total_Get_HAnim_Time/(double)(freq64)*1000.0),
			((double)Total_Create_Render_Obj_Time/(double)(freq64)*1000.0),
			((double)Total_Load_3D_Assets/(double)(freq64)*1000.0));

	DEBUG_LOG(("%s", Buf));
	#endif
	}

	// send the current time to the GameClient
	UnsignedInt now = getFrame();
	TheGameClient->setFrame(now);

	PROFILER_PLOT("LogicFrame", static_cast<int64_t>(now));

	// update (execute) scripts
	{
		TheScriptEngine->UPDATE();
	}

	// TheSuperHackers @info Updates the frozen time status because it may have changed after the script engine update.
	TheFramePacer->setTimeFrozen(TheGameEngine->isTimeFrozen());

	if (TheFramePacer->isTimeFrozen())
		return;

	// Note - TerrainLogic update needs to happen after ScriptEngine update, but before object updates.  jba.
	// This way changes in bridges are noted in the script engine before being cleared in TerrainLogic->update
	{
		TheTerrainLogic->UPDATE();
	}

	// force CRC calculation, so we can keep a cache of the last N CRCs.  We do this right where the recorder
	// would be getting the CRC anyway, so replays can get the CRCs from the exact instant in time as the original.
	Bool isMPGameOrReplay = (TheRecorder && TheRecorder->isMultiplayer() && getGameMode() != GAME_SHELL && getGameMode() != GAME_NONE);
	Bool isSoloGameOrReplay = (TheRecorder && !TheRecorder->isMultiplayer() && getGameMode() != GAME_SHELL && getGameMode() != GAME_NONE);
	Bool generateForMP = (isMPGameOrReplay && (m_frame % TheGameInfo->getCRCInterval()) == 0);
#ifdef DEBUG_CRC
	Bool generateForSolo = isSoloGameOrReplay && ((m_frame && (m_frame%100 == 0)) ||
		(getFrame() >= TheCRCFirstFrameToLog && getFrame() < TheCRCLastFrameToLog && ((m_frame % REPLAY_CRC_INTERVAL) == 0)));
#else
	Bool generateForSolo = isSoloGameOrReplay && ((m_frame % REPLAY_CRC_INTERVAL) == 0);
#endif // DEBUG_CRC

	if (generateForSolo || generateForMP)
	{
		m_CRC = getCRC( CRC_RECALC );
		bool isPlayback = (TheRecorder && TheRecorder->isPlaybackMode());

		GameMessage *msg = newInstance(GameMessage)(GameMessage::MSG_LOGIC_CRC);
		msg->appendIntegerArgument(m_CRC);
		msg->appendBooleanArgument(isPlayback);

		// TheSuperHackers @info helmutbuhler 13/04/2025
		// During replay simulation, we bypass TheMessageStream and instead put the CRC message
		// directly into TheCommandList because we don't update TheMessageStream during simulation.
		GameMessageList *messageList = TheMessageStream;
		if (TheRecorder && TheRecorder->getMode() == RECORDERMODETYPE_SIMULATION_PLAYBACK)
			messageList = TheCommandList;
		messageList->appendMessage(msg);

		DEBUG_LOG(("Appended %sCRC on frame %d: %8.8X", isPlayback ? "Playback " : "", m_frame, m_CRC));
	}

	// collect stats
	if(TheStatsCollector)
	{
		TheStatsCollector->update();
	}

	// Update the Recorder
	{
		TheRecorder->UPDATE();
	}

	// process client commands
	{
		processCommandList( TheCommandList );
	}

#ifdef ALLOW_NONSLEEPY_UPDATES
	{
		for (std::list<UpdateModulePtr>::const_iterator it = m_normalUpdates.begin(); it != m_normalUpdates.end(); ++it)
		{
			UpdateModulePtr u = *it;
			DisabledMaskType dis = u->friend_getObject()->getDisabledFlags();
#if RETAIL_COMPATIBLE_CRC
			if (!dis.any() || dis.anyIntersectionWith(u->getDisabledTypesToProcess()))
#else
			// TheSuperHackers @bugfix Stubbjax 15/03/2026 The disabled-types-to-process mask is now exclusive.
			// Previously, if the disabled mask had any bits in common with the disabled-types-to-process mask,
			// the update would be processed. Now, if any *other* bits are set in the disabled mask, the update
			// is no longer processed.
			if (u->getDisabledTypesToProcess().testForAll(dis))
#endif
			{
				USE_PERF_TIMER(GameLogic_update_normal)

				m_curUpdateModule = u;

				#ifdef DEBUG_LOGGING
					UpdateSleepTime sleep = u->update();
					DEBUG_ASSERTCRASH(sleep == UPDATE_SLEEP_NONE, ("you must return SLEEPNONE from all nonsleepy modules"));
				#else
					u->update();
				#endif

				m_curUpdateModule = nullptr;
			}
		}
	}
#endif

	{
		while (!m_sleepyUpdates.empty())
		{
			UpdateModulePtr u = peekSleepyUpdate();

			if (!u)
			{
				DEBUG_CRASH(("Null update. should not happen."));
				continue;
			}

			// we're done, everyone else is sleeping.
			// break from the loop BEFORE we pop this item off.
			if (u->friend_getNextCallFrame() > now)
			{
				break;
			}

			UpdateSleepTime sleepLen = UPDATE_SLEEP_NONE;	// default, if it is disabled.

			DisabledMaskType dis = u->friend_getObject()->getDisabledFlags();
#if RETAIL_COMPATIBLE_CRC
			if (!dis.any() || dis.anyIntersectionWith(u->getDisabledTypesToProcess()))
#else
			// TheSuperHackers @bugfix Stubbjax 15/03/2026 The disabled-types-to-process mask is now exclusive.
			// Previously, if the disabled mask had any bits in common with the disabled-types-to-process mask,
			// the update would be processed. Now, if any *other* bits are set in the disabled mask, the update
			// is no longer processed.
			if (u->getDisabledTypesToProcess().testForAll(dis))
#endif
			{
				USE_PERF_TIMER(GameLogic_update_sleepy)

				//DEBUG_LOG(("calling update %08lx (%d %d)...",update,update->friend_getNextCallFrame(),update->friend_getNextCallPhase()));
				m_curUpdateModule = u;

				sleepLen = u->update();
				DEBUG_ASSERTCRASH(sleepLen > 0, ("you may not return 0 from update"));
				if (sleepLen < 1)
					sleepLen = UPDATE_SLEEP_NONE;

				m_curUpdateModule = nullptr;

			}

			// else defer it till next frame and re-push it
			u->friend_setNextCallFrame(now + sleepLen);
			rebalanceSleepyUpdate(0);
		}
	}

	validateSleepyUpdate();

	// update the Artificial Intelligence system
	{
		TheAI->UPDATE();
	}

	// production updates
	{
		TheBuildAssistant->UPDATE();
	}

	// update partition info
	{
		ThePartitionManager->UPDATE();
	}

	//
	// End of frame clean-up
	//

	// destroy all pending objects
	processDestroyList();

	// reset the command list, destroying all messages
	TheCommandList->reset();

	TheWeaponStore->UPDATE();
	TheLocomotorStore->UPDATE();
	TheVictoryConditions->UPDATE();

	{
		//Handle disabled statii (and re-enable objects once frame matches)
		for( Object *obj = m_objList; obj; obj = obj->getNextObject() )
		{
			if( obj->isDisabled() )
			{
				obj->checkDisabledStatus();
			}
		}
	}

	// increment world time
	if (!m_startNewGame)
	{
		m_frame++;
		m_hasUpdated = TRUE;
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::preUpdate()
{
	m_hasUpdated = FALSE;

	if (m_pauseFrame == m_frame && m_pauseFrame != 0)
	{
		m_pauseFrame = 0;
		Bool pause = TRUE;
		Bool pauseMusic = FALSE;
		Bool pauseInput = FALSE;
		setGamePaused(pause, pauseMusic, pauseInput);
	}
}

// ------------------------------------------------------------------------------------------------
/** Return the first object in the world list */
// ------------------------------------------------------------------------------------------------
Object *GameLogic::getFirstObject()
{
	return m_objList;
}

// ------------------------------------------------------------------------------------------------
/** Return a new unique object id. */
// ------------------------------------------------------------------------------------------------
ObjectID GameLogic::allocateObjectID()
{
	/// @todo Find unused value in current object set
	ObjectID ret = m_nextObjID;
	m_nextObjID = (ObjectID)((UnsignedInt)m_nextObjID + 1);
	return ret;
}

// ------------------------------------------------------------------------------------------------
/** Add object ID to the lookup table */
// ------------------------------------------------------------------------------------------------
void GameLogic::addObjectToLookupTable( Object *obj )
{

	// sanity
	if( obj == nullptr )
		return;

	// add to lookup
	m_objHash[ obj->getID() ] = obj;

}

// ------------------------------------------------------------------------------------------------
/** Remove object from the ID lookup table */
// ------------------------------------------------------------------------------------------------
void GameLogic::removeObjectFromLookupTable( Object *obj )
{

	// sanity
	if( obj == nullptr )
		return;

	// remove from lookup table
	m_objHash.erase( obj->getID() );

}

// ------------------------------------------------------------------------------------------------
/** Given an object, register it with the GameLogic and give it a unique ID. */
// ------------------------------------------------------------------------------------------------
void GameLogic::registerObject( Object *obj )
{

	// add the object to the global list
	obj->prependToList(&m_objList);

	// add object to lookup table
	addObjectToLookupTable( obj );

	UnsignedInt now = getFrame();
	if (now == 0)
		now = 1;
	for (BehaviorModule** b = obj->getBehaviorModules(); *b; ++b)
	{
#ifdef DIRECT_UPDATEMODULE_ACCESS
		// evil, but necessary at this point. (srj)
		UpdateModulePtr u = (UpdateModulePtr)((*b)->getUpdate());
#else
		UpdateModulePtr u = (*b)->getUpdate();
#endif
		if (!u)
			continue;

		UnsignedInt when = u->friend_getNextCallFrame();
#ifdef ALLOW_NONSLEEPY_UPDATES
		if (when == 0)
		{
			// zero is the magic value for "never sleeps"
			m_normalUpdates.push_back(u);
		}
		else
#else
		// note that 'when' can be zero here for any update module
		// that didn't bother to call setWakeFrame() in its ctor.
		// this is legal.
		if (when == 0)
			u->friend_setNextCallFrame(now);
#endif
		{
			DEBUG_ASSERTCRASH(u->friend_getNextCallFrame() >= now, ("you may not specify a zero initial sleep time for sleepy modules (%d %d)",u->friend_getNextCallFrame(),now));
			pushSleepyUpdate(u);
		}
	}

}

// ------------------------------------------------------------------------------------------------
/** create an object based on the thing template, the reason that this is
	* here and not in the ThingFactory is so that we can mirror what the
	* creation process is on the client side because clients have specific
	* device dependent drawables (such as a W3DDrawable derived from Drawable).
	* here we will allocate an object of the correct type based on thing
	* template properties
	*
	* if we want to have the thing manager actually contain the pools of
	* object and drawable storage it seems OK to have it be friends with the
	* GameLogic/Client for those purposes, or we could put the allocation pools
	* in the GameLogic and GameClient themselves */
// ------------------------------------------------------------------------------------------------
Object *GameLogic::friend_createObject( const ThingTemplate *thing, const ObjectStatusMaskType &statusBits, Team *team )
{
	Object *obj;

	obj = newInstance(Object)( thing, statusBits, team );

	return obj;
}

// ------------------------------------------------------------------------------------------------
/** Mark the object as destroyed, and place on list for deletion at the end of the next update.
 * This is the only interface to destroy objects - objects cannot be directly deleted. */
// ------------------------------------------------------------------------------------------------
void GameLogic::destroyObject( Object *obj )
{
	DEBUG_ASSERTCRASH(obj != nullptr, ("destroying null object"));

	// if already flagged for destruction, ignore
	if (!obj || obj->isDestroyed())
		return;

	// run the object onDestroy event if provided
	for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
	{
		DestroyModuleInterface* destroy = (*m)->getDestroy();
		if (destroy)
			destroy->onDestroy();
	}

	// mark object as destroyed
	obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_DESTROYED ) );

	// We desperately need to stop here, or else the destructor of the statemachine will try to do
	// stopping logic, which uses virtual functions and deleted modules, which will crash us.
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if( ai )
	{
		ai->setLocomotorGoalNone();
		ai->destroyPath();
	}

	// add to end of destruction list, in case something is being destroyed and trying to destroy subobjects
	m_objectsToDestroy.push_back(obj);

	// run any on destroy logic internal to the object
	obj->onDestroy();

	// remove wall pieces from the pathfinder
	if( obj->isKindOf( KINDOF_WALK_ON_TOP_OF_WALL ) )
		TheAI->pathfinder()->removeWallPiece( obj );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool inCRCGen = FALSE;
UnsignedInt GameLogic::getCRC( Int mode, AsciiString deepCRCFileName )
{
	if (mode != CRC_RECALC)
		return m_CRC;

	setFPMode();

	LatchRestore<Bool> latch(inCRCGen, !isInGameLogicUpdate());

	XferCRC *xferCRC;
	AsciiString marker;
	if (deepCRCFileName.isNotEmpty())
	{
		xferCRC = NEW XferDeepCRC;
		xferCRC->open(deepCRCFileName.str());
	}
	else
	{
		AsciiString crcName;
#ifdef DEBUG_CRC
		// TheSuperHackers @info helmutbuhler 04/09/2025
		// This allows you to save the binary data that is involved in the crc calculation
		// to a binary file per frame.
		// This was apparently used early in development and isn't that useful, because diffing
		// that binary data is very difficult. The CRC logging is much easier to diff and also more
		// granular than this because it can capture changes between two frames.
		if (isInGameLogicUpdate() && g_keepCRCSaves && m_frame < 5)
		{
			xferCRC = NEW XferDeepCRC;
			crcName.format("logicFrame%d.crc", (m_frame%5));
		}
		else
#endif // DEBUG_CRC
		{
			xferCRC = NEW XferCRC;
			crcName = "lightCRC";
		}
		xferCRC->open(crcName);
	}

	// calculate CRCs
	Object *obj;
	DEBUG_ASSERTCRASH(this == TheGameLogic, ("Not in GameLogic"));
	if (isInGameLogicUpdate())
	{
		CRCGEN_LOG(("CRC at start of frame %d is 0x%8.8X", m_frame, xferCRC->getCRC()));
	}

	marker = "MARKER:Objects";
	xferCRC->xferAsciiString(&marker);
	for( obj = m_objList; obj; obj=obj->getNextObject() )
	{
		xferCRC->xferSnapshot( obj );
	}
	UnsignedInt seed = GetGameLogicRandomSeedCRC();
	if (isInGameLogicUpdate())
	{
		CRCGEN_LOG(("CRC after objects for frame %d is 0x%8.8X", m_frame, xferCRC->getCRC()));
	}

	if (isInGameLogicUpdate())
	{
		CRCGEN_LOG(("RandomSeed: %d", seed));
	}
	if (xferCRC->getXferMode() == XFER_CRC)
	{
		xferCRC->xferUnsignedInt( &seed );
	}
	marker = "MARKER:ThePartitionManager";
	xferCRC->xferAsciiString(&marker);
	xferCRC->xferSnapshot( ThePartitionManager );
	if (isInGameLogicUpdate())
	{
		CRCGEN_LOG(("CRC after partition manager for frame %d is 0x%8.8X", m_frame, xferCRC->getCRC()));
	}

#ifdef DEBUG_CRC
	if ((g_crcModuleDataFromClient && !isInGameLogicUpdate()) ||
		(g_crcModuleDataFromLogic && isInGameLogicUpdate()))
	{
		marker = "MARKER:TheModuleFactory";
		xferCRC->xferAsciiString(&marker);
		xferCRC->xferSnapshot( TheModuleFactory );
		if (isInGameLogicUpdate())
		{
			CRCGEN_LOG(("CRC after module factory for frame %d is 0x%8.8X", m_frame, xferCRC->getCRC()));
		}
	}
#endif //DEBUG_CRC

	marker = "MARKER:ThePlayerList";
	xferCRC->xferAsciiString(&marker);
	xferCRC->xferSnapshot( ThePlayerList );
	if (isInGameLogicUpdate())
	{
		CRCGEN_LOG(("CRC after PlayerList for frame %d is 0x%8.8X", m_frame, xferCRC->getCRC()));
	}

	marker = "MARKER:TheAI";
	xferCRC->xferAsciiString(&marker);
	xferCRC->xferSnapshot( TheAI );
	if (isInGameLogicUpdate())
	{
		CRCGEN_LOG(("CRC after AI for frame %d is 0x%8.8X", m_frame, xferCRC->getCRC()));
	}

	if (xferCRC->getXferMode() == XFER_SAVE)
	{
		marker = "MARKER:GameSave";
		xferCRC->xferAsciiString(&marker);
		TheGameState->friend_xferSaveDataForCRC(xferCRC, SNAPSHOT_DEEPCRC_LOGICONLY);
	}

	xferCRC->close();

	UnsignedInt theCRC = xferCRC->getCRC();

	delete xferCRC;
	xferCRC = nullptr;

	if (isInGameLogicUpdate())
	{
		CRCGEN_LOG(("CRC for frame %d is 0x%8.8X", m_frame, theCRC));
	}
	return theCRC;
}

// ------------------------------------------------------------------------------------------------
void GameLogic::exitGame()
{
	// TheSuperHackers @fix The logic update must not be halted to process the game exit message.
	setGamePaused(FALSE);
	TheScriptEngine->forceUnfreezeTime();
	TheScriptEngine->doUnfreezeTime();

	TheMessageStream->appendMessage(GameMessage::MSG_CLEAR_GAME_DATA);

#ifdef PROFILER_ENABLED
	AsciiString message;
	message.format("GameEnd: %s", TheGlobalData->m_mapName.str());
	PROFILER_MSG(message.str(), message.getLength());
#endif
}

// ------------------------------------------------------------------------------------------------

void GameLogic::quit(Bool toDesktop)
{
	const Bool isNotLoading = (!isLoadingMap() && !isLoadingSave());

	if (isInGame())
	{
		if (isInInteractiveGame())
		{
			if (canOpenQuitMenu())
			{
				ToggleQuitMenu();
				return;
			}
			
			if (isInMultiplayerGame() && !isInSkirmishGame() && TheGameInfo && !TheGameInfo->isSandbox())
			{
				GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_SELF_DESTRUCT);
				msg->appendBooleanArgument(TRUE);
			}
		}

		if (TheRecorder && TheRecorder->getMode() == RECORDERMODETYPE_RECORD)
		{
			TheRecorder->stopRecording();
		}

		setGamePaused(FALSE);

		if (TheScriptEngine && isNotLoading)
		{
			TheScriptEngine->forceUnfreezeTime();
			TheScriptEngine->doUnfreezeTime();
		}

		if (toDesktop)
		{
			if (isInMultiplayerGame())
			{
				m_quitToDesktopAfterMatch = TRUE;
				if (isNotLoading)
				{
					exitGame();
				}
			}
			else
			{
				if (isNotLoading)
				{
					clearGameData();
				}
			}
		}
		else
		{
			exitGame();
		}
	}

	if (toDesktop)
	{
		if (!isInMultiplayerGame())
		{
			TheGameEngine->setQuitting(TRUE);
		}
	}

	if (TheInGameUI)
	{
		TheInGameUI->setClientQuiet(TRUE);
	}
}

// ------------------------------------------------------------------------------------------------
/** A new GameLogic object has been constructed, therefore create
 * a corresponding drawable and bind them together. */
// ------------------------------------------------------------------------------------------------
void GameLogic::sendObjectCreated( Object *obj )
{
	Drawable *draw = TheThingFactory->newDrawable(obj->getTemplate());

/// @todo COLIN ... shouldn't we have a check here for existing drawable!!!!!

	// bind drawable to object and object to drawable
	bindObjectAndDrawable(obj, draw);

}

// ------------------------------------------------------------------------------------------------
void GameLogic::bindObjectAndDrawable(Object* obj, Drawable* draw)
{
	draw->friend_bindToObject( obj );
	obj->friend_bindToDrawable( draw );
}

// ------------------------------------------------------------------------------------------------
/** Send notification of object destruction. */
// ------------------------------------------------------------------------------------------------
void GameLogic::sendObjectDestroyed( Object *obj )
{
	// Because this implementation is a bridge between the Logic and Interface,
	// we must take extra care to handle such cases as when the system it
	// shutting down.
	if(TheGameClient == nullptr)
		return;

	// destroy the drawable
	Drawable *draw = obj->getDrawable();
	if(draw)
	{
		TheGameClient->destroyDrawable( draw );
	}

	// erase the binding of the drawable to this object
	obj->friend_bindToDrawable( nullptr );

}

// ------------------------------------------------------------------------------------------------
/** Return if the game is paused or not */
// ------------------------------------------------------------------------------------------------
Bool GameLogic::isGamePaused()
{
	return m_gamePaused;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::setGamePausedInFrame( UnsignedInt frame, Bool disableLogicTimeScale )
{
	if (frame >= m_frame)
	{
		m_pauseFrame = frame;

		if (disableLogicTimeScale)
		{
			m_logicTimeScaleEnabledMemory = TheFramePacer->isLogicTimeScaleEnabled();
			TheFramePacer->enableLogicTimeScale(FALSE);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::setGamePaused( Bool paused, Bool pauseMusic, Bool pauseInput )
{
	// We need to ignore an unpause called when we are unpaused or else:
	// Mouse is hidden for some reason (script or something)
	// GamePaused
	// Remember that we were hidden
	// Show mouse
	// GameUnpaused
	// Set mouse the way it was

	// Time passes, mouse is unhidden

	// GameUnpaused
	// Set mouse the way it "was" <--- Was counting on right answer being set in Pause.

	pauseGameLogic(paused);
	pauseGameSound(paused);
	pauseGameMusic(paused && pauseMusic);
	pauseGameInput(paused && pauseInput);

	updateDisplayBusyState();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::pauseGameLogic(Bool paused)
{
	m_gamePaused = paused;

	if (!paused && m_logicTimeScaleEnabledMemory)
	{
		m_logicTimeScaleEnabledMemory = FALSE;
		TheFramePacer->enableLogicTimeScale(TRUE);
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::pauseGameSound(Bool paused)
{
	if(m_pauseSound == paused)
		return;

	m_pauseSound = paused;

	if(paused)
	{
		TheAudio->pauseAudio((AudioAffect)(AudioAffect_All & ~AudioAffect_Music));

		//Stop all ambient sounds!
		Drawable *drawable = TheGameClient->getDrawableList();
		while( drawable )
		{
			drawable->stopAmbientSound();
			drawable = drawable->getNextDrawable();
		}
	}
	else
	{
		TheAudio->resumeAudio((AudioAffect)(AudioAffect_All & ~AudioAffect_Music));

		//Start all ambient sounds!
		Drawable *drawable = TheGameClient->getDrawableList();
		while( drawable )
		{
			drawable->startAmbientSound();
			drawable = drawable->getNextDrawable();
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::pauseGameMusic(Bool paused)
{
	if(m_pauseMusic == paused)
		return;

	m_pauseMusic = paused;

	if(paused)
	{
		TheAudio->pauseAudio(AudioAffect_Music);
	}
	else
	{
		TheAudio->resumeAudio(AudioAffect_Music);
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::pauseGameInput(Bool paused)
{
	if(m_pauseInput == paused)
		return;

	m_pauseInput = paused;

	TheMouse->onGamePaused(paused);

	if(paused)
	{
		// remember the state of the mouse/input so we can return to the same state once we "unpause"
		m_inputEnabledMemory = TheInGameUI->getInputEnabled();
		m_mouseVisibleMemory = TheMouse->getVisibility();

		// Make sure the mouse is visible and the cursor is an arrow
		TheMouse->setVisibility(TRUE);
		TheMouse->setCursor( Mouse::ARROW );

		// if Input is enabled, disable it
		if(m_inputEnabledMemory)
		{
			TheInGameUI->setInputEnabled(FALSE);
		}
	}
	else
	{
		// set the mouse/input states to what they were before we paused.
		TheMouse->setVisibility(m_mouseVisibleMemory);
		if(m_inputEnabledMemory)
		{
			TheInGameUI->setInputEnabled(TRUE);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::processProgress(Int playerId, Int percentage)
{
	if(m_loadScreen)
		m_loadScreen->processProgress(playerId, percentage);
	lastHeardFrom(playerId);
}

// ------------------------------------------------------------------------------------------------
/** Whenever we get a progress complete packet for a Net Game,
	* Set a flag that that player is ready */
// ------------------------------------------------------------------------------------------------
void GameLogic::processProgressComplete(Int playerId)
{
	if(playerId < 0 || playerId >= MAX_SLOTS)
	{
		DEBUG_CRASH(("GameLogic::processProgressComplete, Invalid playerid was passed in %d", playerId));
		return;
	}
	if(m_progressComplete[playerId] == TRUE)
	{
		DEBUG_LOG(("GameLogic::processProgressComplete, playerId %d is marked TRUE already yet we're trying to mark him as true again", playerId));
		return;
	}
	DEBUG_LOG(("Progress Complete for Player %d", playerId));
	m_progressComplete[playerId] = TRUE;
	lastHeardFrom(playerId);
}

// ------------------------------------------------------------------------------------------------
/// @TODO: Add check to account for timeouts
// ------------------------------------------------------------------------------------------------
Bool GameLogic::isProgressComplete()
{
	//If we're not in a network game, always return true
	if(!isInMultiplayerGame() || !TheNetwork || m_forceGameStartByTimeOut)
		return TRUE;

	// Only loop on the Number of players we got in here
	for(Int i =0; i < MAX_SLOTS; ++i)
	{
		if(!m_progressComplete[i])
			return FALSE;
	}
	return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::lastHeardFrom( Int playerId )
{
	if( playerId < 0 || playerId >= MAX_SLOTS)
		return;
	m_progressCompleteTimeout[playerId] = timeGetTime();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::testTimeOut()
{
	// if everyone is loaded, lets just load the game like normal.
	if(isProgressComplete())
		return;

	Int curTime = timeGetTime();
	// Loop and test everyone in our game.
	for(Int i =0; i < MAX_SLOTS; ++i)
	{
		// If they've completed their progress, ignore them
		if(m_progressComplete[i])
			continue;
		if(	m_progressCompleteTimeout[i] + PROGRESS_COMPLETE_TIMEOUT > curTime )
			return;
	}
	// if we made it this far, that means everyone has timed out.
	m_forceGameStartByTimeOut = TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::timeOutGameStart()
{
	DEBUG_LOG(("We got the Force TimeOut Start Message"));
	m_forceGameStartByTimeOut = TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::initTimeOutValues()
{
	if (!TheNetwork)
		return;
	for(Int i = 0; i < TheNetwork->getNumPlayers(); ++i)
	{
		m_progressCompleteTimeout[i] = timeGetTime();
	}
}

// ------------------------------------------------------------------------------------------------
/** returns the total number of objects in the world */
// ------------------------------------------------------------------------------------------------
UnsignedInt GameLogic::getObjectCount()
{
	UnsignedInt totalObjects = 0;
	Object *obj;
	for( obj = getFirstObject(); obj; obj = obj->getNextObject() )
	{
		++totalObjects;
	}
	return totalObjects;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
GhostObjectManager *GameLogic::createGhostObjectManager(bool dummy)
{
	if (dummy)
		return NEW GhostObjectManagerDummy;
	return NEW GhostObjectManager;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
TerrainLogic *GameLogic::createTerrainLogic()
{
	return NEW TerrainLogic;
}

// ------------------------------------------------------------------------------------------------
void GameLogic::setBuildableStatusOverride(const ThingTemplate* tt, BuildableStatus bs)
{
	if (tt)
	{
		m_thingTemplateBuildableOverrides[tt->getName()] = bs;
	}
}

// ------------------------------------------------------------------------------------------------
Bool GameLogic::findBuildableStatusOverride(const ThingTemplate* tt, BuildableStatus& bs) const
{
	if (tt)
	{
		BuildableMap::const_iterator it = m_thingTemplateBuildableOverrides.find(tt->getName());
		if (it != m_thingTemplateBuildableOverrides.end())
		{
			bs = it->second;
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
void GameLogic::setControlBarOverride(const AsciiString& commandSetName, Int slot, ConstCommandButtonPtr commandButton)
{
	char buf[256];
	buf[0] = '0' + slot;
	strcpy(&buf[1], commandSetName.str());
	m_controlBarOverrides[buf] = commandButton;
}

// ------------------------------------------------------------------------------------------------
Bool GameLogic::findControlBarOverride(const AsciiString& commandSetName, Int slot, ConstCommandButtonPtr& commandButton) const
{
	char buf[256];
	buf[0] = '0' + slot;
	strcpy(&buf[1], commandSetName.str());

	ControlBarOverrideMap::const_iterator it = m_controlBarOverrides.find(buf);
	if (it != m_controlBarOverrides.end())
	{
		commandButton = it->second;	// could be null.
		return true;
	}

	// leave commandButton unmodified.
	return false;
}

#ifdef DUMP_PERF_STATS
// ------------------------------------------------------------------------------------------------
void GameLogic::getAIMetricsStatistics( UnsignedInt *numAI, UnsignedInt *numMoving, UnsignedInt *numAttacking, UnsignedInt *numWaitingForPath, UnsignedInt *overallFailedPathfinds )
{
	Object *obj;
	*numAI = 0;
	*numMoving = 0;
	*numAttacking = 0;
	*numWaitingForPath = 0;
	for( obj = getFirstObject(); obj; obj = obj->getNextObject() )
	{
		AIUpdateInterface *ai = obj->getAI();
		if( ai )
		{
			(*numAI)++;

			if( ai->isMoving() )
			{
				(*numMoving)++;
			}
			if( ai->isWaitingForPath() )
			{
				(*numWaitingForPath)++;
			}
			if( ai->isAttacking() )
			{
				(*numAttacking)++;
			}
		}
	}
	*overallFailedPathfinds = m_overallFailedPathfinds;
}
#endif

// ------------------------------------------------------------------------------------------------
/** Light CRC */
// ------------------------------------------------------------------------------------------------
void GameLogic::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Given a string name, find the object TOC entry (if any) associated with it */
// ------------------------------------------------------------------------------------------------
GameLogic::ObjectTOCEntry *GameLogic::findTOCEntryByName( AsciiString name )
{

	for( ObjectTOCListIterator it = m_objectTOC.begin(); it != m_objectTOC.end(); ++it )
		if( (*it).name == name )
			return &(*it);

	return nullptr;

}

// ------------------------------------------------------------------------------------------------
/** Given a object TOC identifier, find the object TOC if any */
// ------------------------------------------------------------------------------------------------
GameLogic::ObjectTOCEntry *GameLogic::findTOCEntryById( UnsignedShort id )
{

	for( ObjectTOCListIterator it = m_objectTOC.begin(); it != m_objectTOC.end(); ++it )
		if( (*it).id == id )
			return &(*it);

	return nullptr;

}

// ------------------------------------------------------------------------------------------------
/** Add an object TOC entry */
// ------------------------------------------------------------------------------------------------
void GameLogic::addTOCEntry( AsciiString name, UnsignedShort id )
{

	ObjectTOCEntry tocEntry;
	tocEntry.name = name;
	tocEntry.id = id;
	m_objectTOC.push_back( tocEntry );

}

// ------------------------------------------------------------------------------------------------
/** Xfer object table of contents */
// ------------------------------------------------------------------------------------------------
void GameLogic::xferObjectTOC( Xfer *xfer )
{

	// version
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// clear our current table of contents
	m_objectTOC.clear();

	// xfer the table
	UnsignedInt tocCount = 0;
	if( xfer->getXferMode() == XFER_SAVE )
	{
		AsciiString templateName;

		// generate a new TOC based on the objects that are in the map
		for( Object *obj = getFirstObject(); obj; obj = obj->getNextObject() )
		{

			// get the name we're working with
			templateName = obj->getTemplate()->getName();

			// if is this object name already in the TOC, skip it
			if( findTOCEntryByName( templateName ) != nullptr )
				continue;

			// add this entry to the TOC
			addTOCEntry( obj->getTemplate()->getName(), ++tocCount );

		}

		// xfer entries in the TOC
		xfer->xferUnsignedInt( &tocCount );

		// xfer each TOC entry
		ObjectTOCListIterator it;
		ObjectTOCEntry *tocEntry;
		for( it = m_objectTOC.begin(); it != m_objectTOC.end(); ++it )
		{

			// get this toc entry
			tocEntry = &(*it);

			// xfer the name
			xfer->xferAsciiString( &tocEntry->name );

			// xfer the paired id
			xfer->xferUnsignedShort( &tocEntry->id );

		}

	}
	else
	{
		AsciiString templateName;
		UnsignedShort id;

		// how many entries are we going to read
		xfer->xferUnsignedInt( &tocCount );

		// read all the entries
		for( UnsignedInt i = 0; i < tocCount; ++i )
		{

			// read the name
			xfer->xferAsciiString( &templateName );

			// read the id
			xfer->xferUnsignedShort( &id );

			// add this to the TOC
			addTOCEntry( templateName, id );

		}

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameLogic::prepareLogicForObjectLoad()
{

	//
	// this is a band-aid :(
	// when loading from a map file, objects were created for the bridges, towers, walls etc.
	// since we're going to load that data from the save game file we will destroy any of
	// those bridge objects and tower objects so that we can create them from the save data
	//
	Object *obj, *next;
	for( obj = getFirstObject(); obj; obj = next )
	{

		// get next
		next = obj->getNextObject();

		// is this a bridge object?
		if( obj->isKindOf( KINDOF_BRIDGE ) )
		{
			Bridge *bridge = TheTerrainLogic->findBridgeAt( obj->getPosition() );

			// sanity
			DEBUG_ASSERTCRASH( bridge, ("GameLogic::prepareLogicForObjectLoad - Unable to find bridge" ));

			// get the old object that is in the bridge info
			const BridgeInfo *bridgeInfo = bridge->peekBridgeInfo();
			Object *oldObject = findObjectByID( bridgeInfo->bridgeObjectID );
			DEBUG_ASSERTCRASH( oldObject, ("GameLogic::prepareLogicForObjectLoad - Unable to find old bridge object") );
			DEBUG_ASSERTCRASH( oldObject == obj, ("GameLogic::prepareLogicForObjectLoad - obj != oldObject") );

			//
			// destroy the 4 towers that are attached to this old object (they will be loaded from
			// the save game file
			//
			Object *oldTower;
			for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
			{

				oldTower = findObjectByID( bridgeInfo->towerObjectID[ i ] );
				if (oldTower) {
					destroyObject( oldTower );
				}

			}

			// destroy the old bridge object
			destroyObject( oldObject );

		}
		else if( obj->isKindOf( KINDOF_WALK_ON_TOP_OF_WALL ) )
		{

			// destroy walk on top of wall things too
			destroyObject( obj );

		}

	}

	// process the destruction of these objects immediately before we proceed with the load process
	processDestroyList();

	// there should be no objects anywhere
	DEBUG_ASSERTCRASH( getFirstObject() == nullptr,
										 ("GameLogic::prepareLogicForObjectLoad - There are still objects loaded in the engine, but it should be empty (Top is '%s')",
										 getFirstObject()->getTemplate()->getName().str()) );

}

// ------------------------------------------------------------------------------------------------
/** Load/Save game logic to xfer
	*	Version Info:
	* 1: Initial version
	* 2: Added m_isScoringEnabled flag (BGC)
	* 3: Added polygon triggers (CBD)
	* 4: Added block markers around object data, no version checking is done and therefore
	*		 this version breaks compatibility with previous versions. (CBD)
	* 5: Added xfering the BuildAssistant's sell list.
	* 9: Added m_rankPointsToAddAtGameStart, or else on a load game, your RestartGame button will forget your exp
	* 10: TheSuperHackers @fix Save objects in reverse order so they load in correct order
	*/
// ------------------------------------------------------------------------------------------------
void GameLogic::xfer( Xfer *xfer )
{

	// version
#if RETAIL_COMPATIBLE_XFER_SAVE
	const XferVersion currentVersion = 9;
#else
	const XferVersion currentVersion = 10;
#endif
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// logic frame number
	xfer->xferUnsignedInt( &m_frame );

	//
	// note that we do not do the id counter here, we did it in the game state block because
	// it's important to do that part very early in the load process
	//
	// !!!DON'T DO THIS!!! ----> xfer->xferObjectID( &m_nextObjectID ); <---- !!!DON'T DO THIS!!!

	//
	// xfer a table of contents that contain thing template and identifier pairs.  this
	// table of contents is good for this save file only as unique numbers are only
	// generated and stored for the actual things that are on this map
	//
	xferObjectTOC( xfer );

	// when loading, we need to clean up bridges and get them ready for a load
	if( xfer->getXferMode() == XFER_LOAD )
		prepareLogicForObjectLoad();

	// object count
	UnsignedInt objectCount = getObjectCount();
	xfer->xferUnsignedInt( &objectCount );

	// object data
	Object *obj;
	ObjectTOCEntry *tocEntry;
	if( xfer->getXferMode() == XFER_SAVE )
	{
#if !RETAIL_COMPATIBLE_XFER_SAVE
		// TheSuperHackers @fix bobtista 07/03/2026 Save objects in reverse order (newest first)
		// so they load in the correct order (oldest objects at head of list).
		Object *lastObj = nullptr;
		for( obj = getFirstObject(); obj; obj = obj->getNextObject() )
			lastObj = obj;

		for( obj = lastObj; obj; obj = obj->getPrevObject() )
#else
		for( obj = getFirstObject(); obj; obj = obj->getNextObject() )
#endif
		{

			// get the object TOC entry for this template
			tocEntry = findTOCEntryByName( obj->getTemplate()->getName() );
			if( tocEntry == nullptr )
			{

				DEBUG_CRASH(( "GameLogic::xfer - Object TOC entry not found for '%s'", obj->getTemplate()->getName().str() ));
				throw SC_INVALID_DATA;

			}

			// transfer TOC id entry
			xfer->xferUnsignedShort( &tocEntry->id );

			// begin a block of data
			xfer->beginBlock();

			// write object data
			xfer->xferSnapshot( obj );

			// end a block of data
			xfer->endBlock();

		}

	}
	else
	{
		Team *defaultTeam = ThePlayerList->getNeutralPlayer()->getDefaultTeam();
		const ThingTemplate *thingTemplate;

		// read all objects
		Int objectDataSize;
		UnsignedShort tocID;
		ObjectTOCEntry *tocEntry;
		for( UnsignedInt i = 0; i < objectCount; ++i )
		{

			// read toc entry identifier
			xfer->xferUnsignedShort( &tocID );

			// find Object TOC entry with this identifier
			tocEntry = findTOCEntryById( tocID );
			if( tocEntry == nullptr )
			{

				DEBUG_CRASH(( "GameLogic::xfer - No TOC entry match for id '%d'", tocID ));
				throw SC_INVALID_DATA;

			}

			// a block of data has begun
			objectDataSize = xfer->beginBlock();

			// find matching thing template
			thingTemplate = TheThingFactory->findTemplate( tocEntry->name );
			if( thingTemplate == nullptr )
			{

				DEBUG_CRASH(( "GameLogic::xfer - Unrecognized thing template name '%s', skipping.  ENGINEERS - Are you *sure* it's OK to be ignoring this object from the save file???  Think hard about it!",
											tocEntry->name.str() ));
				xfer->skip( objectDataSize );
				continue;

			}

			// create new object
			obj = TheThingFactory->newObject( thingTemplate, defaultTeam );

			// xfer the rest of the object data
			xfer->xferSnapshot( obj );

			// end of block of data (not necessary in a load, but looks symettrically nice)
			xfer->endBlock();

			// special case for wall pieces, need to add them to the pathfinder
			if( obj->isKindOf( KINDOF_WALK_ON_TOP_OF_WALL ) )
				TheAI->pathfinder()->addWallPiece( obj );

		}

		// TheSuperHackers @fix bobtista 07/03/2026 Reverse object list after load.
		// Objects are prepended during creation, which reverses the saved order.
		// Version 10+ saves in reverse order so they load in the correct order.
		if ( version <= 9 )
		{
			Object *prev = nullptr;
			Object *current = m_objList;
			Object *next = nullptr;
			while ( current != nullptr )
			{
				next = current->getNextObject();
				current->friend_setNextObject( prev );
				current->friend_setPrevObject( next );
				prev = current;
				current = next;
			}
			m_objList = prev;
		}

	}

	// campaign info
	xfer->xferSnapshot( TheCampaignManager );

	// cave system info
	xfer->xferSnapshot( TheCaveSystem );

	// is scoring enabled
	if( version >= 2 )
		xfer->xferBool(&m_isScoringEnabled);

	// polygon triggers
	if( version >= 3 )
	{
		PolygonTrigger *poly;

		// count the number of polygon triggers we have
		UnsignedInt triggerCount = 0;
		for( poly = PolygonTrigger::getFirstPolygonTrigger(); poly; poly = poly->getNext() )
			triggerCount++;

		// sanity count
		UnsignedInt sanityTriggerCount = triggerCount;

		// xfer count
		xfer->xferUnsignedInt( &triggerCount );

		//
		// since the save game loaded should have exactly the same number of polygon triggers
		// in it that we did from loading the map this is just a sanity check here
		//
		if( sanityTriggerCount != triggerCount )
		{

			DEBUG_CRASH(( "GameLogic::xfer - Polygon trigger count mismatch.  Save file has a count of '%d', but map had '%d' triggers",
										sanityTriggerCount, triggerCount ));
			throw SC_INVALID_DATA;

		}

		// xfer each of the polygon triggers
		if( xfer->getXferMode() == XFER_SAVE )
		{
			Int triggerID;

			for( poly = PolygonTrigger::getFirstPolygonTrigger(); poly; poly = poly->getNext() )
			{

				// write polygon ID
				triggerID = poly->getID();
				xfer->xferInt( &triggerID );

				// xfer polygon data
				xfer->xferSnapshot( poly );

			}

		}
		else
		{
			Int triggerID;

			// loop through all triggers
			for( UnsignedInt i = 0; i < triggerCount; ++i )
			{

				// read ID
				xfer->xferInt( &triggerID );

				// find this polygon trigger
				poly = PolygonTrigger::getPolygonTriggerByID( triggerID );

				// sanity
				if( poly == nullptr )
				{

					DEBUG_CRASH(( "GameLogic::xfer - Unable to find polygon trigger with id '%d'",
												triggerID ));
					throw SC_INVALID_DATA;

				}

				// xfer polygon data
				xfer->xferSnapshot( poly );

			}

			//
			// force a recalculation of the pathfinding cause some of these polygon triggers
			// are water which can move during run-time, and therefore affecting the area
			// that objects can move on. Also, map objects need recalculation. jba.
			//
			TheAI->pathfinder()->newMap();

		}

	}

	// note that version=4 is the same as version=3
	if (version >= 5)
	{
		xfer->xferInt(&m_rankLevelLimit);
	}

	if (version>=6) {
		// We need the list of buildings in process of being sold.
		TheBuildAssistant->xferTheSellList(xfer);
	}

	if (version >= 7)
	{
		if( xfer->getXferMode() == XFER_SAVE )
		{
			for (BuildableMap::const_iterator it = m_thingTemplateBuildableOverrides.begin(); it != m_thingTemplateBuildableOverrides.end(); ++it )
			{
				AsciiString name = it->first;
				BuildableStatus bs = it->second;
				xfer->xferAsciiString(&name);
				xfer->xferUser(&bs, sizeof(bs));
			}
			AsciiString empty;
			xfer->xferAsciiString(&empty);
		}
		else if (xfer->getXferMode() == XFER_LOAD)
		{
			if (m_thingTemplateBuildableOverrides.empty() == false)
			{
				DEBUG_CRASH(( "GameLogic::xfer - m_thingTemplateBuildableOverrides should be empty, but is not"));
				throw SC_INVALID_DATA;
			}

			for (;;)
			{
				AsciiString name;
				xfer->xferAsciiString(&name);
				if (name.isEmpty())
					break;
				BuildableStatus bs;
				xfer->xferUser(&bs, sizeof(bs));
				m_thingTemplateBuildableOverrides[name] = bs;
			}
		}
	}

	if (version >= 8)
	{
		xfer->xferBool(&m_showBehindBuildingMarkers);
		xfer->xferBool(&m_drawIconUI);
		xfer->xferBool(&m_showDynamicLOD);
		xfer->xferInt(&m_scriptHulkMaxLifetimeOverride);

		if( xfer->getXferMode() == XFER_SAVE )
		{
			for (ControlBarOverrideMap::const_iterator it = m_controlBarOverrides.begin(); it != m_controlBarOverrides.end(); ++it )
			{
				AsciiString name = it->first;
				AsciiString value = it->second ? it->second->getName() : AsciiString::TheEmptyString;
				xfer->xferAsciiString(&name);
				xfer->xferAsciiString(&value);
			}
			AsciiString empty;
			xfer->xferAsciiString(&empty);
		}
		else if (xfer->getXferMode() == XFER_LOAD)
		{
			if (m_controlBarOverrides.empty() == false)
			{
				DEBUG_CRASH(( "GameLogic::xfer - m_controlBarOverrides should be empty, but is not"));
				throw SC_INVALID_DATA;
			}

			for (;;)
			{
				AsciiString name;
				xfer->xferAsciiString(&name);
				if (name.isEmpty())
					break;
				AsciiString value;
				xfer->xferAsciiString(&value);
				ConstCommandButtonPtr button = nullptr;
				if (value.isNotEmpty())
				{
					button = TheControlBar->findCommandButton(value);
					DEBUG_ASSERTCRASH(button != nullptr, ("Could not find button %s",value.str()));
				}
				m_controlBarOverrides[name] = button;
			}
		}
	}

	if (version >= 9)
	{
		xfer->xferInt(&m_rankPointsToAddAtGameStart);
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process entry point */
// ------------------------------------------------------------------------------------------------
void GameLogic::loadPostProcess()
{

	//
	// the act of loading objects can (theoretically) as a side effect create other objects,
	// our m_nextObjID that we maintain to give objects unique ID is also continually
	// climbing higher and higher due to us allocating objects during load (even though
	// those objects have their ids overwritten with data from the file.  To prevent the
	// m_nextObjID from getting un-necessarily high we will set it to the next available
	// id from the objects that are now in the world and actually in use
	//
	m_nextObjID = INVALID_ID;
	Object *obj;
	for( obj = getFirstObject(); obj; obj = obj->getNextObject() )
		if( obj->getID() >= m_nextObjID )
			m_nextObjID = (ObjectID)((UnsignedInt)obj->getID() + 1);

	// blow away the sleepy update and normal update module lists
	for (std::vector<UpdateModulePtr>::iterator it = m_sleepyUpdates.begin(); it != m_sleepyUpdates.end(); ++it)
	{
		(*it)->friend_setIndexInLogic(-1);
	}
	m_sleepyUpdates.clear();
#ifdef ALLOW_NONSLEEPY_UPDATES
	m_normalUpdates.clear();
#else
	UnsignedInt now = getFrame();
	if (now == 0)
		now = 1;
#endif

	// go through all objects, examine each update module and put it on the appropriate update list
	for( obj = getFirstObject(); obj; obj = obj->getNextObject() )
	{

		// get the update list of modules for this object
		for( BehaviorModule** b = obj->getBehaviorModules(); *b; ++b )
		{
#ifdef DIRECT_UPDATEMODULE_ACCESS
			// evil, but necessary at this point. (srj)
			UpdateModulePtr u = (UpdateModulePtr)((*b)->getUpdate());
#else
			UpdateModulePtr u = (*b)->getUpdate();
#endif
			if (!u)
				continue;

			DEBUG_ASSERTCRASH(u->friend_getIndexInLogic() == -1, ("Hmm, expected index to be -1 here"));

			// check each update module
			UnsignedInt when = u->friend_getNextCallFrame();
#ifdef ALLOW_NONSLEEPY_UPDATES
			if( when == 0 )
			{
				// zero if the magic value for "never sleeps"
				m_normalUpdates.push_back(u);
			}
			else
#else
			// note that 'when' will only be zero for legacy save files.
			if (when == 0)
				u->friend_setNextCallFrame(now);
#endif
			{
				m_sleepyUpdates.push_back(u);
				u->friend_setIndexInLogic(m_sleepyUpdates.size() - 1);
			}

		}

	}

	// re-sort the priority queue all at once now that all modules are on it
	remakeSleepyUpdate();

}
