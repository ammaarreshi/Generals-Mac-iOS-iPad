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

// FILE: EMPUpdate.cpp ///////////////////////////////////////////////////////////////////////
// Author: Mark Lorenzen Sept. 2002
// Desc:   Update that makes the electromagnetic pulse field grow, fade and disable junk
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/Player.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/EMPUpdate.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameClient/Drawable.h"
#include "Common/KindOf.h"
#include "GameClient/ParticleSys.h"









///////////////////////////////////////////////////////////////////////////////////////////
//	Static member initialization
///////////////////////////////////////////////////////////////////////////////////////////
//Bool	EMPUpdate::s_lastInstanceSpunPositive	= FALSE;

//-------------------------------------------------------------------------------------------------
static void saturateRGB(RGBColor& color, Real factor)
{
	color.red *= factor;
	color.green *= factor;
	color.blue *= factor;

	Real halfFactor = factor * 0.5f;

	color.red -= halfFactor;
	color.green -= halfFactor;
	color.blue -= halfFactor;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
EMPUpdate::EMPUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{

	//s_lastInstanceSpunPositive = !s_lastInstanceSpunPositive; //TOGGLES STATIC BOOL

	const EMPUpdateModuleData *data = getEMPUpdateModuleData();
	if ( data )
	{
		//SANITY
		DEBUG_ASSERTCRASH( TheGameLogic, ("EMPUpdate::EMPUpdate - TheGameLogic is null" ) );
		UnsignedInt now = TheGameLogic->getFrame();

		m_currentScale = data->m_startScale;
		m_dieFrame = REAL_TO_UNSIGNEDINT( now + data->m_lifeFrames );
		m_tintEnvPlayFrame = REAL_TO_UNSIGNEDINT( now + data->m_startFadeFrame );
		m_tintEnvFadeFrames = m_dieFrame - m_tintEnvPlayFrame;
		//m_spinRate = GameLogicRandomValueReal(data->m_spinRateMax * 0.5f, data->m_spinRateMax);
		m_targetScale = GameLogicRandomValueReal(data->m_targetScaleMin, data->m_targetScaleMax);
		//if (s_lastInstanceSpunPositive)
		//{
		//	m_spinRate *= -1.0f;
		//}

		getObject()->setOrientation(GameLogicRandomValueReal(-PI,PI));

		DEBUG_ASSERTCRASH( m_tintEnvPlayFrame < m_dieFrame, ("EMPUpdate::EMPUpdate - you cant play fade after death" ) );

		return;
	}

	//SANITY
	DEBUG_ASSERTCRASH( data, ("EMPUpdate::EMPUpdate - getEMPUpdateModuleData is null" ) );
	m_currentScale = 1.0f;
	m_dieFrame = 0;
	m_tintEnvFadeFrames = 0;
	m_tintEnvPlayFrame  = 0;
	//m_spinRate = 0;
	m_targetScale = 1;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
EMPUpdate::~EMPUpdate()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

UpdateSleepTime EMPUpdate::update()
{
/// @todo srj use SLEEPY_UPDATE here

	Object *obj = getObject();

	const EMPUpdateModuleData *data = getEMPUpdateModuleData();
	Drawable *dr = obj->getDrawable();
	UnsignedInt now = TheGameLogic->getFrame();

	m_currentScale += ( m_targetScale - m_currentScale ) * 0.05f;
	dr->setInstanceScale( m_currentScale );

	if ( now < m_tintEnvPlayFrame)
	{
		RGBColor start = data->m_startColor;
		saturateRGB( start, 2 );
		dr->colorTint( &start );
	}
	if ( now == m_tintEnvPlayFrame)
	{
		RGBColor end = data->m_endColor;
		saturateRGB( end, 5 );
		dr->colorFlash( &end, 0, m_tintEnvFadeFrames, ~0u );
		doDisableAttack();
	}

	//Real curSpin = obj->getOrientation();
	//curSpin += m_spinRate;
	//curSpin = normalizeAngle(curSpin);
	//obj->setOrientation( curSpin );

	if( now >= m_dieFrame )
		obj->kill();

	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void EMPUpdate::doDisableAttack()
{
	Object *object = getObject();
	const EMPUpdateModuleData *data = getEMPUpdateModuleData();
	if( !object || !data )
		return; //sanity

	Real radius = data->m_effectRadius;
	Real curVictimDistSqr;
	const Coord3D *pos = object->getPosition();

	//Kris -- October 28, 2003 -- Patch 1.01
	//If the EMP hits an airborne target, then don't allow the EMP
	//blast to effect anything on the ground.
	Object *producer = TheGameLogic->findObjectByID( object->getProducerID() );
	Object *intendedVictim = nullptr;
	Bool onlyEffectAirborne = FALSE;
	Bool intendedVictimProcessed = FALSE;
	if( producer && producer->getAI() )
	{
		intendedVictim = producer->getAI()->getCurrentVictim();
		if( intendedVictim && intendedVictim->isAirborneTarget() )
		{
			onlyEffectAirborne = TRUE;
		}
	}

	SimpleObjectIterator *iter = nullptr;
	Object *curVictim = nullptr;

	if (radius > 0.0f)
	{
		iter = ThePartitionManager->iterateObjectsInRange(pos,
			radius, FROM_BOUNDINGSPHERE_3D);

		curVictim = iter->firstWithNumeric(&curVictimDistSqr);
	}

	MemoryPoolObjectHolder hold(iter);

	for ( ; curVictim != nullptr; curVictim = iter ? iter->nextWithNumeric(&curVictimDistSqr) : nullptr)
	{
		if ( curVictim != object)
		{

			//Kris -- October 28, 2003 -- Patch 1.01
			//If the EMP hits an airborne target, then don't allow the EMP
			//blast to effect anything on the ground.
			if( onlyEffectAirborne && !curVictim->isAirborneTarget() )
			{
				continue;
			}

			//Some EMP attacks don't affect our own buildings.
			if( data->m_doesNotAffectMyOwnBuildings && curVictim->isKindOf( KINDOF_STRUCTURE ) )
			{
				if( curVictim->getControllingPlayer() == object->getControllingPlayer() )
				{
					continue;
				}
			}




//////////////	    // must match our kindof flags (if any)
//////////////	    if (data && !curVictim->isKindOfMulti(data->m_victimKindOf, data->m_victimKindOfNot))
//////////////		    continue;








      if ( !curVictim->isKindOf( KINDOF_VEHICLE ) && !curVictim->isKindOf(KINDOF_STRUCTURE) && !curVictim->isKindOf(KINDOF_SPAWNS_ARE_THE_WEAPONS) )
			{
				//DONT DISABLE PEOPLE, EXCEPT FOR STINGER SOLDIERS
				continue;
			}
			else if ( curVictim->isKindOf( KINDOF_AIRCRAFT ) && curVictim->isAirborneTarget() )// is in the sky
      {
        // WITHIN THE SET OF ALL FLYING THINGS, WE WANT TO EXEMPT SUPERWEAPON TRANSPORTS
//        if ( curVictim->isKindOf( KINDOF_TRANSPORT ) )                  // is transport kindof
//          if ( curVictim->getContain() )                                // does carry stuff
//            if ( curVictim->getContain()->getContainCount() > 0 )     // is carrying something
//              if ( ! curVictim->isKindOf( KINDOF_PRODUCED_AT_HELIPAD ) )  // but not a helicopter
//                continue;

        if ( curVictim->isKindOf( KINDOF_EMP_HARDENED ) ) // self-explanitory
          continue;

				curVictim->kill();// @todo this should use some sort of DEADSTICK DIE or something...
				Drawable *drw = curVictim->getDrawable();
				if ( drw )
				{
					drw->setTintStatus( TINT_STATUS_DISABLED );// paint it black
				}
				continue;
			}
			else if ( curVictim->isKindOf( KINDOF_STRUCTURE ) )
			{
				if ( ! curVictim->isFactionStructure() )
					continue;
			}
			// handle cases where we do not want allies to be hit by it's own EMP weapons
			else if ( (data->m_rejectMask & WEAPON_AFFECTS_ALLIES) && curVictim->getRelationship( object ) == ALLIES)
			{
				continue;
			}

			//Disable the target for a specified amount of time.
			curVictim->setDisabledUntil( DISABLED_EMP, TheGameLogic->getFrame() + data->m_disabledDuration );

			//Kris -- October 28, 2003 -- Patch 1.01
			if( intendedVictim == curVictim )
			{
				//We know the intended victim was in range. This will catch an edge case
				//where the intended target is hit, but the range is off enough to not effect it.
				intendedVictimProcessed = TRUE;
			}

			Drawable *drw = curVictim->getDrawable();
			if ( drw )
			{

				const ParticleSystemTemplate *tmp = data->m_disableFXParticleSystem;
				if (tmp)
				{
					Real victimHeight = curVictim->getGeometryInfo().getMaxHeightAbovePosition();
					Real victimFootprintArea = curVictim->getGeometryInfo().getFootprintArea();
					Real victimVolume = victimFootprintArea * MIN(victimHeight, 10.0f);

					UnsignedInt emitterCount = MAX(15, REAL_TO_INT_CEIL(data->m_sparksPerCubicFoot * victimVolume));

					for (UnsignedInt e = 0 ; e < emitterCount; ++e)
					{

						ParticleSystem *sys = TheParticleSystemManager->createParticleSystem(tmp);

						if (sys)
						{
							Coord3D offs = {0,0,0};
							curVictim->getGeometryInfo().makeRandomOffsetWithinFootprint( offs );
							offs.z = GameLogicRandomValue(3, victimHeight);

							//This puts all the sparks within a quadrahemicycloid (rectangular dome) volume
							//The same shape as a four cornered camping dome tent, for those with less Greek
							if (offs.length() > victimHeight)
							{
								Real resoreX = offs.x;
								Real resoreY = offs.y;
								offs.normalize();
								offs.z *= victimHeight;
								offs.x = resoreX;
								offs.y = resoreY;
							}

							sys->attachToObject(curVictim);
							sys->setPosition( &offs );
							sys->setSystemLifetime(MAX(0, data->m_disabledDuration - 30));
							sys->setInitialDelay(GameLogicRandomValue(1,100));
						}
					}
				}
			}

		}
	}

	//Kris -- October 28, 2003 -- Patch 1.01
	//Handle edge case when the EMP explodes, but "misses" the intended target.
	if( intendedVictim && !intendedVictimProcessed && intendedVictim->isKindOf( KINDOF_AIRCRAFT ) )
	{
    if( !intendedVictim->isKindOf( KINDOF_EMP_HARDENED ) )
		{
			//Victim position
			Coord3D coord;
			coord.set( *intendedVictim->getPosition() );
			//Subtract this object (distance from missile to victim's previous position)
			coord.sub( *pos );

			Real lengthSqr = coord.lengthSqr();
			if( lengthSqr <= radius * 2.0f || lengthSqr <= 40.0f * 40.0f )
			{
				//Disable the target for a specified amount of time.
				intendedVictim->setDisabledUntil( DISABLED_EMP, TheGameLogic->getFrame() + data->m_disabledDuration );
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void EMPUpdate::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void EMPUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void EMPUpdate::loadPostProcess()
{

}

                                                                                                                                                  ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////^
//  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////^^
//  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////^^^^^^
//  ////////////////////////////////////////////////////////////////////////////////////////////////^^^^^^^^^^^^^^
    /////////////////////////////////////////////////////////////////////////^^^^^^^^^^^^^
    ///////////////////////////////////////////////////^^^^^^^^^^^^
 /////////////////////////////////////^^^^^^^^^^
//////////////////////////^^^^^
//  ////////////////^^^
//  /////////////^
//  ///////////




//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LeafletDropBehavior::LeafletDropBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{

  m_fxFired = FALSE;
	//s_lastInstanceSpunPositive = !s_lastInstanceSpunPositive; //TOGGLES STATIC BOOL

	const LeafletDropBehaviorModuleData *data = getLeafletDropBehaviorModuleData();
	if ( data )
	{
		//SANITY
		DEBUG_ASSERTCRASH( TheGameLogic, ("LeafletDropBehavior::LeafletDropBehavior - TheGameLogic is null" ) );
		UnsignedInt now = TheGameLogic->getFrame();
    m_startFrame = now + data->m_delayFrames;

		return;
	}

	//SANITY
	DEBUG_ASSERTCRASH( data, ("LeafletDropBehavior::LeafletDropBehavior - getLeafletDropBehaviorModuleData is null" ) );
	m_startFrame = TheGameLogic->getFrame() + 1;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LeafletDropBehavior::~LeafletDropBehavior()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

UpdateSleepTime LeafletDropBehavior::update()
{

  if ( ! m_fxFired )
  {
    // start shoveling out those leaflets, boys.
	  const LeafletDropBehaviorModuleData *data = getLeafletDropBehaviorModuleData();
	  const ParticleSystemTemplate *tmp = data->m_leafletFXParticleSystem;
	  if (tmp)
	  {
		  ParticleSystem *sys = TheParticleSystemManager->createParticleSystem(tmp);
		  if (sys)
			  sys->attachToObject(getObject());
    }

    m_fxFired = TRUE; // hey, at least we tried.
  }


  if( TheGameLogic->getFrame() < m_startFrame )
  {
	//	TheGameLogic->destroyObject( getObject() );
    return UPDATE_SLEEP_FOREVER;
  }

  doDisableAttack();

  return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void LeafletDropBehavior::onDie( const DamageInfo *damageInfo )
{
  // the dieModule callback

  doDisableAttack();

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void LeafletDropBehavior::doDisableAttack()
{
	Object *object = getObject();
	const LeafletDropBehaviorModuleData *data = getLeafletDropBehaviorModuleData();
	if( !object || !data )
		return; //sanity

	Real radius = data->m_radius;
	Real curVictimDistSqr;
	const Coord3D *pos = object->getPosition();

	SimpleObjectIterator *iter = nullptr;
	Object *curVictim = nullptr;

	if (radius > 0.0f)
	{
		iter = ThePartitionManager->iterateObjectsInRange(pos,
			radius, FROM_BOUNDINGSPHERE_3D);

		curVictim = iter->firstWithNumeric(&curVictimDistSqr);
	}

	MemoryPoolObjectHolder hold(iter);

	for ( ; curVictim != nullptr; curVictim = iter ? iter->nextWithNumeric(&curVictimDistSqr) : nullptr)
	{
		if ( curVictim != object)
		{
      if ( ! (curVictim->isKindOf( KINDOF_INFANTRY) || curVictim->isKindOf( KINDOF_VEHICLE ) ) ) // both commuters and pedestrians
				continue;

      if ( curVictim->getRelationship( object ) != ENEMIES ) // only enemies
				continue;

			//Disable the target for a specified amount of time.
			curVictim->setDisabledUntil( DISABLED_EMP, TheGameLogic->getFrame() + data->m_disabledDuration );

		}
	}

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void LeafletDropBehavior::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void LeafletDropBehavior::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

  xfer->xferUnsignedInt( &m_startFrame );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void LeafletDropBehavior::loadPostProcess()
{

}





















