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

// FILE: GlobalData.h /////////////////////////////////////////////////////////////////////////////
// Global data used by both the client and logic
// Author: trolfs, Michae Booth, Colin Day, April 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameCommon.h"	// ensure we get DUMP_PERF_STATS, or not
#include "Common/AsciiString.h"
#include "Common/GameType.h"
#include "Common/GameMemory.h"
#include "Common/SubsystemInterface.h"
#include "GameClient/Color.h"
#include "Common/STLTypedefs.h"
#include "Common/Money.h"

// FORWARD DECLARATIONS ///////////////////////////////////////////////////////////////////////////
struct FieldParse;
enum _TerrainLOD CPP_11(: Int);
class CommandLine;
class GlobalData;
class INI;
class WeaponBonusSet;
enum BodyDamageType CPP_11(: Int);
enum AIDebugOptions CPP_11(: Int);

// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////

constexpr const Int MAX_GLOBAL_LIGHTS = 3;
constexpr const Int SIMULATE_REPLAYS_SEQUENTIAL = -1;

//-------------------------------------------------------------------------------------------------
class CommandLineData
{
	friend class CommandLine;
	friend class GlobalData;

	CommandLineData()
		: m_hasParsedCommandLineForStartup(false)
		, m_hasParsedCommandLineForEngineInit(false)
	{}

	Bool m_hasParsedCommandLineForStartup;
	Bool m_hasParsedCommandLineForEngineInit;
};

//-------------------------------------------------------------------------------------------------
/** Global data container class
  *	Defines all global game data used by the system
	* @todo Change this entire system. Otherwise this will end up a huge class containing tons of variables,
	* and will cause re-compilation dependencies throughout the code base. */
//-------------------------------------------------------------------------------------------------
class GlobalData : public SubsystemInterface
{

public:

	GlobalData();
	virtual ~GlobalData() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override { }

	Bool setTimeOfDay( TimeOfDay tod );		///< Use this function to set the Time of day;

	static void parseGameDataDefinition( INI* ini );
	void parseCustomDefinition();

	//-----------------------------------------------------------------------------------------------
	struct TerrainLighting
	{
		RGBColor ambient;
		RGBColor diffuse;
		Coord3D lightPos;
	};

	//-----------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------

	CommandLineData m_commandLineData;

	AsciiString m_mapName;  ///< hack for now, this whole this is going away
	AsciiString m_moveHintName;
	Bool m_useTrees;
	Bool m_useTreeSway;
	Bool m_useDrawModuleLOD;
	Bool m_useHeatEffects;
	Bool m_useFpsLimit;
	Bool m_dumpAssetUsage;
	Int m_framesPerSecondLimit;
	Int	m_chipSetType;	///<See W3DShaderManager::ChipsetType for options

	// TheSuperHackers @feature helmutbuhler 11/04/2025
	// Run game without graphics, input or audio.
	Bool m_headless;

	// GeneralsX @feature BenderAI 21/04/2026 Allow user to opt out of the in-game update checker
	Bool m_checkForUpdates;

	Bool m_windowed;
	Int m_xResolution;
	Int m_yResolution;
	Int m_maxShellScreens;  ///< this many shells layouts can be loaded at once
	Bool m_useCloudMap;
	Int  m_use3WayTerrainBlends;	///< 0 is none, 1 is normal, 2 is debug.
	Bool m_useLightMap;
	Bool m_bilinearTerrainTex;
	Bool m_trilinearTerrainTex;
	Bool m_multiPassTerrain;
	Bool m_adjustCliffTextures;
	Bool m_stretchTerrain;
	Bool m_useHalfHeightMap;
	Bool m_drawEntireTerrain;
	_TerrainLOD m_terrainLOD;
	Bool m_enableDynamicLOD;
	Bool m_enableStaticLOD;
	Int m_terrainLODTargetTimeMS;
	Bool m_useAlternateMouse;
	Bool m_useRightMouseScrollWithAlternateMouse; // TheSuperHackers @feature User option for RMB scroll in Alternate Mouse mode.
	Bool m_clientRetaliationModeEnabled;
	Bool m_doubleClickAttackMove;
	Bool m_rightMouseAlwaysScrolls;
	Bool m_useWaterPlane;
	Bool m_useCloudPlane;
	Bool m_useShadowVolumes;
	Bool m_useShadowDecals;
	Int  m_textureReductionFactor;	//how much to cut texture resolution: 2 is half, 3 is quarter, etc.
	Bool m_enableBehindBuildingMarkers;
	Real m_waterPositionX;
	Real m_waterPositionY;
	Real m_waterPositionZ;
	Real m_waterExtentX;
	Real m_waterExtentY;
	Int	m_waterType;
	Bool m_showSoftWaterEdge;
	Bool m_usingWaterTrackEditor;
	Bool m_isWorldBuilder;

