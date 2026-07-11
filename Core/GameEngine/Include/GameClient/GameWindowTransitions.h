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

// FILE: GameWindowTransitions.h /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Dec 2002
//
//	Filename: 	GameWindowTransitions.h
//
//	author:		Chris Huybregts
//
//	purpose:
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameCommon.h"

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// FORWARD REFERENCES /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
class GameWindow;
class Image;
class DisplayString;
//-----------------------------------------------------------------------------
// TYPE DEFINES ///////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

enum
{
TRANSITION_FLASH = 0,
BUTTON_TRANSITION_FLASH,
WIN_FADE_TRANSITION,
WIN_SCALE_UP_TRANSITION,
MAINMENU_SCALE_UP_TRANSITION,
TEXT_TYPE_TRANSITION,
SCREEN_FADE_TRANSITION,
COUNT_UP_TRANSITION,
FULL_FADE_TRANSITION,
TEXT_ON_FRAME_TRANSITION,
MAINMENU_MEDIUM_SCALE_UP_TRANSITION,
MAINMENU_SMALL_SCALE_DOWN_TRANSITION,
CONTROL_BAR_ARROW_TRANSITION,
SCORE_SCALE_UP_TRANSITION,
REVERSE_SOUND_TRANSITION,

MAX_TRANSITION_WINDOW_STYLES
};

static const LookupListRec TransitionStyleNames[] =
{
	{ "FLASH",					TRANSITION_FLASH },
	{ "BUTTONFLASH",		BUTTON_TRANSITION_FLASH },
	{ "WINFADE",				WIN_FADE_TRANSITION },
	{ "WINSCALEUP",			WIN_SCALE_UP_TRANSITION },
	{ "MAINMENUSCALEUP",			MAINMENU_SCALE_UP_TRANSITION },
	{ "TYPETEXT",			TEXT_TYPE_TRANSITION },
	{ "SCREENFADE",			SCREEN_FADE_TRANSITION },
	{ "COUNTUP",			COUNT_UP_TRANSITION },
	{ "FULLFADE",			FULL_FADE_TRANSITION },
	{ "TEXTONFRAME",			TEXT_ON_FRAME_TRANSITION },
	{ "MAINMENUMEDIUMSCALEUP",			MAINMENU_MEDIUM_SCALE_UP_TRANSITION },
	{ "MAINMENUSMALLSCALEDOWN",			MAINMENU_SMALL_SCALE_DOWN_TRANSITION },
	{ "CONTROLBARARROW",			CONTROL_BAR_ARROW_TRANSITION },
	{ "SCORESCALEUP",			SCORE_SCALE_UP_TRANSITION },
	{ "REVERSESOUND",			REVERSE_SOUND_TRANSITION },
	{ nullptr, 0	}
};
static_assert(ARRAY_SIZE(TransitionStyleNames) == MAX_TRANSITION_WINDOW_STYLES + 1, "Incorrect array size");

// base class for the transitions
// inherit off of this adding in all the values
class Transition
{
public:
	Transition ();
	virtual ~Transition();

	virtual void init( GameWindow *win ) = 0;
	virtual void update( Int frame ) = 0;
	virtual void reverse() = 0;
	virtual void draw() = 0;

	virtual void skip() = 0;

	void unlinkGameWindow(GameWindow* win) { if ( m_win == win ) m_win = nullptr; }
	Bool isFinished() { return m_isFinished;	}
	Int getFrameLength(){ return m_frameLength;	}
protected:

	Int m_frameLength;	// how many frames does this thing take.
	Bool m_isFinished;  // when we finish we set this
	Bool m_isForward;
	Bool m_isReversed;  // when we reverse we set this
	GameWindow *m_win;
};

//-----------------------------------------------------------------------------
class TextOnFrameTransition : public Transition
{
public:
	TextOnFrameTransition ();
	virtual ~TextOnFrameTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	TEXTONFRAMETRANSITION_START = 0,
	TEXTONFRAMETRANSITION_END	 = 1		// Max text type we'll allow.
	};

};

//-----------------------------------------------------------------------------
class ReverseSoundTransition : public Transition
{
public:
	ReverseSoundTransition ();
	virtual ~ReverseSoundTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	REVERSESOUNDTRANSITION_START = 0,
	REVERSESOUNDTRANSITION_FIRESOUND = 1,
	REVERSESOUNDTRANSITION_END	 = 2		// Max text type we'll allow.
	};

};

