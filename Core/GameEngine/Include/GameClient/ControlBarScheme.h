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

// FILE: ControlBarScheme.h /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Apr 2002
//
//	Filename: 	ControlBarScheme.h
//
//	author:		Chris Huybregts
//
//	purpose:
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "GameClient/Color.h"

//-----------------------------------------------------------------------------
// FORWARD REFERENCES /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
class AsciiString;
class playerTemplate;
class Image;
enum TimeOfDay CPP_11(: Int);

//-----------------------------------------------------------------------------
// TYPE DEFINES ///////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#define MAX_CONTROL_BAR_SCHEME_IMAGE_LAYERS 6
#define CONTROL_BAR_SCHEME_FOREGROUND_IMAGE_LAYERS 3

// Class that holds the images the control bar will draw
//-----------------------------------------------------------------------------
class ControlBarSchemeImage
{
public:
	ControlBarSchemeImage();
	~ControlBarSchemeImage();

	AsciiString m_name;						///< Name of the image
	ICoord2D m_position;					///< the position we'll draw it at
	ICoord2D m_size;							///< the size of the image needed when we draw it
	Image *m_image;								///< the actual pointer to the mapped image

	// m_layer is where the image will get drawn,  everything in layer 0-2 gets drawn during the foreground draw
	// the layers 3-5 gets drawn during the background draw
	Int m_layer; //layer means how deep the image will be drawn, it's a number between 0-5 with 0 being on top
};

// Class that will hold the information needed for the animations
//-----------------------------------------------------------------------------
class ControlBarSchemeAnimation
{
public:
	ControlBarSchemeAnimation();
	~ControlBarSchemeAnimation();
	/// Enum that will contain all the kinds of animations we have... make sure in ControlBarScheme.cpp there's a
	/// mapping for it for the INI translation
	enum
	{
		CB_ANIM_SLIDE_RIGHT = 0,

		CB_ANIM_MAX
	};

	AsciiString m_name;										///< Current animation name
	Int m_animType;												///< Type of animation that this will follow
	ControlBarSchemeImage *m_animImage;		///< Pointer of the image that this animation will act on
	UnsignedInt m_animDuration;						///< Contians how long the animation should take based off game frames
	ICoord2D m_finalPos;									///< The final position when we hit the m_animDuration frame

	UnsignedInt getCurrentFrame() { return m_currentFrame; }
	void setCurrentFrame( UnsignedInt currentFrame ) { m_currentFrame = currentFrame; }
	ICoord2D getStartPos() { return m_startPos; }
	void setStartPos(ICoord2D startPos) { m_startPos = startPos;	}
private:
	ICoord2D m_startPos;									///< set when we first begin an animation
	UnsignedInt m_currentFrame;							///< This is the last frame (a value between 0 and m_animDuration)
};


// Class that each scheme will have.  Contains all information about that scheme
//-----------------------------------------------------------------------------
class ControlBarScheme
{
public:
	ControlBarScheme();
	~ControlBarScheme();

	void init();
	void update();
	void drawForeground( Coord2D multi, ICoord2D offset );	///< draw function to be called within a w3d draw procedure for the foreground
	void drawBackground( Coord2D multi, ICoord2D offset );	///< draw function to be called within a w3d draw procedure for the background
	void reset();

	void addAnimation( ControlBarSchemeAnimation *schemeAnim );
	void addImage( ControlBarSchemeImage *schemeImage);
	void updateAnim (ControlBarSchemeAnimation * anim);


	AsciiString m_name;												///< it's name
	ICoord2D m_ScreenCreationRes;							///< Used to determine what screen res this will look the best on
	AsciiString m_side;												///< contain what faction type this command bar was made for (used when selecting command bar by template
	Image *m_buttonQueueImage;								///< We'll probably want each one to have it's own image.
	Image *m_rightHUDImage;										///< We'll probably want each one to have it's own right HUD image.
	Color m_buildUpClockColor;								///< we can setup the color for the buildup clock if we want

	Color m_borderBuildColor;									///< we can setup the color for the button border colors
	Color m_borderActionColor;								///< we can setup the color for the button border colors
	Color m_borderUpgradeColor;								///< we can setup the color for the button border colors
	Color m_borderSystemColor;								///< we can setup the color for the button border colors

	Color m_commandBarBorderColor;

	Image *m_optionsButtonEnable;
	Image *m_optionsButtonHightlited;
	Image *m_optionsButtonPushed;
	Image *m_optionsButtonDisabled;

