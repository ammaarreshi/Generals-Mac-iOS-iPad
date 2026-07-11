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

// FILE: ControlBarUnderConstruction.cpp //////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Methods specific to the control bar under construction context
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <stdio.h>

#include "Common/NameKeyGenerator.h"
#include "Common/ThingTemplate.h"

#include "GameLogic/Object.h"
#include "GameLogic/Module/UpdateModule.h"

#include "GameClient/Drawable.h"
#include "GameClient/GameText.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetStaticText.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::updateConstructionTextDisplay( Object *obj )
{
	UnicodeString text;
	static UnsignedInt descID = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:UnderConstructionDesc" );
	GameWindow *descWindow = TheWindowManager->winGetWindowFromId( nullptr, descID );

	// sanity
	DEBUG_ASSERTCRASH( descWindow, ("Under construction window not found") );

	// format the message
	text.format( TheGameText->fetch( "CONTROLBAR:UnderConstructionDesc" ),
							 obj->getConstructionPercent() );
	GadgetStaticTextSetText( descWindow, text );

	// record this as the last percentage displayed
	m_displayedConstructPercent = obj->getConstructionPercent();

}

//-------------------------------------------------------------------------------------------------
/** Populate the interface for an under construction context. */
//-------------------------------------------------------------------------------------------------
void ControlBar::populateUnderConstruction( Object *objectUnderConstruction )
{

	// sanity
	if( objectUnderConstruction == nullptr )
		return;

	// get our parent window
	GameWindow *parent = m_contextParent[ CP_UNDER_CONSTRUCTION ];

	// set the cancel construction button
/// @todo srj -- remove hard-coding here, please
	const CommandButton *commandButton = findCommandButton( "Command_CancelConstruction" );
	NameKeyType id;
	id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:ButtonCancelConstruction" );
	GameWindow *win = TheWindowManager->winGetWindowFromId( parent, id );

	setControlCommand( win, commandButton );
	win->winSetStatus( WIN_STATUS_USE_OVERLAY_STATES );

	// set the text description of what is building
	updateConstructionTextDisplay( objectUnderConstruction );

	// set the portrait for the thing being constructed
	setPortraitByObject( objectUnderConstruction );

	// and show the rally point, if it should have one,
	ExitInterface *exit = objectUnderConstruction->getObjectExitInterface();
	if( exit )
		showRallyPoint( exit->getRallyPoint() );


}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::updateContextUnderConstruction()
{
	Object *obj = m_currentSelectedDrawable->getObject();

	// if the object is no longer under construction switch to a new appropriate context
	if( !obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{

		evaluateContextUI();
		return;

	}

	// if the construction percent has changed since what was last shown to the user update the text
	if( m_displayedConstructPercent != obj->getConstructionPercent() )
		updateConstructionTextDisplay( obj );

}