//-----------------------------------------------------------------------------
class FullFadeTransition : public Transition
{
public:
	FullFadeTransition ();
	virtual ~FullFadeTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	FULLFADETRANSITION_START = 0,
	FULLFADETRANSITION_END	 = 10		// Max text type we'll allow.
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Real m_percent;
	Int m_drawState;

};
//-----------------------------------------------------------------------------
class ControlBarArrowTransition : public Transition
{
public:
	ControlBarArrowTransition ();
	virtual ~ControlBarArrowTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	CONTROLBARARROWTRANSITION_START = 0,
	CONTROLBARARROWTRANSITION_BEGIN_FADE = 16,
	CONTROLBARARROWTRANSITION_END	 = 22		// Max text type we'll allow.
	};
	ICoord2D m_pos;
	ICoord2D m_incrementPos;
	ICoord2D m_size;
	Real m_percent;
	Real m_fadePercent;
	Int m_drawState;
	const Image* m_arrowImage;

};
//-----------------------------------------------------------------------------
class ScreenFadeTransition : public Transition
{
public:
	ScreenFadeTransition ();
	virtual ~ScreenFadeTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	SCREENFADETRANSITION_START = 0,
	SCREENFADETRANSITION_END	 = 30		// Max text type we'll allow.
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Real m_percent;
	Int m_drawState;

};
//-----------------------------------------------------------------------------
class CountUpTransition : public Transition
{
public:
	CountUpTransition ();
	virtual ~CountUpTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	COUNTUPTRANSITION_START = 0,
	COUNTUPTRANSITION_END	 = 30		// Max text type we'll allow.
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
	UnicodeString m_fullText;
	UnicodeString m_partialText;
	Int m_intValue;
	Int m_currentValue;
	enum{
		COUNT_ONES = 1,
		COUNT_100S = 100,
		COUNT_1000S = 1000
	};
	Int m_countState;
};

//-----------------------------------------------------------------------------
class TextTypeTransition : public Transition
{
public:
	TextTypeTransition ();
	virtual ~TextTypeTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	TEXTTYPETRANSITION_START = 0,
	TEXTTYPETRANSITION_END	 = 30		// Max text type we'll allow.
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
	UnicodeString m_fullText;
	UnicodeString m_partialText;
	DisplayString *m_dStr;
};

//-----------------------------------------------------------------------------
class MainMenuScaleUpTransition : public Transition
{
public:
	MainMenuScaleUpTransition ();
	virtual ~MainMenuScaleUpTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	MAINMENUSCALEUPTRANSITION_START = 0,
	MAINMENUSCALEUPTRANSITION_END = 5
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
	ICoord2D m_growPos;
	ICoord2D m_growSize;
	ICoord2D m_incrementPos;
	ICoord2D m_incrementSize;
	GameWindow *m_growWin;
};


//-----------------------------------------------------------------------------
class MainMenuMediumScaleUpTransition : public Transition
{
public:
	MainMenuMediumScaleUpTransition ();
	virtual ~MainMenuMediumScaleUpTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	MAINMENUMEDIUMSCALEUPTRANSITION_START = 0,
	MAINMENUMEDIUMSCALEUPTRANSITION_END = 3
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
	ICoord2D m_growPos;
	ICoord2D m_growSize;
	ICoord2D m_incrementSize;
	GameWindow *m_growWin;
};

//-----------------------------------------------------------------------------
class MainMenuSmallScaleDownTransition : public Transition
{
public:
	MainMenuSmallScaleDownTransition ();
	virtual ~MainMenuSmallScaleDownTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	MAINMENUSMALLSCALEDOWNTRANSITION_START = 0,
	MAINMENUSMALLSCALEDOWNTRANSITION_1 = 1,
	MAINMENUSMALLSCALEDOWNTRANSITION_2 = 2,
	MAINMENUSMALLSCALEDOWNTRANSITION_3 = 3,
	MAINMENUSMALLSCALEDOWNTRANSITION_4 = 4,
	MAINMENUSMALLSCALEDOWNTRANSITION_5 = 5,
	MAINMENUSMALLSCALEDOWNTRANSITION_END
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
	ICoord2D m_growPos;
	ICoord2D m_growSize;
	ICoord2D m_incrementSize;
	GameWindow *m_growWin;
};

