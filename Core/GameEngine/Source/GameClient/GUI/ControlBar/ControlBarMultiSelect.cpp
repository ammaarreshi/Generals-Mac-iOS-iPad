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

// FILE: ControlBarMultiSelect.cpp ////////////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Context-sensitive GUI for when you select multiple objects.  What we do is show
//				 the commands that you can use between them all
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/Object.h"




//-------------------------------------------------------------------------------------------------
/** Reset the common command data */
//-------------------------------------------------------------------------------------------------
void ControlBar::resetCommonCommandData()
{
	Int i;

	for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
	{
		m_commonCommands[ i ] = nullptr;
		//Clear out any remnant overlays.
		GadgetButtonDrawOverlayImage( m_commandWindows[ i ], nullptr );
	}

}

//-------------------------------------------------------------------------------------------------
/** add the common commands of this drawable to the common command set */
//-------------------------------------------------------------------------------------------------
void ControlBar::addCommonCommands( Drawable *draw, Bool firstDrawable )
{
	Int i;
	const CommandButton *command;

	// sanity
	if( draw == nullptr )
		return;

	Object* obj = draw->getObject();
	if (!obj)
		return;

	if (obj->isKindOf(KINDOF_IGNORED_IN_GUI)) // ignore these guys
		return;

	// get the command set of this drawable
	const CommandSet *commandSet = findCommandSet( obj->getCommandSetString() );
	if( commandSet == nullptr )
	{

		//
		// if there is no command set for this drawable, none of the selected drawables
		// can possibly have matching commands so we'll get rid of them all
		//
		for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
		{

			m_commonCommands[ i ] = nullptr;
			if (m_commandWindows[ i ])
			{
				m_commandWindows[ i ]->winHide( TRUE );
			}
			// After Every change to the m_commandWIndows, we need to show fill in the missing blanks with the images
	// removed from multiplayer branch
			//showCommandMarkers();

		}

		return;

	}


	//
	// easy case, if we're adding the first drawable we simply just add any of the commands
	// in its set that can be multi-select commands to the common command set
	//
	if( firstDrawable == TRUE )
	{

		// just add each command that is classified as a common command
		for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
		{
			// our implementation doesn't necessarily make use of the max possible command buttons
			if (! m_commandWindows[ i ]) continue;

			// get command
			command = commandSet->getCommandButton(i);

			// add if present and can be used in a multi select
			if( command && BitIsSet( command->getOptions(), OK_FOR_MULTI_SELECT ) == TRUE )
			{

				// put it in the common command set
				m_commonCommands[ i ] = command;

				// show and enable this control
				m_commandWindows[ i ]->winHide( FALSE );
				m_commandWindows[ i ]->winEnable( TRUE );

				// set the command into the control
				setControlCommand( m_commandWindows[ i ], command );

			}

		}

	}
	else
	{

		// go through each command one by one
		for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
		{

			// our implementation doesn't necessarily make use of the max possible command buttons
			if (! m_commandWindows[ i ]) continue;

			// get the command
			command = commandSet->getCommandButton(i);

			Bool attackMove = (command && command->getCommandType() == GUI_COMMAND_ATTACK_MOVE) ||
												(m_commonCommands[ i ] && m_commonCommands[ i ]->getCommandType() == GUI_COMMAND_ATTACK_MOVE);

			// Kris: When any units have attack move, they all get it. This is to allow
			// combat units to be selected with the odd dozer or pilot and still retain that ability.
			if( attackMove && !m_commonCommands[ i ] )
			{
				// put it in the common command set
				m_commonCommands[ i ] = command;

				// show and enable this control
				m_commandWindows[ i ]->winHide( FALSE );
				m_commandWindows[ i ]->winEnable( TRUE );

				// set the command into the control
				setControlCommand( m_commandWindows[ i ], command );
			}
			else if( command != m_commonCommands[ i ] && !attackMove )
			{
				//
				// if this command does not match the command that is in the common command set then
				// *neither* this command OR the command in the common command set are really common
				// commands, so we will remove the one that has been stored in the common set
				//

				// remove the common command
				m_commonCommands[ i ] = nullptr;

				//
				// hide the window control cause it should have been made visible from a command
				// that was placed in this common 'slot' earlier
				//
				m_commandWindows[ i ]->winHide( TRUE );
			}

		}

	}

	// After Every change to the m_commandWIndows, we need to show fill in the missing blanks with the images
	// removed from multiplayer branch
	//showCommandMarkers();

}

//-------------------------------------------------------------------------------------------------
/** Populate the visible command bar with commands that are common to all the objects
	* that are selected in the UI */