	Image *m_idleWorkerButtonEnable;
	Image *m_idleWorkerButtonHightlited;
	Image *m_idleWorkerButtonPushed;
	Image *m_idleWorkerButtonDisabled;

	Image *m_buddyButtonEnable;
	Image *m_buddyButtonHightlited;
	Image *m_buddyButtonPushed;
	Image *m_buddyButtonDisabled;

	Image *m_beaconButtonEnable;
	Image *m_beaconButtonHightlited;
	Image *m_beaconButtonPushed;
	Image *m_beaconButtonDisabled;

	Image *m_genBarButtonIn;
	Image *m_genBarButtonOn;

	Image *m_toggleButtonUpIn;
	Image *m_toggleButtonUpOn;
	Image *m_toggleButtonUpPushed;
	Image *m_toggleButtonDownIn;
	Image *m_toggleButtonDownOn;
	Image *m_toggleButtonDownPushed;

	Image *m_generalButtonEnable;
	Image *m_generalButtonHightlited;
	Image *m_generalButtonPushed;
	Image *m_generalButtonDisabled;

	Image *m_uAttackButtonEnable;
	Image *m_uAttackButtonHightlited;
	Image *m_uAttackButtonPushed;

	Image *m_minMaxButtonEnable;
	Image *m_minMaxButtonHightlited;
	Image *m_minMaxButtonPushed;

	Image *m_genArrow;


	ICoord2D m_moneyUL;
	ICoord2D m_moneyLR;

	ICoord2D m_minMaxUL;
	ICoord2D m_minMaxLR;

	ICoord2D m_generalUL;
	ICoord2D m_generalLR;

	ICoord2D m_uAttackUL;
	ICoord2D m_uAttackLR;

	ICoord2D m_optionsUL;
	ICoord2D m_optionsLR;

	ICoord2D m_workerUL;
	ICoord2D m_workerLR;

	ICoord2D m_chatUL;
	ICoord2D m_chatLR;

	ICoord2D m_beaconUL;
	ICoord2D m_beaconLR;

	ICoord2D m_powerBarUL;
	ICoord2D m_powerBarLR;





	Image *m_expBarForeground;

	Image *m_commandMarkerImage;

	Image *m_powerPurchaseImage;

	typedef std::list< ControlBarSchemeImage* > ControlBarSchemeImageList;
	ControlBarSchemeImageList m_layer[MAX_CONTROL_BAR_SCHEME_IMAGE_LAYERS];

	typedef std::list< ControlBarSchemeAnimation* > ControlBarSchemeAnimationList;
	ControlBarSchemeAnimationList m_animations;

};


class ControlBarSchemeManager
{
public:
	ControlBarSchemeManager();
	~ControlBarSchemeManager();

	void init();						///< Initialize from the INI files
	void update();					///< move the animations if we have any
	void drawForeground( ICoord2D offset );	///< draw function to be called within a w3d draw procedure for the foreground
	void drawBackground( ICoord2D offset );	///< draw function to be called within a w3d draw procedure for the background

	void setControlBarSchemeByPlayer(Player *p);																				///< Based off the playerTemplate, pick the right scheme for the control bar
	void setControlBarSchemeByPlayerTemplate( const PlayerTemplate *pt, Bool useSmall = FALSE);
	void setControlBarScheme(AsciiString schemeName);																										///< SchemeName must be a valid INI entry

	// parse Functions for the INI file
	const FieldParse *getFieldParse() const { return m_controlBarSchemeFieldParseTable; }								///< returns the parsing fields
	static const FieldParse m_controlBarSchemeFieldParseTable[];																				///< the parse table
	static void parseImagePart( INI* ini, void *instance, void *store, const void *userData );					///< Parse the image part of the INI file
	static void parseAnimatingPart( INI* ini, void *instance, void *store, const void *userData );			///< Parse the animation part of the INI file
	static void parseAnimatingPartImage( INI* ini, void *instance, void *store, const void *userData );	///< parse the image part of the animation part :)

	ControlBarScheme *findControlBarScheme( AsciiString name ); ///< attempt to find the control bar scheme by it's name
	ControlBarScheme *newControlBarScheme( AsciiString name );	///< create a new control bar scheme and return it.

	void preloadAssets( TimeOfDay timeOfDay );									///< preload the assets

private:
	ControlBarScheme *m_currentScheme;													///< the current scheme that everythign uses
	Coord2D m_multiplier;

	typedef std::list< ControlBarScheme* > ControlBarSchemeList;			///< list of control bar schemes
	ControlBarSchemeList m_schemeList;

};

//-----------------------------------------------------------------------------
// INLINING ///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// EXTERNALS //////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
