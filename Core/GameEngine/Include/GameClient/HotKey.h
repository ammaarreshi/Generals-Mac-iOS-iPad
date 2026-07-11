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


// FILE: HotKey.h /////////////////////////////////////////////////
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
//	Filename: 	HotKey.h
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
#include "Common/SubsystemInterface.h"
#include "Common/MessageStream.h"
//-----------------------------------------------------------------------------
// FORWARD REFERENCES /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
class AsciiString;
class GameWindow;
//-----------------------------------------------------------------------------
// TYPE DEFINES ///////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
class HotKeyTranslator : public GameMessageTranslator
{
public:
	virtual GameMessageDisposition translateGameMessage(const GameMessage *msg) override;
	virtual ~HotKeyTranslator() override { }
};

//-----------------------------------------------------------------------------
class HotKey
{
public:
	HotKey();
	GameWindow *m_win;
	AsciiString m_key;
	// we may need a checkmark system.
};

//-----------------------------------------------------------------------------
class HotKeyManager : public SubsystemInterface
{
public:
	HotKeyManager();
	virtual ~HotKeyManager() override;
	// Inherited from subsystem interface -----------------------------------------------------------
	virtual	void init() override;															///< Initialize the Hotkey system
	virtual void update() override {}														///< A No-op for us
	virtual void reset() override;															///< Reset
	//-----------------------------------------------------------------------------------------------

	void addHotKey( GameWindow *win, const AsciiString& key);
	Bool executeHotKey( const AsciiString& key); // called front eh HotKeyTranslator

	AsciiString searchHotKey( const AsciiString& label);
	AsciiString searchHotKey( const UnicodeString& uStr );

private:
	typedef std::map<AsciiString, HotKey> HotKeyMap;
	HotKeyMap m_hotKeyMap;
};
extern HotKeyManager *TheHotKeyManager;
//-----------------------------------------------------------------------------
// INLINING ///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// EXTERNALS //////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
