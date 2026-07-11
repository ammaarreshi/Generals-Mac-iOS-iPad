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

// FILE: LaserUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, July 2002
// Desc:   Handles laser update processing for render purposes and game control.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameUtility.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Xfer.h"
#include "Common/DrawModule.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Object.h"
#include "GameLogic/GameLogic.h" // For frame number
#include "GameLogic/Module/LaserUpdate.h"
#include "WWMath/vector3.h"


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LaserUpdateModuleData::LaserUpdateModuleData()
{
	m_punchThroughScalar = 0.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void LaserUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "MuzzleParticleSystem",		INI::parseAsciiString,	nullptr, offsetof( LaserUpdateModuleData, m_particleSystemName ) },
		{ "TargetParticleSystem",		INI::parseAsciiString,  nullptr, offsetof( LaserUpdateModuleData, m_targetParticleSystemName ) },
		{ "PunchThroughScalar",			INI::parseReal,					nullptr, offsetof( LaserUpdateModuleData, m_punchThroughScalar ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LaserRadiusUpdate::LaserRadiusUpdate()
{
	m_widening = false;
	m_widenStartFrame = 0;
	m_widenFinishFrame = 0;
	m_currentWidthScalar = 1.0f;
	m_decaying = false;
	m_decayStartFrame = 0;
	m_decayFinishFrame = 0;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LaserUpdate::LaserUpdate( Thing *thing, const ModuleData* moduleData ) : ClientUpdateModule( thing, moduleData )
{
	m_dirty = FALSE;
	m_endPos.zero();
	m_startPos.zero();
	m_particleSystemID = INVALID_PARTICLE_SYSTEM_ID;
	m_targetParticleSystemID = INVALID_PARTICLE_SYSTEM_ID;
	m_parentID = INVALID_DRAWABLE_ID;
	m_targetID = INVALID_DRAWABLE_ID;
	m_parentBoneName.clear();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LaserUpdate::~LaserUpdate()
{

	if( m_particleSystemID )
		TheParticleSystemManager->destroyParticleSystemByID( m_particleSystemID );
	if( m_targetParticleSystemID )
		TheParticleSystemManager->destroyParticleSystemByID( m_targetParticleSystemID );

}

//-------------------------------------------------------------------------------------------------
void LaserUpdate::updateStartPos()
{
	Coord3D oldStartPos = m_startPos;

	if( m_parentID == INVALID_DRAWABLE_ID )
		return;// Can't update if not told to update

	const Drawable *parentDrawable = TheGameClient->findDrawableByID(m_parentID);
	if( parentDrawable == nullptr )
		return;// Can't update if no one to ask

	if( m_parentBoneName.isNotEmpty() )
	{
		Matrix3D startPosMatrix;

		if( !parentDrawable->getCurrentWorldspaceClientBonePositions( m_parentBoneName.str(), startPosMatrix ) )
		{
			// failed to find the required bone, so just die

			//Kris: Doing this CRASHES THE GAME LATER!!!! Instead, let's set the position to the drawable, then
			//      create a nasty assert.
			//TheGameClient->destroyDrawable( getDrawable() );

			m_startPos.set( *parentDrawable->getPosition() );
			DEBUG_CRASH( ("LaserUpdate::updateStartPos() -- Drawable %s is expecting to find a bone %s but can't. Defaulting to position of drawable.",
				parentDrawable->getTemplate()->getName().str(), m_parentBoneName.str() ) );

			return;
		}



		m_startPos.x = startPosMatrix.Get_X_Translation();
		m_startPos.y = startPosMatrix.Get_Y_Translation();
		m_startPos.z = startPosMatrix.Get_Z_Translation();
	}
	else
	{
		// Just use parent position
		m_startPos = *parentDrawable->getPosition();
	}

	if( !(m_startPos == oldStartPos) ) // No != operator.  Heh.
		m_dirty = TRUE;
}

//-------------------------------------------------------------------------------------------------
void LaserUpdate::updateEndPos()
{
	const LaserUpdateModuleData *data = getLaserUpdateModuleData();
	Coord3D oldEndPos = m_endPos;

	if( m_targetID == INVALID_DRAWABLE_ID )
		return;// Can't update if not told to update

	const Drawable *targetDrawable = TheGameClient->findDrawableByID(m_targetID);
	Bool targetDead = (targetDrawable && targetDrawable->getObject())
										? targetDrawable->getObject()->isEffectivelyDead()
										: FALSE;
	if( targetDrawable == nullptr || targetDead )
	{
		// If here, we used to track something, but now it is gone.  So make our end point pierce through
		// the old spot, and then stop trying to find a target Drawable
		if( data->m_punchThroughScalar > 0 )
		{
			Vector3 laserVector;
			laserVector.Set(m_endPos.x, m_endPos.y, m_endPos.z);
			laserVector = laserVector - Vector3(m_startPos.x, m_startPos.y, m_startPos.z);
			laserVector *= data->m_punchThroughScalar;
			laserVector = laserVector + Vector3(m_startPos.x, m_startPos.y, m_startPos.z);
			m_endPos.x = laserVector.X;
			m_endPos.y = laserVector.Y;
			m_endPos.z = laserVector.Z;
		}

		m_targetID = INVALID_DRAWABLE_ID;
	}
	else
	{
		m_endPos = *targetDrawable->getPosition();
	}

	if( !(m_endPos == oldEndPos) ) // No != operator.  Heh.
		m_dirty = TRUE;
}

//-------------------------------------------------------------------------------------------------
/** The update callback. */
//-------------------------------------------------------------------------------------------------
void LaserUpdate::clientUpdate()
{
	updateStartPos();
	updateEndPos();
	m_dirty |= m_laserRadius.updateRadius();
}

//-------------------------------------------------------------------------------------------------
bool LaserRadiusUpdate::updateRadius()
{
	bool updated = false;
	if( m_decaying )
	{
		UnsignedInt now = TheGameLogic->getFrame();
		m_currentWidthScalar = 1.0f - (Real)(now - m_decayStartFrame) / (Real)(m_decayFinishFrame - m_decayStartFrame);
		updated = true;
		if( m_currentWidthScalar <= 0.0f )
		{
			m_currentWidthScalar = 0.0f;
			//m_decaying = false // ?????
		}
	}
	else if( m_widening )
	{
		//We need to resize our laser width based on the growth ratio completed.
		UnsignedInt now = TheGameLogic->getFrame();
		m_currentWidthScalar = (Real)(now - m_widenStartFrame) / (Real)(m_widenFinishFrame - m_widenStartFrame);
		updated = true;
		if( m_currentWidthScalar >= 1.0f )
		{
			m_currentWidthScalar = 1.0f;
			m_widening = false;
		}
	}

	return updated;
}

//-------------------------------------------------------------------------------------------------
void LaserRadiusUpdate::setDecayFrames( UnsignedInt decayFrames )
{
	if( decayFrames > 0 )
	{
		m_decaying = true;
		m_decayStartFrame = TheGameLogic->getFrame();
		m_decayFinishFrame = m_decayStartFrame + decayFrames;
		m_currentWidthScalar = 1.0f;
	}
}

//-------------------------------------------------------------------------------------------------
void LaserRadiusUpdate::initRadius( Int sizeDeltaFrames )
{
	if( sizeDeltaFrames > 0 )
	{
		m_widening = true;
		m_widenStartFrame = TheGameLogic->getFrame();
		m_widenFinishFrame = m_widenStartFrame + sizeDeltaFrames;
		m_currentWidthScalar = 0.0f;
	}
	else if( sizeDeltaFrames < 0 )
	{
		m_decaying = true;
		m_decayStartFrame = TheGameLogic->getFrame();
		m_decayFinishFrame = m_decayStartFrame - sizeDeltaFrames;
		m_currentWidthScalar = 1.0f;
	}
}

//-------------------------------------------------------------------------------------------------
void LaserRadiusUpdate::xfer( Xfer *xfer )
{
	// widening
	xfer->xferBool( &m_widening );

	// decaying
	xfer->xferBool( &m_decaying );

	// widen start frame
	xfer->xferUnsignedInt( &m_widenStartFrame );

	// widen finish frame
	xfer->xferUnsignedInt( &m_widenFinishFrame );

	// current width scalar
	xfer->xferReal( &m_currentWidthScalar );

	// decay start frame
	xfer->xferUnsignedInt( &m_decayStartFrame );

	// decay finish frame
	xfer->xferUnsignedInt( &m_decayFinishFrame );
}

//-------------------------------------------------------------------------------------------------
void LaserUpdate::initLaser( const Object *parent, const Object *target, const Coord3D *startPos, const Coord3D *endPos, AsciiString parentBoneName, Int sizeDeltaFrames )
{
	const LaserUpdateModuleData *data = getLaserUpdateModuleData();
	ParticleSystem *system;

	m_laserRadius.initRadius( sizeDeltaFrames );

	// Write down the bone name override
	m_parentBoneName = parentBoneName;

	//Record IDs if we have them, then figure out starting points
	if( parent )
	{
		// If a source object, use it
		if( parent->getDrawable() )
			m_parentID = parent->getDrawable()->getID();

		updateStartPos();
	}
	else if( startPos )
	{
		// or just use what they gave
		m_startPos = *startPos;
	}
	else
	{
		// if they gave nothing, then we are screwed
		TheGameClient->destroyDrawable( getDrawable() );
		return;
	}

	if( target && !endPos )
	{
		// If a target object, use it (unless we override it!)
		if( target->getDrawable() )
			m_targetID = target->getDrawable()->getID();

		m_endPos = *target->getPosition();
	}
	else if( endPos )
	{
		// just use what they gave, no override here
		m_endPos = *endPos;
	}
	else
	{
		// if they gave nothing, then we are screwed
		TheGameClient->destroyDrawable( getDrawable() );
		return;
	}

	// Create special particle systems
	//PLEASE NOTE You cannot check an ID for nullptr.  This should be a check against INVALID_PARTICLE_SYSTEM_ID.  Can't change it on the last day without a bug though.
	if( !m_particleSystemID )
	{
		const Player *localPlayer = rts::getObservedOrLocalPlayer();

		//Make sure the laser flare is visible to the player. If no parent, assume laser owner will handle it.
		if (!parent || parent->getShroudedStatus( localPlayer->getPlayerIndex() ) <= OBJECTSHROUD_PARTIAL_CLEAR )
		{

			//If we don't have a particle system for the lense flare (muzzle flare), create it.
			if( data->m_particleSystemName.isNotEmpty() )
			{
				const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate( data->m_particleSystemName );
				if( tmp )
				{
					system = TheParticleSystemManager->createParticleSystem( tmp );
					if( system )
					{
						m_particleSystemID = system->getSystemID();
					}
				}
			}

			//If we don't have a particle system for the target effect, create it.
			if( data->m_targetParticleSystemName.isNotEmpty() )
			{
				const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate( data->m_targetParticleSystemName );
				if( tmp )
				{
					system = TheParticleSystemManager->createParticleSystem( tmp );
					if( system )
					{
						m_targetParticleSystemID = system->getSystemID();
					}
				}
			}
		}
	}

	//Adjust the position of any existing particle system.
	//PLEASE NOTE You cannot check an ID for nullptr.  This should be a check against INVALID_PARTICLE_SYSTEM_ID.  Can't change it on the last day without a bug though.
	if( m_particleSystemID )
	{
		system = TheParticleSystemManager->findParticleSystem( m_particleSystemID );
		if( system )
		{
			system->setPosition( &m_startPos );
		}
	}

	//PLEASE NOTE You cannot check an ID for nullptr.  This should be a check against INVALID_PARTICLE_SYSTEM_ID.  Can't change it on the last day without a bug though.
	if( m_targetParticleSystemID )
	{
		system = TheParticleSystemManager->findParticleSystem( m_targetParticleSystemID );
		if( system )
		{
			system->setPosition( &m_endPos );
		}
	}

	//Important! Set the laser position to the average of both points or else
	//it probably won't get rendered!!!
	// And as a client update, we cannot set the logic position.
	Coord3D posToUse;
	if( parent == nullptr )
	{
		posToUse.set( *startPos );
		posToUse.add( *endPos );
		posToUse.scale( 0.5 );
	}
	else
	{
		posToUse = *parent->getPosition();
	}

	Drawable *draw = getDrawable();
	if( draw )
	{
		draw->setPosition( &posToUse );
	}

	m_dirty = true;
}

//-------------------------------------------------------------------------------------------------
Real LaserUpdate::getTemplateLaserRadius() const
{
	const Drawable *draw = getDrawable();
	const LaserDrawInterface* ldi = nullptr;
	for( const DrawModule** d = draw->getDrawModules(); *d; ++d )
	{
		ldi = (*d)->getLaserDrawInterface();
		if( ldi )
		{
			//***NOTE***
			//While it appears the logic is accessing client data, it is actually accessing template module
			//data from the client. This value is INI constant thus can't change. It's grouped with other
			//laser defining attributes and having it there makes it easier for artists.
			return ldi->getLaserTemplateWidth();
		}
	}
	return 0.0f;
}

//-------------------------------------------------------------------------------------------------
Real LaserUpdate::getCurrentLaserRadius() const
{
	return getTemplateLaserRadius() * getWidthScale();
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void LaserUpdate::crc( Xfer *xfer )
{

	// extend base class
	ClientUpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void LaserUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	ClientUpdateModule::xfer( xfer );

	// start pos
	xfer->xferCoord3D( &m_startPos );

	// end pos
	xfer->xferCoord3D( &m_endPos );

	// dirty
	xfer->xferBool( &m_dirty );

	// particle system ID
	xfer->xferUser( &m_particleSystemID, sizeof( ParticleSystemID ) );

	// target particle system id
	xfer->xferUser( &m_targetParticleSystemID, sizeof( ParticleSystemID ) );

	m_laserRadius.xfer( xfer );

	xfer->xferDrawableID(&m_parentID);
	xfer->xferDrawableID(&m_targetID);

	xfer->xferAsciiString(&m_parentBoneName);

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void LaserUpdate::loadPostProcess()
{

	// extend base class
	ClientUpdateModule::loadPostProcess();

}
