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

// HintSpy.cpp
// The HintSpy sits on the message stream and watches for certain messages,
// for which it then generates visual "hints".
// Author: Michael S. Booth, March 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/MessageStream.h"
#include "GameClient/HintSpy.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameClient.h"
#include "GameClient/Drawable.h"
/**
 * This message handler displays UI "hints" (ie: a rectangle for drag selection) based
 * upon the messages that pass through it.
 */
GameMessageDisposition HintSpyTranslator::translateGameMessage(const GameMessage *msg)
{
	GameMessageDisposition disp = KEEP_MESSAGE;

	/// @todo Create an automated way to associate method callbacks with messages
	switch( msg->getType() )
	{
		//-----------------------------------------------------------------------------
		case GameMessage::MSG_MOUSEOVER_DRAWABLE_HINT:
			{
				TheInGameUI->createMouseoverHint( msg );

				disp = DESTROY_MESSAGE; //hint no longer needed by anyone.  Eat it.
			}
			break;
		case GameMessage::MSG_MOUSEOVER_LOCATION_HINT:
			{
				TheInGameUI->createMouseoverHint( msg );

				disp = DESTROY_MESSAGE; //hint no longer needed by anyone.  Eat it.
			}
			break;

		//-----------------------------------------------------------------------------
		case GameMessage::MSG_DEFECTOR_HINT:
		case GameMessage::MSG_DO_MOVETO_HINT:
		case GameMessage::MSG_DO_ATTACKMOVETO_HINT:
		case GameMessage::MSG_DO_ATTACK_OBJECT_HINT:
		case GameMessage::MSG_DO_ATTACK_OBJECT_AFTER_MOVING_HINT:
		case GameMessage::MSG_DO_FORCE_ATTACK_OBJECT_HINT:
		case GameMessage::MSG_DO_FORCE_ATTACK_GROUND_HINT:
		case GameMessage::MSG_ADD_WAYPOINT_HINT:
		case GameMessage::MSG_GET_REPAIRED_HINT:
		case GameMessage::MSG_DOCK_HINT:
		case GameMessage::MSG_GET_HEALED_HINT:
		case GameMessage::MSG_DO_REPAIR_HINT:
		case GameMessage::MSG_RESUME_CONSTRUCTION_HINT:
		case GameMessage::MSG_ENTER_HINT:
		case GameMessage::MSG_HIJACK_HINT:
#ifdef RTS_ZEROHOUR
		case GameMessage::MSG_SABOTAGE_HINT:
#endif
		case GameMessage::MSG_CONVERT_TO_CARBOMB_HINT:
#ifdef ALLOW_SURRENDER
		case GameMessage::MSG_PICK_UP_PRISONER_HINT:
#endif
		case GameMessage::MSG_VALID_GUICOMMAND_HINT:
		case GameMessage::MSG_INVALID_GUICOMMAND_HINT:
		case GameMessage::MSG_CAPTUREBUILDING_HINT:
		case GameMessage::MSG_HACK_HINT:
		case GameMessage::MSG_SET_RALLY_POINT_HINT:
		case GameMessage::MSG_IMPOSSIBLE_ATTACK_HINT:
		case GameMessage::MSG_DO_SPECIAL_POWER_OVERRIDE_DESTINATION_HINT:
		case GameMessage::MSG_DO_SALVAGE_HINT:
		case GameMessage::MSG_DO_INVALID_HINT:
			TheInGameUI->createCommandHint( msg );
			disp = DESTROY_MESSAGE; //hint no longer needed by anyone.  Eat it.
			break;

		//-----------------------------------------------------------------------------
		case GameMessage::MSG_BEGIN_AREA_SELECTION_HINT:
			TheInGameUI->beginAreaSelectHint( msg );
			disp = DESTROY_MESSAGE;
			break;

		//-----------------------------------------------------------------------------
		// A BEGIN_AREA_SELECTION_HINT is always followed by an END_AREA_SELECTION_HINT, so
		// watch for it to stop hinting.
		case GameMessage::MSG_END_AREA_SELECTION_HINT:
			TheInGameUI->endAreaSelectHint( msg );
			disp = DESTROY_MESSAGE;
			break;

		//-----------------------------------------------------------------------------
		case GameMessage::MSG_DO_MOVETO:
		case GameMessage::MSG_DO_ATTACKMOVETO:
		case GameMessage::MSG_DO_FORCEMOVETO:
		case GameMessage::MSG_ADD_WAYPOINT:
			TheInGameUI->createMoveHint( msg );
			break;

		//-----------------------------------------------------------------------------
		case GameMessage::MSG_DO_ATTACK_OBJECT:
			TheInGameUI->createAttackHint( msg );
			break;

		//-----------------------------------------------------------------------------
		case GameMessage::MSG_DO_FORCE_ATTACK_GROUND:
		case GameMessage::MSG_DO_FORCE_ATTACK_OBJECT:
			TheInGameUI->createForceAttackHint( msg );
			break;

		//-----------------------------------------------------------------------------
		case GameMessage::MSG_ENTER:
			TheInGameUI->createGarrisonHint( msg );
			break;

	}
	return disp;
}
