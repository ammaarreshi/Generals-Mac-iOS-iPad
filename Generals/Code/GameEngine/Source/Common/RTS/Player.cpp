/*
**	Command & Conquer Generals(tm)
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

// FILE: Player.cpp /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: Player.cpp
//
// Created:   Steven Johnson, October 2001
//
// Desc:      @todo
//
//-----------------------------------------------------------------------------

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_SCIENCE_AVAILABILITY_NAMES

#include "Common/ActionManager.h"
#include "Common/BuildAssistant.h"
#include "Common/CRCDebug.h"
#include "Common/DisabledTypes.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/MiscAudio.h"
#include "Common/PerfTimer.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/ProductionPrerequisite.h"
#include "Common/Radar.h"
#include "Common/ResourceGatheringManager.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/TunnelTracker.h"
#include "Common/Upgrade.h"
#include "Common/WellKnownKeys.h"
#include "Common/Xfer.h"
#include "Common/BitFlagsIO.h"
#include "Common/SpecialPower.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/Eva.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameText.h"

#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/AISkirmishPlayer.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/Squad.h"
#include "GameLogic/RankInfo.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/AutoDepositUpdate.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Module/BattlePlanUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/VictoryConditions.h"

#include "GameNetwork/GameInfo.h"


//Grey for neutral.
#define NEUTRAL_PLAYER_COLOR 0xffffffff

// ------------------------------------------------------------------------------------------------
class ClosestKindOfData
{
public:

	ClosestKindOfData();

	//In
	KindOfMaskType m_setKindOf;
	KindOfMaskType m_clearKindOf;
	Object *m_source;

	//Out
	Object *m_closest;
	Real m_closestDistSq;

};

// ------------------------------------------------------------------------------------------------
ClosestKindOfData::ClosestKindOfData()
{
	m_setKindOf.clear();
	m_clearKindOf.clear();
	m_source = nullptr;
	m_closest = nullptr;
	m_closestDistSq = FLT_MAX;
}

// ------------------------------------------------------------------------------------------------
static void findClosestKindOf( Object *obj, void *userData )
{
	ClosestKindOfData *closestData = (ClosestKindOfData *)userData;

	if( ! obj->isKindOfMulti( closestData->m_setKindOf, closestData->m_clearKindOf ) )
		return; // Do nothing to the magic running total pointer man.

	// is this the closest one so far
	Real distSq = ThePartitionManager->getDistanceSquared( closestData->m_source, obj, FROM_CENTER_2D );
	if( distSq < closestData->m_closestDistSq )
	{
		closestData->m_closest = obj;
		closestData->m_closestDistSq = distSq;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_CRC
#define CRCDUMPBATTLEPLANBONUSES(x,y,z) dumpBattlePlanBonuses(x, #x, y, z, __FILE__, __LINE__, FALSE)
#define DUMPBATTLEPLANBONUSES(x,y,z) dumpBattlePlanBonuses(x, #x, y, z, __FILE__, __LINE__, TRUE)
AsciiString kindofMaskAsAsciiString(KindOfMaskType m)
{
	AsciiString s;
	const char* const* kindofNames = KindOfMaskType::getBitNames();
	for (Int i=KINDOF_FIRST; i<KINDOF_COUNT; ++i)
	{
		if (m.test(i))
		{
			if (s.isNotEmpty())
				s.concat("|");
			s.concat(kindofNames[i]);
		}
	}
	if (s.isEmpty())
		s = "KINDOF_INVALID";
	return s;
}
void dumpBattlePlanBonuses(const BattlePlanBonusesData *b, AsciiString name, const Player *p, const Object *o, AsciiString fname, Int line, Bool doDebugLog)
{
	CRCDEBUG_LOG(("dumpBattlePlanBonuses() %s:%d %s\n  Player %d(%ls) object %d(%s) armor:%g/%8.8X bombardment:%d, holdTheLine:%d, searchAndDestroy:%d sight:%g/%8.8X, valid:%s invalid:%s",
		fname.str(), line, name.str(),
		(p)?p->getPlayerIndex():-1, (p)?((Player *)p)->getPlayerDisplayName().str():L"<No Name>", (o)?o->getID():-1, (o)?o->getTemplate()->getName().str():"<No Name>",
		b->m_armorScalar, AS_INT(b->m_armorScalar),
		b->m_bombardment, b->m_holdTheLine, b->m_searchAndDestroy,
		b->m_sightRangeScalar, AS_INT(b->m_sightRangeScalar),
		kindofMaskAsAsciiString(b->m_validKindOf).str(),
		kindofMaskAsAsciiString(b->m_invalidKindOf).str()));
	if (!doDebugLog)
		return;
	DEBUG_LOG(("dumpBattlePlanBonuses() %s:%d %s\n  Player %d(%ls) object %d(%s) armor:%g/%8.8X bombardment:%d, holdTheLine:%d, searchAndDestroy:%d sight:%g/%8.8X, valid:%s invalid:%s",
		fname.str(), line, name.str(),
		(p)?p->getPlayerIndex():-1, (p)?((Player *)p)->getPlayerDisplayName().str():L"<No Name>", (o)?o->getID():-1, (o)?o->getTemplate()->getName().str():"<No Name>",
		b->m_armorScalar, AS_INT(b->m_armorScalar),
		b->m_bombardment, b->m_holdTheLine, b->m_searchAndDestroy,
		b->m_sightRangeScalar, AS_INT(b->m_sightRangeScalar),
		kindofMaskAsAsciiString(b->m_validKindOf).str(),
		kindofMaskAsAsciiString(b->m_invalidKindOf).str()));
}
#else
#define DUMPBATTLEPLANBONUSES(x,y,z) {}
#define CRCDUMPBATTLEPLANBONUSES(x,y,z) {}
#endif // DEBUG_CRC

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
PlayerRelationMap::PlayerRelationMap()
{

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
PlayerRelationMap::~PlayerRelationMap()
{

	// make sure the data is cleared
	m_map.clear();

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void PlayerRelationMap::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	*/
// ------------------------------------------------------------------------------------------------
void PlayerRelationMap::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// player relation count
	PlayerRelationMapType::iterator playerRelationIt;
	UnsignedShort playerRelationCount = m_map.size();
	xfer->xferUnsignedShort( &playerRelationCount );

	// player relations
	Int playerIndex;
	Relationship r;
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// go through all player relations
		for( playerRelationIt = m_map.begin(); playerRelationIt != m_map.end(); ++playerRelationIt )
		{

			// write player index
			playerIndex = (*playerRelationIt).first;
			xfer->xferInt( &playerIndex );

			// write relationship
			r = (*playerRelationIt).second;
			xfer->xferUser( &r, sizeof( Relationship ) );

		}

	}
	else
	{

		for( UnsignedShort i = 0; i < playerRelationCount; ++i )
		{

			// read player index
			xfer->xferInt( &playerIndex );

			// read relationship
			xfer->xferUser( &r, sizeof( Relationship ) );

			// assign relationship
			m_map[ playerIndex ] = r;

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void PlayerRelationMap::loadPostProcess()
{

}

//=============================================================================
Player::Player( Int playerIndex )
{
	m_isPreorder = FALSE;
	m_isPlayerDead = FALSE;

	m_playerIndex = playerIndex;

	// allocate new relation map pools
	m_playerRelations = newInstance(PlayerRelationMap);
	m_teamRelations = newInstance(TeamRelationMap);

	m_upgradeList = nullptr;
	m_pBuildList = nullptr;
	m_ai = nullptr;
	m_resourceGatheringManager = nullptr;
	m_defaultTeam = nullptr;
	m_radarCount = 0;
	m_disableProofRadarCount = 0;
	m_radarDisabled = FALSE;
	m_bombardBattlePlans = 0;
	m_holdTheLineBattlePlans = 0;
	m_searchAndDestroyBattlePlans = 0;
	m_tunnelSystem = nullptr;
	m_playerTemplate = nullptr;
	m_visionSpiedMask = PLAYERMASK_NONE;
	m_battlePlanBonuses = nullptr;
	m_skillPointsModifier = 1.0f;
	m_canBuildUnits = TRUE;
	m_canBuildBase  = TRUE;
	m_cashBountyPercent = 0.0f;
	m_color = 0;
	m_currentSelection = nullptr;
	m_rankLevel = 0;
	m_sciencePurchasePoints = 0;
	m_side = nullptr;
	m_skillPoints = 0;
	Int i;
	m_upgradeList = nullptr;
	for(i = 0; i < NUM_HOTKEY_SQUADS; i++)
	{
		m_squads[i] = nullptr;
	}
	for (i = 0; i < MAX_PLAYER_COUNT; ++i)
	{
		m_attackedBy[i] = false;
		m_visionSpiedBy[i] = 0;
	}
	m_attackedFrame = 0;

	m_unitsShouldHunt = FALSE;
	init( nullptr );

}

//=============================================================================
void Player::init(const PlayerTemplate* pt)
{

	DEBUG_ASSERTCRASH(m_playerTeamPrototypes.empty(), ("Player::m_playerTeamPrototypes is not empty at game start!"));
	m_skillPointsModifier = 1.0f;
	m_attackedFrame = 0;

	m_isPreorder = FALSE;
	m_isPlayerDead = FALSE;

	m_radarCount = 0;
	m_disableProofRadarCount = 0;
	m_radarDisabled = FALSE;

	m_bombardBattlePlans = 0;
	m_holdTheLineBattlePlans = 0;
	m_searchAndDestroyBattlePlans = 0;

	deleteInstance(m_battlePlanBonuses);
	m_battlePlanBonuses = nullptr;

	deleteUpgradeList();

	m_energy.init(this);
	m_stats.init();

	deleteInstance(m_pBuildList);
	m_pBuildList = nullptr;

	m_defaultTeam = nullptr;

	deleteInstance(m_ai);
	m_ai = nullptr;

	deleteInstance(m_resourceGatheringManager);
	m_resourceGatheringManager = nullptr;

	for (Int i = 0; i < NUM_HOTKEY_SQUADS; ++i) {
		deleteInstance(m_squads[i]);
		m_squads[i] = newInstance(Squad);
	}

	deleteInstance(m_currentSelection);
	m_currentSelection = newInstance(Squad);

	deleteInstance(m_tunnelSystem);
	m_tunnelSystem = nullptr;

	m_canBuildBase = true;
	m_canBuildUnits = true;
	m_observer = false;
	m_cashBountyPercent = 0.0f;
	m_listInScoreScreen = TRUE;

	m_unitsShouldHunt = FALSE;

#if defined(RTS_DEBUG)
	m_DEMO_ignorePrereqs = FALSE;
	m_DEMO_freeBuild = FALSE;
	m_DEMO_instantBuild = FALSE;
#endif

	if (pt)
	{
		m_side = pt->getSide();
		m_productionCostChanges = pt->getProductionCostChanges();
		m_productionTimeChanges = pt->getProductionTimeChanges();
		m_productionVeterancyLevels = pt->getProductionVeterancyLevels();
		m_color = pt->getPreferredColor()->getAsInt() | 0xff000000;
		m_nightColor = m_color;

		m_money = *pt->getMoney();
		m_money.setPlayerIndex(getPlayerIndex());

		m_handicap = *pt->getHandicap();

		if( m_money.countMoney() == 0 )
		{
			// TheSuperHackers @bugfix Now correctly deposits the money and fixes its audio and academy issues.
			// Note that copying the entire Money class instead would also copy the player index inside of it.
			if ( TheGameInfo )
			{
				m_money.deposit( TheGameInfo->getStartingCash().countMoney(), FALSE, FALSE );
			}
			else
			{
				m_money.deposit( TheGlobalData->m_defaultStartingCash.countMoney(), FALSE, FALSE );
			}
		}

		m_playerDisplayName.clear();
		m_playerName.clear();
		m_playerNameKey = NAMEKEY_INVALID;
		m_playerType = PLAYER_COMPUTER;
		m_observer = pt->isObserver();
		m_isPlayerDead = m_observer; // observers are dead

	}
	else
	{
		// no player template? must be the neutral player!
		m_side = "";
		m_productionCostChanges.clear();
		m_productionTimeChanges.clear();
		m_productionVeterancyLevels.clear();
		m_color = NEUTRAL_PLAYER_COLOR;
		m_nightColor = NEUTRAL_PLAYER_COLOR;
		m_money.init();
		m_handicap.init();

		m_playerDisplayName = UnicodeString::TheEmptyString;
		m_playerName = AsciiString::TheEmptyString;
		m_playerNameKey = NAMEKEY(AsciiString::TheEmptyString);
		m_playerType = PLAYER_COMPUTER;

		// neutral is always "allied" with self -- this is the only thing ever allied with neutral!
		setPlayerRelationship(this, ALLIES);

	}
	// reset each player
	m_scoreKeeper.reset(m_playerIndex);
	m_playerTemplate = pt;

	// reset rank info
	resetRank();
	m_sciencesDisabled.clear();
	m_sciencesHidden.clear();
	m_specialPowerReadyTimerList.clear();

	KindOfPercentProductionChangeListIt it = m_kindOfPercentProductionChangeList.begin();
	while(it != m_kindOfPercentProductionChangeList.end())
	{
		KindOfPercentProductionChange *tof = *it;
		it = m_kindOfPercentProductionChangeList.erase( it );
		deleteInstance(tof);
	}

	//Always off at the beginning of a game! Only GameLogic::update has
	//the power to turn it on. Don't want to cause desyncs!
	m_logicalRetaliationModeEnabled = FALSE;
}

//=============================================================================
Player::~Player()
{
	m_defaultTeam = nullptr;
	m_playerTemplate = nullptr;

	for( PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		(*it)->friend_setOwningPlayer(nullptr);
	}
	m_playerTeamPrototypes.clear();	// empty, but don't free the contents

	// delete the relation maps (the destructor clears the actual map if any data is present)
	deleteInstance(m_teamRelations);
	m_teamRelations = nullptr;

	deleteInstance(m_playerRelations);
	m_playerRelations = nullptr;

	for (Int i = 0; i < NUM_HOTKEY_SQUADS; ++i) {
		deleteInstance(m_squads[i]);
		m_squads[i] = nullptr;
	}

	deleteInstance(m_currentSelection);
	m_currentSelection = nullptr;

	deleteInstance(m_battlePlanBonuses);
	m_battlePlanBonuses = nullptr;
}

//=============================================================================
//DECLARE_PERF_TIMER(Player_getRelationship)
Relationship Player::getRelationship(const Team *that) const
{
	//USE_PERF_TIMER(Player_getRelationship)
	if (that)
	{
		// do we have an override for that particular team? if so, return it.
		if (!m_teamRelations->m_map.empty())
		{
			TeamRelationMapType::const_iterator it = m_teamRelations->m_map.find(that->getID());
			if (it != m_teamRelations->m_map.end())
			{
				return (*it).second;
			}
		}

		// hummm... well, do we have something for that team's player?
		if (!m_playerRelations->m_map.empty())
		{
			const Player* thatPlayer = that->getControllingPlayer();
			if (thatPlayer != nullptr)
			{
				PlayerRelationMapType::const_iterator it = m_playerRelations->m_map.find(thatPlayer->getPlayerIndex());
				if (it != m_playerRelations->m_map.end())
				{
					return (*it).second;
				}
			}
		}
	}
	return NEUTRAL;
}

//=============================================================================
void Player::setPlayerRelationship(const Player *that, Relationship r)
{
	if (that != nullptr)
	{
		// note that this creates the entry if it doesn't exist.
		m_playerRelations->m_map[that->getPlayerIndex()] = r;
	}
}

// ------------------------------------------------------------------------
Bool Player::removePlayerRelationship(const Player *that)
{
	if (!m_playerRelations->m_map.empty())
	{
		if (that == nullptr)
		{
			m_playerRelations->m_map.clear();
			return true;
		}
		else
		{
			PlayerRelationMapType::iterator it = m_playerRelations->m_map.find(that->getPlayerIndex());
			if (it != m_playerRelations->m_map.end())
			{
				m_playerRelations->m_map.erase(it);
				return true;
			}
		}
	}
	return false;
}

//=============================================================================
void Player::setTeamRelationship(const Team *that, Relationship r)
{
	if (that != nullptr)
	{
		// note that this creates the entry if it doesn't exist.
		m_teamRelations->m_map[that->getID()] = r;
	}
}

// ------------------------------------------------------------------------
Bool Player::removeTeamRelationship(const Team *that)
{
	if (!m_teamRelations->m_map.empty())
	{
		if (that == nullptr)
		{
			m_teamRelations->m_map.clear();
			return true;
		}
		else
		{
			TeamRelationMapType::iterator it = m_teamRelations->m_map.find(that->getID());
			if (it != m_teamRelations->m_map.end())
			{
				m_teamRelations->m_map.erase(it);
				return true;
			}
		}
	}
	return false;
}

//=============================================================================
void Player::setBuildList(BuildListInfo *pBuildList)
{

	deleteInstance(m_pBuildList);
	m_pBuildList = pBuildList;

}

//=============================================================================
void Player::addToBuildList(Object *obj)
{
	BuildListInfo *newInfo = newInstance( BuildListInfo );
	newInfo->setObjectID(obj->getID());
	newInfo->setTemplateName(obj->getTemplate()->getName());
	newInfo->setLocation(*obj->getPosition());
	newInfo->setAngle(obj->getOrientation());
	newInfo->setNumRebuilds(0);	 // Can't rebuild.
	newInfo->setNextBuildList(m_pBuildList);
	m_pBuildList = newInfo;
}

//=============================================================================
void Player::addToPriorityBuildList(AsciiString templateName, Coord3D *pos, Real angle)
{
	BuildListInfo *newInfo = newInstance( BuildListInfo );
	newInfo->setTemplateName(templateName);
	newInfo->setLocation(*pos);
	newInfo->setAngle(angle);
	newInfo->markPriorityBuild();
	newInfo->setNumRebuilds(1);	 // Build once.
	newInfo->setNextBuildList(m_pBuildList);
	m_pBuildList = newInfo;
}

//=============================================================================
void Player::update()
{
	if (m_ai)
		m_ai->update();

	// Allow the teams this player owns to update themselves.

	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin(); it != m_playerTeamPrototypes.end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			team->updateGenericScripts();
		}
	}

#if !PRESERVE_TUNNEL_HEAL_STACKING && !RETAIL_COMPATIBLE_CRC
	// TheSuperHackers @bugfix Stubbjax 26/09/2025 The Tunnel System now heals
	// all units once per frame instead of once per frame per Tunnel Network.
	TunnelTracker* tunnelSystem = getTunnelSystem();
	if (tunnelSystem)
		tunnelSystem->healObjects();
#endif

	m_money.updateIncomeBucket();
}

