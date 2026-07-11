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

// FILE: GUICommandTranslator.cpp /////////////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Translator for commands activated from the selection GUI, such as special unit
//				 actions, that require additional clicks in the world like selecting a target
//				 object or location
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ActionManager.h"
#include "Common/GameCommon.h"
#include "Common/GameAudio.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameText.h"
#include "Common/Geometry.h"
#include "GameClient/GUICommandTranslator.h"
#include "GameClient/CommandXlat.h"


// PRIVATE ////////////////////////////////////////////////////////////////////////////////////////
enum CommandStatus
{
	COMMAND_INCOMPLETE = 0,
	COMMAND_COMPLETE
};

// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////

PickAndPlayInfo::PickAndPlayInfo()
{
	m_air = FALSE;
	m_drawTarget = nullptr;
	m_weaponSlot = nullptr;
	m_specialPowerType = SPECIAL_INVALID;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GUICommandTranslator::GUICommandTranslator()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GUICommandTranslator::~GUICommandTranslator()
{

}

//-------------------------------------------------------------------------------------------------
/** Is the object under the mouse position a valid target for the command */
//-------------------------------------------------------------------------------------------------
static Object *validUnderCursor( const ICoord2D *mouse, const CommandButton *command, PickType pickType )
{
	Object *pickObj = nullptr;

	// pick a drawable at the mouse location
	Drawable *pick = TheTacticalView->pickDrawable( mouse, FALSE, pickType );

	// only continue if there is something there
	if( pick && pick->getObject() )
	{
		Player *player = ThePlayerList->getLocalPlayer();

		// get object we picked
		pickObj = pick->getObject();

		if (!command->isValidObjectTarget(player, pickObj))
				pickObj = nullptr;

	}


	return pickObj;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static CommandStatus doFireWeaponCommand( const CommandButton *command, const ICoord2D *mouse )
{

	// sanity
	if( command == nullptr || mouse == nullptr )
		return COMMAND_COMPLETE;

	//
	// for single object selections get the source ID and sanity check for illegal object and
	// bail along the way
	//
	ObjectID sourceID = INVALID_ID;
	if( TheInGameUI->getSelectCount() == 1 )
	{
		Drawable *draw = TheInGameUI->getFirstSelectedDrawable();

		// sanity
		if( draw == nullptr || draw->getObject() == nullptr )
			return COMMAND_COMPLETE;

		// get object id
		sourceID = draw->getObject()->getID();

	}

	// create message and send to the logic
	GameMessage *msg;
	if( BitIsSet( command->getOptions(), NEED_TARGET_POS ) )
	{
		Coord3D world;

		// translate the mouse location into world coords
		if( !TheTacticalView->screenToTerrain( mouse, &world ) )
			return COMMAND_COMPLETE;

		// create the message and append arguments
		msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_WEAPON_AT_LOCATION );
		msg->appendIntegerArgument( command->getWeaponSlot() );
		msg->appendLocationArgument( world );
		msg->appendIntegerArgument( command->getMaxShotsToFire() );

		//Also append the object ID (incase weapon doesn't like obstacles on land).
		Object *target = validUnderCursor( mouse, command, PICK_TYPE_SELECTABLE );
		ObjectID targetID = target ? target->getID() : INVALID_ID;
		msg->appendObjectIDArgument( targetID );
	}
	else if( BitIsSet( command->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET ) )
	{

		// setup the pick type ... some commands allow us to target shrubbery
		PickType pickType = PICK_TYPE_SELECTABLE;

		if( BitIsSet( command->getOptions(), ALLOW_SHRUBBERY_TARGET ) == TRUE )
			pickType = (PickType)((Int)pickType | (Int)PICK_TYPE_SHRUBBERY);

		if( BitIsSet( command->getOptions(), ALLOW_MINE_TARGET ) == TRUE )
			pickType = (PickType)((Int)pickType | (Int)PICK_TYPE_MINES);

		// get the target object under the cursor
		Object *target = validUnderCursor( mouse, command, pickType );

		// only continue if the object meets all the command criteria
		if( target )
		{

			msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_WEAPON_AT_OBJECT );
			msg->appendIntegerArgument( command->getWeaponSlot() );
			msg->appendObjectIDArgument( target->getID() );
			msg->appendIntegerArgument( command->getMaxShotsToFire() );

		}

	}
	else
	{
		msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_WEAPON );
		msg->appendIntegerArgument( command->getWeaponSlot() );
		msg->appendIntegerArgument( command->getMaxShotsToFire() );

		//This could be legit now -- think of firing a self destruct weapon
		//-----------------------------------------------------------------
		//DEBUG_CRASH( ("doFireWeaponCommand: Command options say it doesn't need additional user input '%s'",
		//											command->m_name.str()) );
		//return COMMAND_COMPLETE;

	}

	return COMMAND_COMPLETE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static CommandStatus doGuardCommand( const CommandButton *command, GuardMode guardMode, const ICoord2D *mouse )
{
	// sanity
	if( command == nullptr || mouse == nullptr )
		return COMMAND_COMPLETE;

	if( TheInGameUI->getSelectCount() == 0 )
		return COMMAND_COMPLETE;

	GameMessage *msg = nullptr;

	if ( msg == nullptr && BitIsSet( command->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET ) )
	{
		// get the target object under the cursor
		Object* target = validUnderCursor( mouse, command, PICK_TYPE_SELECTABLE );
		if( target )
		{
			msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_GUARD_OBJECT );
			msg->appendObjectIDArgument( target->getID() );
			msg->appendIntegerArgument(guardMode);
			pickAndPlayUnitVoiceResponse(TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_DO_GUARD_OBJECT);
		}
	}

	if(  msg == nullptr )
	{
		Coord3D world;
		if (BitIsSet( command->getOptions(), NEED_TARGET_POS ))
		{
			// translate the mouse location into world coords
			if( !TheTacticalView->screenToTerrain( mouse, &world ) )
				return COMMAND_COMPLETE;
		}
		else
		{
			Drawable *draw = TheInGameUI->getFirstSelectedDrawable();
			if( draw == nullptr || draw->getObject() == nullptr )
				return COMMAND_COMPLETE;
			world = *draw->getObject()->getPosition();
		}

		// create the message and append arguments
		msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_GUARD_POSITION );
		msg->appendLocationArgument(world);
		msg->appendIntegerArgument(guardMode);
		pickAndPlayUnitVoiceResponse(TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_DO_GUARD_POSITION);
	}

	return COMMAND_COMPLETE;

}

