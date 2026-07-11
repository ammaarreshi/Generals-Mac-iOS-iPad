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

// FILE: SelectionXlat.h ///////////////////////////////////////////////////////////
// Author: Steven Johnson, Dec 2001

#pragma once

#include "GameClient/InGameUI.h"

class ThingTemplate;

typedef std::map<const ThingTemplate *, int> SelectCountMap;
typedef SelectCountMap::iterator SelectCountMapIt;

//-----------------------------------------------------------------------------
class SelectionTranslator : public GameMessageTranslator
{
	// this is an evil friend wrapper due to the current limitations of Drawable iterators.
	friend Bool selectFriendsWrapper( Drawable *draw, void *userData );
	friend Bool killThemKillThemAllWrapper( Drawable *draw, void *userData );
private:

	Bool m_leftMouseButtonIsDown;
	Bool m_dragSelecting;
	Bool m_displayedMaxWarning;	// did we already display a warning about selecting too many units?
	UnsignedInt m_lastGroupSelTime;
	Int m_lastGroupSelGroup;
	ICoord2D m_leftMouseDownAnchor;		// Note: Used for drawing feedback only.
	ICoord2D m_rightMouseDownAnchor;	// Note: Used for drawing feedback only.
	UnsignedInt m_rightMouseDownTimeMs;    ///< timer used for checking double click for type selection
	Coord3D m_rightMouseDownCameraPos;

	SelectCountMap m_selectCountMap;

	Bool selectFriends( Drawable *draw, GameMessage *createTeamMsg, Bool dragSelecting );
	Bool killThemKillThemAll( Drawable *draw, GameMessage *killThemAllMsg );

public:
	SelectionTranslator();
	virtual ~SelectionTranslator() override;
	virtual GameMessageDisposition translateGameMessage(const GameMessage *msg) override;
	//added for fix to the drag selection when entering control bar
	//changes the mode of drag selecting to it's opposite
	void setDragSelecting(Bool dragSelect);
	void setLeftMouseButton(Bool state);

#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
  Bool m_HandOfGodSelectionMode;
  Bool isHandOfGodSelectionMode() { return m_HandOfGodSelectionMode; };
#endif

private:
	GameMessageDisposition onMetaBeginForceAttack(const GameMessage *msg);
	GameMessageDisposition onMetaEndForceAttack(const GameMessage *msg);
	GameMessageDisposition onRawMousePosition(const GameMessage *msg);
	GameMessageDisposition onMouseLeftDoubleClick(const GameMessage *msg);
	GameMessageDisposition onMouseoverDrawableHint(const GameMessage *msg);
	GameMessageDisposition onMouseLeftClick(const GameMessage *msg);
	GameMessageDisposition onRawMouseLeftButtonDown(const GameMessage *msg);
	GameMessageDisposition onRawMouseLeftButtonUp(const GameMessage *msg);
	GameMessageDisposition onRawMouseRightButtonDown(const GameMessage *msg);
	GameMessageDisposition onRawMouseRightButtonUp(const GameMessage *msg);
	GameMessageDisposition onMetaCreateTeam(const GameMessage *msg);
	GameMessageDisposition onMetaSelectTeam(const GameMessage *msg);
	GameMessageDisposition onMetaAddTeam(const GameMessage *msg);
	GameMessageDisposition onMetaViewTeam(const GameMessage *msg);
	GameMessageDisposition onMetaOptions(const GameMessage *msg);
#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	GameMessageDisposition onCheatToggleHandOfGodMode(const GameMessage *msg);
#endif
#if defined(RTS_DEBUG)
	GameMessageDisposition onMetaDemoToggleHandOfGodMode(const GameMessage *msg);
	GameMessageDisposition onMetaDemoToggleHurtMeMode(const GameMessage *msg);
	GameMessageDisposition onMetaDemoDebugSelection(const GameMessage *msg);
#endif
};

Bool CanSelectDrawable( const Drawable *draw, Bool dragSelecting );
extern SelectionTranslator *TheSelectionTranslator;
