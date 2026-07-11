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

// FILE: PlayerTemplate.cpp /////////////////////////////////////////////////////////
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
// File name: PlayerTemplate.cpp
//
// Created:   Steven Johnson, October 2001
//
// Desc:      @todo
//
//-----------------------------------------------------------------------------

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_VETERANCY_NAMES				// for TheVeterancyNames[]

#include "Common/GameCommon.h"
#include "Common/PlayerTemplate.h"
#include "Common/Player.h"
#include "Common/INI.h"
#include "Common/Science.h"
#include "GameClient/Image.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/*static*/ const FieldParse* PlayerTemplate::getFieldParse()
{
	static const FieldParse TheFieldParseTable[] =
	{
		{ "Side",											INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_side ) },
		{ "BaseSide",								INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_baseSide ) },
		{ "PlayableSide",							INI::parseBool,																	nullptr, offsetof( PlayerTemplate, m_playableSide ) },
		{ "DisplayName",							INI::parseAndTranslateLabel,										nullptr, offsetof( PlayerTemplate, m_displayName) },
		{ "StartMoney",								PlayerTemplate::parseStartMoney,								nullptr, offsetof( PlayerTemplate, m_money ) },
		{ "PreferredColor",						INI::parseRGBColor,															nullptr, offsetof( PlayerTemplate, m_preferredColor ) },
		{ "StartingBuilding",					INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingBuilding ) },
		{ "StartingUnit0",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[0] ) },
		{ "StartingUnit1",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[1] ) },
		{ "StartingUnit2",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[2] ) },
		{ "StartingUnit3",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[3] ) },
		{ "StartingUnit4",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[4] ) },
		{ "StartingUnit5",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[5] ) },
		{ "StartingUnit6",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[6] ) },
		{ "StartingUnit7",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[7] ) },
		{ "StartingUnit8",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[8] ) },
		{ "StartingUnit9",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_startingUnits[9] ) },
		{ "ProductionCostChange",			PlayerTemplate::parseProductionCostChange,			nullptr, 0 },
		{ "ProductionTimeChange",			PlayerTemplate::parseProductionTimeChange,			nullptr, 0 },
		{ "ProductionVeterancyLevel",	PlayerTemplate::parseProductionVeterancyLevel,	nullptr, 0 },
		{ "IntrinsicSciences",				INI::parseScienceVector,												nullptr, offsetof( PlayerTemplate, m_intrinsicSciences ) },
		{ "PurchaseScienceCommandSetRank1",INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_purchaseScienceCommandSetRank1 ) },
		{ "PurchaseScienceCommandSetRank3",INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_purchaseScienceCommandSetRank3 ) },
		{ "PurchaseScienceCommandSetRank8",INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_purchaseScienceCommandSetRank8 ) },
		{ "SpecialPowerShortcutCommandSet",INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_specialPowerShortcutCommandSet ) },
		{ "SpecialPowerShortcutWinName"		,INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_specialPowerShortcutWinName) },
		{ "SpecialPowerShortcutButtonCount",INI::parseInt,												nullptr, offsetof( PlayerTemplate, m_specialPowerShortcutButtonCount ) },
		{ "IsObserver",								INI::parseBool,																	nullptr, offsetof( PlayerTemplate, m_observer ) },
    { "OldFaction",               INI::parseBool,                                 nullptr, offsetof( PlayerTemplate, m_oldFaction ) },
		{ "IntrinsicSciencePurchasePoints",				INI::parseInt,												nullptr, offsetof( PlayerTemplate, m_intrinsicSPP ) },
		{ "ScoreScreenImage",					INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_scoreScreenImage ) },
		{ "LoadScreenImage",					INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_loadScreenImage ) },
		{ "LoadScreenMusic",					INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_loadScreenMusic ) },

		{ "HeadWaterMark",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_headWaterMark ) },
		{ "FlagWaterMark",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_flagWaterMark ) },
		{ "EnabledImage",							INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_enabledImage ) },
		//{ "DisabledImage",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_disabledImage ) },
		//{ "HiliteImage",							INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_hiliteImage ) },
		//{ "PushedImage",							INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_pushedImage ) },
		{ "SideIconImage",						INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_sideIconImage ) },

		{ "BeaconName",								INI::parseAsciiString,													nullptr, offsetof( PlayerTemplate, m_beaconTemplate ) },
		{ nullptr,											nullptr,																				nullptr, 0 },
	};

	return TheFieldParseTable;
}