//-------------------------------------------------------------------------------------------------
/** Do the set rally point command */
//-------------------------------------------------------------------------------------------------
static CommandStatus doAttackMoveCommand( const CommandButton *command, const ICoord2D *mouse )
{

	// sanity
	if( command == nullptr || mouse == nullptr )
		return COMMAND_COMPLETE;

	//
	// we can only set rally points for structures ... and we never multiple select structures
	// so we must be sure there is only one thing selected (that thing we will set the point on)
	//
	Drawable *draw = TheInGameUI->getFirstSelectedDrawable();
	DEBUG_ASSERTCRASH( draw, ("doAttackMoveCommand: No selected object(s)") );

	// sanity
	if( draw == nullptr || draw->getObject() == nullptr )
		return COMMAND_COMPLETE;

	// convert mouse point to world coords
	Coord3D world;
	if( !TheTacticalView->screenToTerrain( mouse, &world ) )
		return COMMAND_COMPLETE;

	// send the message to set the rally point
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_ATTACKMOVETO );
	msg->appendLocationArgument( world );

	// Play the unit voice response
	pickAndPlayUnitVoiceResponse(TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_DO_ATTACKMOVETO);

	return COMMAND_COMPLETE;

}


//-------------------------------------------------------------------------------------------------
/** Do the set rally point command */
//-------------------------------------------------------------------------------------------------
static CommandStatus doSetRallyPointCommand( const CommandButton *command, const ICoord2D *mouse )
{

	// sanity
	if( command == nullptr || mouse == nullptr )
		return COMMAND_COMPLETE;

	//
	// we can only set rally points for structures ... and we never multiple select structures
	// so we must be sure there is only one thing selected (that thing we will set the point on)
	//
	DEBUG_ASSERTCRASH( TheInGameUI->getSelectCount() == 1,
										 ("doSetRallyPointCommand: The selected count is not 1, we can only set a rally point on a *SINGLE* building\n") );
	Drawable *draw = TheInGameUI->getFirstSelectedDrawable();
	DEBUG_ASSERTCRASH( draw, ("doSetRallyPointCommand: No selected object") );

	// sanity
	if( draw == nullptr || draw->getObject() == nullptr )
		return COMMAND_COMPLETE;

	// convert mouse point to world coords
	Coord3D world;
	if( !TheTacticalView->screenToTerrain( mouse, &world ) )
		return COMMAND_COMPLETE;

	// send the message to set the rally point
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_SET_RALLY_POINT );
	msg->appendObjectIDArgument( draw->getObject()->getID() );
	msg->appendLocationArgument( world );

	return COMMAND_COMPLETE;

}

