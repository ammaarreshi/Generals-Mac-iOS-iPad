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

// FILE: CleanupHazardUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, August 2002
// Desc:   Update module to handle independent targeting of hazards to cleanup.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_WEAPONSLOTTYPE_NAMES

#include "Common/RandomValue.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/Module/CleanupHazardUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/Module/AIUpdate.h"


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CleanupHazardUpdateModuleData::CleanupHazardUpdateModuleData()
{
	m_weaponSlot				= PRIMARY_WEAPON;
	m_scanFrames				= 0;
	m_scanRange					= 0.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void CleanupHazardUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "WeaponSlot",						INI::parseLookupList,						TheWeaponSlotTypeNamesLookupList, offsetof( CleanupHazardUpdateModuleData, m_weaponSlot ) },
		{ "ScanRate",							INI::parseDurationUnsignedInt,	nullptr, offsetof( CleanupHazardUpdateModuleData, m_scanFrames ) },
		{ "ScanRange",						INI::parseReal,									nullptr, offsetof( CleanupHazardUpdateModuleData, m_scanRange ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
CleanupHazardUpdate::CleanupHazardUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_bestTargetID							= INVALID_ID;
	m_nextScanFrames						= 0;
	m_nextShotAvailableInFrames = 0;
	m_inRange  									= false;
	m_weaponTemplate						= nullptr;
	m_moveRange									= 0.0f;
	m_pos.zero();

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CleanupHazardUpdate::~CleanupHazardUpdate()
{

}


//-------------------------------------------------------------------------------------------------
void CleanupHazardUpdate::onObjectCreated()
{
	const CleanupHazardUpdateModuleData *data = getCleanupHazardUpdateModuleData();
	Object *self = getObject();

	//Make sure we have a weapon template
	self->setWeaponSetFlag( WEAPONSET_VETERAN );
	Weapon *weapon = self->getWeaponInWeaponSlot( data->m_weaponSlot );
	if( !weapon )
	{
		DEBUG_CRASH( ("CleanupHazardUpdate for %s doesn't have a valid weapon template",
			getObject()->getTemplate()->getName().str() ) );
		return;
	}
	m_weaponTemplate = weapon->getTemplate();

	//Make sure our firing range is smaller than the scan range.
	WeaponBonus bonus;
	bonus.clear();
	Real attackRange = m_weaponTemplate->getAttackRange( bonus );
	if( data->m_scanRange <= attackRange )
	{
		DEBUG_CRASH( ("CleanupHazardUpdate for %s requires the scan range (%.1f) being larger than the firing range (%.1f)",
			getObject()->getTemplate()->getName().str(), data->m_scanRange, attackRange ) );
	}
}

//-------------------------------------------------------------------------------------------------
/** The update callback. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime CleanupHazardUpdate::update()
{
/// @todo srj use SLEEPY_UPDATE here
	const CleanupHazardUpdateModuleData *data = getCleanupHazardUpdateModuleData();
	Object *obj = getObject();

	//Make sure we are "busy" for scripting purposes if the unit is cleaning up an area.
	if( m_moveRange > 0.0f )
	{
		//Means we are cleaning up an AREA, not just things immediately in range.
		AIUpdateInterface *ai = obj->getAI();
		if( ai )
		{
			if( ai->isIdle() )
			{
				//Keep him busy even though he's not moving -- he might be cleaning up.
				ai->aiBusy( CMD_FROM_AI );
			}
			else if( ai->getLastCommandSource() != CMD_FROM_AI )
			{
				//Either the player or a script gave a NEW order so abandon the cleanup area cause.
				m_moveRange = 0.0f;
				return UPDATE_SLEEP_NONE;
			}
		}
	}

	//Optimized firing at acquired target
	if( m_nextScanFrames > 0 )
	{
		m_nextScanFrames--;
		fireWhenReady(); //Only happens if something is tracked.
		return UPDATE_SLEEP_NONE;
	}
	m_nextScanFrames = data->m_scanFrames;

	//Periodic scanning (expensive)
	if( scanClosestTarget() )
	{
		//1 frame can make a big difference so fire ASAP!
		fireWhenReady();
	}
	else if( m_moveRange )
	{
		//There's nothing nearby, so if we are cleaning up an area versus hazards
		//immediately in range, set the AI to idle so it can advance to the next script!
		AIUpdateInterface *ai = obj->getAI();
		if( ai && (ai->isIdle() || ai->isBusy()) )
		{
			Real fDist = WWMath::SqrtOrigin( ThePartitionManager->getDistanceSquared( obj, &m_pos, FROM_CENTER_2D ) );
			if( fDist < 25.0f )
			{
				//Abort clean area because there's nothing left to clean!
				m_moveRange = 0.0f;
			}
			else
			{
				//Abort clean area AFTER we move back to our initial position!
				ai->aiMoveToPosition( &m_pos, CMD_FROM_AI );
			}
		}
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
void CleanupHazardUpdate::fireWhenReady()
{
	const CleanupHazardUpdateModuleData *data = getCleanupHazardUpdateModuleData();
	Object *self = getObject();

	//Track our target (this code prevents the object from moving beyond range)
	//If we are ordered to clean an area, then range doesn't matter because
	//we allow the unit to move to the area.
	Object *target = TheGameLogic->findObjectByID( m_bestTargetID );
	if( target && m_moveRange == 0.0f )
	{
		WeaponBonus bonus;
		bonus.clear();
		Real fireRange = m_weaponTemplate->getAttackRange( bonus );
		Object *me = getObject();
		Real fDist = WWMath::SqrtOrigin( ThePartitionManager->getDistanceSquared( me, target, FROM_CENTER_2D ) );
		if( fDist < fireRange )
		{
			//We are currently in range!
			m_inRange = true;
		}
		else
		{
			if( m_inRange )
			{
				//We were in range last frame, but the target has moved out of firing range, so
				//re-evaluate by forcing a new scan.
				m_nextScanFrames = GameLogicRandomValue( 0, 3 );
				m_bestTargetID = INVALID_ID;
				if( !m_nextScanFrames )
				{
					scanClosestTarget();
					m_nextScanFrames = data->m_scanFrames;
					target = nullptr; //Set target to nullptr so we don't shoot at it (might be out of range)
				}
			}
			else
			{
				//Not in range
				m_inRange = false;
			}
		}
	}

	if( m_nextShotAvailableInFrames > 0 )
	{
		//We can't fire this frame.
		m_nextShotAvailableInFrames--;
		return;
	}

	//Fire control!
	if( target )
	{
		AIUpdateInterface *ai = self->getAI();
		if( ai )
		{
			if( ai->isIdle() || ai->isBusy() )
			{
				// lock it just till the weapon is empty or the attack is "done"
				self->setWeaponLock( data->m_weaponSlot, LOCKED_TEMPORARILY );
				ai->aiAttackObject( target, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
Object* CleanupHazardUpdate::scanClosestTarget()
{
	const CleanupHazardUpdateModuleData *data = getCleanupHazardUpdateModuleData();
	Object *me = getObject();
	Object *bestTargetInRange = nullptr;
	m_bestTargetID = INVALID_ID;

	PartitionFilterAcceptByKindOf kindFilter(MAKE_KINDOF_MASK(KINDOF_CLEANUP_HAZARD), KINDOFMASK_NONE);
	PartitionFilterSameMapStatus filterMapStatus(getObject());
	PartitionFilter* filters[] = { &kindFilter, &filterMapStatus, nullptr };

	if( m_moveRange > 0.0f )
	{
		//Look for targets around the target position only (but add scan range and move range).
		//This case only happens when we are performing a cleanup area command.
		bestTargetInRange = ThePartitionManager->getClosestObject( &m_pos, data->m_scanRange + m_moveRange, FROM_CENTER_2D, filters );
	}
	else
	{
		//Look for targets near me -- passive default.
		bestTargetInRange = ThePartitionManager->getClosestObject( me->getPosition(), data->m_scanRange, FROM_CENTER_2D, filters );
	}

	if( bestTargetInRange )
	{
		m_bestTargetID = bestTargetInRange->getID();
	}
	return bestTargetInRange;
}

//-------------------------------------------------------------------------------------------------
//This allows the unit to cleanup an area until clean, then the AI goes idle.
//-------------------------------------------------------------------------------------------------
void CleanupHazardUpdate::setCleanupAreaParameters( const Coord3D *pos, Real range )
{
	Object *obj = getObject();
	AIUpdateInterface *ai = obj->getAI();

	//Setting the move range triggers that passive conditions for it to be allowed to move
	//from the specified position.
	m_moveRange = range;
	m_pos = *pos;

	if( ai )
	{
		//CMD_FROM_AI important because it'll abort when other types are used -- like if a player
		//or script orders the unit to do something else, we need a way to cancel this passive
		//situation.
		ai->aiMoveToPosition( pos, CMD_FROM_AI );
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void CleanupHazardUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void CleanupHazardUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// best target id
	xfer->xferObjectID( &m_bestTargetID );

	// in range
	xfer->xferBool( &m_inRange );

	// next scan frames
	xfer->xferInt( &m_nextScanFrames );

	// next shot available in frames
	xfer->xferInt( &m_nextShotAvailableInFrames );

	// don't need to save weapon template, it's retrieved onObjectCreated
	// const WeaponTemplate *m_weaponTemplate;

	// pos
	xfer->xferCoord3D( &m_pos );

	// move range
	xfer->xferReal( &m_moveRange );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void CleanupHazardUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
