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

// FILE: PointDefenseLaserUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, August 2002
// Desc:   Update module to handle independent targeting of point defense laser.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/BitFlagsIO.h"
#include "Common/RandomValue.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"

#include "GameClient/Drawable.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/Module/PointDefenseLaserUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Weapon.h"



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
PointDefenseLaserUpdateModuleData::PointDefenseLaserUpdateModuleData()
{
	m_weaponTemplate		= nullptr;
	m_scanFrames				= 0;
	m_scanRange					= 0.0f;
	m_velocityFactor		= 0.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PointDefenseLaserUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "WeaponTemplate",				INI::parseWeaponTemplate,				nullptr, offsetof( PointDefenseLaserUpdateModuleData, m_weaponTemplate ) },
		{ "PrimaryTargetTypes",		KindOfMaskType::parseFromINI,								nullptr, offsetof( PointDefenseLaserUpdateModuleData, m_primaryTargetKindOf ) },
		{ "SecondaryTargetTypes",	KindOfMaskType::parseFromINI,								nullptr, offsetof( PointDefenseLaserUpdateModuleData, m_secondaryTargetKindOf ) },
		{ "ScanRate",							INI::parseDurationUnsignedInt,	nullptr, offsetof( PointDefenseLaserUpdateModuleData, m_scanFrames ) },
		{ "ScanRange",						INI::parseReal,									nullptr, offsetof( PointDefenseLaserUpdateModuleData, m_scanRange ) },
		{ "PredictTargetVelocityFactor", INI::parseReal,					nullptr, offsetof( PointDefenseLaserUpdateModuleData, m_velocityFactor ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
PointDefenseLaserUpdate::PointDefenseLaserUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_bestTargetID = INVALID_ID;
	m_nextScanFrames = 0;
	m_nextShotAvailableInFrames = 0;
	m_inRange  					= false;
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);// No starting sleep, but we want to sleep later.
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
PointDefenseLaserUpdate::~PointDefenseLaserUpdate()
{

}


//-------------------------------------------------------------------------------------------------
void PointDefenseLaserUpdate::onObjectCreated()
{
	const PointDefenseLaserUpdateModuleData *data = getPointDefenseLaserUpdateModuleData();

	//Make sure we have a weapon template
	if( !data->m_weaponTemplate )
	{
		DEBUG_CRASH( ("PointDefenseLaserUpdate for %s doesn't have a valid weapon template",
			getObject()->getTemplate()->getName().str() ) );
		return;
	}

	//Make sure our firing range is smaller than the scan range.
	WeaponBonus bonus;
	bonus.clear();
	Real attackRange = data->m_weaponTemplate->getAttackRange( bonus );
	if( data->m_scanRange <= attackRange )
	{
		DEBUG_CRASH( ("PointDefenseLaserUpdate for %s requires the scan range (%.1f) being larger than the firing range (%.1f)",
			getObject()->getTemplate()->getName().str(), data->m_scanRange, attackRange ) );
	}
}

//-------------------------------------------------------------------------------------------------
/** The update callback. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime PointDefenseLaserUpdate::update()
{
/// @todo srj use SLEEPY_UPDATE here
	//*** HERE'S THE UPDATE PHILOSOPHY ***
	//The point defense laser typically has short range, high rate of fire, and shoots at incoming projectiles
	//that move fast. This amounts to a potentially very expensive system. Instead of frantically scanning for
	//targets, we will scan less frequently (data->m_scanFrames) in a larger radius (data->m_scanRange). When
	//this occurs, we'll store the "best" target, and track only that target until the next update or if it is
	//killed.

	Object *me = getObject();
	if( me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;//No more laser fo you.

	const PointDefenseLaserUpdateModuleData *data = getPointDefenseLaserUpdateModuleData();

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
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
void PointDefenseLaserUpdate::fireWhenReady()
{
	const PointDefenseLaserUpdateModuleData *data = getPointDefenseLaserUpdateModuleData();

	//Track our target
	Object *target = TheGameLogic->findObjectByID( m_bestTargetID );
	if( target )
	{
		WeaponBonus bonus;
		bonus.clear();
		Real fireRange = data->m_weaponTemplate->getAttackRange( bonus );
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

	WeaponTemplate *wt = data->m_weaponTemplate;
	if( wt )
	{
		WeaponBonus bonus;

		//Fire control!
		if( target && m_inRange )
		{
			if( !target->isEffectivelyDead() )
			{
				Weapon* w = TheWeaponStore->allocateNewWeapon( wt, TERTIARY_WEAPON );
				w->loadAmmoNow( getObject() );
				w->fireWeapon( getObject(), target );
				deleteInstance(w);

				// And now that we have shot, set our internal reload timer.
				m_nextShotAvailableInFrames = wt->getDelayBetweenShots( bonus );
			}

			if( target->isEffectivelyDead() )
			{
				m_nextScanFrames = GameLogicRandomValue( 0, 3 );
				m_bestTargetID = INVALID_ID;
				if( !m_nextScanFrames )
				{
					scanClosestTarget();
					m_nextScanFrames = data->m_scanFrames;
				}
			}
		}
	}

}

//-------------------------------------------------------------------------------------------------
Object* PointDefenseLaserUpdate::scanClosestTarget()
{
	const PointDefenseLaserUpdateModuleData *data = getPointDefenseLaserUpdateModuleData();
	Object *me = getObject();
	Object *bestTargetOutOfRange[2] = { nullptr, nullptr };
	Object *bestTargetInRange[2] = { nullptr, nullptr };
	Real closestDist[2];
	Real closestOutsideRange[2];
	Int index;
	WeaponBonus bonus;
	bonus.clear();
	Real fireRange = data->m_weaponTemplate->getAttackRange( bonus );

	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me->getPosition(), data->m_scanRange, FROM_CENTER_2D );
	MemoryPoolObjectHolder hold(iter);

	for( Object *other = iter->first(); other; other = iter->next() )
	{
		if( other->isAnyKindOf( data->m_primaryTargetKindOf ) )
		{
			//Primary target type
			index = 0;
		}
		else if( other->isAnyKindOf( data->m_secondaryTargetKindOf ) )
		{
			//Secondary target type (use only if we can't find any primary targets)
			index = 1;
		}
		else
		{
			//Not a valid target.
			continue;
		}

			// order matters: we want to know if I consider it to be an enemy, not vice versa
		if( getObject()->getRelationship( other ) != ENEMIES )
		{
			//Don't kill our friends!
			continue;
		}

		if( other->testStatus( OBJECT_STATUS_STEALTHED ) && !other->testStatus( OBJECT_STATUS_DETECTED ) )
		{
			//We can't see it.
			continue;
		}

		Real fDist = WWMath::SqrtOrigin( ThePartitionManager->getDistanceSquared( me, other, FROM_CENTER_2D ) );

		if( fDist <= fireRange )
		{
			//Inside fire range (which one is closest?)
			if( !bestTargetInRange[index] || fDist < closestDist[index] )
			{
				closestDist[index] = fDist;
				bestTargetInRange[index] = other;
			}
		}
		else if( !bestTargetInRange[index] )
		{
			//Outside fire range.

			//Determine where the target will be based on current velocity using (m_velocityFactor * frames)
			if( data->m_velocityFactor != 0.0f && !other->isKindOf( KINDOF_IMMOBILE ) )
			{
				Coord3D pos;
				PhysicsBehavior *physics = other->getPhysics();
				if( physics )
				{
					pos.set( *physics->getVelocity() );
					pos.scale( data->m_velocityFactor );
					pos.add( *other->getPosition() );

					//Recalculate the distance.
					fDist = WWMath::SqrtOrigin( ThePartitionManager->getDistanceSquared( me, other, FROM_CENTER_2D ) );
				}
			}

			//Now calculate the best outside range target.
			if( !bestTargetOutOfRange[index] || fDist < closestOutsideRange[index] )
			{
				closestOutsideRange[index] = fDist;
				bestTargetOutOfRange[index] = other;
			}
		}
	}

	if( bestTargetInRange[ 0 ] )
	{
		//This is the best primary target in range.
		m_bestTargetID = bestTargetInRange[ 0 ]->getID();
		m_inRange = true;
		return bestTargetInRange[ 0 ];
	}

	if( bestTargetInRange[ 1 ] )
	{
		//This is the best secondary target in range.
		m_bestTargetID = bestTargetInRange[ 1 ]->getID();
		m_inRange = true;
		return bestTargetInRange[ 1 ];
	}

	if( bestTargetOutOfRange[ 0 ] )
	{
		//This is the best primary target out of range.
		m_bestTargetID = bestTargetOutOfRange[ 0 ]->getID();
		m_inRange = false;
		return bestTargetOutOfRange[ 0 ];
	}

	if( bestTargetOutOfRange[ 1 ] )
	{
		//This is the best secondary target out of range.
		m_bestTargetID = bestTargetOutOfRange[ 1 ]->getID();
		m_inRange = false;
		return bestTargetOutOfRange[ 1 ];
	}

	//Utter failure -- nothing on the scope.
	m_bestTargetID = INVALID_ID;
	m_inRange = false;
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void PointDefenseLaserUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void PointDefenseLaserUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// best target ID
	xfer->xferObjectID( &m_bestTargetID );

	// in range
	xfer->xferBool( &m_inRange );

	// next scan frames
	xfer->xferInt( &m_nextScanFrames );

	// next shot available in frames
	xfer->xferInt( &m_nextShotAvailableInFrames );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void PointDefenseLaserUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
