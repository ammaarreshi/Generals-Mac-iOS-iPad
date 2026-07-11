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

// FILE: MissileAIUpdate.cpp
// Author: Michael S. Booth, December 2001
// Desc:   Implementation of missile behavior

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/RandomValue.h"
#include "Common/BitFlagsIO.h"

#include "GameLogic/AIPathfind.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/MissileAIUpdate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"

const Real BIGNUM = 99999.0f;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
MissileAIUpdateModuleData::MissileAIUpdateModuleData()
{
	m_tryToFollowTarget = true;
	m_fuelLifetime = 0;
	m_ignitionDelay = 0;
	m_initialVel = 0;
	m_initialDist = 0.0f;
	m_diveDistance = 0.0f;
	m_ignitionFX = nullptr;
	m_useWeaponSpeed = false;
	m_detonateOnNoFuel = FALSE;
	m_garrisonHitKillCount = 0;
	m_garrisonHitKillFX = nullptr;
	m_lockDistance = 75.0f;
}

//-----------------------------------------------------------------------------
void MissileAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  AIUpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "TryToFollowTarget",			INI::parseBool,		nullptr, offsetof( MissileAIUpdateModuleData, m_tryToFollowTarget ) },
		{ "FuelLifetime",						INI::parseDurationUnsignedInt,		nullptr, offsetof( MissileAIUpdateModuleData, m_fuelLifetime ) },
		{ "IgnitionDelay",					INI::parseDurationUnsignedInt,		nullptr, offsetof( MissileAIUpdateModuleData, m_ignitionDelay ) },
		{ "InitialVelocity",				INI::parseVelocityReal,		nullptr, offsetof( MissileAIUpdateModuleData, m_initialVel) },
		{ "DistanceToTravelBeforeTurning",	INI::parseReal,		nullptr, offsetof( MissileAIUpdateModuleData, m_initialDist ) },
		{ "DistanceToTargetBeforeDiving",		INI::parseReal,		nullptr, offsetof( MissileAIUpdateModuleData, m_diveDistance ) },
		{ "DistanceToTargetForLock",INI::parseReal,		nullptr, offsetof( MissileAIUpdateModuleData, m_lockDistance ) },
		{ "IgnitionFX",							INI::parseFXList,		nullptr, offsetof( MissileAIUpdateModuleData, m_ignitionFX ) },
		{ "UseWeaponSpeed",				  INI::parseBool,			nullptr, offsetof( MissileAIUpdateModuleData, m_useWeaponSpeed ) },
		{ "DetonateOnNoFuel",			  INI::parseBool,			nullptr, offsetof( MissileAIUpdateModuleData, m_detonateOnNoFuel ) },

		{ "GarrisonHitKillRequiredKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof( MissileAIUpdateModuleData, m_garrisonHitKillKindof ) },
		{ "GarrisonHitKillForbiddenKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof( MissileAIUpdateModuleData, m_garrisonHitKillKindofNot ) },
		{ "GarrisonHitKillCount", INI::parseUnsignedInt, nullptr, offsetof( MissileAIUpdateModuleData, m_garrisonHitKillCount ) },
		{ "GarrisonHitKillFX", INI::parseFXList, nullptr, offsetof( MissileAIUpdateModuleData, m_garrisonHitKillFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
MissileAIUpdate::MissileAIUpdate( Thing *thing, const ModuleData* moduleData ) : AIUpdateInterface( thing, moduleData )
{
	const MissileAIUpdateModuleData* d = getMissileAIUpdateModuleData();

	m_state = PRELAUNCH;
	m_stateTimestamp = TheGameLogic->getFrame();
	m_nextTargetTrackTime = 0x7fffffff;	// so that we never recalc target pos, by default
	m_launcherID = INVALID_ID;
	m_victimID = INVALID_ID;
	m_isArmed = false;
	m_fuelExpirationDate = 0;
	m_noTurnDistLeft = d->m_initialDist;
	m_prevPos = *getObject()->getPosition();
	m_maxAccel = BIGNUM;
	m_detonationWeaponTmpl = nullptr;
	m_exhaustSysTmpl = nullptr;
	m_isTrackingTarget = FALSE;
	m_exhaustID = INVALID_PARTICLE_SYSTEM_ID;
	m_extraBonusFlags = 0;
	m_originalTargetPos.zero();
}

//-------------------------------------------------------------------------------------------------
MissileAIUpdate::~MissileAIUpdate()
{
	tossExhaust();
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::onDelete()
{
	//
	// there is no need to destroy the attached particle systems here because the particle
	// systems themselves detect that their "parent" object is gone and destroy themselves.
	// but to be clean, for the exhaust particle system, we will invalidate the ID so that
	// we can't do anything else with it since we're being deleted
	//
	m_exhaustID = INVALID_PARTICLE_SYSTEM_ID;
}


//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::tossExhaust()
{
	if (m_exhaustID != INVALID_PARTICLE_SYSTEM_ID)
	{
		TheParticleSystemManager->destroyParticleSystemByID(m_exhaustID);
		m_exhaustID = INVALID_PARTICLE_SYSTEM_ID;
	}
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::switchToState(MissileStateType s)
{
	if (m_state != s)
	{
		m_state = s;
		m_stateTimestamp = TheGameLogic->getFrame();
	}
}

//-------------------------------------------------------------------------------------------------
// Prepares the missile for launch via proper weapon-system channels.
//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::projectileLaunchAtObjectOrPosition(
	const Object *victim,
	const Coord3D* victimPos,
	const Object *launcher,
	WeaponSlotType wslot,
	Int specificBarrelToUse,
	const WeaponTemplate* detWeap,
	const ParticleSystemTemplate* exhaustSysOverride
)
{
	DEBUG_ASSERTCRASH(specificBarrelToUse>=0, ("specificBarrelToUse must now be explicit"));

	m_launcherID = launcher ? launcher->getID() : INVALID_ID;
	m_detonationWeaponTmpl = detWeap;
	m_extraBonusFlags = launcher ? launcher->getWeaponBonusCondition() : 0;

	Weapon::positionProjectileForLaunch(getObject(), launcher, wslot, specificBarrelToUse);

	projectileFireAtObjectOrPosition( victim, victimPos, detWeap, exhaustSysOverride );

}

#define APPROACH_HEIGHT 10.0f

//-------------------------------------------------------------------------------------------------
// The actual firing of the missile once setup.
//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::projectileFireAtObjectOrPosition( const Object *victim, const Coord3D *victimPos, const WeaponTemplate *detWeap, const ParticleSystemTemplate* exhaustSysOverride )
{
	const MissileAIUpdateModuleData* d = getMissileAIUpdateModuleData();
	Object *obj = getObject();

	m_exhaustSysTmpl = exhaustSysOverride;
	m_detonationWeaponTmpl = detWeap;
	Real initialVelToUse = d->m_initialVel;
	if (d->m_useWeaponSpeed)
	{
		Real weaponSpeed = detWeap->getWeaponSpeed();
		initialVelToUse = weaponSpeed;

		Locomotor* curLoco = getCurLocomotor();
		if (curLoco)
		{
			m_maxAccel = weaponSpeed;
			curLoco->setMaxSpeed(weaponSpeed);
			curLoco->setMaxAcceleration(m_maxAccel);
		}
	}

	Real deltaZ = victimPos->z - obj->getPosition()->z;
	Real dx = victimPos->x - obj->getPosition()->x;
	Real dy = victimPos->y - obj->getPosition()->y;
	Real xyDist = WWMath::SqrtOrigin(sqr(dx)+sqr(dy));
	if (xyDist<1) xyDist = 1;
	Real zFactor = 0;
	if (deltaZ>0) {
		zFactor = deltaZ/xyDist;
	}


	Vector3 dir = getObject()->getTransformMatrix()->Get_X_Vector();
	dir.Normalize();
	dir.Z += 2*zFactor;
	dir.Normalize();
	PhysicsBehavior* physics = getObject()->getPhysics();
	if (physics && initialVelToUse > 0)
	{
		Real forceMag = physics->getMass() * initialVelToUse;

		Coord3D force;
		force.x = forceMag * dir.X;
		force.y = forceMag * dir.Y;
		force.z = forceMag * dir.Z;

		physics->applyMotiveForce( &force );
	}

	Vector3 objPos(obj->getPosition()->x, obj->getPosition()->y, obj->getPosition()->z);

	Matrix3D newXform;
	newXform.buildTransformMatrix( objPos, dir );
	obj->setTransformMatrix( &newXform );

	switchToState(LAUNCH);
	m_isTrackingTarget = false;
	// Missiles do their thing by colliding with something and exploding.  So they Move to the target
	// instead of Attacking the target.
	if (victim && d->m_tryToFollowTarget)
	{
		getStateMachine()->setGoalPosition(victim->getPosition());
		// ick. const-cast is evil. fix. (srj)
 		aiMoveToObject(const_cast<Object*>(victim), CMD_FROM_AI );
		m_originalTargetPos = *victim->getPosition();
		m_isTrackingTarget = TRUE;// Remember that I was originally shot at a moving object, so if the
		// target dies I can do something cool.
		m_victimID = victim->getID();
	}
	else
	{
		// Otherwise, we are just a Coord shot.
		Coord3D initialPos = *victimPos;
		m_originalTargetPos = *victimPos;
		if (d->m_lockDistance>0.0f) {
			initialPos.z += APPROACH_HEIGHT;
		}
		aiMoveToPosition(&initialPos, CMD_FROM_AI );
		m_victimID = INVALID_ID;
	}

	m_prevPos = *getObject()->getPosition();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool MissileAIUpdate::projectileHandleCollision( Object *other )
{
	Object* obj = getObject();
	const MissileAIUpdateModuleData* d = getMissileAIUpdateModuleData();

	// check if our warhead is "armed" - if not, we are inert
	if (projectileIsArmed() == false)
		return true;

	if (other==nullptr) {
		// we hit the ground.  Check to see if we hit something unexpected.
		Coord3D goal = *getGoalPosition();
		Coord3D pos = *obj->getPosition();
		Coord3D delta;
		delta.x = pos.x-goal.x;
		delta.y = pos.y-goal.y;
		delta.z = pos.z-goal.z;
		if (delta.z > PATHFIND_CELL_SIZE_F) {
			// we're above our target goal.
			if (delta.length() > 3*PATHFIND_CELL_SIZE_F) {
				// we're somewhere else.
				return true;
			}
		}
	}

	if (other != nullptr)
	{
 		Object *projectileLauncher = TheGameLogic->findObjectByID( projectileGetLauncherID() );

 		// if it's not the specific thing we were targeting, see if we should incidentally collide...
 		if (!m_detonationWeaponTmpl->shouldProjectileCollideWith(projectileLauncher, obj, other, m_victimID))
		{
			//DEBUG_LOG(("ignoring projectile collision with %s at frame %d",other->getTemplate()->getName().str(),TheGameLogic->getFrame()));
			return true;
		}

		if (d->m_garrisonHitKillCount > 0)
		{
			ContainModuleInterface* contain = other->getContain();
			if( contain && contain->getContainCount() > 0 && contain->isGarrisonable() && !contain->isImmuneToClearBuildingAttacks() )
			{
				Int numKilled = 0;

				// garrisonable buildings subvert the normal process here.
				const ContainedItemsList* items = contain->getContainedItemsList();
				if (items)
				{
					for (ContainedItemsList::const_iterator it = items->begin(); it != items->end() && numKilled < d->m_garrisonHitKillCount; )
					{
						Object* thingToKill = *it++;
						if (!thingToKill->isEffectivelyDead() && thingToKill->isKindOfMulti(d->m_garrisonHitKillKindof, d->m_garrisonHitKillKindofNot))
						{
							//DEBUG_LOG(("Killed a garrisoned unit (%08lx %s) via Flash-Bang!",thingToKill,thingToKill->getTemplate()->getName().str()));
							if (projectileLauncher)
								projectileLauncher->scoreTheKill( thingToKill );
							thingToKill->kill();
							++numKilled;
						}
					}
				}

				if (numKilled > 0)
				{
					// note, fx is played at center of building, not at grenade's location
					FXList::doFXObj(d->m_garrisonHitKillFX, other, nullptr);

					// don't do the normal explosion; just destroy ourselves & return
					TheGameLogic->destroyObject(obj);

					return true;
				}
			}
		}
	}

	// collided with something... blow'd up!
	detonate();

	// mark ourself as "no collisions" (since we might still exist in slow death mode)
	obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_COLLISIONS ) );
	return true;
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::detonate()
{
	Object* obj = getObject();
	if (m_detonationWeaponTmpl)
	{
		TheWeaponStore->handleProjectileDetonation(m_detonationWeaponTmpl, obj, obj->getPosition(), m_extraBonusFlags);
	}
	else
	{
		// kill it (vs destroying it) so that its Die modules are called
//	obj->kill();
		// do it manually, so we can specify DEATH_DETONATED
		DamageInfo damageInfo;
		damageInfo.in.m_damageType = DAMAGE_UNRESISTABLE;
		damageInfo.in.m_deathType = DEATH_DETONATED;
		damageInfo.in.m_sourceID = INVALID_ID;
		damageInfo.in.m_amount = obj->getBodyModule()->getMaxHealth();
		obj->attemptDamage( &damageInfo );
	}
	if (obj->getDrawable())
		obj->getDrawable()->setDrawableHidden(true);
	// Delay destroying the object two frames to let the contrail catch up. jba.
	switchToState(KILL_SELF);
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::doPrelaunchState()
{
	Locomotor* curLoco = getCurLocomotor();
	if (curLoco)
	{
		curLoco->setMaxAcceleration(0);
		curLoco->setMaxTurnRate(0);
	}
}
//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::doKillSelfState()
{
	if (m_stateTimestamp > TheGameLogic->getFrame()-3) {
		// Hold in this state 2 frames to let the contrail catch up. jba.
		return;
	}
	Object* obj = getObject();
	if (m_detonationWeaponTmpl)	{
		TheGameLogic->destroyObject( obj );
	}
	switchToState(DEAD);
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::doLaunchState()
{
	Locomotor* curLoco = getCurLocomotor();
	if (curLoco)
	{
		curLoco->setMaxAcceleration(0);
		curLoco->setMaxTurnRate(0);
	}

	UnsignedInt delay = getMissileAIUpdateModuleData()->m_ignitionDelay;
	if (TheGameLogic->getFrame() - m_stateTimestamp >= delay)
	{
		switchToState(IGNITION);
	}
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::doIgnitionState()
{
	const MissileAIUpdateModuleData* d = getMissileAIUpdateModuleData();

	Locomotor* curLoco = getCurLocomotor();
	if (curLoco)
	{
		curLoco->setMaxAcceleration(m_maxAccel);
		curLoco->setMaxTurnRate(0);
	}

	FXList::doFXObj(d->m_ignitionFX, getObject());
	if (m_exhaustSysTmpl != nullptr)
	{
		m_exhaustID = TheParticleSystemManager->createAttachedParticleSystemID(m_exhaustSysTmpl, getObject());
	}

	// arm the missile's "warhead"
	m_isArmed = true;
	UnsignedInt now = TheGameLogic->getFrame();
	m_fuelExpirationDate = d->m_fuelLifetime ? (now + d->m_fuelLifetime) : 0x7fffffff;

	switchToState(ATTACK_NOTURN);
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::doAttackState(Bool turnOK)
{
	Locomotor* curLoco = getCurLocomotor();

	const MissileAIUpdateModuleData* d = getMissileAIUpdateModuleData();
	if (TheGameLogic->getFrame() >= m_fuelExpirationDate)
	{
		if( d->m_detonateOnNoFuel )
		{
			detonate();
			return;
		}

		if (curLoco)
		{
			curLoco->setMaxAcceleration(0);
			curLoco->setMaxTurnRate(0);
		}
		tossExhaust();
	}
	else
	{
		if (curLoco)
		{
			curLoco->setMaxAcceleration(m_maxAccel);
			curLoco->setMaxTurnRate(turnOK ? BIGNUM : 0);
		}
	}

	if (d->m_lockDistance > 0)
	{
		Real lockDistanceSquared = d->m_lockDistance;
		Real distanceToTargetSquared;
		if (m_isTrackingTarget && (getGoalObject() != nullptr)) {
			distanceToTargetSquared = ThePartitionManager->getDistanceSquared( getObject(), getGoalObject(), FROM_CENTER_2D);
		}	else {
			distanceToTargetSquared = ThePartitionManager->getDistanceSquared( getObject(), getGoalPosition(), FROM_CENTER_2D );
		}
		if (lockDistanceSquared>0) {
			if (!m_isTrackingTarget) {
				// Immobile or ground target.  Halve the lock distance.
				lockDistanceSquared *= 0.5f;
			}
			lockDistanceSquared *= lockDistanceSquared;
			if (distanceToTargetSquared < lockDistanceSquared) {
				if (!m_isTrackingTarget) {
					// Ground pos.  Change to original goal.
					aiMoveToPosition(&m_originalTargetPos, CMD_FROM_AI );
				}
				switchToState(KILL);
				return;
			}
		}
	}

	if(curLoco && curLoco->getPreferredHeight() > 0)
	{
		// Am I close enough to the target to ignore my preferred height setting?
		Real distanceToTargetSquared = ThePartitionManager->getDistanceSquared( getObject(), getGoalPosition(), FROM_CENTER_2D );
		Real diveDistanceSquared = d->m_diveDistance;
		if (curLoco && curLoco->getPreferredHeight()) {
				diveDistanceSquared *= diveDistanceSquared;
			if( distanceToTargetSquared < diveDistanceSquared )
				curLoco->setUsePreciseZPos( true );
		}

	}

	if (m_noTurnDistLeft <= 0.0f)
	{
		switchToState(ATTACK);
	}

	// If I was fired at a flyer and have lost target (most likely they died), then I need to do something better
	// than cloverleaf around their last spot.
	if( m_isTrackingTarget && (getGoalObject() == nullptr) )
		airborneTargetGone();
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::doKillState()
{
	if (TheGameLogic->getFrame() >= m_fuelExpirationDate)
	{
		if( getMissileAIUpdateModuleData()->m_detonateOnNoFuel )
		{
			detonate();
			return;
		}

		// srj sez: killstate missiles should not be allowed to live forever.
		airborneTargetGone();
		return;
	}

	/* Kill state uses the OBJECT_STATUS_BRAKING, which is a flag to the locomotor to
		"Cheat" and move directly towards the target.  This ensures that the missile will hit.
		jba. */
	Locomotor* curLoco = getCurLocomotor();
	Object *obj = getObject();
	// Objects that are braking don't follow the normal physics, so they end up at their destination exactly.
	obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_BRAKING ) );

	if (curLoco)
	{
		curLoco->setMaxAcceleration(m_maxAccel);
		curLoco->setMaxTurnRate(BIGNUM);
	}
	if (isIdle()) {
		// we finished the move
		if (getGoalObject()!=nullptr) {
			Locomotor* curLoco = getCurLocomotor();
			Real closeEnough = 1.0f;
			if (curLoco)
			{
				closeEnough = curLoco->getMaxSpeedForCondition(BODY_PRISTINE);
			}
			Real distanceToTargetSq = ThePartitionManager->getDistanceSquared( getObject(), getGoalObject(), FROM_CENTER_3D);
			//DEBUG_LOG(("Distance to target %f, closeEnough %f", WWMath::SqrtOrigin(distanceToTargetSq), closeEnough));
			if (distanceToTargetSq < closeEnough*closeEnough) {
				Coord3D pos = *getGoalObject()->getPosition();
				getObject()->setPosition(&pos);
				detonate();
			}	else{
				aiMoveToObject(getGoalObject(), CMD_FROM_AI );
			}
		} else {
			detonate();
		}
	}
	// If I was fired at a flyer and have lost target (most likely they died), then I need to do something better
	// than cloverleaf around their last spot.
	if( m_isTrackingTarget && (getGoalObject() == nullptr) )
		airborneTargetGone();
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::doDeadState()
{
	Locomotor* curLoco = getCurLocomotor();
	if (curLoco)
	{
		curLoco->setMaxAcceleration(0);
		curLoco->setMaxTurnRate(0);
	}
}

//-------------------------------------------------------------------------------------------------
/**
 * Simulate one frame of a missile's behavior
 */
UpdateSleepTime MissileAIUpdate::update()
{
	Coord3D newPos = *getObject()->getPosition();
	if (m_noTurnDistLeft > 0.0f && m_state >= IGNITION)
	{
		Real distThisTurn = WWMath::SqrtfOrigin(sqr(newPos.x-m_prevPos.x) + sqr(newPos.y-m_prevPos.y) + sqr(newPos.z-m_prevPos.z));
		m_noTurnDistLeft -= distThisTurn;
		m_prevPos = newPos;
	}

	if (newPos.z < 0)
	{
		// we ended up under the world.  go away.
		TheGameLogic->destroyObject(getObject());
		return UPDATE_SLEEP_FOREVER;
	}
	switch( m_state )
	{
		case PRELAUNCH:
			doPrelaunchState();
			break;

		case LAUNCH:
			doLaunchState();
			// special case!
			if (m_state == IGNITION)
			{
				// just fall thru
			}
			else
			{
				break;
			}
			FALLTHROUGH;

		case IGNITION:
			doIgnitionState();
			break;

		case ATTACK_NOTURN:
			doAttackState(false);
			break;

		case ATTACK:
			doAttackState(true);
			break;

		case KILL:
			doKillState();
			break;

		case KILL_SELF:
			doKillSelfState();
			break;

		case DEAD:
			doDeadState();
			break;
	}

	/*UpdateSleepTime ret =*/ AIUpdateInterface::update();

#if 1
	// srj sez: doh, why was this never in place? I guess 'cuz so few "smart" missiles ever go in
	// a downward-towards-ground attitude. however, comanche rocket pod missiles do, so this fix
	// is needed for them to target bridges correctly.

	// note that we want to use getHighestLayerForDestination() here, so that anything even slightly
	// below the bridge translates into GROUND. (getLayerForDestination just does a "closest" check)
	PathfindLayerEnum oldLayer = getObject()->getLayer();
	PathfindLayerEnum newLayer = TheTerrainLogic->getHighestLayerForDestination(getObject()->getPosition());
	getObject()->setLayer(newLayer);

	if (projectileIsArmed() && oldLayer != LAYER_GROUND && newLayer == LAYER_GROUND)
	{
		// see if we' still in the bridge's xy area
		Coord3D tmp = *getObject()->getPosition();
		tmp.z = 9999.0f;
		PathfindLayerEnum testLayer = TheTerrainLogic->getHighestLayerForDestination(&tmp);
		if (testLayer == oldLayer)
		{
			// ensure we are slightly above the bridge, to account for fudge & sloppy art
			const Real FUDGE = 2.0f;
			tmp.z = TheTerrainLogic->getLayerHeight(tmp.x, tmp.y, testLayer) + FUDGE;
			getObject()->setPosition(&tmp);
			// blow'd up!
			detonate();
			return UPDATE_SLEEP_NONE;
		}
	}
#endif

	//return (mine < ret) ? mine : ret;
	/// @todo srj -- someday, make sleepy. for now, must not sleep.
	return UPDATE_SLEEP_NONE;
}


//-------------------------------------------------------------------------------------------------
Bool MissileAIUpdate::processCollision(PhysicsBehavior *physics, Object *other)
/* Returns true if the physics collide should apply the force.  Normally not.
Also determines whether objects are blocked, and if so, if they are stuck.  jba.*/
{
	// Missiles dont' get blocked by other objects.
	return false;
}

//-------------------------------------------------------------------------------------------------
void MissileAIUpdate::airborneTargetGone()
{
	// I would really love it if this could retarget, but all of the targeting and legality is done
	// by the Weapon that fired me.  The safest thing for me to do in this state is to just run out of gas.
	m_fuelExpirationDate = TheGameLogic->getFrame();
	switchToState(KILL_SELF);
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void MissileAIUpdate::crc( Xfer *xfer )
{
	// extend base class
	AIUpdateInterface::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void MissileAIUpdate::xfer( Xfer *xfer )
{
  // version
  const XferVersion currentVersion = 4;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);

	if (version>=2) {
		xfer->xferCoord3D(&m_originalTargetPos);
	}

	xfer->xferUser(&m_state, sizeof(m_state));
	xfer->xferUnsignedInt(&m_stateTimestamp);
	xfer->xferUnsignedInt(&m_nextTargetTrackTime);
	xfer->xferObjectID(&m_launcherID);
	xfer->xferObjectID(&m_victimID);
	xfer->xferBool(&m_isArmed);
	xfer->xferUnsignedInt(&m_fuelExpirationDate);
	xfer->xferReal(&m_noTurnDistLeft);
	xfer->xferReal(&m_maxAccel);

	AsciiString weaponName;
	if (m_detonationWeaponTmpl)
	{
		weaponName = m_detonationWeaponTmpl->getName();
	}
	xfer->xferAsciiString(&weaponName);
	if (weaponName.isNotEmpty() && m_detonationWeaponTmpl == nullptr)
	{
		m_detonationWeaponTmpl = TheWeaponStore->findWeaponTemplate(weaponName);
	}

	AsciiString exhaustName;
	if (m_exhaustSysTmpl)
	{
		exhaustName = m_exhaustSysTmpl->getName();
	}
	xfer->xferAsciiString(&exhaustName);
	if (exhaustName.isNotEmpty() && m_exhaustSysTmpl == nullptr)
	{
		m_exhaustSysTmpl = TheParticleSystemManager->findTemplate(exhaustName);
	}

	xfer->xferBool(&m_isTrackingTarget);

	if (version >= 3)
	{
		xfer->xferCoord3D(&m_prevPos);
	}

	if (version >= 4)
	{
		xfer->xferUnsignedInt(&m_extraBonusFlags);
		xfer->xferUser( &m_exhaustID, sizeof( m_exhaustID ) );
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void MissileAIUpdate::loadPostProcess()
{
 // extend base class
	AIUpdateInterface::loadPostProcess();
}
