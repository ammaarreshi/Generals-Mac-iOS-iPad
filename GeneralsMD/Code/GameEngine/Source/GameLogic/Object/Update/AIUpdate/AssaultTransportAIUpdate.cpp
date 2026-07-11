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

// AssaultTransportAIUpdate.cpp ////////////
// Author: Kris Morness, December 2002
// Desc:   State machine that allows assault transports (troop crawler) to deploy
//         troops, order them to attack, then return. Can do extra things like ordering
//         injured troops to return to the transport for healing purposes.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/AssaultTransportAIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
AssaultTransportAIUpdate::AssaultTransportAIUpdate( Thing *thing, const ModuleData* moduleData ) : AIUpdateInterface( thing, moduleData )
{
	m_currentMembers = MAX_TRANSPORT_SLOTS; //First time, max it out, to ensure clearing arrays in reset.
	reset();
}

//-------------------------------------------------------------------------------------------------
void AssaultTransportAIUpdate::reset()
{
	for( int i = 0; i < m_currentMembers; i++ )
	{
		m_memberIDs[ i ] = INVALID_ID;
		m_memberHealing[ i ] = FALSE;
		m_newMember[ i ] = FALSE;
	}
	m_currentMembers = 0;
  m_attackMoveGoalPos.zero();
  m_designatedTarget = INVALID_ID;
	m_state = IDLE;
	m_framesRemaining = 0;
	m_isAttackMove = FALSE;
	m_isAttackObject = FALSE;
	m_newOccupantsAreNewMembers = FALSE;
}

//-------------------------------------------------------------------------------------------------
AssaultTransportAIUpdate::~AssaultTransportAIUpdate()
{
}

//-------------------------------------------------------------------------------------------------
void AssaultTransportAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	//Inspect the command and reset everything when necessary.
	if( parms->m_cmdSource != CMD_FROM_AI )
	{
		//Now the only time we care about anything is if we were ordered to attack something or attack move.
		switch( parms->m_cmd )
		{
			case AICMD_ATTACKMOVE_TO_POSITION:
				//Reset because we have been ordered to do something.
				reset();
				m_attackMoveGoalPos = parms->m_pos;
				m_isAttackMove = TRUE;
				break;
			case AICMD_ATTACK_OBJECT:
				//Reset because we have been ordered to do something.
				reset();
				//m_designatedTarget = parms->m_obj ? parms->m_obj->getID() : INVALID_ID;
				m_isAttackObject = TRUE;
				break;
			case AICMD_IDLE:
				m_designatedTarget = INVALID_ID;
				//Order all outside members back inside!
				retrieveMembers();
				reset();
				break;
			default:
				//Reset because we have been ordered to do something we're not handling.
				reset();
				break;
		}
	}

	//Note, in both cases, the transport will fire a dummy DEPLOY weapon that will trigger the
	//evacuation of the troops.
	AIUpdateInterface::aiDoCommand( parms );
}

//-------------------------------------------------------------------------------------------------
void AssaultTransportAIUpdate::beginAssault( const Object *designatedTarget ) const
{
	//The transport has determined it is in range to begin the assault (via weapon system).
	//Now order the evacuation of healthy troops, and let the update handle moving them.
	if( designatedTarget )
	{
		m_designatedTarget = designatedTarget->getID();
	}
}

