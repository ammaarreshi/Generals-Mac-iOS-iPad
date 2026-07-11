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

// FILE: CommandXlat.h ///////////////////////////////////////////////////////////
// Author: Steven Johnson, Dec 2001

#pragma once

#include "GameClient/InGameUI.h"

enum GUICommandType CPP_11(: Int);

//-----------------------------------------------------------------------------
class CommandTranslator : public GameMessageTranslator
{
public:

	CommandTranslator();
	virtual ~CommandTranslator() override;

	enum CommandEvaluateType { DO_COMMAND, DO_HINT, EVALUATE_ONLY };


	GameMessage::Type evaluateForceAttack( Drawable *draw, const Coord3D *pos, CommandEvaluateType type );
	GameMessage::Type evaluateContextCommand( Drawable *draw, const Coord3D *pos, CommandEvaluateType type );

private:

	Int m_objective;
	Bool m_teamExists;				///< is there a currently selected "team"?

	// these are for determining if a drag occurred or it was just a sloppy click
	ICoord2D m_rightMouseDownAnchor;		// the location of a possible mouse drag start
	ICoord2D m_rightMouseUpAnchor;			// the location of a possible mouse drag end
	UnsignedInt m_rightMouseDownTimeMs;	// when the mouse down happened
	UnsignedInt m_rightMouseUpTimeMs;		// when the mouse up happened

	GameMessage::Type createMoveToLocationMessage( Drawable *draw, const Coord3D *dest, CommandEvaluateType commandType );
	GameMessage::Type createAttackMessage( Drawable *draw, Drawable *other, CommandEvaluateType commandType );
	GameMessage::Type createEnterMessage( Drawable *enter, CommandEvaluateType commandType );
	GameMessage::Type issueMoveToLocationCommand( const Coord3D *pos, Drawable *drawableInWay, CommandEvaluateType commandType );
	GameMessage::Type issueAttackCommand( Drawable *target, CommandEvaluateType commandType, GUICommandType command = (GUICommandType)0 );
	GameMessage::Type issueSpecialPowerCommand( const CommandButton *command, CommandEvaluateType commandType, Drawable *target, const Coord3D *pos, Object* ignoreSelObj );
	GameMessage::Type issueFireWeaponCommand( const CommandButton *command, CommandEvaluateType commandType, Drawable *target, const Coord3D *pos );
	GameMessage::Type issueCombatDropCommand( const CommandButton *command, CommandEvaluateType commandType, Drawable *target, const Coord3D *pos );

	virtual GameMessageDisposition translateGameMessage(const GameMessage *msg) override;
};


enum FilterTypes CPP_11(: Int)
{
	FT_NULL_FILTER=0,
	// The following are screen filter shaders, that modify the rendered viewport after it is drawn.
	FT_VIEW_BW_FILTER,		//filter to apply a black & white filter to the screen.
	FT_VIEW_MOTION_BLUR_FILTER, //filter to apply motion blur filter to screen.
	FT_VIEW_CROSSFADE,				///<filter to apply a cross blend between previous/current views.
	FT_VIEW_DEFAULT,				///<default filter mode for default filter.
	FT_MAX
};

enum FilterModes CPP_11(: Int)
{
	FM_NULL_MODE = 0,

	// These apply to FT_VIEW_BW_FILTER
	FM_VIEW_BW_BLACK_AND_WHITE, // BW Filter to black & white
	FM_VIEW_BW_RED_AND_WHITE, // BW Filter to red & white
	FM_VIEW_BW_GREEN_AND_WHITE, // BW Filter to green & white

	// These apply to FT_VIEW_CROSSFADE
	FM_VIEW_CROSSFADE_CIRCLE,	// Fades from previous to current view using expanding circle.
	FM_VIEW_CROSSFADE_FB_MASK,	// Fades from previous to current using mask stored in framebuffer alpha.

	// These apply to FT_VIEW_MOTION_BLUR_FILTER
	FM_VIEW_MB_IN_AND_OUT_ALPHA, // Motion blur filter in and out alpha blur
	FM_VIEW_MB_IN_AND_OUT_SATURATE, // Motion blur filter in and out saturated blur
	FM_VIEW_MB_IN_ALPHA, // Motion blur filter in alpha blur
	FM_VIEW_MB_OUT_ALPHA, // Motion blur filter out alpha blur
	FM_VIEW_MB_IN_SATURATE, // Motion blur filter in saturated blur
	FM_VIEW_MB_OUT_SATURATE, // Motion blur filter out saturated blur
	FM_VIEW_MB_END_PAN_ALPHA, // Moton blur on screen pan (for camera tracks object mode)

	FM_VIEW_DEFAULT,	//Default filter that's enabled when all others are off.

	// NOTE: This has to be the last entry in this enum.
	// Add new entries before this one.  jba.
	FM_VIEW_MB_PAN_ALPHA, // Moton blur on screen pan (for camera tracks object mode)

};

class PickAndPlayInfo
{
public:
	PickAndPlayInfo(); //INITIALIZE THE CONSTRUCTOR IN CPP

	Bool						m_air;					//Are we attacking an airborned target?
	Drawable				*m_drawTarget;	//Do we have an override draw target?
	WeaponSlotType	*m_weaponSlot;	//Are we forcing a specific weapon slot? nullptr if unspecified.
	SpecialPowerType m_specialPowerType; //Which special power are use using? SPECIAL_INVALID if unspecified.
};

extern void pickAndPlayUnitVoiceResponse( const DrawableList *list, GameMessage::Type msgType, PickAndPlayInfo *info = nullptr );
