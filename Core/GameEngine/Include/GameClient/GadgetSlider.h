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

// FILE: GadgetSlider.h ///////////////////////////////////////////////////////
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
// File name:  GadgetSlider.h
//
// Created:    Colin Day, June 2001
//
// Desc:       Helpful interface for Sliders
//
// Slider			 Sliders are constructed of 4 pieces, two end pieces,
//						 a repeating center, and a smaller repeating center pieces
//						 to fill small seams.  Since there are vertical sliders
//						 and horizontal sliders, the LEFT end of a horizontal
//						 slider is stored in the same place as the TOP end for a
//						 vertical slider.  The RIGHT end of a horizontal is
//						 also equivalent to the BOTTOM end of a vertical one.
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/Gadget.h"
#include "GameClient/Image.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////

// TYPE DEFINES ///////////////////////////////////////////////////////////////
enum{
HORIZONTAL_SLIDER_THUMB_POSITION = HORIZONTAL_SLIDER_THUMB_HEIGHT *2/3
};
// INLINING ///////////////////////////////////////////////////////////////////

inline void GadgetSliderGetMinMax( GameWindow *g, Int *min, Int *max )
{
	SliderData *sData = (SliderData *)g->winGetUserData();

	*max = sData->maxVal;
	*min = sData->minVal;

}

// GeneralsX @tweak felipebraz 20/06/2026 Add GadgetSliderSetMinMax to allow dynamically setting slider min/max bounds
inline void GadgetSliderSetMinMax( GameWindow *win, Int min, Int max )
{
	TheWindowManager->winSendSystemMsg( win,
																			GSM_SET_MIN_MAX,
																			min,
																			max );
}

inline GameWindow *GadgetSliderGetThumb( GameWindow *g ) { return g->winGetChild(); }

inline void GadgetSliderSetPosition( GameWindow *win, Int pos )
{
	TheWindowManager->winSendSystemMsg( win,
																			GSM_SET_SLIDER,
																			pos,
																			0 );
}

inline Int GadgetSliderGetPosition( GameWindow *win )
{
	SliderData *sData = (SliderData *)win->winGetUserData();
	if (sData)
	{
		return sData->position;
	}
	return -1;
}

// sliders are drawn from pieces, a left/top, right/bottom, repeating center, and small repeating center
inline void GadgetSliderSetEnabledImages( GameWindow *g, const Image *left, const Image *right, const Image *center, const Image *smallCenter )
{
	g->winSetEnabledImage( 0, left );
	g->winSetEnabledImage( 1, right );
	g->winSetEnabledImage( 2, center );
	g->winSetEnabledImage( 3, smallCenter );
}
inline void GadgetSliderSetEnabledImageLeft( GameWindow *g, const Image *left )			{ g->winSetEnabledImage( 0, left ); }
inline void GadgetSliderSetEnabledImageTop( GameWindow *g, const Image *left )			{ g->winSetEnabledImage( 0, left ); }
inline void GadgetSliderSetEnabledImageRight( GameWindow *g, const Image *right )		{ g->winSetEnabledImage( 1, right ); }
inline void GadgetSliderSetEnabledImageBottom( GameWindow *g, const Image *right )	{ g->winSetEnabledImage( 1, right ); }
inline void GadgetSliderSetEnabledImageCenter( GameWindow *g, const Image *center )	{ g->winSetEnabledImage( 2, center ); }
inline void GadgetSliderSetEnabledImageSmallCenter( GameWindow *g, const Image *smallCenter ) { g->winSetEnabledImage( 3, smallCenter ); }
inline void GadgetSliderSetEnabledColor( GameWindow *g, Color color )					{ g->winSetEnabledColor( 0, color ); }
inline void GadgetSliderSetEnabledBorderColor( GameWindow *g, Color color )		{ g->winSetEnabledBorderColor( 0, color ); }
inline const Image *GadgetSliderGetEnabledImageLeft( GameWindow *g )				{ return g->winGetEnabledImage( 0 ); }
inline const Image *GadgetSliderGetEnabledImageTop( GameWindow *g )					{ return g->winGetEnabledImage( 0 ); }
inline const Image *GadgetSliderGetEnabledImageRight( GameWindow *g )				{ return g->winGetEnabledImage( 1 ); }
inline const Image *GadgetSliderGetEnabledImageBottom( GameWindow *g )			{ return g->winGetEnabledImage( 1 ); }
inline const Image *GadgetSliderGetEnabledImageCenter( GameWindow *g )			{ return g->winGetEnabledImage( 2 ); }
inline const Image *GadgetSliderGetEnabledImageSmallCenter( GameWindow *g ) { return g->winGetEnabledImage( 3 ); }
inline Color	GadgetSliderGetEnabledColor( GameWindow *g )						{ return g->winGetEnabledColor( 0 ); }
inline Color	GadgetSliderGetEnabledBorderColor( GameWindow *g )			{ return g->winGetEnabledBorderColor( 0 ); }

