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

// FILE: MetaEvent.cpp ////////////////////////////////////////////////////////////////////////////
// Created:    Colin Day, September 2001
// Desc:       Translating keystrokes into event command messages
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameUtility.h"
#include "Common/INI.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingTemplate.h"

#include "GameClient/Drawable.h"
#include "GameClient/Mouse.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/ParticleSys.h"	// for ParticleSystemDebugDisplay
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/DebugDisplay.h"	// for AudioDebugDisplay
#include "GameClient/GameText.h"
#include "GameClient/MetaEvent.h"

#include "GameLogic/GameLogic.h" // for TheGameLogic->getFrame()


#define dont_DUMP_ALL_KEYS_TO_LOG


#ifdef DUMP_ALL_KEYS_TO_LOG
#include "GameClient/Keyboard.h"
#endif

MetaMap *TheMetaMap = nullptr;



// DEFINES ////////////////////////////////////////////////////////////////////

// PRIVATE TYPES //////////////////////////////////////////////////////////////////////////////////

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////

static const LookupListRec GameMessageMetaTypeNames[] =
{
	{ "SAVE_VIEW1",																GameMessage::MSG_META_SAVE_VIEW1 },
	{ "SAVE_VIEW2",																GameMessage::MSG_META_SAVE_VIEW2 },
	{ "SAVE_VIEW3",																GameMessage::MSG_META_SAVE_VIEW3 },
	{ "SAVE_VIEW4",																GameMessage::MSG_META_SAVE_VIEW4 },
	{ "SAVE_VIEW5",																GameMessage::MSG_META_SAVE_VIEW5 },
	{ "SAVE_VIEW6",																GameMessage::MSG_META_SAVE_VIEW6 },
	{ "SAVE_VIEW7",																GameMessage::MSG_META_SAVE_VIEW7 },
	{ "SAVE_VIEW8",																GameMessage::MSG_META_SAVE_VIEW8 },
	{ "VIEW_VIEW1",																GameMessage::MSG_META_VIEW_VIEW1 },
	{ "VIEW_VIEW2",																GameMessage::MSG_META_VIEW_VIEW2 },
	{ "VIEW_VIEW3",																GameMessage::MSG_META_VIEW_VIEW3 },
	{ "VIEW_VIEW4",																GameMessage::MSG_META_VIEW_VIEW4 },
	{ "VIEW_VIEW5",																GameMessage::MSG_META_VIEW_VIEW5 },
	{ "VIEW_VIEW6",																GameMessage::MSG_META_VIEW_VIEW6 },
	{ "VIEW_VIEW7",																GameMessage::MSG_META_VIEW_VIEW7 },
	{ "VIEW_VIEW8",																GameMessage::MSG_META_VIEW_VIEW8 },
	{ "CREATE_TEAM0",															GameMessage::MSG_META_CREATE_TEAM0 },
	{ "CREATE_TEAM1",															GameMessage::MSG_META_CREATE_TEAM1 },
	{ "CREATE_TEAM2",															GameMessage::MSG_META_CREATE_TEAM2 },
	{ "CREATE_TEAM3",															GameMessage::MSG_META_CREATE_TEAM3 },
	{ "CREATE_TEAM4",															GameMessage::MSG_META_CREATE_TEAM4 },
	{ "CREATE_TEAM5",															GameMessage::MSG_META_CREATE_TEAM5 },
	{ "CREATE_TEAM6",															GameMessage::MSG_META_CREATE_TEAM6 },
	{ "CREATE_TEAM7",															GameMessage::MSG_META_CREATE_TEAM7 },
	{ "CREATE_TEAM8",															GameMessage::MSG_META_CREATE_TEAM8 },
	{ "CREATE_TEAM9",															GameMessage::MSG_META_CREATE_TEAM9 },
	{ "SELECT_TEAM0",															GameMessage::MSG_META_SELECT_TEAM0 },
	{ "SELECT_TEAM1",															GameMessage::MSG_META_SELECT_TEAM1 },
	{ "SELECT_TEAM2",															GameMessage::MSG_META_SELECT_TEAM2 },
	{ "SELECT_TEAM3",															GameMessage::MSG_META_SELECT_TEAM3 },
	{ "SELECT_TEAM4",															GameMessage::MSG_META_SELECT_TEAM4 },
	{ "SELECT_TEAM5",															GameMessage::MSG_META_SELECT_TEAM5 },
	{ "SELECT_TEAM6",															GameMessage::MSG_META_SELECT_TEAM6 },
	{ "SELECT_TEAM7",															GameMessage::MSG_META_SELECT_TEAM7 },
	{ "SELECT_TEAM8",															GameMessage::MSG_META_SELECT_TEAM8 },
	{ "SELECT_TEAM9",															GameMessage::MSG_META_SELECT_TEAM9 },
	{ "ADD_TEAM0",																GameMessage::MSG_META_ADD_TEAM0 },
	{ "ADD_TEAM1",																GameMessage::MSG_META_ADD_TEAM1 },
	{ "ADD_TEAM2",																GameMessage::MSG_META_ADD_TEAM2 },
	{ "ADD_TEAM3",																GameMessage::MSG_META_ADD_TEAM3 },
	{ "ADD_TEAM4",																GameMessage::MSG_META_ADD_TEAM4 },
	{ "ADD_TEAM5",																GameMessage::MSG_META_ADD_TEAM5 },
	{ "ADD_TEAM6",																GameMessage::MSG_META_ADD_TEAM6 },
	{ "ADD_TEAM7",																GameMessage::MSG_META_ADD_TEAM7 },
	{ "ADD_TEAM8",																GameMessage::MSG_META_ADD_TEAM8 },
	{ "ADD_TEAM9",																GameMessage::MSG_META_ADD_TEAM9 },
	{ "VIEW_TEAM0",																GameMessage::MSG_META_VIEW_TEAM0 },
	{ "VIEW_TEAM1",																GameMessage::MSG_META_VIEW_TEAM1 },
	{ "VIEW_TEAM2",																GameMessage::MSG_META_VIEW_TEAM2 },
	{ "VIEW_TEAM3",																GameMessage::MSG_META_VIEW_TEAM3 },
	{ "VIEW_TEAM4",																GameMessage::MSG_META_VIEW_TEAM4 },
	{ "VIEW_TEAM5",																GameMessage::MSG_META_VIEW_TEAM5 },
	{ "VIEW_TEAM6",																GameMessage::MSG_META_VIEW_TEAM6 },
	{ "VIEW_TEAM7",																GameMessage::MSG_META_VIEW_TEAM7 },
	{ "VIEW_TEAM8",																GameMessage::MSG_META_VIEW_TEAM8 },
	{ "VIEW_TEAM9",																GameMessage::MSG_META_VIEW_TEAM9 },
	{ "SELECT_MATCHING_UNITS",										GameMessage::MSG_META_SELECT_MATCHING_UNITS },
	{ "SELECT_NEXT_UNIT",													GameMessage::MSG_META_SELECT_NEXT_UNIT },
	{ "SELECT_PREV_UNIT",													GameMessage::MSG_META_SELECT_PREV_UNIT },
	{ "SELECT_NEXT_WORKER",												GameMessage::MSG_META_SELECT_NEXT_WORKER },
	{ "SELECT_PREV_WORKER",												GameMessage::MSG_META_SELECT_PREV_WORKER },
	{ "SELECT_NEXT_IDLE_WORKER",											    GameMessage::MSG_META_SELECT_NEXT_IDLE_WORKER },
	{ "SELECT_HERO",												      GameMessage::MSG_META_SELECT_HERO },
	{ "SELECT_ALL",																GameMessage::MSG_META_SELECT_ALL },
	{ "SELECT_ALL_AIRCRAFT",											GameMessage::MSG_META_SELECT_ALL_AIRCRAFT },
	{ "VIEW_COMMAND_CENTER",											GameMessage::MSG_META_VIEW_COMMAND_CENTER },
	{ "VIEW_LAST_RADAR_EVENT",										GameMessage::MSG_META_VIEW_LAST_RADAR_EVENT },
	{ "SCATTER",																	GameMessage::MSG_META_SCATTER },
	{ "STOP",																			GameMessage::MSG_META_STOP },
	{ "DEPLOY",																		GameMessage::MSG_META_DEPLOY },
	{ "CREATE_FORMATION",													GameMessage::MSG_META_CREATE_FORMATION },
	{ "FOLLOW",																		GameMessage::MSG_META_FOLLOW },
	{ "CHAT_PLAYERS",															GameMessage::MSG_META_CHAT_PLAYERS },
	{ "CHAT_ALLIES",															GameMessage::MSG_META_CHAT_ALLIES },
	{ "CHAT_EVERYONE",														GameMessage::MSG_META_CHAT_EVERYONE },
	{ "DIPLOMACY",																GameMessage::MSG_META_DIPLOMACY },
	{ "PLACE_BEACON",															GameMessage::MSG_META_PLACE_BEACON },
	{ "DELETE_BEACON",														GameMessage::MSG_META_REMOVE_BEACON },
	{ "OPTIONS",																	GameMessage::MSG_META_OPTIONS },
	{ "INCREASE_MAX_RENDER_FPS",									GameMessage::MSG_META_INCREASE_MAX_RENDER_FPS },
	{ "DECREASE_MAX_RENDER_FPS",									GameMessage::MSG_META_DECREASE_MAX_RENDER_FPS },
	{ "INCREASE_LOGIC_TIME_SCALE",								GameMessage::MSG_META_INCREASE_LOGIC_TIME_SCALE },
	{ "DECREASE_LOGIC_TIME_SCALE",								GameMessage::MSG_META_DECREASE_LOGIC_TIME_SCALE },
	{ "TOGGLE_LOWER_DETAILS",											GameMessage::MSG_META_TOGGLE_LOWER_DETAILS },
	{ "TOGGLE_CONTROL_BAR",												GameMessage::MSG_META_TOGGLE_CONTROL_BAR },
	{ "TOGGLE_PLAYER_OBSERVER",										GameMessage::MSG_META_TOGGLE_PLAYER_OBSERVER },
	{ "BEGIN_PATH_BUILD",													GameMessage::MSG_META_BEGIN_PATH_BUILD },
	{ "END_PATH_BUILD",														GameMessage::MSG_META_END_PATH_BUILD },
	{ "BEGIN_FORCEATTACK",												GameMessage::MSG_META_BEGIN_FORCEATTACK },
	{ "END_FORCEATTACK",													GameMessage::MSG_META_END_FORCEATTACK },
	{ "BEGIN_FORCEMOVE",													GameMessage::MSG_META_BEGIN_FORCEMOVE },
	{ "END_FORCEMOVE",														GameMessage::MSG_META_END_FORCEMOVE },
	{ "BEGIN_WAYPOINTS",													GameMessage::MSG_META_BEGIN_WAYPOINTS },
	{ "END_WAYPOINTS",														GameMessage::MSG_META_END_WAYPOINTS },
	{ "BEGIN_PREFER_SELECTION",										GameMessage::MSG_META_BEGIN_PREFER_SELECTION },
	{ "END_PREFER_SELECTION",											GameMessage::MSG_META_END_PREFER_SELECTION },

	{ "TAKE_SCREENSHOT",													GameMessage::MSG_META_TAKE_SCREENSHOT },
	{ "ALL_CHEER",																GameMessage::MSG_META_ALL_CHEER },

	{ "BEGIN_CAMERA_ROTATE_LEFT",									GameMessage::MSG_META_BEGIN_CAMERA_ROTATE_LEFT },
	{ "END_CAMERA_ROTATE_LEFT",										GameMessage::MSG_META_END_CAMERA_ROTATE_LEFT },
	{ "BEGIN_CAMERA_ROTATE_RIGHT",								GameMessage::MSG_META_BEGIN_CAMERA_ROTATE_RIGHT },
	{ "END_CAMERA_ROTATE_RIGHT",									GameMessage::MSG_META_END_CAMERA_ROTATE_RIGHT },
	{ "BEGIN_CAMERA_ZOOM_IN",											GameMessage::MSG_META_BEGIN_CAMERA_ZOOM_IN },
	{ "END_CAMERA_ZOOM_IN",												GameMessage::MSG_META_END_CAMERA_ZOOM_IN },
	{ "BEGIN_CAMERA_ZOOM_OUT",										GameMessage::MSG_META_BEGIN_CAMERA_ZOOM_OUT },
	{ "END_CAMERA_ZOOM_OUT",											GameMessage::MSG_META_END_CAMERA_ZOOM_OUT },
	{ "CAMERA_RESET",															GameMessage::MSG_META_CAMERA_RESET },
#ifdef RTS_ZEROHOUR
	{ "TOGGLE_CAMERA_TRACKING_DRAWABLE",					GameMessage::MSG_META_TOGGLE_CAMERA_TRACKING_DRAWABLE },
#endif
	{ "TOGGLE_FAST_FORWARD_REPLAY",								GameMessage::MSG_META_TOGGLE_FAST_FORWARD_REPLAY },
	{ "TOGGLE_PAUSE",															GameMessage::MSG_META_TOGGLE_PAUSE },
	{ "TOGGLE_PAUSE_ALT",													GameMessage::MSG_META_TOGGLE_PAUSE_ALT },
	{ "STEP_FRAME",																GameMessage::MSG_META_STEP_FRAME },
	{ "STEP_FRAME_ALT",														GameMessage::MSG_META_STEP_FRAME_ALT },
	{ "DEMO_INSTANT_QUIT",												GameMessage::MSG_META_DEMO_INSTANT_QUIT },

#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)//may be defined in GameCommon.h
	{ "CHEAT_RUNSCRIPT1",								        	GameMessage::MSG_CHEAT_RUNSCRIPT1 },
	{ "CHEAT_RUNSCRIPT2",								        	GameMessage::MSG_CHEAT_RUNSCRIPT2 },
	{ "CHEAT_RUNSCRIPT3",								        	GameMessage::MSG_CHEAT_RUNSCRIPT3 },
	{ "CHEAT_RUNSCRIPT4",								        	GameMessage::MSG_CHEAT_RUNSCRIPT4 },
	{ "CHEAT_RUNSCRIPT5",								        	GameMessage::MSG_CHEAT_RUNSCRIPT5 },
	{ "CHEAT_RUNSCRIPT6",								        	GameMessage::MSG_CHEAT_RUNSCRIPT6 },
	{ "CHEAT_RUNSCRIPT7",								        	GameMessage::MSG_CHEAT_RUNSCRIPT7 },
	{ "CHEAT_RUNSCRIPT8",								        	GameMessage::MSG_CHEAT_RUNSCRIPT8 },
	{ "CHEAT_RUNSCRIPT9",								        	GameMessage::MSG_CHEAT_RUNSCRIPT9 },
	{ "CHEAT_TOGGLE_SPECIAL_POWER_DELAYS",	      GameMessage::MSG_CHEAT_TOGGLE_SPECIAL_POWER_DELAYS },
  { "CHEAT_SWITCH_TEAMS",							        	GameMessage::MSG_CHEAT_SWITCH_TEAMS },
	{ "CHEAT_KILL_SELECTION",						        	GameMessage::MSG_CHEAT_KILL_SELECTION },
	{ "CHEAT_TOGGLE_HAND_OF_GOD_MODE",		        GameMessage::MSG_CHEAT_TOGGLE_HAND_OF_GOD_MODE },
	{ "CHEAT_INSTANT_BUILD",							        GameMessage::MSG_CHEAT_INSTANT_BUILD },
	{ "CHEAT_DESHROUD",									          GameMessage::MSG_CHEAT_DESHROUD },
	{ "CHEAT_ADD_CASH",									          GameMessage::MSG_CHEAT_ADD_CASH },
	{ "CHEAT_GIVE_ALL_SCIENCES",					        GameMessage::MSG_CHEAT_GIVE_ALL_SCIENCES },
  { "CHEAT_GIVE_SCIENCEPURCHASEPOINTS",        	GameMessage::MSG_CHEAT_GIVE_SCIENCEPURCHASEPOINTS },
  { "CHEAT_SHOW_HEALTH",                        GameMessage::MSG_CHEAT_SHOW_HEALTH },
  { "CHEAT_TOGGLE_MESSAGE_TEXT",                GameMessage::MSG_CHEAT_TOGGLE_MESSAGE_TEXT },

#endif

#if defined(RTS_DEBUG)
	{ "HELP",																			GameMessage::MSG_META_HELP },

	{ "DEMO_TOGGLE_BEHIND_BUILDINGS",							GameMessage::MSG_META_DEMO_TOGGLE_BEHIND_BUILDINGS },
	{ "DEMO_LOD_DECREASE",												GameMessage::MSG_META_DEMO_LOD_DECREASE },
	{ "DEMO_LOD_INCREASE",												GameMessage::MSG_META_DEMO_LOD_INCREASE },
	{ "DEMO_TOGGLE_LETTERBOX",										GameMessage::MSG_META_DEMO_TOGGLE_LETTERBOX },
	{ "DEMO_TOGGLE_MESSAGE_TEXT",									GameMessage::MSG_META_DEMO_TOGGLE_MESSAGE_TEXT },

	{ "DEMO_GIVE_ALL_SCIENCES",										GameMessage::MSG_META_DEMO_GIVE_ALL_SCIENCES },
	{ "DEMO_GIVE_RANKLEVEL",											GameMessage::MSG_META_DEMO_GIVE_RANKLEVEL },
	{ "DEMO_TAKE_RANKLEVEL",											GameMessage::MSG_META_DEMO_TAKE_RANKLEVEL },
	{ "DEMO_GIVE_SCIENCEPURCHASEPOINTS",					GameMessage::MSG_META_DEMO_GIVE_SCIENCEPURCHASEPOINTS },
	{ "DEMO_SWITCH_TEAMS",												GameMessage::MSG_META_DEMO_SWITCH_TEAMS },
	{ "DEMO_SWITCH_TEAMS_CHINA_USA",							GameMessage::MSG_META_DEMO_SWITCH_TEAMS_BETWEEN_CHINA_USA },
	{ "DEMO_TOGGLE_CASHMAPDEBUG",									GameMessage::MSG_META_DEMO_TOGGLE_CASHMAPDEBUG },
	{ "DEMO_TOGGLE_GRAPHICALFRAMERATEBAR",				GameMessage::MSG_META_DEMO_TOGGLE_GRAPHICALFRAMERATEBAR },
	{ "DEMO_TOGGLE_PARTICLEDEBUG",								GameMessage::MSG_META_DEMO_TOGGLE_PARTICLEDEBUG },
	{ "DEMO_TOGGLE_THREATDEBUG",									GameMessage::MSG_META_DEMO_TOGGLE_THREATDEBUG },
	{ "DEMO_TOGGLE_VISIONDEBUG",									GameMessage::MSG_META_DEMO_TOGGLE_VISIONDEBUG },
	{ "DEMO_TOGGLE_PROJECTILEDEBUG",							GameMessage::MSG_META_DEMO_TOGGLE_PROJECTILEDEBUG },
	{ "DEMO_LOD_DECREASE",												GameMessage::MSG_META_DEMO_LOD_DECREASE },
	{ "DEMO_LOD_INCREASE",												GameMessage::MSG_META_DEMO_LOD_INCREASE },
	{ "DEMO_TOGGLE_SHADOW_VOLUMES",								GameMessage::MSG_META_DEMO_TOGGLE_SHADOW_VOLUMES },
	{ "DEMO_TOGGLE_FOGOFWAR",											GameMessage::MSG_META_DEMO_TOGGLE_FOGOFWAR },
	{ "DEMO_KILL_ALL_ENEMIES",										GameMessage::MSG_META_DEMO_KILL_ALL_ENEMIES },
	{ "DEMO_KILL_SELECTION",											GameMessage::MSG_META_DEMO_KILL_SELECTION },
	{ "DEMO_TOGGLE_HURT_ME_MODE",									GameMessage::MSG_META_DEMO_TOGGLE_HURT_ME_MODE },
	{ "DEMO_TOGGLE_HAND_OF_GOD_MODE",							GameMessage::MSG_META_DEMO_TOGGLE_HAND_OF_GOD_MODE },
	{ "DEMO_DEBUG_SELECTION",											GameMessage::MSG_META_DEMO_DEBUG_SELECTION },
	{ "DEMO_LOCK_CAMERA_TO_SELECTION",						GameMessage::MSG_META_DEMO_LOCK_CAMERA_TO_SELECTION },
	{ "DEMO_TOGGLE_SOUND",												GameMessage::MSG_META_DEMO_TOGGLE_SOUND },
	{ "DEMO_TOGGLE_TRACKMARKS",										GameMessage::MSG_META_DEMO_TOGGLE_TRACKMARKS },
	{ "DEMO_TOGGLE_WATERPLANE",										GameMessage::MSG_META_DEMO_TOGGLE_WATERPLANE },
	{ "DEMO_TIME_OF_DAY",													GameMessage::MSG_META_DEMO_TIME_OF_DAY },
	{ "DEMO_TOGGLE_MILITARY_SUBTITLES",						GameMessage::MSG_META_DEMO_TOGGLE_MILITARY_SUBTITLES },
	{ "DEMO_TOGGLE_MUSIC",												GameMessage::MSG_META_DEMO_TOGGLE_MUSIC },
	{ "DEMO_MUSIC_NEXT_TRACK",										GameMessage::MSG_META_DEMO_MUSIC_NEXT_TRACK },
	{ "DEMO_MUSIC_PREV_TRACK",										GameMessage::MSG_META_DEMO_MUSIC_PREV_TRACK },
	{ "DEMO_NEXT_OBJECTIVE_MOVIE",								GameMessage::MSG_META_DEMO_NEXT_OBJECTIVE_MOVIE },
	{ "DEMO_PLAY_OBJECTIVE_MOVIE1",								GameMessage::MSG_META_DEMO_PLAY_OBJECTIVE_MOVIE1 },
	{ "DEMO_PLAY_OBJECTIVE_MOVIE2",								GameMessage::MSG_META_DEMO_PLAY_OBJECTIVE_MOVIE2 },
	{ "DEMO_PLAY_OBJECTIVE_MOVIE3",								GameMessage::MSG_META_DEMO_PLAY_OBJECTIVE_MOVIE3 },
	{ "DEMO_PLAY_OBJECTIVE_MOVIE4",								GameMessage::MSG_META_DEMO_PLAY_OBJECTIVE_MOVIE4 },
	{ "DEMO_PLAY_OBJECTIVE_MOVIE5",								GameMessage::MSG_META_DEMO_PLAY_OBJECTIVE_MOVIE5 },
	{ "DEMO_PLAY_OBJECTIVE_MOVIE6",								GameMessage::MSG_META_DEMO_PLAY_OBJECTIVE_MOVIE6 },
	{ "DEMO_BEGIN_ADJUST_PITCH",									GameMessage::MSG_META_DEMO_BEGIN_ADJUST_PITCH },
	{ "DEMO_END_ADJUST_PITCH",										GameMessage::MSG_META_DEMO_END_ADJUST_PITCH },
	{ "DEMO_BEGIN_ADJUST_DEFAULTPITCH",						GameMessage::MSG_META_DEMO_BEGIN_ADJUST_DEFAULTPITCH },
	{ "DEMO_END_ADJUST_DEFAULTPITCH",							GameMessage::MSG_META_DEMO_END_ADJUST_DEFAULTPITCH },
	{ "DEMO_BEGIN_ADJUST_FOV",										GameMessage::MSG_META_DEMO_BEGIN_ADJUST_FOV },
	{ "DEMO_END_ADJUST_FOV",											GameMessage::MSG_META_DEMO_END_ADJUST_FOV },
	{ "DEMO_LOCK_CAMERA_TO_PLANES",								GameMessage::MSG_META_DEMO_LOCK_CAMERA_TO_PLANES },
	{ "DEMO_REMOVE_PREREQ",												GameMessage::MSG_META_DEMO_REMOVE_PREREQ },
	{ "DEMO_RUNSCRIPT1",													GameMessage::MSG_META_DEMO_RUNSCRIPT1 },
	{ "DEMO_RUNSCRIPT2",													GameMessage::MSG_META_DEMO_RUNSCRIPT2 },
	{ "DEMO_RUNSCRIPT3",													GameMessage::MSG_META_DEMO_RUNSCRIPT3 },
	{ "DEMO_RUNSCRIPT4",													GameMessage::MSG_META_DEMO_RUNSCRIPT4 },
	{ "DEMO_RUNSCRIPT5",													GameMessage::MSG_META_DEMO_RUNSCRIPT5 },
	{ "DEMO_RUNSCRIPT6",													GameMessage::MSG_META_DEMO_RUNSCRIPT6 },
	{ "DEMO_RUNSCRIPT7",													GameMessage::MSG_META_DEMO_RUNSCRIPT7 },
	{ "DEMO_RUNSCRIPT8",													GameMessage::MSG_META_DEMO_RUNSCRIPT8 },
	{ "DEMO_RUNSCRIPT9",													GameMessage::MSG_META_DEMO_RUNSCRIPT9 },
	{ "DEMO_ADDCASH",															GameMessage::MSG_META_DEMO_ADD_CASH },
	{ "DEMO_TOGGLE_RENDER",												GameMessage::MSG_META_DEMO_TOGGLE_RENDER },
	{ "DEMO_TOGGLE_BW_VIEW",											GameMessage::MSG_META_DEMO_TOGGLE_BW_VIEW },
	{ "DEMO_TOGGLE_RED_VIEW",											GameMessage::MSG_META_DEMO_TOGGLE_RED_VIEW },
	{ "DEMO_TOGGLE_GREEN_VIEW",										GameMessage::MSG_META_DEMO_TOGGLE_GREEN_VIEW },
	{ "DEMO_TOGGLE_MOTION_BLUR_ZOOM",							GameMessage::MSG_META_DEMO_TOGGLE_MOTION_BLUR_ZOOM },
	{ "DEMO_SHOW_EXTENTS",												GameMessage::MSG_META_DEBUG_SHOW_EXTENTS },
  { "DEMO_SHOW_AUDIO_LOCATIONS",								GameMessage::MSG_META_DEBUG_SHOW_AUDIO_LOCATIONS },
  { "DEMO_SHOW_HEALTH",													GameMessage::MSG_META_DEBUG_SHOW_HEALTH },
	{ "DEMO_GIVE_VETERANCY",											GameMessage::MSG_META_DEBUG_GIVE_VETERANCY },
	{ "DEMO_TAKE_VETERANCY",											GameMessage::MSG_META_DEBUG_TAKE_VETERANCY },
	{ "DEMO_BATTLE_CRY",													GameMessage::MSG_META_DEMO_BATTLE_CRY },
#ifdef ALLOW_SURRENDER
	{ "DEMO_TEST_SURRENDER",											GameMessage::MSG_META_DEMO_TEST_SURRENDER },
#endif
	{ "DEMO_TOGGLE_AVI",													GameMessage::MSG_META_DEMO_TOGGLE_AVI },
	{ "DEMO_PLAY_CAMEO_MOVIE",										GameMessage::MSG_META_DEMO_PLAY_CAMEO_MOVIE },
	{ "DEMO_TOGGLE_ZOOM_LOCK",										GameMessage::MSG_META_DEMO_TOGGLE_ZOOM_LOCK },
	{ "DEMO_TOGGLE_SPECIAL_POWER_DELAYS",					GameMessage::MSG_META_DEMO_TOGGLE_SPECIAL_POWER_DELAYS },

	{ "DEMO_TOGGLE_METRICS",											GameMessage::MSG_META_DEMO_TOGGLE_METRICS},
	{ "DEMO_DESHROUD",														GameMessage::MSG_META_DEMO_DESHROUD },
	{ "DEMO_ENSHROUD",														GameMessage::MSG_META_DEMO_ENSHROUD },
	{ "DEMO_TOGGLE_AI_DEBUG",											GameMessage::MSG_META_DEMO_TOGGLE_AI_DEBUG },
	{ "DEMO_TOGGLE_SUPPLY_CENTER_PLACEMENT",			GameMessage::MSG_META_DEMO_TOGGLE_SUPPLY_CENTER_PLACEMENT },
	{ "DEMO_TOGGLE_NO_DRAW",											GameMessage::MSG_NO_DRAW },
	{ "DEMO_CYCLE_LOD_LEVEL",											GameMessage::MSG_META_DEMO_CYCLE_LOD_LEVEL },
	{ "DEMO_DUMP_ASSETS",													GameMessage::MSG_META_DEBUG_DUMP_ASSETS},

	{ "DEMO_INSTANT_BUILD",												GameMessage::MSG_META_DEMO_INSTANT_BUILD },
	{ "DEMO_TOGGLE_CAMERA_DEBUG",									GameMessage::MSG_META_DEMO_TOGGLE_CAMERA_DEBUG },

	/// Begin VTUNE
	{ "DEMO_VTUNE_ON",														GameMessage::MSG_META_DEBUG_VTUNE_ON },
	{ "DEMO_VTUNE_OFF",														GameMessage::MSG_META_DEBUG_VTUNE_OFF },
	/// End VTUNE


	//lorenzen's feather water
	{ "DEMO_TOGGLE_FEATHER_WATER",								GameMessage::MSG_META_DEBUG_TOGGLE_FEATHER_WATER },

	{ "DEMO_INCR_ANIM_SKATE_SPEED",								GameMessage::MSG_META_DEBUG_INCR_ANIM_SKATE_SPEED },
	{ "DEMO_DECR_ANIM_SKATE_SPEED",								GameMessage::MSG_META_DEBUG_DECR_ANIM_SKATE_SPEED },
	{ "DEMO_CYCLE_EXTENT_TYPE",										GameMessage::MSG_META_DEBUG_CYCLE_EXTENT_TYPE },
	{ "DEMO_INCR_EXTENT_MAJOR",										GameMessage::MSG_META_DEBUG_INCREASE_EXTENT_MAJOR },
	{ "DEMO_DECR_EXTENT_MAJOR",										GameMessage::MSG_META_DEBUG_DECREASE_EXTENT_MAJOR },
	{ "DEMO_INCR_EXTENT_MAJOR_LARGE",							GameMessage::MSG_META_DEBUG_INCREASE_EXTENT_MAJOR_BIG },
	{ "DEMO_DECR_EXTENT_MAJOR_LARGE",							GameMessage::MSG_META_DEBUG_DECREASE_EXTENT_MAJOR_BIG },
	{ "DEMO_INCR_EXTENT_MINOR",										GameMessage::MSG_META_DEBUG_INCREASE_EXTENT_MINOR },
	{ "DEMO_DECR_EXTENT_MINOR",										GameMessage::MSG_META_DEBUG_DECREASE_EXTENT_MINOR },
	{ "DEMO_INCR_EXTENT_MINOR_LARGE",							GameMessage::MSG_META_DEBUG_INCREASE_EXTENT_MINOR_BIG },
	{ "DEMO_DECR_EXTENT_MINOR_LARGE",							GameMessage::MSG_META_DEBUG_DECREASE_EXTENT_MINOR_BIG },
	{ "DEMO_INCR_EXTENT_HEIGHT",									GameMessage::MSG_META_DEBUG_INCREASE_EXTENT_HEIGHT },
	{ "DEMO_DECR_EXTENT_HEIGHT",									GameMessage::MSG_META_DEBUG_DECREASE_EXTENT_HEIGHT },
	{ "DEMO_INCR_EXTENT_HEIGHT_LARGE",						GameMessage::MSG_META_DEBUG_INCREASE_EXTENT_HEIGHT_BIG },
	{ "DEMO_DECR_EXTENT_HEIGHT_LARGE",						GameMessage::MSG_META_DEBUG_DECREASE_EXTENT_HEIGHT_BIG },
	{ "DEMO_TOGGLE_NETWORK",											GameMessage::MSG_META_DEBUG_TOGGLE_NETWORK },
	{ "DEBUG_DUMP_PLAYER_OBJECTS",								GameMessage::MSG_META_DEBUG_DUMP_PLAYER_OBJECTS },
	{ "DEBUG_DUMP_ALL_PLAYER_OBJECTS",						GameMessage::MSG_META_DEBUG_DUMP_ALL_PLAYER_OBJECTS },
	{ "DEMO_WIN",																	GameMessage::MSG_META_DEBUG_WIN },
	{ "DEMO_TOGGLE_DEBUG_STATS",									GameMessage::MSG_META_DEMO_TOGGLE_DEBUG_STATS },
	{ "DEBUG_OBJECT_ID_PERFORMANCE",							GameMessage::MSG_META_DEBUG_OBJECT_ID_PERFORMANCE },
	{ "DEBUG_DRAWABLE_ID_PERFORMANCE",						GameMessage::MSG_META_DEBUG_DRAWABLE_ID_PERFORMANCE },
	{ "DEBUG_SLEEPY_UPDATE_PERFORMANCE",					GameMessage::MSG_META_DEBUG_SLEEPY_UPDATE_PERFORMANCE },
#endif // defined(RTS_DEBUG)


#if defined(RTS_DEBUG)
	{ "DEMO_TOGGLE_AUDIODEBUG",										GameMessage::MSG_META_DEMO_TOGGLE_AUDIODEBUG },
#endif//defined(RTS_DEBUG)
#ifdef DUMP_PERF_STATS
	{ "DEMO_PERFORM_STATISTICAL_DUMP",						GameMessage::MSG_META_DEMO_PERFORM_STATISTICAL_DUMP },
#endif//DUMP_PERF_STATS


	{ nullptr, 0	}
};


