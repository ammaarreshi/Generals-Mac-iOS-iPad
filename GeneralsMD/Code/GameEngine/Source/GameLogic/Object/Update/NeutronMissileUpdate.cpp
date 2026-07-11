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

// FILE: NeutronMissileUpdate.cpp
// Author: Michael S. Booth, December 2001
// Desc:   Implementation of missile behavior

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameState.h"
#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"

#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/NeutronMissileUpdate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"

#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ParticleSys.h"

static const Real STRAIGHT_DOWN_SLOW_FACTOR = 0.5f;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
NeutronMissileUpdateModuleData::NeutronMissileUpdateModuleData()
{
	m_initialDist = 0.0f;
	m_maxTurnRate = 999.0f;
	m_forwardDamping = 0;
	m_relativeSpeed = 1.0f;
	m_targetFromDirectlyAbove = 0.0f;
	m_ignitionFX = nullptr;
	m_launchFX = nullptr;
	m_specialAccelFactor = 1.0f;
	m_specialSpeedTime = 0;
	m_specialSpeedHeight = 0.0f;
	m_specialJitterDistance = 0.0f;
	m_deliveryDecalRadius = 0;
}

//-----------------------------------------------------------------------------
void NeutronMissileUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "DistanceToTravelBeforeTurning",	INI::parseReal,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_initialDist ) },
		{ "MaxTurnRate",			INI::parseAngularVelocityReal,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_maxTurnRate ) },
		{ "ForwardDamping",		INI::parseReal,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_forwardDamping ) },
		{ "RelativeSpeed",		INI::parseReal,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_relativeSpeed ) },
		{ "TargetFromDirectlyAbove",		INI::parseReal,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_targetFromDirectlyAbove ) },
		{ "LaunchFX",					INI::parseFXList,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_launchFX ) },
		{ "SpecialSpeedTime",	INI::parseDurationUnsignedInt,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_specialSpeedTime ) },
		{ "SpecialSpeedHeight",	INI::parseReal,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_specialSpeedHeight ) },
		{ "SpecialAccelFactor",	INI::parseReal,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_specialAccelFactor ) },
		{ "SpecialJitterDistance",	INI::parseReal,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_specialJitterDistance ) },
		{ "IgnitionFX",				INI::parseFXList,		nullptr, offsetof( NeutronMissileUpdateModuleData, m_ignitionFX ) },
		{ "DeliveryDecal",						RadiusDecalTemplate::parseRadiusDecalTemplate,	nullptr, offsetof( NeutronMissileUpdateModuleData, m_deliveryDecalTemplate ) },
		{ "DeliveryDecalRadius",			INI::parseReal,									nullptr,	offsetof( NeutronMissileUpdateModuleData, m_deliveryDecalRadius ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
NeutronMissileUpdate::NeutronMissileUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	const NeutronMissileUpdateModuleData* d = getNeutronMissileUpdateModuleData();

	m_noTurnDistLeft = d->m_initialDist;
	m_reachedIntermediatePos = true;

	m_targetPos.zero();
	m_intermedPos.zero();
	m_accel.zero();
	m_vel.zero();

	m_state = PRELAUNCH;
	m_stateTimestamp = TheGameLogic->getFrame();
	m_isArmed = false;
	m_isLaunched = false;
	m_launcherID = INVALID_ID;
	m_attach_wslot = PRIMARY_WEAPON;
	m_attach_specificBarrelToUse = 0;
	m_heightAtLaunch = 0;
	m_frameAtLaunch = 0;

	m_exhaustSysTmpl = nullptr;

}

//-------------------------------------------------------------------------------------------------
NeutronMissileUpdate::~NeutronMissileUpdate()
{
	m_deliveryDecal.clear();
}

//-------------------------------------------------------------------------------------------------
void NeutronMissileUpdate::onDelete()
{
}

//-------------------------------------------------------------------------------------------------
// Prepares the missile for launch via proper weapon-system channels.
//-------------------------------------------------------------------------------------------------
void NeutronMissileUpdate::projectileLaunchAtObjectOrPosition(const Object *victim, const Coord3D* victimPos, const Object *launcher, WeaponSlotType wslot, Int specificBarrelToUse, const WeaponTemplate* detWeap, const ParticleSystemTemplate* exhaustSysOverride)
{
	DEBUG_ASSERTCRASH(specificBarrelToUse>=0, ("specificBarrelToUse must now be explicit"));

	m_launcherID = launcher ? launcher->getID() : INVALID_ID;
	m_attach_wslot = wslot;
	m_attach_specificBarrelToUse = specificBarrelToUse;

	m_vel.zero();
	if (launcher)
	{
		const PhysicsBehavior *phys = launcher->getPhysics();
		if (phys)
			m_vel = *phys->getVelocity();
	}

	projectileFireAtObjectOrPosition( victim, victimPos, detWeap, exhaustSysOverride );
}

//-------------------------------------------------------------------------------------------------
// The actual firing of the missile once setup.
//-------------------------------------------------------------------------------------------------
void NeutronMissileUpdate::projectileFireAtObjectOrPosition( const Object *victim, const Coord3D *victimPos, const WeaponTemplate *detWeap, const ParticleSystemTemplate* exhaustSysOverride )
{
	m_exhaustSysTmpl = exhaustSysOverride;

	m_state = LAUNCH;
	m_stateTimestamp = TheGameLogic->getFrame();

	if( victim )
	{
		// CalcTarget will add half the target's height.  But in this case, we are aiming at the ground
		// and need to stay aiming at the ground.
		m_targetPos = *victim->getPosition();
		m_intermedPos = m_targetPos;
		m_intermedPos.z += getNeutronMissileUpdateModuleData()->m_targetFromDirectlyAbove;
	}
	else
	{
		// Otherwise, we are just a Coord shot.
		m_targetPos = *victimPos;
		m_intermedPos = m_targetPos;
		m_intermedPos.z += getNeutronMissileUpdateModuleData()->m_targetFromDirectlyAbove;
	}

	m_deliveryDecal.clear();
	getNeutronMissileUpdateModuleData()->m_deliveryDecalTemplate.createRadiusDecal(m_targetPos,
		getNeutronMissileUpdateModuleData()->m_deliveryDecalRadius, getObject()->getControllingPlayer(), m_deliveryDecal);

}

//-------------------------------------------------------------------------------------------------
/**
 * Implement LAUNCH state
 */
void NeutronMissileUpdate::doLaunch()
{
	if (!m_isLaunched)
	{
		Object *launcher = TheGameLogic->findObjectByID( m_launcherID );

		// if our launch vehicle is gone, destroy ourselves
		if (launcher == nullptr)
		{
			m_launcherID = INVALID_ID;
			TheGameLogic->destroyObject( getObject() );
			return;
		}

		Matrix3D attachTransform;
		if (!launcher->getDrawable() ||
			!launcher->getDrawable()->getProjectileLaunchOffset(m_attach_wslot, m_attach_specificBarrelToUse, &attachTransform, TURRET_INVALID, nullptr))
		{
			DEBUG_CRASH(("ProjectileLaunchPos %d %d not found!",m_attach_wslot, m_attach_specificBarrelToUse));
			attachTransform.Make_Identity();
		}

		Matrix3D worldTransform;
		launcher->convertBonePosToWorldPos(nullptr, &attachTransform, nullptr, &worldTransform);

		Vector3 tmp = worldTransform.Get_Translation();
		Coord3D worldPos;
		worldPos.x = tmp.X;
		worldPos.y = tmp.Y;
		worldPos.z = tmp.Z;

		//
		// the missile on the raising up launch platform is actually 45 degrees from the missile
		// that is flying around the world ... we want to rotate it "on end and in place" so
		// that we don't see any decals on the side of the missile 'pop' to the new angle
		/// @todo, this should not be a hard coded value ... I love demos!!!
		//
		worldTransform.Rotate_X( (PI / 2.0f) );

		getObject()->getDrawable()->setDrawableHidden(false);
		getObject()->setTransformMatrix(&worldTransform);
		getObject()->setPosition(&worldPos);

		getObject()->getExperienceTracker()->setExperienceSink( m_launcherID );

		m_isLaunched = true;

		if (getNeutronMissileUpdateModuleData()->m_targetFromDirectlyAbove)
			m_reachedIntermediatePos = false;

		FXList::doFXObj(getNeutronMissileUpdateModuleData()->m_launchFX, getObject());
		m_heightAtLaunch = getObject()->getPosition()->z;
		m_frameAtLaunch = TheGameLogic->getFrame();

	}

	// fall
	Coord3D pos = *getObject()->getPosition();

	pos.x += m_vel.x;
	pos.y += m_vel.y;
	pos.z += m_vel.z;

	getObject()->setPosition( &pos );

	FXList::doFXObj(getNeutronMissileUpdateModuleData()->m_ignitionFX, getObject());

	if (m_exhaustSysTmpl != nullptr)
		TheParticleSystemManager->createAttachedParticleSystemID(m_exhaustSysTmpl, getObject());

	m_state = ATTACK;
	m_stateTimestamp = TheGameLogic->getFrame();

	// arm the missile's "warhead"
	m_isArmed = true;
}

//-----------------------------------------------------------------------------
// return the angle (in 3-space) we actually turned.
static Real calcTransform(const Object* obj, const Coord3D *pos, Real maxTurnRate, Matrix3D* newTransform )
{
	// convert to Vector3, to use all its handy stuff
	Vector3 objPos(obj->getPosition()->x, obj->getPosition()->y, obj->getPosition()->z);
	Vector3 otherPos(pos->x, pos->y, pos->z);

	Vector3 objDir = obj->getTransformMatrix()->Rotate_Vector(Vector3(1.0f, 0.0f, 0.0f));
	Vector3 otherDir = otherPos - objPos;
	otherDir.Normalize();

	// dot of two unit vectors is cos of angle between them
	Real c = Vector3::Dot_Product(objDir, otherDir);
	// bound it in case of numerical error
	if (c < -1.0)
		c = -1.0;
	else if (c > 1.0)
		c = 1.0;

	Real angle = (Real)ACos( c );

	Vector3 newDir;
	if (fabs(angle) < maxTurnRate)
	{
		// close enough -- point exactly in the right dir
		newDir = otherDir;
	}
	else
	{
		// nah, turn as much as we can... this is icky, 'cuz
		// we need to rotate around the axis perpendicular to these two vecs
		angle = maxTurnRate;

		// cross of two vectors is the perpendicular axis
#ifdef ALLOW_TEMPORARIES
		Vector3 objCrossOther = Vector3::Cross_Product(objDir, otherDir);
		objCrossOther.Normalize();
#else
		Vector3 objCrossOther;
		Vector3::Normalized_Cross_Product(objDir, otherDir, &objCrossOther);
#endif

		Matrix3D rotMtx(objCrossOther, angle);
		newDir = rotMtx.Rotate_Vector(objDir);

	}

	newTransform->buildTransformMatrix( objPos, newDir );

	return angle;
}

//-------------------------------------------------------------------------------------------------
/**
 * Implement ATTACK state
 */
void NeutronMissileUpdate::doAttack()
{
	Matrix3D mx;
	Real speed = getNeutronMissileUpdateModuleData()->m_relativeSpeed;

	if (getNeutronMissileUpdateModuleData()->m_targetFromDirectlyAbove && m_reachedIntermediatePos)
		speed *= STRAIGHT_DOWN_SLOW_FACTOR;

	// if we're still in the no-turning-time, OR if we're out of fuel
	if (m_noTurnDistLeft > 0.0f)
	{
		mx = *getObject()->getTransformMatrix();
	}
	else
	{
		{
			//
			// Orient toward destination
			//
			Real relAngle = calcTransform(getObject(), m_reachedIntermediatePos ? &m_targetPos : &m_intermedPos, getNeutronMissileUpdateModuleData()->m_maxTurnRate, &mx);

			//
			// Modulate speed according to turning. The more we have to turn, the slower we go
			//
			Real angleCoeff = (Real)fabs( relAngle ) / (PI / 2.0f);
			if (angleCoeff > 1.0f)
				angleCoeff = 1.0;
		}
	}

	// get true forward direction of missile
	Vector3 trueDir = mx.Get_X_Vector();
	trueDir.Normalize();

	//
	// Move forward along forward direction
	//
	Real damping = getNeutronMissileUpdateModuleData()->m_forwardDamping;
	m_accel.x = speed * trueDir.X - damping * m_vel.x;
	m_accel.y = speed * trueDir.Y - damping * m_vel.y;
	m_accel.z = speed * trueDir.Z - damping * m_vel.z;

	m_vel.x += m_accel.x;
	m_vel.y += m_accel.y;
	m_vel.z += m_accel.z;

	Coord3D pos = *getObject()->getPosition();

	const NeutronMissileUpdateModuleData* d = getNeutronMissileUpdateModuleData();
	UnsignedInt now = TheGameLogic->getFrame();
	if (d->m_specialSpeedTime > 0 && now <= m_frameAtLaunch + d->m_specialSpeedTime)
	{
		getObject()->getDrawable()->setInstanceMatrix(nullptr);
		UnsignedInt elapsed = now - m_frameAtLaunch;
		if (elapsed < d->m_specialSpeedTime)
		{
			Real timeFrac = (Real)elapsed / (Real)d->m_specialSpeedTime;
			Real accelFactor = d->m_specialAccelFactor;
			if (accelFactor < 0.01f) accelFactor = 0.01f;
			Coord3D newPos = pos;
			newPos.z = m_heightAtLaunch + (sqr(accelFactor * timeFrac) / accelFactor) * d->m_specialSpeedHeight;

			m_vel.x = newPos.x - pos.x;
			m_vel.y = newPos.y - pos.y;
			m_vel.z = newPos.z - pos.z;

			if (d->m_specialJitterDistance > 0.0f)
			{
				Real amplitude = (1.0f - timeFrac) * d->m_specialJitterDistance;
				Vector3 vectmp;
				vectmp.X = 0;
				// MDC: moving to GameLogicRandomValue.  This does not need to be synced, but having it so makes searches *so* much nicer.
				vectmp.Y = GameLogicRandomValueReal(-1.0, 1.0) * amplitude;
				vectmp.Z = GameLogicRandomValueReal(-1.0, 1.0) * amplitude;
				vectmp = mx.Rotate_Vector(vectmp);
				Matrix3D mtxtmp(1);
				mtxtmp.Translate(vectmp);
				getObject()->getDrawable()->setInstanceMatrix( &mtxtmp );
			}
		}
	}

	pos.x += m_vel.x;
	pos.y += m_vel.y;
	pos.z += m_vel.z;

//DEBUG_LOG(("vel %f accel %f z %f",m_vel.length(),m_accel.length(), pos.z));
//Real vm = WWMath::SqrtOrigin(m_vel.x*m_vel.x+m_vel.y*m_vel.y+m_vel.z*m_vel.z);
//DEBUG_LOG(("vel is %f %f %f (%f)",m_vel.x,m_vel.y,m_vel.z,vm));
	getObject()->setTransformMatrix( &mx );
	getObject()->setPosition( &pos );

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool NeutronMissileUpdate::projectileHandleCollision( Object *other )
{
	Object* obj = getObject();

	// check if our warhead is "armed" - if not, we are inert
	if (projectileIsArmed() == false)
		return true;

	// Don't hit your own launcher, ever.
	if (other != nullptr && projectileGetLauncherID() == other->getID())
		return true;

	// collided with something... blow'd up!
	detonate();

	// mark ourself as "no collisions" (since we might still exist in slow death mode)
	obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_COLLISIONS ) );
	return true;
}

