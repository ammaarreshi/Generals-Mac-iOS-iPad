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

// FILE: SpecialAbilityUpdate.cpp /////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, July 2002
// Desc:   Handles processing of unit special abilities.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameAudio.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/MiscAudio.h"
#include "Common/Xfer.h"

#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/Eva.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GameText.h"

#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/ExperienceTracker.h"

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/LaserUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/StickyBombUpdate.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Module/ContainModule.h"



//-------------------------------------------------------------------------------------------------
SpecialAbilityUpdate::SpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_captureFlashPhase = 0.0f;
	m_active = false;
	m_prepFrames = 0;
	m_animFrames = 0;
  m_targetID = INVALID_ID;
	m_targetPos.zero();
	m_locationCount = 0;
	m_specialObjectEntries = 0;
	m_noTargetCommand = false;
	m_packingState = STATE_NONE;
	m_facingInitiated = false;
	m_facingComplete = false;
	m_withinStartAbilityRange = false;
	m_doDisableFXParticles = TRUE;// true always, unless small unit causes it to toggle on-off
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
//  This is the althernate way to one-at-a-time BlackLotus' specials; we'll keep it commented her until Dustin decides, or until 12/10/02
//	setBusy( FALSE );
}

//-------------------------------------------------------------------------------------------------
SpecialAbilityUpdate::~SpecialAbilityUpdate()
{
	onExit( true );
}