//-------------------------------------------------------------------------------------------------
void ControlBar::populateMultiSelect()
{
	Drawable *draw;
	Bool firstDrawable = TRUE;
	Bool portraitSet = FALSE;
	const Image *portrait = nullptr;
	Object *portraitObj = nullptr;

	// first reset the common command data
	resetCommonCommandData();

	// by default, hide all the controls in the command section
	for( Int i = 0; i < MAX_COMMANDS_PER_SET; i++ )
	{
		if (m_commandWindows[ i ])
		{
			m_commandWindows[ i ]->winHide( TRUE );
		}
	}

	// sanity
	DEBUG_ASSERTCRASH( TheInGameUI->getSelectCount() > 1,
										 ("populateMultiSelect: Can't populate multiselect context cause there are only '%d' things selected",
										  TheInGameUI->getSelectCount()) );

	// get the list of drawable IDs from the in game UI
	const DrawableList *selectedDrawables = TheInGameUI->getAllSelectedDrawables();

	// sanity
	DEBUG_ASSERTCRASH( selectedDrawables->empty() == FALSE, ("populateMultiSelect: Drawable list is empty") );

	// loop through all the selected drawables
	for( DrawableListCIt it = selectedDrawables->begin();
			 it != selectedDrawables->end(); ++it )
	{

		// get the drawable
		draw = *it;


		if (draw->getObject()->isKindOf(KINDOF_IGNORED_IN_GUI)) // ignore these guys
			continue;


		//
		// add command for this drawable, note that we also sanity check to make sure the
		// drawable has an object as all interesting drawables that we can select should
		// actually have an object underneath it so that we can do interesting things with
		// it ... otherwise we should have never selected it.
		// NOTE that we're not considering objects that are currently in the process of
		// being sold as those objects can't be issued anymore commands
		//
		if( draw && draw->getObject() &&
				!draw->getObject()->getStatusBits().test( OBJECT_STATUS_SOLD ) )
		{

			// add the common commands of this drawable to the common command set
			addCommonCommands( draw, firstDrawable );

			// not adding the first drawable anymore
			firstDrawable = FALSE;

			//
			// keep track of the portrait images, if all units selected have the same portrait
			// we will display it in the right HUD, otherwise we won't
			//
			if( portraitSet == FALSE )
			{

				portrait = draw->getTemplate()->getSelectedPortraitImage();
				portraitObj = draw->getObject();
				portraitSet = TRUE;

			}
			else if( draw->getTemplate()->getSelectedPortraitImage() != portrait )
				portrait = nullptr;

		}

	}

	// set the portrait image
	setPortraitByObject( portraitObj );

}

//-------------------------------------------------------------------------------------------------
/** Update logic for the multi select context sensitive GUI */
//-------------------------------------------------------------------------------------------------
void ControlBar::updateContextMultiSelect()
{
	Drawable *draw;
	Object *obj;
	const CommandButton *command;
	GameWindow *win;
	Int objectsThatCanDoCommand[ MAX_COMMANDS_PER_SET ];
	Int i;

	// zero the array that counts how many objects can do each command
	memset( objectsThatCanDoCommand, 0, sizeof( objectsThatCanDoCommand ) );

	// sanity
	DEBUG_ASSERTCRASH( TheInGameUI->getSelectCount() > 1,
										 ("updateContextMultiSelect: TheInGameUI only has '%d' things selected",
										  TheInGameUI->getSelectCount()) );

	// get the list of drawable IDs from the in game UI
	const DrawableList *selectedDrawables = TheInGameUI->getAllSelectedDrawables();

	// sanity
	DEBUG_ASSERTCRASH( selectedDrawables->empty() == FALSE, ("populateMultiSelect: Drawable list is empty") );

	// loop through all the selected drawable IDs
	for( DrawableListCIt it = selectedDrawables->begin();
			 it != selectedDrawables->end(); ++it )
	{

		// get the drawable from the ID
		draw = *it;

		if (draw->getObject()->isKindOf(KINDOF_IGNORED_IN_GUI)) // ignore these guys
			continue;


		// get the object
		obj = draw->getObject();

		// sanity
		if( obj == nullptr )
			continue;

		// for each of the visible command windows make sure the object can execute the command
		for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
		{

			// get the control window
			win = m_commandWindows[ i ];

			// our implementation doesn't necessarily make use of the max possible command buttons
			if (!win) continue;

			// don't consider hidden windows
			if( win->winIsHidden() == TRUE )
				continue;

			// get the command
			command = (const CommandButton *)GadgetButtonGetData(win);
			if( command == nullptr )
				continue;

			// can we do the command
			CommandAvailability availability = getCommandAvailability( command, obj, win );

			win->winClearStatus( WIN_STATUS_NOT_READY );
			win->winClearStatus( WIN_STATUS_ALWAYS_COLOR );

			// enable/disable the window control
			switch( availability )
			{
				case COMMAND_HIDDEN:
					win->winHide( TRUE );
					break;
				case COMMAND_RESTRICTED:
					win->winEnable( FALSE );
					break;
				case COMMAND_NOT_READY:
					win->winEnable( FALSE );
					win->winSetStatus( WIN_STATUS_NOT_READY );
					break;
				case COMMAND_CANT_AFFORD:
					win->winEnable( FALSE );
					win->winSetStatus( WIN_STATUS_ALWAYS_COLOR );
					break;
				default:
					win->winEnable( TRUE );
					break;
			}

			//If button is a CHECK_LIKE, then update it's status now.
			if( BitIsSet( command->getOptions(), CHECK_LIKE ) )
			{
				GadgetCheckLikeButtonSetVisualCheck( win, availability == COMMAND_ACTIVE );
			}

			if( availability == COMMAND_AVAILABLE || availability == COMMAND_ACTIVE )
					objectsThatCanDoCommand[ i ]++;

		}

	}

	//
	// for each command, if any objects can do the command we enable the window, otherwise
	// we disable it
	//
	for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
	{
		// our implementation doesn't necessarily make use of the max possible command buttons
		if (! m_commandWindows[ i ]) continue;

		// don't consider hidden commands
		if( m_commandWindows[ i ]->winIsHidden() == TRUE )
			continue;

		// don't consider slots that don't have commands
		if( m_commonCommands[ i ] == nullptr )
			continue;

		// check the count of objects that can do the command and enable/disable the control,
		if( objectsThatCanDoCommand[ i ] > 0 )
			m_commandWindows[ i ]->winEnable( TRUE );
		else
			m_commandWindows[ i ]->winEnable( FALSE );

	}

	// After Every change to the m_commandWIndows, we need to show fill in the missing blanks with the images
	// removed from multiplayer branch
	//showCommandMarkers();


}
