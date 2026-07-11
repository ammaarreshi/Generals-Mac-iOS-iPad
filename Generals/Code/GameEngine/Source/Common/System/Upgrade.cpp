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

// FILE: Upgrade.cpp //////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Upgrade system for players
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_VETERANCY_NAMES
#include "Common/Upgrade.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Image.h"


const char *const TheUpgradeTypeNames[] =
{
	"PLAYER",
	"OBJECT",
	nullptr
};
static_assert(ARRAY_SIZE(TheUpgradeTypeNames) == NUM_UPGRADE_TYPES + 1, "Incorrect array size");

// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////
class UpgradeCenter *TheUpgradeCenter = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////////
// UPGRADE ////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Upgrade::Upgrade( const UpgradeTemplate *upgradeTemplate )
{

	m_template = upgradeTemplate;
	m_status = UPGRADE_STATUS_INVALID;
	m_next = nullptr;
	m_prev = nullptr;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Upgrade::~Upgrade()
{

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void Upgrade::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void Upgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// status
	xfer->xferUser( &m_status, sizeof( UpgradeStatusType ) );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void Upgrade::loadPostProcess()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// UPGRADE TEMPLATE ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const FieldParse UpgradeTemplate::m_upgradeFieldParseTable[] =
{

	{ "DisplayName",				INI::parseAsciiString,		nullptr, offsetof( UpgradeTemplate, m_displayNameLabel ) },
	{ "Type",								INI::parseIndexList,			TheUpgradeTypeNames, offsetof( UpgradeTemplate, m_type ) },
	{ "BuildTime",					INI::parseReal,						nullptr, offsetof( UpgradeTemplate, m_buildTime ) },
	{ "BuildCost",					INI::parseInt,						nullptr, offsetof( UpgradeTemplate, m_cost ) },
	{ "ButtonImage",				INI::parseAsciiString,		nullptr, offsetof( UpgradeTemplate, m_buttonImageName ) },
	{ "ResearchSound",			INI::parseAudioEventRTS,	nullptr, offsetof( UpgradeTemplate, m_researchSound ) },
	{ "UnitSpecificSound",	INI::parseAudioEventRTS,	nullptr, offsetof( UpgradeTemplate, m_unitSpecificSound ) },
	{ "AcademyClassify",		INI::parseIndexList,			TheAcademyClassificationTypeNames, offsetof( UpgradeTemplate, m_academyClassificationType ) },
	{ nullptr,						nullptr,												 nullptr, 0 }

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeTemplate::UpgradeTemplate()
{
	m_cost = 0;
	m_type = UPGRADE_TYPE_PLAYER;
	m_nameKey = NAMEKEY_INVALID;
	m_buildTime = 0.0f;
	m_next = nullptr;
	m_prev = nullptr;
	m_buttonImage = nullptr;
	m_academyClassificationType = ACT_NONE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeTemplate::~UpgradeTemplate()
{

}

//-------------------------------------------------------------------------------------------------
/** Calculate the time it takes (in logic frames) for a player to build this UpgradeTemplate */
//-------------------------------------------------------------------------------------------------
Int UpgradeTemplate::calcTimeToBuild( Player *player ) const
{
#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	if( player->buildsInstantly() )
	{
		return 1;
	}
#endif

	///@todo modify this by power state of player
	return m_buildTime * LOGICFRAMES_PER_SECOND;

}

//-------------------------------------------------------------------------------------------------
/** Calculate the cost takes this player to build this upgrade */
//-------------------------------------------------------------------------------------------------
Int UpgradeTemplate::calcCostToBuild( Player *player ) const
{

	///@todo modify this by any player handicaps
	return m_cost;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static AsciiString getVetUpgradeName(VeterancyLevel v)
{
	AsciiString tmp;
	tmp = "Upgrade_Veterancy_";
	tmp.concat(TheVeterancyNames[v]);
	return tmp;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void UpgradeTemplate::friend_makeVeterancyUpgrade(VeterancyLevel v)
{
	m_type = UPGRADE_TYPE_OBJECT;	// veterancy "upgrades" are always per-object, not per-player
	m_name = getVetUpgradeName(v);
	m_nameKey = TheNameKeyGenerator->nameToKey( m_name );
	m_displayNameLabel.clear();	// should never be displayed
	m_buildTime = 0.0f;
	m_cost = 0.0f;
	// leave this alone.
	//m_upgradeMask = ???;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void UpgradeTemplate::cacheButtonImage()
{
	if( m_buttonImageName.isNotEmpty() )
	{
		m_buttonImage = TheMappedImageCollection->findImageByName( m_buttonImageName );
		DEBUG_ASSERTCRASH( m_buttonImage, ("UpgradeTemplate: %s is looking for button image %s but can't find it. Skipping...", m_name.str(), m_buttonImageName.str() ) );
		m_buttonImageName.clear();	// we're done with this, so nuke it
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// UPGRADE CENTER /////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeCenter::UpgradeCenter()
{

	m_upgradeList = nullptr;
	m_nextTemplateMaskBit = 0;
	buttonImagesCached = FALSE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeCenter::~UpgradeCenter()
{

	// delete all the upgrades loaded from the INI database
	UpgradeTemplate *next;
	while( m_upgradeList )
	{

		// get next
		next = m_upgradeList->friend_getNext();

		// delete head of list
		deleteInstance(m_upgradeList);

		// set head to next element
		m_upgradeList = next;

	}

}

//-------------------------------------------------------------------------------------------------
/** Upgrade center initialization */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::init()
{
	UpgradeTemplate* up;

	// name will be overridden by friend_makeVeterancyUpgrade

// no, there ISN'T an upgrade for this one...
//up = newUpgrade("");
//up->friend_makeVeterancyUpgrade(LEVEL_REGULAR);

	up = newUpgrade("");
	up->friend_makeVeterancyUpgrade(LEVEL_VETERAN);

	up = newUpgrade("");
	up->friend_makeVeterancyUpgrade(LEVEL_ELITE);

	up = newUpgrade("");
	up->friend_makeVeterancyUpgrade(LEVEL_HEROIC);

}

//-------------------------------------------------------------------------------------------------
/** Upgrade center system reset */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::reset()
{
	if( TheMappedImageCollection && !buttonImagesCached )
	{
		UpgradeTemplate *upgrade;
		for( upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		{
			upgrade->cacheButtonImage();
		}
		buttonImagesCached = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by veterancy level */
//-------------------------------------------------------------------------------------------------
const UpgradeTemplate *UpgradeCenter::findVeterancyUpgrade( VeterancyLevel level ) const
{
	AsciiString tmp = getVetUpgradeName(level);
	return findUpgrade(tmp);
}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by name key */
//-------------------------------------------------------------------------------------------------
UpgradeTemplate *UpgradeCenter::findNonConstUpgradeByKey( NameKeyType key )
{
	UpgradeTemplate *upgrade;

	// search list
	for( upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		if( upgrade->getUpgradeNameKey() == key )
			return upgrade;

	// item not found
	return nullptr;

}

// ------------------------------------------------------------------------------------------------
/** Return the first upgrade template */
// ------------------------------------------------------------------------------------------------
UpgradeTemplate *UpgradeCenter::firstUpgradeTemplate()
{

	return m_upgradeList;

}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by name key */
//-------------------------------------------------------------------------------------------------
const UpgradeTemplate *UpgradeCenter::findUpgradeByKey( NameKeyType key ) const
{
	const UpgradeTemplate *upgrade;

	// search list
	for( upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		if( upgrade->getUpgradeNameKey() == key )
			return upgrade;

	// item not found
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by name */
//-------------------------------------------------------------------------------------------------
const UpgradeTemplate *UpgradeCenter::findUpgrade( const AsciiString& name ) const
{
	return findUpgradeByKey( TheNameKeyGenerator->nameToKey( name ) );
}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by name */
//-------------------------------------------------------------------------------------------------
const UpgradeTemplate *UpgradeCenter::findUpgrade( const char* name ) const
{
	return findUpgradeByKey( TheNameKeyGenerator->nameToKey( name ) );
}

//-------------------------------------------------------------------------------------------------
/** Allocate a new upgrade template */
//-------------------------------------------------------------------------------------------------
UpgradeTemplate *UpgradeCenter::newUpgrade( const AsciiString& name )
{
	UpgradeTemplate *newUpgrade = newInstance(UpgradeTemplate);

	// copy data from the default upgrade
	const UpgradeTemplate *defaultUpgrade = findUpgrade( "DefaultUpgrade" );
	if( defaultUpgrade )
		*newUpgrade = *defaultUpgrade;

	// assign name and starting data
	newUpgrade->setUpgradeName( name );
	newUpgrade->setUpgradeNameKey( TheNameKeyGenerator->nameToKey( name ) );

	// Make a unique bitmask for this new template by keeping track of what bits have been assigned
	// damn MSFT! proper ANSI syntax for a proper 64-bit constant is "1LL", but MSVC doesn't recognize it
	UpgradeMaskType newMask;
	newMask.set( m_nextTemplateMaskBit );
	//Int64 newMask = 1i64 << m_nextTemplateMaskBit;
	m_nextTemplateMaskBit++;
	DEBUG_ASSERTCRASH( m_nextTemplateMaskBit < UPGRADE_MAX_COUNT, ("Can't have over %d types of Upgrades and have a Bitfield function.", UPGRADE_MAX_COUNT) );
	newUpgrade->friend_setUpgradeMask( newMask );

	// link upgrade
	linkUpgrade( newUpgrade );

	// return new upgrade
	return newUpgrade;

}

//-------------------------------------------------------------------------------------------------
/** Link an upgrade to our list */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::linkUpgrade( UpgradeTemplate *upgrade )
{

	// sanity
	if( upgrade == nullptr )
		return;

	// link
	upgrade->friend_setPrev( nullptr );
	upgrade->friend_setNext( m_upgradeList );
	if( m_upgradeList )
		m_upgradeList->friend_setPrev( upgrade );
	m_upgradeList = upgrade;

}

//-------------------------------------------------------------------------------------------------
/** Unlink an upgrade from our list */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::unlinkUpgrade( UpgradeTemplate *upgrade )
{

	// sanity
	if( upgrade == nullptr )
		return;

	if( upgrade->friend_getNext() )
		upgrade->friend_getNext()->friend_setPrev( upgrade->friend_getPrev() );
	if( upgrade->friend_getPrev() )
		upgrade->friend_getPrev()->friend_setNext( upgrade->friend_getNext() );
	else
		m_upgradeList = upgrade->friend_getNext();

}

//-------------------------------------------------------------------------------------------------
/** does this player have all the necessary things to make this upgrade */
//-------------------------------------------------------------------------------------------------
Bool UpgradeCenter::canAffordUpgrade( Player *player, const UpgradeTemplate *upgradeTemplate, Bool displayReason ) const
{

	// sanity
	if( player == nullptr || upgradeTemplate == nullptr )
		return FALSE;

	// money check
	Money *money = player->getMoney();
	if( money->countMoney() < upgradeTemplate->calcCostToBuild( player ) )
	{
		//Post reason why we can't make upgrade!
		if( displayReason )
		{
			TheInGameUI->message( "GUI:NotEnoughMoneyToUpgrade" );
		}
		return FALSE;
	}

	/// @todo maybe have prereq checks for upgrades???

	return TRUE;  // all is well

}

//-------------------------------------------------------------------------------------------------
/** generate a list of upgrade names for WorldBuilder */
//-------------------------------------------------------------------------------------------------
std::vector<AsciiString> UpgradeCenter::getUpgradeNames() const
{
	std::vector<AsciiString> upgradeNames;

	for( UpgradeTemplate *upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		upgradeNames.push_back(upgrade->getUpgradeName());

	return upgradeNames;

}

//-------------------------------------------------------------------------------------------------
/** Parse an upgrade definition */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::parseUpgradeDefinition( INI *ini )
{
	// read the name
	const char* name = ini->getNextToken();

	// find existing item if present
	UpgradeTemplate* upgrade = TheUpgradeCenter->findNonConstUpgradeByKey( NAMEKEY(name) );
	if( upgrade == nullptr )
	{

		// allocate a new item
		upgrade = TheUpgradeCenter->newUpgrade( name );

	}

	// sanity
	DEBUG_ASSERTCRASH( upgrade, ("parseUpgradeDefinition: Unable to allocate upgrade '%s'", name) );

	// parse the ini definition
	ini->initFromINI( upgrade, upgrade->getFieldParse() );

}

