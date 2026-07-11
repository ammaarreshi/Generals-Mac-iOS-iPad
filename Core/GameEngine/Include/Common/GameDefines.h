/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#pragma once

#include "WWDefines.h"

// Note: Retail compatibility must not be broken before this project officially does.
// Use RETAIL_COMPATIBLE_CRC and RETAIL_COMPATIBLE_XFER_SAVE to guard breaking changes.

#ifndef PRESERVE_BUILDING_RESUMPTION_DELAY
#define PRESERVE_BUILDING_RESUMPTION_DELAY (0) // The fix for this unfavorable behavior was approved by the Game Design Committee.
#endif

#ifndef PRESERVE_CHINOOK_PASSENGER_DUMPING
#define PRESERVE_CHINOOK_PASSENGER_DUMPING (1)
#endif

#ifndef PRESERVE_HARDCODED_BLACK_LOTUS_CASH_HACK
#define PRESERVE_HARDCODED_BLACK_LOTUS_CASH_HACK (1)
#endif

#ifndef PRESERVE_MULTI_CRATE_PICKUP
#define PRESERVE_MULTI_CRATE_PICKUP (0) // The fix for this unfavorable behavior was approved by the Game Design Committee.
#endif

#ifndef PRESERVE_NO_XP_FROM_FLAME_KILLS
#define PRESERVE_NO_XP_FROM_FLAME_KILLS (0) // The fix for this unfavorable behavior was approved by the Game Design Committee.
#endif

#ifndef PRESERVE_NO_XP_FROM_OCL_KILLS
#define PRESERVE_NO_XP_FROM_OCL_KILLS (1)
#endif

#ifndef PRESERVE_NO_XP_FROM_POISON_KILLS
#define PRESERVE_NO_XP_FROM_POISON_KILLS (1)
#endif

#ifndef PRESERVE_OCCUPANT_DETECTION_VIA_DRAG_SELECTION
#define PRESERVE_OCCUPANT_DETECTION_VIA_DRAG_SELECTION (1)
#endif

#ifndef PRESERVE_PERPETUAL_HORDE_BONUS
#define PRESERVE_PERPETUAL_HORDE_BONUS (1)
#endif

#ifndef PRESERVE_PREMATURE_BATTLE_BUS_DEATH
#define PRESERVE_PREMATURE_BATTLE_BUS_DEATH (1)
#endif

#ifndef PRESERVE_RADAR_WARNING_SUPPRESSION
#define PRESERVE_RADAR_WARNING_SUPPRESSION (1)
#endif

#ifndef PRESERVE_STRUCTURE_STEALTH_DURING_REPAIR
#define PRESERVE_STRUCTURE_STEALTH_DURING_REPAIR (0) // The fix for this unfavorable behavior was approved by the Game Design Committee.
#endif

#ifndef PRESERVE_TUNNEL_HEAL_STACKING
#define PRESERVE_TUNNEL_HEAL_STACKING (1)
#endif

#ifndef PRESERVE_UNRELIABLE_FIRESTORMS
#define PRESERVE_UNRELIABLE_FIRESTORMS (0) // The fix for this unfavorable behavior was approved by the Game Design Committee.
#endif

#ifndef PRESERVE_RETAIL_SCRIPTED_CAMERA
#define PRESERVE_RETAIL_SCRIPTED_CAMERA (1) // Retain scripted camera behavior present in retail Generals 1.08 and Zero Hour 1.04
#endif

#ifndef RETAIL_COMPATIBLE_CRC
#define RETAIL_COMPATIBLE_CRC (1) // Game is expected to be CRC compatible with retail Generals 1.08, Zero Hour 1.04
#endif

#ifndef RETAIL_COMPATIBLE_XFER_SAVE
#define RETAIL_COMPATIBLE_XFER_SAVE (1) // Game is expected to be Xfer Save compatible with retail Generals 1.08, Zero Hour 1.04
#endif

// This is here to easily toggle between the retail compatible with fixed pathfinding fallback and pure fixed pathfinding mode
#ifndef RETAIL_COMPATIBLE_PATHFINDING
#define RETAIL_COMPATIBLE_PATHFINDING (1)
#endif