/*------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::update()

This is the brains of the entire special ability update. There are several optional steps and
variations that can be processed for any particular type of special ability. A special ability
that has every option will do the following in order:

	1 -- APPROACH: If I'm not close enough to the target, then approach it
	2 -- UNPACK: If I need to unpack before I can prepare, then do so now (this uses the model
			 condition unpack).
	3 -- PREPARE: If I need to perform a task for a period of time before I can trigger my special
			 ability, then do so now. A good example is aiming with a targeting laser for a few
			 seconds before firing your special weapon.
	4 -- TRIGGER: Once preparation is complete, fire your special ability now.
	5 -- PACK: If I need to pack after finishing my attack, do so now.
	6 -- FINISH: Clean up the states, and turn off the update.

Variations:

	Persistent Specials -- A persistent special will continually trigger it's effect every so often
		 and never end. A good example of this is the disable building hack. The hacker will run up
		 to the target building, unpack, prepare (firing hack stream), then after a period of time,
		 the building becomes disabled. But because it's persistent, we reset the preparation and
		 trigger the building disabled code over and over again -- which is on a timer.
	No Target Specials -- You can link two different main specials together. Colonel Burton has the
		 ability to lay C4 charges on multiple targets. Activating these specials require a target.
		 The non-target version is actually the detonator, which goes through all the special objects
		 and detonates each one of them.

Options:
	SpecialPowerTemplate -- Defines the special power template that links to a command.
	StartAbilityRange		 -- Specify how far you want your unit to be from the target before
													start your special ability.
	AbilityAbortRange		 -- After starting an attack, it'll allow preparation unless the target goes
	                        beyond this range. If this happens, the ability is aborted outright.
	PreparationTime			 -- How long it takes to prepare your special once in position and unpacked.
	PersistentPrepTime	 -- This value defines whether or not you are using a persistent special.
													Once the special ability is triggered, it'll wait until this specified
													delay occurs and it'll trigger it again, for ever until the unit dies,
													the target dies, or the unit decides to do something else.
	PackTime						 -- How long it takes to pack up the unit after triggering a non persistent
													special ability or after ordering the unit to do something else, or the
													target dies (if applicable).
	PackUnpackVariationFactor -- Randomizes the pack and unpack time by specified range. The closer
													to zero, the smaller the variation. 0.2 would have a pack or unpack range
													of xTime +/- 20%. This is important because it represents averages.
	UnpackTime					 -- Same as PackTime, except used once entering range of target.
	SkipPackingWithNoTarget -- This option is used by the No Target Special variation, and only uses
												  packing/unpacking when you have a specific target. In the case of Colonel
													Burton, this value is set to true. When he plants a C4 charge, he has a
													target, therefore he requires unpacking (laying the charge). When he runs
													away and decides to detonate the charge, he calls the same special
													ability, but without a target -- therefore the packing is skipped and
													detonates it right away.
	SpecialObject				 -- Defines the special object the unit will create via the special ability.
													In one case, it creates and maintains the laser or binary stream during
													preparation. In Colonel Burton's case, it keeps track of the C4 charges
													that he has placed.
	MaxSpecialObjects		 -- Defines the max number of special objects that can exist at any given
													time. The laser example only has one, but the C4 charges can have more.
	SpecialObjectsPersistent -- If this flag is set, then the objects will remain should the owner
													decides to do something else... C4 charges are a good example.
	EffectDuration			 -- Defines the duration of the special ability. In the case of disabling
													the building (hacker), this value will dictate how long the building
													will be disabled should the hacker die or stop the attack.
	UniqueSpecialObjectTargets -- Prevents the owner from placing multiple special objects on the
	                        same target. C4 charges once again.
	SpecialObjectsPersistWhenOwnerDies -- If the owner dies, the special objects will remain in
													the world. Timed C4 charges is a good example, but remote C4 charges
													is a bad example -- because it requires the owner to detonate them.
	FlipObjectAfterPacking -- Simply rotates the object 180 degrees after packing (due to special
													animation case).
	FlipObjectAfterUnPacking -- Simply rotates the object 180 degrees after unpacking (due to
													special	animation case). Used by colonel burton after planting charge.


	Lets use the Age of Kings Trebuchet as a simple example:
	Variation -- PersistentPrepTime (set to true in ini file)
	1 -- APPROACH:	Get within range before unpacking
	2 -- UNPACK:		Unpack the treb at this location.
	3 -- PREPARE:		Aim at the target
	4 -- TRIGGER:		Fire the treb.
	5 -- RESET PREPARATION:	Go to step 3 until target dead.
	6 -- PACK: Assuming we are done... pack up
	7 -- FINISH: Stop the special ability

-------------------------------------------------------------------------------------------------*/
UpdateSleepTime SpecialAbilityUpdate::update()
{

/// @todo srj -- this could probably sleep more between stages. maybe someday.
	// Note: This will be complicated because one difficult example is handling Burton when he dies while
	//       bombs are in the world.

	//Validation of special objects makes sure that they still exist. Sometimes special
	//objects can be destroyed or removed, or detonate, etc! When they go missing,
	//they are removed from the special object list and free up slots for those units
	//that can have multiple.
	//We need to do this now because it's possible the special objects need to be checked while
	//the the special ability is over (thus inactive).
	validateSpecialObjects();

	//Important! This check will see if there has been any commands issued by either the player
	//or script. When told to do something else, we need to immediately cleanup our special ability.
	//This also means some things might be left around like timed charges to detonate.
	if( getObject()->isEffectivelyDead() )
	{
		onExit( TRUE );
		return calcSleepTime();
	}

	if( !m_active )	// Not active.
		return calcSleepTime();

	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if( !ai )
	{
		onExit( false );
		return calcSleepTime();
	}
	if( ai->getLastCommandSource() != CMD_FROM_AI )
	{
		onExit( false );
		return calcSleepTime();
	}

	//STEP 2 & 5(6) -- Handles packing and unpacking in progress. If packing
	//then ends the special ability once complete. Things that don't pack
	//will never be handled, nor do things that aren't in a packing state.
	if( handlePackingProcessing() )
	{
		return calcSleepTime();
	}

	Bool shouldAbort = false;

	// A dead target will end our special (if we are using a target).
	if (m_targetID != INVALID_ID)
	{
		Object* target = TheGameLogic->findObjectByID(m_targetID);

		if (target != nullptr)
		{
			const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
			if (target->isEffectivelyDead())
				shouldAbort = TRUE;
			else switch (data->m_specialPowerTemplate->getSpecialPowerType())
			{
				case SPECIAL_INFANTRY_CAPTURE_BUILDING:
				case SPECIAL_BLACKLOTUS_CAPTURE_BUILDING:
				case SPECIAL_HACKER_DISABLE_BUILDING:
					if (target->getTeam() == getObject()->getTeam())
					{
						// it's been captured by a colleague! we should stop.
						shouldAbort = TRUE;
					}
					break;
			}

		}

	}

	if (shouldAbort)
	{
		// doh, a colleague has already captured it. just stop.
		ai->aiIdle( CMD_FROM_AI );
		onExit( false );
		return calcSleepTime();
	}


	//DETERMINE OUR PHASE! BRAIN LOGIC
	if( !isPreparationComplete() )
	{
		//The special ability has fired, now continue to process the special ability
		//until it expires
		m_prepFrames--;

		if( isPreparationComplete() )
		{
			//STEP 4 -- TRIGGER (with preparation)
			triggerAbilityEffect();
			if( isPersistentAbility() )
			{
				//VARIATION -- PERSISTENCE (repeats preparation)
				resetPreparation();
			}
			else
			{
				endPreparation();
				if( needToPack() )
				{
					//STEP 5 -- PACK
					//Note: If we actually do pack, then cleanup will be handled in
					//handlePackingProcess(), near the top of this function.
					startPacking(true);
				}
				else
				{
					//STEP 6 -- FINISH
					finishAbility();
				}
			}
		}
		else
		{
			//Process the preparation if it's still not complete.
			Bool continuePrep = continuePreparation();
			if( !continuePrep )
			{
				//We failed so abort!
				endPreparation();
				if( needToPack() )
				{
					//STEP 5 -- PACK
					//Note: If we actually do pack, then cleanup will be handled in
					//handlePackingProcess(), near the top of this function.
					startPacking(false);
				}
				else
				{
					//STEP 6 -- FINISH
					finishAbility();
				}
			}
		}
	}
	else if( isWithinStartAbilityRange() )
	{
		m_withinStartAbilityRange = true;
		if( !isFacing() && needToFace() )
		{
			startFacing();
			return calcSleepTime();
		}

		if( needToUnpack() )
		{
			//STEP 2 -- UNPACK
			startUnpacking();
			return calcSleepTime();
		}

		if( m_packingState == STATE_UNPACKED )
		{
			//STEP 3 -- PREPARE
			startPreparation();

			if( isPreparationComplete() )
			{
				//STEP 4 -- TRIGGER (skipping preparation)
				triggerAbilityEffect();
				endPreparation();

				if( needToPack() )
				{
					//STEP 5 -- PACK
					//Note: If we actually do pack, then cleanup will be handled in
					//handlePackingProcess(), near the top of this function.
					startPacking(true);
				}
				else
				{
					//STEP 6 -- FINISH
					finishAbility();
				}
			}
		}
	}
	else if( ai->isIdle() )
	{
		//STEP 1 -- APPROACH
		approachTarget();
	}
	return calcSleepTime();
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate,
																													 const Object *targetObj,
																													 const Coord3D *targetPos,
																													 const Waypoint *way,
																													 UnsignedInt commandOptions )
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	const SpecialPowerTemplate *spTemplate = data->m_specialPowerTemplate;

	if( spTemplate != specialPowerTemplate )
	{
		//Check to make sure our modules are connected.
		return FALSE;
	}

	//Clear target values
	m_targetID = INVALID_ID;
	m_targetPos.zero();
	m_locationCount = 0;
	m_prepFrames = 0;
	m_animFrames = 0;
	m_packingState = STATE_PACKED;
	m_facingInitiated = false;
	m_facingComplete = false;
	m_withinStartAbilityRange = false;

	getObject()->clearModelConditionFlags(
		MAKE_MODELCONDITION_MASK4( MODELCONDITION_UNPACKING, MODELCONDITION_PACKING, MODELCONDITION_FIRING_A, MODELCONDITION_RAISING_FLAG ) );

	if( targetObj )
	{
		//Get the target!
		m_targetID = targetObj ? targetObj->getID() : INVALID_ID;
	}
	else if( targetPos )
	{
		//Get the position!
		m_targetPos = *targetPos;
	}

	//Clear any old AI before starting this special ability.
	if( !getObject()->getAIUpdateInterface() )
	{
		return FALSE;
	}
	getObject()->getAIUpdateInterface()->aiIdle( CMD_FROM_AI );

	//Determine whether we are triggering a command (rather than executing special at location or target)
	m_noTargetCommand = !targetObj && !targetPos;

	if( data->m_unpackTime == 0 || (m_noTargetCommand && data->m_skipPackingWithNoTarget) )
	{
		//Only unpack if we need to -- setting it to unpacked will skip step 2 in the update
		m_packingState = STATE_UNPACKED;
	}

	m_active = true;

	//Prevent other mutually exclusive specials from running (kill them now if we're starting something else)
	SpecialAbilityUpdate *disableSA;
	disableSA = getObject()->findSpecialAbilityUpdate( SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK );
	if( disableSA && disableSA != this )
		disableSA->onExit( FALSE );
	disableSA = getObject()->findSpecialAbilityUpdate( SPECIAL_BLACKLOTUS_STEAL_CASH_HACK );
	if( disableSA && disableSA != this )
		disableSA->onExit( FALSE );
	disableSA = getObject()->findSpecialAbilityUpdate( SPECIAL_BLACKLOTUS_CAPTURE_BUILDING );
	if( disableSA && disableSA != this )
		disableSA->onExit( FALSE );
	disableSA = getObject()->findSpecialAbilityUpdate( SPECIAL_REMOTE_CHARGES );
	if( disableSA && disableSA != this )
		disableSA->onExit( FALSE );
	disableSA = getObject()->findSpecialAbilityUpdate( SPECIAL_TIMED_CHARGES );
	if( disableSA && disableSA != this )
		disableSA->onExit( FALSE );


	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::isPowerCurrentlyInUse( const CommandButton *command ) const
{
	if( command )
	{
		if( command->getSpecialPowerTemplate() && command->getSpecialPowerTemplate()->getSpecialPowerType() == SPECIAL_REMOTE_CHARGES )
		{
			if( !BitIsSet( command->getOptions(), CONTEXTMODE_COMMAND ) )
			{
				//This is the detonate charge button. Treat it backwards saying it's in use when we don't have any special objects (charges).
				//That way, the button will be grayed out.
				return getSpecialObjectCount() == 0;
			}
		}
	}


	if( m_packingState != STATE_NONE )
	{
		//exception for powers with zero reload time... they are ready to use immediately!
		if ( (m_packingState == STATE_PACKING || m_packingState == STATE_PACKED) &&
										command && command->getSpecialPowerTemplate()->getReloadTime() == 0 )
			return false;

		if ( m_withinStartAbilityRange )
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::onExit( Bool cleanup )
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();

	getObject()->clearModelConditionFlags(
		MAKE_MODELCONDITION_MASK4( MODELCONDITION_UNPACKING, MODELCONDITION_PACKING, MODELCONDITION_FIRING_A, MODELCONDITION_RAISING_FLAG ) );
	getObject()->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_IS_USING_ABILITY ) );

	TheAudio->removeAudioEvent( m_prepSoundLoop.getPlayingHandle() );
	endPreparation();

	if( !data->m_specialObjectsPersistent || (cleanup && !data->m_specialObjectsPersistWhenOwnerDies) )
	{
		//Delete special objects that aren't considered persistent whenever we turn off
		//leave the special ability update.
		killSpecialObjects();
	}

	m_active = false;
	m_withinStartAbilityRange = false;
	m_packingState = STATE_NONE;