//-----------------------------------------------------------------------------
class ScaleUpTransition : public Transition
{
public:
	ScaleUpTransition ();
	virtual ~ScaleUpTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	SCALEUPTRANSITION_START = 0,
	SCALEUPTRANSITION_1 = 1,
	SCALEUPTRANSITION_2 = 2,
	SCALEUPTRANSITION_3 = 3,
	SCALEUPTRANSITION_4 = 4,
	SCALEUPTRANSITION_5 = 5,
//	SCALEUPTRANSITION_6 = 6,
//	SCALEUPTRANSITION_7 = 7,
//	SCALEUPTRANSITION_8 = 8,
//	SCALEUPTRANSITION_9 = 9,
//	SCALEUPTRANSITION_10 = 10,
//	SCALEUPTRANSITION_11 = 11,
//	SCALEUPTRANSITION_12 = 12,
//	SCALEUPTRANSITION_13 = 13,
//	SCALEUPTRANSITION_14 = 14,
//	SCALEUPTRANSITION_15 = 15,
//	SCALEUPTRANSITION_16 = 16,
//	SCALEUPTRANSITION_17 = 17,
//	SCALEUPTRANSITION_18 = 18,
//	SCALEUPTRANSITION_19 = 19,
	SCALEUPTRANSITION_END
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
	ICoord2D m_centerPos;
	ICoord2D m_incrementSize;
};

//-----------------------------------------------------------------------------
class ScoreScaleUpTransition : public Transition
{
public:
	ScoreScaleUpTransition ();
	virtual ~ScoreScaleUpTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	SCORESCALEUPTRANSITION_START = 0,
	SCORESCALEUPTRANSITION_1 = 1,
	SCORESCALEUPTRANSITION_2 = 2,
	SCORESCALEUPTRANSITION_3 = 3,
	SCORESCALEUPTRANSITION_4 = 4,
	SCORESCALEUPTRANSITION_5 = 5,

	SCORESCALEUPTRANSITION_END
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
	ICoord2D m_centerPos;
	ICoord2D m_incrementSize;
};

//-----------------------------------------------------------------------------


class FadeTransition : public Transition
{
public:
	FadeTransition ();
	virtual ~FadeTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	FADETRANSITION_START = 0,
	FADETRANSITION_FADE_IN_1 = 1,
	FADETRANSITION_FADE_IN_2 = 2,
	FADETRANSITION_FADE_IN_3 = 3,
	FADETRANSITION_FADE_IN_4 = 4,
	FADETRANSITION_FADE_IN_5 = 5,
	FADETRANSITION_FADE_IN_6 = 6,
	FADETRANSITION_FADE_IN_7 = 7,
	FADETRANSITION_FADE_IN_8 = 8,
	FADETRANSITION_FADE_IN_9 = 9,
	FADETRANSITION_END
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
};


//-----------------------------------------------------------------------------

class FlashTransition : public Transition
{
public:
	FlashTransition ();
	virtual ~FlashTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	FLASHTRANSITION_START = 0,
	FLASHTRANSITION_FADE_IN_1 = 1,
	FLASHTRANSITION_FADE_IN_2 = 2,
	FLASHTRANSITION_FADE_IN_3 = 3,
	FLASHTRANSITION_FADE_TO_BACKGROUND_1 = 4,
	FLASHTRANSITION_FADE_TO_BACKGROUND_2 = 5,
	FLASHTRANSITION_FADE_TO_BACKGROUND_3 = 6,
	FLASHTRANSITION_FADE_TO_BACKGROUND_4 = 7,
	FLASHTRANSITION_END
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
};

//-----------------------------------------------------------------------------
class ButtonFlashTransition : public Transition
{
public:
	ButtonFlashTransition ();
	virtual ~ButtonFlashTransition() override;

	virtual void init( GameWindow *win ) override;
	virtual void update( Int frame ) override;
	virtual void reverse() override;
	virtual void draw() override;