//=============================================================================
void Player::newMap()
{
	if (m_ai)
		m_ai->newMap();
}

//=============================================================================
void Player::setPlayerType(PlayerType t, Bool skirmish)
{
	m_playerType = t;

	deleteInstance(m_ai);
	m_ai = nullptr;

	if (t == PLAYER_COMPUTER)
	{
		if (skirmish || TheAI->getAiData()->m_forceSkirmishAI) {
			// create AIPlayer and attach to this Player
			m_ai = newInstance(AISkirmishPlayer)( this );
		} else {
			// create AIPlayer and attach to this Player
			m_ai = newInstance(AIPlayer)( this );
		}
	}
}

//=============================================================================
// This is called from PlayerList->newGame()
//
void Player::setDefaultTeam() {
	AsciiString tname;
	tname.set("team");
	tname.concat(m_playerName);
	Team *dt = TheTeamFactory->findTeam(tname);
	DEBUG_ASSERTCRASH(dt, ("no team"));
	if (dt) {
		m_defaultTeam = dt;
		dt->setActive();
	}
}

//=============================================================================
void Player::deletePlayerAI()
{
	deleteInstance(m_ai);
	m_ai = nullptr;
}

//=============================================================================
// This is called from PlayerList->newGame()
//
void Player::initFromDict(const Dict* d)
{
	AsciiString tmplname = d->getAsciiString(TheKey_playerFaction);
	const PlayerTemplate* pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(tmplname));
	DEBUG_ASSERTCRASH(pt != nullptr, ("PlayerTemplate %s not found -- this is an obsolete map (please open and resave in WB)",tmplname.str()));

	init(pt);

	m_playerDisplayName = d->getUnicodeString(TheKey_playerDisplayName);
	AsciiString pname = d->getAsciiString(TheKey_playerName);
	m_playerName = pname;
	m_playerNameKey = NAMEKEY(pname);

	Bool exists;
	Bool skirmish = false;
	Bool forceHuman = false;
	// GeneralsX @bugfix Copilot 22/03/2026 Initialize multiplayer start index before any skirmish name/script qualification.
	m_mpStartIndex = d->getInt(TheKey_multiplayerStartIndex, &exists);
	if (d->getBool(TheKey_playerIsSkirmish, &exists))
	{

		// srj sez: check to ensure the map actually has a skirmish player... ordinarily it should, but
		// poorly-formed user maps might not, which would be bad, and could crash us later. so if it doesn't
		// actually have a skirmish player defined for this side, declare it nonskirmish... the player won't
		// really work, but it's better than crashing.
		for (Int spIdx = 0; spIdx < TheSidesList->getNumSkirmishSides(); ++spIdx)
		{
			AsciiString spTemplateName = TheSidesList->getSkirmishSideInfo(spIdx)->getDict()->getAsciiString(TheKey_playerFaction);
			const PlayerTemplate* spt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(spTemplateName));
			if (spt && spt->getSide() == getSide())
			{
				skirmish = true;
				break;
			}
		}

		DEBUG_ASSERTCRASH(skirmish, ("Could not find skirmish player for side %s... quietly making into nonskirmish.", getSide().str()));
		if (!skirmish)
			forceHuman = true;

	}

	if (d->getBool(TheKey_playerIsHuman) || forceHuman)
	{
		setPlayerType(PLAYER_HUMAN, false);
		if (d->getBool(TheKey_playerIsPreorder, &exists))
		{
			m_isPreorder = TRUE;
		}
		if (TheSidesList->getNumSkirmishSides()>0) {
			// Human player gets scripts from CIVILIAN player.
			AsciiString  mySide = "Civilian";
			Int i;
			Bool found = false;
			AsciiString  qualTemplatePlayerName;
			for (i=0; i<TheSidesList->getNumSkirmishSides(); i++) {
				AsciiString templateName = TheSidesList->getSkirmishSideInfo(i)->getDict()->getAsciiString(TheKey_playerFaction);
				pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(templateName));
				if (pt && pt->getSide() == mySide) {
					qualTemplatePlayerName.format("%s%d", TheSidesList->getSkirmishSideInfo(i)->getDict()->getAsciiString(TheKey_playerName).str(), m_mpStartIndex);
					found = true;
					break;
				}
			}
			if (found && TheSidesList->getSkirmishSideInfo(i)->getScriptList()) {
				AsciiString qualifier;
				qualifier.format("%d", m_mpStartIndex);

				ScriptList *scripts = TheSidesList->getSkirmishSideInfo(i)->getScriptList()->duplicateAndQualify(
							qualifier, qualTemplatePlayerName, pname);

				deleteInstance(TheSidesList->getSideInfo(getPlayerIndex())->getScriptList());
				TheSidesList->getSideInfo(getPlayerIndex())->setScriptList(scripts);

				deleteInstance(TheSidesList->getSkirmishSideInfo(i)->getScriptList());
				TheSidesList->getSkirmishSideInfo(i)->setScriptList(nullptr);
			}

		}
		skirmish = false;
	}
	else
	{
		setPlayerType(PLAYER_COMPUTER, skirmish);
	}
	if (skirmish) {
		// Copy and qualify scripts, and teams.

		AsciiString mySide = getSide();
		Int i, skirmishNdx;
		Bool found = false;
		AsciiString  qualTemplatePlayerName;
		for (skirmishNdx=0; skirmishNdx<TheSidesList->getNumSkirmishSides(); skirmishNdx++) {
			AsciiString templateName = TheSidesList->getSkirmishSideInfo(skirmishNdx)->getDict()->getAsciiString(TheKey_playerFaction);
			pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(templateName));
			if (pt && pt->getSide() == mySide) {
				qualTemplatePlayerName.format("%s%d", TheSidesList->getSkirmishSideInfo(skirmishNdx)->getDict()->getAsciiString(TheKey_playerName).str(), m_mpStartIndex);
				found = true;
				break;
			}
		}
		Int diffInt  = d->getInt(TheKey_skirmishDifficulty, &exists);
		GameDifficulty difficulty = TheScriptEngine->getGlobalDifficulty();
		if (exists) {
			difficulty = (GameDifficulty) diffInt;
		}
		if (m_ai) {
			m_ai->setAIDifficulty(difficulty);
		}

		if (!found) {
			DEBUG_CRASH(("Could not find skirmish player for side %s", mySide.str()));
		} else {
			m_playerName = qualTemplatePlayerName;
			AsciiString qualifier;
			qualifier.format("%d", m_mpStartIndex);
			ScriptList *scripts = TheSidesList->getSkirmishSideInfo(skirmishNdx)->getScriptList()->duplicateAndQualify(
						qualifier, qualTemplatePlayerName, pname);

			deleteInstance(TheSidesList->getSideInfo(getPlayerIndex())->getScriptList());
			TheSidesList->getSideInfo(getPlayerIndex())->setScriptList(scripts);

			for (i=0; i<TheSidesList->getNumTeams(); i++) {
				if (TheSidesList->getTeamInfo(i)->getDict()->getAsciiString(TheKey_teamOwner) == pname)
				{
					// Remove any teams in this player.
					TheSidesList->removeTeam(i);
					i--;
				}
			}
			// Now add teams.

			AsciiString originalPlayerName = TheSidesList->getSkirmishSideInfo(skirmishNdx)->getDict()->getAsciiString(TheKey_playerName);
			for (i=0; i<TheSidesList->getNumSkirmishTeams(); i++) {
				if (TheSidesList->getSkirmishTeamInfo(i)->getDict()->getAsciiString(TheKey_teamOwner) == originalPlayerName)
				{
					Dict teamDict(*TheSidesList->getSkirmishTeamInfo(i)->getDict());
					AsciiString teamName = teamDict.getAsciiString(TheKey_teamName);
					AsciiString newName;
					newName.format("%s%d", teamDict.getAsciiString(TheKey_teamName).str(), m_mpStartIndex);

					if (TheSidesList->findTeamInfo(newName)) continue;
					teamDict.setAsciiString(TheKey_teamOwner, pname);
					teamDict.setAsciiString(TheKey_teamName, newName);
					// qualify scripts.

					NameKeyType nameKeys[] =
					{
						TheKey_teamOnCreateScript,
						TheKey_teamOnIdleScript,
						TheKey_teamOnUnitDestroyedScript,
						TheKey_teamOnDestroyedScript,
						TheKey_teamEnemySightedScript,
						TheKey_teamAllClearScript,
						TheKey_teamProductionCondition
					};

					Int j;
					const Int numKeys = sizeof(nameKeys) / sizeof(NameKeyType);
					AsciiString tmpStr;
					for (j = 0; j < numKeys; ++j)
					{
						tmpStr = teamDict.getAsciiString(nameKeys[j], &exists);
						if (exists && !tmpStr.isEmpty())
						{
							newName.format("%s%d", tmpStr.str(), m_mpStartIndex);
							teamDict.setAsciiString(nameKeys[j], newName);
						}
					}

					// Now do the TheKey_teamGenericScriptHookN (where N can be from 0 to 15.)
					for (j = 0; j < MAX_GENERIC_SCRIPTS; ++j) {
						AsciiString keyName;
						keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_teamGenericScriptHook).str(), j);
						tmpStr = teamDict.getAsciiString(NAMEKEY(keyName), &exists);
						if (exists && !tmpStr.isEmpty())
						{
							newName.format("%s%d", tmpStr.str(), m_mpStartIndex);
							teamDict.setAsciiString(NAMEKEY(keyName), newName);
						}
					}

					// Done. Now add the team.
					TheSidesList->addTeam(&teamDict);
				}
			}
		}
	}

	deleteInstance(m_resourceGatheringManager);
	m_resourceGatheringManager = newInstance(ResourceGatheringManager);

	deleteInstance(m_tunnelSystem);
	m_tunnelSystem = newInstance(TunnelTracker);

	m_handicap.readFromDict(d);

	/// @todo Ack!  the todo in PlayerList::reset() mentioning the need for a Player::reset() really needs to get done.
	m_playerRelations->m_map.clear(); // For now, it has been decided to just fix this one.  Dear god me must reset.
	m_teamRelations->m_map.clear(); // For now, it has been decided to just fix this one.  Dear god me must reset.

	Int i;
	for ( i = 0; i < MAX_PLAYER_COUNT; ++i ) // For now, it has been decided to just fix this one.  Dear god me must reset.
	{
		m_attackedBy[i] = false;
		m_visionSpiedBy[i] = 0;
	}
	m_visionSpiedMask = PLAYERMASK_NONE;

	Int c = d->getInt(TheKey_playerColor, &exists);
	if (exists)
	{
		m_color = c | 0xff000000;
		m_nightColor = m_color;
	}

	c = d->getInt(TheKey_playerNightColor, &exists);
	if (exists)
	{
		m_nightColor = c | 0xff000000;
	}

	Int m = d->getInt(TheKey_playerStartMoney, &exists);
	if (exists)
		m_money.deposit(m);

	for ( i = 0; i < NUM_HOTKEY_SQUADS; ++i ) {
		deleteInstance(m_squads[i]);
		m_squads[i] = newInstance( Squad );
	}

	deleteInstance(m_currentSelection);
	m_currentSelection = newInstance( Squad );
}

//=============================================================================
void Player::becomingTeamMember(Object *obj, Bool yes)
{
	if (!obj)
		return;

	// energy production/consumption hooks, note we ignore things that are UNDER_CONSTRUCTION
	if( !obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{
		obj->friend_adjustPowerForPlayer(yes);
	}

	// when we capture a building, we need to see if there's an AutoDepositUpdate hooked to it,
	// if so, award the cash bonus
	if(this != ThePlayerList->getNeutralPlayer() && yes)
	{
		NameKeyType key_AutoDepositUpdate = NAMEKEY("AutoDepositUpdate");
		AutoDepositUpdate *adu = (AutoDepositUpdate *)obj->findUpdateModule(key_AutoDepositUpdate);
		if (adu != nullptr) {
			adu->awardInitialCaptureBonus( this );
		}
	}

	if( getNumBattlePlansActive() > 0 && obj->areModulesReady() )
	{
		if( yes )
		{
			//We are entering a team with active battle plans so add it's bonuses now
			applyBattlePlanBonusesForObject( obj );
		}
		else
		{
			//We are leaving a team with active battle plans so remove them now.
			removeBattlePlanBonusesForObject( obj );
		}
	}


	if (obj->isKindOf(KINDOF_DOZER)
			&& obj->getAIUpdateInterface()
			&& obj->getAIUpdateInterface()->isIdle())
	{
		// Need to remove it from the pick a peasant button
		if (yes)
			TheInGameUI->addIdleWorker(obj);
		else
			TheInGameUI->removeIdleWorker(obj, getPlayerIndex());
	}
}

//=============================================================================
void Player::becomingLocalPlayer(Bool yes)
{
	if (yes)
	{
		// This changes the color of the little dot on the upper right side of the screen indicating
		// which team is under control.
		if( TheGameClient )
		{
			RGBColor rgb;
			rgb.setFromInt(m_color);
			TheGameClient->setTeamColor(REAL_TO_INT(rgb.red*255), REAL_TO_INT(rgb.green*255), REAL_TO_INT(rgb.blue*255));
		}

		if( ThePartitionManager )
		{
			ObjectIterator *iter = ThePartitionManager->iterateAllObjects();
			for( Object* object = iter->first(); object; object = iter->next() )
			{
				// Added support for updating the perceptions of garrisoned buildings containing enemy stealth units.
				// When changing teams, it is necessary to update this information.
				Bool requireRadarRefresh = false;
				ContainModuleInterface *contain = object->getContain();
				if( contain )
				{
					contain->recalcApparentControllingPlayer();
					requireRadarRefresh = true;
				}

				if( object->isKindOf( KINDOF_DISGUISER ) )
				{
					//KM -- August 2002:
					//Added support for disguised objects, based on relationships, we either show the real color or the disguised color.
					Drawable *draw = object->getDrawable();
					if( draw )
					{
						StealthUpdate *update = object->getStealth();
						if( update && update->isDisguised() )
						{
							Player *disguisedPlayer = ThePlayerList->getNthPlayer( update->getDisguisedPlayerIndex() );
							if( getRelationship( object->getTeam() ) != ALLIES && isPlayerActive() )
							{
								//Neutrals and enemies will see this disguised unit as the team it's disguised as.
								if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT)
									draw->setIndicatorColor( disguisedPlayer->getPlayerNightColor());
								else
									draw->setIndicatorColor( disguisedPlayer->getPlayerColor() );
							}
							else
							{
								//Otherwise, the color will show up as the team it really belongs to.
								if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT)
									draw->setIndicatorColor(object->getNightIndicatorColor());
								else
									draw->setIndicatorColor( object->getIndicatorColor() );
							}
							requireRadarRefresh = true;
						}
					}
				}

				if (requireRadarRefresh)
				{
					TheRadar->removeObject( object );
					TheRadar->addObject( object );
				}
			}
			deleteInstance(iter);
		}
	}
	else
	{
		// nothing to do
	}
}