// sliders are drawn from pieces, a left/top, right/bottom, repeating center, and small repeating center
inline void GadgetSliderSetDisabledImages( GameWindow *g, const Image *left, const Image *right, const Image *center, const Image *smallCenter )
{
	g->winSetDisabledImage( 0, left );
	g->winSetDisabledImage( 1, right );
	g->winSetDisabledImage( 2, center );
	g->winSetDisabledImage( 3, smallCenter );
}
inline void GadgetSliderSetDisabledImageLeft( GameWindow *g, const Image *left )			{ g->winSetDisabledImage( 0, left ); }
inline void GadgetSliderSetDisabledImageTop( GameWindow *g, const Image *left )				{ g->winSetDisabledImage( 0, left ); }
inline void GadgetSliderSetDisabledImageRight( GameWindow *g, const Image *right )		{ g->winSetDisabledImage( 1, right ); }
inline void GadgetSliderSetDisabledImageBottom( GameWindow *g, const Image *right )		{ g->winSetDisabledImage( 1, right ); }
inline void GadgetSliderSetDisabledImageCenter( GameWindow *g, const Image *center )	{ g->winSetDisabledImage( 2, center ); }
inline void GadgetSliderSetDisabledImageSmallCenter( GameWindow *g, const Image *smallCenter ) { g->winSetDisabledImage( 3, smallCenter ); }
inline void GadgetSliderSetDisabledColor( GameWindow *g, Color color )					{ g->winSetDisabledColor( 0, color ); }
inline void GadgetSliderSetDisabledBorderColor( GameWindow *g, Color color )		{ g->winSetDisabledBorderColor( 0, color ); }
inline const Image *GadgetSliderGetDisabledImageLeft( GameWindow *g )				{ return g->winGetDisabledImage( 0 ); }
inline const Image *GadgetSliderGetDisabledImageTop( GameWindow *g )				{ return g->winGetDisabledImage( 0 ); }
inline const Image *GadgetSliderGetDisabledImageRight( GameWindow *g )			{ return g->winGetDisabledImage( 1 ); }
inline const Image *GadgetSliderGetDisabledImageBottom( GameWindow *g )			{ return g->winGetDisabledImage( 1 ); }
inline const Image *GadgetSliderGetDisabledImageCenter( GameWindow *g )			{ return g->winGetDisabledImage( 2 ); }
inline const Image *GadgetSliderGetDisabledImageSmallCenter( GameWindow *g ){ return g->winGetDisabledImage( 3 ); }
inline Color	GadgetSliderGetDisabledColor( GameWindow *g )						{ return g->winGetDisabledColor( 0 ); }
inline Color	GadgetSliderGetDisabledBorderColor( GameWindow *g )			{ return g->winGetDisabledBorderColor( 0 ); }

// sliders are drawn from pieces, a left/top, right/bottom, repeating center, and small repeating center
inline void GadgetSliderSetHiliteImages( GameWindow *g, const Image *left, const Image *right, const Image *center, const Image *smallCenter )
{
	g->winSetHiliteImage( 0, left );
	g->winSetHiliteImage( 1, right );
	g->winSetHiliteImage( 2, center );
	g->winSetHiliteImage( 3, smallCenter );
}
inline void GadgetSliderSetHiliteImageLeft( GameWindow *g, const Image *left )			{ g->winSetHiliteImage( 0, left ); }
inline void GadgetSliderSetHiliteImageTop( GameWindow *g, const Image *left )				{ g->winSetHiliteImage( 0, left ); }
inline void GadgetSliderSetHiliteImageRight( GameWindow *g, const Image *right )		{ g->winSetHiliteImage( 1, right ); }
inline void GadgetSliderSetHiliteImageBottom( GameWindow *g, const Image *right )		{ g->winSetHiliteImage( 1, right ); }
inline void GadgetSliderSetHiliteImageCenter( GameWindow *g, const Image *center )	{ g->winSetHiliteImage( 2, center ); }
inline void GadgetSliderSetHiliteImageSmallCenter( GameWindow *g, const Image *smallCenter ) { g->winSetHiliteImage( 3, smallCenter ); }
inline void GadgetSliderSetHiliteColor( GameWindow *g, Color color )					{ g->winSetHiliteColor( 0, color ); }
inline void GadgetSliderSetHiliteBorderColor( GameWindow *g, Color color )		{ g->winSetHiliteBorderColor( 0, color ); }
inline const Image *GadgetSliderGetHiliteImageLeft( GameWindow *g )				{ return g->winGetHiliteImage( 0 ); }
inline const Image *GadgetSliderGetHiliteImageTop( GameWindow *g )				{ return g->winGetHiliteImage( 0 ); }
inline const Image *GadgetSliderGetHiliteImageRight( GameWindow *g )			{ return g->winGetHiliteImage( 1 ); }
inline const Image *GadgetSliderGetHiliteImageBottom( GameWindow *g )			{ return g->winGetHiliteImage( 1 ); }
inline const Image *GadgetSliderGetHiliteImageCenter( GameWindow *g )			{ return g->winGetHiliteImage( 2 ); }
inline const Image *GadgetSliderGetHiliteImageSmallCenter( GameWindow *g ){ return g->winGetHiliteImage( 3 ); }
inline Color	GadgetSliderGetHiliteColor( GameWindow *g )						{ return g->winGetHiliteColor( 0 ); }
inline Color	GadgetSliderGetHiliteBorderColor( GameWindow *g )			{ return g->winGetHiliteBorderColor( 0 ); }

