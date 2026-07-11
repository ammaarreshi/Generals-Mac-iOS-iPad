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

// FILE: MetaEvent.h ///////////////////////////////////////////////////////////
// Author: Steven Johnson, Dec 2001

#pragma once

#include "Common/SubsystemInterface.h"
#include "GameClient/InGameUI.h"


enum MappableKeyCategories CPP_11(: Int)
{
	CATEGORY_CONTROL = 0,
	CATEGORY_INFORMATION,
	CATEGORY_INTERFACE,
	CATEGORY_SELECTION,
	CATEGORY_TAUNT,
	CATEGORY_TEAM,
	CATEGORY_MISC,
	CATEGORY_DEBUG,

	CATEGORY_NUM_CATEGORIES
};

static const LookupListRec CategoryListName[] =
{
	{"CONTROL",						CATEGORY_CONTROL},
	{"INFORMATION",				CATEGORY_INFORMATION},
	{"INTERFACE",					CATEGORY_INTERFACE},
	{"SELECTION",					CATEGORY_SELECTION},
	{"TAUNT",							CATEGORY_TAUNT},
	{"TEAM",							CATEGORY_TEAM},
	{"MISC",							CATEGORY_MISC},
	{"DEBUG",							CATEGORY_DEBUG},
	{ nullptr, 0}
};


// -------------------------------------------------------------------------------
// the keys we allow to be mapped to Meta-events.
// note that this is a subset of all the keys available;
// in particular, "modifier" keys and keypad keys aren't
// available. Note that MappableKeyType is a SUBSET of
// KeyDefType; this is extremely important to maintain!
enum MappableKeyType CPP_11(: Int)
{
	// keypad keys ----------------------------------------------------------------
	MK_KP0					= KEY_KP0,
	MK_KP1					= KEY_KP1,
	MK_KP2					= KEY_KP2,
	MK_KP3					= KEY_KP3,
	MK_KP4					= KEY_KP4,
	MK_KP5					= KEY_KP5,
	MK_KP6					= KEY_KP6,
	MK_KP7					= KEY_KP7,
	MK_KP8					= KEY_KP8,
	MK_KP9					= KEY_KP9,
	MK_KPDEL				= KEY_KPDEL,
	MK_KPSTAR				= KEY_KPSTAR,
	MK_KPMINUS			= KEY_KPMINUS,
	MK_KPPLUS				= KEY_KPPLUS,
	MK_KPENTER			= KEY_KPENTER,
	MK_KPSLASH			= KEY_KPSLASH,

	MK_ESC					= KEY_ESC,
	MK_BACKSPACE		= KEY_BACKSPACE,
	MK_ENTER				= KEY_ENTER,
	MK_SPACE				= KEY_SPACE,
	MK_TAB					= KEY_TAB,
	MK_F1						= KEY_F1,
	MK_F2						= KEY_F2,
	MK_F3						= KEY_F3,
	MK_F4						= KEY_F4,
	MK_F5						= KEY_F5,
	MK_F6						= KEY_F6,
	MK_F7						= KEY_F7,
	MK_F8						= KEY_F8,
	MK_F9						= KEY_F9,
	MK_F10					= KEY_F10,
	MK_F11					= KEY_F11,
	MK_F12					= KEY_F12,
	MK_A						= KEY_A,
	MK_B						= KEY_B,
	MK_C						= KEY_C,
	MK_D						= KEY_D,
	MK_E						= KEY_E,
	MK_F						= KEY_F,
	MK_G						= KEY_G,
	MK_H						= KEY_H,
	MK_I						= KEY_I,
	MK_J						= KEY_J,
	MK_K						= KEY_K,
	MK_L						= KEY_L,
	MK_M						= KEY_M,
	MK_N						= KEY_N,
	MK_O						= KEY_O,
	MK_P						= KEY_P,
	MK_Q						= KEY_Q,
	MK_R						= KEY_R,
	MK_S						= KEY_S,
	MK_T						= KEY_T,
	MK_U						= KEY_U,
	MK_V						= KEY_V,
	MK_W						= KEY_W,
	MK_X						= KEY_X,
	MK_Y						= KEY_Y,
	MK_Z						= KEY_Z,
	MK_1						= KEY_1,
	MK_2						= KEY_2,
	MK_3						= KEY_3,
	MK_4						= KEY_4,
	MK_5						= KEY_5,
	MK_6						= KEY_6,
	MK_7						= KEY_7,
	MK_8						= KEY_8,
	MK_9						= KEY_9,
	MK_0						= KEY_0,
	MK_MINUS				= KEY_MINUS,
	MK_EQUAL				= KEY_EQUAL,
	MK_LBRACKET			= KEY_LBRACKET,
	MK_RBRACKET			= KEY_RBRACKET,
	MK_SEMICOLON		= KEY_SEMICOLON,
	MK_APOSTROPHE		= KEY_APOSTROPHE,
	MK_TICK					= KEY_TICK,
	MK_BACKSLASH		= KEY_BACKSLASH,
	MK_COMMA				= KEY_COMMA,
	MK_PERIOD				= KEY_PERIOD,
	MK_SLASH				= KEY_SLASH,
	MK_UP						= KEY_UP,
	MK_DOWN					= KEY_DOWN,
	MK_LEFT					= KEY_LEFT,
	MK_RIGHT				= KEY_RIGHT,
	MK_HOME					= KEY_HOME,
	MK_END					= KEY_END,
	MK_PGUP					= KEY_PGUP,
	MK_PGDN					= KEY_PGDN,
	MK_INS					= KEY_INS,
	MK_DEL					= KEY_DEL,
	MK_NONE					= KEY_NONE

};

