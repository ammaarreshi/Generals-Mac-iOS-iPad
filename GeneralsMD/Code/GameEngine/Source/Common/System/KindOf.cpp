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

// Kindof.cpp /////////////////////////////////////////////////////////////////////////////////////
// Part of header detangling
// John McDonald, Aug 2002

#include "PreRTS.h"

#include "Common/KindOf.h"
#include "Common/BitFlagsIO.h"

template<>
const char* const KindOfMaskType::s_bitNameList[] =
{
	"OBSTACLE",
	"SELECTABLE",
	"IMMOBILE",
	"CAN_ATTACK",
	"STICK_TO_TERRAIN_SLOPE",
	"CAN_CAST_REFLECTIONS",
	"SHRUBBERY",
	"STRUCTURE",
	"INFANTRY",
	"VEHICLE",
	"AIRCRAFT",
	"HUGE_VEHICLE",
	"DOZER",
	"HARVESTER",
	"COMMANDCENTER",
#ifdef ALLOW_SURRENDER
	"PRISON",
	"COLLECTS_PRISON_BOUNTY",
	"POW_TRUCK",
#endif
	"LINEBUILD",
	"SALVAGER",
	"WEAPON_SALVAGER",
	"TRANSPORT",
	"BRIDGE",
	"LANDMARK_BRIDGE",
	"BRIDGE_TOWER",
	"PROJECTILE",
	"PRELOAD",
	"NO_GARRISON",
	"WAVEGUIDE",
	"WAVE_EFFECT",
	"NO_COLLIDE",
	"REPAIR_PAD",
	"HEAL_PAD",
	"STEALTH_GARRISON",
	"CASH_GENERATOR",
#if RTS_GENERALS
	"AIRFIELD",
#endif
	"DRAWABLE_ONLY",
	"MP_COUNT_FOR_VICTORY",
	"REBUILD_HOLE",
	"SCORE",
	"SCORE_CREATE",
	"SCORE_DESTROY",
	"NO_HEAL_ICON",
	"CAN_RAPPEL",
	"PARACHUTABLE",
#ifdef ALLOW_SURRENDER
	"CAN_SURRENDER",
#endif
	"CAN_BE_REPULSED",
	"MOB_NEXUS",
	"IGNORED_IN_GUI",
	"CRATE",
	"CAPTURABLE",
	"CLEARED_BY_BUILD",
	"SMALL_MISSILE",
	"ALWAYS_VISIBLE",
	"UNATTACKABLE",
	"MINE",
	"CLEANUP_HAZARD",
	"PORTABLE_STRUCTURE",
	"ALWAYS_SELECTABLE",
	"ATTACK_NEEDS_LINE_OF_SIGHT",
	"WALK_ON_TOP_OF_WALL",
	"DEFENSIVE_WALL",
	"FS_POWER",
	"FS_FACTORY",
	"FS_BASE_DEFENSE",
	"FS_TECHNOLOGY",
	"AIRCRAFT_PATH_AROUND",
	"LOW_OVERLAPPABLE",
	"FORCEATTACKABLE",
	"AUTO_RALLYPOINT",
	"TECH_BUILDING",
	"POWERED",
	"PRODUCED_AT_HELIPAD",
	"DRONE",
	"CAN_SEE_THROUGH_STRUCTURE",
	"BALLISTIC_MISSILE",
	"CLICK_THROUGH",
	"SUPPLY_SOURCE_ON_PREVIEW",
	"PARACHUTE",
	"GARRISONABLE_UNTIL_DESTROYED",
	"BOAT",
	"IMMUNE_TO_CAPTURE",
	"HULK",
	"SHOW_PORTRAIT_WHEN_CONTROLLED",
	"SPAWNS_ARE_THE_WEAPONS",
	"CANNOT_BUILD_NEAR_SUPPLIES",
	"SUPPLY_SOURCE",
	"REVEAL_TO_ALL",
	"DISGUISER",
	"INERT",
	"HERO",
	"IGNORES_SELECT_ALL",
	"DONT_AUTO_CRUSH_INFANTRY",
	"CLIFF_JUMPER",
	"FS_SUPPLY_DROPZONE",
	"FS_SUPERWEAPON",
	"FS_BLACK_MARKET",
	"FS_SUPPLY_CENTER",
	"FS_STRATEGY_CENTER",
	"MONEY_HACKER",
	"ARMOR_SALVAGER",
  "REVEALS_ENEMY_PATHS",
	"BOOBY_TRAP",
	"FS_FAKE",
	"FS_INTERNET_CENTER",
  "BLAST_CRATER",
	"PROP",
	"OPTIMIZED_TREE",
	"FS_ADVANCED_TECH",
	"FS_BARRACKS",
	"FS_WARFACTORY",
	"FS_AIRFIELD",
	"AIRCRAFT_CARRIER",
	"NO_SELECT",
	"REJECT_UNMANNED",
	"CANNOT_RETALIATE",
	"TECH_BASE_DEFENSE",
	"EMP_HARDENED",
	"DEMOTRAP",
	"CONSERVATIVE_BUILDING",
	"IGNORE_DOCKING_BONES",

	nullptr
};
static_assert(ARRAY_SIZE(KindOfMaskType::s_bitNameList) == KindOfMaskType::NumBits + 1, "Incorrect array size");

static const Int fsList[] = {
	KINDOF_FS_FACTORY,
	KINDOF_FS_BASE_DEFENSE,
	KINDOF_FS_TECHNOLOGY,
	KINDOF_FS_SUPPLY_DROPZONE,
	KINDOF_FS_SUPERWEAPON,
	KINDOF_FS_BLACK_MARKET,
	KINDOF_FS_SUPPLY_CENTER,
	KINDOF_FS_STRATEGY_CENTER,
	KINDOF_FS_FAKE,
	KINDOF_FS_INTERNET_CENTER,
	KINDOF_FS_ADVANCED_TECH,
	KINDOF_FS_BARRACKS,
	KINDOF_FS_WARFACTORY,
	KINDOF_FS_AIRFIELD,
};

KindOfMaskType KINDOFMASK_NONE;	// inits to all zeroes
KindOfMaskType KINDOFMASK_FS(KindOfMaskType::kInit, fsList, ARRAY_SIZE(fsList));
