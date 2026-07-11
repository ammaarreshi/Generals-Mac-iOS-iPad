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
//  (c) 2003 Electronic Arts Inc.																				      //
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: CountermeasuresBehavior.cpp //////////////////////////////////////////////////////////////
// Author: Kris Morness, April 2003
// Desc: Handles countermeasure firing when under missile threat, and responsible
//       for diverting missiles to the flares.
///////////////////////////////////////////////////////////////////////////////////////////////////


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/Xfer.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Anim2D.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/Module/CountermeasuresBehavior.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
struct CountermeasuresPlayerScanHelper
{
	KindOfMaskType m_kindOfToTest;
	Object *m_theHealer;
	ObjectPointerList *m_objectList;
};

static void checkForCountermeasures( Object *testObj, void *userData )
{
	CountermeasuresPlayerScanHelper *helper = (CountermeasuresPlayerScanHelper*)userData;
	ObjectPointerList *listToAddTo = helper->m_objectList;

	if( testObj->isEffectivelyDead() )
		return;

	if( testObj->getControllingPlayer() != helper->m_theHealer->getControllingPlayer() )
		return;

	if( testObj->isOffMap() )
		return;

	if( !testObj->isAnyKindOf(helper->m_kindOfToTest) )
		return;

	if( testObj->getBodyModule()->getHealth() >= testObj->getBodyModule()->getMaxHealth() )
		return;

	listToAddTo->push_back(testObj);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CountermeasuresBehavior::CountermeasuresBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	const CountermeasuresBehaviorModuleData *data = getCountermeasuresBehaviorModuleData();
	m_availableCountermeasures = data->m_numberOfVolleys * data->m_volleySize;
	m_reactionFrame = 0;
	m_activeCountermeasures = 0;
	m_divertedMissiles = 0;
	m_incomingMissiles = 0;
	m_nextVolleyFrame = 0;

	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CountermeasuresBehavior::~CountermeasuresBehavior()
{
}

// ------------------------------------------------------------------------------------------------
void CountermeasuresBehavior::reportMissileForCountermeasures( Object *missile )
{
	if( !missile )
	{
		return;
	}

	//Record the number of missiles that have been fired at us
	m_incomingMissiles++;

  if( m_availableCountermeasures + m_activeCountermeasures > 0 )
	{
		//We have countermeasures we can use. Determine now whether or not the incoming missile will
		//be diverted.
		const CountermeasuresBehaviorModuleData *data = getCountermeasuresBehaviorModuleData();

		if( GameLogicRandomValueReal( 0.0f, 1.0f ) < data->m_evasionRate )
		{
			//This missile will be diverted!
			ProjectileUpdateInterface* pui = nullptr;
			for( BehaviorModule** u = missile->getBehaviorModules(); *u; ++u )
			{
				if( (pui = (*u)->getProjectileUpdateInterface()) != nullptr )
				{
					//Make sure the missile diverts after a delay. The delay needs to be larger than
					//the countermeasure reaction time or else the missile won't have a countermeasure to divert to!
					DEBUG_ASSERTCRASH( data->m_countermeasureReactionFrames < data->m_missileDecoyFrames,
						("MissileDecoyDelay needs to be less than CountermeasureReactionTime in order to function properly.") );
					pui->setFramesTillCountermeasureDiversionOccurs( data->m_missileDecoyFrames );
					m_divertedMissiles++;

					if( m_activeCountermeasures == 0 && m_reactionFrame == 0 )
					{
						//We need to launch our first volley of countermeasures, but we can't do it now. If we
						//do, it'll look too artificial. Instead, we need to set up a timer to fake a reaction
						//delay.
						m_reactionFrame = TheGameLogic->getFrame() + data->m_countermeasureReactionFrames;
					}
					break;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
ObjectID CountermeasuresBehavior::calculateCountermeasureToDivertTo( const Object& victim )
{
	const CountermeasuresBehaviorModuleData *data = getCountermeasuresBehaviorModuleData();

	// TheSuperHackers @bugfix Mauller/Stubbjax 27/07/2025 Fix unsafe iterator handling and correct the countermeasures behavior
	// The code now iterates through all flares in the volley and selects the nearest one to the weapon as intended in the original EA code
	// This can slightly change behavior but does not significantly impact the overall survivability of the aircraft

	Real closestFlareDist = 1e15f;
	Object *closestFlare = nullptr;

	const UnsignedInt volleySize = max(data->m_volleySize, 1u);
	UnsignedInt volleyFlaresCounted = 0;

	//Flares are pushed to the front of the list, but we only want to acquire the "newest" of the flares, therefore
	//Start at the end of the list and go towards the beginning.
	CountermeasuresVec::reverse_iterator it = m_counterMeasures.rbegin();
	//stop iterating after we've reached size of a single volley.
	while( it != m_counterMeasures.rend() && volleyFlaresCounted < volleySize )
	{
		Object *obj = TheGameLogic->findObjectByID( *it++ );
		if( obj )
		{
			Real weaponToFlareDist = ThePartitionManager->getDistanceSquared( obj, getObject(), FROM_CENTER_2D );
			if( weaponToFlareDist < closestFlareDist )
			{
				closestFlareDist = weaponToFlareDist;
				closestFlare = obj;
			}
#if RETAIL_COMPATIBLE_CRC
			// TheSuperHackers @info Original EA code did not work as intended and stopped on the first retrieved flare
			// The non retail behaviour corrects the code to iterate through all flares in the volley
			break;
#else
			++volleyFlaresCounted;
#endif
		}

#if RETAIL_COMPATIBLE_CRC
		++volleyFlaresCounted;
#endif
	}

	if( closestFlare )
	{
		return closestFlare->getID();
	}
	return INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
Bool CountermeasuresBehavior::isActive() const
{
	return isUpgradeActive();
}

//-------------------------------------------------------------------------------------------------
/** The update callback. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime CountermeasuresBehavior::update()
{
	UnsignedInt now = TheGameLogic->getFrame();
	const CountermeasuresBehaviorModuleData *data = getCountermeasuresBehaviorModuleData();
	Object *obj = getObject();

	if( obj->isEffectivelyDead() )
	{
		return UPDATE_SLEEP_FOREVER;
	}
	if( !isUpgradeActive()  )
	{
		return UPDATE_SLEEP_FOREVER;
	}

	//Validate all existing flares, and clean them up as needed.
	for (CountermeasuresVec::iterator it = m_counterMeasures.begin(); it != m_counterMeasures.end(); /*nothing*/ )
	{
		Object *obj = TheGameLogic->findObjectByID( *it );
		if( !obj )
		{
			it = m_counterMeasures.erase( it );
			m_activeCountermeasures--;
		}
		else
		{
			++it;
		}
	}

	if( obj->isAirborneTarget() )
	{

		//Handle flare volley launching (initial reaction, and continuation firing).
		if( m_availableCountermeasures )
		{
			//Deal with the initial volley, but wait until we are permitted to react.
			if( m_reactionFrame )
			{
				if( m_reactionFrame == now )
				{
					//We have been shot at and now that the reaction timer has expired, fire a full volley of
					//countermeasures.
					launchVolley();
					m_nextVolleyFrame = now + data->m_framesBetweenVolleys;
					m_reactionFrame = 0;
				}
			}

			//Handle subsequent volley launching.
			if( m_nextVolleyFrame == now )
			{
				launchVolley();
				m_nextVolleyFrame = now + data->m_framesBetweenVolleys;
			}
		}
	}

	//Handle auto-reloading (data->m_reloadFrames of zero means it's not possible to auto-reload).
	//Aircraft that don't auto-reload require landing at an airfield for resupply.
	if( !m_availableCountermeasures && data->m_reloadFrames )
	{
		if( m_reloadFrame != 0 )
		{
			if( m_reloadFrame <= now )
			{
				//We've successfully reloaded automatically.
				reloadCountermeasures();
			}
		}
		else
		{
			//We just started reloading, so set the frame it'll be ready.
			m_reloadFrame = now + data->m_reloadFrames;
		}
	}

	return UPDATE_SLEEP( UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
void CountermeasuresBehavior::reloadCountermeasures()
{
	const CountermeasuresBehaviorModuleData *data = getCountermeasuresBehaviorModuleData();
	m_availableCountermeasures = data->m_numberOfVolleys * data->m_volleySize;
	m_reloadFrame = 0;
}

//-------------------------------------------------------------------------------------------------
void CountermeasuresBehavior::launchVolley()
{
	const CountermeasuresBehaviorModuleData *data = getCountermeasuresBehaviorModuleData();
	Object *obj = getObject();

	Real volleySize = (Real)data->m_volleySize;
	for( UnsignedInt i = 0; i < data->m_volleySize; i++ )
	{
		//Each flare in a volley will calculate a different vector to fly out. We have a +/- angle to
		//spread out equally. With only one flare, it'll come straight out the back. Two flares will
		//launch at the extreme positive and negative angle. Three flares will launch at extreme angles
		//plus straight back. Four or more will divvy it up equally.
		Real currentVolley = (Real)i;
		Real ratio = 0.0f;
		if( volleySize != 1.0f )
		{
			//ratio between -1.0 and +1.0f
			ratio = currentVolley / (volleySize - 1.0f) * 2.0f - 1.0f;
		}
		//Now calculate the angle. Simply multiply it by the ratio!
		Real angle = ratio * data->m_volleyArcAngle;

		Coord3D vel;
		PhysicsBehavior *physics = obj->getPhysics();

		//Calculate the angle to fire the flare by taking the facing angle and rotating it
		//and then scaling it by it's velocity (if it's moving).
		obj->getUnitDirectionVector3D( vel );
		Vector2 flareVector;
		flareVector.X = vel.x;
		flareVector.Y = vel.y;
		flareVector.Normalize();
		flareVector.Rotate( angle );
		//Give it back to the Coord3D
		vel.x = flareVector.X;
		vel.y = flareVector.Y;
		vel.z = 0.0f;

		Real velocity = physics->getVelocityMagnitude();
		if( velocity < 1.0f )
		{
			velocity = -10.0f;
		}
		vel.scale( velocity * data->m_volleyVelocityFactor );

		const ThingTemplate *thing = TheThingFactory->findTemplate( data->m_flareTemplateName );
		if( thing )
		{
			Object *flare = TheThingFactory->newObject( thing, obj->getControllingPlayer()->getDefaultTeam() );
			flare->setPosition( obj->getPosition() );
			flare->setOrientation( obj->getOrientation() );
			physics->transferVelocityTo( flare->getPhysics() );
			flare->getPhysics()->applyMotiveForce( &vel );
			m_activeCountermeasures++;
			m_availableCountermeasures--;
			m_counterMeasures.push_back( flare->getID() );
		}
	}
}


//------------------------------------------------------------------------------------------------
/** CRC */
//------------------------------------------------------------------------------------------------
void CountermeasuresBehavior::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

	// extend base class
	UpgradeMux::upgradeMuxCRC( xfer );

}

//------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
//------------------------------------------------------------------------------------------------
void CountermeasuresBehavior::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// extend base class
	UpgradeMux::upgradeMuxXfer( xfer );

	if( currentVersion >= 2 )
	{
		xfer->xferSTLObjectIDVector( &m_counterMeasures );
		xfer->xferUnsignedInt( &m_availableCountermeasures );
		xfer->xferUnsignedInt( &m_activeCountermeasures );
		xfer->xferUnsignedInt( &m_divertedMissiles );
		xfer->xferUnsignedInt( &m_incomingMissiles );
		xfer->xferUnsignedInt( &m_reactionFrame );
		xfer->xferUnsignedInt( &m_nextVolleyFrame );
	}

}

//------------------------------------------------------------------------------------------------
/** Load post process */
//------------------------------------------------------------------------------------------------
void CountermeasuresBehavior::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

	// extend base class
	UpgradeMux::upgradeMuxLoadPostProcess();

}


