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

// FILE: .cpp /////////////////////////////////////////////////////////////////////////////////////
// Author: Colin Day
// Description: Game Client message dispatcher
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/MessageStream.h"
#include "GameClient/GameClient.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/** The Client message dispatcher, this is the last "translator" on the message
	* stream before the messages go to the network for processing.  It gives
	* the client itself the opportunity to respond to any messages on the stream
	* or create new ones to pass along to the network and logic */
GameMessageDisposition GameClientMessageDispatcher::translateGameMessage(const GameMessage *msg)
{
	if (msg->getType() >= GameMessage::MSG_BEGIN_NETWORK_MESSAGES && msg->getType() <= GameMessage::MSG_END_NETWORK_MESSAGES)
		return KEEP_MESSAGE;
	if (msg->getType() == GameMessage::MSG_NEW_GAME || msg->getType() == GameMessage::MSG_CLEAR_GAME_DATA)
		return KEEP_MESSAGE;

	if (msg->getType() == GameMessage::MSG_FRAME_TICK)
		return KEEP_MESSAGE;

	//DEBUG_LOG(("GameClientMessageDispatcher::translateGameMessage() - eating a %s on frame %d",
		//((GameMessage *)msg)->getCommandAsString(), TheGameClient->getFrame()));

	return DESTROY_MESSAGE;
}