// This is here to easily toggle between the retail compatible pathfinding memory allocation and the new static allocated data mode
#ifndef RETAIL_COMPATIBLE_PATHFINDING_ALLOCATION
#define RETAIL_COMPATIBLE_PATHFINDING_ALLOCATION (1)
#endif

#ifndef RETAIL_COMPATIBLE_CIRCLE_FILL_ALGORITHM
#define RETAIL_COMPATIBLE_CIRCLE_FILL_ALGORITHM (1) // Use the original circle fill algorithm, which is more efficient but less accurate
#endif

// Disable non retail fixes in the networking, such as putting more data per UDP packet
#ifndef RETAIL_COMPATIBLE_NETWORKING
#define RETAIL_COMPATIBLE_NETWORKING (1)
#endif

// This is essentially synonymous for RETAIL_COMPATIBLE_CRC. There is a lot wrong with AIGroup, such as use-after-free, double-free, leaks,
// but we cannot touch it much without breaking retail compatibility. Do not shy away from using massive hacks when fixing issues with AIGroup,
// but put them behind this macro.

#ifndef RETAIL_COMPATIBLE_AIGROUP
#define RETAIL_COMPATIBLE_AIGROUP (1) // AIGroup logic is expected to be CRC compatible with retail Generals 1.08, Zero Hour 1.04
#endif

#ifndef ENABLE_GAMETEXT_SUBSTITUTES
#define ENABLE_GAMETEXT_SUBSTITUTES (1) // The code can provide substitute texts when labels and strings are missing in the STR or CSF translation file
#endif

#ifndef ALLOW_MONEY_PER_MINUTE_FOR_PLAYER
#define ALLOW_MONEY_PER_MINUTE_FOR_PLAYER (0) // When enabled, a money-per-minute stat is calculated and displayed in-game
#endif

// Previously the configurable shroud sat behind #if defined(RTS_DEBUG)
// Enable the configurable shroud to properly draw the terrain in World Builder without RTS_DEBUG compiled in.
// Disable the configurable shroud to make shroud hacking a bit less accessible in Release game builds.
#ifndef ENABLE_CONFIGURABLE_SHROUD
#define ENABLE_CONFIGURABLE_SHROUD (1) // When enabled, the GlobalData contains a field to turn on/off the shroud, otherwise shroud is always enabled
#endif

// Enable buffered IO in File System. Was disabled in retail game.
// Buffered IO generally is much faster than unbuffered for small reads and writes.
#ifndef USE_BUFFERED_IO
#define USE_BUFFERED_IO (1)
#endif

// Enable cache for local file existence. Reduces amount of disk accesses for better performance,
// but decreases file existence correctness and runtime stability, if a cached file is deleted on runtime.
#ifndef ENABLE_FILESYSTEM_EXISTENCE_CACHE
#define ENABLE_FILESYSTEM_EXISTENCE_CACHE (1)
#endif

// Enable prioritization of textures by size. This will improve the texture quality of 481 textures in Zero Hour
// by using the larger resolution textures from Generals. Content wise these textures are identical.
#ifndef PRIORITIZE_TEXTURES_BY_SIZE
#define PRIORITIZE_TEXTURES_BY_SIZE (1)
#endif

// Enable obsolete code. This mainly refers to code that existed in Generals but was removed in GeneralsMD.
// Disable and remove this when Generals and GeneralsMD are merged.
#if RTS_GENERALS
#ifndef USE_OBSOLETE_GENERALS_CODE
#define USE_OBSOLETE_GENERALS_CODE (1)
#endif
#endif

// Overwrite window settings until wnd data files are adapted or fixed.
#ifndef ENABLE_GUI_HACKS
#define ENABLE_GUI_HACKS (1)
#endif

// Tell our computer identity in the LAN lobby. Disable for privacy.
// Was enabled in the retail game and exposed the computer login and host names.
#ifdef RTS_DEBUG
#ifndef TELL_COMPUTER_IDENTITY_IN_LAN_LOBBY
#define TELL_COMPUTER_IDENTITY_IN_LAN_LOBBY (1)
#endif
#endif

#define MIN_DISPLAY_BIT_DEPTH       16
#define DEFAULT_DISPLAY_BIT_DEPTH   32
#define DEFAULT_DISPLAY_WIDTH      1024 // The standard resolution this game was designed for
#define DEFAULT_DISPLAY_HEIGHT     768 // The standard resolution this game was designed for