//-------------------------------------------------------------------------------------------------
/** Is this player a skirmish ai player? */
//-------------------------------------------------------------------------------------------------
Bool Player::isSkirmishAIPlayer()
{
	return m_ai ? m_ai->isSkirmishAI() : false;
}


//----------------------------------------------------------------------------------------------------------
/**
 * Find a good spot to fire a superweapon.
 */
void Player::computeSuperweaponTarget(const SpecialPowerTemplate *power, Coord3D *retPos, Int playerNdx, Real weaponRadius)
{
	if (m_ai) {
		m_ai->computeSuperweaponTarget(power, retPos, playerNdx, weaponRadius);
	}
}

//-------------------------------------------------------------------------------------------------
/** Get this player's current enemy. NOTE - Can be nullptr. */
//-------------------------------------------------------------------------------------------------
Player  *Player::getCurrentEnemy()
{
	return m_ai?m_ai->getAiEnemy():nullptr;
}

//-------------------------------------------------------------------------------------------------
// PlayerObjectFindInfo is used to find a player's object. For example, we iterate through
// to find a player's command center, or a specific building capable of firing the specified
// special power.
//
// if he has none, return null.
// if he has multiple, return one arbitrarily.
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Iterator data struct
//-------------------------------------------------------------------------------------------------
struct PlayerObjectFindInfo
{
	Player* player;
	Object* obj;
	SpecialPowerType spType;
	const ThingTemplate *thing;
	UnsignedInt lowestReadyFrame;
	UnsignedInt highestPercentage;
	UnsignedInt numReady;
};

//-------------------------------------------------------------------------------------------------
// Iterator function
// Find the first available command center that is naturally ours (not captured from enemy).
//-------------------------------------------------------------------------------------------------
static void doFindCommandCenter(Object* obj, void* userData)
{
	PlayerObjectFindInfo* info = (PlayerObjectFindInfo*)userData;

	if (info->obj == nullptr
			&& obj->isKindOf(KINDOF_COMMANDCENTER)
			&& obj->getTemplate()->getDefaultOwningSide() == info->player->getSide()
			&& !obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION)
			&& !obj->testStatus(OBJECT_STATUS_SOLD))
	{
		info->obj = obj;
	}
}

//-------------------------------------------------------------------------------------------------
// Iterator function
// Find first object capable of firing specified special power right now.
//-------------------------------------------------------------------------------------------------
static void doFindSpecialPowerSourceObject( Object *obj, void *userData )
{
	PlayerObjectFindInfo* info = (PlayerObjectFindInfo*)userData;

	if( info->lowestReadyFrame == 0 )
	{
		//We already found the best case scenario, so no need to iterate any more.
		return;
	}
	if( !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
			&& !obj->testStatus( OBJECT_STATUS_SOLD )
			&& !obj->isEffectivelyDead() )
	{
		if( info->spType == SPECIAL_INVALID && obj->hasAnySpecialPower() )
		{
			//We just care about find an object that has *any* shortcut capable special power.
			//Iterate through the special power modules and look for one.
			SpecialPowerModuleInterface *spmInterface = obj->findAnyShortcutSpecialPowerModuleInterface();
			if( spmInterface && !spmInterface->isScriptOnly() )
			{
				info->obj = obj;
				info->lowestReadyFrame = 0;
				return;
			}
		}
		else if( obj->hasSpecialPower( info->spType ) )
		{
			SpecialPowerModuleInterface *spmInterface = obj->findSpecialPowerModuleInterface( info->spType );
			if( spmInterface && !spmInterface->isScriptOnly() )
			{
				UnsignedInt readyFrame = spmInterface->getReadyFrame();

#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
				// Everything is ready if timers are debug off'd
				if( ! TheGlobalData->m_specialPowerUsesDelay )
					readyFrame = 0;
#endif
				// A disabled guy should only be considered as a last resort.  We need it to be counted
				// so that a disabled button can appear on the shortcut.
				if( obj->isDisabled() )
					readyFrame = UINT_MAX - 10;

				if( readyFrame < TheGameLogic->getFrame())
				{
					//This special power is ready now and matches, so simply return the
					//first one.
					info->obj = obj;
					info->lowestReadyFrame = 0;
					return;
				}
				else if( readyFrame < info->lowestReadyFrame )
				{
					//This special power isn't ready, but it is going to be ready sooner than any others
					//we checked (or it's the first one we checked).
					info->obj = obj;
					info->lowestReadyFrame = readyFrame;
					return;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Iterator function
// Count number of specified special powers that are ready to fire now.
//-------------------------------------------------------------------------------------------------
static void doCountSpecialPowersReady( Object *obj, void *userData )
{
	PlayerObjectFindInfo* info = (PlayerObjectFindInfo*)userData;

	if( !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
			&& !obj->testStatus( OBJECT_STATUS_SOLD )
			&& !obj->isEffectivelyDead() )
	{
		if( obj->hasSpecialPower( info->spType ) )
		{
			SpecialPowerModuleInterface *spmInterface = obj->findSpecialPowerModuleInterface( info->spType );
			if( spmInterface && !spmInterface->isScriptOnly() )
			{

	      if( spmInterface->getSpecialPowerTemplate()->isSharedNSync() && info->numReady == 1 )
				{
					//Shared powers don't stack after the first one is counted.
					return;
				}

				UnsignedInt readyFrame = spmInterface->getReadyFrame();

#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
				// Everything is ready if timers are debug off'd
				if( ! TheGlobalData->m_specialPowerUsesDelay )
					readyFrame = 0;
#endif

				// A disabled guy should only be considered as a last resort.  We do not want him counted here
				// so that Disabled guys do not go in to the number on the shortcut button.
				if( obj->isDisabled() )
					readyFrame = UINT_MAX - 10;

				if( readyFrame < TheGameLogic->getFrame())
				{
					//This special power is ready now and matches, so simply return the
					//first one.
					info->numReady++;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void doFindMostReadyWeaponForThing( Object *obj, void *userData )
{
	PlayerObjectFindInfo* info = (PlayerObjectFindInfo*)userData;

	if( info->highestPercentage >= 100 )
	{
		//We already found the best case scenario, so no need to iterate any more.
		return;
	}

	if( info->thing && info->thing->isEquivalentTo( obj->getTemplate() ) )
	{
		if( !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
				&& !obj->testStatus( OBJECT_STATUS_SOLD )
				&& !obj->isEffectivelyDead() )
		{
			if( obj->hasAnyWeapon() )
			{
				UnsignedInt percentage = obj->getMostPercentReadyToFireAnyWeapon();
				if( percentage > info->highestPercentage )
				{
					//This weapon is more ready than any others we've checked.
					info->obj = obj;
					info->highestPercentage = percentage;
					return;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void doFindMostReadySpecialPowerForThing( Object *obj, void *userData )
{
	PlayerObjectFindInfo* info = (PlayerObjectFindInfo*)userData;

	if( info->highestPercentage >= 100 )
	{
		//We already found the best case scenario, so no need to iterate any more.
		return;
	}

	if( info->thing && info->thing->isEquivalentTo( obj->getTemplate() ) )
	{
		if( !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
				&& !obj->testStatus( OBJECT_STATUS_SOLD )
				&& !obj->isEffectivelyDead() )
		{
			// search the modules for the one with the matching template
			for( BehaviorModule** m = obj->getBehaviorModules(); *m; ++m )
			{
				SpecialPowerModuleInterface* sp = (*m)->getSpecialPower();
				if (!sp)
					continue;

				UnsignedInt percentage = sp->getPercentReady();
				if( percentage > info->highestPercentage )
				{
					//This weapon is more ready than any others we've checked.
					info->obj = obj;
					info->highestPercentage = percentage;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void doFindExistingObjectWithThingTemplate( Object *obj, void *userData )
{
	PlayerObjectFindInfo* info = (PlayerObjectFindInfo*)userData;

	if( info->obj )
	{
		//We already found a matching obj, so return
		return;
	}

	if( info->thing && info->thing->isEquivalentTo( obj->getTemplate() ) )
	{
		if( !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
				&& !obj->testStatus( OBJECT_STATUS_SOLD )
				&& !obj->isEffectivelyDead() )
		{
			//We found one.
			info->obj = obj;
		}
	}
}

//-------------------------------------------------------------------------------------------------
Object* Player::findNaturalCommandCenter()
{
	PlayerObjectFindInfo info;
	info.player = this;
	info.obj = nullptr;
	iterateObjects(doFindCommandCenter, &info);
	return info.obj;
}

//-------------------------------------------------------------------------------------------------
Object* Player::findMostReadyShortcutSpecialPowerOfType( SpecialPowerType spType )
{
	PlayerObjectFindInfo info;
	info.player = this;
	info.obj = nullptr;
	info.spType = spType;
	info.lowestReadyFrame = 0xffffffff;
	iterateObjects( doFindSpecialPowerSourceObject, &info );
	return info.obj;
}

//-------------------------------------------------------------------------------------------------
Object* Player::findMostReadyShortcutWeaponForThing( const ThingTemplate *thing, UnsignedInt &mostReadyPercentage )
{
	PlayerObjectFindInfo info;
	info.player = this;
	info.obj = nullptr;
	info.thing = thing;
	info.highestPercentage = 0;
	iterateObjects( doFindMostReadyWeaponForThing, &info );
	mostReadyPercentage = info.highestPercentage;
	return info.obj;
}

//-------------------------------------------------------------------------------------------------
Object* Player::findMostReadyShortcutSpecialPowerForThing( const ThingTemplate *thing, UnsignedInt &mostReadyPercentage )
{
	PlayerObjectFindInfo info;
	info.player = this;
	info.obj = nullptr;
	info.thing = thing;
	info.highestPercentage = 0;
	iterateObjects( doFindMostReadySpecialPowerForThing, &info );
	mostReadyPercentage = info.highestPercentage;
	return info.obj;
}

//-------------------------------------------------------------------------------------------------
Object* Player::findAnyExistingObjectWithThingTemplate( const ThingTemplate *thing )
{
	PlayerObjectFindInfo info;
	info.player = this;
	info.obj = nullptr;
	info.thing = thing;
	iterateObjects( doFindExistingObjectWithThingTemplate, &info );
	return info.obj;
}

//-------------------------------------------------------------------------------------------------
// Finds a short-cut firing special power of any type arbitrarily.
//-------------------------------------------------------------------------------------------------
Bool Player::hasAnyShortcutSpecialPower()
{
	PlayerObjectFindInfo info;
	info.player = this;
	info.obj = nullptr;
	info.spType = SPECIAL_INVALID; //Invalid dictates that we don't care about the type.
	info.lowestReadyFrame = 0xffffffff;
	iterateObjects( doFindSpecialPowerSourceObject, &info );
	return info.obj;
}

//-------------------------------------------------------------------------------------------------
Int Player::countReadyShortcutSpecialPowersOfType( SpecialPowerType spType )
{
	PlayerObjectFindInfo info;
	info.spType = spType;
	info.numReady = 0;
	iterateObjects( doCountSpecialPowersReady, &info );
	return info.numReady;
}

//-------------------------------------------------------------------------------------------------
/** Difficulty level for this player */
//-------------------------------------------------------------------------------------------------
GameDifficulty Player::getPlayerDifficulty() const
{
	if (m_ai)
	{
		return m_ai->getAIDifficulty();
	}
	return TheScriptEngine->getGlobalDifficulty();
}

//-------------------------------------------------------------------------------------------------
/** Do any bridges need repair, and if so repair them. */
//-------------------------------------------------------------------------------------------------
Bool Player::checkBridges(Object *unit, Waypoint *way)
{
	return m_ai?m_ai->checkBridges(unit, way):false;
}

//-------------------------------------------------------------------------------------------------
/** Do any bridges need repair, and if so repair them. */
//-------------------------------------------------------------------------------------------------
Bool Player::getAiBaseCenter(Coord3D *pos)
{
	return m_ai?m_ai->getBaseCenter(pos):false;
}

//-------------------------------------------------------------------------------------------------
/** Repair bridge or structure. */
//-------------------------------------------------------------------------------------------------
void Player::repairStructure(ObjectID structureID)
{
	if (m_ai)
	{
		m_ai->repairStructure(structureID);
	}
}

//-------------------------------------------------------------------------------------------------
/** A unit was just created and is ready to control */
//-------------------------------------------------------------------------------------------------
void Player::onUnitCreated( Object *factory, Object *unit )
{
	// When a a unit is completed, it becomes "real" as far as scripting is
	// concerned. jba.
	TheScriptEngine->notifyOfObjectCreationOrDestruction();

	// increment our scorekeeper
	m_scoreKeeper.addObjectBuilt(unit);

	// ai notification callback
	if( m_ai )
		m_ai->onUnitProduced( factory, unit );
}


//-------------------------------------------------------------------------------------------------
/** Is the nearest supply source safe? */
//-------------------------------------------------------------------------------------------------
Bool Player::isSupplySourceSafe( Int minSupplies )
{
	// ai query
	if( m_ai )
		return m_ai->isSupplySourceSafe( minSupplies );
	return true;
}

//-------------------------------------------------------------------------------------------------
/** Is a supply source attacked? */
//-------------------------------------------------------------------------------------------------
Bool Player::isSupplySourceAttacked()
{
	// ai query
	if( m_ai )
		return m_ai->isSupplySourceAttacked();
	return false;
}

//-------------------------------------------------------------------------------------------------
/** Set delay between team production */
//-------------------------------------------------------------------------------------------------
void Player::setTeamDelaySeconds(Int delay  )
{
	// ai action
	if( m_ai )
		m_ai->setTeamDelaySeconds( delay );
}

//-------------------------------------------------------------------------------------------------
/** Guard supply center */
//-------------------------------------------------------------------------------------------------
void Player::guardSupplyCenter( Team *team, Int minSupplies  )
{
	// ai action
	if( m_ai )
		m_ai->guardSupplyCenter( team, minSupplies );
}

//-------------------------------------------------------------------------------------------------
/** A team is about to be destroyed */
//-------------------------------------------------------------------------------------------------
void Player::preTeamDestroy( const Team *team )
{
	// ai notification callback
	if( m_ai )
		m_ai->aiPreTeamDestroy( team );

	// TheSuperHackers @bugfix Mauller/Xezon 03/05/2025 Clear the default team to prevent dangling pointer usage
	if( m_defaultTeam == team )
		m_defaultTeam = nullptr;
}

//-------------------------------------------------------------------------------------------------
/// a structure was just created, but is under construction
//-------------------------------------------------------------------------------------------------
void Player::onStructureCreated( Object *builder, Object *structure )
{

}

//-------------------------------------------------------------------------------------------------
/// a structure that was under construction has become completed
//-------------------------------------------------------------------------------------------------
void Player::onStructureConstructionComplete( Object *builder, Object *structure, Bool isRebuild )
{
	// When a a structure is completed, it becomes "real" as far as scripting is
	// concerned. jba.
	TheScriptEngine->notifyOfObjectCreationOrDestruction();

	// Update the pathfind footprint.  Sometimes when rubble has to be removed
	// to build, the initial footprint while building is goofy.  jba.
	TheAI->pathfinder()->removeObjectFromPathfindMap(structure);
	TheAI->pathfinder()->addObjectToPathfindMap(structure);

	// increment our scorekeeper
	if (isRebuild == FALSE) {
		m_scoreKeeper.addObjectBuilt(structure);
		m_scoreKeeper.addMoneySpent(structure->getTemplate()->calcCostToBuild(this));
	}

	structure->friend_adjustPowerForPlayer(TRUE);

	// ai notification callback
	if( m_ai )
		m_ai->onStructureProduced( builder, structure );

	// the GUI needs to re-evaluate the information being displayed to the user now
	if( TheControlBar )
		TheControlBar->markUIDirty();

	// This object may require us to play some EVA sounds.
	Player *localPlayer = ThePlayerList->getLocalPlayer();
	if (localPlayer == structure->getControllingPlayer() || localPlayer->getRelationship(structure->getTeam()) != ENEMIES)
		return;

	// We need to play some EVA sounds.
	if (structure->hasSpecialPower(SPECIAL_PARTICLE_UPLINK_CANNON))
		TheEva->setShouldPlay(EVA_SuperweaponDetected_ParticleCannon);

	if (structure->hasSpecialPower(SPECIAL_NEUTRON_MISSILE))
		TheEva->setShouldPlay(EVA_SuperweaponDetected_Nuke);

	if (structure->hasSpecialPower(SPECIAL_SCUD_STORM))
		TheEva->setShouldPlay(EVA_SuperweaponDetected_ScudStorm);

}

//=============================================================================
void Player::onStructureUndone(Object *structure)
{
	m_scoreKeeper.removeObjectBuilt(structure);
}

//=============================================================================
void Player::addTeamToList(TeamPrototype* team)
{
	for( PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it )
	{
		if (team == *it)
			return;	// already present
	}

	m_playerTeamPrototypes.push_back(team);
}

//=============================================================================
void Player::removeTeamFromList(TeamPrototype* team)
{
	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			it != m_playerTeamPrototypes.end(); ++it)
	{
		if (team == *it)
		{
			m_playerTeamPrototypes.erase(it);
			return;
		}
	}
}

//=============================================================================
void Player::healAllObjects()
{
	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		(*it)->healAllObjects();
	}
}

//=============================================================================
void Player::iterateObjects( ObjectIterateFunc func, void *userData ) const
{
	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		(*it)->iterateObjects( func, userData );
	}
}

//=============================================================================
void Player::countObjectsByThingTemplate(Int numTmplates, const ThingTemplate* const * things, Bool ignoreDead, Int *counts, Bool ignoreUnderConstruction ) const
{
	Int i;

	for (i = 0; i < numTmplates; ++i)
		counts[i] = 0;

	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end();
			 ++it)
	{
		(*it)->countObjectsByThingTemplate(numTmplates, things, ignoreDead, counts, ignoreUnderConstruction);
	}
}

//=============================================================================
Int Player::countBuildings()
{
	int retVal = 0;

	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		retVal += (*it)->countBuildings();
	}
	return retVal;
}

//=============================================================================
Int Player::countObjects(KindOfMaskType setMask, KindOfMaskType clearMask)
{
	int retVal = 0;

	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		retVal += (*it)->countObjects(setMask, clearMask);
	}
	return retVal;
}

//=============================================================================
Object *Player::findClosestByKindOf( Object *queryObject, KindOfMaskType setMask, KindOfMaskType clearMask )
{
	if( queryObject == nullptr )
		return nullptr;

	ClosestKindOfData data;
	data.m_setKindOf = setMask;
	data.m_clearKindOf = clearMask;
	data.m_source = queryObject;

	// Magic presto!  data ends up with the answer in it!
	iterateObjects( findClosestKindOf, &data );

	return data.m_closest;
}

//=============================================================================
Bool Player::hasAnyBuildings() const
{
	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		if ((*it)->hasAnyBuildings()) {
			return true;
		}
	}
	return false;
}

//=============================================================================
Bool Player::hasAnyBuildings(KindOfMaskType kindOf) const
{
	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		if ((*it)->hasAnyBuildings(kindOf)) {
			return true;
		}
	}
	return false;
}

//=============================================================================
Bool Player::hasAnyUnits() const
{
	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		if ((*it)->hasAnyUnits()) {
			return true;
		}
	}
	return false;
}

//=============================================================================
Bool Player::hasAnyObjects() const
{
	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		if ((*it)->hasAnyObjects()) {
			return true;
		}
	}
	return false;
}

//=============================================================================
Bool Player::hasAnyBuildFacility() const
{
	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		if ((*it)->hasAnyBuildFacility())
			return true;
	}
	return false;
}

//=============================================================================
void Player::updateTeamStates()
{
	for (PlayerTeamList::const_iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		(*it)->updateState();
	}
}

//=============================================================================
Bool Player::isLocalPlayer() const
{
	return this == ThePlayerList->getLocalPlayer();
}

//=============================================================================
void Player::setListInScoreScreen(Bool listInScoreScreen)
{
	m_listInScoreScreen = listInScoreScreen;
}

//=============================================================================
Bool Player::getListInScoreScreen()
{
	return m_listInScoreScreen;
}

//=============================================================================
UnsignedInt Player::getSupplyBoxValue()
{
	/// @todo This would be the hookup for difficulty level modifiers and special economy buildings
	return TheGlobalData->m_baseValuePerSupplyBox;
}

//=============================================================================
Real Player::getProductionCostChangePercent( AsciiString buildTemplateName ) const
{
  ProductionChangeMap::const_iterator it = m_productionCostChanges.find(NAMEKEY(buildTemplateName));
  if (it != m_productionCostChanges.end())
	{
		return (*it).second;
	}

	return 0.0f;
}

//=============================================================================
Real Player::getProductionTimeChangePercent( AsciiString buildTemplateName ) const
{
  ProductionChangeMap::const_iterator it = m_productionTimeChanges.find(NAMEKEY(buildTemplateName));
  if (it != m_productionTimeChanges.end())
	{
		return (*it).second;
	}

	return 0.0f;
}

//=============================================================================
VeterancyLevel Player::getProductionVeterancyLevel( AsciiString buildTemplateName ) const
{
	NameKeyType templateNameKey = NAMEKEY(buildTemplateName);
  ProductionVeterancyMap::const_iterator it = m_productionVeterancyLevels.find(templateNameKey);
  if (it != m_productionVeterancyLevels.end())
	{
		return (*it).second;
	}

	return LEVEL_FIRST;
}

//=============================================================================
void Player::friend_setSkillset(Int skillSet)
{
	if (m_ai) {
		m_ai->selectSkillset(skillSet);
	}
}

//=============================================================================
void Player::setUnitsShouldHunt(Bool unitsShouldHunt, CommandSourceType source)
{
	m_unitsShouldHunt = unitsShouldHunt;

	Coord3D pos;
	ThePartitionManager->getMostValuableLocation(getPlayerIndex(), ALLOW_ENEMIES, VOT_CashValue, &pos);
	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance()) {
				Object *obj = iterObj.cur();
				if (!obj) {
					continue;
				}

				KindOfMaskType disqualifyingKindofs;
				disqualifyingKindofs.set(KINDOF_DOZER);
				disqualifyingKindofs.set(KINDOF_HARVESTER);
				disqualifyingKindofs.set(KINDOF_IGNORES_SELECT_ALL);

				if (obj->isAnyKindOf( disqualifyingKindofs )) {
					continue;	// Harvesters, dozers etc.
				}

				obj->leaveGroup();
				AIUpdateInterface *ai = obj->getAIUpdateInterface();
				if (ai) {
					if (unitsShouldHunt) {
						ai->aiHunt(source);
					} else {
						ai->aiIdle(source);
					}
				}
			}
		}
	}
}

//=============================================================================
void Player::killPlayer()
{
	PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
	for (; it != m_playerTeamPrototypes.end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			team->evacuateTeam(); // force containers on team to dump contents
		}
	}

	m_isPlayerDead = TRUE; // this is so OCLs don't ever again spawn useful units for us.

	for (it = m_playerTeamPrototypes.begin(); it != m_playerTeamPrototypes.end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			team->killTeam();
		}
	}
	if (TheGameLogic->isInSinglePlayerGame()) {
		if (getPlayerType()==PLAYER_COMPUTER) {
			// This is an AI player in a solo mission - leave him alive so he can be used later. jba.
			m_isPlayerDead = FALSE; // this is so we can later spawn useful units for us.
			return;
		}
	}
	if (isLocalPlayer() && !TheGameLogic->isInShellGame())
	{
		// TheSuperHackers @bugfix helmutbuhler 29/03/2025
		// Avoid multiplayer mismatch on surrender in team games when owning a base structure that has
		// been cleared of infantry before, by disabling this bugged logic.
		//
		// Note that becomingLocalPlayer calls recalcApparentControllingPlayer on all objects with a
		// contain, which does two things:
		//   1. Reset the teams of all objects with empty contains
		//   2. Update m_hideGarrisonedStateFromNonallies
		//
		// Likely, the original intent was to reveal hidden garrisoned buildings for the new observer.
		// But this does not appear to work anyway, so this call is useless.
		//
		// There is another bug: The Contain modules keep track of the object's team before any units
		// got inside. And that team does not get updated when the player is killed and the units
		// transfer to another team. That means resetting the team sets it to neutral, and when this
		// is done only for the local player, then a mismatch occurs.
		//
		// When fixing disguises for observers or fixing the team bug in the Contain classes, then this
		// code can be used again.
#if 0
		becomingLocalPlayer(TRUE); // recalc disguises, etc
#endif

		if (TheControlBar )
		{
			if (isPlayerActive())
			{
				TheControlBar->setControlBarSchemeByPlayer(this);
			}
			else
			{
				TheControlBar->setControlBarSchemeByPlayerTemplate(ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY("FactionObserver")));
			}
		}

	}

	m_money.withdraw(m_money.countMoney()); // force $$$ to 0 on death
}

//=============================================================================
void Player::setObjectsEnabled(AsciiString templateTypeToAffect, Bool enable)
{
	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance()) {
				Object *obj = iterObj.cur();
				if (!obj) {
					continue;
				}

				if (obj->getTemplate()->getName().compare(templateTypeToAffect) == 0)
				{
					obj->setScriptStatus( OBJECT_STATUS_SCRIPT_DISABLED, !enable );
				}
			}
		}
	}
}