//-------------------------------------------------------------------------------------------------
void NeutronMissileUpdate::onDie( const DamageInfo *damageInfo )
{
	m_deliveryDecal.clear();
}

//-------------------------------------------------------------------------------------------------
void NeutronMissileUpdate::detonate()
{
	m_deliveryDecal.clear();
	Object* obj = getObject();
	// kill it (vs destroying it) so that its Die modules are called
	obj->kill();
	m_state = DEAD;
	if (obj->getDrawable())
		obj->getDrawable()->setDrawableHidden(true);
}

//-------------------------------------------------------------------------------------------------
/**
 * Simulate one frame of a missile's behavior
 */
UpdateSleepTime NeutronMissileUpdate::update()
{
	m_deliveryDecal.update();

	if (!m_reachedIntermediatePos)
	{
		Real distSqr = ThePartitionManager->getDistanceSquared(getObject(), &m_intermedPos, FROM_CENTER_3D);
		Real boundSqr = sqr(getObject()->getGeometryInfo().getBoundingSphereRadius());
		if (distSqr <= boundSqr)
		{
			m_reachedIntermediatePos = true;
			getObject()->setPosition(&m_intermedPos);
			Real vel = m_vel.length();
			m_vel.x = 0;
			m_vel.y = 0;
			m_vel.z = -vel * STRAIGHT_DOWN_SLOW_FACTOR;

		}
	}

	Coord3D oldPos = *getObject()->getPosition();
	Bool oldPosValid = (m_state == ATTACK);	// not valid till *after* we've launched
	switch( m_state )
	{
		case PRELAUNCH:
			// nothing... just ignore it.
			break;

		case LAUNCH:
			doLaunch();
			break;

		case ATTACK:
			doAttack();
			break;

		case DEAD:
			// do nothing
			break;
	}
	if (m_noTurnDistLeft > 0.0f && oldPosValid)
	{
		Coord3D newPos = *getObject()->getPosition();
		Real distThisTurn = WWMath::SqrtOrigin(sqr(newPos.x-oldPos.x) + sqr(newPos.y-oldPos.y) + sqr(newPos.z-oldPos.z));
		//DEBUG_LOG(("noTurnDist goes from %f to %f",m_noTurnDistLeft,m_noTurnDistLeft-distThisTurn));
		m_noTurnDistLeft -= distThisTurn;
	}

	if (m_state != PRELAUNCH && m_state != DEAD && !getObject()->isAboveTerrain())
	{
		// the normal always points straight down, though we could
		// get the alignWithTerrain() normal if it proves interesting
		Coord3D normal;
		normal.x = normal.y = 0.0f;
		normal.z = -1.0f;
		getObject()->onCollide(nullptr, getObject()->getPosition(), &normal);
	}
	return UPDATE_SLEEP_NONE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void NeutronMissileUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void NeutronMissileUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// state
	xfer->xferUser( &m_state, sizeof( MissileStateType ) );

	// target pos
	xfer->xferCoord3D( &m_targetPos );

	// intermed pos
	xfer->xferCoord3D( &m_intermedPos );

	// launcher ID
	xfer->xferObjectID( &m_launcherID );

	// attach weapon slot
	xfer->xferUser( &m_attach_wslot, sizeof( WeaponSlotType ) );

	// attach specific barrel
	xfer->xferInt( &m_attach_specificBarrelToUse );

	// accel
	xfer->xferCoord3D( &m_accel );

	// vel
	xfer->xferCoord3D( &m_vel );

	// state timestamp
	xfer->xferUnsignedInt( &m_stateTimestamp );

	// is launched
	xfer->xferBool( &m_isLaunched );

	// is armed
	xfer->xferBool( &m_isArmed );

	// no turn dist left
	xfer->xferReal( &m_noTurnDistLeft );

	// raeched intermediate pos
	xfer->xferBool( &m_reachedIntermediatePos );

	// farme at launch
	xfer->xferUnsignedInt( &m_frameAtLaunch );

	// height at launch
	xfer->xferReal( &m_heightAtLaunch );

	// decal, if any
	m_deliveryDecal.xferRadiusDecal(xfer);

	// particle system template
	AsciiString name = m_exhaustSysTmpl ? m_exhaustSysTmpl->getName() : AsciiString::TheEmptyString;
	xfer->xferAsciiString( &name );
	if( xfer->getXferMode() == XFER_LOAD )
	{

		// make system template nullptr to be safe
		m_exhaustSysTmpl = nullptr;
		if( name.isEmpty() == FALSE )
		{

			m_exhaustSysTmpl = TheParticleSystemManager->findTemplate( name );
			if( m_exhaustSysTmpl == nullptr )
			{

				DEBUG_CRASH(( "NeutronMissileUpdate::xfer - Unable to find particle system '%s'", name.str() ));
				throw SC_INVALID_DATA;

			}

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void NeutronMissileUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