	Int m_featherWater;

	// these are for WATER_TYPE_3 vertex animated water
	enum { MAX_WATER_GRID_SETTINGS = 4 };
	AsciiString m_vertexWaterAvailableMaps[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterHeightClampLow[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterHeightClampHi[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterAngle[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterXPosition[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterYPosition[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterZPosition[ MAX_WATER_GRID_SETTINGS ];
	Int m_vertexWaterXGridCells[ MAX_WATER_GRID_SETTINGS ];
	Int m_vertexWaterYGridCells[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterGridSize[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterAttenuationA[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterAttenuationB[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterAttenuationC[ MAX_WATER_GRID_SETTINGS ];
	Real m_vertexWaterAttenuationRange[ MAX_WATER_GRID_SETTINGS ];

	Real m_downwindAngle;
	Real m_skyBoxPositionZ;
	Real m_drawSkyBox;
	Real m_skyBoxScale;
	Real m_viewportHeightScale; // The height scale of the tactical view ranging 0..1. Used to hide the world behind the Control Bar.
	Real m_cameraPitch;
	Real m_cameraYaw;
#if PRESERVE_RETAIL_SCRIPTED_CAMERA
	Real m_cameraHeight;
#endif
	Real m_maxCameraHeight;
	Real m_minCameraHeight;

	Real m_terrainDrawDistanceScale;

	Real m_terrainHeightAtEdgeOfMap;
	Real m_unitDamagedThresh;
	Real m_unitReallyDamagedThresh;
	Real m_groundStiffness;
	Real m_structureStiffness;
	Real m_gravity;	// acceleration due to gravity, in dist/frame^2
	Real m_stealthFriendlyOpacity;
	UnsignedInt m_defaultOcclusionDelay;	///<time to delay building occlusion after object is created.

	Bool m_preloadAssets;
	Bool m_preloadEverything;			///< Preload everything, everywhere (for debugging only)
	Bool m_preloadReport;					///< dump a log of all W3D assets that are being preloaded.

	Real m_partitionCellSize;

	Coord3D m_ammoPipWorldOffset;
	Coord3D m_containerPipWorldOffset;
	Coord2D m_ammoPipScreenOffset;
	Coord2D m_containerPipScreenOffset;
	Real m_ammoPipScaleFactor;
	Real m_containerPipScaleFactor;

	UnsignedInt m_historicDamageLimit;

	//Settings for terrain tracks left by vehicles with treads or wheels
	Int m_maxTerrainTracks; ///<maximum number of objects allowed to generate tracks.
	Int m_maxTankTrackEdges;	///<maximum length of tank track
	Int m_maxTankTrackOpaqueEdges;	///<maximum length of tank track before it starts fading.
	Int m_maxTankTrackFadeDelay;	///<maximum amount of time a tank track segment remains visible.

	AsciiString m_levelGainAnimationName; ///< The animation to play when a level is gained.
	Real m_levelGainAnimationDisplayTimeInSeconds;		///< time to play animation for
	Real m_levelGainAnimationZRisePerSecond;					///< rise animation up while playing

	AsciiString m_getHealedAnimationName; ///< The animation to play when emergency repair does its thing.
	Real m_getHealedAnimationDisplayTimeInSeconds;		///< time to play animation for
	Real m_getHealedAnimationZRisePerSecond;					///< rise animation up while playing

	TimeOfDay	m_timeOfDay;
	Weather m_weather;
	Bool m_makeTrackMarks;
	Bool m_hideGarrisonFlags;
	Bool m_forceModelsToFollowTimeOfDay;
	Bool m_forceModelsToFollowWeather;

	TerrainLighting	m_terrainLighting[TIME_OF_DAY_COUNT][MAX_GLOBAL_LIGHTS];
	TerrainLighting	m_terrainObjectsLighting[TIME_OF_DAY_COUNT][MAX_GLOBAL_LIGHTS];

	//Settings for each global light
	RGBColor m_terrainAmbient[MAX_GLOBAL_LIGHTS];
	RGBColor m_terrainDiffuse[MAX_GLOBAL_LIGHTS];
	Coord3D m_terrainLightPos[MAX_GLOBAL_LIGHTS];

	Real m_infantryLightScale[TIME_OF_DAY_COUNT];
	Real m_scriptOverrideInfantryLightScale;

	Real m_soloPlayerHealthBonusForDifficulty[PLAYERTYPE_COUNT][DIFFICULTY_COUNT];

	Int m_maxVisibleTranslucentObjects;
	Int m_maxVisibleOccluderObjects;
	Int m_maxVisibleOccludeeObjects;
	Int m_maxVisibleNonOccluderOrOccludeeObjects;
	Real m_occludedLuminanceScale;

	Int m_numGlobalLights;	//number of active global lights
	Int m_maxRoadSegments;
	Int m_maxRoadVertex;
	Int m_maxRoadIndex;
	Int m_maxRoadTypes;

	Bool m_audioOn;
	Bool m_musicOn;
	Bool m_soundsOn;
	Bool m_sounds3DOn;
	Bool m_speechOn;
	Bool m_videoOn;
	Bool m_disableCameraMovement;

	Bool m_useFX;									///< If false, don't render effects
	Bool m_showClientPhysics;
	Bool m_showTerrainNormals;

	UnsignedInt m_noDraw;					///< Used to disable drawing, to profile game logic code.
	AIDebugOptions m_debugAI;			///< Used to display AI debug information
	Bool m_debugSupplyCenterPlacement; ///< Dumps to log everywhere it thinks about placing a supply center
	Bool m_debugAIObstacles;			///< Used to display AI obstacle debug information
	Bool m_showObjectHealth;			///< debug display object health
	Bool m_scriptDebug;						///< Should we attempt to load the script debugger window (.DLL)
	Bool m_particleEdit;					///< Should we attempt to load the particle editor (.DLL)
	Bool m_displayDebug;					///< Used to display display debug info
	Bool m_winCursors;						///< Should we force use of windows cursors?
	Bool m_constantDebugUpdate;		///< should we update the debug stats constantly, vs every 2 seconds?
	Bool m_showTeamDot;						///< Shows the little colored team dot representing which team you are controlling.

#ifdef DUMP_PERF_STATS
	Bool m_dumpPerformanceStatistics;
  Bool  m_dumpStatsAtInterval;///< should I automatically dump stats every N frames
  Int   m_statsInterval;       ///< if so, how many is N?
#endif

	Bool m_forceBenchmark;	///<forces running of CPU detection benchmark, even on known cpu's.

	Int m_fixedSeed;							///< fixed random seed for game logic (less than 0 to disable)

	Real m_particleScale;					///< Global size modifier for particle effects.

	AsciiString m_autoFireParticleSmallPrefix;
	AsciiString m_autoFireParticleSmallSystem;
	Int m_autoFireParticleSmallMax;
	AsciiString m_autoFireParticleMediumPrefix;
	AsciiString m_autoFireParticleMediumSystem;
	Int m_autoFireParticleMediumMax;
	AsciiString m_autoFireParticleLargePrefix;
	AsciiString m_autoFireParticleLargeSystem;
	Int m_autoFireParticleLargeMax;
	AsciiString m_autoSmokeParticleSmallPrefix;
	AsciiString m_autoSmokeParticleSmallSystem;
	Int m_autoSmokeParticleSmallMax;
	AsciiString m_autoSmokeParticleMediumPrefix;
	AsciiString m_autoSmokeParticleMediumSystem;
	Int m_autoSmokeParticleMediumMax;
	AsciiString m_autoSmokeParticleLargePrefix;
	AsciiString m_autoSmokeParticleLargeSystem;
	Int m_autoSmokeParticleLargeMax;
	AsciiString m_autoAflameParticlePrefix;
	AsciiString m_autoAflameParticleSystem;
	Int m_autoAflameParticleMax;

	// Latency insertion, packet loss for network debugging
	Int m_netMinPlayers;					///< Min players needed to start a net game

	UnsignedInt m_defaultIP;			///< preferred IP address for LAN
	UnsignedInt m_firewallBehavior;	///< Last detected firewall behavior
	Bool m_firewallSendDelay;			///< Use send delay for firewall connection negotiations
	UnsignedInt m_firewallPortOverride;	///< User-specified port to be used
	Short m_firewallPortAllocationDelta; ///< the port allocation delta last detected.

	Int m_baseValuePerSupplyBox;
	Real m_BuildSpeed;
	Real m_MinDistFromEdgeOfMapForBuild;
	Real m_SupplyBuildBorder;
	Real m_allowedHeightVariationForBuilding;  ///< how "flat" is still flat enough to build on
	Real m_MinLowEnergyProductionSpeed;
	Real m_MaxLowEnergyProductionSpeed;
	Real m_LowEnergyPenaltyModifier;
	Real m_MultipleFactory;
	Real m_RefundPercent;

	Real m_commandCenterHealRange;		///< radius in which close by ally things are healed
	Real m_commandCenterHealAmount;   ///< health per logic frame close by things are healed
	Int m_maxLineBuildObjects;				///< line style builds can be no longer than this
	Int m_maxTunnelCapacity;					///< Max people in Player's tunnel network
	Real m_horizontalScrollSpeedFactor;	///< Factor applied to the game screen scrolling speed.
	Real m_verticalScrollSpeedFactor;		///< Separated because of our aspect ratio
	Real m_scrollAmountCutoff;				///< Scroll speed to not adjust camera height
	Real m_cameraAdjustSpeed;					///< Rate at which we adjust camera height
	Bool m_enforceMaxCameraHeight;		///< Enforce max camera height while scrolling?
	Bool m_buildMapCache;
	AsciiString m_initialFile;				///< If this is specified, load a specific map from the command-line
	AsciiString m_pendingFile;				///< If this is specified, use this map at the next game start

	std::vector<AsciiString> m_simulateReplays; ///< If not empty, simulate this list of replays and exit.
	Int m_simulateReplayJobs; ///< Maximum number of processes to use for simulation, or SIMULATE_REPLAYS_SEQUENTIAL for sequential simulation

	Int m_maxParticleCount;						///< maximum number of particles that can exist
	Int m_maxFieldParticleCount;			///< maximum number of field-type particles that can exist (roughly)
	WeaponBonusSet* m_weaponBonusSet;
	Real m_healthBonus[LEVEL_COUNT];			///< global bonuses to health for veterancy.
	Real m_defaultStructureRubbleHeight;	///< for rubbled structures, compress height to this if none specified

	AsciiString m_shellMapName;				///< Holds the shell map name
	Bool m_shellMapOn;								///< User can set the shell map not to load
	Bool m_playIntro;									///< Flag to say if we're to play the intro or not
	Bool m_playSizzle;								///< Flag to say whether we play the sizzle movie after the logo movie.
	Bool m_afterIntro;								///< we need to tell the game our intro is done
	Bool m_allowExitOutOfMovies;			///< flag to allow exit out of movies only after the Intro has played

	Bool m_loadScreenRender;						///< flag to disallow rendering of almost everything during a loadscreen

	Real m_keyboardScrollFactor;			///< Factor applied to game scrolling speed via keyboard scrolling
	Real m_keyboardDefaultScrollFactor;			///< Factor applied to game scrolling speed via keyboard scrolling
	Bool m_drawScrollAnchor;					///< Set that the scroll anchor should be enabled
	Bool m_moveScrollAnchor;					///< set that the scroll anchor should move

	Bool m_animateWindows;						///< Should we animate window transitions?

	Bool m_incrementalAGPBuf;

	UnsignedInt m_iniCRC;							///< CRC of important INI files
	UnsignedInt m_exeCRC;							///< CRC of the executable

	BodyDamageType m_movementPenaltyDamageState;	///< at this body damage state, we have movement penalties

	Int m_groupSelectMinSelectSize;		// min number of units to treat as group select for audio feedback
	Real m_groupSelectVolumeBase;			// base volume for group select sound
	Real m_groupSelectVolumeIncrement;// increment to volume for selecting more units
	Int m_maxUnitSelectSounds;				// max number of select sounds to play per selection

	Real m_selectionFlashSaturationFactor; /// how colorful should the selection flash be? 0-4
	Bool m_selectionFlashHouseColor ;  /// skip the house color and just use white.

	Real m_cameraAudibleRadius;				///< If the camera is being used as the position of audio, then how far can we hear?
	Real m_groupMoveClickToGatherFactor; /** if you take all the selected units and calculate the smallest possible rectangle
																			 that contains them all, and click within that, all the selected units will break
																			 formation and gather at the point the user clicked (if the value is 1.0). If it's 0.0,
																			 units will always keep their formation. If it's <1.0, then the user must click a
																			 smaller area within the rectangle to order the gather. */

	UnsignedInt m_antiAliasLevel;          ///< value of selected antialias level in the game options
	UnsignedInt m_textureFilteringMode;       ///< value related to TextureFilterClass::TextureFilterModeEnum
	UnsignedInt m_textureAnisotropyLevel;     ///< value related to TextureFilterClass::AnisotropicFilterMode

	Bool m_languageFilterPref;        ///< Bool if user wants to filter language
	Bool m_loadScreenDemo;						///< Bool if true, run the loadscreen demo movie
	Bool m_disableRender;							///< if true, no rendering!

	Bool m_saveCameraInReplay;
	Bool m_useCameraInReplay;

	// TheSuperHackers @feature xezon 09/09/2025 Enable the shroud and everything related for observing individual players in any game mode.
	// Enabling this does have a performance cost because the ghost object manager must keep track of all relevant objects for all players.
	Bool m_enablePlayerObserver;

	// TheSuperHackers @feature L3-M 05/09/2025 allow the network latency counter and render fps counter font size to be set, a size of zero disables them
	Int m_networkLatencyFontSize;
	Int m_renderFpsFontSize;
	// TheSuperHackers @feature Mauller 21/06/2025 allow the system time and game time font size to be set, a size of zero disables them
	Int m_systemTimeFontSize;
	Int m_gameTimeFontSize;
	// TheSuperHackers @feature L3-M 05/11/2025 allow the player info list font size to be set, a size of zero disables it
	Int m_playerInfoListFontSize;

	// TheSuperHackers @feature L3-M 21/08/2025 toggle the money per minute display, false shows only the original current money
	Bool m_showMoneyPerMinute;
	Bool m_allowMoneyPerMinuteForPlayer;

	// TheSuperHackers @feature bobtista 28/06/2026 user-configurable speed multiplier for game window transitions
	Real m_gameWindowTransitionSpeedMultiplier;

	Real m_shakeSubtleIntensity;			///< Intensity for shaking a camera with SHAKE_SUBTLE
	Real m_shakeNormalIntensity;			///< Intensity for shaking a camera with SHAKE_NORMAL
	Real m_shakeStrongIntensity;			///< Intensity for shaking a camera with SHAKE_STRONG
	Real m_shakeSevereIntensity;			///< Intensity for shaking a camera with SHAKE_SEVERE
	Real m_shakeCineExtremeIntensity; ///< Intensity for shaking a camera with SHAKE_CINE_EXTREME
	Real m_shakeCineInsaneIntensity;  ///< Intensity for shaking a camera with SHAKE_CINE_INSANE
	Real m_maxShakeIntensity;					///< The maximum shake intensity we can have
	Real m_maxShakeRange;							///< The maximum shake range we can have

	Real m_sellPercentage;						///< when objects are sold, you get this much of the cost it would take to build it back
	Real m_baseRegenHealthPercentPerSecond;	///< auto healing for bases
	UnsignedInt m_baseRegenDelay;			///< delay in frames we must be damage free before we can auto heal

#ifdef ALLOW_SURRENDER
	Real m_prisonBountyMultiplier;		///< the cost of the unit is multiplied by this and given to the player when prisoners are returned to the a prison with KINDOF_COLLECTS_PRISON_BOUNTY
	Color m_prisonBountyTextColor;		///< color of the text that displays the money acquired at the prison
#endif

	Color m_hotKeyTextColor;					///< standard color for all hotkeys.

	AsciiString m_specialPowerViewObjectName;	///< Created when certain special powers are fired so players can watch.

	Real m_objectPlacementOpacity; ///< Sets the opacity of build preview objects.
	Bool m_objectPlacementShadows; ///< Enables or disables shadows of build preview objects.

	std::vector<AsciiString> m_standardPublicBones;

	Real m_standardMinefieldDensity;
	Real m_standardMinefieldDistance;


	Bool  m_showMetrics;								///< whether or not to show the metrics.
	Money m_defaultStartingCash;				///< The amount of cash a player starts with by default.

	Bool m_debugShowGraphicalFramerate;		///< Whether or not to show the graphical framerate bar.

	Int m_powerBarBase;										///< Logarithmic base for the power bar scale
	Real m_powerBarIntervals;							///< how many logarithmic intervals the width will be divided into
	Int m_powerBarYellowRange;						///< Red if consumption exceeds production, yellow if consumption this close but under, green if further under
	Real m_displayGamma;									///<display gamma that's adjusted with "brightness" control on options screen.

	UnsignedInt m_unlookPersistDuration;	///< How long after unlook until the sighting info executes the undo

	Bool m_shouldUpdateTGAToDDS;					///< Should we attempt to update old TGAs to DDS stuff on loadup?

	UnsignedInt m_doubleClickTimeMS;	///< What is the maximum amount of time that can separate two clicks in order
																		///< for us to generate a double click message?

	RGBColor m_shroudColor;						///< What color should the shroud be?  Remember, this is a lighting multiply, not an add
	UnsignedByte m_clearAlpha;				///< 255 means perfect visibility
	UnsignedByte m_fogAlpha;					///< 127 means fog is half as obscuring as shroud
	UnsignedByte m_shroudAlpha;				///< 0 makes this opaque, but they may get fancy

	// network timing values.
	UnsignedInt m_networkFPSHistoryLength;			///< The number of fps history entries
	UnsignedInt m_networkLatencyHistoryLength;	///< The number of ping history entries.
	UnsignedInt m_networkRunAheadMetricsTime;		///< The number of miliseconds between run ahead metrics things
	UnsignedInt m_networkCushionHistoryLength;	///< The number of cushion values to keep.
	UnsignedInt m_networkRunAheadSlack;					///< The amount of slack in the run ahead value.  This is the percentage of the calculated run ahead that is added.
	UnsignedInt m_networkKeepAliveDelay;				///< The number of seconds between when the connections to each player send a keep-alive packet.
	UnsignedInt m_networkDisconnectTime;				///< The number of milliseconds between when the game gets stuck on a frame for a network stall and when the disconnect dialog comes up.
	UnsignedInt m_networkPlayerTimeoutTime;			///< The number of milliseconds between when a player's last keep alive command was received and when they are considered disconnected from the game.
	UnsignedInt	m_networkDisconnectScreenNotifyTime; ///< The number of milliseconds between when the disconnect screen comes up and when the other players are notified that we are on the disconnect screen.

	Real				m_keyboardCameraRotateSpeed;    ///< How fast the camera rotates when rotated via keyboard controls.
  Int					m_playStats;									///< Int whether we want to log play stats or not, if <= 0 then we don't log

  Bool m_TiVOFastMode;            ///< When true, the client speeds up the framerate... set by HOTKEY!

#if defined(RTS_DEBUG) || ENABLE_CONFIGURABLE_SHROUD
	Bool m_shroudOn;
#endif

#if defined(RTS_DEBUG)
	Bool m_wireframe;
	Bool m_stateMachineDebug;
	Bool m_useCameraConstraints;
	Bool m_specialPowerUsesDelay;
	Bool m_fogOfWarOn;
	Bool m_jabberOn;
	Bool m_munkeeOn;
	Bool m_allowUnselectableSelection;			///< Are we allowed to select things that are unselectable?
	Bool m_disableCameraFade;								///< if true, script commands affecting camera are disabled
	Bool m_disableScriptedInputDisabling;		///< if true, script commands can't disable input
	Bool m_disableMilitaryCaption;					///< if true, military briefings go fast
	Int m_benchmarkTimer;										///< how long to play the game in benchmark mode?
	Bool m_checkForLeaks;
	Bool m_vTune;
	Bool m_debugCamera;						///< Used to display Camera debug information
	Bool m_debugVisibility;						///< Should we actively debug the visibility
	Int m_debugVisibilityTileCount;		///< How many tiles we should show when debugging visibility
	Real m_debugVisibilityTileWidth;	///< How wide should these tiles be?
	Int m_debugVisibilityTileDuration;	///< How long should these tiles stay around, in frames?
	Bool m_debugThreatMap;						///< Should we actively debug the threat map
	UnsignedInt m_maxDebugThreat;			///< This value (and any values greater) will appear full RED.
	Int m_debugThreatMapTileDuration;	///< How long should these tiles stay around, in frames?
	Bool m_debugCashValueMap;					///< Should we actively debug the threat map
	UnsignedInt m_maxDebugValue;			///< This value (and any values greater) will appear full GREEN.
	Int m_debugCashValueMapTileDuration;	///< How long should these tiles stay around, in frames?
	RGBColor m_debugVisibilityTargettableColor;	///< What color should the targettable cells be?
	RGBColor m_debugVisibilityDeshroudColor;			///< What color should the deshrouding cells be?
	RGBColor m_debugVisibilityGapColor;					///< What color should the gap generator cells be?
	Bool m_debugProjectilePath;						///< Should we actively debug the bezier paths on projectiles
	Real m_debugProjectileTileWidth;			///< How wide should these tiles be?
	Int m_debugProjectileTileDuration;		///< How long should these tiles stay around, in frames?
	RGBColor m_debugProjectileTileColor;	///< What color should these tiles be?
	Bool m_showCollisionExtents;	///< Used to display collision extents
  Bool m_showAudioLocations;    ///< Used to display audio markers and ambient sound radii
	Bool m_saveStats;
	Bool m_saveAllStats;
	Bool m_useLocalMOTD;
	AsciiString m_baseStatsDir;
	AsciiString m_MOTDPath;
	Int m_latencyAverage;					///< Average latency to insert
	Int m_latencyAmplitude;				///< Amplitude of sinusoidal modulation of latency
	Int m_latencyPeriod;					///< Period of sinusoidal modulation of latency
	Int m_latencyNoise;						///< Max amplitude of jitter to throw in
	Int m_packetLoss;							///< Percent of packets to drop
	Bool m_extraLogging;					///< More expensive debug logging to catch crashes.
#endif

#ifdef DEBUG_CRASHING
	Bool m_debugIgnoreAsserts;						///< Ignore all asserts.
#endif

#ifdef DEBUG_STACKTRACE
	Bool m_debugIgnoreStackTrace;					///< No stacktraces for asserts.
#endif

	Bool				m_isBreakableMovie;							///< if we enter a breakable movie, set this flag
	Bool				m_breakTheMovie;								///< The user has hit escape!

	AsciiString m_modDir;
	AsciiString m_modBIG;

	// the trailing '\' is included!
	AsciiString getPath_UserData() const;

private:

	static UnsignedInt generateExeCRC();

	static const FieldParse s_GlobalDataFieldParseTable[];

	// this is private, since we read the info from Windows and cache it for
	// future use. No one is allowed to change it, ever. (srj)
	AsciiString m_userDataDir;

	// just the "leaf name", read from INI. private because no one is ever allowed
	// to look at it directly; they must go thru getPath_UserData(). (srj)
	AsciiString m_userDataLeafName;
	static AsciiString BuildUserDataPathFromIni();

	static GlobalData *m_theOriginal;		///< the original global data instance (no overrides)
	GlobalData *m_next;									///< next instance (for overrides)
	virtual GlobalData *newOverride();		/** create a new override, copy data from previous
																			override, and return it */

#if defined(_MSC_VER) && _MSC_VER < 1300
	GlobalData(const GlobalData& that) { DEBUG_CRASH(("unimplemented")); }
	GlobalData& operator=(const GlobalData& that) { DEBUG_CRASH(("unimplemented")); return *this; }
#else
	GlobalData(const GlobalData& that) = delete;
	GlobalData& operator=(const GlobalData& that) = default;
#endif

};

// singleton
extern GlobalData* TheWritableGlobalData;

// use TheGlobalData for all read-only accesses
#if __cplusplus >= 201703L
inline const GlobalData* const& TheGlobalData = TheWritableGlobalData;
#else
#define TheGlobalData ((const GlobalData*)TheWritableGlobalData)
#endif