//=============================================================================
static void cancelUpgradeInProduction(Object* obj, void* userData)
{
	const UpgradeTemplate* upgradeTemplate = static_cast<const UpgradeTemplate*>(userData);
	ProductionUpdateInterface* pui = ProductionUpdate::getProductionUpdateInterfaceFromObject(obj);

	if (pui && pui->isUpgradeInQueue(upgradeTemplate))
	{
		pui->cancelUpgrade(upgradeTemplate);
	}
}

//=============================================================================
void Player::transferAssetsFromThat(Player *that)
{
	Team *defaultTeam = getDefaultTeam();
	if (!defaultTeam) {
		return;
	}

#if !RETAIL_COMPATIBLE_CRC
	// TheSuperHackers @bugfix Stubbjax 03/02/2026 Cancel any in-progress player upgrades 'that'
	// player currently has in progress that 'this' player already has in progress or completed.
	std::vector<const UpgradeTemplate*> upgradesToCancel;
	for (Upgrade* upgrade = that->m_upgradeList; upgrade; upgrade = upgrade->friend_getNext())
	{
		const UpgradeTemplate* upgradeTemplate = upgrade->getTemplate();

		if (upgrade->getStatus() == UPGRADE_STATUS_IN_PRODUCTION
			&& upgradeTemplate->getUpgradeType() == UPGRADE_TYPE_PLAYER
			&& (hasUpgradeComplete(upgradeTemplate) || hasUpgradeInProduction(upgradeTemplate)))
		{
			upgradesToCancel.push_back(upgradeTemplate);
		}
	}

	for (std::vector<const UpgradeTemplate*>::iterator cancelIt = upgradesToCancel.begin(); cancelIt != upgradesToCancel.end(); ++cancelIt)
	{
		const UpgradeTemplate* upgradeTemplate = *cancelIt;
		that->iterateObjects(cancelUpgradeInProduction, const_cast<UpgradeTemplate*>(upgradeTemplate));
	}

	// TheSuperHackers @bugfix Stubbjax 03/02/2026 Ensure the in-progress upgrade mask is copied from 'that'
	// player to 'this' player to prevent duplicate player upgrades being purchased.
	m_upgradesInProgress.set(that->m_upgradesInProgress);
#endif

	std::list<Object *> objsToTransfer;

	// let's not transfer beacons
	const ThingTemplate *beaconTemplate = TheThingFactory->findTemplate( that->getPlayerTemplate()->getBeaconTemplate() );

	// transfer all his units.
	for (PlayerTeamList::iterator it = that->m_playerTeamPrototypes.begin();
			 it != that->m_playerTeamPrototypes.end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance())
		{
			Team *team = iter.cur();
			if (!team)
			{
				continue;
			}

			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance())
			{
				Object *obj = iterObj.cur();
				if (!obj || obj->getTemplate()->isEquivalentTo(beaconTemplate))  // don't transfer nullptr objs or beacons
				{
					continue;
				}
				objsToTransfer.push_back(obj);
			}
		}
	}

	for (std::list<Object *>::iterator itObjs = objsToTransfer.begin(); itObjs != objsToTransfer.end(); ++itObjs) {
		(*itObjs)->setTeam(defaultTeam);
	}

	// transfer all his money
	UnsignedInt allMoney = that->getMoney()->countMoney();
	that->getMoney()->withdraw(allMoney);
	getMoney()->deposit(allMoney, TRUE, FALSE);
}

//=============================================================================
void Player::garrisonAllUnits(CommandSourceType source)
{
	PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_STRUCTURE), KINDOFMASK_NONE);
	PartitionFilter *filters[] = { &f1, nullptr };

	Coord3D pos = {50.0, 50.0, 50.0};
/// @todo srj -- we should really use iterateAllObjects() here instead, but I have no time to
// test such a change... make someday
	ObjectIterator *iterBuilding = ThePartitionManager->iterateObjectsInRange(&pos, 1e9f, FROM_CENTER_3D, filters, ITER_SORTED_NEAR_TO_FAR);
	MemoryPoolObjectHolder hold(iterBuilding);

	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance()) {
				Object *obj = iterObj.cur();
				if (!obj) {
					continue;
				}

				AIUpdateInterface *ai = obj->getAIUpdateInterface();
				if (!ai) {
					continue;
				}

				for (Object *theBuilding = iterBuilding->first(); theBuilding; theBuilding = iterBuilding->next())
				{
					ContainModuleInterface *contain = theBuilding->getContain();
					if (contain)
					{
						PlayerMaskType player = contain->getPlayerWhoEntered();
						if (!((player == 0) || (player == obj->getControllingPlayer()->getPlayerMask()))) {
							continue;
						}
					}
					if( !TheActionManager->canEnterObject( obj, theBuilding, source, CHECK_CAPACITY ) )
					{
						continue;
					}

					ai->aiEnter(theBuilding, source);
				}
			}
		}
	}
}

//=============================================================================
void Player::ungarrisonAllUnits(CommandSourceType source)
{
	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance()) {
				Object *obj = iterObj.cur();
				if (!obj) {
					continue;
				}

				AIUpdateInterface *ai = obj->getAIUpdateInterface();
				if (!ai) {
					continue;
				}

				// Tell everything that has stuff in it to kick them all out.  You can't tell the people to get
				// out of what they are in, because they may not know, or they may be two transports deep, and their
				// transport may not know where they are.
				// And check for building so you don't unload transports in the ungarrison command
				if( obj->isKindOf( KINDOF_STRUCTURE ) )
					ai->aiEvacuate( FALSE, source);
			}
		}
	}
}


//=============================================================================
void Player::setUnitsShouldIdleOrResume(Bool idle)
{
	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance())
		{
			Team *team = iter.cur();
			if (!team)
				continue;

			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance())
			{
				Object *obj = iterObj.cur();
				if (!obj)
					continue;

				if (obj->isKindOf(KINDOF_STRUCTURE))
					continue;

				AIUpdateInterface *ai = obj->getAIUpdateInterface();
				if (!ai)
					continue;

				if (idle)
				{
					// force it to move to its position to make it stop.
 					ai->aiMoveToPosition(obj->getPosition(), CMD_FROM_SCRIPT);
				}
				else
				{
					// Here is the special bit for this exit style, force wanting on SupplyTruck types
					if (ai->isIdle())
					{
						SupplyTruckAIInterface* supplyTruckAI = ai->getSupplyTruckAIInterface();
						if( supplyTruckAI )
							supplyTruckAI->setForceWantingState(true);
					}
				}
			}
		}
	}
}

//=============================================================================
void Player::sellEverythingUnderTheSun()
{
	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance()) {
				Object *obj = iterObj.cur();
				if (!obj) {
					continue;
				}

				// sell the object
				TheBuildAssistant->sellObject( obj );
			}
		}
	}
}


//=============================================================================
Bool Player::allowedToBuild(const ThingTemplate *tmplate) const
{
	if (!m_canBuildBase && tmplate->isKindOf(KINDOF_STRUCTURE)) {
		return false;
	}

	if (!m_canBuildUnits && !tmplate->isKindOf(KINDOF_STRUCTURE)) {
		return false;
	}

	return true;
}

//=============================================================================
void Player::buildSpecificTeam( TeamPrototype *teamProto)
{
	if (m_ai)
	{
		// Do a priority build.
		m_ai->buildSpecificAITeam(teamProto, true);
	}
}

//=============================================================================
void Player::buildBaseDefense(Bool flank)
{
	if (m_ai)
	{
		// Do a priority build.
		m_ai->buildAIBaseDefense(flank);
	}
}

//=============================================================================
void Player::buildBaseDefenseStructure(const AsciiString &thingName, Bool flank)
{
	if (m_ai)
	{
		// Do a priority build.
		m_ai->buildAIBaseDefenseStructure(thingName, flank);
	}
}

//=============================================================================
void Player::buildSpecificBuilding(const AsciiString &thingName)
{
	if (m_ai)
	{
		// Do a priority build.
		m_ai->buildSpecificAIBuilding(thingName);
	}
}

