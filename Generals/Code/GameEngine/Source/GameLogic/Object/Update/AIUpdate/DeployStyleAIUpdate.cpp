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

// DeployStyleAIUpdate.cpp ////////////
// Author: Kris Morness, August 2002
// Desc:   A standard ai update that also handles units that must deploy to attack and pack before moving.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"

#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"

#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"

#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/DeployStyleAIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
DeployStyleAIUpdate::DeployStyleAIUpdate( Thing *thing, const ModuleData* moduleData ) : AIUpdateInterface( thing, moduleData )
{
	m_hasOutsideCommand = false;
	m_state = READY_TO_MOVE;
	m_frameToWakeForDeploy = 0;
	reset();
}

//-------------------------------------------------------------------------------------------------
DeployStyleAIUpdate::~DeployStyleAIUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool DeployStyleAIUpdate::isIdle() const
{
	// we need to do this because we enter an idle state during deploy states in these cases,
	// but scripting relies on us never claiming to be "idle"...
	if (m_hasOutsideCommand)
		return false;

	return AIUpdateInterface::isIdle();
}

//-------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::reset()
{
	m_designatedTargetID	= INVALID_ID;
	m_isAttackMultiple		= FALSE;
	m_overriddenAttack		= FALSE;
	m_isGuardingPosition	= FALSE;
	m_isAttackObject			= FALSE;
	m_attackObjectID			= INVALID_ID;
	m_isAttackPosition		= FALSE;
	m_position.zero();
}

