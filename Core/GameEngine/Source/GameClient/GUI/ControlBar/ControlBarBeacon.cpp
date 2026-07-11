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

// FILE: ControlBarBeacon.cpp /////////////////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Methods specific to the control bar beacon display
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/NameKeyGenerator.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::populateBeacon( Object *beacon )
{

	// set the portrait for the thing being constructed
	setPortraitByObject( beacon );

	static NameKeyType textID = NAMEKEY("ControlBar.wnd:EditBeaconText");
	static NameKeyType staticTextID = NAMEKEY("ControlBar.wnd:StaticTextBeaconLabel");
	static NameKeyType clearButtonID = NAMEKEY("ControlBar.wnd:ButtonClearBeaconText");

	GameWindow *textEntryWin = TheWindowManager->winGetWindowFromId(nullptr, textID);
	GameWindow *staticTextWin = TheWindowManager->winGetWindowFromId(nullptr, staticTextID);
	GameWindow *buttonWin = TheWindowManager->winGetWindowFromId(nullptr, clearButtonID);

	if (beacon->isLocallyControlled())
	{
		if (textEntryWin)
		{
			textEntryWin->winHide(FALSE);
			GadgetTextEntrySetText( textEntryWin, beacon->getDrawable()->getCaptionText() );
			TheWindowManager->winSetFocus( textEntryWin );
		}

		if (staticTextWin)
			staticTextWin->winHide(FALSE);

		if (buttonWin)
			buttonWin->winHide(FALSE);
	}
	else
	{
		if (textEntryWin)
			textEntryWin->winHide(TRUE);

		if (staticTextWin)
			staticTextWin->winHide(TRUE);

		if (buttonWin)
			buttonWin->winHide(TRUE);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::updateContextBeacon()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType BeaconWindowInput( GameWindow *window, UnsignedInt msg,
																			 WindowMsgData mData1, WindowMsgData mData2 )
{
	if (msg == GWM_CHAR && mData1 == KEY_ESC)
	{
		TheInGameUI->deselectAllDrawables(TRUE); // there should only be one beacon and nothing else selected
		return MSG_HANDLED;
	}

	return MSG_IGNORED;

}
