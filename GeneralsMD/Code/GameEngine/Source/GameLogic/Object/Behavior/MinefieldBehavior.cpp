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

// FILE: MinefieldBehavior.cpp //////////////////////////////////////////////////////////////////
// Author: Steven Johnson, June 2002
// Desc:   Minefield behavior
///////////////////////////////////////////////////////////////////////////////////////////////////


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#define DEFINE_RELATIONSHIP_NAMES
#include "Common/GameState.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/MinefieldBehavior.h"
#include "GameLogic/Module/AutoHealBehavior.h"
#include "GameLogic/Weapon.h"


// detonation never puts our health below this, since we probably auto-regen
const Real MIN_HEALTH = 0.1f;

//-------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
MinefieldBehaviorModuleData::MinefieldBehaviorModuleData()
{
	m_detonationWeapon = nullptr;
	m_detonatedBy = (1 << ENEMIES) | (1 << NEUTRAL);
	m_stopsRegenAfterCreatorDies = true;
	m_regenerates = false;
	m_workersDetonate = false;
	m_creatorDeathCheckRate = static_cast<float>(LOGICFRAMES_PER_SECOND);
	m_scootFromStartingPointTime = 0;
	m_repeatDetonateMoveThresh = 1.0f;
	m_numVirtualMines = 1;
	m_healthPercentToDrainPerSecond = 0.0f;
	m_ocl = nullptr;
}

//-------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void MinefieldBehaviorModuleData::buildFieldParse( MultiIniFieldParse &p )
{

  UpdateModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "DetonationWeapon", INI::parseWeaponTemplate,	nullptr, offsetof( MinefieldBehaviorModuleData, m_detonationWeapon ) },
		{ "DetonatedBy", INI::parseBitString32, TheRelationshipNames, offsetof( MinefieldBehaviorModuleData, m_detonatedBy ) },
		{ "StopsRegenAfterCreatorDies", INI::parseBool, nullptr, offsetof( MinefieldBehaviorModuleData, m_stopsRegenAfterCreatorDies ) },
		{ "Regenerates", INI::parseBool, nullptr, offsetof( MinefieldBehaviorModuleData, m_regenerates ) },
		{ "WorkersDetonate", INI::parseBool, nullptr, offsetof( MinefieldBehaviorModuleData, m_workersDetonate ) },
		{ "CreatorDeathCheckRate", INI::parseDurationUnsignedInt, nullptr, offsetof( MinefieldBehaviorModuleData, m_creatorDeathCheckRate ) },
		{ "ScootFromStartingPointTime", INI::parseDurationUnsignedInt, nullptr, offsetof( MinefieldBehaviorModuleData, m_scootFromStartingPointTime ) },
		{ "NumVirtualMines", INI::parseUnsignedInt, nullptr, offsetof( MinefieldBehaviorModuleData, m_numVirtualMines ) },
		{ "RepeatDetonateMoveThresh", INI::parseReal, nullptr, offsetof( MinefieldBehaviorModuleData, m_repeatDetonateMoveThresh ) },
		{ "DegenPercentPerSecondAfterCreatorDies", INI::parsePercentToReal,	nullptr, offsetof( MinefieldBehaviorModuleData, m_healthPercentToDrainPerSecond ) },
		{ "CreationList",	INI::parseObjectCreationList,	nullptr,	offsetof( MinefieldBehaviorModuleData, m_ocl ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  p.add( dataFieldParse );

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
MinefieldBehavior::MinefieldBehavior( Thing *thing, const ModuleData* moduleData )
								 : UpdateModule( thing, moduleData )
{
	const MinefieldBehaviorModuleData* d = getMinefieldBehaviorModuleData();
	m_nextDeathCheckFrame = 0;
	m_scootFramesLeft = 0;
	m_scootVel.zero();
	m_scootAccel.zero();
	m_detonators.clear();
	m_ignoreDamage = false;
	m_regenerates = d->m_regenerates;
	m_draining = false;
	m_virtualMinesRemaining = d->m_numVirtualMines;
	for (Int i = 0; i < MAX_IMMUNITY; ++i)
	{
		m_immunes[i].id = INVALID_ID;
		m_immunes[i].collideTime = 0;
	}

	// start off awake, and we will calcSleepTime from here on
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );

	// mines aren't auto-acquirable
	getObject()->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_ATTACK_FROM_AI ) );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
