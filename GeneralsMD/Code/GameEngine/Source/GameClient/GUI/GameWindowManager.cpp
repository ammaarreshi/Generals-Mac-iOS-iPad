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

// FILE: GameWindowManager.cpp ////////////////////////////////////////////////////////////////////
// Created:   Colin Day, June 2001
//						Dean Iverson, March 1998 (Original window code)
// Desc:      The game window manager is the singleton class that we interface
//						with to interact with the game windowing system.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <stdio.h>

#include "Common/Debug.h"
#include "Common/Language.h"
#include "GameClient/Display.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindow.h"
#include "GameClient/Mouse.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Gadget.h"
#include "GameClient/GameWindowGlobal.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetTabControl.h"
#include "GameClient/GadgetProgressBar.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GadgetSlider.h"
#include "GameClient/GadgetRadioButton.h"
#include "GameClient/GadgetCheckBox.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/GameWindowTransitions.h"
#include "Common/NameKeyGenerator.h"

// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
GameWindowManager *TheWindowManager = nullptr;
UnsignedInt WindowLayoutCurrentVersion = 2;

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//
// with this status set to true, the window system will propagate mouse position
// messages to windows.  You may want to disable this if you feel the mouse position
// messages are "spamming" your window and making a particular debugging situation
// difficult.  Make sure you do enable this before you check in again tho because
// it is necessary for any code that needs to look at objects or anything under
// the radar cursor
//
static Bool sendMousePosMessages = TRUE;