///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static const FieldParse TheMetaMapFieldParseTable[] =
{

	{ "Key",								INI::parseLookupList,						KeyNames, offsetof( MetaMapRec, m_key ) },
	{ "Transition",					INI::parseLookupList,						TransitionNames, offsetof( MetaMapRec, m_transition ) },
	{ "Modifiers",					INI::parseLookupList,						ModifierNames, offsetof( MetaMapRec, m_modState ) },
	{ "UseableIn",					INI::parseBitString32,					TheCommandUsableInNames, offsetof( MetaMapRec, m_usableIn ) },
	{ "Category",						INI::parseLookupList,						CategoryListName, offsetof( MetaMapRec, m_category ) },
	{ "Description",				INI::parseAndTranslateLabel,		nullptr, offsetof( MetaMapRec, m_description ) },
	{ "DisplayName",				INI::parseAndTranslateLabel,		nullptr, offsetof( MetaMapRec, m_displayName ) },

	{ nullptr,									nullptr,														nullptr, 0 }

};

// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
MetaEventTranslator::MetaEventTranslator() :
	m_lastKeyDown(MK_NONE),
	m_lastModState(0)
{
	for (Int i = 0; i < NUM_MOUSE_BUTTONS; ++i) {
		m_nextUpShouldCreateDoubleClick[i] = FALSE;
	}


}

