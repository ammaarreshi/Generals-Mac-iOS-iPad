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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// FILE: SabotageSupplyDropzoneCrateCollide.cpp
// Author: Kris Morness, June 2003
// Desc:   A crate (actually a saboteur - mobile crate) that resets the timer on the target supply dropzone.
//
///////////////////////////////////////////////////////////////////////////////////////////////////



// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameAudio.h"
#include "Common/MiscAudio.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Radar.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"

#include "GameClient/Drawable.h"
#include "GameClient/Eva.h"
#include "GameClient/GameText.h"
#include "GameClient/InGameUI.h"  // useful for printing quick debug strings when we need to

#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/HijackerUpdate.h"
#include "GameLogic/Module/OCLUpdate.h"
#include "GameLogic/Module/SabotageSupplyDropzoneCrateCollide.h"



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SabotageSupplyDropzoneCrateCollide::SabotageSupplyDropzoneCrateCollide( Thing *thing, const ModuleData* moduleData ) : CrateCollide( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SabotageSupplyDropzoneCrateCollide::~SabotageSupplyDropzoneCrateCollide()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool SabotageSupplyDropzoneCrateCollide::isValidToExecute( const Object *other ) const
{
	if( !CrateCollide::isValidToExecute(other) )
	{
		//Extend functionality.
		return FALSE;
	}

	if( other->isEffectivelyDead() )
	{
		//Can't sabotage dead structures
		return FALSE;
	}

	if( !other->isKindOf( KINDOF_FS_SUPPLY_DROPZONE ) )
	{
		//We can only sabotage supply dropzones.
		return FALSE;
	}

#if !RETAIL_COMPATIBLE_CRC
	if (other->getStatusBits().testForAny(MAKE_OBJECT_STATUS_MASK2(OBJECT_STATUS_UNDER_CONSTRUCTION, OBJECT_STATUS_SOLD)))
	{
		// TheSuperHackers @bugfix Stubbjax 03/08/2025 Can't enter something being sold or under construction.
		return FALSE;
	}
#endif

	Relationship r = getObject()->getRelationship( other );
	if( r != ENEMIES )
	{
		//Can only sabotage enemy buildings.
		return FALSE;
	}

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool SabotageSupplyDropzoneCrateCollide::executeCrateBehavior( Object *other )
{
	//Check to make sure that the other object is also the goal object in the AIUpdateInterface
	//in order to prevent an unintentional conversion simply by having the terrorist walk too close
	//to it.
	//Assume ai is valid because CrateCollide::isValidToExecute(other) checks it.
	Object *obj = getObject();
	AIUpdateInterface* ai = obj->getAIUpdateInterface();
	if (ai && ai->getGoalObject() != other)
	{
		return false;
	}

	TheRadar->tryInfiltrationEvent( other );

  doSabotageFeedbackFX( other, CrateCollide::SAB_VICTIM_DROP_ZONE );

	//Reset the timer on the dropzone. Um... only the dropzone has an OCLUpdate and one, so
	//we can "assume" it's going to be safe. Otherwise, we'll have to write code to search for
	//a specific OCLUpdate.
	static NameKeyType key_ocl = NAMEKEY( "OCLUpdate" );
	OCLUpdate *update = (OCLUpdate*)other->findUpdateModule( key_ocl );
	if( update )
	{
		update->resetTimer();
	}

	//Steal cash!
	Money *targetMoney = other->getControllingPlayer()->getMoney();
	Money *objectMoney = obj->getControllingPlayer()->getMoney();
	if( targetMoney && objectMoney )
	{
		UnsignedInt cash = targetMoney->countMoney();
		UnsignedInt desiredAmount = getSabotageSupplyDropzoneCrateCollideModuleData()->m_stealCashAmount;
		//Check to see if they have the cash, otherwise, take the remainder!
		cash = min( desiredAmount, cash );
		if( cash > 0 )
		{
			//Steal the cash
			targetMoney->withdraw( cash );
			objectMoney->deposit( cash );
			Player* controller = obj->getControllingPlayer();
			if (controller)
				controller->getScoreKeeper()->addMoneyEarned( cash );

			//Play the "cash stolen" EVA event if the local player is the victim!
			if( other->isLocallyViewed() )
			{
				TheEva->setShouldPlay( EVA_CashStolen );
			}

			//Display cash income floating over the about to be deleted saboteur.
			UnicodeString moneyString;
			moneyString.format( TheGameText->fetch( "GUI:AddCash" ), cash );
			Coord3D pos;
			pos.set( *obj->getPosition() );
			pos.z += 20.0f; //add a little z to make it show up above the unit.
			TheInGameUI->addFloatingText( moneyString, &pos, GameMakeColor( 0, 255, 0, 255 ) );

			//Display cash lost floating over the target
			moneyString.format( TheGameText->fetch( "GUI:LoseCash" ), cash );
			pos.set( *other->getPosition() );
			pos.z += 30.0f; //add a little z to make it show up above the unit.
			TheInGameUI->addFloatingText( moneyString, &pos, GameMakeColor( 255, 0, 0, 255 ) );
		}
		else
		{
			if( other->isLocallyViewed() )
			{
				TheEva->setShouldPlay( EVA_BuildingSabotaged );
			}
		}
	}


	return TRUE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SabotageSupplyDropzoneCrateCollide::crc( Xfer *xfer )
{

	// extend base class
	CrateCollide::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SabotageSupplyDropzoneCrateCollide::xfer( Xfer *xfer )
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
void SabotageSupplyDropzoneCrateCollide::loadPostProcess()
{

	// extend base class
	CrateCollide::loadPostProcess();

}