//  This is the althernate way to one-at-a-time BlackLotus' specials; we'll keep it commented her until Dustin decides, or until 12/10/02
//	setBusy( FALSE );// My owner is no longer using me


// no, actually, we DON'T want to call this here, since onExit is always called
// (directly or indirectly) from update()... and calling setWakeFrame() from your
// own update() method is a no-no (since it would just be ignored in favor
// of the return value from update() anyway). just set m_active to false,
// and we'll put ourselves to sleep.
//	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

//-------------------------------------------------------------------------------------------------
//Some special abilities requires packing or unpacking. When setup, all this function does is
//decrement the counter and clear the model state when finished.
//Returns TRUE if we are still packing or unpacking.
//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::handlePackingProcessing()
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	if( m_animFrames > 0 )
	{
		m_animFrames--;
		if( m_animFrames == 0 )
		{
			// We're done, so clear the states.
			getObject()->clearModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_UNPACKING, MODELCONDITION_PACKING ) );

			if( m_packingState == STATE_UNPACKING )
			{
				if( data->m_flipObjectAfterUnpacking )
				{
					getObject()->setOrientation( getObject()->getOrientation() + PI );
				}
				m_packingState = STATE_UNPACKED;
			}
			else if( m_packingState == STATE_PACKING )
			{
				if( data->m_flipObjectAfterPacking )
				{
					getObject()->setOrientation( getObject()->getOrientation() + PI );
				}
				//We just finished packing up, therefore
				//we have completed our special ability.
				m_packingState = STATE_PACKED;

				//Do exit preparation.
				finishAbility();

				//Complete the special ability now
				return true;
			}

			//We're finished processing
			return false;
		}



		//This is new... the ability to disable stealth before triggering
		if( getSpecialAbilityUpdateModuleData()->m_loseStealthOnTrigger &&
			m_animFrames < getSpecialAbilityUpdateModuleData()->m_preTriggerUnstealthFrames)
		{
			StealthUpdate* stealth = getObject()->getStealth();
			if( stealth )
			{
				stealth->markAsDetected();
			}
		}



		//We're still processing
		return true;
	}
	//We're not processing
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::needToPack() const
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	if( m_packingState == STATE_UNPACKED )
	{
		if( data->m_skipPackingWithNoTarget && m_noTargetCommand )
		{
			return false;
		}
		if( data->m_packTime )
		{
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::needToUnpack() const
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	if( m_packingState == STATE_PACKED )
	{
		if( data->m_skipPackingWithNoTarget && m_noTargetCommand )
		{
			return false;
		}
		if( data->m_unpackTime )
		{
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::startPacking(Bool success)
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	m_packingState = STATE_PACKING;
	Real variation = GameLogicRandomValueReal( 1.0f - data->m_packUnpackVariationFactor, 1.0f + data->m_packUnpackVariationFactor );
	m_animFrames = data->m_packTime * variation;

	//Set the animation state
	getObject()->clearAndSetModelConditionFlags(
				MAKE_MODELCONDITION_MASK2( MODELCONDITION_UNPACKING, MODELCONDITION_RAISING_FLAG ),
				MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ) );

	AudioEventRTS sound = data->m_packSound;
	sound.setObjectID( getObject()->getID() );
	TheAudio->addAudioEvent( &sound );

	//Sync the animation length to the time it'll take to pack.
	Drawable* draw = getObject()->getDrawable();
	if (draw)
		draw->setAnimationCompletionTime( m_animFrames );
	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if (ai)
		ai->aiBusy(CMD_FROM_AI);

	if (success)
	{
		AudioEventRTS event;
		switch( data->m_specialPowerTemplate->getSpecialPowerType() )
		{
			//case SPECIAL_HACKER_DISABLE_BUILDING:// Awaiting Mical's new sound
			//	event = *getObject()->getTemplate()->getPerUnitSound( "VoiceDisableBuildingComplete" );
			//	break;
			case SPECIAL_BLACKLOTUS_CAPTURE_BUILDING:
				event = *getObject()->getTemplate()->getPerUnitSound( "VoiceCaptureBuildingComplete" );
				break;
			case SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK:
				event = *getObject()->getTemplate()->getPerUnitSound( "VoiceDisableVehicleComplete" );
				break;
			case SPECIAL_BLACKLOTUS_STEAL_CASH_HACK:
				event = *getObject()->getTemplate()->getPerUnitSound( "VoiceStealCashComplete" );
				break;
			default:
				event = *getObject()->getTemplate()->getVoiceTaskComplete();
				break;
		}

		event.setObjectID(getObject()->getID());
		TheAudio->addAudioEvent(&event);
	}

}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::startUnpacking()
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	m_packingState = STATE_UNPACKING;
	Real variation = GameLogicRandomValueReal( 1.0f - data->m_packUnpackVariationFactor, 1.0f + data->m_packUnpackVariationFactor );
	m_animFrames = data->m_unpackTime * variation;

	//Set the animation state
	getObject()->clearAndSetModelConditionFlags(
				MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ),
				MAKE_MODELCONDITION_MASK( MODELCONDITION_UNPACKING ) );


	AudioEventRTS sound = data->m_unpackSound;
	sound.setObjectID( getObject()->getID() );
	TheAudio->addAudioEvent( &sound );

	//Sync the animation length to the time it'll take to unpack.
	Drawable* draw = getObject()->getDrawable();
	if (draw)
		draw->setAnimationCompletionTime( m_animFrames );
	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if (ai)
		ai->aiBusy(CMD_FROM_AI);
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::isWithinStartAbilityRange() const
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	const Object *self = getObject();

	//Quickly convert very short range approaches to "contact" class requiring collision before
	//stopping.
	Real range = data->m_startAbilityRange;
	const Real UNDERSIZE = PATHFIND_CELL_SIZE_F * 0.25f;
	range = __max( 0.0f, range - UNDERSIZE );

	if( m_withinStartAbilityRange )
	{
		//Only get within range once.
		return true;
	}

	Real fDistSquared = 0.0f;
	Object *target = nullptr;
	if( m_targetID != INVALID_ID )
	{
		target = TheGameLogic->findObjectByID( m_targetID );
		if( target )
		{
			fDistSquared = ThePartitionManager->getDistanceSquared( self, target, FROM_BOUNDINGSPHERE_2D );
		}
	}
	else if( m_targetPos.x || m_targetPos.y || m_targetPos.z ) //It's zero if not used...
	{
		fDistSquared = ThePartitionManager->getDistanceSquared( self, &m_targetPos, FROM_BOUNDINGSPHERE_2D );
	}
	else
	{
		//No position, so this step is useless
		return true;
	}

	//Check to see how far we are from the target!
	Real fStartRangeSquared = data->m_startAbilityRange * data->m_startAbilityRange;
	if( fDistSquared <= fStartRangeSquared )
	{
		if( range == 0.0f && m_targetID != INVALID_ID )
		{
			//We want to ensure we collided with our target first!
			ObjectIterator *iter = ThePartitionManager->iteratePotentialCollisions( self->getPosition(), self->getGeometryInfo(), 0.0f );
			MemoryPoolObjectHolder hold(iter);
			for( Object *them = iter->first(); them; them = iter->next() )
			{
				if( target == them )
				{
					return true;
				}
			}
			return false;
		}

		if( data->m_approachRequiresLOS )
		{
			//Make sure we can see the target!
			PartitionFilterLineOfSight	filterLOS( self );
			PartitionFilter *filters[] = { &filterLOS, nullptr };
			ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( self, range, FROM_BOUNDINGSPHERE_2D, filters, ITER_SORTED_NEAR_TO_FAR );
			MemoryPoolObjectHolder hold(iter);
			for( Object *theTarget = iter->first(); theTarget; theTarget = iter->next() )
			{
				//LOS check succeeded.
				if( target == theTarget )
				{
					return true;
				}
			}
		}
		else
		{
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::isWithinAbilityAbortRange() const
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	const Object *self = getObject();

	//Quickly convert very short range approaches to "contact" class requiring collision before
	//stopping.
	Real range = data->m_startAbilityRange;
	const Real UNDERSIZE = PATHFIND_CELL_SIZE_F * 0.25f;
	range = __max( 0.0f, range - UNDERSIZE );

	Real fDistSquared = 0.0f;
	Object *target = nullptr;
	if( m_targetID != INVALID_ID )
	{
		target = TheGameLogic->findObjectByID( m_targetID );
		if( target )
		{
			fDistSquared = ThePartitionManager->getDistanceSquared( self, target, FROM_BOUNDINGSPHERE_2D );
		}
	}
	else if( m_targetPos.x || m_targetPos.y || m_targetPos.z ) //It's zero if not used...
	{
		fDistSquared = ThePartitionManager->getDistanceSquared( self, &m_targetPos, FROM_BOUNDINGSPHERE_2D );
	}
	else
	{
		//No position, so this step is useless
		return true;
	}

	//Check to see how far we are from the target!
	Real fStartRangeSquared = data->m_abilityAbortRange * data->m_abilityAbortRange;
	if( fDistSquared <= fStartRangeSquared )
	{
		if( range == 0.0f && m_targetID != INVALID_ID )
		{
			//We want to ensure we collided with our target first!
			ObjectIterator *iter = ThePartitionManager->iteratePotentialCollisions( self->getPosition(), self->getGeometryInfo(), 0.0f );
			MemoryPoolObjectHolder hold(iter);
			for( Object *them = iter->first(); them; them = iter->next() )
			{
				if( target == them )
				{
					return true;
				}
			}
			return false;
		}

		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::approachTarget()
{
	Object *self = getObject();
	if( m_targetID != INVALID_ID )
	{
		Object *target = TheGameLogic->findObjectByID( m_targetID );
		if( target )
		{
			AIUpdateInterface *ai = self->getAIUpdateInterface();
			if( ai )
			{
				ai->ignoreObstacle( target );
				ai->aiMoveToObject( target, CMD_FROM_AI );
				return true;
			}
		}
	}
	else if( m_targetPos.x || m_targetPos.y || m_targetPos.z ) //It's zero if not used...
	{
		AIUpdateInterface *ai = self->getAIUpdateInterface();
		if( ai )
		{
			ai->aiMoveToPosition( &m_targetPos, CMD_FROM_AI );
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::startPreparation()
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	const SpecialPowerTemplate *spTemplate = data->m_specialPowerTemplate;

	//Set the preparation timer
	m_prepFrames = data->m_preparationFrames;

	switch( spTemplate->getSpecialPowerType() )
	{
		case SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES:
		{
			Object *target = TheGameLogic->findObjectByID( m_targetID );
			if( target )
			{
				//Specialized code that specifically creates and looks up a laser update.
				Object *specialObject = createSpecialObject();
				if( specialObject )
				{
					if (!initLaser(specialObject, target))
						return;
				}
			}
			break;
		}
		case SPECIAL_INFANTRY_CAPTURE_BUILDING:
		{
			Object *target = TheGameLogic->findObjectByID( m_targetID );
			if (target)
			{
				if (target->getTeam() == getObject()->getTeam())
				{
					// it's been captured by a colleague! we should stop.
					return;
				}
			}

			getObject()->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_UNPACKING ),
																									 MAKE_MODELCONDITION_MASK( MODELCONDITION_RAISING_FLAG ) );
			Drawable* draw = getObject()->getDrawable();
			if (draw)
				draw->setAnimationCompletionTime(data->m_preparationFrames);

			//Warn the victim so he might have a chance to react!
			if( target && target->isLocallyViewed() )
			{
				TheEva->setShouldPlay( EVA_BuildingBeingStolen );
			}

			break;
		}

		case SPECIAL_HACKER_DISABLE_BUILDING:
		case SPECIAL_BLACKLOTUS_CAPTURE_BUILDING:
		case SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK:
		case SPECIAL_BLACKLOTUS_STEAL_CASH_HACK:
		{
			Object *target = TheGameLogic->findObjectByID( m_targetID );
			if( target )
			{

				Relationship r = getObject()->getRelationship(target);
				if( r == ALLIES )
					return;

				//Specialized code that specifically creates and looks up a laser update.
				Object *specialObject = createSpecialObject();
				if( specialObject )
				{
					if (!initLaser(specialObject, target))
						return;

					//For the hacker this sets up the looping typing animation.
					getObject()->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_UNPACKING ),
																											 MAKE_MODELCONDITION_MASK( MODELCONDITION_FIRING_A ) );
				}

				//Warn the victim so he might have a chance to react!
				if( spTemplate->getSpecialPowerType() == SPECIAL_BLACKLOTUS_CAPTURE_BUILDING && target && target->isLocallyViewed() )
				{
					TheEva->setShouldPlay( EVA_BuildingBeingStolen );
				}

			}
			break;
		}
	}

	//Trigger the reset timer on the special power because we're officially starting it now!
	SpecialPowerModuleInterface *spmInterface = getMySPM();
	if( spmInterface )
	{
		spmInterface->markSpecialPowerTriggered(nullptr);// Null for not creating a view object
	}

	if (getObject()->getAI()) {
		getObject()->getAI()->aiIdle( CMD_FROM_AI ); // just in case.  jba.
	}
	getObject()->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_IS_USING_ABILITY ) );

	m_prepSoundLoop = data->m_prepSoundLoop;
	m_prepSoundLoop.setObjectID( getObject()->getID() );
	m_prepSoundLoop.setPlayingHandle( TheAudio->addAudioEvent( &m_prepSoundLoop ) );

}

