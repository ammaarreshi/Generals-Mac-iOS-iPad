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

// FILE: PlayerList.h ////////////////////////////////////////////////////////////
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
// File name:  PlayerList.h
//
// Created:    Steven Johnson, October 2001
//
// Desc:			 @todo
//
//-----------------------------------------------------------------------------

#pragma once

#include "Common/SubsystemInterface.h"
#include "Common/GameCommon.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Snapshot.h"

class DataChunkInput;
struct DataChunkInfo;
class DataChunkOutput;
class Player;
class Team;
class TeamFactory;

//-------------------------------------------------------------------------------------------------
enum AllowPlayerRelationship CPP_11(: Int)
{
	ALLOW_SAME_PLAYER			= 0x01,		///< allow only objects of the same player as m_obj
	ALLOW_ALLIES					= 0x02,		///< allow objects that m_obj considers allies
	ALLOW_ENEMIES					= 0x04,		///< allow objects that m_obj considers enemy
	ALLOW_NEUTRAL					= 0x08		///< allow objects that m_obj considers neutral
};


//-------------------------------------------------------------------------------------------------
/**
	This is a singleton class that maintains the list of Players.
*/
class PlayerList : public SubsystemInterface,
									 public Snapshot
{

public:

	PlayerList();
	virtual ~PlayerList() override;

	// subsystem methods
	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;

	virtual void newGame(); // called during GameLogic::startNewGame()
	virtual void newMap();	 // Called after a new map is loaded.

	void teamAboutToBeDeleted(Team* team);

	/**
		return the total number of players (including the neutral player).
	*/
	inline Int getPlayerCount() { return m_playerCount; }

	/**
		return the nth player. Note that players are in an arbitrary order.
		you should generally only use this if you want to iterate thru
		all players, NOT to get a specific player!
	*/
	Player *getNthPlayer(Int i);

	/**
		return the "neutral" Player. there is always a player that is "neutral" wrt
		all other players (this is so that everything can be associated with a nonnull
		Player, to simplify the universe). This will never return null.
	*/
	Player *getNeutralPlayer() { DEBUG_ASSERTCRASH(m_players[0] != nullptr, ("null neutral")); return m_players[0]; }

	/**
		return the Player with the given internal name, or null if none found.
	*/
	Player *findPlayerWithNameKey(NameKeyType key);

	/**
		// GeneralsX @bugfix felipebraz 21/02/2026 Find player by ASCII name string.
		// Used as a fallback when the nameKey is stale (e.g. skirmish-qualified player names).
	*/
	Player *findPlayerWithName(const AsciiString& name);

	/**
		Return the "local" player (ie, the human playing the game).
		This will never return null.
	*/
	inline Player *getLocalPlayer() { DEBUG_ASSERTCRASH(m_local != nullptr, ("null m_local")); return m_local; }

	/**
		Set the local player. You cannot set it to null; if you pass null, you'll
		end up setting the local player to be the neutral player.
	*/
	void setLocalPlayer(Player *player);

	/**
		Return the player matching the player mask
	*/
	Player *getPlayerFromMask( PlayerMaskType mask );

	/**
		Get each player in numerical order that this mask represents.
		Note that maskToAdjust will be adjusted by removing the associated player's mask from it.
	*/
	Player *getEachPlayerFromMask( PlayerMaskType& maskToAdjust );

	Team *validateTeam( AsciiString owner );

	/**
		a convenience routine to quickly clear the entered/exited flags on all teams.
	*/
	void updateTeamStates();

	/**
		a convenience routine to return the players who srcPlayer considers to have one of the
		relationships specified in allowedRelationships. Note that allowedRelationships should be
		a bitwise OR of AllowPlayerRelationship flags.
	*/
	PlayerMaskType getPlayersWithRelationship( Int srcPlayerIndex, UnsignedInt allowedRelationships );

	// GeneralsX @bugfix GitHubCopilot 10/07/2026 Keep the -1 slot sentinel independent of Byte signedness.
	Int getSlotIndex(Int playerIndex) const;

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

private:
	void resolveSlotIndices();
	void setSlotIndex(Int playerIndex, Int slotIndex);

	Player				*m_local;
	Int						m_playerCount;
	Player				*m_players[MAX_PLAYER_COUNT];
	Int						m_slotIndices[MAX_PLAYER_COUNT];

};


// ----------------------------------------------------------------------------------------------
extern PlayerList *ThePlayerList;	///< singleton instance of PlayerList
