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

// DeliverPayloadAIUpdate.cpp ////////////
// Author: Graham Smallwood, March 2002
// Desc:   State machine that controls the approach and deployment of airborne cargo
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_WEAPONSLOTTYPE_NAMES

#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/SmartBombTargetHomingUpdate.h"
#include "GameLogic/Module/GenerateMinefieldBehavior.h"
#include "GameLogic/Module/DeliverPayloadAIUpdate.h"
#include "GameLogic/Module/ParachuteContain.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"


//-------------------------------------------------------------------------------------------------
const FieldParse* DeliverPayloadData::getFieldParse()
{
	static const FieldParse dataFieldParse[] =
	{

		{ "DeliveryDistance",								INI::parseReal,								nullptr, offsetof( DeliverPayloadData, m_distToTarget) },
		{ "PreOpenDistance",								INI::parseReal,								nullptr, offsetof( DeliverPayloadData, m_preOpenDistance) },
		{ "MaxAttempts",										INI::parseInt,								nullptr, offsetof( DeliverPayloadData, m_maxAttempts) },

		//Drop information
		{ "DropDelay",											INI::parseDurationUnsignedInt,nullptr, offsetof( DeliverPayloadData, m_dropDelay ) },
		{ "DropOffset",											INI::parseCoord3D,						nullptr, offsetof( DeliverPayloadData, m_dropOffset ) },
		{ "DropVariance",										INI::parseCoord3D,						nullptr, offsetof( DeliverPayloadData, m_dropVariance ) },
		{ "InheritTransportVelocity",				INI::parseBool,								nullptr, offsetof( DeliverPayloadData, m_inheritTransportVelocity ) },
		{ "ExitPitchRate",									INI::parseAngularVelocityReal,nullptr, offsetof( DeliverPayloadData, m_exitPitchRate ) },
		{ "ParachuteDirectly",							INI::parseBool,								nullptr, offsetof( DeliverPayloadData, m_isParachuteDirectly) },

		//Visible payload information (payload assumed to be show visibly and it's created only when dropped)
		{ "VisibleItemsDroppedPerInterval",	INI::parseInt,								nullptr, offsetof( DeliverPayloadData, m_visibleItemsDroppedPerInterval ) },
		{ "VisibleDropBoneBaseName",				INI::parseAsciiString,				nullptr, offsetof( DeliverPayloadData, m_visibleDropBoneName ) },
		{ "VisibleSubObjectBaseName",				INI::parseAsciiString,				nullptr, offsetof( DeliverPayloadData, m_visibleSubObjectName ) },
		{ "VisibleNumBones",								INI::parseInt,								nullptr, offsetof( DeliverPayloadData, m_visibleNumBones ) },
		{ "VisiblePayloadTemplateName",			INI::parseAsciiString,				nullptr, offsetof( DeliverPayloadData, m_visiblePayloadTemplateName ) },
		{ "VisiblePayloadWeaponTemplate",		INI::parseWeaponTemplate,			nullptr, offsetof( DeliverPayloadData, m_visiblePayloadWeaponTemplate ) },
		{ "SelfDestructObject",		          INI::parseBool,		           	nullptr, offsetof( DeliverPayloadData, m_selfDestructObject ) },

		//Weapon based payload
		{ "FireWeapon",											INI::parseBool,								nullptr, offsetof( DeliverPayloadData, m_fireWeapon ) },

		//Specify an additional weaponslot to be fired while strafing
		{ "DiveStartDistance",							INI::parseReal,								nullptr, offsetof( DeliverPayloadData, m_diveStartDistance ) },
		{ "DiveEndDistance",								INI::parseReal,								nullptr, offsetof( DeliverPayloadData, m_diveEndDistance ) },
		{ "StrafingWeaponSlot",							INI::parseLookupList,					TheWeaponSlotTypeNamesLookupList, offsetof( DeliverPayloadData, m_strafingWeaponSlot ) },
		{ "StrafeWeaponFX",									INI::parseFXList,							nullptr, offsetof( DeliverPayloadData, m_strafeFX ) },
		{ "StrafeLength",										INI::parseReal,								nullptr, offsetof( DeliverPayloadData, m_strafeLength ) },

		{ "DeliveryDecal",									RadiusDecalTemplate::parseRadiusDecalTemplate,	nullptr, offsetof( DeliverPayloadData, m_deliveryDecalTemplate ) },
		{ "DeliveryDecalRadius",						INI::parseReal, nullptr, offsetof(DeliverPayloadData, m_deliveryDecalRadius) },

		{ nullptr, nullptr, nullptr, 0 }
	};
	return dataFieldParse;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
AIStateMachine* DeliverPayloadAIUpdate::makeStateMachine()
{
	return newInstance(AIStateMachine)( getObject(), "DeliverPayloadBasicAI");
}

//-------------------------------------------------------------------------------------------------
DeliverPayloadAIUpdate::DeliverPayloadAIUpdate( Thing *thing, const ModuleData* moduleData ) : AIUpdateInterface( thing, moduleData )
{
	m_deliverPayloadStateMachine = nullptr;
	m_targetPos.zero();
	m_moveToPos.zero();
	m_visibleItemsDelivered = 0;
	m_deliveryDecal.clear();
	m_previousDistanceSqr = 0;
	m_freeToExit = FALSE;
	m_acceptingCommands = TRUE;
	m_diveState = DIVESTATE_PREDIVE;
}

//-------------------------------------------------------------------------------------------------
DeliverPayloadAIUpdate::~DeliverPayloadAIUpdate()
{
	m_deliveryDecal.clear();
	deleteInstance(m_deliverPayloadStateMachine);
}

//-------------------------------------------------------------------------------------------------
AIFreeToExitType DeliverPayloadAIUpdate::getAiFreeToExit(const Object* exiter) const
{
	if( getObject()->isEffectivelyDead() )
		return NOT_FREE_TO_EXIT;

	return m_freeToExit ? FREE_TO_EXIT : NOT_FREE_TO_EXIT;
}

//-------------------------------------------------------------------------------------------------
void DeliverPayloadAIUpdate::killDeliveryDecal()
{
	m_deliveryDecal.clear();
}

//-------------------------------------------------------------------------------------------------
Bool DeliverPayloadAIUpdate::isAllowedToRespondToAiCommands(const AICommandParms* parms) const
{
	if (!m_acceptingCommands)
	{
		return false;
	}

	return AIUpdateInterface::isAllowedToRespondToAiCommands(parms);
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime DeliverPayloadAIUpdate::update()
{
	m_deliveryDecal.update();

	if(!(isAiInDeadState()) && m_deliverPayloadStateMachine)
		m_deliverPayloadStateMachine->updateStateMachine();

	//Handling diving logic (regardless of state)
	if( m_diveState != DIVESTATE_POSTDIVE )
	{
		if( m_diveState == DIVESTATE_PREDIVE )
		{
			//Check to see if we are close enough to start diving.
			Real startDiveDistanceSquared = sqr( getData()->m_diveStartDistance );
			Real currentDistanceSquared  = ThePartitionManager->getDistanceSquared( getObject(), getTargetPos(), FROM_CENTER_2D );
			if( currentDistanceSquared <= startDiveDistanceSquared )
			{
				m_diveState = DIVESTATE_DIVING;
				getObject()->getAIUpdateInterface()->getCurLocomotor()->setUsePreciseZPos( true );

				AudioEventRTS soundDive = *(getObject()->getTemplate()->getPerUnitSound("StartDive"));
				if( soundDive.getEventName().isNotEmpty() )
				{
					soundDive.setPosition( getObject()->getPosition() );
					TheAudio->addAudioEvent( &soundDive );
				}
			}
		}
		else
		{
			//Check to see when we shall end diving
			Real endDiveDistanceSquared = sqr( getData()->m_diveEndDistance );
			Real currentDistanceSquared  = ThePartitionManager->getDistanceSquared( getObject(), getTargetPos(), FROM_CENTER_3D );
			if( currentDistanceSquared <= endDiveDistanceSquared )
			{
				m_diveState = DIVESTATE_POSTDIVE;
				getObject()->getAIUpdateInterface()->getCurLocomotor()->setUsePreciseZPos( false );
			}

			if( m_data.m_strafingWeaponSlot != -1 )
			{
				//SHOOT!!
				const Coord3D *velocity = getObject()->getPhysics()->getVelocity();
				if( velocity->z < 5.0f )
				{
					//Calc strafe ratio
					Real startDiveDistance = getData()->m_diveStartDistance;
					Real endDiveDistance = WWMath::SqrtOrigin( endDiveDistanceSquared );
					Real currentDistance = WWMath::SqrtOrigin( currentDistanceSquared );

					Real diveRatio = (startDiveDistance - currentDistance) / (startDiveDistance - endDiveDistance);

					Coord3D velocity = *getObject()->getPhysics()->getVelocity();
					velocity.z = 0.0f;
					velocity.normalize();
					velocity.scale( diveRatio * 100.0f );

					Coord3D backwards = velocity;
					backwards.scale( 0.33f );

					Coord3D strafePoint = *getTargetPos();
					strafePoint.sub( backwards );

					strafePoint.add( velocity );
					strafePoint.z = TheTerrainLogic->getGroundHeight( strafePoint.x, strafePoint.y );

					// lock it just till the weapon is empty or the attack is "done"
					getObject()->setWeaponLock( m_data.m_strafingWeaponSlot, LOCKED_TEMPORARILY );
					getObject()->fireCurrentWeapon( &strafePoint );
					FXList::doFXPos( m_data.m_strafeFX, &strafePoint );
				}
			}
		}
	}

	AIUpdateInterface::update();

	return UPDATE_SLEEP_NONE;	// ignore our parent, and never sleep
}

//-------------------------------------------------------------------------------------------------
void DeliverPayloadAIUpdate::deliverPayload(
	const Coord3D *moveToPos,
	const Coord3D *targetPos,
	const DeliverPayloadData *data
)
{

	//****************************************************
	//THIS GETS CALLED VIA OBJECT CREATION LISTS ONLY!!!
	//****************************************************

	deleteInstance(m_deliverPayloadStateMachine);
	m_deliverPayloadStateMachine = nullptr;

	m_moveToPos = *moveToPos;
	m_targetPos = *targetPos;
	m_data			= *data;

	m_deliveryDecal.clear();
	m_data.m_deliveryDecalTemplate.createRadiusDecal(*targetPos,
		m_data.m_deliveryDecalRadius, getObject()->getControllingPlayer(), m_deliveryDecal);

	if( m_data.m_diveStartDistance <= 0.0f )
	{
		//We can't dive, so setting it to post-dive will prevent
		//dive logic from being processed.
		m_diveState = DIVESTATE_POSTDIVE;
	}
	else
	{
		m_diveState = DIVESTATE_PREDIVE;
	}

	m_visibleItemsDelivered = 0;

	//If we have visible bones, then show each of those subobjects.
	Drawable *draw = getObject()->getDrawable();
	if( draw )
	{
		for( int i = 1; i <= m_data.m_visibleNumBones; i++ )
		{
			if( m_data.m_visibleSubObjectName.isNotEmpty() )
			{
				AsciiString name;
				name.format( "%s%02d", m_data.m_visibleSubObjectName.str(), i );
				draw->showSubObject( name, true );
			}
		}
	}


	// must make the state machine AFTER initing the other stuff, since it may inquire of its values...
	m_deliverPayloadStateMachine = newInstance(DeliverPayloadStateMachine)( getObject() );
	m_deliverPayloadStateMachine->initDefaultState();
#ifdef RTS_DEBUG
	m_deliverPayloadStateMachine->setName("DeliverPayloadSpecificAI");
#endif
}

//-------------------------------------------------------------------------------------------------
void DeliverPayloadAIUpdate::deliverPayloadViaModuleData( const Coord3D *moveToPos )
{
	const DeliverPayloadAIUpdateModuleData *data = getDeliverPayloadAIUpdateModuleData();

	//****************************************************
	//THIS GETS CALLED FOR SCRIPTED REINFORCEMENTS ONLY!!!
	//These don't and can't use object creation lists.
	//****************************************************

	DeliverPayloadData dpData;
	dpData.m_dropOffset.set( data->m_dropOffset );
	dpData.m_dropVariance.set( data->m_dropVariance );
	dpData.m_distToTarget						= data->m_maxDistanceToTarget;
	dpData.m_maxAttempts						= data->m_maxNumberAttempts;
	dpData.m_dropDelay							= data->m_dropDelay;
	dpData.m_deliveryDecalTemplate	= data->m_deliveryDecalTemplate;
	dpData.m_deliveryDecalRadius		= data->m_deliveryDecalRadius;

	deliverPayload( moveToPos, moveToPos, &dpData );
}

//-------------------------------------------------------------------------------------------------
const ThingTemplate* DeliverPayloadAIUpdate::getPutInContainerTemplateViaModuleData() const
{
	AsciiString name = getDeliverPayloadAIUpdateModuleData()->m_putInContainerName;
	return name.isEmpty() ? nullptr : TheThingFactory->findTemplate( name );
}

//-------------------------------------------------------------------------------------------------
Real DeliverPayloadAIUpdate::calcMinTurnRadius(Real* timeToTravelThatDist) const
{
	const Locomotor* loco = getCurLocomotor();
	BodyDamageType bdt = getObject()->getBodyModule()->getDamageState();
	/// @todo srj -- this should probably use min-speed, not max-speed... fix after E3
	Real maxSpeed = loco->getMaxSpeedForCondition(bdt);				// in dist/frame
	Real maxTurnRate = loco->getMaxTurnRate(bdt);	// in rads/frame

	/*
		our minimum circumference will be like so:

		Real minTurnCircum = maxSpeed * (2*PI / maxTurnRate);

		so therefore our minimum turn radius is:

		Real minTurnRadius = minTurnCircum / 2*PI;

		so we just eliminate the middleman:
	*/
	Real minTurnRadius = (maxTurnRate > 0.0f) ? (maxSpeed / maxTurnRate) : 999999.0f;

	if (timeToTravelThatDist)
		*timeToTravelThatDist = minTurnRadius / maxSpeed;

	return minTurnRadius;
}


//-------------------------------------------------------------------------------------------------
Bool DeliverPayloadAIUpdate::isCloseEnoughToTarget()
{
	// In addition to testing distance, it is also sensitive to being in/outward bound
////The new getPreOpenDistance() allows the deliver state to fire early, but only if inbound,
////so the doors can open and payload can get ready...

	Real allowedDistanceSqr = sqr( getAllowedDistanceToTarget() );
	Real currentDistanceSqr = ThePartitionManager->getDistanceSquared( getObject(), getTargetPos(), FROM_CENTER_2D );
	Bool inBound = m_previousDistanceSqr > currentDistanceSqr;
	m_previousDistanceSqr = currentDistanceSqr;// for the next test

	if ( inBound )
		allowedDistanceSqr = sqr(getAllowedDistanceToTarget() + getPreOpenDistance());

	//DEBUG_LOG(("Dist to target is %f (allowed %f)",WWMath::SqrtOrigin(currentDistanceSqr),WWMath::SqrtOrigin(allowedDistanceSqr)));


	if ( allowedDistanceSqr > currentDistanceSqr )
		return TRUE;

	return FALSE;

}

//-------------------------------------------------------------------------------------------------
Bool DeliverPayloadAIUpdate::isOffMap() const
{
	Region3D mapRegion;
	TheTerrainLogic->getExtentIncludingBorder( &mapRegion );

	if (!mapRegion.isInRegionNoZ( *getObject()->getPosition() ))
		return true;

	return false;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void DeliverPayloadAIUpdate::crc( Xfer *xfer )
{
 // extend base class
	AIUpdateInterface::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 5: Whoops version.  preOpenDistance just wasn't saved.
*/
// ------------------------------------------------------------------------------------------------
void DeliverPayloadAIUpdate::xfer( Xfer *xfer )
{
  // version
  const XferVersion currentVersion = 5;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);

	xfer->xferCoord3D(&m_targetPos);
	xfer->xferCoord3D(&m_moveToPos);
	xfer->xferInt(&m_visibleItemsDelivered);
	xfer->xferUser(&m_diveState, sizeof(m_diveState));

	DeliverPayloadData data = m_data;

	xfer->xferAsciiString(&data.m_visibleDropBoneName);
	xfer->xferAsciiString(&data.m_visibleSubObjectName);
	xfer->xferAsciiString(&data.m_visiblePayloadTemplateName);
  xfer->xferReal(&data.m_distToTarget);
	if( version >= 5 )
		xfer->xferReal(&data.m_preOpenDistance);
	xfer->xferInt(&data.m_maxAttempts);
	xfer->xferCoord3D(&data.m_dropOffset);
	xfer->xferCoord3D(&data.m_dropVariance);
	xfer->xferUnsignedInt(&data.m_dropDelay);
	xfer->xferBool(&data.m_fireWeapon);
	xfer->xferBool(&data.m_selfDestructObject);
	xfer->xferInt(&data.m_visibleNumBones);
	xfer->xferReal(&data.m_diveStartDistance);
	xfer->xferReal(&data.m_diveEndDistance);
	xfer->xferUser(&data.m_strafingWeaponSlot, sizeof(data.m_strafingWeaponSlot));
	xfer->xferInt(&data.m_visibleItemsDroppedPerInterval);
	xfer->xferBool(&data.m_inheritTransportVelocity);
	xfer->xferBool(&data.m_isParachuteDirectly);
	xfer->xferReal(&data.m_exitPitchRate);
	// const FXList					*m_strafeFX;
	xfer->xferReal(&data.m_strafeLength);
	AsciiString weaponTemplateName;
	if (data.m_visiblePayloadWeaponTemplate)
	{
		weaponTemplateName = data.m_visiblePayloadWeaponTemplate->getName();
	}
	xfer->xferAsciiString(&weaponTemplateName);
	if( xfer->getXferMode() == XFER_LOAD && weaponTemplateName.isNotEmpty())
	{
		data.m_visiblePayloadWeaponTemplate = TheWeaponStore->findWeaponTemplate(weaponTemplateName);
	}
	data.m_deliveryDecalTemplate.xferRadiusDecalTemplate(xfer);
	xfer->xferReal(&data.m_deliveryDecalRadius);
	 *((DeliverPayloadData*)&m_data) = data;

	Bool hasStateMachine = m_deliverPayloadStateMachine!=nullptr;
	xfer->xferBool(&hasStateMachine);
	if (hasStateMachine && m_deliverPayloadStateMachine==nullptr)
	{
		m_deliverPayloadStateMachine = newInstance(DeliverPayloadStateMachine)( getObject() );
	}
	if (m_deliverPayloadStateMachine && version >= 2)
	{
		xfer->xferSnapshot(m_deliverPayloadStateMachine);
	}

	m_deliveryDecal.xferRadiusDecal(xfer);

	if (version >= 2)
	{
		xfer->xferBool(&m_freeToExit);
	}
	if (version >= 3)
	{
		xfer->xferBool(&m_acceptingCommands);
	}
	if (version >= 4)
	{
		xfer->xferReal(&m_previousDistanceSqr);
	}


}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void DeliverPayloadAIUpdate::loadPostProcess()
{
 // extend base class
	AIUpdateInterface::loadPostProcess();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
DeliverPayloadStateMachine::DeliverPayloadStateMachine( Object *owner ) : StateMachine( owner, "DeliverPayloadStateMachine" )
{
//	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();

	static const StateConditionInfo considerConditions[] =
	{
		StateConditionInfo(DeliverPayloadStateMachine::isOffMap, RECOVER_FROM_OFF_MAP, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	// order matters: first state is the default state.
	defineState( APPROACH,								newInstance(ApproachState)( this ), DELIVERING, CONSIDER_NEW_APPROACH );
	defineState( DELIVERING,							newInstance(DeliveringState)( this ), HEAD_OFF_MAP, CONSIDER_NEW_APPROACH );
	defineState( CONSIDER_NEW_APPROACH,		newInstance(ConsiderNewApproachState)( this ), APPROACH, HEAD_OFF_MAP, considerConditions );
	defineState( RECOVER_FROM_OFF_MAP,		newInstance(RecoverFromOffMapState)( this ), APPROACH, APPROACH );
	defineState( HEAD_OFF_MAP,						newInstance(HeadOffMapState)( this ), CLEAN_UP, CLEAN_UP );
	defineState( CLEAN_UP,								newInstance(CleanUpState)( this ), INVALID_STATE_ID, INVALID_STATE_ID );
}

//-------------------------------------------------------------------------------------------------
DeliverPayloadStateMachine::~DeliverPayloadStateMachine()
{
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void DeliverPayloadStateMachine::crc( Xfer *xfer )
{
	StateMachine::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void DeliverPayloadStateMachine::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );

	StateMachine::xfer(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void DeliverPayloadStateMachine::loadPostProcess()
{
	StateMachine::loadPostProcess();
}

//-------------------------------------------------------------------------------------------------
/*static*/ Bool DeliverPayloadStateMachine::isOffMap( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return TRUE;
	}
	return ai->isOffMap();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
StateReturnType ApproachState::onEnter() // Give the move command
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}
	if (ai->isAiInDeadState())
	{
		return STATE_FAILURE;
	}
 	ai->aiMoveToPosition( ai->getMoveToPos(), CMD_FROM_AI );

	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
StateReturnType ApproachState::update()
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

	if (ai->isAiInDeadState())
	{
		return STATE_FAILURE;
	}

	if (ai->isCloseEnoughToTarget())
		return STATE_SUCCESS;

	if (ai->getAIStateType() != AI_MOVE_TO)
	{
    if ( ai->getAIStateType() == AI_IDLE )
    // Because something outside us has told us to IDLE, so geez, ignore it and try our approach again
      return STATE_FAILURE;
    else
    {
		  DEBUG_CRASH(("hmm, bailed from moveto state early... should this be possible?"));
		  ai->aiMoveToPosition( ai->getMoveToPos(), CMD_FROM_AI );
    }

	}

	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void DeliveringState::crc( Xfer *xfer )
{
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void DeliveringState::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

	xfer->xferUnsignedInt(&m_dropDelayLeft);
	xfer->xferBool(&m_didOpen);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void DeliveringState::loadPostProcess()
{
}


//-------------------------------------------------------------------------------------------------
StateReturnType DeliveringState::onEnter() // Open the pod bay doors, Hal
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

				/// @todo srj -- for now, this assumes at most one door
	owner->clearAndSetModelConditionState( MODELCONDITION_DOOR_1_CLOSING, MODELCONDITION_DOOR_1_OPENING );
	m_dropDelayLeft = ai->getDoorDelay();
	m_didOpen = false;
	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
StateReturnType DeliveringState::update() // Kick a dude out every so often
{
	if (m_dropDelayLeft > 0)
	{
		--m_dropDelayLeft;
		return STATE_CONTINUE;
	}

	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

	ai->friend_setFreeToExit(true);

	m_didOpen = true;
	m_dropDelayLeft = ai->getDropDelay();

	if (!ai->isCloseEnoughToTarget())
		return STATE_FAILURE;

	const ContainedItemsList* items = owner->getContain() ? owner->getContain()->getContainedItemsList() : nullptr;
	if( (!items || !items->size()) && ai->getVisibleItemsDelivered() == ai->getData()->m_visibleNumBones )
	{
		//We are out of payload to drop AND our visible payload is empty. It's possible for deliverers to
		//have one or the other or even both.
		return STATE_SUCCESS;
	}

	//Handle contained payload delivery.
	if( items )
	{
		Object* item = items->front();

		if( ai->isFireWeapon() )
		{
			Coord3D pos = *ai->getTargetPos();
			pos.x += ai->getDropOffset().x;
			pos.y += ai->getDropOffset().y;
			pos.z += ai->getDropOffset().z;
			owner->fireCurrentWeapon( &pos );
			TheGameLogic->destroyObject( item );
		}
		else
		{
			AIUpdateInterface* itemAI = item->getAIUpdateInterface();
			if (itemAI)
			{
				itemAI->aiExit(nullptr, CMD_FROM_AI);
			}
			Coord3D pos = *item->getPosition();

			if (ai->getDropVariance().x > 0)
				pos.x += GameLogicRandomValueReal(-ai->getDropVariance().x, ai->getDropVariance().x);
			if (ai->getDropVariance().y > 0)
				pos.y += GameLogicRandomValueReal(-ai->getDropVariance().y, ai->getDropVariance().y);
			if (ai->getDropVariance().z > 0)
				pos.z += GameLogicRandomValueReal(-ai->getDropVariance().z, ai->getDropVariance().z);

			pos.x += ai->getDropOffset().x;
			pos.y += ai->getDropOffset().y;
			pos.z += ai->getDropOffset().z;
			item->setPosition(&pos);

			ContainModuleInterface *contain = item->getContain();
			if( ai->getData()->m_isParachuteDirectly  &&  contain )
			{
				contain->setOverrideDestination( ai->getTargetPos() );// go for the direct hit
			}
			else if (itemAI)
			{
				// This will most likely be ignored, as the item is still in the transport, and therefore Held.
				itemAI->aiMoveToPosition( ai->getMoveToPos(), CMD_FROM_AI );
			}

			/// @todo srj -- urg. icky.
			static NameKeyType key_GenerateMinefieldBehavior = NAMEKEY("GenerateMinefieldBehavior");
			GenerateMinefieldBehavior* mfb = (GenerateMinefieldBehavior *)item->findUpdateModule(key_GenerateMinefieldBehavior);
			if (mfb)
			{
				mfb->setMinefieldTarget(ai->getMoveToPos());
			}


			static NameKeyType key_SmartBombTargetHomingUpdate = NAMEKEY("SmartBombTargetHomingUpdate");
			SmartBombTargetHomingUpdate* smthu = (SmartBombTargetHomingUpdate *)item->findUpdateModule(key_SmartBombTargetHomingUpdate);
			if (smthu)
			{
				smthu->SetTargetPosition( *ai->getMoveToPos() );
			}



			if( ai->getData()->m_inheritTransportVelocity )
			{
				Coord3D velocity = *owner->getPhysics()->getVelocity();
				item->getPhysics()->applyForce( &velocity );
			}
		}
	}

	//Handle visible payload delivery.
	if( ai->getVisibleItemsDelivered() < ai->getData()->m_visibleNumBones )
	{
		Int attemptDrops = ai->getData()->m_visibleItemsDroppedPerInterval;
		Drawable *draw = owner->getDrawable();
		Bool updateSubObjects = false;
		while( attemptDrops && ai->getVisibleItemsDelivered() < ai->getData()->m_visibleNumBones )
		{
			//Drop an item!
			if( draw )
			{
				//First hide the subobject as it becomes the delivered payload.
				if( ai->getData()->m_visibleSubObjectName.isNotEmpty() )
				{
					AsciiString name;
					name.format( "%s%02d", ai->getData()->m_visibleSubObjectName.str(), ai->getVisibleItemsDelivered() + 1 );
					draw->showSubObject( name, false );
					updateSubObjects = true;
				}

				//Second, create the subobject for real
				if( ai->getData()->m_visiblePayloadTemplateName.isNotEmpty() )
				{
					const ThingTemplate* thing = TheThingFactory->findTemplate( ai->getData()->m_visiblePayloadTemplateName.str() );
					Object *payload = TheThingFactory->newObject( thing, owner->getControllingPlayer()->getDefaultTeam() );

					if( payload )
					{
						payload->setProducer( owner );

						if( ai->getData()->m_visibleDropBoneName.isNotEmpty() )
						{
							Coord3D pos;
							AsciiString bone;
							bone.format( "%s%02d", ai->getData()->m_visibleDropBoneName.str(), ai->getVisibleItemsDelivered() + 1 );
							if( draw->getPristineBonePositions( ai->getData()->m_visibleDropBoneName.str(), ai->getVisibleItemsDelivered() + 1, &pos, nullptr, 1 ) > 0 )
							{
								draw->convertBonePosToWorldPos( &pos, nullptr, &pos, nullptr );
								payload->setPosition( &pos );
							}
							else
							{
								payload->setPosition( owner->getPosition() );
							}
						}
						else
						{
							payload->setPosition( owner->getPosition() );
						}
						payload->setOrientation( owner->getOrientation() );

						//If we want the payload to inherit the transport's velocity, do so now.
						if( ai->getData()->m_inheritTransportVelocity )
						{
							Coord3D startingForce = *owner->getPhysics()->getVelocity();
							startingForce.scale( payload->getPhysics()->getMass() );
							payload->getPhysics()->applyMotiveForce( &startingForce );

							Coord3D backPosition = *owner->getPhysics()->getVelocity();
							backPosition.scale( -1.0f );
							backPosition.add( *payload->getPosition() );
							payload->setPosition( &backPosition );
						}

						//Are we firing a missile?
						Bool projectileFired = false;
						for( BehaviorModule** u = payload->getBehaviorModules(); *u; ++u )
						{
							ProjectileUpdateInterface* pui = (*u)->getProjectileUpdateInterface();
							if( pui  )
							{
								//Missile!
								const WeaponTemplate *weaponTemplate = ai->getData()->m_visiblePayloadWeaponTemplate;
								if( !weaponTemplate )
								{
									DEBUG_CRASH( ("%s tried to fire missile %s via DeliverPayload, and is missing required weapon template in ObjectCreationList.ini entry.",
																				owner->getTemplate()->getName().str(), payload->getTemplate()->getName().str() ) );
									break;
								}
								VeterancyLevel v = owner->getVeterancyLevel();
								pui->projectileFireAtObjectOrPosition( nullptr, ai->getTargetPos(), weaponTemplate, weaponTemplate->getProjectileExhaust(v) );
								projectileFired = true;
								//damageInfo.in.m_sourceID = pui->projectileGetLauncherID();
								break;
							}
						}

						//If not, are we dropping a bomb?
						if( !projectileFired )
						{
							//Bomb!
							if( ai->getData()->m_exitPitchRate != 0 )
							{
								//If the payload needs to pitch, do so now.
								Real pitchRate = payload->getPhysics()->getCenterOfMassOffset() * ai->getData()->m_exitPitchRate;
								payload->getPhysics()->setPitchRate( pitchRate );
							}

							//If payload can move independently, order the move. A lot of bombs don't have locomotors,
							//in which case the "start move sound" is played, but nothing else actually happens.
							if( payload->getAIUpdateInterface() )
							{
								payload->getAIUpdateInterface()->aiMoveToPosition( ai->getMoveToPos(), CMD_FROM_AI );
							}

						}
					}
				}
			}

			//Update the counters
			attemptDrops--;
			ai->setVisibleItemsDelivered( ai->getVisibleItemsDelivered() + 1 );
		}
		if( updateSubObjects )
		{
			draw->updateSubObjects();
		}
	}

	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
void DeliveringState::onExit( StateExitType ) // Close the doors
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if (ai)
		ai->friend_setFreeToExit(false);

	DEBUG_ASSERTCRASH(m_didOpen, ("Not enough time for the doors to open to drop anything!"));
				/// @todo srj -- for now, this assumes at most one door
	owner->clearAndSetModelConditionState( MODELCONDITION_DOOR_1_OPENING, MODELCONDITION_DOOR_1_CLOSING );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ConsiderNewApproachState::crc( Xfer *xfer )
{
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void ConsiderNewApproachState::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

	xfer->xferInt(&m_numberEntriesToState);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ConsiderNewApproachState::loadPostProcess()
{
}


//-------------------------------------------------------------------------------------------------
StateReturnType ConsiderNewApproachState::onEnter() // Increment local counter o' futility
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

	++m_numberEntriesToState;

	DEBUG_LOG(("Considering approach #%d...",m_numberEntriesToState));
	if( m_numberEntriesToState > ai->getMaxNumberAttempts() )
	{
		DEBUG_LOG(("Too many approaches! Time to give up."));
		return STATE_FAILURE;
	}

	// ok, play some games here... the pathfinder is pretty dumb about things with big turning radii,
	// so we have to force the issue here... basically, estimate our necessary turning radius
	// based on loco values, move off far enough so we can turn, then head back. (if we just say
	// "head back directly", the code will just keep turning in circles, not realizing that our
	// turning radius is too large for that ever to work.)
	Real minTurnRadius = ai->calcMinTurnRadius(nullptr);

	// how far is "far enough"? we must be at least 2*radius dist away from our target.
	// (we add a little fudge since we may not be able to travel our max speed while
	// turning.)
	const Real DIST_FUDGE = 2.2f;
	Real minReApproachDist = minTurnRadius * DIST_FUDGE;

	const Coord3D* dir = owner->getUnitDirectionVector2D();

	Coord3D reApproachPoint;
	reApproachPoint.x = owner->getPosition()->x + dir->x * minReApproachDist;
	reApproachPoint.y = owner->getPosition()->y + dir->y * minReApproachDist;
	reApproachPoint.z = 0.0f;	// yeah, yeah, should be terrainpos, but doesn't really matter here...

	ai->aiMoveToPosition( &reApproachPoint, CMD_FROM_AI );

	// we allow these guys to go to invalid (ie, off-map) positions
	ai->getCurLocomotor()->setAllowInvalidPosition(true);

	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
StateReturnType ConsiderNewApproachState::update() // Success if we should try again, failure if we should give up
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

	// stay in this state 'till we reach our goal.
	if (ai->getAIStateType() != AI_MOVE_TO)
	{
		return STATE_SUCCESS;
	}

	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
void ConsiderNewApproachState::onExit( StateExitType status )
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return;
	}

	// go back to normal.
	ai->getCurLocomotor()->setAllowInvalidPosition(true);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void RecoverFromOffMapState::crc( Xfer *xfer )
{
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void RecoverFromOffMapState::xfer( Xfer *xfer )
{
  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

	xfer->xferUnsignedInt(&m_reEntryFrame);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void RecoverFromOffMapState::loadPostProcess()
{
}


//-------------------------------------------------------------------------------------------------
StateReturnType RecoverFromOffMapState::onEnter() // Increment local counter o' futility
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

  // have him hold in place, if possible.
	ai->aiMoveToPosition( owner->getPosition(), CMD_FROM_AI );

  // a little cheesy... make a delay based on turn-radius time to simulate offscreen
  // maneuvering.
  Real timeToTravelThatDist;
	/*Real minTurnRadius =*/ ai->calcMinTurnRadius(&timeToTravelThatDist);
  m_reEntryFrame = TheGameLogic->getFrame() + REAL_TO_INT_CEIL(timeToTravelThatDist);

	// kill its momentum
	PhysicsBehavior* physics = owner->getPhysics();
	physics->resetDynamicPhysics();

	ThePartitionManager->unRegisterObject( owner );
	if( owner->getDrawable() )
		owner->getDrawable()->setDrawableHidden( true );

	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
StateReturnType RecoverFromOffMapState::update() // Success if we should try again, failure if we should give up
{
	if (TheGameLogic->getFrame() < m_reEntryFrame)
		return STATE_CONTINUE;

	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

	ThePartitionManager->registerObject( owner );
	if( owner->getDrawable() )
		owner->getDrawable()->setDrawableHidden( false );

	Coord3D enterCoord = TheTerrainLogic->findClosestEdgePoint( owner->getPosition() );
	if (owner->isAboveTerrain())
		enterCoord.z = owner->getPosition()->z;
	owner->setPosition(&enterCoord);

	Real enterAngle = WWMath::Atan2Origin(ai->getMoveToPos()->y - enterCoord.y, ai->getMoveToPos()->x - enterCoord.x);
	owner->setOrientation(enterAngle);

	PhysicsBehavior* physics = owner->getPhysics();
	physics->resetDynamicPhysics();

	return STATE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
StateReturnType HeadOffMapState::onEnter() // Give move order out of town
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

	ai->killDeliveryDecal();

	if ( ai->getData()->m_selfDestructObject ) //Lorenzen... this makes delivery unit pop away like a bubble...
	{
		TheGameLogic->destroyObject( owner );
		return STATE_CONTINUE;
	}

	//Coord3D exitCoord = TheTerrainLogic->findClosestEdgePoint( owner->getPosition() );

	// just keep moving straight ahead till we exit the map.
	Coord3D exitCoord = *owner->getPosition();
	const Coord3D* dir = owner->getUnitDirectionVector2D();

	Region3D terrainExtent;
	TheTerrainLogic->getExtent( &terrainExtent );
	const Real FUDGE = 1.2f;
	Real HUGE_DIST = FUDGE * WWMath::SqrtOrigin(sqr(terrainExtent.hi.x - terrainExtent.lo.x) + sqr(terrainExtent.hi.y - terrainExtent.lo.y));

	exitCoord.x += dir->x * HUGE_DIST;
	exitCoord.y += dir->y * HUGE_DIST;

	ai->getCurLocomotor()->setAllowInvalidPosition(true);
	ai->getCurLocomotor()->setUltraAccurate(true);	// set ultra-accurate just so AI won't try to adjust our dest
	ai->aiMoveToPosition( &exitCoord, CMD_FROM_AI );

		// once we get into head-off-map state, we're done... don't respond to anything else.
		// this addresses the bug where a plane delivering team "foo" hasn't made it off the map
		// before all of team "foo" lands, whereupon team "foo" is given a script command to
		// (say) move to a given point... and since the cargo plane might be a member of "foo"
		// it would respond to said command, which sucks and is confusing. might be better
		// to have these guys generally ignore most commands, but this is pretty safe, in that
		// AFAIK nothing with deliverpayload should ever respond to anything, not even IDLE,
		// once they start heading off the map. (srj)
	ai->friend_setAcceptingCommands(false);


  owner->getUnitDirectionVector3D( facingDirectionUponDelivery );

	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
StateReturnType HeadOffMapState::update()
{
	Object *owner = getMachineOwner();
	DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)owner->getAIUpdateInterface();
	if( !ai )
	{
		return STATE_FAILURE;
	}

	if (ai->isOffMap())
		return STATE_SUCCESS;

  // greasy as this hack seems... if terrain or objects or whatever have turned me away from the map-edge that I had first headed for,...
  //I blow up, rather than face eternally spinning on the point of a mineret or derrick or something awful.
  if ( owner->getPhysics()->getTurning() != 0 )
  {
    Coord3D currentDirection;
    owner->getUnitDirectionVector3D( currentDirection );
  	Real dot = facingDirectionUponDelivery.x * currentDirection.x
             + facingDirectionUponDelivery.y * currentDirection.y
             + facingDirectionUponDelivery.z * currentDirection.z;

    if ( dot < 0.3f )
      owner->kill();
  }



//  Coord3D pos = *owner->getPosition();
//
//  if( TheTerrainLogic->getGroundHeight( pos->x, pos,y ) )


	return STATE_CONTINUE;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
StateReturnType CleanUpState::onEnter() // Delete my successful butt
{
	if( getMachineOwner()->getContain() )
	{
		DEBUG_ASSERTCRASH(getMachineOwner()->getContain()->getContainCount() == 0, ("did not drop all items!"));
	}

	Object *owner = getMachineOwner();
	TheGameLogic->destroyObject( owner );

	return STATE_CONTINUE;
}

