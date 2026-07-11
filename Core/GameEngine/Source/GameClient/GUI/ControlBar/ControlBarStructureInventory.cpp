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

// FILE: ControlBarStructureInventory.cpp /////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Methods specific to the control bar garrison display
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/NameKeyGenerator.h"
#include "Common/ThingTemplate.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/OpenContain.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Drawable.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/HotKey.h"


#define STOP_ID			10
#define EVACUATE_ID	11

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
struct PopulateButtonInfo
{
	Object *source;
	Int buttonIndex;
	ControlBar* self;
	GameWindow** inventoryButtons;
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::populateButtonProc( Object *obj, void *userData )
{
	PopulateButtonInfo* info = (PopulateButtonInfo*)userData;

	// sanity
	DEBUG_ASSERTCRASH( info->buttonIndex < MAX_STRUCTURE_INVENTORY_BUTTONS,
										 ("Too many objects inside '%s' for the inventory buttons to hold",
											info->source->getTemplate()->getName().str()) );

	// put object in inventory data
	info->self->m_containData[ info->buttonIndex ].control = info->inventoryButtons[ info->buttonIndex ];
	info->self->m_containData[ info->buttonIndex ].objectID = obj->getID();

	// set the UI button that will allow us to press it and cause the object to exit the container
	const Image *image;
	image = obj->getTemplate()->getButtonImage();
	GadgetButtonSetEnabledImage( info->inventoryButtons[ info->buttonIndex ], image );

	//Show the auto-contained object's veterancy symbol!
	image = calculateVeterancyOverlayForObject( obj );
	GadgetButtonDrawOverlayImage( info->inventoryButtons[ info->buttonIndex ], image );

	// Enable the button
	info->inventoryButtons[ info->buttonIndex ]->winEnable( TRUE );

	// move to the next button index
	info->buttonIndex++;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::populateStructureInventory( Object *building )
{
	Int i;

	// reset the inventory data
	resetContainData();

	// reset hotkeys -- seeing it only is reset in switching contexts. This is a special case
	// because we are building the hotkeys on the fly sort of... and it changes as guys enter
	// and leave. Taking this call out will cause multiple hotkeys to be added.
	if(TheHotKeyManager)
		TheHotKeyManager->reset();

	// Start by hiding all the buttons.  Otherwise buttons we don't use will have the buttons
	// the last thing selected left behind.
	for( Int commandIndex = 0; commandIndex < MAX_COMMANDS_PER_SET; commandIndex++ )
	{
		if( m_commandWindows[commandIndex] )
		{
			m_commandWindows[commandIndex]->winHide(TRUE);
		}
	}

	// get the contain module of the object
	ContainModuleInterface *contain = building->getContain();
	DEBUG_ASSERTCRASH( contain, ("Object in structure inventory does not contain a Contain Module") );
	if (!contain)
		return;

	/// @todo srj -- remove hard-coding here, please
	const CommandButton *evacuateCommand = findCommandButton( "Command_Evacuate" );
	setControlCommand( m_commandWindows[ EVACUATE_ID ], evacuateCommand );
	m_commandWindows[ EVACUATE_ID ]->winEnable( FALSE );

	/// @todo srj -- remove hard-coding here, please
	const CommandButton *stopCommand = findCommandButton( "Command_Stop" );
	setControlCommand( m_commandWindows[ STOP_ID ], stopCommand );
	m_commandWindows[ STOP_ID ]->winEnable( FALSE );

	// get the inventory exit command to assign into the button
	/// @todo srj -- remove hard-coding here, please
	const CommandButton *exitCommand = findCommandButton( "Command_StructureExit" );

	// get window handles for each of the inventory buttons
	AsciiString windowName;
	for( i = 0; i < MAX_STRUCTURE_INVENTORY_BUTTONS; i++ )
	{
		// show the window
		m_commandWindows[ i ]->winHide( FALSE );

		//
		// disable the button for now, it will be enabled if there is something there
		// for its contents
		//
		m_commandWindows[ i ]->winEnable( FALSE );
		m_commandWindows[ i ]->winSetStatus( WIN_STATUS_ALWAYS_COLOR );
		m_commandWindows[ i ]->winClearStatus( WIN_STATUS_NOT_READY );

		// set an inventory command into the game window UI element
		setControlCommand( m_commandWindows[ i ], exitCommand );

		// Clear any veterancy icon incase the unit leaves!
		GadgetButtonDrawOverlayImage( m_commandWindows[ i ], nullptr );
		//
		// if the structure can hold a lesser amount inside it than what the GUI displays
		// we will completely hide the buttons that can't contain anything
		//
		if( i + 1 > contain->getContainMax() )
			m_commandWindows[ i ]->winHide( TRUE );


	}

	// show the window
	m_commandWindows[ EVACUATE_ID ]->winHide( FALSE );
	m_commandWindows[ STOP_ID ]->winHide( FALSE );

	// if there is at least one item in there enable the evacuate and stop buttons
	if( contain->getContainCount() != 0 )
	{
		m_commandWindows[ EVACUATE_ID ]->winEnable( TRUE );
		m_commandWindows[ STOP_ID ]->winEnable( TRUE );
	}

	//
	// iterate each of the objects inside the container and put them in a button, note
	// we're iterating in reverse order here
	//
	PopulateButtonInfo info;
	info.source = building;
	info.buttonIndex = 0;
	info.self = this;
	info.inventoryButtons = m_commandWindows;
	contain->iterateContained(populateButtonProc, &info, FALSE );

	//
	// save how many items were contained by the object at this time so that we can update
	// it if they change in the future while selected
	//
	m_lastRecordedInventoryCount = contain->getContainCount();

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::updateContextStructureInventory()
{
	Object *source = m_currentSelectedDrawable->getObject();

	//
	// we're visible, so there is something selected.  It is possible that we had a building
	// selected that can be garrisoned, and while it was selected the enemy occupied it.
	// in that case we want to unselect the building so that we can't see the contents
	//
	Player *localPlayer = ThePlayerList->getLocalPlayer();
	if( source->isLocallyControlled() == FALSE &&
			localPlayer->getRelationship( source->getTeam() ) != NEUTRAL )
	{
		Drawable *draw = source->getDrawable();

		if( draw )
			TheInGameUI->deselectDrawable( draw );
		return;

	}

	//
	// if the object being displayed in the interface has a different count than we last knew
	// about we need to repopulate the buttons of the interface
	//
	ContainModuleInterface *contain = source->getContain();
	DEBUG_ASSERTCRASH( contain, ("No contain module defined for object in the inventory bar") );
	if (!contain)
		return;

	if( m_lastRecordedInventoryCount != contain->getContainCount() )
		populateStructureInventory( source );

}