//-------------------------------------------------------------------------------------------------
/** Process windows waiting to be destroyed */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::processDestroyList()
{
	GameWindow *next;
	GameWindow *doDestroy;

	//
	// we need to pass the ownership of the destroy list so
	// if, while destroying a window, we need to add other windows
	// to the destroy list it won't cause problems.
	//
	doDestroy = m_destroyList;

	// set the list to empty
	m_destroyList = nullptr;

	// do the destroys
	for( ; doDestroy; doDestroy = next )
	{

		next = doDestroy->m_next;

		// Check to see if this window is "special"
		if( m_mouseCaptor == doDestroy )
			winRelease( doDestroy );

		if( m_keyboardFocus == doDestroy )
			winSetFocus( nullptr );

		if( (m_modalHead != nullptr) && (doDestroy == m_modalHead->window) )
			winUnsetModal( m_modalHead->window );

		if( m_currMouseRgn == doDestroy )
			m_currMouseRgn = nullptr;

		if( m_grabWindow == doDestroy )
			m_grabWindow = nullptr;

		// send the destroy message to the window we're about to kill
		winSendSystemMsg( doDestroy, GWM_DESTROY, 0, 0 );

		DEBUG_ASSERTCRASH(doDestroy->winGetUserData() == nullptr, ("Win user data is expected to be deleted now"));

		// free the memory
		deleteInstance(doDestroy);

	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** Generic function to simply propagate only button press messages to parent and let it deal with it */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType PassSelectedButtonsToParentSystem( GameWindow *window, UnsignedInt msg,
																												WindowMsgData mData1, WindowMsgData mData2 )
{

	// sanity
	if( window == nullptr )
		return MSG_IGNORED;

	if( (msg == GBM_SELECTED)  ||  (msg == GBM_SELECTED_RIGHT) || (msg == GBM_MOUSE_ENTERING) || (msg == GBM_MOUSE_LEAVING) || (msg == GEM_EDIT_DONE))
	{
		GameWindow *parent = window->winGetParent();

		if( parent )
			return TheWindowManager->winSendSystemMsg( parent, msg, mData1, mData2 );

	}

	return MSG_IGNORED;

}

//-------------------------------------------------------------------------------------------------
/** Generic function to simply propagate only button press messages to parent and let it deal with it */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType PassMessagesToParentSystem( GameWindow *window, UnsignedInt msg,
																												WindowMsgData mData1, WindowMsgData mData2 )
{

	// sanity
	if( window == nullptr )
		return MSG_IGNORED;


	GameWindow *parent = window->winGetParent();

	if( parent )
		return TheWindowManager->winSendSystemMsg( parent, msg, mData1, mData2 );

	return MSG_IGNORED;

}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GameWindowManager::GameWindowManager()
{

	m_windowList = nullptr;			// list of all top level windows
	m_windowTail = nullptr;			// last in windowList

	m_destroyList = nullptr;			// list of windows to destroy

	m_currMouseRgn = nullptr;		// window that mouse is over
	m_mouseCaptor = nullptr;			// window that captured mouse
	m_keyboardFocus = nullptr;		// window that has input focus
	m_modalHead = nullptr;			// top of windows in the modal stack
	m_grabWindow = nullptr;			// window that grabbed the last down event
	m_loneWindow = nullptr;		// Set if we just opened a combo box

	m_cursorBitmap = nullptr;
	m_captureFlags = 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GameWindowManager::~GameWindowManager()
{

	// destroy all windows
	winDestroyAll();
	freeStaticStrings();

	delete TheTransitionHandler;
	TheTransitionHandler = nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Initialize the game window manager system */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::init()
{
	if(!TheTransitionHandler)
		TheTransitionHandler = NEW GameWindowTransitionsHandler;
	TheTransitionHandler->load();
	TheTransitionHandler->init();
}

//-------------------------------------------------------------------------------------------------
/** Reset window system */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::reset()
{

	// destroy all windows left
	winDestroyAll();
	if(TheTransitionHandler)
		TheTransitionHandler->reset();

}

//-------------------------------------------------------------------------------------------------
/** Update cycle for game window manager */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::update()
{

	// Process windows waiting to be destroyed
	processDestroyList();
	if(TheTransitionHandler)
		TheTransitionHandler->update();
}

//-------------------------------------------------------------------------------------------------
/** Puts a window at the head of the window list */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::linkWindow( GameWindow *window )
{
	GameWindow *lastModalWindow = nullptr;
	GameWindow *tmp = m_windowList;
	while (tmp)
	{
		const ModalWindow *modal = m_modalHead;
		while (modal)
		{
			if (modal->window == tmp && modal->window != window)
			{
				lastModalWindow = tmp;
			}
			modal = modal->next;
		}
		tmp = tmp->m_next;
	}

	if (!lastModalWindow)
	{

		// Add to head of the top level window list
		window->m_prev = nullptr;
		window->m_next = m_windowList;

		if( m_windowList )
			m_windowList->m_prev = window;
		else
		{
			// first on list is also the tail
			m_windowTail = window;
		}

		m_windowList = window;
	}
	else
	{
		// lastModalWindow points to a modal window - add behind it
		window->m_prev = lastModalWindow;
		window->m_next = lastModalWindow->m_next;
		lastModalWindow->m_next = window;
		if (window->m_next)
		{
			window->m_next->m_prev = window;
		}
	}

}

//-------------------------------------------------------------------------------------------------
/** Insert the window ahead of the the 'aheadOf' window.  'aheadOf' can
	* be a window in the master list or a child of any window in that master
	* list */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::insertWindowAheadOf( GameWindow *window,
																						 GameWindow *aheadOf )
{

	// sanity
	if( window == nullptr )
		return;

	// we'll say that an aheadOf window means at the head of the list
	if( aheadOf == nullptr )
	{

		linkWindow( window );
		return;

	}

	// get parent of aheadOf
	GameWindow *aheadOfParent = aheadOf->winGetParent();

	//
	// if ahead of has no parent insert it in the master list just before
	// ahead of
	//
	if( aheadOfParent == nullptr )
	{

		window->m_prev = aheadOf->m_prev;

		if( aheadOf->m_prev )
			aheadOf->m_prev->m_next = window;
		else
			m_windowList = window;

		aheadOf->m_prev = window;
		window->m_next = aheadOf;

	}
	else
	{

		window->m_prev = aheadOf->m_prev;

		if( aheadOf->m_prev )
			aheadOf->m_prev->m_next = window;
		else
			aheadOfParent->m_child = window;

		aheadOf->m_prev = window;
		window->m_next = aheadOf;

		window->m_parent = aheadOfParent;

	}

}

//-------------------------------------------------------------------------------------------------
/** Takes a window off the window list */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::unlinkWindow( GameWindow *window )
{

	if( window->m_next )
		window->m_next->m_prev = window->m_prev;
	else
	{
		// no next means this is the tail
		m_windowTail = window->m_prev;
	}

	if( window->m_prev )
		window->m_prev->m_next = window->m_next;
	else
		m_windowList = window->m_next;

}

//-------------------------------------------------------------------------------------------------
/** Takes a child window off its parent's window list */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::unlinkChildWindow( GameWindow *window )
{

	if( window->m_prev )
	{

		window->m_prev->m_next = window->m_next;

		if( window->m_next )
			window->m_next->m_prev = window->m_prev;

	}
	else
	{

		if( window->m_next )
		{

			window->m_parent->m_child = window->m_next;

			window->m_next->m_prev = window->m_prev;

			window->m_next = nullptr;

		}
		else
		{

			window->m_parent->m_child = nullptr;

		}

	}

	// remove the parent reference from this window
	window->m_parent = nullptr;

}

//-------------------------------------------------------------------------------------------------
/** Check window and parents to see if this window is enabled */
//-------------------------------------------------------------------------------------------------
Bool GameWindowManager::isEnabled( GameWindow *win )
{

	// sanity
	if( win == nullptr )
		return FALSE;

	if( BitIsSet( win->m_status, WIN_STATUS_ENABLED ) == FALSE )
	{
		return FALSE;
	}

	while( win->m_parent )
	{
		win = win->m_parent;
		if( BitIsSet( win->m_status, WIN_STATUS_ENABLED ) == FALSE )
		{
			return FALSE;
		}
	}

	return TRUE;

}

//-------------------------------------------------------------------------------------------------
/** Check window and parents to see if this window is hidden */
//-------------------------------------------------------------------------------------------------
Bool GameWindowManager::isHidden( GameWindow *win )
{

	// we'll allow for the idea that if a window doesn't exist it is hidden
	if( win == nullptr )
		return TRUE;

	if( BitIsSet( win->m_status, WIN_STATUS_HIDDEN ))
	{
		return TRUE;
	}

	while( win->m_parent )
	{
		win = win->m_parent;
		if( BitIsSet( win->m_status, WIN_STATUS_HIDDEN ))
		{
			return TRUE;
		}
	}

	return FALSE;

}

//-------------------------------------------------------------------------------------------------
// Adds a child window to its parent.
//-------------------------------------------------------------------------------------------------
void GameWindowManager::addWindowToParent( GameWindow *window,
																					 GameWindow *parent )
{
	if( parent )
	{

		// add to parent's list of children
		window->m_prev = nullptr;
		window->m_next = parent->m_child;

		if( parent->m_child )
			parent->m_child->m_prev = window;

		parent->m_child = window;

		window->m_parent = parent;

	}

}

//-------------------------------------------------------------------------------------------------
/** Add a child window to the parent, put place it at the end of the
	* parent window child list */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::addWindowToParentAtEnd( GameWindow *window,
																								GameWindow *parent )
{

	if( parent )
	{

		window->m_prev = nullptr;
		window->m_next = nullptr;
		if( parent->m_child )
		{
			GameWindow *last;

			// wind down to last child in list
			last = parent->m_child;
			while( last->m_next != nullptr )
				last = last->m_next;

			// tie to list
			last->m_next = window;
			window->m_prev = last;

		}
		else
			parent->m_child = window;

		// assign the parent to the window
		window->m_parent = parent;

	}

}

//-------------------------------------------------------------------------------------------------
/** this gets called from winHide() when a window hides itself */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::windowHiding( GameWindow *window )
{

	// if this window has keyboard focus remove it
	if( m_keyboardFocus == window )
		m_keyboardFocus = nullptr;

	// if this is the modal head, unset it
	if( m_modalHead && m_modalHead->window == window )
		winUnsetModal( window );

	// if this is the captor, it shall no longer be
	if( m_mouseCaptor == window )
		winCapture( nullptr );

	//
	// since hiding a parent will also hide the children, when a parent
	// hides we must call this same method for all the children so they
	// each have a chance to go through this logic
	//
	GameWindow *child;
	for( child = window->winGetChild(); child; child = child->winGetNext() )
		windowHiding( child );

}

//-------------------------------------------------------------------------------------------------
/** Hide all windows in a certain range of id's (inclusive) */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::hideWindowsInRange( GameWindow *baseWindow,
																						Int first, Int last,
																						Bool hideFlag )
{
	Int i;
	GameWindow *window;

	for( i = first; i <= last; i++ )
	{

		window = winGetWindowFromId( baseWindow, i );
		if( window )
			window->winHide( hideFlag );

	}

}

//-------------------------------------------------------------------------------------------------
// Enable all windows in a certain range of id's (inclusive)
//-------------------------------------------------------------------------------------------------
void GameWindowManager::enableWindowsInRange( GameWindow *baseWindow,
																							Int first, Int last,
																							Bool enableFlag )
{
	Int i;
	GameWindow *window;

	for( i =first; i <= last; i++ )
	{

		window = winGetWindowFromId( baseWindow, i );
		if( window )
			window->winEnable( enableFlag );

	}

}

//-------------------------------------------------------------------------------------------------
/** Captures the mouse capture. */
//-------------------------------------------------------------------------------------------------
Int GameWindowManager::winCapture( GameWindow *window )
{

	if( m_mouseCaptor != nullptr)
		return WIN_ERR_MOUSE_CAPTURED;

	m_mouseCaptor = window;

	return WIN_ERR_OK;

}

//-------------------------------------------------------------------------------------------------
/** Releases the mouse capture. */
//-------------------------------------------------------------------------------------------------
Int GameWindowManager::winRelease( GameWindow *window )
{

	if( window == m_mouseCaptor )
		m_mouseCaptor = nullptr;

	return WIN_ERR_OK;

}

//-------------------------------------------------------------------------------------------------
/** Returns the current mouse captor. */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::winGetCapture()
{

	return m_mouseCaptor;

}

//-------------------------------------------------------------------------------------------------
/** Gets the window pointer from its id */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::winGetWindowFromId( GameWindow *window, Int id )
{

	if( window == nullptr )
		window = m_windowList;

	for( ; window; window = window->m_next )
	{

		if( window->winGetWindowId() == id)
			return window;
		else if( window->m_child )
		{
			GameWindow *child = winGetWindowFromId( window->m_child, id );

			if( child )
				return child;

		}

	}

	return nullptr;

}

//-------------------------------------------------------------------------------------------------
/** Gets the Window List Pointer */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::winGetWindowList()
{

	return m_windowList;

}

//-------------------------------------------------------------------------------------------------
/** Send a system message to the specified window */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType GameWindowManager::winSendSystemMsg( GameWindow *window,
																					UnsignedInt msg,
																					WindowMsgData mData1,
																					WindowMsgData mData2 )
{

	if( window == nullptr)
		return MSG_IGNORED;

	if( msg != GWM_DESTROY && BitIsSet( window->m_status, WIN_STATUS_DESTROYED ) )
		return MSG_IGNORED;

	return window->m_system( window, msg, mData1, mData2 );

}

//-------------------------------------------------------------------------------------------------
/** Send a system message to the specified window */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType GameWindowManager::winSendInputMsg( GameWindow *window,
																				 UnsignedInt msg,
																				 WindowMsgData mData1,
																				 WindowMsgData mData2 )
{

	if( window == nullptr )
		return MSG_IGNORED;

	if( msg != GWM_DESTROY && BitIsSet( window->m_status, WIN_STATUS_DESTROYED ) )
		return MSG_IGNORED;

	return window->m_input( window, msg, mData1, mData2 );

}

//-------------------------------------------------------------------------------------------------
/** Get the current input focus */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::winGetFocus()
{

	return m_keyboardFocus;

}

//-------------------------------------------------------------------------------------------------
/** Set the current input focus */
//-------------------------------------------------------------------------------------------------
Int GameWindowManager::winSetFocus( GameWindow *window )
{
	Bool wantsFocus = FALSE;

	// if a window doesn't want keyboard focus don't give it
	if( window )
		if( BitIsSet( window->winGetStatus(), WIN_STATUS_NO_FOCUS) )
			return 0;

	//
	// Tell current focus window that it's losing focus
	// unless we are trying to give focus to the current focus window
	//
	if( (m_keyboardFocus) && (m_keyboardFocus != window) )
	{
		Bool wf;	// dummy var, ignored, but must be passed
		winSendSystemMsg( m_keyboardFocus, GWM_INPUT_FOCUS, FALSE, (WindowMsgData)&wf );
	}

	// Set focus to new window
	m_keyboardFocus = window;

	// Tell new focus window that it has focus
	if( m_keyboardFocus )
	{

		for (;;)
		{
			winSendSystemMsg( window, GWM_INPUT_FOCUS, TRUE, (WindowMsgData)&wantsFocus );
			if (wantsFocus)
				break;

			window = window->winGetParent();
			if( window == nullptr )
				break;
		}

	}

	// If new window doesn't want focus, set focus to nullptr
	if( wantsFocus == FALSE )
		m_keyboardFocus = nullptr;

	return WIN_ERR_OK;

}

//-------------------------------------------------------------------------------------------------
/** Process key press through the GUI. */
//-------------------------------------------------------------------------------------------------
WinInputReturnCode GameWindowManager::winProcessKey( UnsignedByte key,
																										 UnsignedByte state )
{
	WinInputReturnCode returnCode = WIN_INPUT_NOT_USED;

	// Check for keyboard focus and a legal key for sanity
	if( m_keyboardFocus && (key != KEY_NONE) )
	{
		GameWindow *win = m_keyboardFocus;

		returnCode = WIN_INPUT_USED;  // assume input will be used

		//
		// Pass the keystroke up the window hierarchy until it is
		// processed or we reach the top level window
		//
		while( winSendInputMsg( win, GWM_CHAR, key, state ) == MSG_IGNORED )
		{

			win = win->winGetParent();
			if( win == nullptr )
			{

				returnCode = WIN_INPUT_NOT_USED;  // oops, it wasn't used after all
				break;

			}

		}

	}

	return returnCode;

}

//-------------------------------------------------------------------------------------------------
/** Process a single mouse event through the window system */
//-------------------------------------------------------------------------------------------------
WinInputReturnCode GameWindowManager::winProcessMouseEvent( GameWindowMessage msg,
																														ICoord2D *mousePos,
																														void *data )
{
	WinInputReturnCode returnCode = WIN_INPUT_NOT_USED;
	Bool objectTooltip = FALSE;
	UnsignedInt packedMouseCoords;
	GameWindow *window = nullptr;
	GameWindow *toolTipWindow = nullptr;
	Int dx, dy;
	Bool clearGrabWindow = FALSE;

	// pack mouse coords into one entity for message passing
	packedMouseCoords = SHORTTOLONG( mousePos->x, mousePos->y );

	// clear tooltip ... it will be reset if necessary
	TheMouse->setCursorTooltip( UnicodeString::TheEmptyString );

	// Check for mouse capture
	if( m_mouseCaptor )
	{

		// no window grabbed as of yet
		m_grabWindow = nullptr;

		// what what window within the captured window are we in
		window = m_mouseCaptor->winPointInChild( mousePos->x, mousePos->y );

		//
		// send buttons, drags, wheels to the windows, we don't continually
		// send mouse positions
		//
		if( sendMousePosMessages == TRUE || msg != GWM_MOUSE_POS )
		{
			GameWindow *win = window;

			if( win )
			{
				while( win != nullptr )
				{

					if( winSendInputMsg( win, msg, packedMouseCoords, 0 ) == MSG_HANDLED )
					{

						// if used clear the event
						returnCode = WIN_INPUT_USED;
						break;

					}

					// if we just tested mouseCaptor don't go any higher in the chain
					if( win == m_mouseCaptor )
						break;

					win = win->winGetParent();

				}

			}
			else
			{

				// if used clear the event
				if(	winSendInputMsg( m_mouseCaptor, msg, packedMouseCoords, 0 ) == MSG_HANDLED )
					returnCode = WIN_INPUT_USED;

			}

		}

	}
	else
	{

		if( m_grabWindow )
		{
			GameWindow *parent;

			switch( msg )
			{

				// --------------------------------------------------------------------
				case GWM_LEFT_UP:
				{
					//Play a beep sound if the window is disabled.
					m_grabWindow->winPointInChild( mousePos->x, mousePos->y, FALSE, TRUE );

					BitClear( m_grabWindow->m_status, WIN_STATUS_ACTIVE );
					if( m_grabWindow->winPointInWindow( mousePos->x, mousePos->y ) )
						winSendInputMsg( m_grabWindow, GWM_LEFT_UP, packedMouseCoords, 0 );
					else if( BitIsSet( m_grabWindow->m_status, WIN_STATUS_DRAGGABLE ))
					{
						winSendInputMsg( m_grabWindow, GWM_LEFT_UP, packedMouseCoords, 0 );
					}

					clearGrabWindow = TRUE;
					break;

				}

				// --------------------------------------------------------------------
				case MOUSE_EVENT_NONE:
				case GWM_LEFT_DRAG:
				{

					if( BitIsSet( m_grabWindow->m_status, WIN_STATUS_DRAGGABLE ) )
					{
						ICoord2D *mouseDelta = (ICoord2D *)data;
						dx = mouseDelta->x;
						dy = mouseDelta->y;

						// Clip window to parent
						if( m_grabWindow->winGetParent() )
						{

							parent = m_grabWindow->winGetParent();

							if( m_grabWindow->m_region.lo.x + dx < 0 )
								dx = 0 - m_grabWindow->m_region.lo.x;
							else if( m_grabWindow->m_region.hi.x + dx > parent->m_size.x )
								dx = parent->m_size.x - m_grabWindow->m_region.hi.x;

							if( m_grabWindow->m_region.lo.y + dy < 0 )
								dy = 0 - m_grabWindow->m_region.lo.y;
							else if( m_grabWindow->m_region.hi.y + dy > parent->m_size.y )
								dy = parent->m_size.y - m_grabWindow->m_region.hi.y;
						}

						// Move the window, but keep it completely visible within screen boundaries
						IRegion2D newRegion;
						ICoord2D grabSize;

						m_grabWindow->winGetPosition( &newRegion.lo.x, &newRegion.lo.y );
						m_grabWindow->winGetSize( &grabSize.x, &grabSize.y );

						newRegion.lo.x += dx;
						newRegion.lo.y += dy;
						if( newRegion.lo.x < 0 )
							newRegion.lo.x = 0;
						if( newRegion.lo.y < 0 )
							newRegion.lo.y = 0;

						newRegion.hi.x = newRegion.lo.x + grabSize.x;
						newRegion.hi.y = newRegion.lo.y + grabSize.y;
						if( newRegion.hi.x > (Int)TheDisplay->getWidth() )
							newRegion.hi.x = (Int)TheDisplay->getWidth();
						if( newRegion.hi.y > (Int)TheDisplay->getHeight() )
							newRegion.hi.y = (Int)TheDisplay->getHeight();

						newRegion.lo.x = newRegion.hi.x - grabSize.x;
						newRegion.lo.y = newRegion.hi.y - grabSize.y;

						m_grabWindow->winSetPosition( newRegion.lo.x, newRegion.lo.y );

					}

					// Send mouse drag message
					winSendInputMsg( m_grabWindow, msg, packedMouseCoords, 0 );
					break;

				}

			}

			// mark event handled
			returnCode = WIN_INPUT_USED;

		}
		else
		{

			if( m_modalHead && m_modalHead->window )
			{
				window = m_modalHead->window->winPointInChild( mousePos->x, mousePos->y );
			}
			else
			{
				// search for top-level window which contains pointer
				window = findWindowUnderMouse(toolTipWindow, mousePos, WIN_STATUS_ABOVE, WIN_STATUS_HIDDEN);

				// check !above, below and hidden
				if( window == nullptr )
					window = findWindowUnderMouse(toolTipWindow, mousePos, WIN_STATUS_NONE, WIN_STATUS_ABOVE | WIN_STATUS_BELOW | WIN_STATUS_HIDDEN);

				// check below and !hidden
				if( window == nullptr )
					window = findWindowUnderMouse(toolTipWindow, mousePos, WIN_STATUS_BELOW, WIN_STATUS_HIDDEN);
			}

			if( window )
				if( BitIsSet( window->m_status, WIN_STATUS_NO_INPUT ) )
				{
					if(window->winGetParent() && BitIsSet( window->winGetParent()->winGetInstanceData()->getStyle(), GWS_COMBO_BOX ))
						window = window->winGetParent();
					else
						window = nullptr;
				}

			if( window )
			{
				GameWindow *tempWin;

				//
				// only send messages for button states, wheel states, we do not
				// continually send messages for mouse positions
				//
				if( sendMousePosMessages == TRUE || msg != GWM_MOUSE_POS )
				{

					tempWin = window;

					// Give everyone a chance to do something with the clicks
					GameWindow *oldLoneWindow = m_loneWindow;
					while( winSendInputMsg( tempWin, msg, packedMouseCoords, 0 ) == MSG_IGNORED )
					{

						tempWin = tempWin->m_parent;
						if( tempWin == nullptr )
							break;

					}


					// First check to see if m_loneWindow is set if so, close the window
					if( m_loneWindow && m_loneWindow == oldLoneWindow
					&&( msg == GWM_LEFT_UP || msg == GWM_MIDDLE_UP || msg == GWM_RIGHT_UP || tempWin))
					{
						if(!m_loneWindow->winIsChild(tempWin))
							winSetLoneWindow( nullptr );
						/*
								ComboBoxData *cData = (ComboBoxData *)m_comboBoxOpen->winGetUserData();
															// verify that the window that ate the message wasn't one of our own
															if(cData->dropDownButton != tempWin &&
																cData->editBox != tempWin &&
																cData->listBox != tempWin &&
																cData->listboxData->upButton != tempWin &&
																cData->listboxData->downButton != tempWin &&
																cData->listboxData->slider != tempWin &&
																cData->listboxData->slider != tempWin->winGetParent())
																	winSetOpenComboBoxWindow( nullptr );*/

					}
					if( tempWin )
					{

						//
						// Someone cares, if this is a left button down event
						// it should get "grabbed"
						/// @todo should allow for left handed mouse configs here?
						//
						if( msg == GWM_LEFT_DOWN )
						{

//						if( tempWin != windowList )
//							WinActivate( tempWin );
							m_grabWindow = tempWin;

						}

						// event is used
						returnCode = WIN_INPUT_USED;

					}

				}

			}

			if( toolTipWindow == nullptr )
			{

				if( isHidden( window ) == FALSE )
					toolTipWindow = window;

			}

			// if tooltips are on set them into the window
			Bool tooltipsOn = TRUE;
			if( tooltipsOn )
			{
//				if( toolTipWindow && toolTipWindow->winGetParent() && BitIsSet( toolTipWindow->winGetParent()->winGetInstanceData()->getStyle(), GWS_COMBO_BOX ))
//					toolTipWindow = toolTipWindow->winGetParent();
				if( toolTipWindow )
				{
					// do we have a callback to call for the tooltip
					if( toolTipWindow->m_tooltip )
						toolTipWindow->m_tooltip( toolTipWindow,
																			&toolTipWindow->m_instData,
																			packedMouseCoords );

					// else, do we have a normal tooltip to set
					else if( toolTipWindow->m_instData.getTooltipTextLength() )
						TheMouse->setCursorTooltip( toolTipWindow->m_instData.getTooltipText(), toolTipWindow->m_instData.m_tooltipDelay );

				}
				else
				{

					//
					// not pointing at a window... perhaps we are pointing at a valid
					// tooltip-able object in the game world ... let's set a flag so
					// during the object testing we can set the tooltip, we can do
					// whatever we like now that we know no other tooltip was set from
					// a window
					//
					objectTooltip = TRUE;

				}

			}

		}

	}

	//
	// check if new current window is different from the last
	// but only if both windows fall within the mouseCaptor if one exists
	//
	if( (m_grabWindow == nullptr) && (window != m_currMouseRgn) )
	{
		if( m_mouseCaptor )
		{
			if( m_mouseCaptor->winIsChild( m_currMouseRgn ) )
				winSendInputMsg( m_currMouseRgn, GWM_MOUSE_LEAVING, packedMouseCoords, 0 );
		}
		else if( m_currMouseRgn )
			winSendInputMsg( m_currMouseRgn, GWM_MOUSE_LEAVING, packedMouseCoords, 0 );

		if( window )
			winSendInputMsg( window, GWM_MOUSE_ENTERING, packedMouseCoords, 0 );

		m_currMouseRgn = window;

	}

	// clear grabWindow if necessary
	if( clearGrabWindow == TRUE )
	{

		m_grabWindow = nullptr;
		clearGrabWindow = FALSE;

	}

	return returnCode;

}

bool GameWindowManager::isMouseWithinWindow(GameWindow* window, const ICoord2D* mousePos, unsigned int requiredStatusMask, unsigned int forbiddenStatusMask)
{
	return ((BitIsSet(window->m_status, requiredStatusMask) || requiredStatusMask == 0) &&
		!BitIsSet(window->m_status, forbiddenStatusMask) &&
		mousePos->x >= window->m_region.lo.x &&
		mousePos->x <= window->m_region.hi.x &&
		mousePos->y >= window->m_region.lo.y &&
		mousePos->y <= window->m_region.hi.y);
}

GameWindow* GameWindowManager::findWindowUnderMouse(GameWindow*& toolTipWindow, const ICoord2D* mousePos, unsigned int requiredStatusMask, unsigned int forbiddenStatusMask)
{
	for (GameWindow* window = m_windowList; window; window = window->m_next)
	{
		if (!isMouseWithinWindow(window, mousePos, requiredStatusMask, forbiddenStatusMask))
			continue;

		if (toolTipWindow == nullptr)
		{
			GameWindow* childWindow = window->winPointInAnyChild(mousePos->x, mousePos->y, TRUE, TRUE);

			if (childWindow->m_tooltip || childWindow->m_instData.getTooltipTextLength())
				toolTipWindow = childWindow;
		}

		if (BitIsSet(window->m_status, WIN_STATUS_ENABLED))
		{
			// determine which child window the mouse is in
			return window->winPointInChild(mousePos->x, mousePos->y);
		}
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Draw a window and its children, in parent-first order.
	* Children's coordinates are relative to their parents.
	* Note that hidden windows automatically will not draw any
	* of their children ... but see-thru windows only will not
	* draw themselves, but will give their children an
	* opportunity to draw */
//-------------------------------------------------------------------------------------------------
Int GameWindowManager::drawWindow( GameWindow *window )
{
	GameWindow *child;

	if( window == nullptr )
		return WIN_ERR_INVALID_WINDOW;

	if( BitIsSet( window->m_status, WIN_STATUS_HIDDEN ) == FALSE )
	{

		if( !BitIsSet( window->m_status, WIN_STATUS_SEE_THRU ) && window->m_draw )
			window->m_draw( window, &window->m_instData );

		/// @todo visit list boxes and borders, this is stupid!
		// for list boxes only draw the borders BEFORE the children
		if( BitIsSet( window->winGetStyle(), GWS_SCROLL_LISTBOX ) )
			if( BitIsSet( window->m_status, WIN_STATUS_BORDER ) == TRUE &&
					!BitIsSet( window->m_status, WIN_STATUS_SEE_THRU ) )
				window->winDrawBorder();

		// draw children in reverse order just like the window list
		child = window->m_child;
		while( child && child->m_next )
			child = child->m_next;

		for( ; child; child = child->m_prev )
				drawWindow( child );

		//
		// draw the border for the window AFTER the window contents AND the
		// children contents have drawn
		//
		if( !BitIsSet( window->winGetStyle(), GWS_SCROLL_LISTBOX ) )
			if( BitIsSet( window->m_status, WIN_STATUS_BORDER ) == TRUE &&
					!BitIsSet( window->m_status, WIN_STATUS_SEE_THRU ) )
				window->winDrawBorder();

	}

	return WIN_ERR_OK;

}

//-------------------------------------------------------------------------------------------------
/** Draw the GUI in reverse order to correlate with clicking priority */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::winRepaint()
{
	GameWindow *window, *next;

	// draw below windows
	for( window = m_windowTail; window; window = next )
	{
		next = window->m_prev;

		if( BitIsSet( window->m_status, WIN_STATUS_BELOW ) )
			drawWindow( window );
	}

	// draw non-above and non-below windows
	for( window = m_windowTail; window; window = next )
	{
		next = window->m_prev;

		if (BitIsSet( window->m_status, WIN_STATUS_ABOVE |
																	 WIN_STATUS_BELOW ) == FALSE)
			drawWindow( window );
	}

	// draw above windows
	for( window = m_windowTail; window; window = next )
	{
		next = window->m_prev;

		if( BitIsSet( window->m_status, WIN_STATUS_ABOVE ) )
			drawWindow( window );
	}

	if(TheTransitionHandler)
		TheTransitionHandler->draw();
}

//-------------------------------------------------------------------------------------------------
/** Dump information about all the windows for resource problems */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::dumpWindow( GameWindow *window )
{
#ifndef FINAL
	GameWindow *child;

	if( window == nullptr )
		return;

	DEBUG_LOG(( "ID: %d\tRedraw: 0x%08X\tUser Data: %d",
				 	 window->winGetWindowId(), window->m_draw, window->m_userData ));

	for( child = window->m_child; child; child = child->m_next )
		dumpWindow( child );

	return;
#endif
}

//-------------------------------------------------------------------------------------------------
/** Create a new window by setting up its parameters and callbacks. */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::winCreate( GameWindow *parent,
																				  UnsignedInt status,
																				  Int x, Int y,
																				  Int width, Int height,
																					GameWinSystemFunc system,
																					WinInstanceData *instData )
{
	GameWindow *window;

	// allocate new window
	window = allocateNewWindow();
	if( window == nullptr )
	{

		DEBUG_LOG(( "WinCreate error: Could not allocate new window" ));
#ifndef FINAL
		{
			GameWindow *win;

			for( win = m_windowList; win; win = win->m_next )
				dumpWindow( win );
		}
#endif

		return nullptr;

	}

	// If this is a child window add it to the parent's window list
	if( parent )
		addWindowToParent( window, parent );
	else
		linkWindow( window );

	window->m_status = status;
	window->m_size.x = width;
	window->m_size.y = height;

	window->m_region.lo.x = x;
	window->m_region.lo.y = y;
	window->m_region.hi.x = x + width;
	window->m_region.hi.y = y + height;

	window->normalizeWindowRegion();

	// set the system function and send a create message to window
	window->winSetSystemFunc( system );
	winSendSystemMsg( window, GWM_CREATE, 0, 0 );

	// copy over instance data if present
	if( instData )
		window->winSetInstanceData( instData );

	// set default font
	if (TheGlobalLanguageData && TheGlobalLanguageData->m_defaultWindowFont.name.isNotEmpty())
	{
		window->winSetFont( winFindFont(
			TheGlobalLanguageData->m_defaultWindowFont.name,
			TheGlobalLanguageData->m_defaultWindowFont.size,
			TheGlobalLanguageData->m_defaultWindowFont.bold) );
	}
	else
	{
		window->winSetFont( winFindFont( "Times New Roman", 14, FALSE ) );
	}

	return window;

}

//-------------------------------------------------------------------------------------------------
/** Take a window and its children off the top level list and free
	* their allocation class data. */
//-------------------------------------------------------------------------------------------------
Int GameWindowManager::winDestroy( GameWindow *window )
{
	GameWindow *child, *next;

	if( window == nullptr )
		return WIN_ERR_INVALID_WINDOW;

	//
	// we should never have edit data allocated in the window code, it's
	// completely handled by the editor ONLY
	//
	DEBUG_ASSERTCRASH( window->winGetEditData() == nullptr,
										 ("winDestroy(): edit data should NOT be present!") );

	if( BitIsSet( window->m_status, WIN_STATUS_DESTROYED ) )
		return WIN_ERR_OK;

	BitSet( window->m_status, WIN_STATUS_DESTROYED );
	window->freeImages();

	if( m_mouseCaptor == window )
		winRelease( window );

	if( m_keyboardFocus == window )
		winSetFocus( nullptr );

	if( (m_modalHead != nullptr) && (window == m_modalHead->window) )
		winUnsetModal( m_modalHead->window );

	if( m_currMouseRgn == window )
		m_currMouseRgn = nullptr;

	if( m_grabWindow == window )
		m_grabWindow = nullptr;

	for( child = window->m_child; child; child = next )
	{
		next = child->m_next;
		winDestroy( child );
	}

	// Remove the top level window from list
	if( window->m_parent == nullptr )
		unlinkWindow( window );
	else
		unlinkChildWindow( window );

	// Add to head of the destroy list
	window->m_prev = nullptr;
	window->m_next = m_destroyList;

	m_destroyList = window;

	//
	// if this window is part of a layout screen, notify the screen that
	// this window is going away
	//
	if( window->m_layout )
		window->m_layout->removeWindow( window );

	return WIN_ERR_OK;

}

//-------------------------------------------------------------------------------------------------
/** Destroy all windows on the window list IMMEDIATELY */
//-------------------------------------------------------------------------------------------------
Int GameWindowManager::winDestroyAll()
{
	GameWindow *win, *next;

	//
	// NOTE that it is CRITICAL that the windows be destroyed this way,
	// the editor has windows that are not on this main list that must
	// exist throughout a reset of the system (copy/paste for instance)
	// so DO NOT ever change this to a clever pool of memory for the
	// windows and reset the _pool_ ... I will have to kill you!  CBD
	//

	for( next = win = m_windowList; next; win = next)
	{
		next = win->m_next;

		winDestroy( win );

	}

	// Destroy All Windows just added to destroy list
	processDestroyList();

	return WIN_ERR_OK;

}

//-------------------------------------------------------------------------------------------------
/** Sets selected window into a modal state.  This window will get
	* put at the top of a modal stack */
//-------------------------------------------------------------------------------------------------
Int GameWindowManager::winSetModal( GameWindow *window )
{
	ModalWindow *modal;

	if( window == nullptr )
		return WIN_ERR_INVALID_WINDOW;

	// verify requesting window is a root window
	if( window->m_parent != nullptr )
	{
		DEBUG_LOG(( "WinSetModal: Non Root window attempted to go modal." ));
		return WIN_ERR_INVALID_PARAMETER;			// return error if not
	}
	// Allocate new Modal Window Entry
	modal = newInstance(ModalWindow);
	if( modal == nullptr )
	{
		DEBUG_LOG(( "WinSetModal: Unable to allocate space for Modal Entry." ));
		return WIN_ERR_GENERAL_FAILURE;
	}

	// Put new entry at top of list
	modal->window = window;
	modal->next = m_modalHead;
	m_modalHead = modal;

	return WIN_ERR_OK;

}

//-------------------------------------------------------------------------------------------------
/** pops window off of the modal stack.  If this window is not the top
	* of the modal stack an error will occur. */
//-------------------------------------------------------------------------------------------------
Int GameWindowManager::winUnsetModal( GameWindow *window )
{
	ModalWindow *next;

	if( window == nullptr )
		return WIN_ERR_INVALID_WINDOW;

	// verify entry is at top of list
	if( (m_modalHead == nullptr) || (m_modalHead->window != window) )
	{

		// return error if not
		DEBUG_LOG(( "WinUnsetModal: Invalid window attempting to unset modal (%d)",
								window->winGetWindowId() ));
		return WIN_ERR_GENERAL_FAILURE;

	}

	// remove from top of list
	next = m_modalHead->next;
	deleteInstance(m_modalHead);
	m_modalHead = next;

	return WIN_ERR_OK;

}

//-------------------------------------------------------------------------------------------------
/** Get the grabbed window */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::winGetGrabWindow()
{

	return m_grabWindow;

}

//-------------------------------------------------------------------------------------------------
/** Explicitly set the grab window */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::winSetGrabWindow( GameWindow *window )
{

	m_grabWindow = window;

}

//-------------------------------------------------------------------------------------------------
/** Explicitly set the grab window */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::winSetLoneWindow( GameWindow *window )
{
	// ignore if we're trying to set the same window
	if( m_loneWindow == window )
		return;
	if( m_loneWindow )
		winSendSystemMsg( m_loneWindow, GGM_CLOSE, 0, 0 );
	m_loneWindow = window;

}

//-------------------------------------------------------------------------------------------------
/** Create a Modal Message Box */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoMessageBox(Int x, Int y, Int width, Int height, UnsignedShort buttonFlags,
                        UnicodeString titleString, UnicodeString bodyString,
                        GameWinMsgBoxFunc yesCallback,
                        GameWinMsgBoxFunc noCallback,
                        GameWinMsgBoxFunc okCallback,
                        GameWinMsgBoxFunc cancelCallback )
{
	return gogoMessageBox(x,y, width,height,buttonFlags,titleString,bodyString,yesCallback,noCallback,okCallback,cancelCallback, FALSE);
}

GameWindow *GameWindowManager::gogoMessageBox(Int x, Int y, Int width, Int height, UnsignedShort buttonFlags,
                        UnicodeString titleString, UnicodeString bodyString,
                        GameWinMsgBoxFunc yesCallback,
                        GameWinMsgBoxFunc noCallback,
                        GameWinMsgBoxFunc okCallback,
                        GameWinMsgBoxFunc cancelCallback, Bool useLogo )

{
	// first check to make sure we have some buttons to display
	if(buttonFlags == 0 )
	{
		return nullptr;
	}
	GameWindow *trueParent = nullptr;
	//Changed by Chris
	if(useLogo)
		trueParent = winCreateFromScript( "Menus/QuitMessageBox.wnd" );
	else
		trueParent = winCreateFromScript( "Menus/MessageBox.wnd" );
	//Added By Chris
	AsciiString menuName;
	if(useLogo)
		menuName.set("QuitMessageBox.wnd:");
	else
		menuName.set("MessageBox.wnd:");

	AsciiString tempName;
	GameWindow *parent = nullptr;

	tempName = menuName;
	tempName.concat("MessageBoxParent");
	parent = winGetWindowFromId(trueParent, TheNameKeyGenerator->nameToKey( tempName ));
	winSetModal( trueParent );
	winSetFocus( nullptr ); // make sure we lose focus from other windows even if we refuse focus ourselves
	winSetFocus( parent	 );

	// If the user wants the size to be different then the default
	float ratioX, ratioY = 1;

	if( width > 0 && height > 0 )
	{
		ICoord2D temp;
		//First grab the percent increase/decrease compared to the default size
		parent->winGetSize( &temp.x, &temp.y);
		ratioX = (float)width / (float)temp.x;
		ratioY = (float)height / (float)temp.y;
		//Set the window's new size
		parent->winSetSize( width, height);

		//Resize/reposition all the children windows based off the ratio
		GameWindow *child;
		for( child = parent->winGetChild(); child; child = child->winGetNext() )
		{
			child->winGetSize(&temp.x, &temp.y);
			temp.x =Int(temp.x * ratioX);
			temp.y =Int(temp.y * ratioY);
			child->winSetSize(temp.x, temp.y);

			child->winGetPosition(&temp.x, &temp.y);
			temp.x =Int(temp.x * ratioX);
			temp.y =Int(temp.y * ratioY);
			child->winSetPosition(temp.x, temp.y);
		}
	}

	// If the user wants to position the message box somewhere other then default
	if( x >= 0 && y >= 0)
		parent->winSetPosition(x, y);

	// Reposition the buttons
	Int buttonX[3], buttonY[3];

	//In the layout, buttonOk will be in the first button position
	NameKeyType buttonOkID = NAMEKEY_INVALID;

	tempName = menuName;
	tempName.concat("ButtonOk");
	buttonOkID = TheNameKeyGenerator->nameToKey( tempName );
	GameWindow *buttonOk = winGetWindowFromId(parent, buttonOkID);
	buttonOk->winGetPosition(&buttonX[0], &buttonY[0]);

	tempName = menuName;
	tempName.concat("ButtonYes");
	NameKeyType buttonYesID = TheNameKeyGenerator->nameToKey( tempName );
	GameWindow *buttonYes = winGetWindowFromId(parent, buttonYesID);
	//buttonNo in the second position
	tempName = menuName;
	tempName.concat("ButtonNo");
	NameKeyType buttonNoID = TheNameKeyGenerator->nameToKey(tempName);
	GameWindow *buttonNo = winGetWindowFromId(parent, buttonNoID);
	buttonNo->winGetPosition(&buttonX[1], &buttonY[1]);

	//and buttonCancel in the third
	tempName = menuName;
	tempName.concat("ButtonCancel");
	NameKeyType buttonCancelID = TheNameKeyGenerator->nameToKey( tempName );
	GameWindow *buttonCancel = winGetWindowFromId(parent, buttonCancelID);
	buttonCancel->winGetPosition(&buttonX[2], &buttonY[2]);

	//we shouldn't have button OK and Yes on the same dialog
	if((buttonFlags & (MSG_BOX_OK | MSG_BOX_YES)) == (MSG_BOX_OK | MSG_BOX_YES) )
	{
		DEBUG_CRASH(("Passed in MSG_BOX_OK and MSG_BOX_YES.  Big No No."));
	}

	//Position the OK button if we have one
	if( (buttonFlags & MSG_BOX_OK) == MSG_BOX_OK)
	{
		buttonOk->winSetPosition(buttonX[0], buttonY[0]);
		buttonOk->winHide(FALSE);
	}
	else if( (buttonFlags & MSG_BOX_YES) == MSG_BOX_YES)
	{
		//Position the Yes if we have one
		buttonYes->winSetPosition(buttonX[0], buttonY[0]);
		buttonYes->winHide(FALSE);
	}

	if((buttonFlags & (MSG_BOX_NO | MSG_BOX_CANCEL)) == (MSG_BOX_NO | MSG_BOX_CANCEL) )
	{
		//If we have both the No and Cancel button, then the no should go in the middle position
		buttonNo->winSetPosition(buttonX[1], buttonY[1]);
		buttonCancel->winSetPosition(buttonX[2], buttonY[2]);
		buttonNo->winHide(FALSE);
		buttonCancel->winHide(FALSE);
	}
	else if( (buttonFlags & MSG_BOX_NO) == MSG_BOX_NO)
	{
		//if we just have the no button, then position it in the right most spot
		buttonNo->winSetPosition(buttonX[2], buttonY[2]);
		buttonNo->winHide(FALSE);
	}
	else if( (buttonFlags & MSG_BOX_CANCEL) == MSG_BOX_CANCEL)
	{
		//else if we just have the Cancel button, well, it should always go in the right spot
		buttonCancel->winSetPosition(buttonX[2], buttonY[2]);
		buttonCancel->winHide(FALSE);
	}

	// Fill the text into the text boxes
	tempName = menuName;
	tempName.concat("StaticTextTitle");
	NameKeyType staticTextTitleID = TheNameKeyGenerator->nameToKey( tempName );
	GameWindow *staticTextTitle = winGetWindowFromId(parent, staticTextTitleID);
	GadgetStaticTextSetText(staticTextTitle,titleString);
	tempName = menuName;
	tempName.concat("StaticTextMessage");
	NameKeyType staticTextMessageID = TheNameKeyGenerator->nameToKey( tempName );
	GameWindow *staticTextMessage = winGetWindowFromId(parent, staticTextMessageID);
	GadgetStaticTextSetText(staticTextMessage,bodyString);

	// create a structure that will pass the functions to
	WindowMessageBoxData *MsgBoxCallbacks = NEW WindowMessageBoxData;
	MsgBoxCallbacks->cancelCallback = cancelCallback;
	MsgBoxCallbacks->noCallback = noCallback;
	MsgBoxCallbacks->okCallback = okCallback;
	MsgBoxCallbacks->yesCallback = yesCallback;
	//pass the structure to the dialog
	trueParent->winSetUserData( MsgBoxCallbacks );

	//make sure the dialog is showing and bring it to the top
	parent->winHide(FALSE);
	parent->winBringToTop();

	//Changed By Chris
	return trueParent;
}

//-------------------------------------------------------------------------------------------------
/** Create a button GUI control */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetPushButton( GameWindow *parent,
																										 UnsignedInt status,
																										 Int x, Int y,
																										 Int width, Int height,
																										 WinInstanceData *instData,
																										 GameFont *defaultFont,
																										 Bool defaultVisual )
{
	GameWindow *button;

	// we MUST have a push button style window to do this
	if( BitIsSet( instData->getStyle(), GWS_PUSH_BUTTON ) == FALSE )
	{

		DEBUG_LOG(( "Can't create button gadget, instance data not button type" ));
		assert( 0 );
		return nullptr;

	}

	// create the button window
	button = winCreate( parent, status,
																				x, y, width, height,
																				GadgetPushButtonSystem,
																				instData );
	if( button == nullptr )
	{

		DEBUG_LOG(( "Unable to create button for push button gadget" ));
		assert( 0 );
		return nullptr;

	}

	// assign input function
	button->winSetInputFunc( GadgetPushButtonInput );

	//
	// assign draw function, the draw functions must actually be implemented
	// on the device level of the engine
	//
	if( BitIsSet( button->winGetStatus(), WIN_STATUS_IMAGE ) )
		button->winSetDrawFunc( getPushButtonImageDrawFunc() );
	else
		button->winSetDrawFunc( getPushButtonDrawFunc() );

	// set the owner to the parent, or if no parent it will be itself
	button->winSetOwner( parent );

	// Init the userdata to null
	button->winSetUserData(nullptr);

	// assign the default images/colors
	assignDefaultGadgetLook( button, defaultFont, defaultVisual );

	// assign text from label
	UnicodeString text = winTextLabelToText( instData->m_textLabelString );
	if( text.getLength() )
		GadgetButtonSetText( button, text );

	return button;

}

//-------------------------------------------------------------------------------------------------
/** Create a checkbox UI element */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetCheckbox( GameWindow *parent,
																									 UnsignedInt status,
																									 Int x, Int y,
																									 Int width, Int height,
																									 WinInstanceData *instData,
																									 GameFont *defaultFont,
																									 Bool defaultVisual )

{
	GameWindow *checkbox;

	// we MUST have a push button style window to do this
	if( BitIsSet( instData->getStyle(), GWS_CHECK_BOX ) == FALSE )
	{

		DEBUG_LOG(( "Can't create checkbox gadget, instance data not checkbox type" ));
		assert( 0 );
		return nullptr;

	}

	// create the button window
	checkbox = winCreate( parent, status,
																					x, y, width, height,
																					GadgetCheckBoxSystem,
																					instData );
	if( checkbox == nullptr )
	{

		DEBUG_LOG(( "Unable to create checkbox window" ));
		assert( 0 );
		return nullptr;

	}

	// assign input function
	checkbox->winSetInputFunc( GadgetCheckBoxInput );

	//
	// assign draw function, the draw functions must actually be implemented
	// on the device level of the engine
	//
	if( BitIsSet( checkbox->winGetStatus(), WIN_STATUS_IMAGE ) )
		checkbox->winSetDrawFunc( getCheckBoxImageDrawFunc() );
	else
		checkbox->winSetDrawFunc( getCheckBoxDrawFunc() );

	// set the owner to the parent, or if no parent it will be itself
	checkbox->winSetOwner( parent );

	// assign the default images/colors
	assignDefaultGadgetLook( checkbox, defaultFont, defaultVisual );

	// assign text from label
	UnicodeString text = winTextLabelToText( instData->m_textLabelString );
	if( text.getLength() )
		GadgetCheckBoxSetText( checkbox, text );

	return checkbox;

}

//-------------------------------------------------------------------------------------------------
/** Create a radio button GUI element */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetRadioButton( GameWindow *parent,
																										  UnsignedInt status,
																										  Int x, Int y,
																										  Int width, Int height,
																										  WinInstanceData *instData,
																											RadioButtonData *rData,
																										  GameFont *defaultFont,
																										  Bool defaultVisual )

{
	GameWindow *radioButton;
	RadioButtonData *radioData;

	// we MUST have a push button style window to do this
	if( BitIsSet( instData->getStyle(), GWS_RADIO_BUTTON ) == FALSE )
	{

		DEBUG_LOG(( "Can't create radioButton gadget, instance data not radioButton type" ));
		assert( 0 );
		return nullptr;

	}

	// create the button window
	radioButton = winCreate( parent, status,
																					   x, y, width, height,
																						 GadgetRadioButtonSystem,
																						 instData );
	if( radioButton == nullptr )
	{

		DEBUG_LOG(( "Unable to create radio button window" ));
		assert( 0 );
		return nullptr;

	}

	// allocate and store the radio button user data
	radioData = NEW RadioButtonData;
	memcpy( radioData, rData, sizeof( RadioButtonData ) );
	radioButton->winSetUserData( radioData );

	// assign input function
	radioButton->winSetInputFunc( GadgetRadioButtonInput );

	//
	// assign draw function, the draw functions must actually be implemented
	// on the device level of the engine
	//
	if( BitIsSet( radioButton->winGetStatus(), WIN_STATUS_IMAGE ) )
		radioButton->winSetDrawFunc( getRadioButtonImageDrawFunc() );
	else
		radioButton->winSetDrawFunc( getRadioButtonDrawFunc() );

	// set the owner to the parent, or if no parent it will be itself
	radioButton->winSetOwner( parent );

	// assign the default images/colors
	assignDefaultGadgetLook( radioButton, defaultFont, defaultVisual );

	// assign text from label
	UnicodeString text = winTextLabelToText( instData->m_textLabelString );
	if( text.getLength() )
		GadgetRadioSetText( radioButton, text );

	return radioButton;

}

//-------------------------------------------------------------------------------------------------
/** Create a tab control GUI element */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetTabControl( GameWindow *parent,
																										  UnsignedInt status,
																										  Int x, Int y,
																										  Int width, Int height,
																										  WinInstanceData *instData,
																											TabControlData *tData,
																										  GameFont *defaultFont,
																										  Bool defaultVisual )

{
	GameWindow *tabControl;
	TabControlData *tabData;

	// we MUST have a tab control style window to do this
	if( BitIsSet( instData->getStyle(), GWS_TAB_CONTROL ) == FALSE )
	{

		DEBUG_LOG(( "Can't create tabControl gadget, instance data not tabControl type" ));
		assert( 0 );
		return nullptr;

	}

	// create the tab control window
	tabControl = winCreate( parent, status,
																					   x, y, width, height,
																						 GadgetTabControlSystem,
																						 instData );
	if( tabControl == nullptr )
	{

		DEBUG_LOG(( "Unable to create tab control window" ));
		assert( 0 );
		return nullptr;

	}

	// allocate and store the tab control user data
	tabData = NEW TabControlData;
	memcpy( tabData, tData, sizeof( TabControlData ) );
	tabControl->winSetUserData( tabData );

	GadgetTabControlComputeTabRegion( tabControl );
	GadgetTabControlCreateSubPanes( tabControl );
	GadgetTabControlShowSubPane( tabControl, 0 );

	// assign input function
	tabControl->winSetInputFunc( GadgetTabControlInput );

	//
	// assign draw function, the draw functions must actually be implemented
	// on the device level of the engine
	//
	if( BitIsSet( tabControl->winGetStatus(), WIN_STATUS_IMAGE ) )
		tabControl->winSetDrawFunc( getTabControlImageDrawFunc() );
	else
		tabControl->winSetDrawFunc( getTabControlDrawFunc() );

	// set the owner to the parent, or if no parent it will be itself
	tabControl->winSetOwner( parent );

	// assign the default images/colors
	assignDefaultGadgetLook( tabControl, defaultFont, defaultVisual );

	return tabControl;

}

//-------------------------------------------------------------------------------------------------
/** Create a list box GUI control */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetListBox( GameWindow *parent,
																									UnsignedInt status,
										                              Int x, Int y,
																									Int width, Int height,
																									WinInstanceData *instData,
																									ListboxData *listboxDataTemplate,
																								  GameFont *defaultFont,
																								  Bool defaultVisual )

{
  GameWindow *listbox;
  ListboxData *listboxData;
	Bool title = FALSE;

	// we MUST have a push button style window to do this
	if( BitIsSet( instData->getStyle(), GWS_SCROLL_LISTBOX ) == FALSE )
	{

		DEBUG_LOG(( "Can't create listbox gadget, instance data not listbox type" ));
		assert( 0 );
		return nullptr;

	}

	// create the listbox
  listbox = winCreate( parent, status, x, y, width, height,
											 GadgetListBoxSystem, instData );

	if( listbox == nullptr )
	{

		DEBUG_LOG(( "Unable to create listbox window" ));
		assert( 0 );
		return nullptr;

	}

	// allocate the listbox data, copy template data over and set it into box
	listboxData = NEW ListboxData;
	DEBUG_ASSERTCRASH(listboxDataTemplate, ("listboxDataTemplate not initialized"));
	memcpy( listboxData, listboxDataTemplate, sizeof( ListboxData ) );

  // Add the list box data struct to the window's class data
  listbox->winSetUserData( listboxData );

	// set the owner to the parent, or if no parent it will be itself
	listbox->winSetOwner( parent );

	// Adjust for list title if present
	if( instData->getTextLength() )
		title = TRUE;

	// Set up list box redraw callbacks
	if( BitIsSet( listbox->winGetStatus(), WIN_STATUS_IMAGE ))
		listbox->winSetDrawFunc( getListBoxImageDrawFunc() );
	else
		listbox->winSetDrawFunc( getListBoxDrawFunc() );

	// Set up list box input callbacks
	if( listboxData->multiSelect )
		listbox->winSetInputFunc( GadgetListBoxMultiInput );
	else
		listbox->winSetInputFunc( GadgetListBoxInput );



	//
	// allocate and set the list length, note that setting the list length will
	// automatically allocate the selection entries that are needed for multi
	// select list boxes etc.
	//
	Int length = listboxData->listLength;
	listboxData->listLength = 0;  // hacky!
	GadgetListBoxSetListLength( listbox, length );

	// store display height
	listboxData->displayHeight = height;

	if( title )
		listboxData->displayHeight -= winFontHeight( instData->getFont() );

  // Set display position to the top of the list
  listboxData->displayPos = 0;
  listboxData->selectPos = -1;
	listboxData->doubleClickTime = 0;
  listboxData->insertPos = 0;
  listboxData->endPos = 0;
	listboxData->totalHeight = 0;

	// if this listbox has multiple selections prep it
	//if( listboxData->multiSelect )
	//	GadgetListBoxAddMultiSelect( listbox );

	// If ScrollBar was requested ... create it.
	if( listboxData->scrollBar )
		GadgetListboxCreateScrollbar( listbox );
	//
	// Setup listbox Columns
	//
	if( listboxData->columns == 1 )
	{
		listboxData->columnWidth = NEW Int[listboxData->columns];
		listboxData->columnWidth[0] = width;
		if( listboxData->slider )
		{
			ICoord2D sliderSize;

			listboxData->slider->winGetSize( &sliderSize.x, &sliderSize.y );
			listboxData->columnWidth[0] -= (sliderSize.x + 2);

		}
	}
	else
	{
		if( !listboxData->columnWidthPercentage )
			return nullptr;
		listboxData->columnWidth = NEW Int[listboxData->columns];
		if(!listboxData->columnWidth)
			return nullptr;

		Int totalWidth = width;
		if( listboxData->slider )
		{
			ICoord2D sliderSize;

			listboxData->slider->winGetSize( &sliderSize.x, &sliderSize.y );
			totalWidth -= (sliderSize.x + 2);

		}
		for(Int i = 0; i < listboxData->columns; i++ )
		{
			listboxData->columnWidth[i] = listboxData->columnWidthPercentage[i] * totalWidth / 100;
		}
	}
	// assign the default images/colors
	assignDefaultGadgetLook( listbox, defaultFont, defaultVisual );

  return listbox;

}

//-------------------------------------------------------------------------------------------------
/** Does all generic window creation, calls appropriate slider create
	* function to set up slider-specific data */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetSlider( GameWindow *parent,
																								 UnsignedInt status,
																								 Int x, Int y,
																								 Int width, Int height,
																								 WinInstanceData *instData,
																								 SliderData *sliderData,
																								 GameFont *defaultFont,
																								 Bool defaultVisual )

{
	GameWindow *slider;
	GameWindow *button;
	SliderData *data;

	//
	// All sliders need to have the tab stop status in order for
	// the focus chain to work correctly.
	//
	BitSet( status, WIN_STATUS_TAB_STOP );

	if( BitIsSet( instData->getStyle(), GWS_HORZ_SLIDER ) )
	{

		slider = winCreate( parent, status, x, y, width, height,
												GadgetHorizontalSliderSystem, instData );

		// Set up horizontal slider callbacks
		slider->winSetInputFunc( GadgetHorizontalSliderInput );
		if( BitIsSet( slider->winGetStatus(), WIN_STATUS_IMAGE ) )
			slider->winSetDrawFunc( getHorizontalSliderImageDrawFunc() );
		else
			slider->winSetDrawFunc( getHorizontalSliderDrawFunc() );

	}
	else if ( BitIsSet( instData->getStyle(), GWS_VERT_SLIDER ) )
	{

		slider = winCreate( parent, status, x, y, width, height,
												GadgetVerticalSliderSystem, instData );

		// Set up vertical slider callbacks
		slider->winSetInputFunc( GadgetVerticalSliderInput );

		if( BitIsSet( slider->winGetStatus(), WIN_STATUS_IMAGE ) && !(parent && BitIsSet(parent->winGetStyle(), GWS_SCROLL_LISTBOX)))
			slider->winSetDrawFunc( getVerticalSliderImageDrawFunc() );
		else
			slider->winSetDrawFunc( getVerticalSliderDrawFunc() );

	}
	else
	{

		DEBUG_LOG(( "gogoGadgetSlider warning: unrecognized slider style." ));
		assert( 0 );
		return nullptr;

	}

	// sanity
	if( slider == nullptr )
	{

		DEBUG_LOG(( "Unable to create slider control window" ));
		assert( 0 );
		return nullptr;

	}

	// set the owner to the parent, or if no parent it will be itself
	slider->winSetOwner( parent );

	// create the slider thumb button
	WinInstanceData buttonInstData;
	UnsignedInt statusFlags = status | WIN_STATUS_ENABLED | WIN_STATUS_DRAGGABLE;

	buttonInstData.init();

	// if parent is hidden we don't need to be.
	BitClear( statusFlags, WIN_STATUS_HIDDEN );

	buttonInstData.m_owner = slider;
	buttonInstData.m_style = GWS_PUSH_BUTTON;

	// if slider tracks, so will this sub control
	if( BitIsSet( instData->getStyle(), GWS_MOUSE_TRACK ) )
		BitSet( buttonInstData.m_style, GWS_MOUSE_TRACK );

	if( BitIsSet( instData->getStyle(), GWS_HORZ_SLIDER ) )
		button = gogoGadgetPushButton( slider, statusFlags, 0, HORIZONTAL_SLIDER_THUMB_POSITION,
											 						 HORIZONTAL_SLIDER_THUMB_WIDTH, HORIZONTAL_SLIDER_THUMB_HEIGHT, &buttonInstData, nullptr, TRUE );
	else
		button = gogoGadgetPushButton( slider, statusFlags, 0, 0,
																	 width, width+1, &buttonInstData, nullptr, TRUE );

	// Protect against divide by zero
	if( sliderData->maxVal == sliderData->minVal )
		sliderData->maxVal = sliderData->minVal + 1;

	if( BitIsSet( instData->getStyle(), GWS_HORZ_SLIDER ) )
	{
		sliderData->numTicks = (float)(width - HORIZONTAL_SLIDER_THUMB_WIDTH) /
													 (float)(sliderData->maxVal - sliderData->minVal);
	} else
	{
		sliderData->numTicks = (float)(height - GADGET_SIZE) /
													 (float)(sliderData->maxVal - sliderData->minVal);
	}

	data = NEW SliderData;
	memcpy( data, sliderData, sizeof(SliderData) );

	// Add the slider data struct to the window's class data
	slider->winSetUserData( data );

	// assign the default images/colors
	assignDefaultGadgetLook( slider, defaultFont, defaultVisual );

	return slider;

}

//-------------------------------------------------------------------------------------------------
/** Create a Combo Box GUI element */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetComboBox( GameWindow *parent,
																									UnsignedInt status,
										                              Int x, Int y,
																									Int width, Int height,
																									WinInstanceData *instData,
																									ComboBoxData *comboBoxDataTemplate,
																								  GameFont *defaultFont,
																								  Bool defaultVisual )
{
  GameWindow *comboBox;
  ComboBoxData *comboBoxData;
	Bool title = FALSE;

	// we MUST have a push button style window to do this
	if( BitIsSet( instData->getStyle(), GWS_COMBO_BOX) == FALSE )
	{

		DEBUG_LOG(( "Can't create ComboBox gadget, instance data not ComboBox type" ));
		assert( 0 );
		return nullptr;

	}

	// create the listbox
  comboBox = winCreate( parent, status, x, y, width, height,
											 GadgetComboBoxSystem, instData );

	if( comboBox == nullptr )
	{

		DEBUG_LOG(( "Unable to create ComboBox window" ));
		assert( 0 );
		return nullptr;

	}


// begin here
	// allocate the listbox data, copy template data over and set it into box
	comboBoxData = NEW ComboBoxData;
	memcpy( comboBoxData, comboBoxDataTemplate, sizeof( ComboBoxData ) );

  // Add the list box data struct to the window's class data
  comboBox->winSetUserData( comboBoxData );

	// set the owner to the parent, or if no parent it will be itself
	comboBox->winSetOwner( parent );

	// Adjust for list title if present
	if( instData->getTextLength() )
		title = TRUE;

	// Set up list box redraw callbacks
	if( BitIsSet( comboBox->winGetStatus(), WIN_STATUS_IMAGE ))
		comboBox->winSetDrawFunc( getComboBoxImageDrawFunc() );
	else
		comboBox->winSetDrawFunc( getComboBoxDrawFunc() );

	// Set up list box input callbacks
	comboBox->winSetInputFunc( GadgetComboBoxInput );

	//Create the windows that make up


	WinInstanceData winInstData;
	Int buttonWidth, buttonHeight;
	Int fontHeight;
	Int top;
	Int bottom;


	// do we have a title
	if( comboBox->winGetTextLength() )
		title = TRUE;

	// remove unwanted status bits.
	status &= ~(WIN_STATUS_BORDER | WIN_STATUS_HIDDEN);

	fontHeight = winFontHeight( comboBox->winGetFont() );
	top = title ? (fontHeight + 1):0;
	bottom = title ? (height - (fontHeight + 1)):height;

	// initialize instData
	winInstData.init();

	// size of button
	buttonWidth = 21;
	buttonHeight = 22;

	// ----------------------------------------------------------------------
	// Create Drop Down Button
	// ----------------------------------------------------------------------

	winInstData.m_owner = comboBox;
	winInstData.m_style = GWS_PUSH_BUTTON;

	// if listbox tracks, so will this sub control
	if( BitIsSet( comboBox->winGetStyle(), GWS_MOUSE_TRACK ) )
		BitSet( winInstData.m_style, GWS_MOUSE_TRACK );

	comboBoxData->dropDownButton =
		 gogoGadgetPushButton( comboBox,
																						 status | WIN_STATUS_ACTIVE | WIN_STATUS_ENABLED,
																						 width - buttonWidth, 0,
																						 buttonWidth, height,
																						 &winInstData, nullptr, TRUE );
	comboBoxData->dropDownButton->winSetTooltipFunc(comboBox->winGetTooltipFunc());
	comboBoxData->dropDownButton->winSetTooltip(instData->getTooltipText());
	comboBoxData->dropDownButton->setTooltipDelay(comboBox->getTooltipDelay());
	// ----------------------------------------------------------------------
	// Create text entry
	// ----------------------------------------------------------------------
	UnsignedInt statusTextEntry;
	winInstData.init();

	winInstData.m_owner = comboBox;
  winInstData.m_style |= GWS_ENTRY_FIELD;
	winInstData.m_textLabelString = "Entry";
	if( BitIsSet( comboBox->winGetStyle(), GWS_MOUSE_TRACK ) )
		BitSet( winInstData.m_style, GWS_MOUSE_TRACK );
	if( comboBoxData->isEditable)
	{
		statusTextEntry = status;
	}
	else
	{
		statusTextEntry = status | WIN_STATUS_NO_INPUT ;//| WIN_STATUS_NO_FOCUS;
		comboBoxData->entryData->drawTextFromStart = TRUE;
	}
  comboBoxData->editBox = gogoGadgetTextEntry( comboBox, statusTextEntry ,
																										 0,0 ,
																										width - buttonWidth , height ,
																										&winInstData, comboBoxData->entryData,
																										winInstData.m_font, FALSE );
	comboBoxData->editBox->winSetTooltipFunc(comboBox->winGetTooltipFunc());
	comboBoxData->editBox->winSetTooltip(instData->getTooltipText());
	comboBoxData->editBox->setTooltipDelay(comboBox->getTooltipDelay());

	delete (comboBoxData->entryData);
	comboBoxData->entryData = (EntryData *)comboBoxData->editBox->winGetUserData();
	// ----------------------------------------------------------------------
	// Create list box
	// ----------------------------------------------------------------------
	winInstData.init();

	winInstData.m_owner = comboBox;
  if( BitIsSet( comboBox->winGetStyle(), GWS_MOUSE_TRACK ) )
		BitSet( winInstData.m_style, GWS_MOUSE_TRACK );
	BitSet( winInstData.m_style, WIN_STATUS_HIDDEN );
  winInstData.m_style |= GWS_SCROLL_LISTBOX;
	status &= ~(WIN_STATUS_IMAGE);
  comboBoxData->listBox = gogoGadgetListBox( comboBox, status | WIN_STATUS_ABOVE | WIN_STATUS_ONE_LINE, 0, height,
																								width, height,
																								&winInstData, comboBoxData->listboxData,
																								winInstData.m_font, FALSE );
	comboBoxData->listBox->winHide(TRUE);
	delete(comboBoxData->listboxData);
	comboBoxData->listboxData = (ListboxData *)comboBoxData->listBox->winGetUserData();

	comboBoxData->listBox->winSetTooltipFunc(comboBox->winGetTooltipFunc());
	comboBoxData->listBox->winSetTooltip(instData->getTooltipText());
	comboBoxData->listBox->setTooltipDelay(comboBox->getTooltipDelay());
	GadgetListBoxSetAudioFeedback(comboBoxData->listBox, TRUE);

	// Initialize the ComboBox's variables and the controls with them
	GadgetComboBoxSetIsEditable(comboBox, comboBoxData->isEditable);
	GadgetComboBoxSetMaxChars( comboBox, comboBoxData->maxChars );
	GadgetComboBoxSetMaxDisplay( comboBox, comboBoxData->maxDisplay );

	//Initialize the control's text colors
	Color color, border;

	color = comboBox->winGetEnabledTextColor();
	border = comboBox->winGetEnabledTextBorderColor();
	if(comboBoxData->listBox)
		comboBoxData->listBox->winSetEnabledTextColors( color,border);
	if(comboBoxData->editBox)
		comboBoxData->editBox->winSetEnabledTextColors(color,border);

	color = comboBox->winGetDisabledTextColor();
	border = comboBox->winGetDisabledTextBorderColor();
	if(comboBoxData->listBox)
		comboBoxData->listBox->winSetDisabledTextColors( color,border);
	if(comboBoxData->editBox)
		comboBoxData->editBox->winSetDisabledTextColors(color,border);


	color = comboBox->winGetHiliteTextColor();
	border = comboBox->winGetHiliteTextBorderColor();
	if(comboBoxData->listBox)
		comboBoxData->listBox->winSetHiliteTextColors( color,border);
	if(comboBoxData->editBox)
		comboBoxData->editBox->winSetHiliteTextColors(color,border);

	comboBoxData->dontHide = FALSE;

	// assign the default images/colors
	assignDefaultGadgetLook( comboBox, defaultFont, defaultVisual );

  return comboBox;

}

//-------------------------------------------------------------------------------------------------
/** Create a progress bar GUI element */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetProgressBar( GameWindow *parent,
																										  UnsignedInt status,
																										  Int x, Int y,
																										  Int width, Int height,
																										  WinInstanceData *instData,
																										  GameFont *defaultFont,
																										  Bool defaultVisual )

{
	GameWindow *progressBar;

	// we MUST have a push button style window to do this
	if( BitIsSet( instData->getStyle(), GWS_PROGRESS_BAR ) == FALSE )
	{

		DEBUG_LOG(( "Can't create progressBar gadget, instance data not progressBar type" ));
		assert( 0 );
		return nullptr;

	}

	// create the button window
	progressBar = winCreate( parent, status,
																					   x, y, width, height,
																						 GadgetProgressBarSystem,
																						 instData );
	if( progressBar == nullptr )
	{

		DEBUG_LOG(( "Unable to create progress bar control" ));
		assert( 0 );
		return nullptr;

	}

	//
	// assign draw function, the draw functions must actually be implemented
	// on the device level of the engine
	//
	if( BitIsSet( progressBar->winGetStatus(), WIN_STATUS_IMAGE ) )
		progressBar->winSetDrawFunc( getProgressBarImageDrawFunc() );
	else
		progressBar->winSetDrawFunc( getProgressBarDrawFunc() );

	// set the owner to the parent, or if no parent it will be itself
	progressBar->winSetOwner( parent );

	// assign the default images/colors
	assignDefaultGadgetLook( progressBar, defaultFont, defaultVisual );

	return progressBar;

}

//-------------------------------------------------------------------------------------------------
/** Does all generic window creation, calls appropriate text field create
	* function to set up specific data */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetStaticText( GameWindow *parent,
																										 UnsignedInt status,
																										 Int x, Int y,
																										 Int width, Int height,
																										 WinInstanceData *instData,
																										 TextData *textData,
																										 GameFont *defaultFont,
																										 Bool defaultVisual )

{
  GameWindow *textWin;
  TextData *data;

	// Static Text can not be a Tab Stop
	BitClear( instData->m_style, GWS_TAB_STOP );

  if( BitIsSet( instData->getStyle(), GWS_STATIC_TEXT ) )
	{
    textWin = winCreate( parent, status, x, y, width, height,
												 GadgetStaticTextSystem, instData );
  }
	else
	{
    DEBUG_LOG(( "gogoGadgetText warning: unrecognized text style." ));
    return nullptr;
  }

  if( textWin != nullptr )
	{

		// set the owner to the parent, or if no parent it will be itself
		textWin->winSetOwner( parent );

		// assign callbacks
		textWin->winSetInputFunc( GadgetStaticTextInput );
		if( BitIsSet( textWin->winGetStatus(), WIN_STATUS_IMAGE ) )
			textWin->winSetDrawFunc( getStaticTextImageDrawFunc() );
		else
			textWin->winSetDrawFunc( getStaticTextDrawFunc() );

    data = NEW TextData;
		assert( textData != nullptr );
    memcpy( data, textData, sizeof(TextData) );

		// allocate a display string for the tet
		data->text = TheDisplayStringManager->newDisplayString();
		// set whether or not we center the wrapped text
		data->text->setWordWrapCentered( BitIsSet( instData->getStatus(), WIN_STATUS_WRAP_CENTERED ));
    // Add the entry field data struct to the window's class data
    textWin->winSetUserData( data );

		// assign the default images/colors
		assignDefaultGadgetLook( textWin, defaultFont, defaultVisual );

		// assign text from label
		UnicodeString text = winTextLabelToText( instData->m_textLabelString );
		if( text.getLength() )
			GadgetStaticTextSetText( textWin, text );

  }

  return textWin;

}

//-------------------------------------------------------------------------------------------------
/** Does all generic window creation, calls appropriate entry field create
	* function to set up specific data */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::gogoGadgetTextEntry( GameWindow *parent,
																										UnsignedInt status,
																										Int x, Int y,
																										Int width, Int height,
																										WinInstanceData *instData,
																										EntryData *entryData,
																										GameFont *defaultFont,
																										Bool defaultVisual )

{
	GameWindow *entry;
	EntryData *data;

	if( BitIsSet( instData->getStyle(), GWS_ENTRY_FIELD ) == FALSE )
	{

		DEBUG_LOG(( "Unable to create text entry, style not entry type" ));
		assert( 0 );
		return nullptr;

	}

	// create the window
	entry = winCreate( parent, status, x, y, width, height,
										 GadgetTextEntrySystem, instData );
	if( entry == nullptr )
	{

		DEBUG_LOG(( "Unable to create text entry window" ));
		assert( 0 );
		return nullptr;

	}

	// set owner of this control
	entry->winSetOwner( parent );

	// assign callbacks
	entry->winSetInputFunc( GadgetTextEntryInput );
	if( BitIsSet( entry->winGetStatus(), WIN_STATUS_IMAGE ) )
		entry->winSetDrawFunc( getTextEntryImageDrawFunc() );
	else
		entry->winSetDrawFunc( getTextEntryDrawFunc() );

	// zero entry data
//	memset( entryData->text, 0, ENTRY_TEXT_LEN );
//	memset( entryData->constructText, 0, ENTRY_TEXT_LEN );

	// initialize character positions, lengths etc
	if( entryData->text )
		entryData->charPos = entryData->text->getTextLength();
	else
		entryData->charPos = 0;
	entryData->conCharPos = 0;
	entryData->receivedUnichar = FALSE;
	if( entryData->maxTextLen > ENTRY_TEXT_LEN )
		entryData->maxTextLen = ENTRY_TEXT_LEN;

	// allocate entry data
	data = NEW EntryData;

	// copy over data control
	memcpy( data, entryData, sizeof(EntryData) );

	// allocate new text display string
	data->text = TheDisplayStringManager->newDisplayString();
	data->sText = TheDisplayStringManager->newDisplayString();
	data->constructText = TheDisplayStringManager->newDisplayString();

	// set the max for the text lengths
//	data->text->allocateFixed( ENTRY_TEXT_LEN );
//	data->sText->allocateFixed( ENTRY_TEXT_LEN );

	// do any real display string copies
	if( entryData->text )
		data->text->setText( entryData->text->getText() );
	if( entryData->sText )
		data->sText->setText( entryData->sText->getText() );

	// set data into window
	entry->winSetUserData( data );

	// asian languages get to have list box kanji character completion
	data->constructList = nullptr;
	if( OurLanguage == LANGUAGE_ID_KOREAN ||
			OurLanguage == LANGUAGE_ID_JAPANESE )
	{
		// we need to create the construct listbox
		WinInstanceData boxInstData;
		ListboxData lData;

			// initialize instData
		boxInstData.init();

		// define display region
		memset( &lData, 0, sizeof(ListboxData) );
		lData.listLength = 128;
		lData.autoScroll = FALSE;
		lData.scrollIfAtEnd = FALSE;
		lData.autoPurge = TRUE;
		lData.scrollBar = TRUE;
		lData.multiSelect = FALSE;
		lData.columns = 1;
		lData.columnWidth = nullptr;

		boxInstData.m_style = GWS_SCROLL_LISTBOX | GWS_MOUSE_TRACK;

		data->constructList = gogoGadgetListBox( nullptr,
																						 WIN_STATUS_ABOVE |
																						 WIN_STATUS_HIDDEN |
																						 WIN_STATUS_NO_FOCUS |
																						 WIN_STATUS_ONE_LINE,
																						 0, height,
																						 110, 119,
																						 &boxInstData,
																						 &lData,
																						 nullptr,
																						 TRUE );

		if( data->constructList == nullptr )
		{

			DEBUG_LOG(( "gogoGadgetEntry warning: Failed to create listbox." ));
			assert( 0 );
			winDestroy( entry );
			return nullptr;

		}

	}

	// assign the default images/colors
	assignDefaultGadgetLook( entry, defaultFont, defaultVisual );

	// assign text from label
	UnicodeString text = winTextLabelToText( instData->m_textLabelString );
	if( text.getLength() )
		GadgetTextEntrySetText( entry, text );

	return entry;

}

//-------------------------------------------------------------------------------------------------
/** Use this method to assign the default images/colors to gadgets as
	* they area created */
//-------------------------------------------------------------------------------------------------
void GameWindowManager::assignDefaultGadgetLook( GameWindow *gadget,
																								 GameFont *defaultFont,
																								 Bool assignVisual )
{
	UnsignedByte alpha = 255;
	static Color red				= winMakeColor( 255,   0,   0, alpha );
	static Color darkRed		= winMakeColor( 128,   0,   0, alpha );
	static Color lightRed		= winMakeColor( 255, 128, 128, alpha );
	static Color green			= winMakeColor(   0, 255,   0, alpha );
	static Color darkGreen	= winMakeColor(   0, 128,   0, alpha );
	static Color lightGreen	= winMakeColor( 128, 255, 128, alpha );
	static Color blue				= winMakeColor(   0,   0, 255, alpha );
	static Color darkBlue		= winMakeColor(   0,   0, 128, alpha );
	static Color lightBlue	= winMakeColor( 128, 128, 255, alpha );
	static Color purple			= winMakeColor( 255,   0, 255, alpha );
	static Color darkPurple	= winMakeColor( 128,   0, 128, alpha );
	static Color lightPurple= winMakeColor( 255, 128, 255, alpha );
	static Color yellow			= winMakeColor( 255, 255,   0, alpha );
	static Color darkYellow	= winMakeColor( 128, 128,   0, alpha );
	static Color lightYellow= winMakeColor( 255, 255, 128, alpha );
	static Color cyan				= winMakeColor(   0, 255, 255, alpha );
	static Color darkCyan		= winMakeColor(  64, 128, 128, alpha );
	static Color lightCyan	= winMakeColor( 128, 255, 255, alpha );
	static Color gray				= winMakeColor( 128, 128, 128, alpha );
	static Color darkGray		= winMakeColor(  64,  64,  64, alpha );
	static Color lightGray	= winMakeColor( 192, 192, 192, alpha );
	static Color black			= winMakeColor(   0,   0,   0, alpha );
	static Color white			= winMakeColor( 254, 254, 254, alpha );
	static Color enabledText					= white;
	static Color enabledTextBorder		= darkGray;
	static Color disabledText					= darkGray;
	static Color disabledTextBorder		= black;
	static Color hiliteText						= lightBlue;
	static Color hiliteTextBorder			= blue;
	static Color imeCompositeText				= green;
	static Color imeCompositeTextBorder	= blue;
	WinInstanceData *instData;

	// sanity
	if( gadget == nullptr )
		return;

	// get instance data
	instData = gadget->winGetInstanceData();

	// set default font
	if( defaultFont )
		gadget->winSetFont( defaultFont );
	else
	{
		if (TheGlobalLanguageData && TheGlobalLanguageData->m_defaultWindowFont.name.isNotEmpty())
		{
			gadget->winSetFont( winFindFont(
				TheGlobalLanguageData->m_defaultWindowFont.name,
				TheGlobalLanguageData->m_defaultWindowFont.size,
				TheGlobalLanguageData->m_defaultWindowFont.bold) );
		}
		else
		{
			gadget->winSetFont( winFindFont( "Times New Roman", 14, FALSE ) );
		}
	}

	// if we don't want to assign default colors/images get out of here
	if( assignVisual == FALSE )
		return;

	// create images for the correct gadget type
	if( BitIsSet( instData->getStyle(), GWS_PUSH_BUTTON ) )
	{

		// enabled background
		GadgetButtonSetEnabledImage( gadget, winFindImage( "PushButtonEnabled" ) );
		GadgetButtonSetEnabledColor( gadget, red );
		GadgetButtonSetEnabledBorderColor( gadget, lightRed );
		// enabled selected button
		GadgetButtonSetEnabledSelectedImage( gadget, winFindImage( "PushButtonEnabledSelected" ) );
		GadgetButtonSetEnabledSelectedColor( gadget, yellow );
		GadgetButtonSetEnabledSelectedBorderColor( gadget, white );

		// Disabled background
		GadgetButtonSetDisabledImage( gadget, winFindImage( "PushButtonDisabled" ) );
		GadgetButtonSetDisabledColor( gadget, gray );
		GadgetButtonSetDisabledBorderColor( gadget, lightGray );
		// Disabled selected button
		GadgetButtonSetDisabledSelectedImage( gadget, winFindImage( "PushButtonDisabledSelected" ) );
		GadgetButtonSetDisabledSelectedColor( gadget, lightGray );
		GadgetButtonSetDisabledSelectedBorderColor( gadget, gray );

		// Hilite background
		GadgetButtonSetHiliteImage( gadget, winFindImage( "PushButtonHilite" ) );
		GadgetButtonSetHiliteColor( gadget, green );
		GadgetButtonSetHiliteBorderColor( gadget, darkGreen );
		// Hilite selected button
		GadgetButtonSetHiliteSelectedImage( gadget, winFindImage( "PushButtonHiliteSelected" ) );
		GadgetButtonSetHiliteSelectedColor( gadget, yellow );
		GadgetButtonSetHiliteSelectedBorderColor( gadget, white );

		// set default text colors for the gadget
		gadget->winSetEnabledTextColors( enabledText, enabledTextBorder );
		gadget->winSetDisabledTextColors( disabledText, disabledTextBorder );
		gadget->winSetHiliteTextColors( hiliteText, hiliteTextBorder );
		gadget->winSetIMECompositeTextColors( imeCompositeText, imeCompositeTextBorder );

	}
	else if( BitIsSet( instData->getStyle(), GWS_CHECK_BOX ) )
	{

		// enabled background
		GadgetCheckBoxSetEnabledImage( gadget, winFindImage( "CheckBoxEnabled" ) );
		GadgetCheckBoxSetEnabledColor( gadget, red );
		GadgetCheckBoxSetEnabledBorderColor( gadget, lightRed );
		// enabled CheckBox unselected
		GadgetCheckBoxSetEnabledUncheckedBoxImage( gadget, winFindImage( "CheckBoxEnabledBoxUnselected" ) );
		GadgetCheckBoxSetEnabledUncheckedBoxColor( gadget, WIN_COLOR_UNDEFINED );
		GadgetCheckBoxSetEnabledUncheckedBoxBorderColor( gadget, lightBlue );
		// enabled CheckBox selected
		GadgetCheckBoxSetEnabledCheckedBoxImage( gadget, winFindImage( "CheckBoxEnabledBoxSelected" ) );
		GadgetCheckBoxSetEnabledCheckedBoxColor( gadget, blue );
		GadgetCheckBoxSetEnabledCheckedBoxBorderColor( gadget, lightBlue );

		// disabled background
		GadgetCheckBoxSetDisabledImage( gadget, winFindImage( "CheckBoxDisabled" ) );
		GadgetCheckBoxSetDisabledColor( gadget, gray );
		GadgetCheckBoxSetDisabledBorderColor( gadget, lightGray );
		// Disabled CheckBox unselected
		GadgetCheckBoxSetDisabledUncheckedBoxImage( gadget, winFindImage( "CheckBoxDisabledBoxUnselected" ) );
		GadgetCheckBoxSetDisabledUncheckedBoxColor( gadget, WIN_COLOR_UNDEFINED );
		GadgetCheckBoxSetDisabledUncheckedBoxBorderColor( gadget, lightGray );
		// Disabled CheckBox selected
		GadgetCheckBoxSetDisabledCheckedBoxImage( gadget, winFindImage( "CheckBoxDisabledBoxSelected" ) );
		GadgetCheckBoxSetDisabledCheckedBoxColor( gadget, darkGray );
		GadgetCheckBoxSetDisabledCheckedBoxBorderColor( gadget, white );

		// Hilite background
		GadgetCheckBoxSetHiliteImage( gadget, winFindImage( "CheckBoxHilite" ) );
		GadgetCheckBoxSetHiliteColor( gadget, green );
		GadgetCheckBoxSetHiliteBorderColor( gadget, lightGreen );
		// Hilite CheckBox unselected
		GadgetCheckBoxSetHiliteUncheckedBoxImage( gadget, winFindImage( "CheckBoxHiliteBoxUnselected" ) );
		GadgetCheckBoxSetHiliteUncheckedBoxColor( gadget, WIN_COLOR_UNDEFINED );
		GadgetCheckBoxSetHiliteUncheckedBoxBorderColor( gadget, lightBlue );
		// Hilite CheckBox selected
		GadgetCheckBoxSetHiliteCheckedBoxImage( gadget, winFindImage( "CheckBoxHiliteBoxSelected" ) );
		GadgetCheckBoxSetHiliteCheckedBoxColor( gadget, yellow );
		GadgetCheckBoxSetHiliteCheckedBoxBorderColor( gadget, white );

		// set default text colors for the gadget
		gadget->winSetEnabledTextColors( enabledText, enabledTextBorder );
		gadget->winSetDisabledTextColors( disabledText, disabledTextBorder );
		gadget->winSetHiliteTextColors( hiliteText, hiliteTextBorder );
		gadget->winSetIMECompositeTextColors( imeCompositeText, imeCompositeTextBorder );

	}
	else if( BitIsSet( instData->getStyle(), GWS_RADIO_BUTTON ) )
	{

		// enabled background
		GadgetRadioSetEnabledImage( gadget, winFindImage( "RadioButtonEnabled" ) );
		GadgetRadioSetEnabledColor( gadget, red );
		GadgetRadioSetEnabledBorderColor( gadget, lightRed );
		// enabled radio unselected
		GadgetRadioSetEnabledUncheckedBoxImage( gadget, winFindImage( "RadioButtonEnabledBoxUnselected" ) );
		GadgetRadioSetEnabledUncheckedBoxColor( gadget, darkRed );
		GadgetRadioSetEnabledUncheckedBoxBorderColor( gadget, black );
		// enabled radio selected
		GadgetRadioSetEnabledCheckedBoxImage( gadget, winFindImage( "RadioButtonEnabledBoxSelected" ) );
		GadgetRadioSetEnabledCheckedBoxColor( gadget, blue );
		GadgetRadioSetEnabledCheckedBoxBorderColor( gadget, lightBlue );

		// disabled background
		GadgetRadioSetDisabledImage( gadget, winFindImage( "RadioButtonDisabled" ) );
		GadgetRadioSetDisabledColor( gadget, gray );
		GadgetRadioSetDisabledBorderColor( gadget, lightGray );
		// Disabled radio unselected
		GadgetRadioSetDisabledUncheckedBoxImage( gadget, winFindImage( "RadioButtonDisabledBoxUnselected" ) );
		GadgetRadioSetDisabledUncheckedBoxColor( gadget, gray );
		GadgetRadioSetDisabledUncheckedBoxBorderColor( gadget, lightGray );
		// Disabled radio selected
		GadgetRadioSetDisabledCheckedBoxImage( gadget, winFindImage( "RadioButtonDisabledBoxSelected" ) );
		GadgetRadioSetDisabledCheckedBoxColor( gadget, darkGray );
		GadgetRadioSetDisabledCheckedBoxBorderColor( gadget, white );

		// Hilite background
		GadgetRadioSetHiliteImage( gadget, winFindImage( "RadioButtonHilite" ) );
		GadgetRadioSetHiliteColor( gadget, green );
		GadgetRadioSetHiliteBorderColor( gadget, lightGreen );
		// Hilite radio unselected
		GadgetRadioSetHiliteUncheckedBoxImage( gadget, winFindImage( "RadioButtonHiliteBoxUnselected" ) );
		GadgetRadioSetHiliteUncheckedBoxColor( gadget, darkGreen );
		GadgetRadioSetHiliteUncheckedBoxBorderColor( gadget, lightGreen );
		// Hilite radio selected
		GadgetRadioSetHiliteCheckedBoxImage( gadget, winFindImage( "RadioButtonHiliteBoxSelected" ) );
		GadgetRadioSetHiliteCheckedBoxColor( gadget, yellow );
		GadgetRadioSetHiliteCheckedBoxBorderColor( gadget, white );

		// set default text colors for the gadget
		gadget->winSetEnabledTextColors( enabledText, enabledTextBorder );
		gadget->winSetDisabledTextColors( disabledText, disabledTextBorder );
		gadget->winSetHiliteTextColors( hiliteText, hiliteTextBorder );
		gadget->winSetIMECompositeTextColors( imeCompositeText, imeCompositeTextBorder );

	}
	else if( BitIsSet( instData->getStyle(), GWS_HORZ_SLIDER ) )
	{

		// enabled
		GadgetSliderSetEnabledImageLeft( gadget, winFindImage( "HSliderEnabledLeftEnd" ) );
		GadgetSliderSetEnabledImageRight( gadget, winFindImage( "HSliderEnabledRightEnd" ) );
		GadgetSliderSetEnabledImageCenter( gadget, winFindImage( "HSliderEnabledRepeatingCenter" ) );
		GadgetSliderSetEnabledImageSmallCenter( gadget, winFindImage( "HSliderEnabledSmallRepeatingCenter" ) );
		GadgetSliderSetEnabledColor( gadget, red );
		GadgetSliderSetEnabledBorderColor( gadget, lightRed );

		// disabled
		GadgetSliderSetDisabledImageLeft( gadget, winFindImage( "HSliderDisabledLeftEnd" ) );
		GadgetSliderSetDisabledImageRight( gadget, winFindImage( "HSliderDisabledRightEnd" ) );
		GadgetSliderSetDisabledImageCenter( gadget, winFindImage( "HSliderDisabledRepeatingCenter" ) );
		GadgetSliderSetDisabledImageSmallCenter( gadget, winFindImage( "HSliderDisabledSmallRepeatingCenter" ) );
		GadgetSliderSetDisabledColor( gadget, red );
		GadgetSliderSetDisabledBorderColor( gadget, lightRed );

		// hilite
		GadgetSliderSetHiliteImageLeft( gadget, winFindImage( "HSliderHiliteLeftEnd" ) );
		GadgetSliderSetHiliteImageRight( gadget, winFindImage( "HSliderHiliteRightEnd" ) );
		GadgetSliderSetHiliteImageCenter( gadget, winFindImage( "HSliderHiliteRepeatingCenter" ) );
		GadgetSliderSetHiliteImageSmallCenter( gadget, winFindImage( "HSliderHiliteSmallRepeatingCenter" ) );
		GadgetSliderSetHiliteColor( gadget, red );
		GadgetSliderSetHiliteBorderColor( gadget, lightRed );

		// set default text colors for the gadget
		gadget->winSetEnabledTextColors( enabledText, enabledTextBorder );
		gadget->winSetDisabledTextColors( disabledText, disabledTextBorder );
		gadget->winSetHiliteTextColors( hiliteText, hiliteTextBorder );
		gadget->winSetIMECompositeTextColors( imeCompositeText, imeCompositeTextBorder );

		//
		// set the default colors and images for the slider thumb
		//
		// enabled
		GadgetSliderSetEnabledThumbImage( gadget, winFindImage( "HSliderThumbEnabled" ) );
		GadgetSliderSetEnabledThumbColor( gadget, GadgetSliderGetEnabledColor( gadget ) );
		GadgetSliderSetEnabledThumbBorderColor( gadget, GadgetSliderGetEnabledBorderColor( gadget ) );
		GadgetSliderSetEnabledSelectedThumbImage( gadget, winFindImage( "HSliderThumbEnabled" ) );
		GadgetSliderSetEnabledSelectedThumbColor( gadget, GadgetSliderGetEnabledBorderColor( gadget ) );
		GadgetSliderSetEnabledSelectedThumbBorderColor( gadget, GadgetSliderGetEnabledColor( gadget ) );

		// disabled
		GadgetSliderSetDisabledThumbImage( gadget, winFindImage( "HSliderThumbDisabled" ) );
		GadgetSliderSetDisabledThumbColor( gadget, GadgetSliderGetDisabledColor( gadget ) );
		GadgetSliderSetDisabledThumbBorderColor( gadget, GadgetSliderGetDisabledBorderColor( gadget ) );
		GadgetSliderSetDisabledSelectedThumbImage( gadget, winFindImage( "HSliderThumbDisabled" ) );
		GadgetSliderSetDisabledSelectedThumbColor( gadget, GadgetSliderGetDisabledBorderColor( gadget ) );
		GadgetSliderSetDisabledSelectedThumbBorderColor( gadget, GadgetSliderGetDisabledColor( gadget ) );

		// hilite
		GadgetSliderSetHiliteThumbImage( gadget, winFindImage( "HSliderThumbHilite" ) );
		GadgetSliderSetHiliteThumbColor( gadget, GadgetSliderGetHiliteColor( gadget ) );
		GadgetSliderSetHiliteThumbBorderColor( gadget, GadgetSliderGetHiliteBorderColor( gadget ) );
		GadgetSliderSetHiliteSelectedThumbImage( gadget, winFindImage( "HSliderThumbHiliteSelected" ) );
		GadgetSliderSetHiliteSelectedThumbColor( gadget, GadgetSliderGetHiliteBorderColor( gadget ) );
		GadgetSliderSetHiliteSelectedThumbBorderColor( gadget, GadgetSliderGetHiliteColor( gadget ) );


	}
	else if( BitIsSet( instData->getStyle(), GWS_VERT_SLIDER ) )
	{
		// enabled
		GadgetSliderSetEnabledImageTop( gadget, winFindImage( "VSliderEnabledTopEnd" ) );
		GadgetSliderSetEnabledImageBottom( gadget, winFindImage( "VSliderEnabledBottomEnd" ) );
		GadgetSliderSetEnabledImageCenter( gadget, winFindImage( "VSliderEnabledRepeatingCenter" ) );
		GadgetSliderSetEnabledImageSmallCenter( gadget, winFindImage( "VSliderEnabledSmallRepeatingCenter" ) );
		GadgetSliderSetEnabledColor( gadget, red );
		GadgetSliderSetEnabledBorderColor( gadget, lightRed );

		// disabled
		GadgetSliderSetDisabledImageTop( gadget, winFindImage( "VSliderDisabledTopEnd" ) );
		GadgetSliderSetDisabledImageBottom( gadget, winFindImage( "VSliderDisabledBottomEnd" ) );
		GadgetSliderSetDisabledImageCenter( gadget, winFindImage( "VSliderDisabledRepeatingCenter" ) );
		GadgetSliderSetDisabledImageSmallCenter( gadget, winFindImage( "VSliderDisabledSmallRepeatingCenter" ) );
		GadgetSliderSetDisabledColor( gadget, red );
		GadgetSliderSetDisabledBorderColor( gadget, lightRed );

		// hilite
		GadgetSliderSetHiliteImageTop( gadget, winFindImage( "VSliderHiliteTopEnd" ) );
		GadgetSliderSetHiliteImageBottom( gadget, winFindImage( "VSliderHiliteBottomEnd" ) );
		GadgetSliderSetHiliteImageCenter( gadget, winFindImage( "VSliderHiliteRepeatingCenter" ) );
		GadgetSliderSetHiliteImageSmallCenter( gadget, winFindImage( "VSliderHiliteSmallRepeatingCenter" ) );
		GadgetSliderSetHiliteColor( gadget, red );
		GadgetSliderSetHiliteBorderColor( gadget, lightRed );

		// set default text colors for the gadget
		gadget->winSetEnabledTextColors( enabledText, enabledTextBorder );
		gadget->winSetDisabledTextColors( disabledText, disabledTextBorder );
		gadget->winSetHiliteTextColors( hiliteText, hiliteTextBorder );
		gadget->winSetIMECompositeTextColors( imeCompositeText, imeCompositeTextBorder );

		//
		// set the default colors and images for the slider thumb
		//
		// enabled
		GadgetSliderSetEnabledThumbImage( gadget, winFindImage( "VSliderThumbEnabled" ) );
		GadgetSliderSetEnabledThumbColor( gadget, GadgetSliderGetEnabledColor( gadget ) );
		GadgetSliderSetEnabledThumbBorderColor( gadget, GadgetSliderGetEnabledBorderColor( gadget ) );
		GadgetSliderSetEnabledSelectedThumbImage( gadget, winFindImage( "VSliderThumbEnabled" ) );
		GadgetSliderSetEnabledSelectedThumbColor( gadget, GadgetSliderGetEnabledBorderColor( gadget ) );
		GadgetSliderSetEnabledSelectedThumbBorderColor( gadget, GadgetSliderGetEnabledColor( gadget ) );

		// disabled
		GadgetSliderSetDisabledThumbImage( gadget, winFindImage( "VSliderThumbDisabled" ) );
		GadgetSliderSetDisabledThumbColor( gadget, GadgetSliderGetDisabledColor( gadget ) );
		GadgetSliderSetDisabledThumbBorderColor( gadget, GadgetSliderGetDisabledBorderColor( gadget ) );
		GadgetSliderSetDisabledSelectedThumbImage( gadget, winFindImage( "VSliderThumbDisabled" ) );
		GadgetSliderSetDisabledSelectedThumbColor( gadget, GadgetSliderGetDisabledBorderColor( gadget ) );
		GadgetSliderSetDisabledSelectedThumbBorderColor( gadget, GadgetSliderGetDisabledColor( gadget ) );

		// hilite
		GadgetSliderSetHiliteThumbImage( gadget, winFindImage( "VSliderThumbHilite" ) );
		GadgetSliderSetHiliteThumbColor( gadget, GadgetSliderGetHiliteColor( gadget ) );
		GadgetSliderSetHiliteThumbBorderColor( gadget, GadgetSliderGetHiliteBorderColor( gadget ) );
		GadgetSliderSetHiliteSelectedThumbImage( gadget, winFindImage( "VSliderThumbHiliteSelected" ) );
		GadgetSliderSetHiliteSelectedThumbColor( gadget, GadgetSliderGetHiliteBorderColor( gadget ) );
		GadgetSliderSetHiliteSelectedThumbBorderColor( gadget, GadgetSliderGetHiliteColor( gadget ) );

	}
	else if( BitIsSet( instData->getStyle(), GWS_SCROLL_LISTBOX ) )
	{
		ListboxData *listboxData = (ListboxData *)gadget->winGetUserData();

		// set the colors
		GadgetListBoxSetColors( gadget,
														red,							// enabled
														lightRed,					// enabled border
														yellow,						// enabled selected item
														white,						// enabled selected item border
														gray,							// disabled
														lightGray,				// disabled border
														lightGray,				// disabled selected item
														white,						// disabled selected item border
														green,						// hilite
														darkGreen,				// hilite border
														white,						// hilite selected item
														darkGreen );			// hilite selected item border

		// now set the images

		// enabled
		GadgetListBoxSetEnabledImage( gadget, winFindImage( "ListBoxEnabled" ) );
		GadgetListBoxSetEnabledSelectedItemImageLeft( gadget, winFindImage( "ListBoxEnabledSelectedItemLeftEnd" ) );
		GadgetListBoxSetEnabledSelectedItemImageRight( gadget, winFindImage( "ListBoxEnabledSelectedItemRightEnd" ) );
		GadgetListBoxSetEnabledSelectedItemImageCenter( gadget, winFindImage( "ListBoxEnabledSelectedItemRepeatingCenter" ) );
		GadgetListBoxSetEnabledSelectedItemImageSmallCenter( gadget, winFindImage( "ListBoxEnabledSelectedItemSmallRepeatingCenter" ) );

		// disabled
		GadgetListBoxSetDisabledImage( gadget, winFindImage( "ListBoxDisabled" ) );
		GadgetListBoxSetDisabledSelectedItemImageLeft( gadget, winFindImage( "ListBoxDisabledSelectedItemLeftEnd" ) );
		GadgetListBoxSetDisabledSelectedItemImageRight( gadget, winFindImage( "ListBoxDisabledSelectedItemRightEnd" ) );
		GadgetListBoxSetDisabledSelectedItemImageCenter( gadget, winFindImage( "ListBoxDisabledSelectedItemRepeatingCenter" ) );
		GadgetListBoxSetDisabledSelectedItemImageSmallCenter( gadget, winFindImage( "ListBoxDisabledSelectedItemSmallRepeatingCenter" ) );


		// hilited
		GadgetListBoxSetHiliteImage( gadget, winFindImage( "ListBoxHilite" ) );
		GadgetListBoxSetHiliteSelectedItemImageLeft( gadget, winFindImage( "ListBoxHiliteSelectedItemLeftEnd" ) );
		GadgetListBoxSetHiliteSelectedItemImageRight( gadget, winFindImage( "ListBoxHiliteSelectedItemRightEnd" ) );
		GadgetListBoxSetHiliteSelectedItemImageCenter( gadget, winFindImage( "ListBoxHiliteSelectedItemRepeatingCenter" ) );
		GadgetListBoxSetHiliteSelectedItemImageSmallCenter( gadget, winFindImage( "ListBoxHiliteSelectedItemSmallRepeatingCenter" ) );

		// assign default slider colors and images as part of the list box
		GameWindow *slider = listboxData->slider;
		if( slider )
		{
			GameWindow *upButton = listboxData->upButton;
			GameWindow *downButton = listboxData->downButton;

			// slider and slider thumb ----------------------------------------------

			// enabled
			GadgetSliderSetEnabledImageTop( slider, winFindImage( "VSliderLargeEnabledTopEnd" ) );
			GadgetSliderSetEnabledImageBottom( slider, winFindImage( "VSliderLargeEnabledBottomEnd" ) );
			GadgetSliderSetEnabledImageCenter( slider, winFindImage( "VSliderLargeEnabledRepeatingCenter" ) );
			GadgetSliderSetEnabledImageSmallCenter( slider, winFindImage( "VSliderLargeEnabledSmallRepeatingCenter" ) );
			GadgetSliderSetEnabledThumbImage( slider, winFindImage( "VSliderLargeThumbEnabled" ) );
			GadgetSliderSetEnabledSelectedThumbImage( slider, winFindImage( "VSliderLargeThumbEnabled" ) );

			// disabled
			GadgetSliderSetDisabledImageTop( slider, winFindImage( "VSliderLargeDisabledTopEnd" ) );
			GadgetSliderSetDisabledImageBottom( slider, winFindImage( "VSliderLargeDisabledBottomEnd" ) );
			GadgetSliderSetDisabledImageCenter( slider, winFindImage( "VSliderLargeDisabledRepeatingCenter" ) );
			GadgetSliderSetDisabledImageSmallCenter( slider, winFindImage( "VSliderLargeDisabledSmallRepeatingCenter" ) );
			GadgetSliderSetDisabledThumbImage( slider, winFindImage( "VSliderLargeThumbDisabled" ) );
			GadgetSliderSetDisabledSelectedThumbImage( slider, winFindImage( "VSliderLargeThumbDisabled" ) );

			// hilite
			GadgetSliderSetHiliteImageTop( slider, winFindImage( "VSliderLargeHiliteTopEnd" ) );
			GadgetSliderSetHiliteImageBottom( slider, winFindImage( "VSliderLargeHiliteBottomEnd" ) );
			GadgetSliderSetHiliteImageCenter( slider, winFindImage( "VSliderLargeHiliteRepeatingCenter" ) );
			GadgetSliderSetHiliteImageSmallCenter( slider, winFindImage( "VSliderLargeHiliteSmallRepeatingCenter" ) );
			GadgetSliderSetHiliteThumbImage( slider, winFindImage( "VSliderLargeThumbHilite" ) );
			GadgetSliderSetHiliteSelectedThumbImage( slider, winFindImage( "VSliderLargeThumbHilite" ) );

			// up button ------------------------------------------------------------

			// enabled
			GadgetButtonSetEnabledImage( upButton, winFindImage( "VSliderLargeUpButtonEnabled" ) );
			GadgetButtonSetEnabledSelectedImage( upButton, winFindImage( "VSliderLargeUpButtonEnabled" ) );

			// disabled
			GadgetButtonSetDisabledImage( upButton, winFindImage( "VSliderLargeUpButtonDisabled" ) );
			GadgetButtonSetDisabledSelectedImage( upButton, winFindImage( "VSliderLargeUpButtonDisabled" ) );

			// hilite
			GadgetButtonSetHiliteImage( upButton, winFindImage( "VSliderLargeUpButtonHilite" ) );
			GadgetButtonSetHiliteSelectedImage( upButton, winFindImage( "VSliderLargeUpButtonHiliteSelected" ) );

			// down button ----------------------------------------------------------

			// enabled
			GadgetButtonSetEnabledImage( downButton, winFindImage( "VSliderLargeDownButtonEnabled" ) );
			GadgetButtonSetEnabledSelectedImage( downButton, winFindImage( "VSliderLargeDownButtonEnabled" ) );

			// disabled
			GadgetButtonSetDisabledImage( downButton, winFindImage( "VSliderLargeDownButtonDisabled" ) );
			GadgetButtonSetDisabledSelectedImage( downButton, winFindImage( "VSliderLargeDownButtonDisabled" ) );

			// hilite
			GadgetButtonSetHiliteImage( downButton, winFindImage( "VSliderLargeDownButtonHilite" ) );
			GadgetButtonSetHiliteSelectedImage( downButton, winFindImage( "VSliderLargeDownButtonHiliteSelected" ) );

		}

	}
	else if( BitIsSet( instData->getStyle(), GWS_COMBO_BOX ) )
	{
//		ComboBoxData *comboBoxData = (ComboBoxData *)gadget->winGetUserData();

		GadgetComboBoxSetColors( gadget,
														red,							// enabled
														lightRed,					// enabled border
														yellow,						// enabled selected item
														white,						// enabled selected item border
														gray,							// disabled
														lightGray,				// disabled border
														lightGray,				// disabled selected item
														white,						// disabled selected item border
														green,						// hilite
														darkGreen,				// hilite border
														white,						// hilite selected item
														darkGreen );			// hilite selected item border

		// enabled
		GadgetComboBoxSetEnabledImage( gadget, winFindImage( "ListBoxEnabled" ) );
		GadgetComboBoxSetEnabledSelectedItemImageLeft( gadget, winFindImage( "ListBoxEnabledSelectedItemLeftEnd" ) );
		GadgetComboBoxSetEnabledSelectedItemImageRight( gadget, winFindImage( "ListBoxEnabledSelectedItemRightEnd" ) );
		GadgetComboBoxSetEnabledSelectedItemImageCenter( gadget, winFindImage( "ListBoxEnabledSelectedItemRepeatingCenter" ) );
		GadgetComboBoxSetEnabledSelectedItemImageSmallCenter( gadget, winFindImage( "ListBoxEnabledSelectedItemSmallRepeatingCenter" ) );

		// disabled
		GadgetComboBoxSetDisabledImage( gadget, winFindImage( "ListBoxDisabled" ) );
		GadgetComboBoxSetDisabledSelectedItemImageLeft( gadget, winFindImage( "ListBoxDisabledSelectedItemLeftEnd" ) );
		GadgetComboBoxSetDisabledSelectedItemImageRight( gadget, winFindImage( "ListBoxDisabledSelectedItemRightEnd" ) );
		GadgetComboBoxSetDisabledSelectedItemImageCenter( gadget, winFindImage( "ListBoxDisabledSelectedItemRepeatingCenter" ) );
		GadgetComboBoxSetDisabledSelectedItemImageSmallCenter( gadget, winFindImage( "ListBoxDisabledSelectedItemSmallRepeatingCenter" ) );


		// hilited
		GadgetComboBoxSetHiliteImage( gadget, winFindImage( "ListBoxHilite" ) );
		GadgetComboBoxSetHiliteSelectedItemImageLeft( gadget, winFindImage( "ListBoxHiliteSelectedItemLeftEnd" ) );
		GadgetComboBoxSetHiliteSelectedItemImageRight( gadget, winFindImage( "ListBoxHiliteSelectedItemRightEnd" ) );
		GadgetComboBoxSetHiliteSelectedItemImageCenter( gadget, winFindImage( "ListBoxHiliteSelectedItemRepeatingCenter" ) );
		GadgetComboBoxSetHiliteSelectedItemImageSmallCenter( gadget, winFindImage( "ListBoxHiliteSelectedItemSmallRepeatingCenter" ) );

		gadget->winSetEnabledTextColors( enabledText, enabledTextBorder );
		gadget->winSetDisabledTextColors( disabledText, disabledTextBorder );
		gadget->winSetHiliteTextColors( hiliteText, hiliteTextBorder );
		gadget->winSetIMECompositeTextColors( imeCompositeText, imeCompositeTextBorder );

		GameWindow *dropDownButton = GadgetComboBoxGetDropDownButton( gadget );
		if ( dropDownButton )
		{
			// enabled background
			GadgetButtonSetEnabledImage( dropDownButton, winFindImage( "PushButtonEnabled" ) );
			// enabled selected button
			GadgetButtonSetEnabledSelectedImage( dropDownButton, winFindImage( "PushButtonEnabledSelected" ) );

			// Disabled background
			GadgetButtonSetDisabledImage( dropDownButton, winFindImage( "PushButtonDisabled" ) );
			// Disabled selected button
			GadgetButtonSetDisabledSelectedImage( dropDownButton, winFindImage( "PushButtonDisabledSelected" ) );

			// Hilite background
			GadgetButtonSetHiliteImage( dropDownButton, winFindImage( "PushButtonHilite" ) );
			// Hilite selected button
			GadgetButtonSetHiliteSelectedImage( dropDownButton, winFindImage( "PushButtonHiliteSelected" ) );

		}

		GameWindow *editBox = GadgetComboBoxGetEditBox( gadget );
		if ( editBox )
		{
			// enabled
			GadgetTextEntrySetEnabledImageLeft( editBox, winFindImage( "TextEntryEnabledLeftEnd" ) );
			GadgetTextEntrySetEnabledImageRight( editBox, winFindImage( "TextEntryEnabledRightEnd" ) );
			GadgetTextEntrySetEnabledImageCenter( editBox, winFindImage( "TextEntryEnabledRepeatingCenter" ) );
			GadgetTextEntrySetEnabledImageSmallCenter( editBox, winFindImage( "TextEntryEnabledSmallRepeatingCenter" ) );

			// disabled
			GadgetTextEntrySetDisabledImageLeft( editBox, winFindImage( "TextEntryDisabledLeftEnd" ) );
			GadgetTextEntrySetDisabledImageRight( editBox, winFindImage( "TextEntryDisabledRightEnd" ) );
			GadgetTextEntrySetDisabledImageCenter( editBox, winFindImage( "TextEntryDisabledRepeatingCenter" ) );
			GadgetTextEntrySetDisabledImageSmallCenter( editBox, winFindImage( "TextEntryDisabledSmallRepeatingCenter" ) );

			// hilited
			GadgetTextEntrySetHiliteImageLeft( editBox, winFindImage( "TextEntryHiliteLeftEnd" ) );
			GadgetTextEntrySetHiliteImageRight( editBox, winFindImage( "TextEntryHiliteRightEnd" ) );
			GadgetTextEntrySetHiliteImageCenter( editBox, winFindImage( "TextEntryHiliteRepeatingCenter" ) );
			GadgetTextEntrySetHiliteImageSmallCenter( editBox, winFindImage( "TextEntryHiliteSmallRepeatingCenter" ) );

		}

		GameWindow * listBox = GadgetComboBoxGetListBox( gadget );
		if ( listBox )
		{

			// now set the images

			// enabled
			GadgetListBoxSetEnabledImage( listBox, winFindImage( "ListBoxEnabled" ) );
			GadgetListBoxSetEnabledSelectedItemImageLeft( listBox, winFindImage( "ListBoxEnabledSelectedItemLeftEnd" ) );
			GadgetListBoxSetEnabledSelectedItemImageRight( listBox, winFindImage( "ListBoxEnabledSelectedItemRightEnd" ) );
			GadgetListBoxSetEnabledSelectedItemImageCenter( listBox, winFindImage( "ListBoxEnabledSelectedItemRepeatingCenter" ) );
			GadgetListBoxSetEnabledSelectedItemImageSmallCenter( listBox, winFindImage( "ListBoxEnabledSelectedItemSmallRepeatingCenter" ) );

			// disabled
			GadgetListBoxSetDisabledImage( listBox, winFindImage( "ListBoxDisabled" ) );
			GadgetListBoxSetDisabledSelectedItemImageLeft( listBox, winFindImage( "ListBoxDisabledSelectedItemLeftEnd" ) );
			GadgetListBoxSetDisabledSelectedItemImageRight( listBox, winFindImage( "ListBoxDisabledSelectedItemRightEnd" ) );
			GadgetListBoxSetDisabledSelectedItemImageCenter( listBox, winFindImage( "ListBoxDisabledSelectedItemRepeatingCenter" ) );
			GadgetListBoxSetDisabledSelectedItemImageSmallCenter( listBox, winFindImage( "ListBoxDisabledSelectedItemSmallRepeatingCenter" ) );


			// hilited
			GadgetListBoxSetHiliteImage( listBox, winFindImage( "ListBoxHilite" ) );
			GadgetListBoxSetHiliteSelectedItemImageLeft( listBox, winFindImage( "ListBoxHiliteSelectedItemLeftEnd" ) );
			GadgetListBoxSetHiliteSelectedItemImageRight( listBox, winFindImage( "ListBoxHiliteSelectedItemRightEnd" ) );
			GadgetListBoxSetHiliteSelectedItemImageCenter( listBox, winFindImage( "ListBoxHiliteSelectedItemRepeatingCenter" ) );
			GadgetListBoxSetHiliteSelectedItemImageSmallCenter( listBox, winFindImage( "ListBoxHiliteSelectedItemSmallRepeatingCenter" ) );

			// assign default slider colors and images as part of the list box
			GameWindow *slider = GadgetListBoxGetSlider( listBox );
			if( slider )
			{
				GameWindow *upButton = GadgetListBoxGetUpButton( listBox );
				GameWindow *downButton = GadgetListBoxGetDownButton( listBox );

				// slider and slider thumb ----------------------------------------------

				// enabled
				GadgetSliderSetEnabledImageTop( slider, winFindImage( "VSliderLargeEnabledTopEnd" ) );
				GadgetSliderSetEnabledImageBottom( slider, winFindImage( "VSliderLargeEnabledBottomEnd" ) );
				GadgetSliderSetEnabledImageCenter( slider, winFindImage( "VSliderLargeEnabledRepeatingCenter" ) );
				GadgetSliderSetEnabledImageSmallCenter( slider, winFindImage( "VSliderLargeEnabledSmallRepeatingCenter" ) );
				GadgetSliderSetEnabledThumbImage( slider, winFindImage( "VSliderLargeThumbEnabled" ) );
				GadgetSliderSetEnabledSelectedThumbImage( slider, winFindImage( "VSliderLargeThumbEnabled" ) );

				// disabled
				GadgetSliderSetDisabledImageTop( slider, winFindImage( "VSliderLargeDisabledTopEnd" ) );
				GadgetSliderSetDisabledImageBottom( slider, winFindImage( "VSliderLargeDisabledBottomEnd" ) );
				GadgetSliderSetDisabledImageCenter( slider, winFindImage( "VSliderLargeDisabledRepeatingCenter" ) );
				GadgetSliderSetDisabledImageSmallCenter( slider, winFindImage( "VSliderLargeDisabledSmallRepeatingCenter" ) );
				GadgetSliderSetDisabledThumbImage( slider, winFindImage( "VSliderLargeThumbDisabled" ) );
				GadgetSliderSetDisabledSelectedThumbImage( slider, winFindImage( "VSliderLargeThumbDisabled" ) );

				// hilite
				GadgetSliderSetHiliteImageTop( slider, winFindImage( "VSliderLargeHiliteTopEnd" ) );
				GadgetSliderSetHiliteImageBottom( slider, winFindImage( "VSliderLargeHiliteBottomEnd" ) );
				GadgetSliderSetHiliteImageCenter( slider, winFindImage( "VSliderLargeHiliteRepeatingCenter" ) );
				GadgetSliderSetHiliteImageSmallCenter( slider, winFindImage( "VSliderLargeHiliteSmallRepeatingCenter" ) );
				GadgetSliderSetHiliteThumbImage( slider, winFindImage( "VSliderLargeThumbHilite" ) );
				GadgetSliderSetHiliteSelectedThumbImage( slider, winFindImage( "VSliderLargeThumbHilite" ) );

				// up button ------------------------------------------------------------

				// enabled
				GadgetButtonSetEnabledImage( upButton, winFindImage( "VSliderLargeUpButtonEnabled" ) );
				GadgetButtonSetEnabledSelectedImage( upButton, winFindImage( "VSliderLargeUpButtonEnabled" ) );

				// disabled
				GadgetButtonSetDisabledImage( upButton, winFindImage( "VSliderLargeUpButtonDisabled" ) );
				GadgetButtonSetDisabledSelectedImage( upButton, winFindImage( "VSliderLargeUpButtonDisabled" ) );

				// hilite
				GadgetButtonSetHiliteImage( upButton, winFindImage( "VSliderLargeUpButtonHilite" ) );
				GadgetButtonSetHiliteSelectedImage( upButton, winFindImage( "VSliderLargeUpButtonHiliteSelected" ) );

				// down button ----------------------------------------------------------

				// enabled
				GadgetButtonSetEnabledImage( downButton, winFindImage( "VSliderLargeDownButtonEnabled" ) );
				GadgetButtonSetEnabledSelectedImage( downButton, winFindImage( "VSliderLargeDownButtonEnabled" ) );

				// disabled
				GadgetButtonSetDisabledImage( downButton, winFindImage( "VSliderLargeDownButtonDisabled" ) );
				GadgetButtonSetDisabledSelectedImage( downButton, winFindImage( "VSliderLargeDownButtonDisabled" ) );

				// hilite
				GadgetButtonSetHiliteImage( downButton, winFindImage( "VSliderLargeDownButtonHilite" ) );
				GadgetButtonSetHiliteSelectedImage( downButton, winFindImage( "VSliderLargeDownButtonHiliteSelected" ) );

			}
		}
	}
	else if( BitIsSet( instData->getStyle(), GWS_PROGRESS_BAR ) )
	{

		// enabled
		GadgetProgressBarSetEnabledColor( gadget, red );
		GadgetProgressBarSetEnabledBorderColor( gadget, lightRed );
		GadgetProgressBarSetEnabledImageLeft( gadget, winFindImage( "ProgressBarEnabledLeftEnd" ) );
		GadgetProgressBarSetEnabledImageRight( gadget, winFindImage( "ProgressBarEnabledRightEnd" ) );
		GadgetProgressBarSetEnabledImageCenter( gadget, winFindImage( "ProgressBarEnabledRepeatingCenter" ) );
		GadgetProgressBarSetEnabledImageSmallCenter( gadget, winFindImage( "ProgressBarEnabledSmallRepeatingCenter" ) );
		GadgetProgressBarSetEnabledBarColor( gadget, yellow );
		GadgetProgressBarSetEnabledBarBorderColor( gadget, white );
		GadgetProgressBarSetEnabledBarImageLeft( gadget, winFindImage( "ProgressBarEnabledBarLeftEnd" ) );
		GadgetProgressBarSetEnabledBarImageRight( gadget, winFindImage( "ProgressBarEnabledBarRightEnd" ) );
		GadgetProgressBarSetEnabledBarImageCenter( gadget, winFindImage( "ProgressBarEnabledBarRepeatingCenter" ) );
		GadgetProgressBarSetEnabledBarImageSmallCenter( gadget, winFindImage( "ProgressBarEnabledBarSmallRepeatingCenter" ) );

		// disabled
		GadgetProgressBarSetDisabledColor( gadget, darkGray );
		GadgetProgressBarSetDisabledBorderColor( gadget, lightGray );
		GadgetProgressBarSetDisabledImageLeft( gadget, winFindImage( "ProgressBarDisabledLeftEnd" ) );
		GadgetProgressBarSetDisabledImageRight( gadget, winFindImage( "ProgressBarDisabledRightEnd" ) );
		GadgetProgressBarSetDisabledImageCenter( gadget, winFindImage( "ProgressBarDisabledRepeatingCenter" ) );
		GadgetProgressBarSetDisabledImageSmallCenter( gadget, winFindImage( "ProgressBarDisabledSmallRepeatingCenter" ) );
		GadgetProgressBarSetDisabledBarColor( gadget, lightGray );
		GadgetProgressBarSetDisabledBarBorderColor( gadget, white );
		GadgetProgressBarSetDisabledBarImageLeft( gadget, winFindImage( "ProgressBarDisabledBarLeftEnd" ) );
		GadgetProgressBarSetDisabledBarImageRight( gadget, winFindImage( "ProgressBarDisabledBarRightEnd" ) );
		GadgetProgressBarSetDisabledBarImageCenter( gadget, winFindImage( "ProgressBarDisabledBarRepeatingCenter" ) );
		GadgetProgressBarSetDisabledBarImageSmallCenter( gadget, winFindImage( "ProgressBarDisabledBarSmallRepeatingCenter" ) );

		// Hilite
		GadgetProgressBarSetHiliteColor( gadget, green );
		GadgetProgressBarSetHiliteBorderColor( gadget, darkGreen );
		GadgetProgressBarSetHiliteImageLeft( gadget, winFindImage( "ProgressBarHiliteLeftEnd" ) );
		GadgetProgressBarSetHiliteImageRight( gadget, winFindImage( "ProgressBarHiliteRightEnd" ) );
		GadgetProgressBarSetHiliteImageCenter( gadget, winFindImage( "ProgressBarHiliteRepeatingCenter" ) );
		GadgetProgressBarSetHiliteImageSmallCenter( gadget, winFindImage( "ProgressBarHiliteSmallRepeatingCenter" ) );
		GadgetProgressBarSetHiliteBarColor( gadget, yellow );
		GadgetProgressBarSetHiliteBarBorderColor( gadget, white );
		GadgetProgressBarSetHiliteBarImageLeft( gadget, winFindImage( "ProgressBarHiliteBarLeftEnd" ) );
		GadgetProgressBarSetHiliteBarImageRight( gadget, winFindImage( "ProgressBarHiliteBarRightEnd" ) );
		GadgetProgressBarSetHiliteBarImageCenter( gadget, winFindImage( "ProgressBarHiliteBarRepeatingCenter" ) );
		GadgetProgressBarSetHiliteBarImageSmallCenter( gadget, winFindImage( "ProgressBarHiliteBarSmallRepeatingCenter" ) );

	}
	else if( BitIsSet( instData->getStyle(), GWS_STATIC_TEXT ) )
	{

		// enabled
		GadgetStaticTextSetEnabledImage( gadget, winFindImage( "StaticTextEnabled" ) );
		GadgetStaticTextSetEnabledColor( gadget, red );
		GadgetStaticTextSetEnabledBorderColor( gadget, lightRed );

		// disabled
		GadgetStaticTextSetDisabledImage( gadget, winFindImage( "StaticTextDisabled" ) );
		GadgetStaticTextSetDisabledColor( gadget, darkGray );
		GadgetStaticTextSetDisabledBorderColor( gadget, lightGray );

		// hilite
		GadgetStaticTextSetHiliteImage( gadget, winFindImage( "StaticTextHilite" ) );
		GadgetStaticTextSetHiliteColor( gadget, darkGreen );
		GadgetStaticTextSetHiliteBorderColor( gadget, lightGreen );

		// set default text colors for the gadget
		gadget->winSetEnabledTextColors( enabledText, enabledTextBorder );
		gadget->winSetDisabledTextColors( disabledText, disabledTextBorder );
		gadget->winSetHiliteTextColors( hiliteText, hiliteTextBorder );
		gadget->winSetIMECompositeTextColors( imeCompositeText, imeCompositeTextBorder );

	}
	else if( BitIsSet( instData->getStyle(), GWS_ENTRY_FIELD ) )
	{

		// enabled
		GadgetTextEntrySetEnabledImageLeft( gadget, winFindImage( "TextEntryEnabledLeftEnd" ) );
		GadgetTextEntrySetEnabledImageRight( gadget, winFindImage( "TextEntryEnabledRightEnd" ) );
		GadgetTextEntrySetEnabledImageCenter( gadget, winFindImage( "TextEntryEnabledRepeatingCenter" ) );
		GadgetTextEntrySetEnabledImageSmallCenter( gadget, winFindImage( "TextEntryEnabledSmallRepeatingCenter" ) );
		GadgetTextEntrySetEnabledColor( gadget, red );
		GadgetTextEntrySetEnabledBorderColor( gadget, lightRed );

		// disabled
		GadgetTextEntrySetDisabledImageLeft( gadget, winFindImage( "TextEntryDisabledLeftEnd" ) );
		GadgetTextEntrySetDisabledImageRight( gadget, winFindImage( "TextEntryDisabledRightEnd" ) );
		GadgetTextEntrySetDisabledImageCenter( gadget, winFindImage( "TextEntryDisabledRepeatingCenter" ) );
		GadgetTextEntrySetDisabledImageSmallCenter( gadget, winFindImage( "TextEntryDisabledSmallRepeatingCenter" ) );
		GadgetTextEntrySetDisabledColor( gadget, gray );
		GadgetTextEntrySetDisabledBorderColor( gadget, black );

		// hilited
		GadgetTextEntrySetHiliteImageLeft( gadget, winFindImage( "TextEntryHiliteLeftEnd" ) );
		GadgetTextEntrySetHiliteImageRight( gadget, winFindImage( "TextEntryHiliteRightEnd" ) );
		GadgetTextEntrySetHiliteImageCenter( gadget, winFindImage( "TextEntryHiliteRepeatingCenter" ) );
		GadgetTextEntrySetHiliteImageSmallCenter( gadget, winFindImage( "TextEntryHiliteSmallRepeatingCenter" ) );
		GadgetTextEntrySetHiliteColor( gadget, green );
		GadgetTextEntrySetHiliteBorderColor( gadget, darkGreen );

		// set default text colors for the gadget
		gadget->winSetEnabledTextColors( enabledText, enabledTextBorder );
		gadget->winSetDisabledTextColors( disabledText, disabledTextBorder );
		gadget->winSetHiliteTextColors( hiliteText, hiliteTextBorder );
		gadget->winSetIMECompositeTextColors( imeCompositeText, imeCompositeTextBorder );

	}

}

//-------------------------------------------------------------------------------------------------
/** Given a text label, retreive the real localized text associated
	* with that label */
//-------------------------------------------------------------------------------------------------
UnicodeString GameWindowManager::winTextLabelToText( AsciiString label )
{

	// sanity
	if( label.isEmpty() )
		return UnicodeString::TheEmptyString;

	/// @todo we need to write the string manager here, this is TEMPORARY!!!
	UnicodeString tmp;
	tmp.translate(label);
	return tmp;

}

//-------------------------------------------------------------------------------------------------
/** find the top window at the given coordinates */
//-------------------------------------------------------------------------------------------------
GameWindow *GameWindowManager::getWindowUnderCursor( Int x, Int y, Bool ignoreEnabled )
{
	if( m_mouseCaptor )
	{
		// in what what window within the captured window are we?
		return m_mouseCaptor->winPointInChild( x, y, ignoreEnabled );
	}

	if( m_grabWindow )
	{
		// in what what window within the grabbed window are we?
		return m_grabWindow->winPointInChild( x, y, ignoreEnabled );
	}

	GameWindow *window = nullptr;
	if( m_modalHead && m_modalHead->window )
	{
		return m_modalHead->window->winPointInChild( x, y, ignoreEnabled );
	}
	else
	{
		// search for top-level window which contains pointer
		for( window = m_windowList; window; window = window->m_next )
		{

			if( BitIsSet( window->m_status, WIN_STATUS_ABOVE ) &&
					!BitIsSet( window->m_status, WIN_STATUS_HIDDEN ) &&
					x >= window->m_region.lo.x &&
					x <= window->m_region.hi.x &&
					y >= window->m_region.lo.y &&
					y <= window->m_region.hi.y)
			{
				if( BitIsSet( window->m_status, WIN_STATUS_ENABLED ) || ignoreEnabled )
				{
					// determine which child window the mouse is in
					window = window->winPointInChild( x, y, ignoreEnabled );
					break;  // exit for
				}
			}
		}

		// check !above, below and hidden
		if( window == nullptr )
		{
			for( window = m_windowList; window; window = window->m_next )
			{
				if( !BitIsSet( window->m_status, WIN_STATUS_ABOVE |
																				WIN_STATUS_BELOW |
																				WIN_STATUS_HIDDEN ) &&
						x >= window->m_region.lo.x &&
						x <= window->m_region.hi.x &&
						y >= window->m_region.lo.y &&
						y <= window->m_region.hi.y)
				{
					if( BitIsSet( window->m_status, WIN_STATUS_ENABLED )|| ignoreEnabled)
					{
						// determine which child window the mouse is in
						window = window->winPointInChild( x, y, ignoreEnabled );
						break;  // exit for
					}
				}
			}
		}

		// check below and !hidden
		if( window == nullptr )
		{
			for( window = m_windowList; window; window = window->m_next )
			{
				if( BitIsSet( window->m_status, WIN_STATUS_BELOW ) &&
						!BitIsSet( window->m_status, WIN_STATUS_HIDDEN ) &&
						x >= window->m_region.lo.x &&
						x <= window->m_region.hi.x &&
						y >= window->m_region.lo.y &&
						y <= window->m_region.hi.y)
				{
					if( BitIsSet( window->m_status, WIN_STATUS_ENABLED )|| ignoreEnabled)
					{
						// determine which child window the mouse is in
						window = window->winPointInChild( x, y, ignoreEnabled );
						break;  // exit for
					}
				}
			}
		}
	}

	if( window )
	{
		if( BitIsSet( window->m_status, WIN_STATUS_NO_INPUT ))
		{
			// this window does not accept input, discard
			window = nullptr;
		}
		else if( ignoreEnabled && !( BitIsSet( window->m_status, WIN_STATUS_ENABLED ) ))
		{
			window = nullptr;
		}
	}

	return window;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static WindowMsgHandledType testGrab( GameWindow *window, UnsignedInt msg,
											WindowMsgData mData1, WindowMsgData mData2 )
{

	switch( msg )
	{

		case GWM_LEFT_DOWN:  return MSG_HANDLED;  // use it

	}

	return MSG_IGNORED;

}

//-------------------------------------------------------------------------------------------------
/** Just for testing */
//-------------------------------------------------------------------------------------------------
Bool GameWindowManager::initTestGUI()
{

//	winCreateFromScript( "_ATest.wnd" );
	return TRUE;

//	UnsignedByte alpha = 200;
	GameWindow *window;
	UnsignedInt statusFlags = WIN_STATUS_ENABLED | WIN_STATUS_DRAGGABLE | WIN_STATUS_IMAGE;
	WinInstanceData instData;

	// make some windows inside each other in the upper left
	window = winCreate( nullptr, statusFlags, 0, 0, 100, 100, nullptr, nullptr );
	window->winSetInputFunc( testGrab );
	window->winSetEnabledColor( 0, winMakeColor( 255, 254, 255, 255 ) );
	window->winSetEnabledBorderColor( 0 , winMakeColor( 0, 0, 0, 255 ) );
	window = winCreate( window, statusFlags, 10, 10, 50, 50, nullptr, nullptr );
	window->winSetInputFunc( testGrab );
	window->winSetEnabledColor( 0, winMakeColor( 128, 128, 128, 255 ) );
	window->winSetEnabledBorderColor( 0 , winMakeColor( 0, 0, 0, 255 ) );

	// make a push button
	instData.init();
	BitSet( instData.m_style, GWS_PUSH_BUTTON | GWS_MOUSE_TRACK );
	instData.m_textLabelString = "What Up?";
	window = gogoGadgetPushButton( nullptr,
																									 WIN_STATUS_ENABLED | WIN_STATUS_IMAGE,
																									 200, 100,
																									 100, 30,
																									 &instData, nullptr, TRUE );

	// make a push button
	instData.init();
	BitSet( instData.m_style, GWS_PUSH_BUTTON | GWS_MOUSE_TRACK );
	instData.m_textLabelString = "Enabled";
	window = gogoGadgetPushButton( nullptr,
																									 WIN_STATUS_ENABLED,
																									 330, 100,
																									 100, 30,
																									 &instData, nullptr, TRUE );

	// make a push button
	instData.init();
	BitSet( instData.m_style, GWS_PUSH_BUTTON | GWS_MOUSE_TRACK );
	instData.m_textLabelString = "Disabled";
	window = gogoGadgetPushButton( nullptr,
																									 0,
																									 450, 100,
																									 100, 30,
																									 &instData, nullptr, TRUE );

	// make a check box
	instData.init();
	instData.m_style = GWS_CHECK_BOX | GWS_MOUSE_TRACK;
	instData.m_textLabelString = "Check";
	window = gogoGadgetCheckbox( nullptr,
																								 WIN_STATUS_ENABLED |
																								 WIN_STATUS_IMAGE,
																								 200, 150,
																								 100, 30,
																								 &instData, nullptr, TRUE );

	// make a check box
	instData.init();
	instData.m_style = GWS_CHECK_BOX | GWS_MOUSE_TRACK;
	instData.m_textLabelString = "Check";
	window = gogoGadgetCheckbox( nullptr,
																								 WIN_STATUS_ENABLED,
																								 330, 150,
																								 100, 30,
																								 &instData, nullptr, TRUE );

	// make window to hold radio buttons
	window = winCreate( nullptr, WIN_STATUS_ENABLED | WIN_STATUS_DRAGGABLE,
																				200, 200, 250, 45, nullptr );
	window->winSetInputFunc( testGrab );
	window->winSetEnabledColor( 0, winMakeColor( 50, 50, 50, 200 ) );
	window->winSetEnabledBorderColor( 0, winMakeColor( 254, 254, 254, 255 ) );

	// make a radio button
	GameWindow *radio;
	RadioButtonData rData;
	instData.init();
	instData.m_style = GWS_RADIO_BUTTON | GWS_MOUSE_TRACK;
	instData.m_textLabelString = "Mama Said!";
	rData.group = 1;
	radio = gogoGadgetRadioButton( window,
																									 WIN_STATUS_ENABLED | WIN_STATUS_IMAGE,
																									 10, 10,
																									 100, 30,
																									 &instData,
																									 &rData, nullptr, TRUE );

	// make a radio button
	instData.init();
	instData.m_style = GWS_RADIO_BUTTON | GWS_MOUSE_TRACK;
	instData.m_textLabelString = "On the Run";
	radio = gogoGadgetRadioButton( window,
																									 WIN_STATUS_ENABLED,
																									 130, 10,
																									 100, 30,
																									 &instData,
																									 &rData, nullptr, TRUE );
	GadgetRadioSetEnabledColor( radio, GameMakeColor( 0, 0, 255, 255 ) );
	GadgetRadioSetEnabledBorderColor( radio, GameMakeColor( 0, 0, 255, 255 ) );

	// make a listbox
	ListboxData listData;
	memset( &listData, 0, sizeof( ListboxData ) );
	listData.listLength = 8;
	listData.autoScroll = 1;
	listData.scrollIfAtEnd = FALSE;
	listData.autoPurge = 1;
	listData.scrollBar = 1;
	listData.multiSelect = 1;
	listData.forceSelect = 0;
	listData.columns = 1;
	listData.columnWidth = nullptr;
	instData.init();
	instData.m_style = GWS_SCROLL_LISTBOX | GWS_MOUSE_TRACK;
	window = gogoGadgetListBox( nullptr,
																								WIN_STATUS_ENABLED,
																								200, 250,
																								100, 100,
																								&instData,
																								&listData, nullptr, TRUE );
	GadgetListBoxAddEntryText( window, L"Listbox text",
												 winMakeColor( 255, 255, 255, 255 ), -1, 0 );
	GadgetListBoxAddEntryText( window, L"More text",
												 winMakeColor( 105, 105, 255, 255 ), -1, 0 );
	GadgetListBoxAddEntryText( window, L"Nothing",
												 winMakeColor( 105, 105, 255, 255 ), -1, 0 );
	GadgetListBoxAddEntryText( window, L"Seasons",
												 winMakeColor( 105, 205, 255, 255 ), -1, 0 );
	GadgetListBoxAddEntryText( window, L"Misery",
												 winMakeColor( 235, 105, 255, 255 ), -1, 0 );
	GadgetListBoxAddEntryText( window, L"Natural",
												 winMakeColor( 105, 205, 45, 255 ), -1, 0 );
	window->winSetFont( TheFontLibrary->getFont( "Times New Roman", 12, FALSE ) );

	// make a listbox
	memset( &listData, 0, sizeof( ListboxData ) );
	listData.listLength = 8;
	listData.autoScroll = 1;
	listData.scrollIfAtEnd = FALSE;
	listData.autoPurge = 1;
	listData.scrollBar = 1;
	listData.multiSelect = 0;
	listData.forceSelect = 0;
	listData.columns = 1;
	listData.columnWidth = nullptr;
	instData.init();
	instData.m_style = GWS_SCROLL_LISTBOX | GWS_MOUSE_TRACK;
	window = gogoGadgetListBox( nullptr,
																								WIN_STATUS_ENABLED | WIN_STATUS_IMAGE,
																								75, 250,
																								100, 100,
																								&instData,
																								&listData, nullptr, TRUE );
	GadgetListBoxAddEntryText( window, L"Listbox text",
												 winMakeColor( 255, 255, 255, 255 ), -1, -1 );
	GadgetListBoxAddEntryText( window, L"More text",
												 winMakeColor( 105, 105, 255, 255 ), -1, -1 );
	GadgetListBoxAddEntryText( window, L"Nothing",
												 winMakeColor( 105, 105, 255, 255 ), -1, -1 );
	GadgetListBoxAddEntryText( window, L"Seasons",
												 winMakeColor( 105, 205, 255, 255 ), -1, -1 );
	GadgetListBoxAddEntryText( window, L"Misery",
												 winMakeColor( 235, 105, 255, 255 ), -1, -1 );
	GadgetListBoxAddEntryText( window, L"Natural",
												 winMakeColor( 105, 205, 45, 255 ), -1, -1 );

	// make a vert slider
	SliderData sliderData;
	memset( &sliderData, 0, sizeof( sliderData ) );
	sliderData.maxVal = 100;
	sliderData.minVal = 0;
	sliderData.numTicks = 100;
	sliderData.position = 0;
	instData.init();
	instData.m_style = GWS_VERT_SLIDER | GWS_MOUSE_TRACK;
	window = gogoGadgetSlider( nullptr,
																							 WIN_STATUS_ENABLED,
																							 360, 250,
																							 11, 100,
																							 &instData,
																							 &sliderData, nullptr, TRUE );

	// make a vert slider
	memset( &sliderData, 0, sizeof( sliderData ) );
	sliderData.maxVal = 100;
	sliderData.minVal = 0;
	sliderData.numTicks = 100;
	sliderData.position = 0;
	instData.init();
	instData.m_style = GWS_VERT_SLIDER | GWS_MOUSE_TRACK;
	window = gogoGadgetSlider( nullptr,
																							 WIN_STATUS_ENABLED | WIN_STATUS_IMAGE,
																							 400, 250,
																							 11, 100,
																							 &instData,
																							 &sliderData, nullptr, TRUE );

	// make a horizontal slider
	memset( &sliderData, 0, sizeof( sliderData ) );
	sliderData.maxVal = 100;
	sliderData.minVal = 0;
	sliderData.numTicks = 100;
	sliderData.position = 0;
	instData.init();
	instData.m_style = GWS_HORZ_SLIDER | GWS_MOUSE_TRACK;
	window = gogoGadgetSlider( nullptr,
																							 WIN_STATUS_ENABLED,
																							 200, 400,
																							 200, 11,
																							 &instData,
																							 &sliderData, nullptr, TRUE );

	// make a horizontal slider
	memset( &sliderData, 0, sizeof( sliderData ) );
	sliderData.maxVal = 100;
	sliderData.minVal = 0;
	sliderData.numTicks = 100;
	sliderData.position = 0;
	instData.init();
	instData.m_style = GWS_HORZ_SLIDER | GWS_MOUSE_TRACK;
	window = gogoGadgetSlider( nullptr,
																							 WIN_STATUS_ENABLED | WIN_STATUS_IMAGE,
																							 200, 420,
																							 200, 11,
																							 &instData,
																							 &sliderData, nullptr, TRUE );

	// make a progress bar
	instData.init();
	instData.m_style = GWS_PROGRESS_BAR | GWS_MOUSE_TRACK;
	window = gogoGadgetProgressBar( nullptr,
																									 WIN_STATUS_ENABLED,
																									 200, 450,
																									 250, 15,
																									 &instData, nullptr, TRUE );

	// make a progress bar
	instData.init();
	instData.m_style = GWS_PROGRESS_BAR | GWS_MOUSE_TRACK;
	window = gogoGadgetProgressBar( nullptr,
																									 WIN_STATUS_ENABLED | WIN_STATUS_IMAGE,
																									 200, 470,
																									 250, 15,
																									 &instData, nullptr, TRUE );

	// make some static text
	TextData textData;
	textData.centered = 1;
	instData.init();
	instData.m_style = GWS_STATIC_TEXT | GWS_MOUSE_TRACK;
	instData.m_textLabelString = "Centered Static Text";
	window = gogoGadgetStaticText( nullptr,
																									 WIN_STATUS_ENABLED,
																									 200, 490,
																									 300, 25,
																									 &instData,
																									 &textData, nullptr, TRUE );

	// make some static text
	textData.centered = 0;
	instData.init();
	instData.m_style = GWS_STATIC_TEXT | GWS_MOUSE_TRACK;
	instData.m_textLabelString = "Not Centered Static Text";
	window = gogoGadgetStaticText( nullptr,
																									 WIN_STATUS_ENABLED | WIN_STATUS_IMAGE,
																									 200, 520,
																									 300, 25,
																									 &instData,
																									 &textData, nullptr, TRUE );
	window->winSetEnabledTextColors( winMakeColor( 128, 128, 255, 255 ),
																	 winMakeColor( 255, 255, 255, 255 ) );

	// make some entry text
	EntryData entryData;
	memset( &entryData, 0, sizeof( entryData ) );
	entryData.maxTextLen = 30;
	instData.init();
	instData.m_style = GWS_ENTRY_FIELD | GWS_MOUSE_TRACK;
	instData.m_textLabelString = "Entry";
	window = gogoGadgetTextEntry( nullptr,
																									 WIN_STATUS_ENABLED,
																									 450, 270,
																									 400, 30,
																									 &instData,
																									 &entryData, nullptr, TRUE );

	// make some entry text
	memset( &entryData, 0, sizeof( entryData ) );
	entryData.maxTextLen = 30;
	instData.init();
	instData.m_style = GWS_ENTRY_FIELD | GWS_MOUSE_TRACK;
	instData.m_textLabelString = "Entry";
	window = gogoGadgetTextEntry( nullptr,
																									 WIN_STATUS_ENABLED | WIN_STATUS_IMAGE,
																									 450, 310,
																									 400, 30,
																									 &instData,
																									 &entryData, nullptr, TRUE );

	return TRUE;

}


void GameWindowManager::winNextTab( GameWindow *window )
{
	if(m_tabList.empty()|| m_modalHead)
		return;

	GameWindowList::iterator it = m_tabList.begin();
	while( it != m_tabList.end())
	{
		if(*it == window)
		{
			it++;
			break;
		}
		it++;
	}
	if(it != m_tabList.end())
		winSetFocus(*it);
	else
	{
		winSetFocus(*m_tabList.begin());
	}
	winSetLoneWindow(nullptr);
}

void GameWindowManager::winPrevTab( GameWindow *window )
{
	if(m_tabList.empty() || m_modalHead)
		return;

	GameWindowList::reverse_iterator it = m_tabList.rbegin();
	while( it != m_tabList.rend())
	{
		if(*it == window)
		{
			it++;
			break;
		}
		it++;
	}
	if(it != m_tabList.rend())
		winSetFocus(*it);
	else
	{
		winSetFocus(*m_tabList.rbegin());
	}
	winSetLoneWindow(nullptr);
}

void GameWindowManager::registerTabList( GameWindowList tabList )
{
	m_tabList.clear();
	m_tabList = tabList;
}

void GameWindowManager::clearTabList()
{
	m_tabList.clear();
}


GameWindow *GameWindowManagerDummy::winGetWindowFromId(GameWindow *window, Int id)
{
	window = GameWindowManager::winGetWindowFromId(window, id);
	if (window != nullptr)
		return window;

	// Just return any window, callers expect this to be non-null
	return m_windowList;
}

WindowMsgHandledType DummyWindowSystem(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	return MSG_IGNORED;
}

GameWindow *GameWindowManagerDummy::winCreateFromScript(AsciiString filenameString, WindowLayoutInfo *info)
{
	WindowLayoutInfo scriptInfo;
	GameWindow* dummyWindow = winCreate(nullptr, 0, 0, 0, 100, 100, DummyWindowSystem, nullptr);
	scriptInfo.windows.push_back(dummyWindow);
	if (info)
		*info = scriptInfo;
	return dummyWindow;
}

GameWindowDummy::~GameWindowDummy()
{
}