//=============================================================================
void Player::buildBySupplies(Int minimumCash, const AsciiString &thingName)
{
	if (m_ai)
	{
		m_ai->buildBySupplies(minimumCash, thingName);
	}
}

//=============================================================================
void Player::buildUpgrade( const AsciiString &upgrade)
{
	if (m_ai)
	{
		m_ai->buildUpgrade(upgrade);
	}
}

//=============================================================================
void Player::recruitSpecificTeam( TeamPrototype *teamProto, Real recruitRadius)
{
	if (m_ai)
	{
		// Do a priority build.
		m_ai->recruitSpecificAITeam(teamProto, recruitRadius);
	}
}

//=============================================================================
void Player::doBountyForKill(const Object* killer, const Object* victim)
{
	if (!killer || !victim)
		return;

	// srj sez: per dustin, no experience (et al) for killing things under construction.
	if (victim->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return;

	Int costToBuild = victim->getTemplate()->calcCostToBuild(victim->getControllingPlayer());
#if RETAIL_COMPATIBLE_CRC
	Int bounty = REAL_TO_INT_CEIL(costToBuild * m_cashBountyPercent);
#else
	// TheSuperHackers @bugfix Stubbjax 20/02/2026 Subtract epsilon to ensure bounty is rounded up correctly.
	Int bounty = WWMath::Ceil((costToBuild * m_cashBountyPercent) - WWMATH_EPSILON);
#endif

	if( bounty )
	{

		getMoney()->deposit( bounty );
		m_scoreKeeper.addMoneyEarned( bounty );

		//Display cash income floating over the recipient.
		UnicodeString moneyString;
		moneyString.format( TheGameText->fetch( "GUI:AddCash" ), bounty );
		Coord3D pos;
		pos.zero();
		pos.add( *killer->getPosition() );
		pos.z += 10.0f; //add a little z to make it show up above the unit.
		TheInGameUI->addFloatingText( moneyString, &pos, GameMakeColor( 255, 255, 0, 255 ) );
	}
}

//=============================================================================
Bool Player::hasPrereqsForScience(ScienceType t) const
{
	return TheScienceStore->playerHasPrereqsForScience(this, t);
}

//=============================================================================
/// returns TRUE if the player gained/lost levels as a result.
Bool Player::addSkillPoints(Int delta)
{
	delta = REAL_TO_INT_CEIL(m_skillPointsModifier * INT_TO_REAL(delta));

	if( delta == 0 )
		return false;

	Int levelCap = min( TheGameLogic->getRankLevelLimit(), TheRankInfoStore->getRankLevelCount() );
	Int pointCap = TheRankInfoStore->getRankInfo(levelCap)->m_skillPointsNeeded; // Cap at lowest point of cap level, not highest.

	Bool levelGained = FALSE;
	m_skillPoints = min( pointCap, (m_skillPoints + delta) );
	while( m_skillPoints >= m_levelUp )
	{
		// LevelUp gets increased as a side effect of setRankLevel, and this won't infinitely loop,
		// because when there are no more levels to be gained, m_levelUp is set to INT_MAX.
		setRankLevel( m_rankLevel + 1 );
		levelGained = TRUE;
	}

	return levelGained;
}

//=============================================================================
/// returns TRUE if the player gained/lost levels as a result.
Bool Player::addSkillPointsForKill(const Object* killer, const Object* victim)
{
	if (!killer || !victim)
		return false;

	// srj sez: per dustin, no experience (et al) for killing things under construction.
	if (victim->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return false;

	Int victimLevel = victim->getVeterancyLevel();
	Int skillValue = victim->getTemplate()->getSkillPointValue(victimLevel);

	return addSkillPoints(skillValue);
}

//=============================================================================
void Player::resetSciences()
{
	m_sciences.clear();

	if (getPlayerTemplate())
		m_sciences = getPlayerTemplate()->getIntrinsicSciences();

	for (Int i = 1; i <= m_rankLevel; ++i)
	{
		const RankInfo* rank = TheRankInfoStore->getRankInfo(i);
		if (rank)
		{
			for (ScienceVec::const_iterator it = rank->m_sciencesGranted.begin(); it != rank->m_sciencesGranted.end(); ++it)
			{
				addScience(*it);
			}
		}
	}

	for (ScienceVec::const_iterator it = m_sciences.begin(); it != m_sciences.end(); ++it)
		TheScriptEngine->notifyOfAcquiredScience(getPlayerIndex(), *it);
}

//=============================================================================
/// returns TRUE if sciences were gained/lost.
Bool Player::addScience(ScienceType science)
{
	if (hasScience(science))
		return false;

	//DEBUG_LOG(("Adding Science %s",TheScienceStore->getInternalNameForScience(science).str()));

	m_sciences.push_back(science);

	// 'wake up' any special powers controlled by, well, stuff
	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance())
		{
			Team *team = iter.cur();
			if (!team)
				continue;

			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance())
			{
				Object *obj = iterObj.cur();
				if (!obj)
					continue;

				for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
				{
					SpecialPowerModuleInterface* sp = (*m)->getSpecialPower();
					if (!sp)
						continue;

					if (sp->getRequiredScience() == science)
					{
						// Turn on the power, and set it to be instantly ready, because that is cool.
						sp->onSpecialPowerCreation();
						sp->setReadyFrame( TheGameLogic->getFrame() );
					}
				}
			}
		}

		TheControlBar->markUIDirty();// Refresh the UI to show new cameos, etc

	}

	// notify the script engine
	TheScriptEngine->notifyOfAcquiredScience(getPlayerIndex(), science);

	return true;
}

//=============================================================================
void Player::addSciencePurchasePoints(Int delta)
{
	//DEBUG_LOG(("Adding SciencePurchasePoints %d -> %d",m_sciencePurchasePoints,m_sciencePurchasePoints+delta));
	Int oldSPP = m_sciencePurchasePoints;
	m_sciencePurchasePoints += delta;
	if (m_sciencePurchasePoints < 0)
		m_sciencePurchasePoints = 0;

	if (oldSPP != m_sciencePurchasePoints && TheControlBar != nullptr)
		TheControlBar->onPlayerSciencePurchasePointsChanged(this);

}

//=============================================================================
Bool Player::attemptToPurchaseScience(ScienceType science)
{
	if (!isCapableOfPurchasingScience(science))
	{
		DEBUG_CRASH(("isCapableOfPurchasingScience: need other prereqs/points to purchase, request is ignored!"));
		return false;
	}

	Int cost = TheScienceStore->getSciencePurchaseCost(science);
	addSciencePurchasePoints(-cost);
	addScience(science);
	return true;
}

//=============================================================================
Bool Player::grantScience(ScienceType science)
{
	if (!TheScienceStore->isScienceGrantable(science))
	{
		DEBUG_CRASH(("Cannot grant science %s, since it is marked as nonGrantable.",TheScienceStore->getInternalNameForScience(science).str()));
		return false;	// it's not grantable, so tough, can't have it, even via this method.
	}

	return addScience(science);
}

//=============================================================================
Bool Player::isCapableOfPurchasingScience(ScienceType science) const
{
	if (science == SCIENCE_INVALID)
	{
		return false;
	}

	if (hasScience(science))
	{
		return false;
	}

	if( isScienceDisabled( science ) || isScienceHidden( science ) )
	{
		return false;
	}

	if (!hasPrereqsForScience(science))
	{
		return false;
	}

	Int cost = TheScienceStore->getSciencePurchaseCost(science);
	// purchase cost of zero means "not purchasable!"
	if (cost == 0 || cost > getSciencePurchasePoints())
	{
		return false;
	}

	return true;
}

//=============================================================================
void Player::resetRank()
{
	m_rankLevel = 1;
	m_skillPoints = 0;
	const RankInfo* nextRank = TheRankInfoStore->getRankInfo(m_rankLevel+1);
	m_levelUp = nextRank ? nextRank->m_skillPointsNeeded : INT_MAX;
	m_levelDown = 0;
	m_sciences.clear();
	m_sciencePurchasePoints = getPlayerTemplate() ? getPlayerTemplate()->getIntrinsicSciencePurchasePoints() : 0;
	const RankInfo* curRank = TheRankInfoStore->getRankInfo(m_rankLevel);
	m_sciencePurchasePoints += curRank ? curRank->m_sciencePurchasePointsGranted : 0;
	m_generalName = TheGameText? TheGameText->fetch("SCIENCE:GeneralName"):UnicodeString::TheEmptyString;
	resetSciences();
}

//=============================================================================
/// returns TRUE if rank level really changed.
Bool Player::setRankLevel(Int newLevel)
{
	if (newLevel < 1)
		newLevel = 1;
	else if (newLevel > TheRankInfoStore->getRankLevelCount())
		newLevel = TheRankInfoStore->getRankLevelCount();

	if (newLevel > TheGameLogic->getRankLevelLimit())
		newLevel = TheGameLogic->getRankLevelLimit();

	if (newLevel == m_rankLevel)
		return false;

	//DEBUG_LOG(("Set Rank Level %d -> %d",m_rankLevel,newLevel));

	Int oldSPP = m_sciencePurchasePoints;

	if (newLevel < m_rankLevel)
	{
		// when downgrading in rank, you lose everything. oh well...
		resetRank();
	}

	for (Int i = m_rankLevel + 1; i <= newLevel; ++i)
	{
		const RankInfo* rank = TheRankInfoStore->getRankInfo(i);
		DEBUG_ASSERTCRASH(rank, ("rank should never be null here"));
		if (rank)
		{
			//addSciencePurchasePoints(rank->m_sciencePurchasePointsGranted);
			// do this directly, so we can defer the UI notification till the end
			m_sciencePurchasePoints += rank->m_sciencePurchasePointsGranted;
			if (m_sciencePurchasePoints < 0)
				m_sciencePurchasePoints = 0;

			if (m_skillPoints < rank->m_skillPointsNeeded)
				m_skillPoints = rank->m_skillPointsNeeded;

			for (ScienceVec::const_iterator it = rank->m_sciencesGranted.begin(); it != rank->m_sciencesGranted.end(); ++it)
			{
				addScience(*it);
			}

			m_levelDown = rank->m_skillPointsNeeded;
		}
	}

	const RankInfo* nextRank = TheRankInfoStore->getRankInfo(newLevel + 1);
	m_levelUp = nextRank ? nextRank->m_skillPointsNeeded : INT_MAX;
	m_rankLevel = newLevel;

	DEBUG_ASSERTCRASH(m_skillPoints >= m_levelDown && m_skillPoints < m_levelUp, ("hmm, wrong"));
	//DEBUG_LOG(("Rank %d, Skill %d, down %d, up %d",m_rankLevel,m_skillPoints, m_levelDown, m_levelUp));

	if (TheControlBar != nullptr)
	{
		if( m_levelUp )
		{
			//Play an EVA sound
			if (isLocalPlayer())
				TheEva->setShouldPlay( EVA_GeneralLevelUp );
		}

		// notify of rank-change first
		TheControlBar->onPlayerRankChanged(this);

		// then of SPP change, if any
		if (oldSPP != m_sciencePurchasePoints)
			TheControlBar->onPlayerSciencePurchasePointsChanged(this);
	}

	return true;
}

//=============================================================================
Bool Player::hasScience(ScienceType t) const
{
	return std::find(m_sciences.begin(), m_sciences.end(), t) != m_sciences.end();
}

//=============================================================================
Bool Player::isScienceDisabled( ScienceType t ) const
{
	return std::find( m_sciencesDisabled.begin(), m_sciencesDisabled.end(), t ) != m_sciencesDisabled.end();
}

//=============================================================================
Bool Player::isScienceHidden( ScienceType t ) const
{
	return std::find( m_sciencesHidden.begin(), m_sciencesHidden.end(), t ) != m_sciencesHidden.end();
}

//=============================================================================
void Player::setScienceAvailability( ScienceType science, ScienceAvailabilityType type )
{
	ScienceType sType;
	ScienceVec::iterator it;
	Bool found = false;

	//First remove it from disabled sciences if it's there.
	for( it = m_sciencesDisabled.begin(); it != m_sciencesDisabled.end(); ++it )
	{
		sType = *it;
		if( sType == science )
		{
			m_sciencesDisabled.erase( it );
			found = true;
			break;
		}
	}
	if( !found )
	{
		//If still not found, remove it from hidden sciences if it's there.
		for( it = m_sciencesHidden.begin(); it != m_sciencesHidden.end(); ++it )
		{
			sType = *it;
			if( sType == science )
			{
				m_sciencesHidden.erase( it );
				found = true;
				break;
			}
		}
	}

	//At this point, this science shouldn't be disabled nor hidden!
	if( type == SCIENCE_DISABLED )
	{
		//Add science to disabled vec.
		m_sciencesDisabled.push_back( science );
	}
	else if( type == SCIENCE_HIDDEN )
	{
		//Add science to hidden vec.
		m_sciencesHidden.push_back( science );
	}
	else if( type == SCIENCE_AVAILABLE )
	{
		//Do nothing, because being available means that the science isn't in one of the hidden or disabled vecs.
	}
}

//=============================================================================
ScienceAvailabilityType Player::getScienceAvailabilityTypeFromString( const AsciiString& name )
{
	for( Int i = 0; i < SCIENCE_AVAILABILITY_COUNT; i++ )
	{
		if( !name.compareNoCase( ScienceAvailabilityNames[ i ] ) )
		{
			return (ScienceAvailabilityType)i;
		}
	}
	return SCIENCE_AVAILABILITY_INVALID;
}

//=============================================================================
Bool Player::canBuild(const ThingTemplate *tmplate) const
{
	if (!tmplate)
		return false;

	if (!allowedToBuild(tmplate))
		return false;

	if (tmplate->getBuildable() == BSTATUS_NO)
		return false;

	if (tmplate->getBuildable() == BSTATUS_IGNORE_PREREQUISITES)
		return true;

	if (tmplate->getBuildable() == BSTATUS_ONLY_BY_AI && getPlayerType() != PLAYER_COMPUTER)
		return false;

	// else BSTATUS tmplate->getBuildable() == BSTATUS_YES
	{

		// we must satisfy all of the prereqs
		Bool prereqsOK = true;
		for (Int i = 0; i < tmplate->getPrereqCount(); i++)
		{
			const ProductionPrerequisite *pre = tmplate->getNthPrereq(i);
			if (pre->isSatisfied(this) == false )
				prereqsOK = false;
		}

#if defined(RTS_DEBUG)
		if (ignoresPrereqs())
			prereqsOK = true;
#endif

		if (!prereqsOK)
			return false;

	}

	return true;
}

//=================================================================================================
Bool Player::canAffordBuild( const ThingTemplate *whatToBuild ) const
{
	// make sure we have enough money to build this
	const Money *money = getMoney();
	if( whatToBuild->calcCostToBuild( this ) <= money->countMoney() )
	{
		return true;
	}
	return false;
}

//=================================================================================================
void Player::deleteUpgradeList()
{
	Upgrade *next;

	// delete all of the upgrades we have
	while( m_upgradeList )
	{

		next = m_upgradeList->friend_getNext();
		deleteInstance(m_upgradeList);
		m_upgradeList = next;

	}

	// This doesn't call removeUpgrade, so clear these ourselves.
	m_upgradesInProgress.clear();
	m_upgradesCompleted.clear();

}