//-------------------------------------------------------------------------------------------------
SpecialPowerModuleInterface* SpecialAbilityUpdate::getMySPM()
{
	const SpecialAbilityUpdateModuleData* d = getSpecialAbilityUpdateModuleData();
	return getObject()->getSpecialPowerModule(d->m_specialPowerTemplate);
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::initLaser(Object* specialObject, Object* target )
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	static NameKeyType key_LaserUpdate = NAMEKEY( "LaserUpdate" );
	Drawable *draw = specialObject->getDrawable();
	if( !draw )
	{
		killSpecialObjects();
		return false;
	}
	LaserUpdate *update = (LaserUpdate*)draw->findClientUpdateModule( key_LaserUpdate );
	if( !update )
	{
		killSpecialObjects();
		return false;
	}

	Coord3D startPos;
	if( !getObject()->getSingleLogicalBonePosition( data->m_specialObjectAttachToBoneName.str(), &startPos, nullptr ) )
	{
		//If we can't find the bone, then set it to our current position.
		startPos.set( *getObject()->getPosition() );
	}

	Coord3D endPos;
	if (target)
	{
		target->getGeometryInfo().getCenterPosition( *target->getPosition(), endPos );
	}
	else
	{
		endPos = startPos;
	}
	update->initLaser( nullptr, &startPos, &endPos );
	return true;
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::continuePreparation()
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	const SpecialPowerTemplate *spTemplate = data->m_specialPowerTemplate;

	//Check if we are within the abort distance, otherwise abort!
	if( data->m_abilityAbortRange < SPECIAL_ABILITY_HUGE_DISTANCE )
	{
		if( !isWithinAbilityAbortRange() )
		{
			return false;
		}
	}

	switch( spTemplate->getSpecialPowerType() )
	{
		case SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES:
		case SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK:
		{
			Object *target = TheGameLogic->findObjectByID( m_targetID );

			if( !target )
			{
				//Target is dead, stop.
				return false;
			}

			Relationship r = getObject()->getRelationship(target);
			if( r == ALLIES )
			{
				//It's been captured by a colleague, so cancel!
				return false;
			}

			//Specialized code that specifically creates and looks up a laser update.
			for( std::list<ObjectID>::iterator it = m_specialObjectIDList.begin(); it != m_specialObjectIDList.end(); ++it )
			{
				Object* specialObject = TheGameLogic->findObjectByID( *it );
				if( specialObject )
				{
					if( !initLaser( specialObject, target ) )
					{
						return false;
					}
				}
			}
			break;
		}

		case SPECIAL_INFANTRY_CAPTURE_BUILDING:
		case SPECIAL_BLACKLOTUS_CAPTURE_BUILDING:

			// PROCESS THE FLASHING WHILE GETTING CAPTURED FX HERE
			Object* target = TheGameLogic->findObjectByID( m_targetID );

			if( !target )
			{
				//Target is dead, stop.
				return false;
			}

			Relationship r = getObject()->getRelationship(target);
			if( r == ALLIES )
			{
				//It's been captured by a colleague, so cancel!
				return false;
			}

			if (data->m_doCaptureFX)
			{
				Drawable *targetDraw = target->getDrawable();

				if (targetDraw) // skip fx if merely 'invulnerable'
				{
					Bool lastPhase = ( ((Int)m_captureFlashPhase) & 1 );// were we in a flashy phase last frame?

					Real denominator = MAX(1, data->m_preparationFrames);
					Real increment = 1.0f - ((Real)m_prepFrames / denominator );
					m_captureFlashPhase += increment / 3.0f;

					Bool thisPhase = ( ((Int)m_captureFlashPhase) & 1 );// are we in a flashy phase this frame?

					if ( lastPhase && ( ! thisPhase ) )
					{

						RGBColor myHouseColor;
						myHouseColor.setFromInt( getObject()->getIndicatorColor() );

						Real saturation = TheGlobalData->m_selectionFlashSaturationFactor;
						targetDraw->saturateRGB( myHouseColor, saturation );

						targetDraw->flashAsSelected( &myHouseColor ); //In MY house color, not his!

						AudioEventRTS defectorTimerSound = TheAudio->getMiscAudio()->m_defectorTimerTickSound;
						defectorTimerSound.setObjectID( m_targetID );
						TheAudio->addAudioEvent(&defectorTimerSound);

					}
				}
			}


			SpecialPowerModuleInterface *spmInterface = getMySPM();
			if (spmInterface && spTemplate->getSpecialPowerType() == SPECIAL_INFANTRY_CAPTURE_BUILDING )
			{
				// these keep resetting the recharge timer while in use. (srj) // only for infantry capture, not black lotus which resets in triggerAbilityEffect()
				spmInterface->startPowerRecharge();
			}
			break;
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::triggerAbilityEffect()
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	const SpecialPowerTemplate *spTemplate = data->m_specialPowerTemplate;
	Object *object = getObject();

	//Award experience to units for triggering the ability (optional and ini specified).
	//NOTE: Be aware of persistent abilities that call trigger over and over again!
	if( data->m_awardXPForTriggering )
	{
		ExperienceTracker *xpTracker = object->getExperienceTracker();
		if( xpTracker )
		{
			xpTracker->addExperiencePoints( data->m_awardXPForTriggering );
		}
	}
	//Also add skill points. If unspecified, it'll use the award experience to units for triggering value.
	Int skillPoints = data->m_skillPointsForTriggering != -1 ? data->m_skillPointsForTriggering : data->m_awardXPForTriggering;
	if( skillPoints > 0 )
	{
		Player *player = object->getControllingPlayer();
		if( player )
		{
			player->addSkillPoints( skillPoints );
		}
	}


	AudioEventRTS sound = data->m_triggerSound;
	sound.setObjectID( object->getID() );
	TheAudio->addAudioEvent( &sound );


	Bool okToLoseStealth = TRUE;

	switch( spTemplate->getSpecialPowerType() )
	{
		case SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES:
		{
			Object *target = TheGameLogic->findObjectByID( m_targetID );
			if( target )
			{
				const Weapon *weapon = object->getWeaponInWeaponSlot( SECONDARY_WEAPON );
				if( weapon )
				{
					// lock it just till the weapon is empty or the attack is "done"
					object->setWeaponLock( SECONDARY_WEAPON, LOCKED_TEMPORARILY );
					AIUpdateInterface *ai = object->getAIUpdateInterface();
					if( ai )
					{
						ai->aiAttackObject( target, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
					}
				}
			}
			break;
		}
		case SPECIAL_TANKHUNTER_TNT_ATTACK:
		case SPECIAL_TIMED_CHARGES:
		{
			//Place new tnt.
			Object *target = TheGameLogic->findObjectByID( m_targetID );
			//sanity
			if( !target )
			{
				return;
			}

			Object *charge = createSpecialObject();
			if( charge )
			{
				static NameKeyType key_StickyBombUpdate = NAMEKEY( "StickyBombUpdate" );
				StickyBombUpdate *update = (StickyBombUpdate*)charge->findUpdateModule( key_StickyBombUpdate );
				if( !update )
				{
					DEBUG_CRASH( 
						("Unit '%s' attempted to place %s on %s but the bomb requires a StickyBombUpdate module.",
						object->getTemplate()->getName().str(),
						charge->getTemplate()->getName().str(),
						target->getTemplate()->getName().str() ) );
					killSpecialObjects();
					return;
				}
				//Setting the producer ID allows the sticky bomb update module to initialize
				//and setup timers, etc.
				update->init( target, object );


			}
			break;
		}
		case SPECIAL_HACKER_DISABLE_BUILDING:
		case SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK:
		{
			//Disable the target temporarily.
			Object *target = TheGameLogic->findObjectByID( m_targetID );

			//sanity
			if( !target )
			{
				return;
			}

			Relationship r = object->getRelationship(target);
			if ( r == ALLIES)
				return;

			//Disable the target for a specified amount of time.
			target->setDisabledUntil( DISABLED_HACKED, TheGameLogic->getFrame() + data->m_effectDuration );

			UnsignedInt durationInterleaveFactor = 1;
			Real targetFootprintArea = target->getGeometryInfo().getFootprintArea();

			if ( ( targetFootprintArea < 300) && target->isKindOf( KINDOF_STRUCTURE ))
			{
				m_doDisableFXParticles = !m_doDisableFXParticles; // toggle if small building
				durationInterleaveFactor = 2;
			}


			if ( m_doDisableFXParticles )
			{
				const ParticleSystemTemplate *tmp = data->m_disableFXParticleSystem;
				if (tmp)
				{
					ParticleSystem *sys = TheParticleSystemManager->createParticleSystem(tmp);
					if (sys)
					{
						Coord3D offs = {0,0,0};
						target->getGeometryInfo().makeRandomOffsetWithinFootprint( offs );

						sys->attachToObject(target);
						sys->setPosition( &offs );
						sys->setSystemLifetime( data->m_effectDuration * durationInterleaveFactor ); //lifetime of the system, not the particles

					}
				}
			}
			break;
		}

		case SPECIAL_INFANTRY_CAPTURE_BUILDING:
		case SPECIAL_BLACKLOTUS_CAPTURE_BUILDING:
		{
			Object *target = TheGameLogic->findObjectByID( m_targetID );

			//sanity
			if( !target )
			{
				return;
			}

			if (target->getTeam() == object->getTeam())
			{
				// it's been captured by a colleague! we should stop.
				return;
			}

			// Just in case we are capturing a building which is already garrisoned by other
			ContainModuleInterface * contain = 	target->getContain();
			if ( contain && contain->isGarrisonable() )
			{
				contain->removeAllContained( TRUE );
				break; // we do not want to set a neutral building to our team if we are not in it, that would be confusing!
			}

			//Play the "building stolen" EVA event if the local player is the victim!
			if( target && target->isLocallyViewed() )
			{
				TheEva->setShouldPlay( EVA_BuildingStolen );
			}

			target->defect( object->getControllingPlayer()->getDefaultTeam(), 1); // one frame of flash!

			SpecialPowerModuleInterface *spmInterface = getMySPM();
			if (spmInterface && spTemplate->getSpecialPowerType() == SPECIAL_BLACKLOTUS_CAPTURE_BUILDING )
			{
				// only for black lotus, not infantry capture which resets in contunueprep()
				spmInterface->startPowerRecharge();
			}
			break;

			break;
		}
		case SPECIAL_BLACKLOTUS_STEAL_CASH_HACK:
		{
			Object *target = TheGameLogic->findObjectByID( m_targetID );

			//sanity
			if( !target )
			{
				return;
			}

			//Steal cash from the other team!
			Money *targetMoney = target->getControllingPlayer()->getMoney();
			Money *objectMoney = object->getControllingPlayer()->getMoney();
			if( targetMoney && objectMoney )
			{
				UnsignedInt cash = targetMoney->countMoney();
#if RETAIL_COMPATIBLE_CRC || PRESERVE_HARDCODED_BLACK_LOTUS_CASH_HACK
				UnsignedInt desiredAmount = 1000;
#else
				UnsignedInt desiredAmount = data->m_effectValue;
#endif
				//Check to see if they have the cash, otherwise, take the remainder!
				cash = min( desiredAmount, cash );
				if( cash > 0 )
				{
					//Steal the cash
					targetMoney->withdraw( cash );
					objectMoney->deposit( cash );
					Player* controller = object->getControllingPlayer();
					if (controller)
						controller->getScoreKeeper()->addMoneyEarned( cash );

					//Play the "cash stolen" EVA event if the local player is the victim!
					if( target && target->isLocallyViewed() )
					{
						TheEva->setShouldPlay( EVA_CashStolen );
					}

					//Display cash income floating over the blacklotus
					UnicodeString moneyString;
					moneyString.format( TheGameText->fetch( "GUI:AddCash" ), cash );
					Coord3D pos;
					pos.set( *object->getPosition() );
					pos.z += 20.0f; //add a little z to make it show up above the unit.
					TheInGameUI->addFloatingText( moneyString, &pos, GameMakeColor( 0, 255, 0, 255 ) );

					//Display cash lost floating over the target
					moneyString.format( TheGameText->fetch( "GUI:LoseCash" ), cash );
					pos.set( *target->getPosition() );
					pos.z += 30.0f; //add a little z to make it show up above the unit.
					TheInGameUI->addFloatingText( moneyString, &pos, GameMakeColor( 255, 0, 0, 255 ) );
				}
			}
			break;
		}

		case SPECIAL_REMOTE_CHARGES:
		{
			static NameKeyType key_StickyBombUpdate = NAMEKEY( "StickyBombUpdate" );
			if( m_targetID == INVALID_ID && !m_targetPos.x && !m_targetPos.y && !m_targetPos.z )
			{
				//If there is no target object nor position, then we are detonating the existing charges.
				std::list<ObjectID>::iterator i;
				for( i = m_specialObjectIDList.begin(); i != m_specialObjectIDList.end(); ++i )
				{
					Object *specialObject = TheGameLogic->findObjectByID( *i );
					if( specialObject )
					{
						StickyBombUpdate *update = (StickyBombUpdate*)specialObject->findUpdateModule( key_StickyBombUpdate );
						if( update )
						{
							//Blow it up!!!
							update->detonate();
							okToLoseStealth = FALSE;
							//Note: while the objects are detonating, they will still exist in the game.
							//Our update will be responsible for validating their existence and removing them..
							//in case either the enemy player cleans one up, or after it's gone.
						}
					}
				}
			}
			else
			{
				//Place a new charge.
				Object *target = TheGameLogic->findObjectByID( m_targetID );
				//sanity
				if( !target )
				{
					return;
				}

				Object *charge = createSpecialObject();
				if( charge )
				{
					StickyBombUpdate *update = (StickyBombUpdate*)charge->findUpdateModule( key_StickyBombUpdate );
					if( !update )
					{
						DEBUG_CRASH( 
							("Unit '%s' attempted to place remote charge but the charge '%s' requires a StickyBombUpdate module.",
							object->getTemplate()->getName().str(),
							charge->getTemplate()->getName().str() ) );
						killSpecialObjects();
						return;
					}
					//Setting the producer ID allows the sticky bomb update module to initialize
					//and setup timers, etc.
					update->init( target, object );
				}
			}
			break;
		}

		case SPECIAL_DISGUISE_AS_VEHICLE:
		{
			Object *target = TheGameLogic->findObjectByID( m_targetID );
			if( target )
			{
				StealthUpdate *update = object->getStealth();
				if( update )
				{
					update->disguiseAsObject( target );
				}
			}

			break;
		}
	}

	if( data->m_loseStealthOnTrigger && okToLoseStealth)
	{
		StealthUpdate* stealth = object->getStealth();
		if( stealth )
		{
			stealth->markAsDetected();
		}
	}
}

//-------------------------------------------------------------------------------------------------
Object* SpecialAbilityUpdate::createSpecialObject()
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	Object *specialObject = nullptr;

	if( m_specialObjectEntries == data->m_maxSpecialObjects )
	{
		if( data->m_specialObjectsPersistent )
		{
			//If we are dealing with persistent objects, and we have reached our
			//limit we can have, then don't allow any more to be created....
			//We could add recycling code if need be.. but the logic that handles
			//canDoSpecialPowerXXX should prevent this triggering.
			return nullptr;
		}
		else
		{
			//We've reached our limit with non-persistent object(s), so remove them.
			killSpecialObjects();
		}
	}

	//Get the template of the special object
	const ThingTemplate* thingTemplate = TheThingFactory->findTemplate( data->m_specialObjectName );
	if( thingTemplate )
	{
		//Create a new special object
		specialObject = TheThingFactory->newObject( thingTemplate, getObject()->getTeam() );
		if( specialObject )
		{
			m_specialObjectIDList.push_back( specialObject->getID() );
			m_specialObjectEntries++;
			specialObject->setPosition( getObject()->getPosition() );

			//So we can get experience from it when it blows up (if applicable)
			//specialObject->setProducer( getObject() ); --This causes it to be an enemy which is naughty.
			ExperienceTracker *xpTracker = specialObject->getExperienceTracker();
			if( xpTracker )
			{
				xpTracker->setExperienceSink( getObject()->getID() );
			}
		}
	}
	return specialObject;
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::killSpecialObjects()
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	const SpecialPowerTemplate *spTemplate = data->m_specialPowerTemplate;
	Object *specialObject;

	std::list<ObjectID>::iterator i;
	for( i = m_specialObjectIDList.begin(); i != m_specialObjectIDList.end(); ++i )
	{
		specialObject = TheGameLogic->findObjectByID( *i );
		if( specialObject )
		{
			//Delete the old one!
			TheGameLogic->destroyObject( specialObject );
		}
	}

	//Reset the list
	m_specialObjectIDList.clear();
	m_specialObjectEntries = 0;

	switch( spTemplate->getSpecialPowerType() )
	{
		case SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES:
			//The instant we remove the laser, reset the weaponset!
			Object *object = getObject();
			const Weapon *weapon = object->getWeaponInWeaponSlot( PRIMARY_WEAPON );
			if( weapon )
			{
				// lock it just till the weapon is empty or the attack is "done"
				object->setWeaponLock( PRIMARY_WEAPON, LOCKED_TEMPORARILY );
			}
			break;
	}

}

//-------------------------------------------------------------------------------------------------
UnsignedInt SpecialAbilityUpdate::getSpecialObjectCount() const
{
	return m_specialObjectEntries;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt SpecialAbilityUpdate::getSpecialObjectMax() const
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	return data->m_maxSpecialObjects;
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::validateSpecialObjects()
{
	Object *specialObject;

	std::list<ObjectID>::iterator i = m_specialObjectIDList.begin();

	while( i != m_specialObjectIDList.end() )
	{
		std::list<ObjectID>::iterator prev = i;
		specialObject = TheGameLogic->findObjectByID( *i );
		i++;
		if( !specialObject )
		{
			//Remove it from the list, and decrement our counter.
			m_specialObjectIDList.erase( prev );
			--m_specialObjectEntries;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::finishAbility()
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();

	m_withinStartAbilityRange = false;
	m_packingState = STATE_NONE;

	//The flee range code is used only if the unit has completed a special ability against a target.
	//If we don't have a valid target, then we don't flee -- the specific example is when Colonel Burton
	//uses his remote charge special ability. With a target, he will plant the charge then flee. Without
	//a target, he simply detonates it without fleeing.
	Bool validTarget = m_targetPos.x || m_targetPos.y || m_targetPos.z || m_targetID != INVALID_ID;
	if( data->m_fleeRangeAfterCompletion && validTarget )
	{
		Coord3D pos;
		pos.set( *getObject()->getPosition() );

		AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
		if( ai )
		{
			Coord3D dir;
			dir.set( *getObject()->getUnitDirectionVector2D() );
			dir.normalize();
			dir.scale( data->m_fleeRangeAfterCompletion );

			if( data->m_flipObjectAfterUnpacking || data->m_flipObjectAfterPacking )
			{
				pos.add( dir );
			}
			else
			{
				pos.sub( dir );
			}
			// Now check for mines.  Normally we are fleeing from a bomb we just planted.
			// It is not good to run back towards the previous mine we just planted about
			// the time it explodes. jba.
			Object *obj = getObject();
			if (obj) {
				Player *contPlayer = obj->getControllingPlayer();
				if (contPlayer) {
					PartitionFilterSamePlayer filterPlayer( contPlayer );	// Look for our own mines.
					PartitionFilterAcceptByKindOf filterKind(MAKE_KINDOF_MASK(KINDOF_MINE), KINDOFMASK_NONE);
					PartitionFilter *filters[] = { &filterKind, &filterPlayer, nullptr };
					Object *mine = ThePartitionManager->getClosestObject( &pos, data->m_fleeRangeAfterCompletion, FROM_CENTER_2D, filters );// could be null. this is ok.
					if (mine) {
						dir.set(pos.x-mine->getPosition()->x, pos.y-mine->getPosition()->y, 0);
						dir.normalize();
						dir.scale(data->m_fleeRangeAfterCompletion);
						pos = *mine->getPosition();
						pos.add(dir);
					}
				}
			}


			// this is just a trick: apply a motive force (even zero!) so that
			// AI will think we were already "moving". this causes us to go directly
			// to the moving anim rather than popping into idle for 1 frame (srj)
			PhysicsBehavior* phys = getObject()->getPhysics();
			if (phys)
			{
				Coord3D bogusForce;
				bogusForce.zero();
				phys->applyMotiveForce(&bogusForce);
			}
			ai->aiMoveToPosition( &pos, CMD_FROM_AI );
			Object *target = TheGameLogic->findObjectByID( m_targetID );
			if( target )
			{
				ai->ignoreObstacle( target );
			}
		}
	}
	else
	{
		//Clear any old AI before ending this special ability.
		AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
		if (ai)
	  	ai->aiIdle(CMD_FROM_AI);
	}

	//// Emit finished sound //Moved to StartPacking(), thank you, ML
	//AudioEventRTS event = *getObject()->getTemplate()->getVoiceTaskComplete();
	//event.setObjectID(getObject()->getID());
	//TheAudio->addAudioEvent(&event);

	onExit( false );
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::isFacing()
{
	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if( ai )
	{
		if( !m_facingComplete && m_facingInitiated)
		{
			if( ai->isIdle() )
			{
				//We finished facing the target
				m_facingComplete = true;
				return false;
			}
			//We are still in the process of facing the target
			return true;
		}
	}
	else
	{
		//Something without AI always faces target...
		return true;
	}

	//Either the facing hasn't initiated yet, or we are complete facing.
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SpecialAbilityUpdate::needToFace() const
{
	const AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if( !ai )
	{
		//If we don't have AI, we can't face target!
		return false;
	}

	//Return true if we haven't initiated facing or if we haven't completed it.
	return !m_facingInitiated || !m_facingComplete;
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::startFacing()
{

	Object *target = TheGameLogic->findObjectByID( m_targetID );
	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if( !ai )
	{
		return;
	}

	// First kill it's physics.
	ai->aiIdle( CMD_FROM_AI );
	if (getObject()->getPhysics())
		getObject()->getPhysics()->resetDynamicPhysics();

// NO, do not do this; we promise Update modules that they will be
// called *exactly* once per frame... no more, no less! (srj)
//ai->update();

	m_facingInitiated = true;
	if( target )
	{
		ai->aiFaceObject( target, CMD_FROM_AI );
	}
	else if( m_targetPos.x || m_targetPos.y || m_targetPos.z ) //It's zero if not used...
	{
		ai->aiFacePosition( &m_targetPos, CMD_FROM_AI );
	}
}

//-------------------------------------------------------------------------------------------------
const SpecialPowerTemplate* SpecialAbilityUpdate::getTemplate() const
{
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	return data->m_specialPowerTemplate;
}

//-------------------------------------------------------------------------------------------------
SpecialPowerType SpecialAbilityUpdate::getSpecialPowerType() const
{
	return getTemplate()->getSpecialPowerType();
}

//-------------------------------------------------------------------------------------------------
// This looks for a special object with specified producer ID
//-------------------------------------------------------------------------------------------------
Object* SpecialAbilityUpdate::findSpecialObjectWithProducerID( const Object *target )
{
	Object *specialObject;
	std::list<ObjectID>::iterator i;
	for( i = m_specialObjectIDList.begin(); i != m_specialObjectIDList.end(); ++i )
	{
		specialObject = TheGameLogic->findObjectByID( *i );
		if( specialObject )
		{
			if( specialObject->getProducerID() == target->getID() )
			{
				return specialObject;
			}
		}
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::endPreparation()
{
	getObject()->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_IS_USING_ABILITY ) );
	TheAudio->removeAudioEvent( m_prepSoundLoop.getPlayingHandle() );

	// Based on the special that we just finished preparing (either by failure or success),
	// do we want to keep the "special objects" created? Some specials will -- others won't.
	// Note that persistent specials will not call this until preparation is complete (not
	// recycling).
	const SpecialAbilityUpdateModuleData* data = getSpecialAbilityUpdateModuleData();
	const SpecialPowerTemplate *spTemplate = data->m_specialPowerTemplate;
	switch( spTemplate->getSpecialPowerType() )
	{
		case SPECIAL_TANKHUNTER_TNT_ATTACK:
		case SPECIAL_TIMED_CHARGES:
		case SPECIAL_REMOTE_CHARGES:
		case SPECIAL_DISGUISE_AS_VEHICLE:
			// No, don't delete placed charges.
			// -OR- Not applicable (doesn't use special objects).
			break;

		case SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES:
		case SPECIAL_HACKER_DISABLE_BUILDING:
		case SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK:
		case SPECIAL_BLACKLOTUS_CAPTURE_BUILDING:
		case SPECIAL_BLACKLOTUS_STEAL_CASH_HACK:
		case SPECIAL_INFANTRY_CAPTURE_BUILDING:
			killSpecialObjects();
			break;

	}
}



// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// active
	xfer->xferBool( &m_active );

	// prep frames
	xfer->xferUnsignedInt( &m_prepFrames );

	// anim frames
	xfer->xferUnsignedInt( &m_animFrames );

	// target ID
	xfer->xferObjectID( &m_targetID );

	// target position
	xfer->xferCoord3D( &m_targetPos );

	// location count
	xfer->xferInt( &m_locationCount );

	// special object id list
	xfer->xferSTLObjectIDList( &m_specialObjectIDList );

	// special object entries
	xfer->xferUnsignedInt( &m_specialObjectEntries );

	// no target command
	xfer->xferBool( &m_noTargetCommand );

	// packing state
	xfer->xferUser( &m_packingState, sizeof( PackingState ) );

	// facing initiated
	xfer->xferBool( &m_facingInitiated );

	// facing complete
	xfer->xferBool( &m_facingComplete );

	// within start ability range
	xfer->xferBool( &m_withinStartAbilityRange );

	// do disable fx particles
	xfer->xferBool( &m_doDisableFXParticles );

  // capture flash phase
  xfer->xferReal( &m_captureFlashPhase );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SpecialAbilityUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