//-------------------------------------------------------------------------------------------------
MetaEventTranslator::~MetaEventTranslator()
{
}

//-------------------------------------------------------------------------------------------------
static const char * findGameMessageNameByType(GameMessage::Type type)
{
	for (const LookupListRec* metaNames = GameMessageMetaTypeNames; metaNames->name; metaNames++)
		if (metaNames->value == (Int)type)
			return metaNames->name;

	DEBUG_CRASH(("MetaTypeName %d not found -- did you remember to add it to GameMessageMetaTypeNames[] ?", (Int)type));
	return "???";
}

//-------------------------------------------------------------------------------------------------
static Bool isMessageUsable(CommandUsableInType usableIn)
{
	// We will ignore all commands if the game client has not yet incremented to frame 1.
	// It prevents the user from doing commands during a map load, which throws the input
	// system into whack because there isn't a client frame for the input event, and in
	// the case of a command that pauses the game, like the quit menu, the client frame
	// will never get beyond 0 and we lose the ability to process any input.
	if (TheGameClient->getFrame() == 0)
		return false;

	const Bool usableInShell = (usableIn & COMMANDUSABLE_SHELL);
	const Bool usableInGame = (usableIn & COMMANDUSABLE_GAME);
	const Bool usableAsObserver = (usableIn & COMMANDUSABLE_OBSERVER);
	const Bool isShellActive = TheShell && TheShell->isShellActive();
	const Bool isObserving = !ThePlayerList->getLocalPlayer()->isPlayerActive();

	if (usableInShell && isShellActive)
		return true;

	if (usableInGame && !isShellActive)
		return true;

	if (usableAsObserver && isObserving)
		return true;

	return false;
}