//=================================================================================================
/** Find an upgrade in our list of upgrades with matching name key */
//=================================================================================================
Upgrade *Player::findUpgrade( const UpgradeTemplate *upgradeTemplate )
{
	Upgrade *upgrade;

	for( upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		if( upgrade->getTemplate() == upgradeTemplate )
			return upgrade;

	return nullptr;

}

//=================================================================================================
/** Does the player have this completed upgrade */
//=================================================================================================
Bool Player::hasUpgradeComplete( const UpgradeTemplate *upgradeTemplate ) const
{
	const UpgradeMaskType& testMask = upgradeTemplate->getUpgradeMask();
	return hasUpgradeComplete( testMask );
}

//=================================================================================================
/**
	Does the player have this completed upgrade.  This form is exposed so Objects can do quick lookups.
*/
//=================================================================================================
Bool Player::hasUpgradeComplete( const UpgradeMaskType& testMask ) const
{
	return m_upgradesCompleted.testForAll( testMask );
}

//=================================================================================================
/** Does the player have this upgrade In Production*/
//=================================================================================================
Bool Player::hasUpgradeInProduction( const UpgradeTemplate *upgradeTemplate )
{
	const UpgradeMaskType& testMask = upgradeTemplate->getUpgradeMask();
	return m_upgradesInProgress.testForAll( testMask );
}

//=================================================================================================
/** Give the player an upgrade or change status on an existing upgrade entry */
//=================================================================================================
Upgrade *Player::addUpgrade( const UpgradeTemplate *upgradeTemplate, UpgradeStatusType status )
{
	Upgrade *u = findUpgrade( upgradeTemplate );

	// if no upgrade instance found, make a new one
	if( u == nullptr )
	{

		// make new one
		u = newInstance(Upgrade)( upgradeTemplate );

		// tie to list
		u->friend_setPrev( nullptr );
		u->friend_setNext( m_upgradeList );
		if( m_upgradeList )
			m_upgradeList->friend_setPrev( u );
		m_upgradeList = u;

	}

	// set the new status for the upgrade
	u->setStatus( status );

	// Update our Bitmasks
	const UpgradeMaskType& newMask = upgradeTemplate->getUpgradeMask();
	if( status == UPGRADE_STATUS_IN_PRODUCTION )
	{
		m_upgradesInProgress.set( newMask );
	}
	else if( status == UPGRADE_STATUS_COMPLETE )
	{
		m_upgradesInProgress.clear( newMask );
		m_upgradesCompleted.set( newMask );
		onUpgradeCompleted( upgradeTemplate );
	}

	return u;

}

//=================================================================================================
/**
	An upgrade just finished, do things like tell all objects to recheck UpgradeModules
*/
void Player::onUpgradeCompleted( const UpgradeTemplate *upgradeTemplate )
{
	for (PlayerTeamList::iterator it = m_playerTeamPrototypes.begin();
			 it != m_playerTeamPrototypes.end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance())
		{
			Team *team = iter.cur();
			if( team == nullptr )
			{
				continue;
			}
			for (DLINK_ITERATOR<Object> iterObj = team->iterate_TeamMemberList(); !iterObj.done(); iterObj.advance())
			{
				Object *obj = iterObj.cur();
				if( obj == nullptr )
				{
					continue;
				}
				// Dear copy-paste monkeys, the meat of this iterate-all-player-objects loop goes twixt the MEAT comments

				obj->updateUpgradeModules();

				// end MEAT
			}
		}
	}
}

//=================================================================================================
/** Remove upgrade from a player */
//=================================================================================================
void Player::removeUpgrade( const UpgradeTemplate *upgradeTemplate )
{
	Upgrade *upgrade = findUpgrade( upgradeTemplate );

	if( upgrade )
	{
		if( upgrade->friend_getNext() )
			upgrade->friend_getNext()->friend_setPrev( upgrade->friend_getPrev() );
		if( upgrade->friend_getPrev() )
			upgrade->friend_getPrev()->friend_setNext( upgrade->friend_getNext() );
		else
			m_upgradeList = upgrade->friend_getNext();

		// Clear this upgrade's bits from our mind
		const UpgradeMaskType& oldMask = upgradeTemplate->getUpgradeMask();
		m_upgradesInProgress.clear( oldMask );
		m_upgradesCompleted.clear( oldMask );

		if( upgrade->getStatus() == UPGRADE_STATUS_COMPLETE )
			onUpgradeRemoved();

		deleteInstance(upgrade);
	}
}


//-------------------------------------------------------------------------------------------------
Bool Player::okToPlayRadarEdgeSound()
{
	return (
		! TheVictoryConditions->hasSinglePlayerBeenDefeated( this )
		&& ! m_isPlayerDead
		&& ! TheInGameUI->isClientQuiet()
		&& TheGameLogic->isInGameLogicUpdate()
		&& TheGameLogic->getFrame() > 0 );

}

//-------------------------------------------------------------------------------------------------
/** The parameter object has just acquired a radar */
//-------------------------------------------------------------------------------------------------
void Player::addRadar( Bool disableProof )
{
	Bool hadRadar = hasRadar();

	// increment count
	++m_radarCount;

	if( disableProof )
		++m_disableProofRadarCount;// Disable proof is also in the normal refcount

	if( !hadRadar && hasRadar()	&& okToPlayRadarEdgeSound() )
	{
		// This player just got radar, so play the "You have Radar!" sound
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_radarOnlineSound;
		soundToPlay.setPlayerIndex(getPlayerIndex());
		TheAudio->addAudioEvent(&soundToPlay);
	}
}

//-------------------------------------------------------------------------------------------------
/** The parameter object has is taking its radar away from the player */
//-------------------------------------------------------------------------------------------------
void Player::removeRadar( Bool disableProof )
{
	Bool hadRadar = hasRadar();

	// decrement count
	DEBUG_ASSERTCRASH( m_radarCount > 0, ("removeRadar: An Object is taking its radar away, but the player radar count says they don't have radar!") );
	--m_radarCount;

	if( disableProof )
		--m_disableProofRadarCount;// Disable proof is also in the normal refcount

	if( hadRadar && !hasRadar()	&& okToPlayRadarEdgeSound() )
	{
		// This player just lost radar, so play the "You lost Radar!" sound
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_radarOfflineSound;
		soundToPlay.setPlayerIndex(getPlayerIndex());
		TheAudio->addAudioEvent(&soundToPlay);
	}
}

//-------------------------------------------------------------------------------------------------
void Player::disableRadar()
{
	Bool hadRadar = hasRadar();
	m_radarDisabled = TRUE;

	if( hadRadar
		&& !hasRadar() && okToPlayRadarEdgeSound() )
	{
		// This player just lost radar, so play the "You lost Radar!" sound
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_radarOfflineSound;
		soundToPlay.setPlayerIndex(getPlayerIndex());
		TheAudio->addAudioEvent(&soundToPlay);
	}
}

