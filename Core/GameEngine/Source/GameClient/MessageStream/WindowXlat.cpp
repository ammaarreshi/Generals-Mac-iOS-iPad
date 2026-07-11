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

// FILE: WindowXlat.cpp ///////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    RTS3
//
// File name:  WindowXlat.cpp
//
// Created:    Colin Day, September 2001
//
// Desc:       Window system translator that monitors raw input messages
//						 on the stream from the input devices and acts on anything
//						 relevant to the windowing system.
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Common/MessageStream.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/WindowXlat.h"
#include "GameClient/Shell.h"
#include "GameClient/Display.h"


// DEFINES ////////////////////////////////////////////////////////////////////

// PRIVATE TYPES //////////////////////////////////////////////////////////////

// PRIVATE DATA ///////////////////////////////////////////////////////////////

// PUBLIC DATA ////////////////////////////////////////////////////////////////

// PRIVATE PROTOTYPES /////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if defined(RTS_DEBUG)	//debug hack to view object under mouse stats
extern ICoord2D TheMousePos;
#endif

// rawMouseToWindowMessage ====================================================
/** Translate a raw mouse input event to a game window specific message
	* for the window system */
//=============================================================================
static GameWindowMessage rawMouseToWindowMessage( const GameMessage *msg )
{
	GameWindowMessage gwm = GWM_NONE;

	switch( msg->getType() )
	{
		// ------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_POSITION:
			gwm = GWM_MOUSE_POS;
			break;

		// ------------------------------------------------------------------------
		// Strange, but true. The window stuff really doesn't care about double clicks, so just
		// treat it as a down click.. Kinda like a second click.
		case GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK:
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
			gwm = GWM_LEFT_DOWN;
			break;

		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP:
			gwm = GWM_LEFT_UP;
			break;

		case GameMessage::MSG_RAW_MOUSE_LEFT_DRAG:
			gwm = GWM_LEFT_DRAG;
			break;

		// ------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_DOUBLE_CLICK:
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_BUTTON_DOWN:
			gwm = GWM_MIDDLE_DOWN;
			break;

		case GameMessage::MSG_RAW_MOUSE_MIDDLE_BUTTON_UP:
			gwm = GWM_MIDDLE_UP;
			break;

		case GameMessage::MSG_RAW_MOUSE_MIDDLE_DRAG:
			gwm = GWM_MIDDLE_DRAG;
			break;

		// ------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_RIGHT_DOUBLE_CLICK:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN:
			gwm = GWM_RIGHT_DOWN;
			break;

		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP:
			gwm = GWM_RIGHT_UP;
			break;

		case GameMessage::MSG_RAW_MOUSE_RIGHT_DRAG:
			gwm = GWM_RIGHT_DRAG;
			break;

		// ------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_WHEEL:
			if( msg->getArgument( 1 )->real > 0 )
				gwm = GWM_WHEEL_UP;
			else
				gwm = GWM_WHEEL_DOWN;
			break;

	}

	return gwm;

}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//=============================================================================
WindowTranslator::WindowTranslator()
{
}

//=============================================================================
WindowTranslator::~WindowTranslator()
{
}

// WindowTranslator ===========================================================
/** Window translator that monitors raw input messages on the stream and
	* acts on anything relevant to the windowing system */
