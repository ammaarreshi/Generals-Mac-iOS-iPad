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

// FILE: ControlBarCommandProcessing.cpp //////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   This file contain just the method responsible for processing the actual command
//				 clicks from the window controls in the UI
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/BuildAssistant.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/PlayerTemplate.h"

#include "GameClient/CommandXlat.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/Eva.h"
#include "GameClient/GameClient.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/AnimateWindowManager.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ProductionUpdate.h"



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
struct SelectObjectsInfo
{
	const ThingTemplate *thingTemplate;
	GameMessage *msg;
};

//-------------------------------------------------------------------------------------------------

static void selectObjectOfType( Object* obj, void* selectObjectsInfo )
{
	SelectObjectsInfo *soInfo = (SelectObjectsInfo*)selectObjectsInfo;

	//Do the templates match?
	if( obj->getTemplate()->isEquivalentTo( soInfo->thingTemplate ) )
	{
		//Okay, then add it to the selected group.
		soInfo->msg->appendObjectIDArgument( obj->getID() );

		Drawable *draw = obj->getDrawable();
		if( draw )
		{
			TheInGameUI->selectDrawable( draw );
		}
	}
}
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
/** Process a button transition message from the window system that should be for one of
	* our GUI commands */
//-------------------------------------------------------------------------------------------------
CBCommandStatus ControlBar::processCommandTransitionUI( GameWindow *control, GadgetGameMessage gadgetMessage )
{
	// sanity, we won't process messages if we have no source object
	if( m_currContext != CB_CONTEXT_MULTI_SELECT &&
			(m_currentSelectedDrawable == nullptr ||
			 m_currentSelectedDrawable->getObject() == nullptr) )
	{

		if( m_currContext != CB_CONTEXT_NONE &&
				m_currContext != CB_CONTEXT_OBSERVER_INFO &&
				m_currContext != CB_CONTEXT_OBSERVER_LIST)
			switchToContext( CB_CONTEXT_NONE, nullptr );
		return CBC_COMMAND_NOT_USED;

	}

	return CBC_COMMAND_USED;

}

//-------------------------------------------------------------------------------------------------
/** Process a button selected message from the window system that should be for one of
	* our GUI commands */