//-------------------------------------------------------------------------------------------------
/** Do the beacon placement command */
//-------------------------------------------------------------------------------------------------
static CommandStatus doPlaceBeacon( const CommandButton *command, const ICoord2D *mouse )
{

	// sanity
	if( command == nullptr || mouse == nullptr )
		return COMMAND_COMPLETE;

	// convert mouse point to world coords
	Coord3D world;
	if( !TheTacticalView->screenToTerrain( mouse, &world ) )
		return COMMAND_COMPLETE;

	// send the message to set the rally point
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_PLACE_BEACON );
	msg->appendLocationArgument( world );

	return COMMAND_COMPLETE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GameMessageDisposition GUICommandTranslator::translateGameMessage(const GameMessage *msg)
{
	GameMessageDisposition disp = KEEP_MESSAGE;

	// only pay attention to clicks in this translator if there is a pending GUI command
	const CommandButton *command = TheInGameUI->getGUICommand();
	if( command == nullptr )
		return disp;

	switch( msg->getType() )
	{

		//---------------------------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
		{

			//
			//
			// it is necessary to use this input when there is a pending gui command, we don't wan't
			// it to fall through to the rest of the system when we're in pending gui command "mode"
			// because things like selection rectangles will start when we want to stay totally
			// within the gui command "mode" here
			//
			disp = DESTROY_MESSAGE;

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GameMessage::MSG_MOUSE_LEFT_DOUBLE_CLICK:
		case GameMessage::MSG_MOUSE_LEFT_CLICK:
		{
			CommandStatus commandStatus = COMMAND_COMPLETE;
			ICoord2D mouse = msg->getArgument(0)->pixelRegion.hi;

			// do the command action
			if( command && !command->isContextCommand() )
			{
				switch( command->getCommandType() )
				{

					//---------------------------------------------------------------------------------------
					case GUI_COMMAND_FIRE_WEAPON:
					{
						commandStatus = doFireWeaponCommand( command, &mouse );

						PickAndPlayInfo info;
						WeaponSlotType slot = command->getWeaponSlot();
						info.m_weaponSlot = &slot;

	 					pickAndPlayUnitVoiceResponse( TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_DO_WEAPON_AT_LOCATION, &info );
						break;

					}


					//---------------------------------------------------------------------------------------
					case GUI_COMMAND_EVACUATE:
					{
						if (BitIsSet(command->getOptions(), NEED_TARGET_POS)) {
							Coord3D worldPos;

							if( TheTacticalView->screenToTerrain(&mouse, &worldPos) )
							{
								GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_EVACUATE);
								msg->appendLocationArgument(worldPos);

								pickAndPlayUnitVoiceResponse( TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_EVACUATE );
							}

							commandStatus = COMMAND_COMPLETE;
						}

						break;
					}

					//---------------------------------------------------------------------------------------
					case GUI_COMMAND_GUARD:
					{
						commandStatus = doGuardCommand( command, GUARDMODE_NORMAL, &mouse );
						break;
					}

					//---------------------------------------------------------------------------------------
					case GUI_COMMAND_GUARD_WITHOUT_PURSUIT:
					{
						commandStatus = doGuardCommand( command, GUARDMODE_GUARD_WITHOUT_PURSUIT, &mouse );
						break;
					}

					//---------------------------------------------------------------------------------------
					case GUI_COMMAND_GUARD_FLYING_UNITS_ONLY:
					{
						commandStatus = doGuardCommand( command, GUARDMODE_GUARD_FLYING_UNITS_ONLY, &mouse );
						break;
					}

					//Special weapons are now always context commands...
					//---------------------------------------------------------------------------------------
					case GUI_COMMAND_SPECIAL_POWER:
					case GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT:
					{
						return KEEP_MESSAGE;
						break;

					}

					case GUI_COMMAND_ATTACK_MOVE:
					{
						commandStatus = doAttackMoveCommand( command, &mouse );
						break;
					}

					//---------------------------------------------------------------------------------------
					case GUI_COMMAND_SET_RALLY_POINT:
					{
						commandStatus = doSetRallyPointCommand( command, &mouse );
						break;

					}

					//---------------------------------------------------------------------------------------
					case GUICOMMANDMODE_PLACE_BEACON:
					{
						commandStatus = doPlaceBeacon( command, &mouse );
						break;

					}

				}

				// used the input
				disp = DESTROY_MESSAGE;

				// get out of GUI command mode if we completed the command one way or another
				if( commandStatus == COMMAND_COMPLETE )
				{
					TheInGameUI->setPreventLeftClickDeselectionInAlternateMouseModeForOneClick( TRUE );
					TheInGameUI->setGUICommand( nullptr );
				}
			}

			break;

		}

	}

	// If we're destroying the message, it means we used it. Therefore, destroy the current
	// attack move instruction as well.
	if (disp == DESTROY_MESSAGE)
		TheInGameUI->clearAttackMoveToMode();


	return disp;

}


