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

// FILE: ControlBarPrintPositions.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Sep 2002
//
//	Filename: 	ControlBarPrintPositions.cpp
//
//	author:		Chris Huybregts
//
//	purpose:	Convenience function for degayifying the whole squished control bar
//						process
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "GameClient/GameWindowManager.h"
#include "GameClient/WindowLayout.h"
//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
void PrintInfoRecursive( GameWindow *win, FILE *fp)
{
	if(!win)
		return;
	ICoord2D pos, size;
	win->winGetSize(&size.x, &size.y);
	win->winGetPosition(&pos.x, &pos.y);
	fprintf(fp, "ControlBarResizer %s\n",win->winGetInstanceData()->m_decoratedNameString.str());
	fprintf(fp, "  AltPosition = X:%d Y:%d\n",pos.x, pos.y);
	fprintf(fp, "  AltSize = X:%d Y:%d\n",size.x, size.y);
	fprintf(fp, "END\n\n");

	PrintInfoRecursive(win->winGetChild(),fp);
	PrintInfoRecursive(win->winGetNext(),fp);

}

void PrintOffsetsFromControlBarParent()
{
	GameWindow *controlBarParent = TheWindowManager->winGetWindowFromId( nullptr,  TheNameKeyGenerator->nameToKey( "ControlBar.wnd:ControlBarParent" ));
	if(!controlBarParent)
		return;

	WindowLayout *layout = TheWindowManager->winCreateLayout("controlBarHidden.wnd");
	if(!layout)
		return;
	FILE *fp = fopen("ControlBarEasier.txt", "w");
	if(!fp)
		return;

	PrintInfoRecursive(layout->getFirstWindow(), fp);

	fclose(fp);
	layout->destroyWindows();
	deleteInstance(layout);
}


//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

