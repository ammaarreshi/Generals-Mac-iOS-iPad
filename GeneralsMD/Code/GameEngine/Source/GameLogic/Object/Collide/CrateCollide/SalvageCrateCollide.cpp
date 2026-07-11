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

// FILE: SalvageCrateCollide.cpp ///////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, March 2002
// Desc:   The Savlage system can give a Weaponset bonus, a level, or money.  Salvagers create them
//					by killing marked units, and only WeaponSalvagers can get the WeaponSet bonus
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "GameLogic/Module/SalvageCrateCollide.h"

#include "Common/AudioEventRTS.h"
#include "Common/MiscAudio.h"
#include "Common/KindOf.h"
#include "Common/RandomValue.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/GameText.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SalvageCrateCollide::SalvageCrateCollide( Thing *thing, const ModuleData* moduleData ) : CrateCollide( thing, moduleData )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SalvageCrateCollide::~SalvageCrateCollide()
{

}

//-------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::isValidToExecute( const Object *other ) const
{
	if( ! CrateCollide::isValidToExecute( other ) )
		return FALSE;

	// Only salvage units can pick up a Salvage crate
	if( ! other->getTemplate()->isKindOf( KINDOF_SALVAGER ) )
		return FALSE;

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::executeCrateBehavior( Object *other )
{
	if( eligibleForArmorSet(other) )// No percent chance on this one, if you can get it, you get it.
	{
		doArmorSet(other);

		//Play the salvage installation crate pickup sound.
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_crateSalvage;
		soundToPlay.setObjectID( other->getID() );
		TheAudio->addAudioEvent( &soundToPlay );
	}
	else if( eligibleForWeaponSet( other ) && testWeaponChance() )
	{
		doWeaponSet( other );

		//Play the salvage installation crate pickup sound.
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_crateSalvage;
		soundToPlay.setObjectID( other->getID() );
		TheAudio->addAudioEvent( &soundToPlay );

		//Play the unit voice acknowledgement for upgrading weapons.
		//Already handled by the "move order"
		//const AudioEventRTS *soundToPlayPtr = other->getTemplate()->getPerUnitSound( "VoiceSalvage" );
		//soundToPlay = *soundToPlayPtr;
		//soundToPlay.setObjectID( other->getID() );
		//TheAudio->addAudioEvent( &soundToPlay );
	}
	else if( eligibleForLevel( other ) && testLevelChance() )
	{
		doLevelGain( other );

		//Sound will play in
		//soundToPlay = TheAudio->getMiscAudio()->m_unitPromoted;
	}
	else // just assume the testMoneyChance
	{
		doMoney( other );
		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_crateMoney;
		soundToPlay.setObjectID( other->getID() );
		TheAudio->addAudioEvent(&soundToPlay);
	}

	other->getControllingPlayer()->getAcademyStats()->recordSalvageCollected();

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::eligibleForWeaponSet( Object *other )
{
	if( other == nullptr )
		return FALSE;

	// A kindof marks eligibility, and you must not be fully upgraded
	if( !other->isKindOf(KINDOF_WEAPON_SALVAGER) )
		return FALSE;
	if( other->testWeaponSetFlag(WEAPONSET_CRATEUPGRADE_TWO) )
		return FALSE;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::eligibleForArmorSet( Object *other )
{
	if( other == nullptr )
		return FALSE;

	// A kindof marks eligibility, and you must not be fully upgraded
	if( !other->isKindOf(KINDOF_ARMOR_SALVAGER) )
		return FALSE;
	if( other->testArmorSetFlag(ARMORSET_CRATE_UPGRADE_TWO) )
		return FALSE;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::eligibleForLevel( Object *other )
{
	if( other == nullptr )
		return FALSE;

	// Sorry, you are max level
	if( other->getExperienceTracker()->getVeterancyLevel() == LEVEL_HEROIC )
		return FALSE;

	// Sorry, you can't gain levels
	if( !other->getExperienceTracker()->isTrainable() )
		return FALSE;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::testWeaponChance()
{
	const SalvageCrateCollideModuleData *md = getSalvageCrateCollideModuleData();
	if( md->m_weaponChance == 1.0f )
		return TRUE; // don't waste a random number for a 100%

	Real randomNumber = GameLogicRandomValueReal( 0, 1 );
	if( randomNumber < md->m_weaponChance )
		return TRUE;

	return FALSE;
}

// ------------------------------------------------------------------------------------------------
Bool SalvageCrateCollide::testLevelChance()
{
	const SalvageCrateCollideModuleData *md = getSalvageCrateCollideModuleData();
	if( md->m_levelChance == 1.0f )
		return TRUE; // don't waste a random number for a 100%

	Real randomNumber = GameLogicRandomValueReal( 0, 1 );
	if( randomNumber < md->m_levelChance )
		return TRUE;

	return FALSE;
}

// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::doWeaponSet( Object *other )
{
	if( other->testWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE ) )
	{
		other->clearWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE );
		other->setWeaponSetFlag( WEAPONSET_CRATEUPGRADE_TWO );
	}
	else
	{
		other->setWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE );
	}
}

// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::doArmorSet( Object *other )
{
	if( other->testArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE ) )
	{
		other->clearArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE );
		other->setArmorSetFlag( ARMORSET_CRATE_UPGRADE_TWO );

		other->clearAndSetModelConditionState(MODELCONDITION_ARMORSET_CRATEUPGRADE_ONE, MODELCONDITION_ARMORSET_CRATEUPGRADE_TWO);
	}
	else
	{
		other->setArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE );

		other->setModelConditionState(MODELCONDITION_ARMORSET_CRATEUPGRADE_ONE);
	}
}

// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::doLevelGain( Object *other )
{
	other->getExperienceTracker()->gainExpForLevel( 1 );
}

// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::doMoney( Object *other )
{
	const SalvageCrateCollideModuleData *md = getSalvageCrateCollideModuleData();

	Int money;
	if( md->m_minimumMoney != md->m_maximumMoney )// Random value doesn't like to get a constant range
		money = GameLogicRandomValue( md->m_minimumMoney, md->m_maximumMoney );
	else
		money = md->m_minimumMoney;

	if( money > 0 )
	{
		other->getControllingPlayer()->getMoney()->deposit( money );
		other->getControllingPlayer()->getScoreKeeper()->addMoneyEarned( money );

		//Display cash income floating over the crate.  Position is me, everything else is them.
		UnicodeString moneyString;
		moneyString.format( TheGameText->fetch( "GUI:AddCash" ), money );
		Coord3D pos;
		pos.set( *getObject()->getPosition() );
		pos.z += 10.0f; //add a little z to make it show up above the unit.
		Color color = other->getControllingPlayer()->getPlayerColor() | GameMakeColor( 0, 0, 0, 230 );
		TheInGameUI->addFloatingText( moneyString, &pos, color );
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::crc( Xfer *xfer )
{

	// extend base class
	CrateCollide::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	CrateCollide::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SalvageCrateCollide::loadPostProcess()
{

	// extend base class
	CrateCollide::loadPostProcess();

}
