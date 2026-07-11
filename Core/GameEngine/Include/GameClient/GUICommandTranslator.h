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

// FILE: GUICommandTranslator.h ///////////////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Translator for commands activated from the selection GUI, such as special unit
//				 actions, that require additional clicks in the world like selecting a target
//				 object or location
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "GameClient/InGameUI.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class GUICommandTranslator : public GameMessageTranslator
{

public:

	GUICommandTranslator();
	virtual ~GUICommandTranslator() override;

	virtual GameMessageDisposition translateGameMessage( const GameMessage *msg ) override;
};