MinefieldBehavior::~MinefieldBehavior()
{
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime MinefieldBehavior::calcSleepTime()
{
	const MinefieldBehaviorModuleData* d = getMinefieldBehaviorModuleData();

	// if we're draining we have to update every frame
	if (m_draining)
		return UPDATE_SLEEP_NONE;

	// if we're scooting we need to update every frame
	if( m_scootFramesLeft > 0 )
		return UPDATE_SLEEP_NONE;

	// if there is anybody in our immulity monitoring we need to update every frame
	for( Int i = 0; i < MAX_IMMUNITY; ++i )
		if( m_immunes[ i ].id != INVALID_ID )
			return UPDATE_SLEEP_NONE;

	UnsignedInt sleepTime = FOREVER;
	UnsignedInt now = TheGameLogic->getFrame();
	//
	// sleep until the next death check frame we already have figured outif we care
	// about it (that is, when our creator dies)
	//
	if (m_regenerates && d->m_stopsRegenAfterCreatorDies)
		sleepTime = min( sleepTime, m_nextDeathCheckFrame - now );

	// if we don't want to sleep forever, prevent 0 frame sleeps
	if( sleepTime == 0 )
		sleepTime = 1;

	// sleep forever
	return UPDATE_SLEEP( sleepTime );

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime MinefieldBehavior::update()
{
	Object* obj = getObject();
	const MinefieldBehaviorModuleData* d = getMinefieldBehaviorModuleData();
	UnsignedInt now = TheGameLogic->getFrame();

	if (m_scootFramesLeft > 0)
	{
		Coord3D pt = *obj->getPosition();

		m_scootVel.x += m_scootAccel.x;
		m_scootVel.y += m_scootAccel.y;
		m_scootVel.z += m_scootAccel.z;

		pt.x += m_scootVel.x;
		pt.y += m_scootVel.y;
		pt.z += m_scootVel.z;

		// srj sez: scooting mines always go on the highest layer.
		Coord3D tmp = pt;
		tmp.z = 99999.0f;
		PathfindLayerEnum newLayer = TheTerrainLogic->getHighestLayerForDestination(&tmp);
		obj->setLayer(newLayer);

		Real ground = TheTerrainLogic->getLayerHeight( pt.x, pt.y, newLayer );

		if (newLayer != LAYER_GROUND)
		{
			// ensure we are slightly above the bridge, to account for fudge & sloppy art
			const Real FUDGE = 1.0f;
			ground += FUDGE;
		}

		if (pt.z < ground || m_scootFramesLeft <= 1)
			pt.z = ground;

		obj->setPosition(&pt);

		--m_scootFramesLeft;
	}

	// check for expired immunities.
	for (Int i = 0; i < MAX_IMMUNITY; ++i)
	{
		if (m_immunes[i].id == INVALID_ID)
			continue;

		if (TheGameLogic->findObjectByID(m_immunes[i].id) == nullptr ||
				now > m_immunes[i].collideTime + 2)
		{
			//DEBUG_LOG(("expiring an immunity %d",m_immunes[i].id));
			m_immunes[i].id = INVALID_ID;	// he's dead, jim.
			m_immunes[i].collideTime = 0;
		}
	}

	if (now >= m_nextDeathCheckFrame)
	{

		// check to see if there is an enemy building on me... since enemy buildings can be build on top of me

		// check to see if the building that made me is gone, and whether therefore, I should go
		if (m_regenerates && d->m_stopsRegenAfterCreatorDies)
		{
			m_nextDeathCheckFrame = now + d->m_creatorDeathCheckRate;
			ObjectID producerID = getObject()->getProducerID();
			if (producerID != INVALID_ID)
			{
				Object* producer = TheGameLogic->findObjectByID(producerID);
				if (producer == nullptr || producer->isEffectivelyDead())
				{
					m_regenerates = false;
					m_draining = true;
					static const NameKeyType key_AutoHealBehavior = NAMEKEY("AutoHealBehavior");
					AutoHealBehavior* ahb = (AutoHealBehavior*)obj->findUpdateModule( key_AutoHealBehavior );
					if (ahb)
						ahb->stopHealing();
				}
			}
		}
	}

	if (m_draining)
	{
		DamageInfo damageInfo;
		damageInfo.in.m_amount = (obj->getBodyModule()->getMaxHealth() * d->m_healthPercentToDrainPerSecond) / static_cast<float>(LOGICFRAMES_PER_SECOND);
		damageInfo.in.m_sourceID = obj->getID();
		damageInfo.in.m_damageType = DAMAGE_UNRESISTABLE;
		damageInfo.in.m_deathType = DEATH_NORMAL;
		obj->attemptDamage( &damageInfo );
	}

	return calcSleepTime();
}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void MinefieldBehavior::detonateOnce(const Coord3D& position)
{
	const MinefieldBehaviorModuleData* d = getMinefieldBehaviorModuleData();
	if (d->m_detonationWeapon)
	{
		Object* obj = getObject();
	  TheWeaponStore->createAndFireTempWeapon(d->m_detonationWeapon, obj, &position);
	}

	if (m_virtualMinesRemaining > 0)
		--m_virtualMinesRemaining;

	if (!m_regenerates && m_virtualMinesRemaining == 0)
	{
		TheGameLogic->destroyObject(getObject());
	}
	else
	{
		Real percent = (Real)m_virtualMinesRemaining / (Real)d->m_numVirtualMines;
		BodyModuleInterface* body = getObject()->getBodyModule();
		Real health = body->getHealth();
		Real desired = percent * body->getMaxHealth();
		if (desired < MIN_HEALTH)
			desired = MIN_HEALTH;
		Real amount = health - desired;

		if (amount > 0.0f)
		{
			m_ignoreDamage = true;

			//body->internalChangeHealth(desired - health);
			//can't use this, AutoHeal won't work unless we go thru normal damage stuff

			DamageInfo extraDamageInfo;
			extraDamageInfo.in.m_damageType = DAMAGE_UNRESISTABLE;
			extraDamageInfo.in.m_deathType = DEATH_NONE;
			extraDamageInfo.in.m_sourceID = getObject()->getID();
			extraDamageInfo.in.m_amount = amount;
			getObject()->attemptDamage(&extraDamageInfo);

			m_ignoreDamage = false;
		}
	}

	if (m_virtualMinesRemaining == 0)
	{
		getObject()->setModelConditionState(MODELCONDITION_RUBBLE);
		getObject()->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_MASKED ) );
	}
	else
	{
		getObject()->clearModelConditionState(MODELCONDITION_RUBBLE);
		getObject()->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_MASKED ) );
	}


	if (d->m_ocl)
	{
		ObjectCreationList::create(d->m_ocl, getObject(), getObject());
	}
}