//-------------------------------------------------------------------------------------------------
Bool AssaultTransportAIUpdate::isIdle() const
{
	return AIUpdateInterface::isIdle();
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime calcSleepTime()
{
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime AssaultTransportAIUpdate::update()
{
	Object *transport = getObject();
	//const AssaultTransportAIUpdateModuleData *data = getAssaultTransportAIUpdateModuleData();

	if( transport->isEffectivelyDead() )
	{
		giveFinalOrders();
		return UPDATE_SLEEP_FOREVER;
	}

	//First removing dead members or members that have been ordered to do something outside of this AI.
	if( m_currentMembers )
	{
		for( int i = 0; i < m_currentMembers; i++ )
		{
			Object *member = TheGameLogic->findObjectByID( m_memberIDs[ i ] );
			AIUpdateInterface *ai = member ? member->getAI() : nullptr;
			if( !member || member->isEffectivelyDead() || ai->getLastCommandSource() != CMD_FROM_AI )
			{
				//Member is toast -- so remove him from our list!
				if( m_currentMembers - 1 > i )
				{
					//Move the last slot to this slot to keep array contiguous.
					m_memberIDs[ i ]			= m_memberIDs[ m_currentMembers - 1 ];
					m_memberHealing[ i ]	= m_memberHealing[ m_currentMembers - 1 ];
					m_newMember[ i ]			= m_newMember[ m_currentMembers - 1 ];
				}
				else
				{
					//Just clean out last slot.
					m_memberIDs[ i ]			= INVALID_ID;
					m_memberHealing[ i ]	= FALSE;
					m_newMember[ i ]			= FALSE;
				}
				if( ai )
				{
					//Important! Members of our assault transport must be allowed to chase down designated enemies.
					//Generally only player commands allow this, so this flag allows AI commands to do the same.
					//We need to turn this off though, because this ex-member is no longer under transport control.
					ai->setAllowedToChase( FALSE );
				}
				m_currentMembers--;
			}
		}
	}

	//Now add any potentially new members to the group.
	ContainModuleInterface *contain = transport->getContain();
	if( contain )
	{
		const ContainedItemsList *passengerList = contain->getContainedItemsList();
		ContainedItemsList::const_iterator passengerIterator;
		passengerIterator = passengerList->begin();
		while( passengerIterator != passengerList->end() )
		{
			Object *passenger = *passengerIterator;
			//Advance to the next iterator
			passengerIterator++;

			//Make sure it isn't in our list already.
			Bool found = FALSE;
			for( int i = 0; i < m_currentMembers; i++ )
			{
				if( passenger->getID() == m_memberIDs[ i ] )
				{
					//He is in the list... so skip him.
					found = TRUE;
					break;
				}
			}
			if( found )
			{
				//Get next passenger.
				continue;
			}

			//It's possible to add members manually -- but if we already have 10 members, then wait!
			if( m_currentMembers < MAX_TRANSPORT_SLOTS )
			{
				//Not in list, so add him!
				m_memberIDs[ m_currentMembers ] = passenger->getID();
				if( passenger->getAI() )
				{
					//Important! Members of our assault transport must be allowed to chase down designated enemies.
					//Generally only player commands allow this, so this flag allows AI commands to do the same.
					passenger->getAI()->setAllowedToChase( TRUE );
				}

				//Check if the passenger is wounded below threshold (if so make sure we heal him before ordering him to fight!)
				if( isMemberWounded( passenger ) )
				{
					m_memberHealing[ m_currentMembers ] = TRUE;
				}
				if( m_newOccupantsAreNewMembers )
				{
					//New members won't eject out until a new attack order is issued.
					m_newMember[ m_currentMembers ] = TRUE;
				}

				m_currentMembers++;
			}
		}
		m_newOccupantsAreNewMembers = TRUE;
	}

	if( isAttackPointless() )
	{
		aiIdle( CMD_FROM_AI );
		return UPDATE_SLEEP_NONE;
	}

	//Keep track of the average position of all combat units assigned to me.
	Coord3D fighterCentroidPos;
	UnsignedInt fightingMembers = 0;
	fighterCentroidPos.zero();

	//If we're already in the process, reacquire the designated target again... see if
	//it's still alive.
	Object *designatedTarget = TheGameLogic->findObjectByID( m_designatedTarget );
	if( designatedTarget && designatedTarget->isEffectivelyDead() )
	{
		designatedTarget = nullptr;
	}
	if( designatedTarget )
	{
		//Look for members not currently attacking this target.
		for( int i = 0; i < m_currentMembers; i++ )
		{
			Object *member = TheGameLogic->findObjectByID( m_memberIDs[ i ] );
			AIUpdateInterface *ai = member ? member->getAI() : nullptr;

			if( member && ai )
			{
				Bool contained = member->isContained();
				Bool wounded = isMemberWounded( member );
				if( contained && isMemberHealthy( member ) && !m_newMember[ i ] )
				{
					//This contained member is healthy so order him to exit to start fighting!
					//New members are exempt!
					ai->aiExit( transport, CMD_FROM_AI );
				}
				if( !contained )
				{
					if( wounded )
					{
						if( ai->getAIStateType() != AI_ENTER )
						{
							//Order wounded members back to get healed.
							ai->aiEnter( transport, CMD_FROM_AI );
						}
					}
					else
					{
						//Increment the number of fighters and their position.
						fighterCentroidPos.add( *member->getPosition() );
						fightingMembers++;

						if( !ai->isMoving() )
						{
							if( ai->getGoalObject() != designatedTarget )
							{
								//Okay, this dude is outside and waiting... order him to attack the designated target
								ai->aiAttackObject( designatedTarget, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
							}
						}
					}
				}
			}
		}
	}
	else
	{
		if( m_isAttackMove && getAIStateType() != AI_ATTACK_MOVE_TO )
		{
			//Continue to move towards the attackmove area.
			aiAttackMoveToPosition( &m_attackMoveGoalPos, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
		}
		else if( m_isAttackObject )
		{
			retrieveMembers();
		}
	}

	/*
	//Keep near the troops.
	if( !m_framesRemaining )
	{
		if( !isMoving() && fightingMembers && designatedTarget )
		{
			m_framesRemaining = 45;

			//Get centroid pos now that we know the number of fighting members.
			Real scale = 1.0f / (Real)fightingMembers;
			fighterCentroidPos.scale( scale );

			Coord3D designatedTargetPos = *designatedTarget->getPosition();

			//Calculate a vector from the target passed the fighters to be at a safe place
			//to be as a transport.
			Coord3D vector;
			vector.set( fighterCentroidPos );
			vector.sub( designatedTargetPos );
			vector.normalize();
			vector.scale( 150.0f );

			Coord3D transportGoalPos;
			transportGoalPos.set( designatedTargetPos );
			transportGoalPos.add( vector );

			Real distanceSqrd = ThePartitionManager->getDistanceSquared( transport, &transportGoalPos, FROM_CENTER_2D );
			if( distanceSqrd > 40.0f * 40.0f )
			{
				//Order the transport to move to the safer position
				//aiMoveToPosition( &transportGoalPos, CMD_FROM_AI );
			}
		}
	}
	else
	{
		m_framesRemaining--;
	}
	if( designatedTarget && !isMoving() )
	{
		//Order the transport to face the designated target!
		//aiFaceObject( designatedTarget, CMD_FROM_AI );
	}
	*/

	/*UpdateSleepTime ret =*/ AIUpdateInterface::update();
	//return (mine < ret) ? mine : ret;
	/// @todo srj -- someday, make sleepy. for now, must not sleep.
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
Bool AssaultTransportAIUpdate::isAttackPointless() const
{
	//If all members are new members (thus can't attack), and the transport itself
	//is still attacking, stop!
	const Object *transport = getObject();
	if( transport->testStatus( OBJECT_STATUS_IS_ATTACKING ) )
	{
		for( int i = 0; i < m_currentMembers; i++ )
		{
			if( !m_newMember[ i ] )
			{
				//We have a non-new member, so attack is valid.
				return FALSE;
			}
		}

		//We are trying to attack, but can't because all our members are new.
		return TRUE;
	}

	//We aren't trying to attack, so everything is good.
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool AssaultTransportAIUpdate::isMemberWounded( const Object *member ) const
{
	const AssaultTransportAIUpdateModuleData *data = getAssaultTransportAIUpdateModuleData();
	BodyModuleInterface *body = member->getBodyModule();
	if( body )
	{
		Real ratio = body->getHealth() / body->getMaxHealth();
		if( ratio < data->m_membersGetHealedAtLifeRatio )
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool AssaultTransportAIUpdate::isMemberHealthy( const Object *member ) const
{
	BodyModuleInterface *body = member->getBodyModule();
	if( body )
	{
		if( body->getHealth() == body->getMaxHealth() )
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void AssaultTransportAIUpdate::retrieveMembers()
{
	//Order all outside members back inside!
	for( int i = 0; i < m_currentMembers; i++ )
	{
		Object *member = TheGameLogic->findObjectByID( m_memberIDs[ i ] );
		AIUpdateInterface *ai = member ? member->getAI() : nullptr;
		if( member && ai )
		{
			Bool contained = member->isContained();
			if( !contained )
			{
				//This contained member is healthy so order him to exit to start fighting!
				if (ai->getAIStateType() != AI_ENTER) {
					ai->aiEnter( getObject(), CMD_FROM_AI );
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void AssaultTransportAIUpdate::giveFinalOrders()
{
	//All members have been ejected outside already -- transfer the original order to the troops
	for( int i = 0; i < m_currentMembers; i++ )
	{
		Object *member = TheGameLogic->findObjectByID( m_memberIDs[ i ] );
		AIUpdateInterface *ai = member ? member->getAI() : nullptr;
		if( member && ai )
		{
			Object *designatedTarget = TheGameLogic->findObjectByID( m_designatedTarget );

			if( m_isAttackObject && designatedTarget )
			{
				ai->aiAttackObject( designatedTarget, NO_MAX_SHOTS_LIMIT, CMD_FROM_PLAYER );
			}
			else if( m_isAttackMove )
			{
				ai->aiAttackMoveToPosition( &m_attackMoveGoalPos, NO_MAX_SHOTS_LIMIT, CMD_FROM_PLAYER );
			}

			ai->setAllowedToChase( FALSE );
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** CRC */
//-------------------------------------------------------------------------------------------------
void AssaultTransportAIUpdate::crc( Xfer *xfer )
{
	// extend base class
	AIUpdateInterface::crc(xfer);
}

//-------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
//-------------------------------------------------------------------------------------------------
void AssaultTransportAIUpdate::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);

	xfer->xferInt( &m_currentMembers );

	for( int i = 0; i < m_currentMembers; i++ )
	{
		xfer->xferObjectID( &(m_memberIDs[ i ]) );
		xfer->xferBool( &(m_memberHealing[ i ]) );
	}

	xfer->xferCoord3D( &m_attackMoveGoalPos );
	xfer->xferObjectID( &m_designatedTarget );

	Int state = (Int)m_state;
	xfer->xferInt( &state );
	m_state = (AssaultStateTypes)state;

	xfer->xferUnsignedInt( &m_framesRemaining );
	xfer->xferBool( &m_isAttackMove );
	xfer->xferBool( &m_isAttackObject );

}

//-------------------------------------------------------------------------------------------------
/** Load post process */
//-------------------------------------------------------------------------------------------------
void AssaultTransportAIUpdate::loadPostProcess()
{
 // extend base class
	AIUpdateInterface::loadPostProcess();
}