	virtual void skip() override;

protected:
	enum{
	BUTTONFLASHTRANSITION_START = 0,
	BUTTONFLASHTRANSITION_FADE_IN_1 = 1,
	BUTTONFLASHTRANSITION_FADE_IN_2 = 2,
	BUTTONFLASHTRANSITION_FADE_IN_3 = 3,
	BUTTONFLASHTRANSITION_FADE_TO_BACKGROUND_1 =4,
	BUTTONFLASHTRANSITION_FADE_TO_BACKGROUND_2 = 5,
	BUTTONFLASHTRANSITION_FADE_TO_BACKGROUND_3 = 6,
	BUTTONFLASHTRANSITION_FADE_TO_BACKGROUND_4 = 7,
	BUTTONFLASHTRANSITION_FADE_TO_GRADE_IN_1 = 11,
	BUTTONFLASHTRANSITION_FADE_TO_GRADE_IN_2 = BUTTONFLASHTRANSITION_FADE_TO_GRADE_IN_1 +1,
	BUTTONFLASHTRANSITION_FADE_TO_GRADE_OUT_1 = BUTTONFLASHTRANSITION_FADE_TO_GRADE_IN_2 +1,
	BUTTONFLASHTRANSITION_FADE_TO_GRADE_OUT_2 = BUTTONFLASHTRANSITION_FADE_TO_GRADE_OUT_1 +1,
	BUTTONFLASHTRANSITION_FADE_TO_GRADE_OUT_3 = BUTTONFLASHTRANSITION_FADE_TO_GRADE_OUT_2 +1,
	BUTTONFLASHTRANSITION_FADE_TO_GRADE_OUT_4 = BUTTONFLASHTRANSITION_FADE_TO_GRADE_OUT_3 + 1,
	BUTTONFLASHTRANSITION_END, // this is the end of the sequence, we need some special defines... well put them below here
	BUTTONFLASHTRANSITION_SHOW_BACKGROUND
	};
	ICoord2D m_pos;
	ICoord2D m_size;
	Int m_drawState;
	Image *m_gradient;
};

//-----------------------------------------------------------------------------
class TransitionWindow
{
public:
	TransitionWindow();
	~TransitionWindow();

	Bool init();
	void update( Int frame );
	Bool isFinished();
	void reverse( Int totalFrames );
	Int  getTotalFrames();
	void skip();
	void draw();

	void unlinkGameWindow( GameWindow* win );

// INI parsed vars
	AsciiString m_winName;
	Int m_frameDelay;					// what frame number we start on
	Int m_style;							// the style we'll be using

// standard vars
	NameKeyType	m_winID;
	GameWindow *m_win;
	Transition *m_transition; // each window is allowed one transition
	Int m_currentFrameDelay;	// this will change based on if we're going forward or backwards
};

//-----------------------------------------------------------------------------
class TransitionGroup
{
public:
	TransitionGroup();
	~TransitionGroup();

	void init();
	void update();
	Bool isFinished();
	void reverse();
	void draw();

	void skip ();
	AsciiString getName() { return m_name; }
	void setName( AsciiString name){ m_name = name;	}
	void addWindow( TransitionWindow *transWin );
	Bool isReversed();
	Bool isFireOnce() { return m_fireOnce; }
	Bool m_fireOnce;
private:
	typedef std::list<TransitionWindow *> TransitionWindowList;
	TransitionWindowList m_transitionWindowList;
	Int m_directionMultiplier;
	Real m_currentFrame; ///< maintain how long we've spent on this transition (in 30fps-equivalent frames);
	AsciiString m_name;
};

//-----------------------------------------------------------------------------
class GameWindowTransitionsHandler: public SubsystemInterface
{
public:
	GameWindowTransitionsHandler();
	virtual ~GameWindowTransitionsHandler() override;

	virtual void init() override;
	void load();
	virtual void reset() override;
	virtual void update() override;
	virtual void draw() override;
	Bool isFinished();
	const FieldParse *getFieldParse() const { return m_gameWindowTransitionsFieldParseTable; }								///< returns the parsing fields
	static const FieldParse m_gameWindowTransitionsFieldParseTable[];																				///< the parse table
	static void parseWindow( INI* ini, void *instance, void *store, const void *userData );

	void setGroup(AsciiString groupName, Bool immediate = FALSE);		// THis will be the next group to fire off.
	void reverse( AsciiString groupName );// reverse the animations for the current group.
	void remove( AsciiString groupName, Bool skipPending = FALSE );// remove the animation from the current or pending groups.
	TransitionGroup *getNewGroup( AsciiString name );
private:
	TransitionGroup *findGroup( AsciiString groupName );
	typedef std::list<TransitionGroup *> TransitionGroupList;
	TransitionGroupList m_transitionGroupList;
	TransitionGroup *m_currentGroup;
	TransitionGroup *m_pendingGroup;
	TransitionGroup *m_drawGroup;
	TransitionGroup *m_secondaryDrawGroup; // needed to draw the last frame of the previous draw group once more.
};

void PushButtonImageDrawThree(GameWindow *window, Int alpha );


//-----------------------------------------------------------------------------
// INLINING ///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// EXTERNALS //////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
extern GameWindowTransitionsHandler *TheTransitionHandler;