AsciiString PlayerTemplate::getStartingUnit( Int i ) const
{
	if (i<0 || i>= MAX_MP_STARTING_UNITS)
		return AsciiString::TheEmptyString;

	return m_startingUnits[i];
}

//-------------------------------------------------------------------------------------------------
// This is is a Template, and a percent change to the cost of producing it.
/*static*/ void PlayerTemplate::parseProductionCostChange( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	PlayerTemplate* self = (PlayerTemplate*)instance;

	NameKeyType buildTemplateKey = NAMEKEY(ini->getNextToken());
	Real percentChange = INI::scanPercentToReal(ini->getNextToken());

	self->m_productionCostChanges[buildTemplateKey] = percentChange;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PlayerTemplate::parseProductionTimeChange( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	PlayerTemplate* self = (PlayerTemplate*)instance;

	NameKeyType buildTemplateKey = NAMEKEY(ini->getNextToken());
	Real percentChange = INI::scanPercentToReal(ini->getNextToken());

	self->m_productionTimeChanges[buildTemplateKey] = percentChange;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PlayerTemplate::parseProductionVeterancyLevel( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	PlayerTemplate* self = (PlayerTemplate*)instance;

	// Format is ThingTemplatename VeterancyName
	AsciiString HACK = AsciiString(ini->getNextToken());
	NameKeyType buildTemplateKey = NAMEKEY(HACK.str());

	VeterancyLevel startLevel = (VeterancyLevel)INI::scanIndexList(ini->getNextToken(), TheVeterancyNames);
	self->m_productionVeterancyLevels[buildTemplateKey] = startLevel;
}

//-------------------------------------------------------------------------------------------------
/** Parse integer money and deposit in the m_money */
//-------------------------------------------------------------------------------------------------
/*static*/ void PlayerTemplate::parseStartMoney( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	Int money = 0;

	// parse the money as a regular "FIELD = <integer>"
	INI::parseInt( ini, instance, &money, nullptr );

	// assign the money into the 'Money' (m_money) pointed to at 'store'
	Money *theMoney = (Money *)store;
	theMoney->init();
	theMoney->setStartingCash(money);

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
PlayerTemplate::PlayerTemplate() :
	m_nameKey(NAMEKEY_INVALID),
	m_observer(false),
	m_playableSide(false),
  m_oldFaction(false),
	m_intrinsicSPP(0),
	m_specialPowerShortcutButtonCount(0)
{
	m_preferredColor.red = m_preferredColor.green = m_preferredColor.blue = 0.0f;
	m_beaconTemplate.clear();
}

//-----------------------------------------------------------------------------
const Image *PlayerTemplate::getHeadWaterMarkImage() const
{
	return TheMappedImageCollection->findImageByName(m_headWaterMark);
}

//-----------------------------------------------------------------------------
const Image *PlayerTemplate::getFlagWaterMarkImage() const
{
	return TheMappedImageCollection->findImageByName(m_flagWaterMark);
}

//-----------------------------------------------------------------------------
const Image *PlayerTemplate::getSideIconImage() const
{
	return TheMappedImageCollection->findImageByName(m_sideIconImage);
}

//-----------------------------------------------------------------------------
const Image *PlayerTemplate::getEnabledImage() const
{
	return TheMappedImageCollection->findImageByName(m_enabledImage);
}

//-----------------------------------------------------------------------------
//const Image *PlayerTemplate::getDisabledImage() const
//{
//	return TheMappedImageCollection->findImageByName(m_disabledImage);
//}

//-----------------------------------------------------------------------------
//const Image *PlayerTemplate::getHiliteImage() const
//{
//	return TheMappedImageCollection->findImageByName(m_hiliteImage);
//}

//-----------------------------------------------------------------------------
//const Image *PlayerTemplate::getPushedImage() const
//{
//	return TheMappedImageCollection->findImageByName(m_pushedImage);
//}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/*extern*/ PlayerTemplateStore *ThePlayerTemplateStore = nullptr;

//-----------------------------------------------------------------------------
PlayerTemplateStore::PlayerTemplateStore()
{
	// nothing
}

//-----------------------------------------------------------------------------
PlayerTemplateStore::~PlayerTemplateStore()
{
	// nothing
}

//-----------------------------------------------------------------------------
void PlayerTemplateStore::init()
{
	m_playerTemplates.clear();
}

//-----------------------------------------------------------------------------
void PlayerTemplateStore::reset()
{
// don't reset this list here; we want to retain this info.
//	m_playerTemplates.clear();
}

//-----------------------------------------------------------------------------
void PlayerTemplateStore::update()
{
	// nothing
}

//-----------------------------------------------------------------------------
const PlayerTemplate* PlayerTemplateStore::findPlayerTemplate(NameKeyType namekey) const
{
// begin ugly, hokey code to quietly load old maps...
	static NameKeyType a0 = NAMEKEY("FactionAmerica");
	static NameKeyType a1 = NAMEKEY("FactionAmericaChooseAGeneral");
	static NameKeyType a2 = NAMEKEY("FactionAmericaTankCommand");
	static NameKeyType a3 = NAMEKEY("FactionAmericaSpecialForces");
	static NameKeyType a4 = NAMEKEY("FactionAmericaAirForce");

	static NameKeyType c0 = NAMEKEY("FactionChina");
	static NameKeyType c1 = NAMEKEY("FactionChinaChooseAGeneral");
	static NameKeyType c2 = NAMEKEY("FactionChinaRedArmy");
	static NameKeyType c3 = NAMEKEY("FactionChinaSpecialWeapons");
	static NameKeyType c4 = NAMEKEY("FactionChinaSecretPolice");

	static NameKeyType g0 = NAMEKEY("FactionGLA");
	static NameKeyType g1 = NAMEKEY("FactionGLAChooseAGeneral");
	static NameKeyType g2 = NAMEKEY("FactionGLATerrorCell");
	static NameKeyType g3 = NAMEKEY("FactionGLABiowarCommand");
	static NameKeyType g4 = NAMEKEY("FactionGLAWarlordCommand");

	if (namekey == a1 || namekey == a2 || namekey == a3 || namekey == a4)
		namekey = a0;
	else if (namekey == c1 || namekey == c2 || namekey == c3 || namekey == c4)
		namekey = c0;
	else if (namekey == g1 || namekey == g2 || namekey == g3 || namekey == g4)
		namekey = g0;
// end ugly, hokey code to quietly load old maps...

	#ifdef RTS_DEBUG
	AsciiString nn = KEYNAME(namekey);
	#endif
  for (PlayerTemplateVector::const_iterator it = m_playerTemplates.begin(); it != m_playerTemplates.end(); ++it)
	{
		#ifdef RTS_DEBUG
		AsciiString n = KEYNAME((*it).getNameKey());
		#endif
		if ((*it).getNameKey() == namekey)
			return &(*it);
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
const PlayerTemplate* PlayerTemplateStore::getNthPlayerTemplate(Int i) const
{
	if (i >= 0 && i < m_playerTemplates.size())
		return &m_playerTemplates[i];

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
// @todo: PERF_EVALUATE Get a perf timer on this.
// If this function is called frequently, there are some relatively trivial changes we could make to
// have it run a lot faster.
void PlayerTemplateStore::getAllSideStrings(AsciiStringList *outStringList)
{
	if (!outStringList)
		return;

	// should outStringList be cleared first? If so, that would go here
	AsciiStringList tmpList;

	Int numTemplates = getPlayerTemplateCount();
	for ( Int i = 0; i < numTemplates; ++i )
	{
		const PlayerTemplate *pt = getNthPlayerTemplate(i);
		// Sanity
		if (!pt)
			continue;

		if (std::find(tmpList.begin(), tmpList.end(), pt->getSide()) == tmpList.end())
			tmpList.push_back(pt->getSide());
	}
	// tmpList is now filled with all unique sides found in the player templates.

	// splice is a constant-time function which takes all elements from tmpList and
	// inserts them before outStringList->end(), and also removes them from tmpList
	outStringList->splice(outStringList->end(), tmpList);

	// all done
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PlayerTemplateStore::parsePlayerTemplateDefinition( INI* ini )
{
	const char* c = ini->getNextToken();
	NameKeyType namekey = NAMEKEY(c);

	PlayerTemplate* pt = const_cast<PlayerTemplate*>(ThePlayerTemplateStore->findPlayerTemplate(namekey));
	if (pt)
	{
		ini->initFromINI(pt, pt->getFieldParse() );
		pt->setNameKey(namekey);
	}
	else
	{
		PlayerTemplate npt;
		ini->initFromINI( &npt, npt.getFieldParse() );
		npt.setNameKey(namekey);
		ThePlayerTemplateStore->m_playerTemplates.push_back(npt);
	}

}

//-------------------------------------------------------------------------------------------------
void INI::parsePlayerTemplateDefinition( INI* ini )
{
	PlayerTemplateStore::parsePlayerTemplateDefinition(ini);
}