//-------------------------------------------------------------------------------------------------
GameMessageDisposition MetaEventTranslator::translateGameMessage(const GameMessage *msg)
{
	GameMessageDisposition disp = KEEP_MESSAGE;
	const GameMessage::Type t = msg->getType();

	if (t == GameMessage::MSG_RAW_KEY_DOWN || t == GameMessage::MSG_RAW_KEY_UP)
	{
		onKeyEvent(msg, disp);
	}
	else if (t > GameMessage::MSG_RAW_MOUSE_BEGIN && t < GameMessage::MSG_RAW_MOUSE_END )
	{
		onMouseEvent(msg);
	}

	return disp;
}

//-------------------------------------------------------------------------------------------------
void MetaEventTranslator::onMouseEvent(const GameMessage *msg)
{
	Int index = 3;
	switch (msg->getType())
	{
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
			--index;
			FALLTHROUGH;
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_BUTTON_DOWN:
			--index;
			FALLTHROUGH;
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN:
		{
			--index;
			m_mouseDownPosition[index] = msg->getArgument(0)->pixel;
			m_nextUpShouldCreateDoubleClick[index] = FALSE;
			break;
		}

		case GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK:
			--index;
			FALLTHROUGH;
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_DOUBLE_CLICK:
			--index;
			FALLTHROUGH;
		case GameMessage::MSG_RAW_MOUSE_RIGHT_DOUBLE_CLICK:
		{
			--index;
			m_nextUpShouldCreateDoubleClick[index] = TRUE;
			break;
		}

		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP:
			--index;
			FALLTHROUGH;
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_BUTTON_UP:
			--index;
			FALLTHROUGH;
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP:
		{
			--index;

			constexpr const GameMessage::Type SingleClickMessages[3] =
			{
				GameMessage::MSG_MOUSE_LEFT_CLICK,
				GameMessage::MSG_MOUSE_MIDDLE_CLICK,
				GameMessage::MSG_MOUSE_RIGHT_CLICK,
			};
			constexpr const GameMessage::Type DoubleClickMessages[3] =
			{
				GameMessage::MSG_MOUSE_LEFT_DOUBLE_CLICK,
				GameMessage::MSG_MOUSE_MIDDLE_DOUBLE_CLICK,
				GameMessage::MSG_MOUSE_RIGHT_DOUBLE_CLICK,
			};

			const ICoord2D location = msg->getArgument(0)->pixel;
			const GameMessage::Type messageType = m_nextUpShouldCreateDoubleClick[index] ? DoubleClickMessages[index] : SingleClickMessages[index];
			GameMessage *newMessage = TheMessageStream->insertMessage(messageType, const_cast<GameMessage*>(msg));

			IRegion2D pixelRegion;
			buildRegion( &m_mouseDownPosition[index], &location, &pixelRegion );
			if (abs(pixelRegion.hi.x - pixelRegion.lo.x) < TheMouse->m_dragTolerance &&
					abs(pixelRegion.hi.y - pixelRegion.lo.y) < TheMouse->m_dragTolerance)
			{
				pixelRegion.hi.x = pixelRegion.lo.x;
				pixelRegion.hi.y = pixelRegion.lo.y;
			}

			newMessage->appendPixelRegionArgument( pixelRegion );

			// append the modifier keys to the message.
			newMessage->appendIntegerArgument( msg->getArgument(1)->integer );

			// append the time to the message.
			//newMessage->appendIntegerArgument( msg->getArgument(2)->integer );
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
// GeneralsX @refactor fbraz3 06/05/2026 — adopt upstream's structural split (#2758)
// into a single onKeyEvent() helper while keeping GeneralsX's pre-existing
// input semantics: a "mods-only-changed" branch using m_lastModState and a
// m_lastKeyDown-tracking normal key-transition branch. We do not adopt
// upstream's m_keyDownInfos / KeyDownInfo bit-array (introduced for the
// "ignore order of modifier release" fix in #2577) because that fix is not
// part of GeneralsX's behavior, and the bit-array member is not declared
// in MetaEvent.h.
void MetaEventTranslator::onKeyEvent(const GameMessage *msg, GameMessageDisposition &disp)
{
	const Int systemKey = msg->getArgument(0)->integer;
	const Int systemKeyState = msg->getArgument(1)->integer;

	const MappableKeyType key = (MappableKeyType)systemKey;

	// for our purposes here, we don't care to distinguish between right and left keys,
	// so just fudge a little to simplify things.
	Int newModState = 0;

	if (systemKeyState & KEY_STATE_CONTROL)
	{
		newModState |= CTRL;
	}

	if (systemKeyState & KEY_STATE_SHIFT)
	{
		newModState |= SHIFT;
	}

	if (systemKeyState & KEY_STATE_ALT)
	{
		newModState |= ALT;
	}

	for (const MetaMapRec *map = TheMetaMap->getFirstMetaMapRec(); map; map = map->m_next)
	{
		if (!isMessageUsable(map->m_usableIn))
			continue;

		// check for the special case of mods-only-changed.
		if (
				map->m_key == MK_NONE &&
				newModState != m_lastModState &&
				(
					(map->m_transition == UP && map->m_modState == m_lastModState) ||
					(map->m_transition == DOWN && map->m_modState == newModState)
				)
			)
		{
			//DEBUG_LOG(("Frame %d: MetaEventTranslator::translateGameMessage() Mods-only change: %s", TheGameLogic->getFrame(), findGameMessageNameByType(map->m_meta)));
			/*GameMessage *metaMsg =*/ TheMessageStream->appendMessage(map->m_meta);
			disp = DESTROY_MESSAGE;
			break;
		}

		// ok, now check for "normal" key transitions.
		if (
				map->m_key == key &&
				map->m_modState == newModState &&
				(
					(map->m_transition == UP && (systemKeyState & KEY_STATE_UP)) ||
					(map->m_transition == DOWN && (systemKeyState & KEY_STATE_DOWN)) //||
					//(map->m_transition == DOUBLEDOWN && (systemKeyState & KEY_STATE_DOWN) && m_lastKeyDown == key)
				)
			)
		{

			if (systemKeyState & KEY_STATE_AUTOREPEAT)
			{
				// if it's an autorepeat of a "known" key, don't generate the meta-event,
				// but DO eat the keystroke so no one else can mess with it
				//DEBUG_LOG(("Frame %d: MetaEventTranslator::translateGameMessage() auto-repeat: %s", TheGameLogic->getFrame(), findGameMessageNameByType(map->m_meta)));
			}
			else
			{

        // THIS IS A GREASY HACK... MESSAGE SHOULD BE HANDLED IN A TRANSLATOR, BUT DURING CINEMATICS THE TRANSLATOR IS DISABLED
        if (map->m_meta == GameMessage::MSG_META_TOGGLE_FAST_FORWARD_REPLAY)
	      {
				#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)//may be defined in GameCommon.h
			      if (TheGlobalData)
				#else
				  if (TheGlobalData && TheGameLogic->isInReplayGame())
				#endif
			      {
	            if (TheWritableGlobalData)
                TheWritableGlobalData->m_TiVOFastMode = 1 - TheGlobalData->m_TiVOFastMode;

              if (TheInGameUI)
								TheInGameUI->messageNoFormat( TheGlobalData->m_TiVOFastMode
									? TheGameText->FETCH_OR_SUBSTITUTE("GUI:FF_ON", L"Fast Forward is on")
									: TheGameText->FETCH_OR_SUBSTITUTE("GUI:FF_OFF", L"Fast Forward is off")
								);
			      }
			      disp = KEEP_MESSAGE; // cause for goodness sake, this key gets used a lot by non-replay hotkeys
			      break;
	      }


				/*GameMessage *metaMsg =*/ TheMessageStream->appendMessage(map->m_meta);
				//DEBUG_LOG(("Frame %d: MetaEventTranslator::translateGameMessage() normal: %s", TheGameLogic->getFrame(), findGameMessageNameByType(map->m_meta)));
			}
			disp = DESTROY_MESSAGE;
			break;
		}
	}

	if (msg->getType() == GameMessage::MSG_RAW_KEY_DOWN)
  {
		m_lastKeyDown = key;


#ifdef DUMP_ALL_KEYS_TO_LOG

	          WideChar Wkey = TheKeyboard->getPrintableKey(key, 0);
	          UnicodeString uKey;
	          uKey.set(&Wkey);
	          AsciiString aKey;
	          aKey.translate(uKey);
  	          DEBUG_LOG(("^%s ", aKey.str()));
#endif

  }



  m_lastModState = newModState;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
MetaMap::MetaMap() :
	m_metaMaps(nullptr)
{
}

//-------------------------------------------------------------------------------------------------
MetaMap::~MetaMap()
{
	while (m_metaMaps)
	{
		MetaMapRec *next = m_metaMaps->m_next;
		deleteInstance(m_metaMaps);
		m_metaMaps = next;
	}
}

//-------------------------------------------------------------------------------------------------
GameMessage::Type MetaMap::findGameMessageMetaType(const char* name)
{
	for (const LookupListRec* metaNames = GameMessageMetaTypeNames; metaNames->name; metaNames++)
		if (stricmp(metaNames->name, name) == 0)
			return (GameMessage::Type)metaNames->value;

	DEBUG_CRASH(("MetaTypeName %s not found -- did you remember to add it to GameMessageMetaTypeNames[] ?", name));
	return GameMessage::MSG_INVALID;
}

//-------------------------------------------------------------------------------------------------
MetaMapRec *MetaMap::getMetaMapRec(GameMessage::Type t)
{
	for (MetaMapRec *map = m_metaMaps; map; map = map->m_next)
	{
		if (map->m_meta == t)
			return map;
	}

	// not found.. create a new one.
	MetaMapRec *m = newInstance(MetaMapRec);
	m->m_meta = t;
	m->m_key = MK_NONE;
	m->m_transition = DOWN;
	m->m_modState = NONE;
	m->m_usableIn = COMMANDUSABLE_NONE;
	m->m_category = CATEGORY_MISC;
	m->m_description.clear();
	m->m_displayName.clear();
	m->m_next = m_metaMaps;
	m_metaMaps = m;

	return m;
}

//-------------------------------------------------------------------------------------------------
/*static */ void MetaMap::parseMetaMap(INI* ini)
{
	// read and ignore the meta-map name
	const char *c = ini->getNextToken();

	GameMessage::Type t = TheMetaMap->findGameMessageMetaType(c);
	if (t == GameMessage::MSG_INVALID)
		throw INI_INVALID_DATA;

	MetaMapRec *map = TheMetaMap->getMetaMapRec(t);
	if (map == nullptr)
		throw INI_INVALID_DATA;

	ini->initFromINI(map, TheMetaMapFieldParseTable);
}

//-------------------------------------------------------------------------------------------------
void MetaMap::generateMetaMap()
{
	// TheSuperHackers @info A default mapping for MSG_META_SELECT_ALL_AIRCRAFT would be useful for Generals
	// but is not recommended, because it will cause key mapping conflicts with original game languages.

	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_INCREASE_MAX_RENDER_FPS);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_KPPLUS;
			map->m_transition = DOWN;
			map->m_modState = CTRL;
			map->m_usableIn = COMMANDUSABLE_EVERYWHERE;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_DECREASE_MAX_RENDER_FPS);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_KPMINUS;
			map->m_transition = DOWN;
			map->m_modState = CTRL;
			map->m_usableIn = COMMANDUSABLE_EVERYWHERE;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_INCREASE_LOGIC_TIME_SCALE);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_KPPLUS;
			map->m_transition = DOWN;
			map->m_modState = SHIFT_CTRL;
			map->m_usableIn = COMMANDUSABLE_EVERYWHERE;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_DECREASE_LOGIC_TIME_SCALE);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_KPMINUS;
			map->m_transition = DOWN;
			map->m_modState = SHIFT_CTRL;
			map->m_usableIn = COMMANDUSABLE_EVERYWHERE;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_TOGGLE_PLAYER_OBSERVER);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_M;
			map->m_transition = DOWN;
			map->m_modState = NONE;
			map->m_usableIn = COMMANDUSABLE_OBSERVER;
		}
	}
	{
		// Is mostly useful for Generals.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_TOGGLE_FAST_FORWARD_REPLAY);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_F;
			map->m_transition = DOWN;
			map->m_modState = NONE;
			map->m_usableIn = COMMANDUSABLE_GAME; // @todo COMMANDUSABLE_OBSERVER
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_TOGGLE_PAUSE);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_P;
			map->m_transition = DOWN;
			map->m_modState = NONE;
			map->m_usableIn = COMMANDUSABLE_OBSERVER;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_TOGGLE_PAUSE_ALT);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_P;
			map->m_transition = DOWN;
			map->m_modState = SHIFT; // Requires modifier to avoid key conflicts as a player.
			map->m_usableIn = COMMANDUSABLE_EVERYWHERE;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_STEP_FRAME);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_O;
			map->m_transition = DOWN;
			map->m_modState = NONE;
			map->m_usableIn = COMMANDUSABLE_OBSERVER;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_STEP_FRAME_ALT);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_O;
			map->m_transition = DOWN;
			map->m_modState = SHIFT; // Requires modifier to avoid key conflicts as a player.
			map->m_usableIn = COMMANDUSABLE_EVERYWHERE;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_SELECT_NEXT_IDLE_WORKER);
		if (map->m_key == MK_NONE) {
			map->m_key = MK_I;
			map->m_transition = DOWN;
			map->m_modState = CTRL;
			map->m_usableIn = COMMANDUSABLE_GAME;
			map->m_category = CATEGORY_SELECTION;
			map->m_description = TheGameText->FETCH_OR_SUBSTITUTE("GUI:SelectNextIdleWorkerDescription", L"Select the next idle worker");
			map->m_displayName = TheGameText->FETCH_OR_SUBSTITUTE("GUI:SelectNextIdleWorker", L"Next Idle Worker");
		}
	}
	{
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_ALT_CAMERA_ROTATE_LEFT);
		if (map->m_key == MK_NONE) {
			map->m_key = MK_KP4;
			map->m_transition = DOWN;
			map->m_modState = CTRL;
			map->m_usableIn = COMMANDUSABLE_GAME;
		}
	}
	{
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_ALT_CAMERA_ROTATE_RIGHT);
		if (map->m_key == MK_NONE) {
			map->m_key = MK_KP6;
			map->m_transition = DOWN;
			map->m_modState = CTRL;
			map->m_usableIn = COMMANDUSABLE_GAME;
		}
	}

