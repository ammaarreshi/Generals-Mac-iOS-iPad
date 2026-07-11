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

// FILE: ToppleUpdate.cpp ///////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, December 2001
// Desc:   Update for something that sits around until hit, then topples and stops
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/ToppleUpdate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameClient/Drawable.h"
#include "GameClient/Module/SwayClientUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Damage.h"

//-------------------------------------------------------------------------------------------------
// this is our "bounce" limit -- slightly less that 90 degrees, to account for slop.
static const Real ANGULAR_LIMIT = PI/2 - PI/64;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ToppleUpdateModuleData::ToppleUpdateModuleData()
{
	const Real START_VELOCITY_PERCENT = 0.2f;
	const Real START_ACCEL_PERCENT = 0.01f;
	const Real VELOCITY_BOUNCE_PERCENT = 0.3f;			// multiply the velocity by this when you bounce
	m_toppleFX = nullptr;
	m_bounceFX = nullptr;
	m_stumpName.clear();
	m_killWhenToppled = true;
	m_killWhenStartToppled = false;
	m_killStumpWhenToppled = false;
	m_toppleLeftOrRightOnly = false;
	m_reorientToppledRubble = false;
	m_initialVelocityPercent = START_VELOCITY_PERCENT;
	m_initialAccelPercent = START_ACCEL_PERCENT;
	m_bounceVelocityPercent = VELOCITY_BOUNCE_PERCENT;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ToppleUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "ToppleFX",	INI::parseFXList, nullptr, offsetof( ToppleUpdateModuleData, m_toppleFX ) },
		{ "BounceFX",	INI::parseFXList, nullptr, offsetof( ToppleUpdateModuleData, m_bounceFX ) },
		{ "StumpName",	INI::parseAsciiString, nullptr, offsetof( ToppleUpdateModuleData, m_stumpName ) },
		{ "KillWhenStartToppling",	INI::parseBool, nullptr, offsetof( ToppleUpdateModuleData, m_killWhenStartToppled ) },
		{ "KillWhenFinishedToppling",	INI::parseBool, nullptr, offsetof( ToppleUpdateModuleData, m_killWhenToppled ) },
		{ "KillStumpWhenToppled",	INI::parseBool, nullptr, offsetof( ToppleUpdateModuleData, m_killStumpWhenToppled ) },
		{ "ToppleLeftOrRightOnly",	INI::parseBool, nullptr, offsetof( ToppleUpdateModuleData, m_toppleLeftOrRightOnly ) },
		{ "ReorientToppledRubble",	INI::parseBool, nullptr, offsetof( ToppleUpdateModuleData, m_reorientToppledRubble ) },
		{ "InitialVelocityPercent",	INI::parsePercentToReal, nullptr, offsetof( ToppleUpdateModuleData, m_initialVelocityPercent ) },
		{ "InitialAccelPercent",	INI::parsePercentToReal, nullptr, offsetof( ToppleUpdateModuleData, m_initialAccelPercent ) },
		{ "BounceVelocityPercent",	INI::parsePercentToReal, nullptr, offsetof( ToppleUpdateModuleData, m_bounceVelocityPercent ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ToppleUpdate::ToppleUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_angleDeltaX = 0.0f;
	m_doBounceFX = FALSE;
	m_numAngleDeltaX = 0;
	m_angularVelocity = 0;
	m_angularAccumulation = 0;
	m_angularAcceleration = 0;
	m_toppleDirection.x = 0;
	m_toppleDirection.y = 0;
	m_toppleDirection.z = 0;
	m_toppleState = TOPPLE_UPRIGHT;
	m_options = TOPPLE_OPTIONS_NONE;
	m_stumpID = INVALID_ID;
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ToppleUpdate::~ToppleUpdate()
{
}

//-------------------------------------------------------------------------------------------------
static Real angleClosestTo(Real a1, Real a2, Real desired)
{
	a1 = normalizeAngle(a1);
	a2 = normalizeAngle(a2);
	return (fabs(stdAngleDiff(desired, a1)) < fabs(stdAngleDiff(desired, a2))) ? a1 : a2;
}

//-------------------------------------------------------------------------------------------------
///< Start the toppling process by giving a force vector
//-------------------------------------------------------------------------------------------------
void ToppleUpdate::applyTopplingForce( const Coord3D* toppleDirection, Real toppleSpeed,
																			 UnsignedInt options )
{
	if (getObject()->isEffectivelyDead())
		return;

	//DEBUG_LOG(("awaking ToppleUpdate %08lx",this));
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);

	const ToppleUpdateModuleData* d = getToppleUpdateModuleData();

	if (d->m_killWhenStartToppled)
	{
		setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
		getObject()->kill();
		return;
	}

	m_toppleDirection = *toppleDirection;
	m_toppleDirection.normalize();
	TheScriptEngine->adjustToppleDirection(getObject(), &m_toppleDirection);

	m_angularVelocity = toppleSpeed * d->m_initialVelocityPercent;
	m_angularAcceleration = toppleSpeed * d->m_initialAccelPercent;
	m_toppleState = TOPPLE_FALLING;
	m_options = options;

	// tell the drawable to stop swaying
	Drawable * draw = getObject()->getDrawable();
	static NameKeyType nameKeySwayUpdate = NAMEKEY("SwayClientUpdate");

	ClientUpdateModule ** clientModules = draw->getClientUpdateModules();
	if (clientModules)
	{
		while (*clientModules)
		{
			if ((*clientModules)->getModuleNameKey() == nameKeySwayUpdate)
				(*(SwayClientUpdate **)clientModules)->stopSway();

			++clientModules;
		}
	}

	// rotate around the z-axis so that our x-axis is perpendicular to the topple direction.
	// this is really a trick to ensure that relatively planar things (eg, streetlights)
	// fall parallel to the ground, so that they don't up sticking thru the ground.
	// yeah, it assumes the models are constructed appropriately, but is a cheap way
	// of minimizing the problem. (srj)
	Real curAngleX = normalizeAngle(getObject()->getOrientation());
	Real toppleAngle = normalizeAngle(WWMath::Atan2Origin(m_toppleDirection.y, m_toppleDirection.x));
	if (d->m_toppleLeftOrRightOnly)
	{
		// it's a fence or such, and can only topple left or right, so pick the closest
		toppleAngle = angleClosestTo(curAngleX + PI/2, curAngleX - PI/2, toppleAngle);
		m_toppleDirection.x = Cos(toppleAngle);
		m_toppleDirection.y = Sin(toppleAngle);

		// go ahead and remove it from the pathfinder now, rather than waiting for the topple to
		// finish.... since we might be in a slightly different position when toppled, which can
		// confuse the pathfinder and not de-obstacle everything correctly
		TheAI->pathfinder()->removeObjectFromPathfindMap(getObject());

	}
	// desired angle is toppleAngle +/- pi/2, whichever is closer to curangle
	Real desiredAngleX = angleClosestTo(toppleAngle + PI/2, toppleAngle - PI/2, curAngleX);
	m_numAngleDeltaX = REAL_TO_INT_FLOOR(ANGULAR_LIMIT / (m_angularVelocity * 2));
	if (m_numAngleDeltaX < 1)
		m_numAngleDeltaX = 1;
	m_angleDeltaX = (desiredAngleX - curAngleX) / m_numAngleDeltaX;

	getObject()->getDrawable()->setModelConditionState(MODELCONDITION_TOPPLED);
	FXList::doFXObj(d->m_toppleFX, getObject());

	// if this is a tree, create a stump
	if (!d->m_stumpName.isEmpty())
	{
		const ThingTemplate* ttn = TheThingFactory->findTemplate(d->m_stumpName);
		Object *stump = TheThingFactory->newObject( ttn, nullptr );
		if (stump)
		{
			stump->setPosition( getObject()->getPosition() );
			stump->setOrientation( getObject()->getOrientation() );
			m_stumpID = stump->getID();

			// if we are "burned", then we will burn our stump
			const Drawable* draw = getObject()->getDrawable();
			if( draw )
			{
				if( draw->getModelConditionFlags().test( MODELCONDITION_BURNED ) == TRUE )
				{
					Drawable* stumpDraw = stump->getDrawable();

					if( stumpDraw )
						stumpDraw->setModelConditionState( MODELCONDITION_BURNED );

				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
///< Ask if this module is able to be toppled
//-------------------------------------------------------------------------------------------------
Bool ToppleUpdate::isAbleToBeToppled() const
{
	return m_toppleState == TOPPLE_UPRIGHT;
}

//-------------------------------------------------------------------------------------------------
static void deathByToppling(Object* obj)
{
	// use a special "topppling" damage type here so that
	// toppled stuff can have different damage/die modules
	// for toppled-death vs other-death
	DamageInfo damageInfo;
	damageInfo.in.m_damageType = DAMAGE_UNRESISTABLE;
	damageInfo.in.m_deathType = DEATH_TOPPLED;
	damageInfo.in.m_sourceID = INVALID_ID;
	damageInfo.in.m_amount = HUGE_DAMAGE_AMOUNT;
	obj->attemptDamage(&damageInfo);
}

//-------------------------------------------------------------------------------------------------
///< Keep track of rotational fall distance, bounce and/or stop when needed.
//-------------------------------------------------------------------------------------------------
UpdateSleepTime ToppleUpdate::update()
{
	//DEBUG_LOG(("updating ToppleUpdate %08lx",this));
	DEBUG_ASSERTCRASH(m_toppleState != TOPPLE_UPRIGHT, ("hmm, we should be sleeping here"));
	if ( (m_toppleState == TOPPLE_UPRIGHT)  ||  (m_toppleState == TOPPLE_DOWN) )
		return UPDATE_SLEEP_FOREVER;

	const ToppleUpdateModuleData* d = getToppleUpdateModuleData();
	const Real VELOCITY_BOUNCE_LIMIT = 0.01f;				// if the velocity after a bounce will be this or lower, just stop at zero
	const Real VELOCITY_BOUNCE_SOUND_LIMIT = 0.03f;	// and if this low, then skip the bounce sound

	Object* obj = getObject();
	if (m_numAngleDeltaX)
	{
		Matrix3D xfrm = *obj->getTransformMatrix();
		xfrm.In_Place_Pre_Rotate_Z(m_angleDeltaX);
		obj->setTransformMatrix(&xfrm);
		--m_numAngleDeltaX;
	}

	Real curVelToUse = m_angularVelocity;
	if (m_angularAccumulation + curVelToUse > ANGULAR_LIMIT)
		curVelToUse = ANGULAR_LIMIT - m_angularAccumulation;

	Matrix3D xfrm = *obj->getTransformMatrix();
	xfrm.In_Place_Pre_Rotate_X(-curVelToUse * m_toppleDirection.y);
	xfrm.In_Place_Pre_Rotate_Y(curVelToUse * m_toppleDirection.x);
	obj->setTransformMatrix(&xfrm);

	m_angularAccumulation += curVelToUse;
	if ((m_angularAccumulation >= ANGULAR_LIMIT) && (m_angularVelocity > 0))
	{
		// Hit so either bounce or stop if too little remaining velocity.
		m_angularVelocity *= -d->m_bounceVelocityPercent;

		if( BitIsSet( m_options, TOPPLE_OPTIONS_NO_BOUNCE ) == TRUE ||
				fabs(m_angularVelocity) < VELOCITY_BOUNCE_LIMIT )
		{
			// too slow, just stop
			m_angularVelocity = 0;
			m_toppleState = TOPPLE_DOWN;

			if (d->m_killWhenToppled)
			{
				deathByToppling(obj);
				if (d->m_reorientToppledRubble)
				{
					// we have a separate rubble state that needs to be upright, and centered
					// on the new "center" pos...
					Vector3 pos;
					pos.X = 0;
					pos.Y = 0;
					pos.Z = obj->getGeometryInfo().getMaxHeightAbovePosition();
					Matrix3D::Transform_Vector(*obj->getTransformMatrix(), pos, &pos);

					Coord3D tmp;
					tmp.x = pos.X;
					tmp.y = pos.Y;
					tmp.z = pos.Z;
					obj->setPosition(&tmp);

					// this relies on the fact that setOrientation always forces us straight up in the Z axis!
					obj->setOrientation(obj->getOrientation());

				}
			}

			if (d->m_killStumpWhenToppled)
			{
				Object* stump = TheGameLogic->findObjectByID(m_stumpID);
				if (stump)
				{
					deathByToppling(stump);
				}
			}
		}
		else if( fabs(m_angularVelocity) >= VELOCITY_BOUNCE_SOUND_LIMIT )
		{
			// fast enough bounce to warrant the bounce fx
			if( BitIsSet( m_options, TOPPLE_OPTIONS_NO_FX ) == FALSE )
				FXList::doFXObj(d->m_bounceFX, obj);
		}
	}
	else
	{
		m_angularVelocity += m_angularAcceleration;
	}

	Drawable *draw=obj->getDrawable();
	if (draw)
		draw->setShadowsEnabled(false);

	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
/** Do the collision */
//-------------------------------------------------------------------------------------------------
void ToppleUpdate::onCollide( Object *other, const Coord3D *loc, const Coord3D *normal )
{
	// Note that other == null means "collide with ground"
	//
	if (other == nullptr)
		return;

	//@todo JohnA -- Should you get around to adding trees to avoidance pathfinding, then you'll
	//want to change this code:
	//if( other->getCrusherLevel() > getObject()->getCrushableLevel() ) //<----proper tree method
	if( other->getCrusherLevel() > 1 )
	{

		// Give a vector with direction to thing and my speed.
		Coord3D toppleVector = *getObject()->getPosition();
		toppleVector.x -= other->getPosition()->x;
		toppleVector.y -= other->getPosition()->y;
		toppleVector.z = 0;

		Coord3D vel;
		PhysicsBehavior* phys = other->getPhysics();
		if (phys)
			vel = *phys->getVelocity();
		else
			vel.zero();
		getObject()->topple( &toppleVector, vel.length(), TOPPLE_OPTIONS_NONE );

	}

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ToppleUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ToppleUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// angular velocity
	xfer->xferReal( &m_angularVelocity );

	// angular acceleration
	xfer->xferReal( &m_angularAcceleration );

	// topple direction
	xfer->xferCoord3D( &m_toppleDirection );

	// topple state
	xfer->xferUser( &m_toppleState, sizeof( ToppleState	) );

	// angular accumulation
	xfer->xferReal( &m_angularAccumulation );

	// angle delta X
	xfer->xferReal( &m_angleDeltaX );

	// num angle delta X
	xfer->xferInt( &m_numAngleDeltaX );

	// do bounce FX
	xfer->xferBool( &m_doBounceFX );

	// options
	xfer->xferUnsignedInt( &m_options );

	// stump id
	xfer->xferObjectID( &m_stumpID );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ToppleUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