//-------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::aiDoCommand( const AICommandParms* parms )
{
	if (!isAllowedToRespondToAiCommands(parms))
		return;

	if( parms->m_cmd != AICMD_IDLE && parms->m_cmd != AICMD_FOLLOW_PATH_APPEND )
	{
		aiIdle( CMD_FROM_AI );
	}
	if( parms->m_cmdSource != CMD_FROM_AI )
	{
		reset();
		if( parms->m_cmd != AICMD_IDLE )
		{
			m_lastOutsideCommand.store( *parms );
			m_hasOutsideCommand = TRUE;
		}

		if( m_state != DEPLOY && m_state != UNDEPLOY )
		{
			//Only issue the command if we're not in the process of deploying/undeploying.
			AIUpdateInterface::aiDoCommand( parms );
		}
		switch( parms->m_cmd )
		{
			case AICMD_GUARD_POSITION:
				m_position.set( parms->m_pos );
				m_isGuardingPosition = TRUE;
				//fall through (no break)
			case AICMD_GUARD_OBJECT:
			case AICMD_GUARD_AREA:
			case AICMD_ATTACKMOVE_TO_POSITION:
			case AICMD_HUNT:
			case AICMD_ATTACKFOLLOW_WAYPOINT_PATH:
			case AICMD_ATTACKFOLLOW_WAYPOINT_PATH_AS_TEAM:
			case AICMD_ATTACK_AREA:
				m_isAttackMultiple = TRUE;
				break;
			case AICMD_ATTACK_OBJECT:
			case AICMD_FORCE_ATTACK_OBJECT:
				m_isAttackObject = TRUE;
				m_attackObjectID = parms->m_obj ? parms->m_obj->getID() : INVALID_ID;
				break;
			case AICMD_ATTACK_POSITION:
				m_isAttackPosition = TRUE;
				m_position.set( parms->m_pos );
				break;
		}
	}
	else
	{
		//Always process AI issued commands.
		AIUpdateInterface::aiDoCommand( parms );
	}
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime DeployStyleAIUpdate::update()
{
	// have to call our parent's isIdle, because we override it to never return true
	// when we have a pending command...
	Object *self = getObject();
	Weapon *weapon = self->getCurrentWeapon();
	Bool inRange = FALSE;
	Object *designatedTarget = nullptr;
	Bool isAttacking = FALSE;

	if( weapon )
	{
		if( m_isAttackPosition )
		{
			//Handle force attacking attacking specific position
			inRange = weapon->isWithinAttackRange( self, &m_position );
			isAttacking = TRUE;
		}
		else if( m_isAttackObject )
		{
			//Handle attacking a specific object.
			designatedTarget = TheGameLogic->findObjectByID( m_attackObjectID );
			if( designatedTarget && designatedTarget->isEffectivelyDead() )
			{
				designatedTarget = nullptr;
			}
			if( designatedTarget )
			{
				inRange = weapon->isWithinAttackRange( self, designatedTarget );
				isAttacking = TRUE;
			}
		}
		else if( m_isAttackMultiple )
		{
			Bool newTarget = FALSE;
			//Handle attackmove and guard.
			//We are attacking in a different way... so attempt to figure out how.
			WhichTurretType tur = getWhichTurretForCurWeapon();
			if( tur != TURRET_INVALID )
			{
				//Get the turret's current target.
				designatedTarget = getTurretTargetObject( tur );
			}
			else
			{
				//Get the current goal object (nullptr if we have a turret).
				designatedTarget = getGoalObject();
			}
			if( !designatedTarget )
			{
				//If we still don't have a target, get the last known target.
				designatedTarget = TheGameLogic->findObjectByID( m_designatedTargetID );
			}
			if( designatedTarget && designatedTarget->isEffectivelyDead() )
			{
				//See if we can acquire one!
				designatedTarget = getNextMoodTarget( TRUE, FALSE );
				newTarget = TRUE;
			}
			if( !designatedTarget && m_isGuardingPosition )
			{
				//While guarding, acquire a target periodically.
				designatedTarget = getNextMoodTarget( FALSE, FALSE );
				if( designatedTarget )
				{
					inRange = weapon->isWithinAttackRange( self, designatedTarget );
					isAttacking = TRUE;
					if( inRange )
					{
						//Continue overriding it as long as there are enemies in range!
						aiAttackObject( designatedTarget, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
						m_overriddenAttack = TRUE;
						m_designatedTargetID = designatedTarget->getID();
					}
					else
					{
						designatedTarget = nullptr;
					}
				}
				else
				{
					designatedTarget = nullptr;
				}
			}
			else if( designatedTarget )
			{
				//Finally... we have a target -- so are we in range?
				inRange = weapon->isWithinAttackRange( self, designatedTarget );
				isAttacking = TRUE;
				m_designatedTargetID = designatedTarget->getID();
				if( m_overriddenAttack && newTarget && inRange )
				{
					//Continue overriding it as long as there are enemies in range!
					aiAttackObject( designatedTarget, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
				}
			}
			else
			{
				//No target, we must be done or moving.
				m_designatedTargetID = INVALID_ID;
			}
		}
	}

	Bool remainDeployed = m_isGuardingPosition && !designatedTarget && !isMoving() && !isWaitingForPath();

	UnsignedInt now = TheGameLogic->getFrame();
	switch( m_state )
	{
		case READY_TO_MOVE:
			if( remainDeployed || (inRange && isAttacking) )
			{
				setMyState( DEPLOY );
			}
			break;

		case READY_TO_ATTACK:
			if( !remainDeployed && ((!inRange && isAttacking) || (!isAttacking && (isWaitingForPath() || getPath()))) )
			{
				WhichTurretType tur = getWhichTurretForCurWeapon();
				if( tur != TURRET_INVALID )
				{
					if( doTurretsHaveToCenterBeforePacking() )
					{
						setMyState( ALIGNING_TURRETS );
						break;
					}
				}
				setMyState( UNDEPLOY );
			}
			else if( !designatedTarget && m_overriddenAttack && m_hasOutsideCommand )
			{
				//For multiple attacks we specifically order the unit to attack one object. After
				//object is dead, we revert to our original command.
				AICommandParms parms( AICMD_MOVE_TO_POSITION, CMD_FROM_AI );	// values don't matter, will be wiped by next line
				m_lastOutsideCommand.reconstitute( parms );
 				aiDoCommand(&parms);
			}
			break;
		case DEPLOY:
			if( m_frameToWakeForDeploy != 0 && now >= m_frameToWakeForDeploy)
			{
				setMyState( READY_TO_ATTACK );
				if( m_isAttackMultiple && inRange && isAttacking && designatedTarget )
				{
					aiAttackObject( designatedTarget, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
					m_overriddenAttack = TRUE;
				}
			}
			break;
		case UNDEPLOY:
			if( m_frameToWakeForDeploy != 0 && now >= m_frameToWakeForDeploy)
			{
				setMyState( READY_TO_MOVE );
			}
			break;
		case ALIGNING_TURRETS:
		{
			WhichTurretType tur = getWhichTurretForCurWeapon();
			if( tur != TURRET_INVALID )
			{
				if( isTurretInNaturalPosition( tur ) )
				{
					setMyState( UNDEPLOY );
				}
			}
			break;
		}
	}

	UpdateSleepTime mine = UPDATE_SLEEP_FOREVER;
	switch( m_state )
	{
		case READY_TO_ATTACK:
		case READY_TO_MOVE:
			mine = UPDATE_SLEEP_FOREVER;	// we can sleep for, well, a while
			break;

		case DEPLOY:
		case UNDEPLOY:
			mine = m_frameToWakeForDeploy > now ? UPDATE_SLEEP(m_frameToWakeForDeploy - now) : UPDATE_SLEEP_NONE;
			aiIdle( CMD_FROM_AI );
			break;

		case ALIGNING_TURRETS:
			mine = UPDATE_SLEEP_NONE;	// no sleep for us right now.
			aiIdle( CMD_FROM_AI );
			break;
	}
	UpdateSleepTime ret = AIUpdateInterface::update();
	return (mine < ret) ? mine : ret;
}

//-------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::setMyState( DeployStateTypes stateID )
{
	m_state = stateID;
	Object *self = getObject();
	switch( stateID )
	{
		case DEPLOY:
		{
			//Tell our object to deploy (so it can continue the same attack later).
			aiIdle( CMD_FROM_AI );
			self->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ),
																						 MAKE_MODELCONDITION_MASK( MODELCONDITION_UNPACKING ) );
			m_frameToWakeForDeploy = getUnpackTime(); //In frames
			//Make sure the animation matches the length of unpacking
			self->getDrawable()->setAnimationLoopDuration( m_frameToWakeForDeploy );
			m_frameToWakeForDeploy += TheGameLogic->getFrame();	// convert to absolute frame

			//Play deploy sound
			const ThingTemplate *thing = self->getTemplate();
			const AudioEventRTS* soundToPlayPtr = thing->getPerUnitSound( "Deploy" );
			if( soundToPlayPtr )
			{
				AudioEventRTS soundToPlay = *soundToPlayPtr;
				soundToPlay.setObjectID( self->getID() );
				TheAudio->addAudioEvent( &soundToPlay );
			}

			break;
		}
		case UNDEPLOY:
		{
			//Tell our object to pack up (so it can continue the same move later).
			aiIdle( CMD_FROM_AI );
			self->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_UNPACKING, MODELCONDITION_DEPLOYED ),
																						 MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ) );
			m_frameToWakeForDeploy = getPackTime(); //In frames
			//Make sure the animation matches the length of packing
			self->getDrawable()->setAnimationLoopDuration( m_frameToWakeForDeploy );
			m_frameToWakeForDeploy += TheGameLogic->getFrame();	// convert to absolute frame

			if( doTurretsFunctionOnlyWhenDeployed() )
			{
				WhichTurretType tur = getWhichTurretForCurWeapon();
				if( tur != TURRET_INVALID )
				{
					setTurretEnabled( tur, false );
				}
			}

			//Play undeploy sound
			const ThingTemplate *thing = self->getTemplate();
			const AudioEventRTS* soundToPlayPtr = thing->getPerUnitSound( "Undeploy" );
			if( soundToPlayPtr )
			{
				AudioEventRTS soundToPlay = *soundToPlayPtr;
				soundToPlay.setObjectID( self->getID() );
				TheAudio->addAudioEvent( &soundToPlay );
			}

			break;
		}
		case READY_TO_MOVE:
		{
			m_frameToWakeForDeploy = 0;

			//We're ready to move, so restore our command!
			if( m_hasOutsideCommand )
			{
				AICommandParms parms( AICMD_MOVE_TO_POSITION, CMD_FROM_AI );	// values don't matter, will be wiped by next line
				m_lastOutsideCommand.reconstitute( parms );
	 			aiDoCommand(&parms);
			}

			self->clearModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ) );
			break;
		}
		case READY_TO_ATTACK:
		{
			m_frameToWakeForDeploy = 0;

			//We're ready to attack, so restore our command!
			if( !m_isAttackMultiple && m_hasOutsideCommand )
			{
				AICommandParms parms( AICMD_MOVE_TO_POSITION, CMD_FROM_AI );	// values don't matter, will be wiped by next line
				m_lastOutsideCommand.reconstitute( parms );
 				aiDoCommand(&parms);
			}

			self->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_UNPACKING ),
																						 MAKE_MODELCONDITION_MASK( MODELCONDITION_DEPLOYED) );

			if( doTurretsFunctionOnlyWhenDeployed() )
			{
				WhichTurretType tur = getWhichTurretForCurWeapon();
				if( tur != TURRET_INVALID )
				{
					setTurretEnabled( tur, true );
				}
			}

			break;
		}
		case ALIGNING_TURRETS:
		{
			m_frameToWakeForDeploy = 0;

			WhichTurretType tur = getWhichTurretForCurWeapon();
			if( tur != TURRET_INVALID )
			{
				recenterTurret( tur );
			}
			break;
		}
	}

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::crc( Xfer *xfer )
{
	// extend base class
	AIUpdateInterface::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: Added support for attack move
	* 3: Added improved support for guard, and support for hunt AI
 **/
// ------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 3;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);

	xfer->xferBool(&m_hasOutsideCommand);
	xfer->xferUnsignedInt(&m_frameToWakeForDeploy);
	xfer->xferUser(&m_state, sizeof(m_state));

	if( version >= 2 )
	{
		xfer->xferObjectID( &m_designatedTargetID );
		xfer->xferObjectID( &m_attackObjectID );
		xfer->xferCoord3D( &m_position );
		xfer->xferBool( &m_isAttackMultiple );
		xfer->xferBool( &m_isAttackObject );
		xfer->xferBool( &m_isAttackPosition );
	}
	if( version >= 3 )
	{
		xfer->xferBool( &m_isGuardingPosition );
		xfer->xferBool( &m_overriddenAttack );
	}

	m_lastOutsideCommand.doXfer(xfer);

	if( version < 2 )
	{
		AICommandParmsStorage obsolete;
		obsolete.doXfer(xfer);
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void DeployStyleAIUpdate::loadPostProcess()
{
 // extend base class
	AIUpdateInterface::loadPostProcess();
}