#if defined(RTS_DEBUG)
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_DEMO_REMOVE_PREREQ);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_P;
			map->m_transition = DOWN;
			map->m_modState = ALT;
			map->m_usableIn = COMMANDUSABLE_GAME;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = getMetaMapRec(GameMessage::MSG_META_DEMO_FREE_BUILD);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_B;
			map->m_transition = DOWN;
			map->m_modState = ALT;
			map->m_usableIn = COMMANDUSABLE_GAME;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = TheMetaMap->getMetaMapRec(GameMessage::MSG_META_DEMO_BEGIN_ADJUST_DEFAULTPITCH);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_COMMA;
			map->m_transition = DOWN;
			map->m_modState = CTRL;
			map->m_usableIn = COMMANDUSABLE_GAME;
		}
	}
	{
		// Is useful for Generals and Zero Hour.
		MetaMapRec *map = TheMetaMap->getMetaMapRec(GameMessage::MSG_META_DEMO_END_ADJUST_DEFAULTPITCH);
		if (map->m_key == MK_NONE)
		{
			map->m_key = MK_COMMA;
			map->m_transition = UP;
			map->m_modState = CTRL;
			map->m_usableIn = COMMANDUSABLE_GAME;
		}
	}
#endif // defined(RTS_DEBUG)
}

//-------------------------------------------------------------------------------------------------
void MetaMap::verifyMetaMap()
{
#ifdef DEBUG_CRASHING
	for (const MetaMapRec *map = getFirstMetaMapRec(); map; map = map->m_next)
	{
		DEBUG_ASSERTCRASH(
			map->m_meta > GameMessage::MSG_BEGIN_META_MESSAGES &&
			map->m_meta < GameMessage::MSG_END_META_MESSAGES,
			("hmm, expected only meta-msgs here"));
	}
#endif
}

//-------------------------------------------------------------------------------------------------
/*static*/ void INI::parseMetaMapDefinition( INI* ini )
{
	MetaMap::parseMetaMap(ini);
}