//-----------------------------------------------------------------------------
static Real calcDistSquared(const Coord3D& a, const Coord3D& b)
{
	return sqr(a.x - b.x) + sqr(a.y - b.y) + sqr(a.z - b.z);
}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void MinefieldBehavior::onCollide( Object *other, const Coord3D *loc, const Coord3D *normal )
{
	if (other == nullptr || other->isEffectivelyDead())
		return;

	if (m_virtualMinesRemaining == 0)
		return;

	Object* obj = getObject();
	const MinefieldBehaviorModuleData* d = getMinefieldBehaviorModuleData();
	UnsignedInt now = TheGameLogic->getFrame();

	// is this guy in our immune list?
	// NOTE NOTE NOTE, must always do this check FIRST so that 'collideTime' is updated...
	for (Int i = 0; i < MAX_IMMUNITY; ++i)
	{
		if (m_immunes[i].id == other->getID())
		{
			//DEBUG_LOG(("ignoring due to immunity %d",m_immunes[i].id));
			m_immunes[i].collideTime = now;
			return;
		}
	}

	if (!d->m_workersDetonate)
	{
		// infantry+dozer=worker.
		if (other->isKindOf(KINDOF_INFANTRY) && other->isKindOf(KINDOF_DOZER))
			return;
	}

	Int requiredMask = 0;
	Relationship r = obj->getRelationship(other);
	if (r == ALLIES) requiredMask = (1 << ALLIES);
	else if (r == ENEMIES) requiredMask = (1 << ENEMIES);
	else if (r == NEUTRAL) requiredMask = (1 << NEUTRAL);
	if ((d->m_detonatedBy & requiredMask) == 0)
		return;

	// are we active?
	if (m_scootFramesLeft > 0)
		return;

	// things that are in the process of clearing mines are immune to mine detonation,
	// even if we aren't the specific mine they are trying to clear. (however, they must
	// have a real mine they area trying to clear... it's possible they could be trying to
	// clear a position where there is no mine, in which case we grant them no immunity, muwahahaha)
	AIUpdateInterface* otherAI = other->getAI();
	if (otherAI && otherAI->isClearingMines() && otherAI->getGoalObject() != nullptr)
	{
		// mine-clearers are granted immunity to us for as long as they continuously
		// collide, even if no longer clearing mines. (this prevents the problem
		// of a guy who touches two close-together mines while clearing, then puts up his
		// detector and is blown to smithereens by the other one.)
		for (Int i = 0; i < MAX_IMMUNITY; ++i)
		{
			if (m_immunes[i].id == INVALID_ID || m_immunes[i].id == other->getID())
			{
				//DEBUG_LOG(("add/update immunity %d",m_immunes[i].id));
				m_immunes[i].id = other->getID();
				m_immunes[i].collideTime = now;

				// wake up
				setWakeFrame( obj, calcSleepTime() );

				break;
			}
		}
		return;
	}

	// if we detonated another one nearby, we have to move a little bit to detonate another one.
	Bool found = false;
	for (std::vector<DetonatorInfo>::iterator it = m_detonators.begin(); it != m_detonators.end(); ++it)
	{
		if (other->getID() == it->id)
		{
			found = TRUE;
			Real distSqr = calcDistSquared(*other->getPosition(), it->where);
			if (distSqr <= sqr(d->m_repeatDetonateMoveThresh))
			{
				// too close. punt for now.
				return;
			}
			else
			{
				// far enough. update the loc, then break out and blow up.
				it->where = *other->getPosition();
				break;
			}
		}
	}
	if (!found)
	{
		// add him to the list.
		DetonatorInfo detInfo;
		detInfo.id = other->getID();
		detInfo.where = *other->getPosition();
		m_detonators.push_back(detInfo);
	}

	Coord3D detPt = *other->getPosition();
	obj->getGeometryInfo().clipPointToFootprint(*obj->getPosition(), detPt);
	detonateOnce(detPt);
}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void MinefieldBehavior::onDamage( DamageInfo *damageInfo )
{
	if (m_ignoreDamage)
		return;

	const MinefieldBehaviorModuleData* d = getMinefieldBehaviorModuleData();

	// detonate as many times as necessary for our virtual mine count to match our health
	BodyModuleInterface* body = getObject()->getBodyModule();

	for (;;)
	{
		Real virtualMinesExpectedF = ((Real)d->m_numVirtualMines * body->getHealth() / body->getMaxHealth());
		Int virtualMinesExpected =
			damageInfo->in.m_damageType == DAMAGE_HEALING ?
			REAL_TO_INT_FLOOR(virtualMinesExpectedF) :
			REAL_TO_INT_CEIL(virtualMinesExpectedF);
		if (virtualMinesExpected > d->m_numVirtualMines)
			virtualMinesExpected = d->m_numVirtualMines;
		if (m_virtualMinesRemaining < virtualMinesExpected)
		{
			m_virtualMinesRemaining = virtualMinesExpected;
		}
		else if (m_virtualMinesRemaining > virtualMinesExpected)
		{
			if (m_draining &&
						damageInfo->in.m_sourceID == getObject()->getID() &&
						damageInfo->in.m_damageType == DAMAGE_UNRESISTABLE)
			{
				// don't detonate.... just ditch a mine
				--m_virtualMinesRemaining;
			}
			else
			{
				detonateOnce(*getObject()->getPosition());
			}
		}
		else
		{
			break;
		}
	}

	if (m_virtualMinesRemaining == 0)
	{
		// oops, if someone did weapon damage they may have nuked our health to zero,
		// which would be bad if we regen. prevent this. (srj)
		if (m_regenerates && body->getHealth() < MIN_HEALTH)
		{
			body->internalChangeHealth(MIN_HEALTH - body->getHealth());
		}

		getObject()->setModelConditionState(MODELCONDITION_RUBBLE);
		getObject()->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_MASKED ) );
	}
	else
	{
		getObject()->clearModelConditionState(MODELCONDITION_RUBBLE);
		getObject()->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_MASKED ) );
	}
}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void MinefieldBehavior::onHealing( DamageInfo *damageInfo )
{
	onDamage(damageInfo);
}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void MinefieldBehavior::onDie( const DamageInfo *damageInfo )
{
	TheGameLogic->destroyObject(getObject());
}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void MinefieldBehavior::disarm()
{
	if (!m_regenerates)
	{
		TheGameLogic->destroyObject(getObject());
		return;
	}

	// detonation never puts our health below this, since we probably auto-regen
	const Real MIN_HEALTH = 0.1f;

	BodyModuleInterface* body = getObject()->getBodyModule();
	Real desired = MIN_HEALTH;
	Real amount = body->getHealth() - desired;

	m_ignoreDamage = true;

	//body->internalChangeHealth(desired - health);
	//can't use this, AutoHeal won't work unless we go thru normal damage stuff

	DamageInfo extraDamageInfo;
	extraDamageInfo.in.m_damageType = DAMAGE_UNRESISTABLE;
	extraDamageInfo.in.m_deathType = DEATH_NONE;
	extraDamageInfo.in.m_sourceID = getObject()->getID();
	extraDamageInfo.in.m_amount = amount;
	getObject()->attemptDamage(&extraDamageInfo);

	m_ignoreDamage = false;

	m_virtualMinesRemaining = 0;
	getObject()->setModelConditionState(MODELCONDITION_RUBBLE);
	getObject()->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_MASKED ) );
}