//-------------------------------------------------------------------------------------------------
void Player::enableRadar()
{
	Bool hadRadar = hasRadar();
	m_radarDisabled = FALSE;

	if( !hadRadar && hasRadar() && okToPlayRadarEdgeSound() )
	{
		// This player just got radar, so play the "You have Radar!" sound
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_radarOnlineSound;
		soundToPlay.setPlayerIndex(getPlayerIndex());
		TheAudio->addAudioEvent(&soundToPlay);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool Player::hasRadar() const
{
	if( m_radarDisabled  && (m_disableProofRadarCount == 0) )
		return FALSE;// Nope, no matter how many you have, if I say no, you don't have it

	// Otherwise, check if I actually do have it.
	return m_radarCount > 0;
}

//-------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
static void doPowerDisable( Object *obj, void *userData )
{
	Bool disabling = *((Bool*)userData);
	if( obj && obj->isKindOf(KINDOF_POWERED) )
	{
		if( disabling )
			obj->setDisabled( DISABLED_UNDERPOWERED ); //set disabled has a pauseAllSpecialPowers that prevents double pausing
		else
			obj->clearDisabled( DISABLED_UNDERPOWERED );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Player::onPowerBrownOutChange( Bool brownOut )
{
	// Everything that changes due to Player's power supply goes in here.
	if( brownOut )
		disableRadar();
	else
		enableRadar(); //This doesn't give radar necessarily, it just removes the restriction

	iterateObjects( doPowerDisable, &brownOut );// This function is so cool.
}




//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// SEVERAL OBJECTS (LIKE MULTIPLE COMMAND CENTERS) MAY HAVE MATCHING SETS OF SPECIAL POWERS
// THIS KEEPS THEM IN SYNC SO IT LOOKS LIKE EACH SPECIAL POWER IS SHARED BETWEEN ALL

void Player::addNewSharedSpecialPowerTimer( const SpecialPowerTemplate *temp, UnsignedInt frame )
{

	SpecialPowerReadyTimerType newTimer;
	newTimer.m_templateID = temp->getID();
	newTimer.m_readyFrame = frame;
	m_specialPowerReadyTimerList.push_back( newTimer );

}

void Player::resetOrStartSpecialPowerReadyFrame( const SpecialPowerTemplate *temp )
{

	UnsignedInt lookupID = temp->getID();
	UnsignedInt now = TheGameLogic->getFrame();

	SpecialPowerReadyTimerType *timer;
	SpecialPowerReadyTimerListIterator it;
	for( it = m_specialPowerReadyTimerList.begin(); it != m_specialPowerReadyTimerList.end(); ++it )
	{
		timer = &(*it);
		if ( timer->m_templateID == lookupID )
		{
			timer->m_readyFrame = now + temp->getReloadTime();
			return;
		}
	}

	addNewSharedSpecialPowerTimer( temp, now );
}


void Player::expressSpecialPowerReadyFrame( const SpecialPowerTemplate *temp, UnsignedInt frame )
{
	SpecialPowerReadyTimerType *timer;
	SpecialPowerReadyTimerListIterator it;
	for( it = m_specialPowerReadyTimerList.begin(); it != m_specialPowerReadyTimerList.end(); ++it )
	{
		timer = &(*it);
		if ( timer->m_templateID == temp->getID() )
		{
			timer->m_readyFrame = frame;
			return;
		}
	}

	addNewSharedSpecialPowerTimer( temp, frame );
}





//-------------------------------------------------------------------------------------------------
UnsignedInt Player::getOrStartSpecialPowerReadyFrame( const SpecialPowerTemplate *temp)
{

	UnsignedInt lookupID = temp->getID();
	UnsignedInt now = TheGameLogic->getFrame();

	SpecialPowerReadyTimerType *timer;
	SpecialPowerReadyTimerListIterator it;


	UnsignedInt count = 0;
	UnsignedInt timerID = 0xfacade;

	for( it = m_specialPowerReadyTimerList.begin(); it != m_specialPowerReadyTimerList.end(); ++it )
	{
		timer = &(*it);
		++count;

		timerID = timer->m_templateID;

		if ( timerID == lookupID )
		{
			return timer->m_readyFrame;
		}
	}

	addNewSharedSpecialPowerTimer( temp, now );
	return now;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Player::friend_applyDifficultyBonusesForObject(Object* obj, Bool apply) const
{
	if (TheGameLogic->isInSinglePlayerGame())
	{
		Real healthFactor = TheGlobalData->m_soloPlayerHealthBonusForDifficulty[getPlayerType()][getPlayerDifficulty()];
		if (healthFactor != 1.0f)
		{
			BodyModuleInterface* body = obj->getBodyModule();
			if (apply)
				body->setMaxHealth(body->getMaxHealth() * healthFactor, PRESERVE_RATIO);
			else
				body->setMaxHealth(body->getMaxHealth() / healthFactor, PRESERVE_RATIO);
		}
		static const WeaponBonusConditionType wbonus[PLAYERTYPE_COUNT][DIFFICULTY_COUNT] =
		{
			{
				WEAPONBONUSCONDITION_SOLO_HUMAN_EASY,
				WEAPONBONUSCONDITION_SOLO_HUMAN_NORMAL,
				WEAPONBONUSCONDITION_SOLO_HUMAN_HARD
			},
			{
				WEAPONBONUSCONDITION_SOLO_AI_EASY,
				WEAPONBONUSCONDITION_SOLO_AI_NORMAL,
				WEAPONBONUSCONDITION_SOLO_AI_HARD
			}
		};
		if (apply)
			obj->setWeaponBonusCondition(wbonus[getPlayerType()][getPlayerDifficulty()]);
		else
			obj->clearWeaponBonusCondition(wbonus[getPlayerType()][getPlayerDifficulty()]);
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool Player::doesObjectQualifyForBattlePlan( Object *obj ) const
{
	if( m_battlePlanBonuses && obj )
	{
		if( obj->isAnyKindOf( m_battlePlanBonuses->m_validKindOf ) )
		{
			if( !obj->isAnyKindOf( m_battlePlanBonuses->m_invalidKindOf ) )
			{
				return true;
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
void Player::changeBattlePlan( BattlePlanStatus plan, Int delta, const BattlePlanBonusesData *bonus )
{
	DUMPBATTLEPLANBONUSES(bonus, this, nullptr);
	Bool addBonus = false;
	Bool removeBonus = false;
	switch( plan )
	{
		case PLANSTATUS_BOMBARDMENT:
		{
			m_bombardBattlePlans += delta;
			if( m_bombardBattlePlans == 1 && delta == 1 )
			{
				addBonus = true;
			}
			else if( m_bombardBattlePlans == 0 && delta == -1 )
			{
				removeBonus = true;
			}
			break;
		}
		case PLANSTATUS_HOLDTHELINE:
		{
			m_holdTheLineBattlePlans += delta;
			if( m_holdTheLineBattlePlans == 1 && delta == 1 )
			{
				addBonus = true;
			}
			else if( m_holdTheLineBattlePlans == 0 && delta == -1 )
			{
				removeBonus = true;
			}
			break;
		}
		case PLANSTATUS_SEARCHANDDESTROY:
		{
			m_searchAndDestroyBattlePlans += delta;
			if( m_searchAndDestroyBattlePlans == 1 && delta == 1 )
			{
				addBonus = true;
			}
			else if( m_searchAndDestroyBattlePlans == 0 && delta == -1 )
			{
				removeBonus = true;
			}
			break;
		}
	}
	if( addBonus )
	{
		applyBattlePlanBonusesForPlayerObjects( bonus );
	}
	else if( removeBonus )
	{
		//First, inverse the bonuses
		BattlePlanBonusesData invertedBonus = *bonus;

		invertedBonus.m_armorScalar = 1.0f / std::max(bonus->m_armorScalar, 0.01f);
		invertedBonus.m_sightRangeScalar = 1.0f / std::max(bonus->m_sightRangeScalar, 0.01f);
		if (invertedBonus.m_bombardment > 0)
		{
			invertedBonus.m_bombardment = -1;
		}
		if (invertedBonus.m_holdTheLine > 0)
		{
			invertedBonus.m_holdTheLine = -1;
		}
		if (invertedBonus.m_searchAndDestroy > 0)
		{
			invertedBonus.m_searchAndDestroy = -1;
		}

		applyBattlePlanBonusesForPlayerObjects(&invertedBonus);
	}
}

//-------------------------------------------------------------------------------------------------
Int Player::getBattlePlansActiveSpecific( BattlePlanStatus plan ) const
{
	switch( plan )
	{
		case PLANSTATUS_BOMBARDMENT:
		{
			return m_bombardBattlePlans;
		}
		case PLANSTATUS_HOLDTHELINE:
		{
			return m_holdTheLineBattlePlans;
		}
		case PLANSTATUS_SEARCHANDDESTROY:
		{
			return m_searchAndDestroyBattlePlans;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------------------------
static void localApplyBattlePlanBonusesToObject( Object *obj, void *userData )
{
	const BattlePlanBonusesData* bonus = static_cast<const BattlePlanBonusesData*>(userData);
	Object *objectToValidate = obj;
	Object *objectToModify = obj;

	DEBUG_LOG(("localApplyBattlePlanBonusesToObject() - looking at object %d (%s)",
		(objectToValidate)?objectToValidate->getID():INVALID_ID,
		(objectToValidate)?objectToValidate->getTemplate()->getName().str():"<No Object>"));

	//First check if the obj is a projectile -- if so split the
	//object so that the producer is validated, not the projectile.
	Bool isProjectile = obj->isKindOf( KINDOF_PROJECTILE );
	if( isProjectile )
	{
		objectToValidate = TheGameLogic->findObjectByID( obj->getProducerID() );
		DEBUG_LOG(("Object is a projectile - looking at object %d (%s) instead",
			(objectToValidate)?objectToValidate->getID():INVALID_ID,
			(objectToValidate)?objectToValidate->getTemplate()->getName().str():"<No Object>"));
	}
	if( objectToValidate && objectToValidate->isAnyKindOf( bonus->m_validKindOf ) )
	{
		DEBUG_LOG(("Is valid kindof"));
		if( !objectToValidate->isAnyKindOf( bonus->m_invalidKindOf ) )
		{
			DEBUG_LOG(("Is not invalid kindof"));
			//Quite the trek eh? Now we can apply the bonuses!
			if( !isProjectile )
			{
				DEBUG_LOG(("Is not projectile.  Armor scalar is %g", bonus->m_armorScalar));
				//Really important to not apply certain bonuses like health augmentation to projectiles!
				if( bonus->m_armorScalar != 1.0f )
				{
					BodyModuleInterface *body = objectToModify->getBodyModule();
					body->applyDamageScalar( bonus->m_armorScalar );
					CRCDEBUG_LOG(("Applying armor scalar of %g (%8.8X) to object %d (%ls) owned by player %d",
						bonus->m_armorScalar, AS_INT(bonus->m_armorScalar), objectToModify->getID(),
						objectToModify->getTemplate()->getDisplayName().str(),
						objectToModify->getControllingPlayer()->getPlayerIndex()));
					DEBUG_LOG(("After apply, armor scalar is %g", body->getDamageScalar()));
				}
				if( bonus->m_sightRangeScalar != 1.0f )
				{
					objectToModify->setVisionRange( obj->getVisionRange() * bonus->m_sightRangeScalar );
					objectToModify->setShroudClearingRange( obj->getShroudClearingRange() * bonus->m_sightRangeScalar );
				}
			}

			if( bonus->m_bombardment > 0 )
			{
				objectToModify->setWeaponBonusCondition( WEAPONBONUSCONDITION_BATTLEPLAN_BOMBARDMENT );
			}
			else
			{
				objectToModify->clearWeaponBonusCondition( WEAPONBONUSCONDITION_BATTLEPLAN_BOMBARDMENT );
			}
			if( bonus->m_holdTheLine > 0 )
			{
				objectToModify->setWeaponBonusCondition( WEAPONBONUSCONDITION_BATTLEPLAN_HOLDTHELINE );
			}
			else
			{
				objectToModify->clearWeaponBonusCondition( WEAPONBONUSCONDITION_BATTLEPLAN_HOLDTHELINE );
			}
			if( bonus->m_searchAndDestroy > 0 )
			{
				objectToModify->setWeaponBonusCondition( WEAPONBONUSCONDITION_BATTLEPLAN_SEARCHANDDESTROY );
			}
			else
			{
				objectToModify->clearWeaponBonusCondition( WEAPONBONUSCONDITION_BATTLEPLAN_SEARCHANDDESTROY );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
//New object or converted object gaining our current battle plan bonuses.
//-------------------------------------------------------------------------------------------------
void Player::applyBattlePlanBonusesForObject( Object *obj ) const
{
	localApplyBattlePlanBonusesToObject( obj, static_cast<BattlePlanBonusesData*>(m_battlePlanBonuses) );
}

//-------------------------------------------------------------------------------------------------
//Object has just left our team, so remove it's bonuses!
//-------------------------------------------------------------------------------------------------
void Player::removeBattlePlanBonusesForObject( Object *obj ) const
{
	//Create inverted bonuses.
	BattlePlanBonusesData bonus;
	bonus.m_armorScalar = 1.0f / std::max( m_battlePlanBonuses->m_armorScalar, 0.01f );
	bonus.m_sightRangeScalar = 1.0f / std::max( m_battlePlanBonuses->m_sightRangeScalar, 0.01f );
	bonus.m_bombardment = -ALL_PLANS; //Safe to remove as it clears the weapon bonus flag
	bonus.m_searchAndDestroy = -ALL_PLANS; //Safe to remove as it clears the weapon bonus flag
	bonus.m_holdTheLine = -ALL_PLANS; //Safe to remove as it clears the weapon bonus flag
	bonus.m_validKindOf = m_battlePlanBonuses->m_validKindOf;
	bonus.m_invalidKindOf = m_battlePlanBonuses->m_invalidKindOf;

	DUMPBATTLEPLANBONUSES(&bonus, this, obj);
	localApplyBattlePlanBonusesToObject( obj, &bonus );
}

//-------------------------------------------------------------------------------------------------
//Battle plan bonuses changing, so apply to all of our objects!
//-------------------------------------------------------------------------------------------------
void Player::applyBattlePlanBonusesForPlayerObjects( const BattlePlanBonusesData *bonus )
{
	DUMPBATTLEPLANBONUSES(bonus, this, nullptr);

	//Only allocate the battle plan bonuses if we actually use it!
	if( !m_battlePlanBonuses )
	{
		DEBUG_LOG(("Allocating new m_battlePlanBonuses"));
		m_battlePlanBonuses = newInstance( BattlePlanBonuses );
		*static_cast<BattlePlanBonusesData*>(m_battlePlanBonuses) = *bonus;
	}
	else
	{
		DEBUG_LOG(("Adding bonus into existing m_battlePlanBonuses"));
		DUMPBATTLEPLANBONUSES(m_battlePlanBonuses, this, nullptr);
		//Just apply the differences by multiplying the scalars together (kindofs won't change)
		//These bonuses are used for new objects that are created or objects that are transferred
		//to our team.
		m_battlePlanBonuses->m_armorScalar					*= bonus->m_armorScalar;
		m_battlePlanBonuses->m_sightRangeScalar			*= bonus->m_sightRangeScalar;
		m_battlePlanBonuses->m_bombardment					+= bonus->m_bombardment;
		m_battlePlanBonuses->m_bombardment					=	 MAX( 0, m_battlePlanBonuses->m_bombardment );
		m_battlePlanBonuses->m_holdTheLine					+= bonus->m_holdTheLine;
		m_battlePlanBonuses->m_holdTheLine					=	 MAX( 0, m_battlePlanBonuses->m_holdTheLine );
		m_battlePlanBonuses->m_searchAndDestroy			+= bonus->m_searchAndDestroy;
		m_battlePlanBonuses->m_searchAndDestroy			=	 MAX( 0, m_battlePlanBonuses->m_searchAndDestroy );
	}

	DUMPBATTLEPLANBONUSES(m_battlePlanBonuses, this, nullptr);
#if RETAIL_COMPATIBLE_CRC
	iterateObjects( localApplyBattlePlanBonusesToObject, const_cast<BattlePlanBonusesData *>(bonus) );
#else
	// TheSuperHackers @bugfix Stubbjax 05/02/2026 Apply all bonuses the player has rather than just the most recent.
	BattlePlanBonusesData newBonus = *m_battlePlanBonuses;
	newBonus.m_armorScalar = bonus->m_armorScalar;
	newBonus.m_sightRangeScalar = bonus->m_sightRangeScalar;

	iterateObjects(localApplyBattlePlanBonusesToObject, &newBonus);
#endif
}


//-------------------------------------------------------------------------------------------------
/** Create a hotkey team based on this GameMessage */
//-------------------------------------------------------------------------------------------------
void Player::processCreateTeamGameMessage(Int hotkeyNum, const GameMessage *msg) {
	// GameMessage arguments are the object ID's of the objects that are to be in this team.

	if ((hotkeyNum < 0) || (hotkeyNum >= NUM_HOTKEY_SQUADS)) {
		DEBUG_CRASH(("processCreateTeamGameMessage got an invalid hotkey number"));
		return;
	}

	m_squads[hotkeyNum]->clearSquad();

	UnsignedByte numArgs = msg->getArgumentCount();
	for (UnsignedByte i = 0; i < numArgs; ++i) {
		ObjectID objID = msg->getArgument(i)->objectID;
		Object *obj = TheGameLogic->findObjectByID(objID);
		if (obj != nullptr) {
			// first, remove it from any other hotkey squads it is in.
			removeObjectFromHotkeySquad(obj);
			m_squads[hotkeyNum]->addObject(obj);
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Select a hotkey team based on this GameMessage */
//-------------------------------------------------------------------------------------------------
void Player::processSelectTeamGameMessage(Int hotkeyNum) {
	if ((hotkeyNum < 0) || (hotkeyNum >= NUM_HOTKEY_SQUADS)) {
		DEBUG_CRASH(("processSelectTeamGameMessage got an invalid hotkey number"));
		return;
	}

	if (m_squads[hotkeyNum] == nullptr) {
		return;
	}

	m_currentSelection->clearSquad();

	VecObjectPtr objectList = m_squads[hotkeyNum]->getLiveObjects();
	Int numObjs = objectList.size();

	for (Int i = 0; i < numObjs; ++i) {
		m_currentSelection->addObject(objectList[i]);
	}
}

//-------------------------------------------------------------------------------------------------
/** Select a hotkey team based on this GameMessage */
//-------------------------------------------------------------------------------------------------
void Player::processAddTeamGameMessage(Int hotkeyNum) {
	if ((hotkeyNum < 0) || (hotkeyNum >= NUM_HOTKEY_SQUADS)) {
		DEBUG_CRASH(("processAddTeamGameMessage got an invalid hotkey number"));
		return;
	}

	if (m_squads[hotkeyNum] == nullptr) {
		return;
	}

	if (m_currentSelection == nullptr) {
		m_currentSelection = newInstance( Squad );
	}

	VecObjectPtr objectList = m_squads[hotkeyNum]->getLiveObjects();
	Int numObjs = objectList.size();

	for (Int i = 0; i < numObjs; ++i) {
		m_currentSelection->addObject(objectList[i]);
	}
}

//-------------------------------------------------------------------------------------------------
/** Select a hotkey team based on this GameMessage */
//-------------------------------------------------------------------------------------------------
void Player::getCurrentSelectionAsAIGroup(AIGroup *group) {
	if (m_currentSelection != nullptr) {
		m_currentSelection->aiGroupFromSquad(group);
	}
}

//-------------------------------------------------------------------------------------------------
/** Select a hotkey team based on this GameMessage */
//-------------------------------------------------------------------------------------------------
void Player::setCurrentlySelectedAIGroup(AIGroup *group) {
	if (m_currentSelection == nullptr) {
		m_currentSelection = newInstance( Squad );
	}

	m_currentSelection->clearSquad();

	if (group != nullptr) {
		m_currentSelection->squadFromAIGroup(group, true);
	}
}

//-------------------------------------------------------------------------------------------------
/** Select a hotkey team based on this GameMessage */
//-------------------------------------------------------------------------------------------------
Squad *Player::getHotkeySquad(Int squadNumber)
{
	if ((squadNumber >= 0) && (squadNumber < NUM_HOTKEY_SQUADS)) {
		return m_squads[squadNumber];
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** return the hotkey squad that a unit is in, or NO_HOTKEY_SQUAD if it isn't in one */
//-------------------------------------------------------------------------------------------------
Int Player::getSquadNumberForObject(const Object *objToFind) const
{
	for (Int i = 0; i < NUM_HOTKEY_SQUADS; ++i) {
		if (m_squads[i]->isOnSquad(objToFind)) {
			return i;
		}
	}

	return NO_HOTKEY_SQUAD;
}

//-------------------------------------------------------------------------------------------------
/** Remove an object from any hotkey squads its on. (Should never be more than one, but do them */
/** all for good measure. */
//-------------------------------------------------------------------------------------------------
void Player::removeObjectFromHotkeySquad(Object *objToRemove)
{
	for (Int i = 0; i < NUM_HOTKEY_SQUADS; ++i) {
		if (!m_squads[i]) {
			continue;
		}

		m_squads[i]->removeObject(objToRemove);
	}
}

//-------------------------------------------------------------------------------------------------
/** Select a hotkey team based on this GameMessage */
//-------------------------------------------------------------------------------------------------
void Player::addAIGroupToCurrentSelection(AIGroup *group) {
	if (group == nullptr) {
		return;
	}

	if (m_currentSelection == nullptr) {
		m_currentSelection = newInstance( Squad );
	}

	VecObjectID objectIDVec = group->getAllIDs();
	Int numObjs = objectIDVec.size();
	for (Int i = 0; i < numObjs; ++i) {
		m_currentSelection->addObjectID(objectIDVec[i]);
	}
}

//-------------------------------------------------------------------------------------------------
/** addTypeOfProductionCostChange adds a production change to the typeof list */
//-------------------------------------------------------------------------------------------------
void Player::addKindOfProductionCostChange(	KindOfMaskType kindOf, Real percent )
{
	KindOfPercentProductionChangeListIt it = m_kindOfPercentProductionChangeList.begin();
	while(it != m_kindOfPercentProductionChangeList.end())
	{

		KindOfPercentProductionChange *tof = *it;
		if( tof->m_percent == percent && tof->m_kindOf == kindOf)
		{
			tof->m_ref++;
			return;
		}
		++it;
	}

	KindOfPercentProductionChange *newTof = newInstance( KindOfPercentProductionChange );
	newTof->m_kindOf = kindOf;
	newTof->m_percent = percent;
	newTof->m_ref = 1;
	m_kindOfPercentProductionChangeList.push_back(newTof);

}

//-------------------------------------------------------------------------------------------------
/** addTypeOfProductionCostChange adds a production change to the typeof list */
//-------------------------------------------------------------------------------------------------
void Player::removeKindOfProductionCostChange(	KindOfMaskType kindOf, Real percent )
{
	KindOfPercentProductionChangeListIt it = m_kindOfPercentProductionChangeList.begin();
	while(it != m_kindOfPercentProductionChangeList.end())
	{

		KindOfPercentProductionChange* tof = *it;
		if( tof->m_percent == percent && tof->m_kindOf == kindOf)
		{
			tof->m_ref--;
			if(tof->m_ref == 0)
			{
				m_kindOfPercentProductionChangeList.erase( it );
				deleteInstance(tof);
			}
			return;
		}
		++it;
	}
	DEBUG_CRASH(("removeKindOfProductionCostChange was called with kindOf=%d and percent=%f. We could not find the entry in the list with these variables. CLH.",kindOf, percent));
}

//-------------------------------------------------------------------------------------------------
/** getProductionCostChangeBasedOnKindOf gets the cost percentage change based off of Kindof Mask */
//-------------------------------------------------------------------------------------------------
Real Player::getProductionCostChangeBasedOnKindOf( KindOfMaskType kindOf ) const
{
	Real start = 1.0f;
	KindOfPercentProductionChangeListIt it = m_kindOfPercentProductionChangeList.begin();
	while(it != m_kindOfPercentProductionChangeList.end())
	{

		KindOfPercentProductionChange *tof = *it;
		if(TEST_KINDOFMASK_MULTI(kindOf, tof->m_kindOf, KINDOFMASK_NONE))
		{
			start *= (1 + tof->m_percent);
		}
		++it;
	}
	return (start);
}

//-------------------------------------------------------------------------------------------------
/** setAttackedBy */
//-------------------------------------------------------------------------------------------------
void Player::setAttackedBy( Int playerNdx )
{
	DEBUG_ASSERTCRASH(playerNdx >= 0, ("Player::setAttackedBy Player index is %d", playerNdx));
	m_attackedBy[playerNdx] = true;
	m_attackedFrame = TheGameLogic->getFrame();

}

//-------------------------------------------------------------------------------------------------
/** getAttackedBy */
//-------------------------------------------------------------------------------------------------
Bool Player::getAttackedBy( Int playerNdx ) const
{
	return m_attackedBy[playerNdx];
}

// ------------------------------------------------------------------------------------------------
static void callHandleShroud( Object *obj, void * )
{
	// I feel I have to disapprove of the naming of this gathering of cell functions.  It is called by death,
	// alliance change, containment, spy change, and dynamic view range as well as partition cell change.
	if( obj )
		obj->handlePartitionCellMaintenance();
}

// ------------------------------------------------------------------------------------------------
void Player::setUnitsVisionSpied( Bool setting, PlayerIndex byWhom )
{
	Bool needRefresh = FALSE; // If this setting is an edge trigger on the reference count, I need
	// to tell all of my guys to handleShroud so they will start/stop looking for the new team.

	if( setting )
	{
		m_visionSpiedBy[ byWhom ] = m_visionSpiedBy[ byWhom ] + 1;
		if( m_visionSpiedBy[ byWhom ] == 1 )
			needRefresh = TRUE;
	}
	else
	{
		m_visionSpiedBy[ byWhom ] = m_visionSpiedBy[ byWhom ] - 1;
		if( m_visionSpiedBy[ byWhom ] == 0 )
			needRefresh = TRUE;
	}

	if( needRefresh )
	{
		PlayerMaskType workingMask = 0;
		for (Int i = 0; i < MAX_PLAYER_COUNT; ++i)
		{
			if( m_visionSpiedBy[i] > 0 )
				BitSet( workingMask, ( 1 << i ) );
			else
				BitClear( workingMask, ( 1 << i ) );
		}

		m_visionSpiedMask = workingMask;

		iterateObjects( callHandleShroud, nullptr );
	}
}

// ------------------------------------------------------------------------------------------------
PlayerMaskType Player::getVisionSpiedMask() const
{
	return m_visionSpiedMask;
}

// ------------------------------------------------------------------------------------------------
Bool Player::isPlayerObserver() const
{
	return m_observer;
}

// ------------------------------------------------------------------------------------------------
Bool Player::isPlayerDead() const
{
	return m_isPlayerDead;
}

// ------------------------------------------------------------------------------------------------
Bool Player::isPlayerActive() const
{
	return !m_observer && !m_isPlayerDead;
}

// ------------------------------------------------------------------------------------------------
Bool Player::isPlayableSide() const
{

	return m_playerTemplate ? m_playerTemplate->isPlayableSide() : FALSE;

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void Player::crc( Xfer *xfer )
{
	// Player battle plan bonuses
	Bool battlePlanBonus = m_battlePlanBonuses != nullptr;
	xfer->xferBool( &battlePlanBonus );
	CRCDEBUG_LOG(("Player %d[%ls] %s battle plans", m_playerIndex, m_playerDisplayName.str(), (battlePlanBonus)?"has":"doesn't have"));
	if( m_battlePlanBonuses )
	{
		CRCDUMPBATTLEPLANBONUSES(m_battlePlanBonuses, this, nullptr);
		xfer->xferReal( &m_battlePlanBonuses->m_armorScalar );
		xfer->xferReal( &m_battlePlanBonuses->m_sightRangeScalar );
		xfer->xferInt( &m_battlePlanBonuses->m_bombardment );
		xfer->xferInt( &m_battlePlanBonuses->m_holdTheLine );
		xfer->xferInt( &m_battlePlanBonuses->m_searchAndDestroy );
		m_battlePlanBonuses->m_validKindOf.xfer(xfer);
		m_battlePlanBonuses->m_invalidKindOf.xfer(xfer);
	}

	// People have reported memory hacking their points.  That would work since
	// buttons are authoritative, and these points just unhided buttons.
	// Same cheat principle as pulling NeedScience off your Generals buttons.
	xfer->xferInt( &m_skillPoints );
	xfer->xferInt( &m_sciencePurchasePoints );

}

// ------------------------------------------------------------------------------------------------
/** Xfer Method
	* Version Info:
	* 1: Initial version
	* 2: Player can now have a modifier on his skill points (multiplicative)
	* 3: Player can be excluded from the score screen via script.
	* 4: Player stores a list of specialpowerreadyframe timers, used by specialpowermodules abroad
	* 5: ??? (Profit)
	* 6: Store m_unitsShouldHunt, set to true after the script "Tell player to hunt" is called.
	* 7: added Preorder flag
	* 8: Save m_disabledSciences & m_hiddenSciences. jba.
	*/
// ------------------------------------------------------------------------------------------------
void Player::xfer( Xfer *xfer )
{

	// version
	const XferVersion currentVersion = 8;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// money
	xfer->xferSnapshot( &m_money );

	// upgrade list count
	Upgrade *upgrade;
	UnsignedShort upgradeCount = 0;
	for( upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		upgradeCount++;
	xfer->xferUnsignedShort( &upgradeCount );

	if (version >= 7)
	{
		// preorder info
		xfer->xferBool( & m_isPreorder );
	}

	if (version >= 8)
	{
		xfer->xferScienceVec(&m_sciencesDisabled);
		xfer->xferScienceVec(&m_sciencesHidden);
	}

	// xfer upgrade instances
	AsciiString upgradeName;
	if( xfer->getXferMode() == XFER_SAVE )
	{

		for( upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		{

			// write upgrade name
			upgradeName = upgrade->getTemplate()->getUpgradeName();
			xfer->xferAsciiString( &upgradeName );

			// xfer upgrade data
			xfer->xferSnapshot( upgrade );

		}

	}
	else
	{
		const UpgradeTemplate *upgradeTemplate;

		for( UnsignedShort i = 0; i < upgradeCount; ++i )
		{

			// read name
			xfer->xferAsciiString( &upgradeName );

			// find template for this upgrade
			upgradeTemplate = TheUpgradeCenter->findUpgrade( upgradeName );

			// sanity
			if( upgradeTemplate == nullptr )
			{

				DEBUG_CRASH(( "Player::xfer - Unable to find upgrade '%s'", upgradeName.str() ));
				throw SC_INVALID_DATA;

			}

			// add upgrade to player, the status is invalid, but that's OK cause we're about to xfer it
			upgrade = addUpgrade( upgradeTemplate, UPGRADE_STATUS_INVALID );

			// xfer upgrade data
			xfer->xferSnapshot( upgrade );

		}

	}

	// radar info
	xfer->xferInt( &m_radarCount );
	xfer->xferBool( & m_isPlayerDead );
	xfer->xferInt( &m_disableProofRadarCount );
	xfer->xferBool( & m_radarDisabled );

	// upgrades in progress
	xfer->xferUpgradeMask( &m_upgradesInProgress );

	// upgrades complete
	xfer->xferUpgradeMask( &m_upgradesCompleted );

	// energy info
	xfer->xferSnapshot( &m_energy );

	//
	// team prototypes ... this is only the fact that team prototypes are on this player
	// it is not the team prototype data itself
	//
	UnsignedShort prototypeCount = m_playerTeamPrototypes.size();
	xfer->xferUnsignedShort( &prototypeCount );
	TeamPrototypeID prototypeID;
	TeamPrototype *prototype;
	if( xfer->getXferMode() == XFER_SAVE )
	{

		PlayerTeamList::iterator it;
		for( it = m_playerTeamPrototypes.begin(); it != m_playerTeamPrototypes.end(); ++it )
		{

			prototype = *it;
			prototypeID = prototype->getID();
			xfer->xferUser( &prototypeID, sizeof( TeamPrototypeID ) );

		}

	}
	else
	{

		// empty the list right now
		m_playerTeamPrototypes.clear();

		// read all the data
		for( UnsignedShort i = 0; i < prototypeCount; ++i )
		{

			// read id
			xfer->xferUser( &prototypeID, sizeof( TeamPrototypeID ) );

			// find prototype
			prototype = TheTeamFactory->findTeamPrototypeByID( prototypeID );

			// sanity
			if( prototype == nullptr )
			{

				DEBUG_CRASH(( "Player::xfer - Unable to find team prototype by id" ));
				throw SC_INVALID_DATA;

			}

			// put in list
			m_playerTeamPrototypes.push_back( prototype );

		}

	}

	// build list info
	UnsignedShort buildListInfoCount = 0;
	BuildListInfo *buildListInfo;
	for( buildListInfo = m_pBuildList; buildListInfo; buildListInfo = buildListInfo->getNext() )
		buildListInfoCount++;
	xfer->xferUnsignedShort( &buildListInfoCount );
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// xfer each build list info
		for( buildListInfo = m_pBuildList; buildListInfo; buildListInfo = buildListInfo->getNext() )
			xfer->xferSnapshot( buildListInfo );

	}
	else
	{

		//
		// destroy any build list that we got from loading the bare bones map, note that deleting
		// the head of these structures automatically deletes any links attached
		//
		deleteInstance(m_pBuildList);
		m_pBuildList = nullptr;

		// read each build list info
		for( UnsignedShort i = 0; i < buildListInfoCount; ++i )
		{

			// allocate new build list
			buildListInfo = newInstance( BuildListInfo );
			buildListInfo->setNextBuildList( nullptr );

			// attach to the *end* of the list in the player
			if( m_pBuildList == nullptr )
				m_pBuildList = buildListInfo;
			else
			{
				BuildListInfo *last = m_pBuildList;

				while( last->getNext() != nullptr )
					last = last->getNext();

				last->setNextBuildList( buildListInfo );

			}

			// xfer data
			xfer->xferSnapshot( buildListInfo );

		}

	}

	// ai player data
	Bool aiPlayerPresent = m_ai ? TRUE : FALSE;
	xfer->xferBool( &aiPlayerPresent );
	if( (aiPlayerPresent == TRUE && m_ai == nullptr) || (aiPlayerPresent == FALSE && m_ai != nullptr) )
	{

		DEBUG_CRASH(( "Player::xfer - m_ai present/missing mismatch" ));
		throw SC_INVALID_DATA;

	}
	if( m_ai )
		xfer->xferSnapshot( m_ai );

	// resource gathering manager
	Bool resourceGatheringManagerPresent = m_resourceGatheringManager ? TRUE : FALSE;
	xfer->xferBool( &resourceGatheringManagerPresent );
	if( (resourceGatheringManagerPresent == TRUE && m_resourceGatheringManager == nullptr) ||
			(resourceGatheringManagerPresent == FALSE && m_resourceGatheringManager != nullptr ) )
	{

		DEBUG_CRASH(( "Player::xfer - m_resourceGatheringManager present/missing mismatch" ));
		throw SC_INVALID_DATA;

	}
	if( m_resourceGatheringManager )
		xfer->xferSnapshot( m_resourceGatheringManager );

	// tunnel tracking system
	Bool tunnelTrackerPresent = m_tunnelSystem ? TRUE : FALSE;
	xfer->xferBool( &tunnelTrackerPresent );
	if( (tunnelTrackerPresent == TRUE && m_tunnelSystem == nullptr) ||
			(tunnelTrackerPresent == FALSE && m_tunnelSystem != nullptr) )
	{

		DEBUG_CRASH(( "Player::xfer - m_tunnelSystem present/missing mismatch" ));
		throw SC_INVALID_DATA;

	}
	if( m_tunnelSystem )
		xfer->xferSnapshot( m_tunnelSystem );

	// default team
	TeamID teamID = m_defaultTeam ? m_defaultTeam->getID() : TEAM_ID_INVALID;
	xfer->xferUser( &teamID, sizeof( TeamID ) );
	if( xfer->getXferMode() == XFER_LOAD )
		m_defaultTeam = TheTeamFactory->findTeamByID( teamID );

	// sciences
	if (version >= 5)
	{
		// m_sciences will contain some intrinsic sciences and stuff, which we don't want.
		// so nuke 'em for load.
		if( xfer->getXferMode() == XFER_LOAD )
			m_sciences.clear();
		xfer->xferScienceVec(&m_sciences);
	}
	else
	{
		/*
			This code is WRONG WRONG WRONG and must not be used or mimicked; it
			is present for backwards "compatibility" only. (srj)
		*/
		UnsignedShort scienceCount = m_sciences.size();
		xfer->xferUnsignedShort( &scienceCount );
		ScienceType science;
		if( xfer->getXferMode() == XFER_SAVE )
		{
			ScienceVec::const_iterator it;
			for( it = m_sciences.begin(); it != m_sciences.end(); ++it )
			{

				science = *it;
				xfer->xferUser( &science, sizeof( ScienceType ) );
			}
		}
		else
		{
			for( UnsignedShort i = 0; i < scienceCount; ++i )
			{
				xfer->xferUser( &science, sizeof( ScienceType ) );
				m_sciences.push_back( science );

			}
		}
		/*
			This code is WRONG WRONG WRONG and must not be used or mimicked; it
			is present for backwards "compatibility" only. (srj)
		*/
	}

	// rank level
	xfer->xferInt( &m_rankLevel );

	// skill points
	xfer->xferInt( &m_skillPoints );

	// science purchase points
	xfer->xferInt( &m_sciencePurchasePoints );

	// level up
	xfer->xferInt( &m_levelUp );

	// level down
	xfer->xferInt( &m_levelDown );

	// general name
	xfer->xferUnicodeString( &m_generalName );

	// player relations
	xfer->xferSnapshot( m_playerRelations );

	// team relations
	xfer->xferSnapshot( m_teamRelations );

	// can build units
	xfer->xferBool( &m_canBuildUnits );

	// can build base
	xfer->xferBool( &m_canBuildBase );

	// observer
	xfer->xferBool( &m_observer );

	if (version >= 2)
	{
		// current skill point modifier value
		xfer->xferReal( &m_skillPointsModifier);
	}
	else
	{
		m_skillPointsModifier = 1.0f;
	}

	if (version >= 3)
	{
		xfer->xferBool( &m_listInScoreScreen );
	}
	else
	{
		m_listInScoreScreen = TRUE;
	}
	// attacked by
	xfer->xferUser( m_attackedBy, sizeof( Bool ) * MAX_PLAYER_COUNT );

	// vision spied by
	xfer->xferUser( m_visionSpiedBy, sizeof( Int ) * MAX_PLAYER_COUNT );

	// vision spied by mask
	xfer->xferUser( &m_visionSpiedMask, sizeof( PlayerMaskType ) );

	// cash bounty percent
	xfer->xferReal( &m_cashBountyPercent );

	// score keeper
	xfer->xferSnapshot( &m_scoreKeeper );






	// size of and data for kindof percent production change list
	UnsignedShort percentProductionChangeCount = m_kindOfPercentProductionChangeList.size();
	xfer->xferUnsignedShort( &percentProductionChangeCount );
	KindOfPercentProductionChange *entry;
	if( xfer->getXferMode() == XFER_SAVE )
	{
		KindOfPercentProductionChangeListIt it;

		// save each item
		for( it = m_kindOfPercentProductionChangeList.begin();
				 it != m_kindOfPercentProductionChangeList.end();
				 ++it )
		{

			// get entry data
			entry = *it;

			// kind of mask type
			entry->m_kindOf.xfer(xfer);

			// percent
			xfer->xferReal( &entry->m_percent );

			// ref
			xfer->xferUnsignedInt( &entry->m_ref );

		}

	}
	else
	{

		// sanity, list must be empty right now
		if( !m_kindOfPercentProductionChangeList.empty() )
		{

			DEBUG_CRASH(( "Player::xfer - m_kindOfPercentProductionChangeList should be empty but is not" ));
			throw SC_INVALID_DATA;

		}

		// read each entry
		for( UnsignedInt i = 0; i < percentProductionChangeCount; ++i )
		{

			// allocate new entry
			entry = newInstance( KindOfPercentProductionChange );

			// read data
			entry->m_kindOf.xfer(xfer);
			xfer->xferReal( &entry->m_percent );
			xfer->xferUnsignedInt( &entry->m_ref );

			// put at end of list
			m_kindOfPercentProductionChangeList.push_back( entry );

		}

	}




	///////////////////////////////////////////////////////////////////////////
	if ( version < 4 )
	{
		 m_specialPowerReadyTimerList.clear();
	}
	else
	{
		UnsignedShort timerListSize = m_specialPowerReadyTimerList.size();
		xfer->xferUnsignedShort( &timerListSize );// HANDY LITTLE SHORT TO SIZE MY LIST
		if( xfer->getXferMode() == XFER_SAVE )
		{

			SpecialPowerReadyTimerType *timer;
			SpecialPowerReadyTimerListIterator it;
			for( it = m_specialPowerReadyTimerList.begin(); it != m_specialPowerReadyTimerList.end(); ++it )
			{
				timer = &(*it);
				xfer->xferUnsignedInt( &timer->m_templateID );
				xfer->xferUnsignedInt( &timer->m_readyFrame );
			}
		}
		else
		{
			if( !m_specialPowerReadyTimerList.empty() ) // sanity, list must be empty right now
			{
				DEBUG_CRASH(( "Player::xfer - m_specialPowerReadyTimerList should be empty but is not" ));
				throw SC_INVALID_DATA;
			}

			// read each entry
			for( UnsignedInt i = 0; i < timerListSize; ++i )
			{
				SpecialPowerReadyTimerType timer;

				// read data
				xfer->xferUnsignedInt( &timer.m_templateID );
				xfer->xferUnsignedInt( &timer.m_readyFrame );

				// put at end of list
				m_specialPowerReadyTimerList.push_back( timer );

			}
		}
	}
	///////////////////////////////////////////////////////////////////////////




	// squads
	UnsignedShort squadCount = NUM_HOTKEY_SQUADS;
	xfer->xferUnsignedShort( &squadCount );
	if( squadCount != NUM_HOTKEY_SQUADS )
	{

		DEBUG_CRASH(( "Player::xfer - size of m_squadCount array has changed" ));
		throw SC_INVALID_DATA;

	}
	for( UnsignedShort i = 0; i < squadCount; ++i )
	{

		if( m_squads[ i ] == nullptr )
		{

			DEBUG_CRASH(( "Player::xfer - null squad at index '%d'", i ));
			throw SC_INVALID_DATA;

		}

		xfer->xferSnapshot( m_squads[ i ] );

	}

	// current squad selection
	Bool currentSelectionPresent = m_currentSelection ? TRUE : FALSE;
	xfer->xferBool( &currentSelectionPresent );
	if( currentSelectionPresent )
	{

		// allocate squad if needed
		if( m_currentSelection == nullptr && xfer->getXferMode() == XFER_LOAD )
			m_currentSelection = newInstance( Squad );

		// xfer
		xfer->xferSnapshot( m_currentSelection );

	}

	// Player battle plan bonuses
	Bool battlePlanBonus = m_battlePlanBonuses != nullptr;
	xfer->xferBool( &battlePlanBonus ); //If we're loading, it just replaces the bool
	if( xfer->getXferMode() == XFER_LOAD )
	{
		deleteInstance(m_battlePlanBonuses);
		m_battlePlanBonuses = nullptr;

		if ( battlePlanBonus )
		{
			m_battlePlanBonuses = newInstance( BattlePlanBonuses );
		}
	}
	if( m_battlePlanBonuses )
	{
		xfer->xferReal( &m_battlePlanBonuses->m_armorScalar );
		xfer->xferReal( &m_battlePlanBonuses->m_sightRangeScalar );
		xfer->xferInt( &m_battlePlanBonuses->m_bombardment );
		xfer->xferInt( &m_battlePlanBonuses->m_holdTheLine );
		xfer->xferInt( &m_battlePlanBonuses->m_searchAndDestroy );
		m_battlePlanBonuses->m_validKindOf.xfer(xfer);
		m_battlePlanBonuses->m_invalidKindOf.xfer(xfer);
	}
	xfer->xferInt( &m_bombardBattlePlans );
	xfer->xferInt( &m_holdTheLineBattlePlans );
	xfer->xferInt( &m_searchAndDestroyBattlePlans );

	if (version >= 6)
	{
		xfer->xferBool(&m_unitsShouldHunt);
	}
	else
		m_unitsShouldHunt = FALSE;

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void Player::loadPostProcess()
{

}

