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

// FILE: SlavedUpdate.cpp /////////////////////////////////////////////////////////////////////////
// Author: Matt Campbell, March 2002
// Updated: Kris Morness, July 2002 -- Add support for advanced scout drone abilities
// Desc:  Slaved unit(s) remain close to their master. Used by scout drones, and used by stinger
//        soldiers that are close to a stinger site. It's important to note that any slaved units
//				can use any or all features, some of which are specialized.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "Common/Team.h"
#include "Common/MiscAudio.h"
#include "GameClient/Drawable.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Weapon.h"


#define STRAY_MULTIPLIER 2.0f // Multiplier from stating diestance from tunnel, to max distance from
const Real CLOSE_ENOUGH = 15;				// Our moveTo commands and pathfinding can't handle people in the way, so quit trying to hump someone on your spot
const Real CLOSE_ENOUGH_SQR = (CLOSE_ENOUGH * CLOSE_ENOUGH);

//-------------------------------------------------------------------------------------------------
SlavedUpdate::SlavedUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
//	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();

	m_slaver = INVALID_ID;
	m_guardPointOffset.zero();
	m_framesToWait = 0;
	m_repairState = REPAIRSTATE_NONE;
	m_repairing = false;
}

//-------------------------------------------------------------------------------------------------
SlavedUpdate::~SlavedUpdate()
{
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::onObjectCreated()
{
	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();

	if( data->m_repairRatePerSecond > 0.0f )
	{
		//If this object can repair, pack it up at init.
		getObject()->setModelConditionState( MODELCONDITION_PACKING );
	}
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::onEnslave( const Object *slaver )
{
	startSlavedEffects( slaver );
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::onSlaverDie( const DamageInfo *info )
{
	stopSlavedEffects();
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::onSlaverDamage( const DamageInfo *info )
{
	// Only slaves with a ProneUpdate will even care.
	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if( ai )
		ai->aiGoProne( info, CMD_FROM_AI );
}


//-------------------------------------------------------------------------------------------------
UpdateSleepTime SlavedUpdate::update()
{
/// @todo srj use SLEEPY_UPDATE here
	if( m_framesToWait > 0 )
	{
		m_framesToWait--;
	}
	if( m_repairState == REPAIRSTATE_NONE )
	{
		if( m_framesToWait > 0 )
		{
			return UPDATE_SLEEP_NONE;
		}
		m_framesToWait = SLAVED_UPDATE_RATE;
	}

	if( m_slaver == INVALID_ID )
		return UPDATE_SLEEP_NONE;

	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();
	Object *me = getObject();
	if( !me )
	{
		return UPDATE_SLEEP_NONE;
	}
	AIUpdateInterface *myAI = me->getAIUpdateInterface();
	if( !myAI )
	{
		return UPDATE_SLEEP_NONE;
	}
	Locomotor *locomotor = myAI->getCurLocomotor();
	if( !locomotor )
	{
		return UPDATE_SLEEP_NONE;
	}

	Object *master = TheGameLogic->findObjectByID( m_slaver );
	if( !master || master->isEffectivelyDead() || master->isDisabledByType( DISABLED_UNMANNED ) )
	{
		stopSlavedEffects();

		//Killing is lame.
		//me->kill();

		//Let's disable the drone so it crashes instead!
		//Added special case code in physics falling to ensure death.
		me->setDisabled( DISABLED_UNMANNED );
    if ( me->getAI() )
      me->getAI()->aiIdle( CMD_FROM_AI);

		return UPDATE_SLEEP_NONE;
	}
	else
	{
		Team *masterTeam = master->getTeam();
		Team *myTeam     = me->getTeam();
		if ( masterTeam->getRelationship( myTeam ) != ALLIES )
		{//slaver must have been hijacked or something..	// we will join his team
			me->defect( masterTeam, 0 );
		}




	}

	if (data->m_stayOnSameLayerAsMaster)
		me->setLayer(master->getLayer());

	//Clear the drone spotting bonus to the master. Up to the drone
	//to satisfy the conditions to set it again for the next update.
	master->clearWeaponBonusCondition( WEAPONBONUSCONDITION_DRONE_SPOTTING );

	//Get my master's AI. If he is attacking something, grant him a range bonus,
	//and I'll fly over the target.
	Object *target = nullptr;
	AIUpdateInterface *masterAI = master->getAIUpdateInterface();
	if( masterAI )
	{
		target = masterAI->getCurrentVictim();
	}

	//Calculate the health percentage of the master -- there are two places we care.
	//NOTE: Health percentage will always be 100 should the drone be incapable of
	//repairing.
	Int healthPercentage = 100;
	if( data->m_repairRatePerSecond > 0.0f )
	{
		BodyModuleInterface *body = master->getBodyModule();
		if( body )
		{
			Real health = body->getHealth();
			Real maxHealth = body->getMaxHealth();
			healthPercentage = (Int)(health / maxHealth * 100.0f);
		}
	}

	//Determine whether or not we need to go back to the master to repair him
	if( healthPercentage <= data->m_repairWhenHealthBelowPercentage )
	{
		//1ST PRIORITY: Go to the master's position to repair him because he needs it.
		doRepairLogic();
		return UPDATE_SLEEP_NONE;
	}

	if( data->m_attackRange )
	{
		//2ND PRIORITY: Go to the master's current victim (as close as wander distance allows)
		if( target )
		{
			//At this point, we officially are in an attack mode! Now, simply
			endRepair();
			doAttackLogic( target );
			return UPDATE_SLEEP_NONE;
		}
	}

	if( data->m_scoutRange )
	{
		//3RD PRIORITY: Hover above master's current move destination (as close as wander distance
		//allows).
		if( masterAI->getPath() )
		{
			const Coord3D *masterDest = masterAI->getPath()->getLastNode()->getPosition();

			//Check to see if master is close to the goal position.
			Real distSqr = ThePartitionManager->getDistanceSquared( master, masterDest, FROM_BOUNDINGSPHERE_2D );
			if( distSqr > (data->m_guardMaxRange * 0.5f) * (data->m_guardMaxRange * 0.5f) )
			{
				//If the master's distance to destination is more than half of the guarding range of the slave,
				//then order the slave to scout it.
				endRepair();
				doScoutLogic( masterDest );
				return UPDATE_SLEEP_NONE;
			}
		}
	}

	//NOTE: Health percentage will always be 100 should the drone be incapable of
	//repairing.
	if( healthPercentage < 100 )
	{
		//3RD PRIORITY: Go to the master's position to repair him (because we're idle)
		doRepairLogic();
		return UPDATE_SLEEP_NONE;
	}

	// update our "pinned" location based on where our master is
	Coord3D pinnedPosition = *master->getPosition();
	pinnedPosition.x += m_guardPointOffset.x;
	pinnedPosition.y += m_guardPointOffset.y;
	m_guardPointOffset.z = TheTerrainLogic->getGroundHeight( pinnedPosition.x, pinnedPosition.y );

	if( data->m_guardMaxRange )
	{
		//3RD PRIORITY: Guard the master's area.
		if( myAI->isIdle() && ThePartitionManager->getDistanceSquared(me, &pinnedPosition, FROM_CENTER_3D) > CLOSE_ENOUGH_SQR )
		{
			//I'm idle and too far away.
			endRepair();
			doGuardLogic( &pinnedPosition );
		}
		else if( ThePartitionManager->getDistanceSquared( me, master, FROM_CENTER_3D ) > sqr(STRAY_MULTIPLIER * data->m_guardMaxRange ) )
		{
			//I'm too far away, no matter what I'm doing.
			endRepair();
			doGuardLogic( &pinnedPosition );
		}
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
// We are ordered to attempt to get as close as possible to my master's target.
//-------------------------------------------------------------------------------------------------
void SlavedUpdate::doAttackLogic( const Object *target )
{
	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();
	Object *me = getObject();
	Object *master = TheGameLogic->findObjectByID( m_slaver );
	Coord3D attackPosition;

	//First, determine the attack position. If the target is too far away, then we'll
	//calculate the closest allowable position.
	const Coord3D *targetPos = target->getPosition();
	Real dist = ThePartitionManager->getDistanceSquared( me, targetPos, FROM_BOUNDINGSPHERE_2D );
	if( dist > sqr( data->m_attackRange ) )
	{
		//The distance is too far, so calculate the best allowable position.
		Coord3D vector;
		vector.set( *targetPos );
		vector.sub( *master->getPosition() );
		vector.normalize();
		vector.scale( data->m_attackRange );

		//Now that we have calculated the vector relative to me, add it to my position to get my goal.
		attackPosition.set( *master->getPosition() );
		attackPosition.add( vector );
	}
	else
	{
		//We are close enough, so use the target position -- easy!
		attackPosition.set( *targetPos );
	}

	//Finally, if we have a wander distance, then randomly select a point within
	//the wander range radius of the pinned position, and we'll go there.
	if( data->m_attackWanderRange )
	{
		//Allow me to wander away from the pinnedPosition.
		Real randomDirection = GameLogicRandomValue( 0, 2*PI );
		m_guardPointOffset.zero();
		m_guardPointOffset.x += data->m_attackWanderRange * Cos( randomDirection );
		m_guardPointOffset.y += data->m_attackWanderRange * Sin( randomDirection );

		//Offset our pinned position by our random offset.
		attackPosition.x += m_guardPointOffset.x;
		attackPosition.y += m_guardPointOffset.y;
		m_guardPointOffset.z = TheTerrainLogic->getGroundHeight( attackPosition.x, attackPosition.y );
	}

	//Move to the updated position!
	AIUpdateInterface *ai = me->getAIUpdateInterface();
	if( ai )
	{
		ai->aiMoveToPosition( &attackPosition, CMD_FROM_AI );
	}

	if( dist < sqr( data->m_distToTargetToGrantRangeBonus ) )
	{
		//Finally, seeing we are close enough to the target, grant our
		//master extended weapon range!
		master->setWeaponBonusCondition( WEAPONBONUSCONDITION_DRONE_SPOTTING );
	}
}

//-------------------------------------------------------------------------------------------------
// We are ordered to attempt to get as close as possible to my master's movement destination point.
//-------------------------------------------------------------------------------------------------
void SlavedUpdate::doScoutLogic( const Coord3D *mastersDestination )
{
	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();
	Object *me = getObject();
	Object *master = TheGameLogic->findObjectByID( m_slaver );
	Coord3D scoutPosition;

	//First, determine the scout position. If our master's destination is too far away, then we'll
	//calculate the closest allowable position.
	Real dist = ThePartitionManager->getDistanceSquared( me, mastersDestination, FROM_BOUNDINGSPHERE_2D );
	if( dist > sqr( data->m_scoutRange ) )
	{
		//The distance is too far, so calculate the best allowable position.
		Coord3D vector;
		vector.set( *mastersDestination );
		vector.sub( *master->getPosition() );
		vector.normalize();
		vector.scale( data->m_scoutRange );

		//Now that we have calculated the vector relative to me, add it to my position to get my goal.
		scoutPosition.set( *master->getPosition() );
		scoutPosition.add( vector );
	}
	else
	{
		//We are close enough, so use the target position -- easy!
		scoutPosition.set( *mastersDestination );
	}

	//Finally, if we have a wander distance, then randomly select a point within
	//the wander range radius of the pinned position, and we'll go there.
	if( data->m_scoutWanderRange )
	{
		//Allow me to wander away from the pinnedPosition.
		Real randomDirection = GameLogicRandomValue( 0, 2*PI );
		m_guardPointOffset.zero();
		m_guardPointOffset.x += data->m_scoutWanderRange * Cos( randomDirection );
		m_guardPointOffset.y += data->m_scoutWanderRange * Sin( randomDirection );

		//Offset our pinned position by our random offset.
		scoutPosition.x += m_guardPointOffset.x;
		scoutPosition.y += m_guardPointOffset.y;
		m_guardPointOffset.z = TheTerrainLogic->getGroundHeight( scoutPosition.x, scoutPosition.y );
	}

	//Move to the updated position!
	AIUpdateInterface *ai = me->getAIUpdateInterface();
	if( ai )
	{
		ai->aiMoveToPosition( &scoutPosition, CMD_FROM_AI );
	}
}

//-------------------------------------------------------------------------------------------------
// We are ordered to attempt to get as close as possible to my master's position.
//-------------------------------------------------------------------------------------------------
void SlavedUpdate::doGuardLogic( Coord3D *pinnedPosition )
{
	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();
	Object *me = getObject();

	if( data->m_guardWanderRange )
	{
		// recalc where we want to be if we wander around
		Real randomDirection = GameLogicRandomValue( 0, 2*PI );
		m_guardPointOffset.zero();
		m_guardPointOffset.x += data->m_guardMaxRange * Cos( randomDirection );
		m_guardPointOffset.y += data->m_guardMaxRange * Sin( randomDirection );

		pinnedPosition->x += m_guardPointOffset.x;
		pinnedPosition->y += m_guardPointOffset.y;
		m_guardPointOffset.z = TheTerrainLogic->getGroundHeight( pinnedPosition->x, pinnedPosition->y );
	}
	AIUpdateInterface *ai = me->getAIUpdateInterface();
	if( ai )
	{
		ai->aiMoveToPosition( pinnedPosition, CMD_FROM_AI );
	}
}

//-------------------------------------------------------------------------------------------------
// We are ordered to repair our master
//-------------------------------------------------------------------------------------------------
void SlavedUpdate::doRepairLogic()
{
	Object *me = getObject();
	Object *master = TheGameLogic->findObjectByID( m_slaver );
	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();
	AIUpdateInterface *ai = me->getAIUpdateInterface();
	if( !ai )
	{
		return;
	}

	//There are two major things... either move closer or repair.
	Real distSqr = ThePartitionManager->getDistanceSquared( me, master, FROM_BOUNDINGSPHERE_2D );
	Bool closeEnough = distSqr < 12.0f * 12.0f;

	//We're going to do different things based on the repair state.
	if( closeEnough )
	{
		//Now drive the repairing states.
		switch( m_repairState )
		{
			case REPAIRSTATE_NONE:
				setRepairState( REPAIRSTATE_READY );
				break;
			case REPAIRSTATE_READY:
			case REPAIRSTATE_EXTENDING:
				if( m_framesToWait == 0 )
				{
					setRepairState( REPAIRSTATE_WELDING );
				}
				break;
			case REPAIRSTATE_UNPACKING:
			case REPAIRSTATE_WELDING:
			case REPAIRSTATE_RETRACTING:
				if( m_framesToWait == 0 )
				{
					setRepairState( REPAIRSTATE_READY );
				}
				break;
		}
	}
	else
	{
		m_repairing = false;

		Bool closeEnoughForZPrecision = distSqr < sqr(master->getGeometryInfo().getBoundingSphereRadius() * 2);

		//We're too far away to repair, so get closer.
		Locomotor *locomotor = ai->getCurLocomotor();
		if( locomotor )
		{
			locomotor->setUsePreciseZPos( closeEnoughForZPrecision );
		}
		Coord3D pos;
		pos.set( *master->getPosition() );
		Real altitude = GameLogicRandomValueReal( data->m_repairMinAltitude, data->m_repairMaxAltitude );
		pos.z += altitude;
		ai->aiMoveToPosition( &pos, CMD_FROM_AI );

		//Also speed things up by retracting the repair arm (so viewers
		//will see it's intentions)
		if( m_framesToWait == 0 )
		{
			setRepairState( REPAIRSTATE_READY );
		}
	}

	if( closeEnough && m_repairing )
	{
		//We're close enough to repair.
		BodyModuleInterface *body = master->getBodyModule();
		if( body )
		{
			//Calculate the repair rate per frame.
			Real repairAmount = data->m_repairRatePerSecond / static_cast<float>(LOGICFRAMES_PER_SECOND);

			DamageInfo healingInfo;
			healingInfo.in.m_amount = repairAmount;
			healingInfo.in.m_damageType = DAMAGE_HEALING;
			healingInfo.in.m_deathType = DEATH_NONE;
			body->attemptHealing( &healingInfo );
		}
	}
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::endRepair()
{
	if( m_repairState != REPAIRSTATE_NONE )
	{
		//Remove the state so we can start using the update timer again.
		m_repairState = REPAIRSTATE_NONE;
		m_framesToWait = SLAVED_UPDATE_RATE;
		m_repairing = false;

		//Set the packing state and clear any other states (to force the arm to fully retract)
		setRepairModelConditionStates( MODELCONDITION_PACKING );
	}

	AIUpdateInterface* ai = getObject()->getAIUpdateInterface();
	if( ai )
	{
		ai->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
		Locomotor* locomotor = ai->getCurLocomotor();
		if (locomotor)
		{
			locomotor->setUltraAccurate(false);
			locomotor->setUsePreciseZPos(false);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::setRepairModelConditionStates( ModelConditionFlagType flag )
{
	Object *obj = getObject();
	obj->clearModelConditionState( MODELCONDITION_PACKING );
	obj->clearModelConditionState( MODELCONDITION_UNPACKING );
	obj->clearModelConditionState( MODELCONDITION_FIRING_B );
	obj->clearModelConditionState( MODELCONDITION_FIRING_C );
	obj->clearModelConditionState( MODELCONDITION_BETWEEN_FIRING_SHOTS_B );
	obj->clearModelConditionState( MODELCONDITION_BETWEEN_FIRING_SHOTS_C );
	obj->clearModelConditionState( MODELCONDITION_RELOADING_B );
	obj->clearModelConditionState( MODELCONDITION_RELOADING_C );
	obj->setModelConditionState( flag );
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::setRepairState( RepairStates repairState )
{
	Object *obj = getObject();
	Drawable *draw = obj->getDrawable();
	//Object *master = TheGameLogic->findObjectByID( m_slaver );
	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();

	if( repairState == m_repairState )
	{
		return;
	}

	switch( repairState )
	{
		case REPAIRSTATE_UNPACKING:
			setRepairModelConditionStates( MODELCONDITION_UNPACKING );
			m_framesToWait = 15;
			break;
		case REPAIRSTATE_PACKING:
			setRepairModelConditionStates( MODELCONDITION_PACKING );
			m_framesToWait = 15;
			break;
		case REPAIRSTATE_READY:
		{
			switch( m_repairState )
			{
				case REPAIRSTATE_NONE:
					//We're not in a repair state -- so assume we need to unpack
					setRepairModelConditionStates( MODELCONDITION_UNPACKING );
					m_repairState = REPAIRSTATE_UNPACKING;
					m_framesToWait = 15;
					break;
				case REPAIRSTATE_WELDING:
					//We are welding so retract before going into ready state.
					m_repairState = REPAIRSTATE_RETRACTING;
					m_framesToWait = 5;
					setRepairModelConditionStates( MODELCONDITION_FIRING_C );
					moveToNewRepairSpot();
					break;
				default:
					m_repairState = REPAIRSTATE_READY;
					m_framesToWait = GameLogicRandomValue( data->m_minReadyFrames, data->m_maxReadyFrames );
					break;
			}
			break;
		}
		case REPAIRSTATE_WELDING:
		{
			if( m_repairState == REPAIRSTATE_READY )
			{
				m_repairState = REPAIRSTATE_EXTENDING;
				m_framesToWait = 5;
				setRepairModelConditionStates( MODELCONDITION_FIRING_B );
				break;
			}
			else
			{
				m_repairState = REPAIRSTATE_WELDING;
				m_framesToWait = GameLogicRandomValue( data->m_minWeldFrames, data->m_maxWeldFrames );

				//Make sparks!
				if( !data->m_weldingSysName.isEmpty() )
				{
					const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate( data->m_weldingSysName );
					if( tmp )
					{
						ParticleSystem *weldingSys = TheParticleSystemManager->createParticleSystem(tmp);
						if( weldingSys )
						{
							Coord3D pos;
							//Get the bone position
							if( draw->getPristineBonePositions( data->m_weldingFXBone.str(), 0, &pos, nullptr, 1 ) )
							{
								pos.add( *obj->getPosition() );
							}
							else
							{
								pos.set( *obj->getPosition() );
							}

							weldingSys->setPosition( &pos );
							Real time = (Real)(m_framesToWait * static_cast<float>(LOGICFRAMES_PER_SECOND));
							weldingSys->setLifetimeRange( time, time );

							AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_repairSparks;
							soundToPlay.setPosition( &pos );
							TheAudio->addAudioEvent( &soundToPlay );
						}
					}
				}

				if( !m_repairing )
				{
					//Don't actually start healing until we fire our first sparks
					m_repairing = true;
					AIUpdateInterface *ai = obj->getAIUpdateInterface();
					if( ai )
					{
						//ai->aiFaceObject( master, CMD_FROM_AI );
					}
				}
				break;
			}
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::moveToNewRepairSpot()
{
	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();
	Object *me = getObject();
	Object *master = TheGameLogic->findObjectByID( m_slaver );

	//Finally, if we have a wander distance, then randomly select a point within
	//the wander range radius of the pinned position, and we'll go there.
	if( data->m_repairRange )
	{
		//Allow me to wander away from the pinnedPosition.
		Real randomDirection = GameLogicRandomValue( 0, 2*PI );
		m_guardPointOffset.set( *master->getPosition() );
		m_guardPointOffset.x += data->m_repairRange * Cos( randomDirection );
		m_guardPointOffset.y += data->m_repairRange * Sin( randomDirection );
		m_guardPointOffset.z = TheTerrainLogic->getGroundHeight( m_guardPointOffset.x, m_guardPointOffset.y );
		Real altitude = GameLogicRandomValueReal( data->m_repairMinAltitude, data->m_repairMaxAltitude );
		m_guardPointOffset.z += altitude;

		AIUpdateInterface *ai = me->getAIUpdateInterface();
		if( ai )
		{
			ai->chooseLocomotorSet( LOCOMOTORSET_PANIC );
			ai->getCurLocomotor()->setUltraAccurate( true );
			ai->aiMoveToPosition( &m_guardPointOffset, CMD_FROM_AI );

			Locomotor *locomotor = ai->getCurLocomotor();
			if( locomotor )
			{
				locomotor->setUsePreciseZPos( true );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::startSlavedEffects( const Object *slaver )
{
	if( slaver == nullptr )
		return;

	m_slaver = slaver->getID();
	const SlavedUpdateModuleData* data = getSlavedUpdateModuleData();

	// Decide where our pinned stray point is
	Real randomDirection = GameLogicRandomValue( 0, 2*PI );
	m_guardPointOffset.zero();
	m_guardPointOffset.x += data->m_guardMaxRange * Cos( randomDirection );
	m_guardPointOffset.y += data->m_guardMaxRange * Sin( randomDirection );

	// mark selves as not selectable
	getObject()->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNSELECTABLE ) );


  if ( slaver->testStatus( OBJECT_STATUS_STEALTHED ) )
  {
    StealthUpdate *myStealth = getObject()->getStealth();
    if ( myStealth )
    {
      myStealth->receiveGrant( true );
      // note to anyone... once stealth is granted to this drone(or such)
      // let its own stealthupdate govern the allowedtostealth cases
    }
  }


}

//-------------------------------------------------------------------------------------------------
void SlavedUpdate::stopSlavedEffects()
{
	m_slaver = INVALID_ID;
	m_guardPointOffset.zero();

	/// @todo Just a thought.  Our Status bits on objects really need to be reference counts so you don't clear someone else's flag
	getObject()->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNSELECTABLE ) );
	getObject()->clearDisabled( DISABLED_HELD );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SlavedUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SlavedUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// slaver
	xfer->xferObjectID( &m_slaver );

	// guard point offset
	xfer->xferCoord3D( &m_guardPointOffset);

	// frames to wait
	xfer->xferInt( &m_framesToWait );

	// repair state
	xfer->xferUser( &m_repairState, sizeof( RepairStates ) );

	// repairing
	xfer->xferBool( &m_repairing );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SlavedUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}