static const LookupListRec KeyNames[] =
{
	// keypad keys ----------------------------------------------------------------
	{ "KEY_KP0", MK_KP0 },
	{ "KEY_KP1", MK_KP1 },
	{ "KEY_KP2", MK_KP2 },
	{ "KEY_KP3", MK_KP3 },
	{ "KEY_KP4", MK_KP4 },
	{ "KEY_KP5", MK_KP5 },
	{ "KEY_KP6", MK_KP6 },
	{ "KEY_KP7", MK_KP7 },
	{ "KEY_KP8", MK_KP8 },
	{ "KEY_KP9", MK_KP9 },
	{ "KEY_KPDEL", MK_KPDEL },
	{ "KEY_KPSTAR", MK_KPSTAR },
	{ "KEY_KPMINUS", MK_KPMINUS },
	{ "KEY_KPPLUS", MK_KPPLUS },
	{ "KEY_KPENTER", MK_KPENTER },
	{ "KEY_KPSLASH", MK_KPSLASH },

	{ "KEY_ESC", MK_ESC },
	{ "KEY_BACKSPACE", MK_BACKSPACE },
	{ "KEY_ENTER", MK_ENTER },
	{ "KEY_SPACE", MK_SPACE },
	{ "KEY_TAB", MK_TAB },
	{ "KEY_F1", MK_F1 },
	{ "KEY_F2", MK_F2 },
	{ "KEY_F3", MK_F3 },
	{ "KEY_F4", MK_F4 },
	{ "KEY_F5", MK_F5 },
	{ "KEY_F6", MK_F6 },
	{ "KEY_F7", MK_F7 },
	{ "KEY_F8", MK_F8 },
	{ "KEY_F9", MK_F9 },
	{ "KEY_F10", MK_F10 },
	{ "KEY_F11", MK_F11 },
	{ "KEY_F12", MK_F12 },
	{ "KEY_A", MK_A },
	{ "KEY_B", MK_B },
	{ "KEY_C", MK_C },
	{ "KEY_D", MK_D },
	{ "KEY_E", MK_E },
	{ "KEY_F", MK_F },
	{ "KEY_G", MK_G },
	{ "KEY_H", MK_H },
	{ "KEY_I", MK_I },
	{ "KEY_J", MK_J },
	{ "KEY_K", MK_K },
	{ "KEY_L", MK_L },
	{ "KEY_M", MK_M },
	{ "KEY_N", MK_N },
	{ "KEY_O", MK_O },
	{ "KEY_P", MK_P },
	{ "KEY_Q", MK_Q },
	{ "KEY_R", MK_R },
	{ "KEY_S", MK_S },
	{ "KEY_T", MK_T },
	{ "KEY_U", MK_U },
	{ "KEY_V", MK_V },
	{ "KEY_W", MK_W },
	{ "KEY_X", MK_X },
	{ "KEY_Y", MK_Y },
	{ "KEY_Z", MK_Z },
	{ "KEY_1", MK_1 },
	{ "KEY_2", MK_2 },
	{ "KEY_3", MK_3 },
	{ "KEY_4", MK_4 },
	{ "KEY_5", MK_5 },
	{ "KEY_6", MK_6 },
	{ "KEY_7", MK_7 },
	{ "KEY_8", MK_8 },
	{ "KEY_9", MK_9 },
	{ "KEY_0", MK_0 },
	{ "KEY_MINUS", MK_MINUS },
	{ "KEY_EQUAL", MK_EQUAL },
	{ "KEY_LBRACKET", MK_LBRACKET },
	{ "KEY_RBRACKET", MK_RBRACKET },
	{ "KEY_SEMICOLON", MK_SEMICOLON },
	{ "KEY_APOSTROPHE", MK_APOSTROPHE },
	{ "KEY_TICK", MK_TICK },
	{ "KEY_BACKSLASH", MK_BACKSLASH },
	{ "KEY_COMMA", MK_COMMA },
	{ "KEY_PERIOD", MK_PERIOD },
	{ "KEY_SLASH", MK_SLASH },
	{ "KEY_UP", MK_UP },
	{ "KEY_DOWN", MK_DOWN },
	{ "KEY_LEFT", MK_LEFT },
	{ "KEY_RIGHT", MK_RIGHT },
	{ "KEY_HOME", MK_HOME },
	{ "KEY_END", MK_END },
	{ "KEY_PGUP", MK_PGUP },
	{ "KEY_PGDN", MK_PGDN },
	{ "KEY_INS", MK_INS },
	{ "KEY_DEL", MK_DEL },
	{ "KEY_NONE", MK_NONE },
	{ nullptr, 0	}
};