///////////////////////////////////////////////////////////////////////////////
// Functions to set the images and colors for the slider thumb
///////////////////////////////////////////////////////////////////////////////

// enabled
inline void GadgetSliderSetEnabledThumbImage( GameWindow *g, const Image *image )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetEnabledImage( thumb, image );
																						}
inline void GadgetSliderSetEnabledThumbColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetEnabledColor( thumb, color );
																						}
inline void GadgetSliderSetEnabledThumbBorderColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetEnabledBorderColor( thumb, color );
																						}
inline void GadgetSliderSetEnabledSelectedThumbImage( GameWindow *g, const Image *image )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetEnabledSelectedImage( thumb, image );
																						}
inline void GadgetSliderSetEnabledSelectedThumbColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetEnabledSelectedColor( thumb, color );
																						}
inline void GadgetSliderSetEnabledSelectedThumbBorderColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetEnabledSelectedBorderColor( thumb, color );
																						}
inline const Image *GadgetSliderGetEnabledThumbImage( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetEnabledImage( thumb );
																							else
																								return nullptr;
																						}
inline Color GadgetSliderGetEnabledThumbColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetEnabledColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline Color GadgetSliderGetEnabledThumbBorderColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetEnabledBorderColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline const Image *GadgetSliderGetEnabledSelectedThumbImage( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetEnabledSelectedImage( thumb );
																							else
																								return nullptr;
																						}
inline Color GadgetSliderGetEnabledSelectedThumbColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetEnabledSelectedColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline Color GadgetSliderGetEnabledSelectedThumbBorderColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetEnabledSelectedBorderColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}


// disabled
inline void GadgetSliderSetDisabledThumbImage( GameWindow *g, const Image *image )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetDisabledImage( thumb, image );
																						}
inline void GadgetSliderSetDisabledThumbColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetDisabledColor( thumb, color );
																						}
inline void GadgetSliderSetDisabledThumbBorderColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetDisabledBorderColor( thumb, color );
																						}
inline void GadgetSliderSetDisabledSelectedThumbImage( GameWindow *g, const Image *image )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetDisabledSelectedImage( thumb, image );
																						}
inline void GadgetSliderSetDisabledSelectedThumbColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetDisabledSelectedColor( thumb, color );
																						}
inline void GadgetSliderSetDisabledSelectedThumbBorderColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetDisabledSelectedBorderColor( thumb, color );
																						}
inline const Image *GadgetSliderGetDisabledThumbImage( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetDisabledImage( thumb );
																							else
																								return nullptr;
																						}
inline Color GadgetSliderGetDisabledThumbColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetDisabledColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline Color GadgetSliderGetDisabledThumbBorderColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetDisabledBorderColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline const Image *GadgetSliderGetDisabledSelectedThumbImage( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetDisabledSelectedImage( thumb );
																							else
																								return nullptr;
																						}
inline Color GadgetSliderGetDisabledSelectedThumbColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetDisabledSelectedColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline Color GadgetSliderGetDisabledSelectedThumbBorderColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetDisabledSelectedBorderColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}

// hilite
inline void GadgetSliderSetHiliteThumbImage( GameWindow *g, const Image *image )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetHiliteImage( thumb, image );
																						}
inline void GadgetSliderSetHiliteThumbColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetHiliteColor( thumb, color );
																						}
inline void GadgetSliderSetHiliteThumbBorderColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetHiliteBorderColor( thumb, color );
																						}
inline void GadgetSliderSetHiliteSelectedThumbImage( GameWindow *g, const Image *image )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetHiliteSelectedImage( thumb, image );
																						}
inline void GadgetSliderSetHiliteSelectedThumbColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetHiliteSelectedColor( thumb, color );
																						}
inline void GadgetSliderSetHiliteSelectedThumbBorderColor( GameWindow *g, Color color )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								GadgetButtonSetHiliteSelectedBorderColor( thumb, color );
																						}
inline const Image *GadgetSliderGetHiliteThumbImage( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetHiliteImage( thumb );
																							else
																								return nullptr;
																						}
inline Color GadgetSliderGetHiliteThumbColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetHiliteColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline Color GadgetSliderGetHiliteThumbBorderColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetHiliteBorderColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline const Image *GadgetSliderGetHiliteSelectedThumbImage( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetHiliteSelectedImage( thumb );
																							else
																								return nullptr;
																						}
inline Color GadgetSliderGetHiliteSelectedThumbColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetHiliteSelectedColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}
inline Color GadgetSliderGetHiliteSelectedThumbBorderColor( GameWindow *g )
																						{
																							GameWindow *thumb = g->winGetChild();
																							if( thumb )
																								return GadgetButtonGetHiliteSelectedBorderColor( thumb );
																							else
																								return WIN_COLOR_UNDEFINED;
																						}

// EXTERNALS //////////////////////////////////////////////////////////////////