//=============================================================================
GameMessageDisposition WindowTranslator::translateGameMessage(const GameMessage *msg)
{
	GameMessageDisposition disp = KEEP_MESSAGE;
	Bool forceKeepMessage = FALSE;
	WinInputReturnCode returnCode = WIN_INPUT_NOT_USED;

	if (TheTacticalView && TheTacticalView->isMouseLocked())
	{
		//Kris: Aug 15, 2003
		//Added the scrolling check that will not return KEEP_MESSAGE if we happen
		//to in scrolling mode (via keyboard or mouse) and left click in the controlbar.
		//Without this code, the left click goes through the interface ignoring buttons and blockage
		//and ends up issuing orders right through the controlbar!
		if( TheInGameUI->isScrolling() )
		{
			if( msg->getType() != GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP &&
					msg->getType() != GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN )
			{
				//We're scrolling, but unless we're clicking the left button, get out.
				return KEEP_MESSAGE;
			}
			//Pass through and handle button clicks or getting input blocked!
		}
		else
		{
			return KEEP_MESSAGE;
		}
	}

	switch( msg->getType() )
	{
		// ------------------------------------------------------------------------
		case GameMessage::MSG_META_TOGGLE_ATTACKMOVE:
		{
			// Basically, we're cheating here. The mouse no longer sends us useless spam.
			ICoord2D mousePos = TheMouse->getMouseStatus()->pos;

			if( TheWindowManager )
				TheWindowManager->winProcessMouseEvent( GWM_NONE, &mousePos, nullptr );

			// Force it to keep the message, regardless of what the window thinks it did with the input.
			return KEEP_MESSAGE;
		}

		// ------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP:
		{
			if( TheInGameUI && TheInGameUI->isPlacementAnchored() )
			{
				//If we release the button outside
				forceKeepMessage = TRUE;
			}
			FALLTHROUGH; //FALL THROUGH INTENTIONALLY!
		}
		case GameMessage::MSG_RAW_MOUSE_POSITION:
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
		case GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK:
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_BUTTON_DOWN:
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_DOUBLE_CLICK:
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_BUTTON_UP:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_DOUBLE_CLICK:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP:
		{
			// all window events have the position of the mouse as arg 0
			ICoord2D mousePos = msg->getArgument( 0 )->pixel;
#if defined(RTS_DEBUG)	//debug hack to view object under mouse stats
			TheMousePos.x = mousePos.x;
			TheMousePos.y = mousePos.y;
#endif

			// process the mouse event position
			GameWindowMessage gwm = rawMouseToWindowMessage( msg );
			if( TheWindowManager )
				returnCode = TheWindowManager->winProcessMouseEvent( gwm, &mousePos, nullptr );

			if( TheShell && TheShell->isShellActive() )
				returnCode = WIN_INPUT_USED;

			if ( TheInGameUI && TheInGameUI->getInputEnabled() == FALSE )
				returnCode = WIN_INPUT_USED;

			break;

		}

		// ------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_LEFT_DRAG:
		case GameMessage::MSG_RAW_MOUSE_MIDDLE_DRAG:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_DRAG:
		{
			// all window events have the position of the mouse as arg 0
			ICoord2D mousePos = msg->getArgument( 0 )->pixel;

			// get delta for drag
			ICoord2D delta = msg->getArgument( 1 )->pixel;

			// process drag event
			GameWindowMessage gwm = rawMouseToWindowMessage( msg );
			if( TheWindowManager )
				returnCode = TheWindowManager->winProcessMouseEvent( gwm, &mousePos, &delta );

			if( TheShell && TheShell->isShellActive() )
				returnCode = WIN_INPUT_USED;

			if ( TheInGameUI && TheInGameUI->getInputEnabled() == FALSE )
				returnCode = WIN_INPUT_USED;

			break;

		}

		// ------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_WHEEL:
		{
			// all window events have the position of the mouse as arg 0
			ICoord2D mousePos = msg->getArgument( 0 )->pixel;

			// get wheel position
			Real wheelPos = msg->getArgument( 1 )->real;

			// process wheel event
			GameWindowMessage gwm = rawMouseToWindowMessage( msg );
			if( TheWindowManager )
				returnCode = TheWindowManager->winProcessMouseEvent( gwm, &mousePos,
																														 &wheelPos );

			if( TheShell && TheShell->isShellActive() )
				returnCode = WIN_INPUT_USED;

			if ( TheInGameUI && TheInGameUI->getInputEnabled() == FALSE )
				returnCode = WIN_INPUT_USED;

			break;

		}

		// ------------------------------------------------------------------------
		case GameMessage::MSG_RAW_KEY_DOWN:
		case GameMessage::MSG_RAW_KEY_UP:
		{
			// get key and state from args
			UnsignedByte key		= msg->getArgument( 0 )->integer;
			UnsignedByte state	= msg->getArgument( 1 )->integer;

			// process event through window system
			if( TheWindowManager )
				returnCode = TheWindowManager->winProcessKey( key, state );


			// If we're in a movie, we want to be able to escape out of it
			if(returnCode != WIN_INPUT_USED
				&& (key == KEY_ESC)
				&& (BitIsSet( state, KEY_STATE_UP ))
				&& TheDisplay->isMoviePlaying()
				&& TheGlobalData->m_allowExitOutOfMovies == TRUE )
			{
				TheDisplay->stopMovie();
				returnCode = WIN_INPUT_USED;
			}

			// TheSuperHackers @bugfix If the input is disabled, then only allow the ESC button to get through.
			// Otherwise it would be possible to call user camera actions during scripted camera scenes.
			if(returnCode != WIN_INPUT_USED
				&& (key != KEY_ESC)
				&& (TheInGameUI && (TheInGameUI->getInputEnabled() == FALSE)) )
			{
				returnCode = WIN_INPUT_USED;
			}

			break;

		}

		// ------------------------------------------------------------------------
		default:
			break;

	}

	// remove event from the stream if the return code specifies to do so
	// If TheShell doesn't exist, then well, we're not in RTS, we're in GUIEdit
	if( returnCode == WIN_INPUT_USED && !forceKeepMessage )// || (TheShell && TheShell->isShellActive()))
	{
		disp = DESTROY_MESSAGE;
	}

	return disp;

}