// -------------------------------------------------------------------------------
enum MappableKeyTransition CPP_11(: Int)
{
	DOWN,
	UP,
	DOUBLEDOWN,	// if a key transition is repeated immediately, we generate this.

	MAPPABLE_KEY_TRANSITION_COUNT
};

static const LookupListRec TransitionNames[] =
{
	{ "DOWN",				DOWN },
	{ "UP",					UP },
	{ "DOUBLEDOWN",	DOUBLEDOWN },
	{ nullptr, 0	}
};
static_assert(ARRAY_SIZE(TransitionNames) == MAPPABLE_KEY_TRANSITION_COUNT + 1, "Incorrect array size");

// -------------------------------------------------------------------------------
// an easier-to-type subset of the KEY_STATE stuff.
enum MappableKeyModState CPP_11(: Int)
{
	NONE							= 0,
	CTRL							= KEY_STATE_LCONTROL,
	ALT								= KEY_STATE_LALT,
	SHIFT							= KEY_STATE_LSHIFT,
	CTRL_ALT					= CTRL|ALT,
	SHIFT_CTRL				= SHIFT|CTRL,
	SHIFT_ALT					= SHIFT|ALT,
	SHIFT_ALT_CTRL		= SHIFT|ALT|CTRL
};

static const LookupListRec ModifierNames[] =
{
	{ "NONE",							NONE },
	{ "CTRL",							CTRL },
	{ "ALT",							ALT },
	{ "SHIFT",						SHIFT },
	{ "CTRL_ALT",					CTRL_ALT },
	{ "SHIFT_CTRL",				SHIFT_CTRL },
	{ "SHIFT_ALT",				SHIFT_ALT },
	{ "SHIFT_ALT_CTRL" ,	SHIFT_ALT_CTRL },
	{ nullptr, 0	}
};



// -------------------------------------------------------------------------------
// CommandUsableInType sets in what state the commands are allowed.
enum CommandUsableInType CPP_11(: Int)
{
	COMMANDUSABLE_NONE				= 0,

	COMMANDUSABLE_SHELL				= (1 << 0), // Command is usable when in Shell (Menus)
	COMMANDUSABLE_GAME				= (1 << 1), // Command is usable when not in Shell
	COMMANDUSABLE_OBSERVER		= (1 << 2), // TheSuperHackers @feature Command is usable when observing

	COMMANDUSABLE_EVERYWHERE = ~0,
};

static const char* const TheCommandUsableInNames[] =
{
	"SHELL",
	"GAME",
	"OBSERVER",

	nullptr
};

// -------------------------------------------------------------------------------
class MetaMapRec : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(MetaMapRec, "MetaMapRec")
public:
	MetaMapRec*							m_next;
	GameMessage::Type				m_meta;						///< the meta-event to emit
	MappableKeyType					m_key;						///< the key we want
	MappableKeyTransition		m_transition;			///< the state of the key
	MappableKeyModState			m_modState;				///< the required state of the ctrl-alt-shift keys
	CommandUsableInType			m_usableIn;				///< the allowed place the command can be used in
	// Next fields are added for Key mapping Dialog
	MappableKeyCategories		m_category;				///< This is the category the key falls under
	UnicodeString						m_description;		///< The description string for the keys
	UnicodeString						m_displayName;		///< The display name of our command
};
EMPTY_DTOR(MetaMapRec)


//-----------------------------------------------------------------------------
class MetaEventTranslator : public GameMessageTranslator
{
private:

	Int						m_lastKeyDown;	// really a MappableKeyType
	Int						m_lastModState;	// really a MappableKeyModState

	enum { NUM_MOUSE_BUTTONS = 3 };
	ICoord2D m_mouseDownPosition[NUM_MOUSE_BUTTONS];
	Bool	m_nextUpShouldCreateDoubleClick[NUM_MOUSE_BUTTONS];

public:
	MetaEventTranslator();
	virtual ~MetaEventTranslator() override;
	virtual GameMessageDisposition translateGameMessage(const GameMessage *msg) override;

private:
	void onMouseEvent(const GameMessage *msg);
	void onKeyEvent(const GameMessage *msg, GameMessageDisposition &disp);
};

//-----------------------------------------------------------------------------
class MetaMap : public SubsystemInterface
{
	friend class MetaEventTranslator;

private:
	MetaMapRec *m_metaMaps;

protected:
	GameMessage::Type findGameMessageMetaType(const char* name);
	MetaMapRec *getMetaMapRec(GameMessage::Type t);

public:

	MetaMap();
	virtual ~MetaMap() override;

	virtual void init() override { }
	virtual void reset() override { }
	virtual void update() override { }

	static void parseMetaMap(INI* ini);

	// TheSuperHackers @feature Function to generate default key mappings
	// for actions that were not found in a CommandMap.ini
	void generateMetaMap();

	void verifyMetaMap();

	const MetaMapRec *getFirstMetaMapRec() const { return m_metaMaps; }
};

extern MetaMap *TheMetaMap;