//-------------------------------------------------------------------------------------------------
CBCommandStatus ControlBar::processCommandUI( GameWindow *control,
																							GadgetGameMessage gadgetMessage )
{
	// get the command pointer from the control user data we put in the button
	const CommandButton *commandButton = (const CommandButton *)GadgetButtonGetData(control);
	if( !commandButton )
	{
		DEBUG_CRASH( ("ControlBar::processCommandUI() -- Button activated has no data. Ignoring...") );
		return CBC_COMMAND_NOT_USED;
	}

	// sanity, we won't process messages if we have no source object,
	// unless we're CB_CONTEXT_PURCHASE_SCIENCE or GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT
	if( m_currContext != CB_CONTEXT_MULTI_SELECT &&
			commandButton->getCommandType() != GUI_COMMAND_PURCHASE_SCIENCE &&
			commandButton->getCommandType() != GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT &&
			commandButton->getCommandType() != GUI_COMMAND_SPECIAL_POWER_CONSTRUCT_FROM_SHORTCUT &&
			commandButton->getCommandType() != GUI_COMMAND_SELECT_ALL_UNITS_OF_TYPE &&
			(m_currentSelectedDrawable == nullptr || m_currentSelectedDrawable->getObject() == nullptr) )
	{

		if( m_currContext != CB_CONTEXT_NONE )
			switchToContext( CB_CONTEXT_NONE, nullptr );
		return CBC_COMMAND_NOT_USED;

	}

	// sanity
	if( control == nullptr )
		return CBC_COMMAND_NOT_USED;

	// the context sensitive gui only is only made of buttons ... sanity
	if( control->winGetInputFunc() != GadgetPushButtonInput )
		return CBC_COMMAND_NOT_USED;


	if( commandButton == nullptr )
		return CBC_COMMAND_NOT_USED;

	// if the button is flashing, tell it to stop flashing
	commandButton->setFlashCount(0);
	setFlash( FALSE );

	if( commandButton->getCommandType() != GUI_COMMAND_EXIT_CONTAINER )
	{
		GadgetButtonSetEnabledImage( control, commandButton->getButtonImage() );
	}

	//
	// get the object that is driving the context sensitive UI if we're not in a multi
	// select context
	//
	Object *obj = nullptr;
	if( m_currContext != CB_CONTEXT_MULTI_SELECT &&
			commandButton->getCommandType() != GUI_COMMAND_PURCHASE_SCIENCE &&
			commandButton->getCommandType() != GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT &&
			commandButton->getCommandType() != GUI_COMMAND_SPECIAL_POWER_CONSTRUCT_FROM_SHORTCUT &&
			commandButton->getCommandType() != GUI_COMMAND_SELECT_ALL_UNITS_OF_TYPE )
		obj = m_currentSelectedDrawable->getObject();

	//@todo Kris -- Special case code so convoy trucks can detonate nuke trucks -- if other things need this,
	//rethink it.
	if( obj && BitIsSet( commandButton->getOptions(), SINGLE_USE_COMMAND ) )
	{
		/** @todo Added obj check because Single Use and Multi Select crash when used together, but with this check
			* they just won't work.  When the "rethinking" occurs, this can get fixed.  Right now it is unused.
			* Convoy Truck needs Multi Select so Single Use is turned off, and noone else has it.
		*/

		//Make sure the command button is marked as used if it's a single use command. That way
		//we can never press the button again. This was added specifically for nuke convoy trucks.
		//When you click to detonate the nuke, it takes a few seconds to detonate in order to play
		//a sound. But we want to disable the button after the first click.
		obj->markSingleUseCommandUsed(); //Yeah, an object can only use one single use command...
	}

	TheInGameUI->placeBuildAvailable( nullptr, nullptr );

	//Play any available unit specific sound for button
	Player *player = ThePlayerList->getLocalPlayer();
	if( player )
	{
		AudioEventRTS sound = *commandButton->getUnitSpecificSound();
		sound.setPlayerIndex( player->getPlayerIndex() );
		TheAudio->addAudioEvent( &sound );
	}

	if( BitIsSet( commandButton->getOptions(), COMMAND_OPTION_NEED_TARGET ) )
	{
		if (commandButton->getOptions() & USES_MINE_CLEARING_WEAPONSET)
		{
			TheMessageStream->appendMessage( GameMessage::MSG_SET_MINE_CLEARING_DETAIL );
		}

		//June 06, 2002 -- Major change
		//I've added support for specific context sensitive commands which need targets just like
		//other options may need. When we need a target, the user must move the cursor to a position
		//where he wants the GUI command to take place. Older commands such as napalm strikes or daisy
		//cutter drops simply needed the user to click anywhere he desired.
		//
		//Now, we have new commands that will only work when the user clicks on valid targets to interact
		//with. For example, the terrorist can jack a car and convert it into a carbomb, but he has to
		//click on a valid car. In this case the doCommandOrHint code will determine if the mode is valid
		//or not and the cursor modes will be set appropriately.
		TheInGameUI->setGUICommand( commandButton );
	}
	else switch( commandButton->getCommandType() )
	{

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_DOZER_CONSTRUCT:
		{

			// sanity
			if( m_currentSelectedDrawable == nullptr )
				break;

			//Kris: September 27, 2002
			//Make sure we have enough CASH to build it WHEN we click the button to build it,
			//before actually previewing the purchase, otherwise, cancel altogether.
			const ThingTemplate *whatToBuild = commandButton->getThingTemplate();
			CanMakeType cmt = TheBuildAssistant->canMakeUnit( obj, whatToBuild );
			if (cmt == CANMAKE_NO_MONEY)
			{
				TheEva->setShouldPlay(EVA_InsufficientFunds);
				TheInGameUI->message( "GUI:NotEnoughMoneyToBuild" );
				break;
			}
			else if (cmt == CANMAKE_QUEUE_FULL)
			{
				TheInGameUI->message( "GUI:ProductionQueueFull" );
				break;
			}
			else if (cmt == CANMAKE_PARKING_PLACES_FULL)
			{
				TheInGameUI->message( "GUI:ParkingPlacesFull" );
				break;
			}
			else if( cmt == CANMAKE_MAXED_OUT_FOR_PLAYER )
			{
				TheInGameUI->message( "GUI:UnitMaxedOut" );
				break;
			}

			// tell the UI that we want to build something so we get a building at the cursor
			TheInGameUI->placeBuildAvailable( commandButton->getThingTemplate(), m_currentSelectedDrawable );

			break;

		}


		case GUI_COMMAND_SPECIAL_POWER_CONSTRUCT_FROM_SHORTCUT:
		{
			//Determine the object that would construct it.
			const SpecialPowerTemplate *spTemplate = commandButton->getSpecialPowerTemplate();
			DEBUG_ASSERTCRASH(spTemplate != nullptr, ("Special Power Button is missing Special Power template"));

			SpecialPowerType spType = spTemplate->getSpecialPowerType();
			Object* obj = ThePlayerList->getLocalPlayer()->findMostReadyShortcutSpecialPowerOfType( spType );
			if( !obj )
				break;
			Drawable *draw = obj->getDrawable();

			const ThingTemplate *whatToBuild = commandButton->getThingTemplate();

			CanMakeType cmt = TheBuildAssistant->canMakeUnit( obj, whatToBuild );
			if (cmt == CANMAKE_NO_MONEY)
			{
				TheEva->setShouldPlay(EVA_InsufficientFunds);
				TheInGameUI->message( "GUI:NotEnoughMoneyToBuild" );
				break;
			}
			else if (cmt == CANMAKE_QUEUE_FULL)
			{
				TheInGameUI->message( "GUI:ProductionQueueFull" );
				break;
			}
			else if (cmt == CANMAKE_PARKING_PLACES_FULL)
			{
				TheInGameUI->message( "GUI:ParkingPlacesFull" );
				break;
			}
			else if( cmt == CANMAKE_MAXED_OUT_FOR_PLAYER )
			{
				TheInGameUI->message( "GUI:UnitMaxedOut" );
				break;
			}

			// tell the UI that we want to build something so we get a building at the cursor
			TheInGameUI->placeBuildAvailable( commandButton->getThingTemplate(), draw );

			ProductionUpdateInterface* pu = obj->getProductionUpdateInterface();
			if( pu )
			{
				pu->setSpecialPowerConstructionCommandButton( commandButton );
			}

			break;
		}
		case GUI_COMMAND_SPECIAL_POWER_CONSTRUCT:
		{
			// sanity
			if( m_currentSelectedDrawable == nullptr )
				break;

			const ThingTemplate *whatToBuild = commandButton->getThingTemplate();

			CanMakeType cmt = TheBuildAssistant->canMakeUnit( obj, whatToBuild );
			if (cmt == CANMAKE_NO_MONEY)
			{
				TheEva->setShouldPlay(EVA_InsufficientFunds);
				TheInGameUI->message( "GUI:NotEnoughMoneyToBuild" );
				break;
			}
			else if (cmt == CANMAKE_QUEUE_FULL)
			{
				TheInGameUI->message( "GUI:ProductionQueueFull" );
				break;
			}
			else if (cmt == CANMAKE_PARKING_PLACES_FULL)
			{
				TheInGameUI->message( "GUI:ParkingPlacesFull" );
				break;
			}
			else if( cmt == CANMAKE_MAXED_OUT_FOR_PLAYER )
			{
				TheInGameUI->message( "GUI:UnitMaxedOut" );
				break;
			}

			// tell the UI that we want to build something so we get a building at the cursor
			TheInGameUI->placeBuildAvailable( commandButton->getThingTemplate(), m_currentSelectedDrawable );

			ProductionUpdateInterface* pu = obj->getProductionUpdateInterface();
			if( pu )
			{
				pu->setSpecialPowerConstructionCommandButton( commandButton );
			}

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_DOZER_CONSTRUCT_CANCEL:
		{

			// get the object we have selected
			Object *building = obj;
			if( building == nullptr )
				break;

			// sanity check, the building must be under our control to cancel construction
			if( !building->isLocallyControlled() )
				break;

			// do the message
			TheMessageStream->appendMessage( GameMessage::MSG_DOZER_CANCEL_CONSTRUCT );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_UNIT_BUILD:
		{
			const ThingTemplate *whatToBuild = commandButton->getThingTemplate();

			// get the "factory" object that is going to make the thing
			Object *factory = obj;
			if( factory == nullptr )
				break;

			// sanity, we must have something to build
			DEBUG_ASSERTCRASH( whatToBuild, ("Undefined BUILD command for object '%s'",
												 commandButton->getThingTemplate()->getName().str()) );

			CanMakeType cmt = TheBuildAssistant->canMakeUnit(factory, whatToBuild);

			if (cmt == CANMAKE_NO_MONEY)
			{
				TheEva->setShouldPlay(EVA_InsufficientFunds);
				TheInGameUI->message( "GUI:NotEnoughMoneyToBuild" );
				break;
			}
			else if (cmt == CANMAKE_QUEUE_FULL)
			{
				TheInGameUI->message( "GUI:ProductionQueueFull" );
				break;
			}
			else if (cmt == CANMAKE_PARKING_PLACES_FULL)
			{
				TheInGameUI->message( "GUI:ParkingPlacesFull" );
				break;
			}
			else if( cmt == CANMAKE_MAXED_OUT_FOR_PLAYER )
			{
				TheInGameUI->message( "GUI:UnitMaxedOut" );
				break;
			}
			else if (cmt != CANMAKE_OK)
			{
				DEBUG_CRASH( ("Cannot create '%s' because the factory object '%s' returns false for canMakeUnit",
																whatToBuild->getName().str(),
																factory->getTemplate()->getName().str()) );
				break;
			}

			// get the production interface from the factory object
			ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();

			// sanity, we can't build things if we can't produce units
			if( pu == nullptr )
			{

				DEBUG_CRASH( ("Cannot create '%s' because the factory object '%s' is not capable of producing units",
																whatToBuild->getName().str(),
																factory->getTemplate()->getName().str()) );
				break;

			}

			// get a new production id to assign to this
			ProductionID productionID = pu->requestUniqueUnitID();

			// create a message to build this thing

			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_QUEUE_UNIT_CREATE );
			msg->appendIntegerArgument( whatToBuild->getTemplateID() );
			msg->appendIntegerArgument( productionID );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_CANCEL_UNIT_BUILD:
		{
			Int i;

			// find out which index (i) in the queue represents the button clicked
			for( i = 0; i < MAX_BUILD_QUEUE_BUTTONS; i++ )
				if( m_queueData[ i ].control == control )
					break;

			// sanity, control not found
			if( i == MAX_BUILD_QUEUE_BUTTONS )
			{

				DEBUG_CRASH( ("Control not found in build queue data") );
				break;

			}

			// sanity
			if( m_queueData[ i ].type != PRODUCTION_UNIT )
				break;

			// the the production ID to cancel
			ProductionID productionIDToCancel = m_queueData[ i ].productionID;

			// get the object that is the producer
			Object *producer = obj;
			if( producer == nullptr )
				break;

			// sanity, we must control the producer ... if this isn't true they might be hacking the game
			if( !producer->isLocallyControlled() )
				break;

			// send a message to cancel that particular production entry
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CANCEL_UNIT_CREATE );
			msg->appendIntegerArgument( productionIDToCancel );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_PLAYER_UPGRADE:
		{
			const UpgradeTemplate *upgradeT = commandButton->getUpgradeTemplate();
			DEBUG_ASSERTCRASH( upgradeT, ("Undefined upgrade '%s' in player upgrade command", "UNKNOWN") );

			// sanity
			if( obj == nullptr || upgradeT == nullptr )
				break;

			// make sure the player can really make this
			if( TheUpgradeCenter->canAffordUpgrade( ThePlayerList->getLocalPlayer(), upgradeT, TRUE ) == FALSE )
			{
				break;
			}

			ProductionUpdateInterface* pu = obj ? obj->getProductionUpdateInterface() : nullptr;
			if (pu != nullptr)
			{
				CanMakeType cmt = pu->canQueueUpgrade(upgradeT);
				if (cmt == CANMAKE_QUEUE_FULL)
				{
					TheInGameUI->message( "GUI:ProductionQueueFull" );
					break;
				}
			}

			// send the message
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_QUEUE_UPGRADE );
			msg->appendObjectIDArgument( obj->getID() );
			msg->appendIntegerArgument( upgradeT->getUpgradeNameKey() );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_OBJECT_UPGRADE:
		{
			const UpgradeTemplate *upgradeT = commandButton->getUpgradeTemplate();
			DEBUG_ASSERTCRASH( upgradeT, ("Undefined upgrade '%s' in object upgrade command", "UNKNOWN") );
			// sanity
			if( upgradeT == nullptr )
				break;

			//Make sure the player can really make this
			if( TheUpgradeCenter->canAffordUpgrade( ThePlayerList->getLocalPlayer(), upgradeT, TRUE ) == FALSE )
			{
				//Kris: Disabled because we can get a valid reason for not being able to afford the upgrade!
				//TheInGameUI->message( "upgrade unsupported in commandprocessing." );
				break;
			}

			ProductionUpdateInterface* pu = obj ? obj->getProductionUpdateInterface() : nullptr;
			if (pu != nullptr)
			{
				CanMakeType cmt = pu->canQueueUpgrade(upgradeT);
				if (cmt == CANMAKE_QUEUE_FULL)
				{
					TheInGameUI->message( "GUI:ProductionQueueFull" );
					break;
				}
			}

			ObjectID objID = INVALID_ID;
			if (obj)
				objID = obj->getID();

			// make sure that the this object can actually build the upgrade
			if( obj && (obj->hasUpgrade( upgradeT ) == TRUE || obj->affectedByUpgrade( upgradeT ) == FALSE) )
				break;

			// send the message
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_QUEUE_UPGRADE );
			msg->appendObjectIDArgument( objID );
			msg->appendIntegerArgument( upgradeT->getUpgradeNameKey() );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_CANCEL_UPGRADE:
		{
			Int i;

			// find out which index (i) in the queue represents the button clicked
			for( i = 0; i < MAX_BUILD_QUEUE_BUTTONS; i++ )
				if( m_queueData[ i ].control == control )
					break;

			// sanity, control not found
			if( i == MAX_BUILD_QUEUE_BUTTONS )
			{

				DEBUG_CRASH( ("Control not found in build queue data") );
				break;

			}

			// sanity
			if( m_queueData[ i ].type != PRODUCTION_UPGRADE )
				break;

			// get the upgrade to cancel
			const UpgradeTemplate *upgradeT = m_queueData[ i ].upgradeToResearch;

			// get producer object (the thing driving our UI)
			Object *producer = obj;

			// sanity
			if( upgradeT == nullptr || producer == nullptr )
				break;

			// send the message
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CANCEL_UPGRADE );
			msg->appendIntegerArgument( upgradeT->getUpgradeNameKey() );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_ATTACK_MOVE:
			TheMessageStream->appendMessage(GameMessage::MSG_META_TOGGLE_ATTACKMOVE);
			break;

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_STOP:
		{
			// This message always works on the currently selected team
			TheMessageStream->appendMessage(GameMessage::MSG_DO_STOP);
			break;
		}

		case GUI_COMMAND_SELECT_ALL_UNITS_OF_TYPE:
		{
			Player* localPlayer = ThePlayerList->getLocalPlayer();
			if( !localPlayer )
			{
				break;
			}

			const ThingTemplate *thing = commandButton->getThingTemplate();
			if( !thing )
			{
				break;
			}

			//deselect other units
			TheInGameUI->deselectAllDrawables();

			// create a new group.
			GameMessage *teamMsg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP );

			//New group or add to group? Passed in value is true if we are creating a new group.
			teamMsg->appendBooleanArgument( TRUE );

			//Iterate through the player's entire team and select each member that matches the template.
			SelectObjectsInfo info;
			info.thingTemplate = thing;
			info.msg = teamMsg;
			localPlayer->iterateObjects( selectObjectOfType, (void*)&info );

			break;
		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_WAYPOINTS:
			break;

		//-------------------------------------------------------------------------------------------------
		case GUI_COMMAND_EXIT_CONTAINER:
		{
			Int i;
			// TheSuperHackers @fix Caball009/Mauller 23/05/2025 Fix uninitialized variable and control lookup behaviour to prevent a buffer-overflow when the control container is empty
			ObjectID objID = INVALID_ID;

			//
			// find the object ID that wants to exit by scanning through the transport data and looking
			// for the matching control button
			//
			for (i = 0; i < MAX_COMMANDS_PER_SET; i++)
			{
				if (m_containData[i].control == control)
				{
					objID = m_containData[ i ].objectID;
					break;
				}
			}

			if (objID == INVALID_ID)
				break;

			// get the actual object
			Object *objWantingExit = TheGameLogic->findObjectByID( objID );

			// if the control container returns an object ID but the object is not found, remove the control entry and exit
			if( objWantingExit == nullptr )
			{

				//
				// remove from inventory data to avoid future matches ... the inventory update
				// cycle of the UI will repopulate any buttons as the contents of objects
				// change so this is only an edge case that will be visually corrected next frame
				//
				m_containData[ i ].control = nullptr;
				m_containData[ i ].objectID = INVALID_ID;
				break;  // exit case

			}

      //what if container is subdued... assert a logic failure, perhaps?

			// send message to exit
			GameMessage *exitMsg = TheMessageStream->appendMessage( GameMessage::MSG_EXIT );
			exitMsg->appendObjectIDArgument( objWantingExit->getID() ); // 0 is the thing inside coming out

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_EVACUATE:
		{
			// Cancel GUI command mode.
			TheInGameUI->setGUICommand( nullptr );

			if (BitIsSet(commandButton->getOptions(), NEED_TARGET_POS) == FALSE) {
				pickAndPlayUnitVoiceResponse( TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_EVACUATE );
				TheMessageStream->appendMessage( GameMessage::MSG_EVACUATE );
			}

			break;
		}

		// --------------------------------------------------------------------------------------------
		case GUI_COMMAND_EXECUTE_RAILED_TRANSPORT:
		{
			TheMessageStream->appendMessage( GameMessage::MSG_EXECUTE_RAILED_TRANSPORT );
			break;
		}

		// --------------------------------------------------------------------------------------------
		case GUI_COMMAND_HACK_INTERNET:
		{
			pickAndPlayUnitVoiceResponse( TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_INTERNET_HACK );
			TheMessageStream->appendMessage( GameMessage::MSG_INTERNET_HACK );
			break;
		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_SET_RALLY_POINT:
		{
			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_SELL:
		{

			// command needs no additional data, send the message
			TheMessageStream->appendMessage( GameMessage::MSG_SELL );
			break;

		}

		// --------------------------------------------------------------------------------------------
		case GUI_COMMAND_TOGGLE_OVERCHARGE:
		{

			TheMessageStream->appendMessage( GameMessage::MSG_TOGGLE_OVERCHARGE );
			break;

		}

#ifdef ALLOW_SURRENDER
		// ------------------------------------------------------------------------------------------------
		case GUI_COMMAND_POW_RETURN_TO_PRISON:
		{

			TheMessageStream->appendMessage( GameMessage::MSG_RETURN_TO_PRISON );
			break;

		}
#endif

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_BEACON_DELETE:
		{

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_GUARD:
		case GUI_COMMAND_GUARD_WITHOUT_PURSUIT:
		case GUI_COMMAND_GUARD_FLYING_UNITS_ONLY:
		case GUI_COMMAND_COMBATDROP:
		{
			DEBUG_CRASH(("hmm, should never occur"));
		}
		break;

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_SWITCH_WEAPON:
		{
				// command needs no additional data, send the message
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_SWITCH_WEAPONS );

				//Play mode change acknowledgement
				PickAndPlayInfo info;
				WeaponSlotType slot = commandButton->getWeaponSlot();
				info.m_weaponSlot = &slot;
				pickAndPlayUnitVoiceResponse( TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_SWITCH_WEAPONS, &info );

				msg->appendIntegerArgument( commandButton->getWeaponSlot() );
				break;
		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_FIRE_WEAPON:
		{
			// command needs no additional data, send the message
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_WEAPON );
			msg->appendIntegerArgument( commandButton->getWeaponSlot() );
			msg->appendIntegerArgument( commandButton->getMaxShotsToFire() );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT:
		{
			const SpecialPowerTemplate *spTemplate = commandButton->getSpecialPowerTemplate();
			SpecialPowerType spType = spTemplate->getSpecialPowerType();

			Object* obj = ThePlayerList->getLocalPlayer()->findMostReadyShortcutSpecialPowerOfType( spType );
			if( !obj )
				break;

			// command needs no additional data, send the message
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER );
			msg->appendIntegerArgument( spTemplate->getID() );
			msg->appendIntegerArgument( commandButton->getOptions() );
			msg->appendObjectIDArgument( obj->getID() );
			break;

		}

		case GUI_COMMAND_SPECIAL_POWER:
		{
			// command needs no additional data, send the message
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER );
			msg->appendIntegerArgument( commandButton->getSpecialPowerTemplate()->getID() );
			msg->appendIntegerArgument( commandButton->getOptions() );
			msg->appendObjectIDArgument( INVALID_ID );	// no specific source
			break;

		}

		//---------------------------------------------------------------------------------------------
		case GUI_COMMAND_PURCHASE_SCIENCE:
		{

			// loop through all the sciences on the button and select the one we don't have

			ScienceType	st = SCIENCE_INVALID;
			Player *player = ThePlayerList->getLocalPlayer();
			for(size_t i = 0; i < commandButton->getScienceVec().size(); ++i)
			{
				st = commandButton->getScienceVec()[ i ];
				if(!player->hasScience(st) && TheScienceStore->playerHasPrereqsForScience(player, st) && TheScienceStore->getSciencePurchaseCost(st) <= player->getSciencePurchasePoints())
				{
					break;
				}
			}

			if( st == SCIENCE_INVALID)
			{
				switchToContext( CB_CONTEXT_NONE, nullptr );
				break;
			}


			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_PURCHASE_SCIENCE );
			msg->appendIntegerArgument( st );

			markUIDirty();

			break;

		}

		//---------------------------------------------------------------------------------------------
		default:

			DEBUG_CRASH( ("Unknown command '%d'", commandButton->getCommandType()) );
			return CBC_COMMAND_NOT_USED;

	}

	return CBC_COMMAND_USED;

}