// ------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void MinefieldBehavior::setScootParms(const Coord3D& start, const Coord3D& end)
{
	Object* obj = getObject();
	const MinefieldBehaviorModuleData* d = getMinefieldBehaviorModuleData();
	UnsignedInt scootFromStartingPointTime = d->m_scootFromStartingPointTime;

	Coord3D endOnGround = end;
	endOnGround.z = TheTerrainLogic->getGroundHeight( endOnGround.x, endOnGround.y );
	if (start.z > endOnGround.z)
	{
		// figure out how long it will take to fall, and replace scoot time with that
		UnsignedInt fallingTime = REAL_TO_INT_CEIL(WWMath::SqrtfOrigin(2.0f * (start.z - endOnGround.z) / fabs(TheGlobalData->m_gravity)));
		// we can scoot after we land, but don't want to stop scooting before we land
		if (scootFromStartingPointTime < fallingTime)
			scootFromStartingPointTime = fallingTime;
	}

	if (scootFromStartingPointTime == 0)
	{
		obj->setPosition(&endOnGround);
		m_scootFramesLeft = 0;
	}
	else
	{
		// x = x0 + vt + 0.5at^2
		// thus 2(dx - vt)/t^2 = a
		Real dx = endOnGround.x - start.x;
		Real dy = endOnGround.y - start.y;
		Real dz = endOnGround.z - start.z;
		Real dist = WWMath::SqrtOrigin(sqr(dx) + sqr(dy));
		if (dist <= 0.1f && fabs(dz) <= 0.1f)
		{
			obj->setPosition(&endOnGround);
			m_scootFramesLeft = 0;
		}
		else
		{
			Real t = (Real)scootFromStartingPointTime;
			Real scootFromStartingPointSpeed = dist / t;
			Real accelMag = fabs(2.0f * (dist - scootFromStartingPointSpeed*t)/sqr(t));
			Real dxNorm = (dist <= 0.1f) ? 0.0f : (dx / dist);
			Real dyNorm = (dist <= 0.1f) ? 0.0f : (dy / dist);
			m_scootVel.x = dxNorm * scootFromStartingPointSpeed;
			m_scootVel.y = dyNorm * scootFromStartingPointSpeed;
			m_scootAccel.x = -dxNorm * accelMag;
			m_scootAccel.y = -dyNorm * accelMag;
			m_scootAccel.z = TheGlobalData->m_gravity;
			obj->setPosition(&start);
			m_scootFramesLeft = scootFromStartingPointTime;

			// we need to wake ourselves up because we could be lying here sleeping forever
			setWakeFrame( obj, calcSleepTime() );

		}
	}

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void MinefieldBehavior::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void MinefieldBehavior::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// mines remaining
	/// @todo srj -- ensure health, appearance, etc are correct for save/reload! post-MP!
	xfer->xferUnsignedInt( &m_virtualMinesRemaining );

	// next death check frame
	xfer->xferUnsignedInt( &m_nextDeathCheckFrame );

	// scoot frames left
	xfer->xferUnsignedInt( &m_scootFramesLeft );

	// scoot velocity
	xfer->xferCoord3D( &m_scootVel );

	// scoot acceleration
	xfer->xferCoord3D( &m_scootAccel );

	xfer->xferBool( &m_ignoreDamage );
	xfer->xferBool( &m_regenerates );
	xfer->xferBool( &m_draining );

	// immunities
	UnsignedByte maxImmunity = MAX_IMMUNITY;
	xfer->xferUnsignedByte( &maxImmunity );
	if( maxImmunity != MAX_IMMUNITY )
	{

		DEBUG_CRASH(( "MinefieldBehavior::xfer - MAX_IMMUNITY has changed size, you must version this code and then you can remove this error message" ));
		throw SC_INVALID_DATA;

	}
	for( UnsignedByte i = 0; i < maxImmunity; ++i )
	{

		// object id
		xfer->xferObjectID( &m_immunes[ i ].id );

		// collide time
		xfer->xferUnsignedInt( &m_immunes[ i ].collideTime );

	}

	if( xfer->getXferMode() == XFER_LOAD )
		m_detonators.clear();

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void MinefieldBehavior::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
