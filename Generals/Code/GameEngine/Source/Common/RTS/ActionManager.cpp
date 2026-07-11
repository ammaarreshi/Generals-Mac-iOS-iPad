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

// FILE: ActionManager.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day
// Desc:   TheActionManager is a convenient place for us to wrap up all sorts of logical
//				 queries about what objects can do in the world and to other objects.  The purpose
//				 of having a central place for this logic assists us in making these logical kind
//				 of queries in the user interface and allows us to use the same code to validate
//				 commands as they come in over the network interface in order to do the
//				 real action.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ActionManager.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "Common/ThingTemplate.h"

#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"

#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/CollideModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/RailroadGuideAIUpdate.h"
#include "GameLogic/Module/RailedTransportDockUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Module/SupplyCenterDockUpdate.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Weapon.h"

#include "GameLogic/ExperienceTracker.h"//LORENZEN


// GLOBAL /////////////////////////////////////////////////////////////////////////////////////////
ActionManager *TheActionManager = nullptr;

// LOCAL //////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static Bool appearsToContainFriendlies(const Object* obj, const Object* otherObject)
{
	// check if the object is a container containing stealth units tricking
	// the player into thinking it isn't actually an enemy.
	const ContainModuleInterface *otherContain = otherObject->getContain();
	if( otherContain )
	{
		const Player *otherPlayer = otherContain->getApparentControllingPlayer(obj->getControllingPlayer());
//	if( otherPlayer && otherPlayer->getRelationship( obj->getTeam() ) != ENEMIES )
// the above test is wrong; we want to know how WE consider THEM, not how THEY consider US
		if (otherPlayer && obj->getTeam()->getRelationship(otherPlayer->getDefaultTeam()) != ENEMIES)
		{
			return TRUE;
		}
	}
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static Bool isObjectShroudedForAction ( const Object *source, const Object *target, CommandSourceType commandSource )
{
	/// @todo - reenable this when we can avoid breaking scripted actions.
	// In order to support ai and scripted actions in singler player, we have to disable this for now.
	// We can re-enable it when we can tell that this is a player generated action.  jba.
//	return false;
	// GS Keeping this comment to show we now have commandSource, so everything should be fine again.

	// The target is only shrouded for action if...

	// the asking player is human
	// the asking impetus is not from a script
	// and the target object is Fogged or worse

	if( source && target && source->getControllingPlayer() )
	{
		if( source->getControllingPlayer()->getPlayerType() == PLAYER_HUMAN
			&& commandSource != CMD_FROM_SCRIPT
			&& target->getShroudedStatus( source->getControllingPlayer()->getPlayerIndex() ) >= OBJECTSHROUD_FOGGED
			)
		{
			return TRUE;
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
ActionManager::ActionManager()
{

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
ActionManager::~ActionManager()
{

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canGetRepairedAt( const Object *obj, const Object *repairDest, CommandSourceType commandSource )
{

	// sanity
	if( obj == nullptr || repairDest == nullptr )
		return FALSE;

	Relationship r = obj->getRelationship(repairDest);

	// only available by our allies
	if( r != ALLIES )
		return FALSE;

	// dead objects cannot be repaired
	if( obj->isEffectivelyDead() )
		return FALSE;

	// nothing can be done with things that are under construction
	if( obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) ||
			repairDest->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return FALSE;

	// Can't get repaired at something being sold
	if( repairDest->testStatus(OBJECT_STATUS_SOLD) )
		return FALSE;

	// only vehicles can go get repaired at something
	if( obj->isKindOf( KINDOF_VEHICLE ) == FALSE )
		return FALSE;

	// vehicles can only be repaired at something that is designated as a repair pad
	if (obj->isKindOf( KINDOF_AIRCRAFT ))
	{
		// aircraft require an airfield.
		if( !obj->isAboveTerrain() ||
					repairDest->isKindOf( KINDOF_FS_AIRFIELD ) == FALSE )
			return FALSE;
	}
	else
	{
		if( repairDest->isKindOf( KINDOF_REPAIR_PAD ) == FALSE )
			return FALSE;
	}

	// if I am at full health, I can't get repair there
	BodyModuleInterface *body = obj->getBodyModule();
	if( body->getHealth() == body->getMaxHealth() )
		return FALSE;

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, repairDest, commandSource))
		return FALSE;

	// all is well, we can be repaired here
	return TRUE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// note that "dest" is typically a building...
Bool ActionManager::canTransferSuppliesAt( const Object *obj, const Object *transferDest )
{

	// sanity
	if( obj == nullptr || transferDest == nullptr )
		return FALSE;

	if( transferDest->isEffectivelyDead() )
	{
		return FALSE;
	}

	// nothing can be done with things that are under construction
	if( obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) ||
			transferDest->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return FALSE;

	// Can't transfer at something being sold
	if( transferDest->testStatus(OBJECT_STATUS_SOLD) )
		return FALSE;

	// I must be something with a Supply Transferring AI interface
	const AIUpdateInterface *ai= obj->getAI();
	if( ai == nullptr )
		return FALSE;

	const SupplyTruckAIInterface* supplyTruck = ai->getSupplyTruckAIInterface();
	if( supplyTruck == nullptr )
		return FALSE;

	// If it is a warehouse, it must have boxes left and not be an enemy
	static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
	SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*)transferDest->findUpdateModule( key_warehouseUpdate );
	if( warehouseModule )
		if( warehouseModule->getBoxesStored() == 0 || transferDest->getRelationship( obj ) == ENEMIES )
			return FALSE;

	// if it is a supply center, I must have boxes, and must be controlled by the same player
	// (not merely an ally... otherwise you may find yourself funding your allies. ick.)
	static const NameKeyType key_centerUpdate = NAMEKEY("SupplyCenterDockUpdate");
	SupplyCenterDockUpdate *centerModule = (SupplyCenterDockUpdate*)transferDest->findUpdateModule( key_centerUpdate );
	if( centerModule  )
		if( supplyTruck->getNumberBoxes() == 0  || transferDest->getControllingPlayer() != obj->getControllingPlayer() )
			return FALSE;

	// if he is not a warehouse or a center, then shut the hell up
	if( (warehouseModule == nullptr)  &&  (centerModule == nullptr) )
		return FALSE;

	// We do not check ClearToApproach, as it is a temporary failure that is handled
	// in the state logic.  This function is for Legality, not conditionals.
	// however, we DO check for the unit to be available (cf Chinook) (srj)
	if (!supplyTruck->isAvailableForSupplying())
		return FALSE;

	// if the target is in the shroud, we can't do anything
//	if (isObjectShroudedForAction(obj, transferDest))
//		return FALSE;
	//Commented out to show it is an intentional difference to most commands.

	// Fogged is okay for player, and anything is okay for AI.
	Player *objPlayer = obj->getControllingPlayer();
	if( objPlayer )
	{
		if( objPlayer->getPlayerType() == PLAYER_HUMAN &&
			transferDest->getShroudedStatus( objPlayer->getPlayerIndex() ) == OBJECTSHROUD_SHROUDED )
		{
			return FALSE;
		}
	}

	// all is well, we can transfer here
	return TRUE;

}

// ------------------------------------------------------------------------------------------------
/** Can object 'obj' dock with object 'dockDest' for any reason */
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canDockAt( const Object *obj, const Object *dockDest, CommandSourceType commandSource )
{

	// look for a dock interface
	DockUpdateInterface *di = nullptr;
	for (BehaviorModule **u = dockDest->getBehaviorModules(); *u; ++u)
	{
		if ((di = (*u)->getDockUpdateInterface()) != nullptr)
			break;
	}
	if( di == nullptr )
		return FALSE;  // no dock update interface, can't possibly dock

/*
	// can't dock if the dock is closed
	if( di->isDockOpen() == FALSE )
		return FALSE;
*/

	// transferring supplies is a valid docking action
	if( canTransferSuppliesAt( obj, dockDest ) == TRUE )
		return TRUE;

	// units and infantry can dock with a railed transport
	static const NameKeyType key = NAMEKEY( "RailedTransportDockUpdate" );
	RailedTransportDockUpdate *fdu = (RailedTransportDockUpdate *)dockDest->findUpdateModule( key );
	if( fdu )
	{

		if( obj->isKindOf( KINDOF_VEHICLE ) || obj->isKindOf( KINDOF_INFANTRY ) )
			return TRUE;

	}

	// cannot dock
	return FALSE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canGetHealedAt( const Object *obj, const Object *healDest, CommandSourceType commandSource )
{

	// sanity
	if( obj == nullptr || healDest == nullptr )
		return FALSE;

	Relationship r = obj->getRelationship(healDest);

	// only available by our allies
	if( r != ALLIES )
		return FALSE;

	// dead objects cannot be healed
	if( healDest->isEffectivelyDead() )
		return FALSE;

	// nothing can be done with things that are under construction
	if( obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) ||
			healDest->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return FALSE;

	// Can't get healed at something being sold
	if( healDest->testStatus(OBJECT_STATUS_SOLD) )
		return FALSE;

	// only infantry can go get "healed" somewhere (vehicles get "repaired")
	if( obj->isKindOf( KINDOF_INFANTRY ) == FALSE )
		return FALSE;

	// infantry can only be healed at something that is designated as a heal pad
	if( healDest->isKindOf( KINDOF_HEAL_PAD ) == FALSE )
		return FALSE;

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, healDest, commandSource))
		return FALSE;

	BodyModuleInterface *body = obj->getBodyModule();
	if( body && body->getHealth() == body->getMaxHealth() )
	{
		//No point in healing if you have full health!
		return FALSE;
	}

	// all is well, we can be healed here
	return TRUE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canRepairObject( const Object *obj, const Object *objectToRepair, CommandSourceType commandSource )
{

	// sanity
	if( obj == nullptr || objectToRepair == nullptr )
		return FALSE;

	Relationship r = obj->getRelationship(objectToRepair);

	// you can only repair allies, we ignore this restriction for bridges
	// srj sez: nope, allow neutral too, so civ bldgs can be repaired
	// GS repairing bridges cut 12/12/02 , so this check is just no to enemies
	if( r == ENEMIES )
		return FALSE;

	//
	// can't repair dead things ... the exception is bridges and bridge towers, which can
	// be destroyed and die, but can be repaired to bring the bring "back to life"
	//
	// GS and again, repairing bridges is cut, so this is just a dead check
	if( objectToRepair->isEffectivelyDead() )
	{
		return FALSE;
	}

	//GS So here's the ensuring that they can't be repaired
	if( objectToRepair->isKindOf(KINDOF_BRIDGE) || objectToRepair->isKindOf(KINDOF_BRIDGE_TOWER) )
		return FALSE;

	// nothing can be done with things that are under construction
	if( obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) ||
			objectToRepair->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return FALSE;

	// we cannot manually repair things that are regeneration holes
	if( objectToRepair->isKindOf( KINDOF_REBUILD_HOLE ) == TRUE )
		return FALSE;

	// only Dozers can go repair things
	if( obj->isKindOf( KINDOF_DOZER ) == FALSE )
		return FALSE;

	// dozers can only repair buildings
	if( objectToRepair->isKindOf( KINDOF_STRUCTURE ) == FALSE )
		return FALSE;

	// get the body module from the object to repair
	BodyModuleInterface *body = objectToRepair->getBodyModule();

	// buildings that are at full health cannot be repaired
	if( body->getHealth() == body->getMaxHealth() )
		return FALSE;

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToRepair, commandSource))
		return FALSE;

	if( obj->getContainedBy() )
	{
		// We can't heal things while in a transport (especially our own transport, you cheater)
		return FALSE;
	}

	return TRUE;

}

// ------------------------------------------------------------------------------------------------
/** Can 'obj' resume the construction of 'objectBeingConstructed' */
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canResumeConstructionOf( const Object *obj,
																						 const Object *objectBeingConstructed,
																						 CommandSourceType commandSource )
{

	// sanity
	if( obj == nullptr || objectBeingConstructed == nullptr )
		return FALSE;

	// only dozers or workers can resume construction of things
	if( obj->isKindOf( KINDOF_DOZER ) == FALSE )
		return FALSE;

	// TheSuperHackers @bugfix Stubbjax 06/01/2026 Ensure only the owner of the construction can resume it.
#if RETAIL_COMPATIBLE_CRC
	Relationship r = obj->getRelationship(objectBeingConstructed);

	// only available to our allies
	if( r != ALLIES )
		return FALSE;
#else
	if (obj->getControllingPlayer() != objectBeingConstructed->getControllingPlayer())
		return FALSE;
#endif

	// if the objectBeingConstructed is not actually under construction we can't resume that!
	if( !objectBeingConstructed->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return FALSE;

	// dead things can do nothing
	if( obj->isEffectivelyDead() )
	{
		return FALSE;
	}

	//
	// if the object being constructed is actively being built by another dozer we cannot
	// add more effectiveness to the construction so we'll say no (this might change for workers
	// in the future)
	//
	Object *builder = TheGameLogic->findObjectByID( objectBeingConstructed->getBuilderID() );
#if RETAIL_COMPATIBLE_CRC || PRESERVE_BUILDING_RESUMPTION_DELAY
	if( builder )
#else
	// TheSuperHackers @bugfix Stubbjax 18/11/2025 Allow scaffold to be immediately resumed after builder death.
	if (builder && !builder->isEffectivelyDead())
#endif
	{
		AIUpdateInterface *ai = builder->getAI();
		DEBUG_ASSERTCRASH( ai, ("Builder object does not have an AI interface!") );

		if( ai )
		{
			DozerAIInterface *dozerAI = ai->getDozerAIInterface();
			DEBUG_ASSERTCRASH( dozerAI, ("Builder object doest not have a DozerAI interface!") );

			if( dozerAI )
			{

				if( dozerAI->getCurrentTask() == DOZER_TASK_BUILD &&
						dozerAI->getTaskTarget( DOZER_TASK_BUILD ) == objectBeingConstructed->getID() )
					return FALSE;

			}

		}

	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectBeingConstructed, commandSource))
		return FALSE;

	//
	// all is well, the objectBeingConstructeds' builder object has gone away or is no longer
	// building the object anymore
	//

	return TRUE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canEnterObject( const Object *obj, const Object *objectToEnter, CommandSourceType commandSource, CanEnterType mode )
{

	// sanity
	if( obj == nullptr || objectToEnter == nullptr )
		return FALSE;

	if( obj == objectToEnter )
	{
		//You can't contain yourself (crash fix for pow truck reselection)
		return FALSE;
	}

	// can't enter dead things
	if( objectToEnter->isEffectivelyDead() )
		return FALSE;

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToEnter, commandSource))
		return FALSE;

	// nothing can be done with things that are under construction
	if( obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) ||
			objectToEnter->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{
		return FALSE;
	}

	// Can't enter something being sold
	if( objectToEnter->testStatus(OBJECT_STATUS_SOLD) )
		return FALSE;
	if ( obj->isKindOf( KINDOF_IGNORED_IN_GUI )  //As in, Angry Mob Members, Cargo Planes
		|| obj->isKindOf( KINDOF_MOB_NEXUS )
		|| objectToEnter->isKindOf( KINDOF_IGNORED_IN_GUI ) )  // As in Cargo Planes
	{

		return FALSE;
	}

	if( obj->isKindOf( KINDOF_STRUCTURE ) || obj->isKindOf( KINDOF_IMMOBILE ) )
	{
		//Structures or immobiles can't garrison
		return FALSE;
	}

	// Special case for unmanned vehicles. Any infantry unit can take over any unmanned vehicle!
	if( obj->isKindOf( KINDOF_INFANTRY ) && objectToEnter->isDisabledByType( DISABLED_UNMANNED ) )
	{
		return TRUE;
	}

	// Special case for aircraft.
	if( obj->isKindOf( KINDOF_AIRCRAFT ) && objectToEnter->isKindOf( KINDOF_FS_AIRFIELD ) )
	{
		if (!obj->isAboveTerrain())
			return FALSE;

		if( obj->getControllingPlayer() == objectToEnter->getControllingPlayer() )
		{
			//Kris -- added code to prevent aircraft from landing in any airstrips other than their own!

			/// @todo srj -- this is horrible, but expedient.
			for (BehaviorModule** i = objectToEnter->getBehaviorModules(); *i; ++i)
			{
				ParkingPlaceBehaviorInterface* pp = (*i)->getParkingPlaceBehaviorInterface();
				if (pp == nullptr)
					continue;

				if (pp->hasReservedSpace(obj->getID()))
					return TRUE;

				if (pp->shouldReserveDoorWhenQueued(obj->getTemplate()) && pp->hasAvailableSpaceFor(obj->getTemplate()))
					return TRUE;
			}
		}
		return FALSE;
	}

	// first, see if we'd like to collide with 'other'
	for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
	{
		CollideModuleInterface* collide = (*m)->getCollide();
		if (!collide)
			continue;

		if( collide->wouldLikeToCollideWith( objectToEnter ) )
		{
			//I thought this was a little confusing that it would return TRUE here before
			//getting to any of the other checks. The key is that it usually doesn't return
			//TRUE because most things aren't trying to collide with objects. This is different
			//for terrorist converting carbombs, and pilots entering vehicles. In these cases,
			//the vehicles don't have transport capacities, therefore returning true here
			//foregoes that checking later on.
			return TRUE;
		}
	}

#ifdef ALLOW_SURRENDER
	if( objectToEnter->isKindOf( KINDOF_PRISON ) )
	{
		//We can't manually enter a prison!
		return FALSE;
	}
#endif

#ifdef ALLOW_SURRENDER
	if( objectToEnter->isKindOf( KINDOF_POW_TRUCK ) )
	{
		//We can't manually enter POWTruck, either!
		return FALSE;
	}
#endif

	// make sure our objectToEnter has a contain module.
	ContainModuleInterface *contain = objectToEnter->getContain();
	if( !contain )
	{
		return FALSE;
	}

	if( contain->isHealContain() )
	{
		BodyModuleInterface *body = obj->getBodyModule();
		if( body->getHealth() == body->getMaxHealth() )
		{
			//This container is only used for the purposes of healing and we cannot
			//enter it with full health. This is not a normal container.
			return FALSE;
		}
	}

	if (mode == COMBATDROP_INTO)
	{
		// we don't care about valid container-ness, but we DO care about faction structures...
		// we aren't allowed to combat drop into them
		if (objectToEnter->isFactionStructure())
			return FALSE;
	}
	else
	{
		Bool checkCapacity = (mode == CHECK_CAPACITY);
		Int containCount = contain->getContainCount();
		Int stealthContainCount = contain->getStealthUnitsContained();
		Int nonStealthContainCount = containCount - stealthContainCount;

		// not ours... must do special checks.
		if (objectToEnter->getControllingPlayer() != obj->getControllingPlayer())
		{
			// not empty... can't do it.
			if (nonStealthContainCount > 0)
				return FALSE;

			// faction structure... can't do it.
			if (objectToEnter->isFactionStructure())
				return FALSE;

			// it's stealth-garrisoned... ignore check-cap and fall thru to
			// normal isValid test.
			if (stealthContainCount > 0 && nonStealthContainCount == 0)
				checkCapacity = FALSE;
		}

		// if our transport slot count is zero, we can't be transported. so punt.
		/// @todo srj -- seems like we should check always (not just for checkCap), but scared to change now -- check later
		if( checkCapacity && obj->getTransportSlotCount() == 0 )
		{
			return FALSE;
		}

		// finally: make sure that objectToEnter is a valid container for obj
		if( contain->isValidContainerFor( obj, checkCapacity ) == FALSE )
		{
			return FALSE;
		}
	}

	return TRUE;

}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
CanAttackResult ActionManager::getCanAttackObject( const Object *obj, const Object *objectToAttack, CommandSourceType commandSource, AbleToAttackType attackType )
{
	// sanity
	if( !obj || !objectToAttack || obj->isEffectivelyDead() || objectToAttack->isEffectivelyDead() || objectToAttack == obj )
	{
		return ATTACKRESULT_NOT_POSSIBLE;
	}

	if( !obj->isAbleToAttack() )
	{
		return ATTACKRESULT_NOT_POSSIBLE;
	}

	//has any weapons that are capable of inflicting damage. Special damage types are rejected
	//such as hack weapons... others can be added.
	CanAttackResult result = obj->getAbleToAttackSpecificObject( attackType, objectToAttack, commandSource );
	if( result != ATTACKRESULT_NOT_POSSIBLE  )
	{
		if( result == ATTACKRESULT_INVALID_SHOT && obj->isKindOf( KINDOF_DOZER ) )
		{
			//For the case of dozers, we don't ever want to see an attack cursor
			//unless it's valid on a mine.
			const Weapon *weapon = obj->getCurrentWeapon();
			if( weapon && weapon->getDamageType() == DAMAGE_DISARM )
			{
				return ATTACKRESULT_NOT_POSSIBLE;
			}
		}
		return result;
	}

	//Special case code for stinger sites: Stinger sites have no weapons, instead -- their spawns are the weapons, in this case the
	//stinger soldiers.
	if( obj->isKindOf( KINDOF_SPAWNS_ARE_THE_WEAPONS ) )
	{
		//Look at the spawn behavior and evaluate them!
		SpawnBehaviorInterface *spawnInterface = obj->getSpawnBehaviorInterface();
		if( spawnInterface )
		{
			//We found the spawn interface, now get the closest slave to the target.
			Object *slave = spawnInterface->getClosestSlave( objectToAttack->getPosition() );

			if( slave )
			{
				result = slave->getAbleToAttackSpecificObject( attackType, objectToAttack, commandSource );
				if( result != ATTACKRESULT_NOT_POSSIBLE )
				{
					return result;
				}
			}
		}
	}

	return ATTACKRESULT_NOT_POSSIBLE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canConvertObjectToCarBomb( const Object *obj, const Object *objectToConvert, CommandSourceType commandSource )
{

	// sanity
	if( obj == nullptr || objectToConvert == nullptr )
	{
		return FALSE;
	}

	if( objectToConvert->isEffectivelyDead() )
	{
		return FALSE;
	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToConvert, commandSource))
		return FALSE;

	// first, see if we'd like to collide with 'other'
	for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
	{
		CollideModuleInterface* collide = (*m)->getCollide();
		if (!collide)
			continue;

		if( collide->wouldLikeToCollideWith( objectToConvert ) && collide->isCarBombCrateCollide() )
		{
			return TRUE;
		}
	}
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canHijackVehicle( const Object *obj, const Object *objectToHijack, CommandSourceType commandSource ) //LORENZEN
{

	int foo = 10;
	++foo;

	// sanity
	if( obj == nullptr || objectToHijack == nullptr )
	{
		return FALSE;
	}

	//Make sure it's alive.
	if( objectToHijack->isEffectivelyDead() )
	{
		return FALSE;
	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToHijack, commandSource))
	{
		return FALSE;
	}

	Relationship r = obj->getRelationship(objectToHijack);
	//Only hijack enemy objects
	if( r != ENEMIES )
	{
		return FALSE;
	}

	//Make sure target is a vehicle.
	if( ! objectToHijack->isKindOf( KINDOF_VEHICLE ) )
	{
		return FALSE;
	}

	//Silly hijacker, you can't hijack that plane from the ground!
	//if( objectToHijack->isAirborneTarget() )
	//{
	//	return FALSE;
	//}

	//Kris -- Hijackers can no longer hijack any aircraft.
	if( objectToHijack->isKindOf( KINDOF_AIRCRAFT ) )
	{
		return FALSE;
	}

// Dustin asked for this to be removed, 12/13... ML
	//Elite and heroic units are immune to this kind of attack
//	VeterancyLevel veterancyLevel = objectToHijack->getVeterancyLevel();
//	if( veterancyLevel >= LEVEL_ELITE )
//	{
//		return FALSE;
//	}

	// last, see if we'd like to collide with 'objectToHijack'
	for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
	{
		CollideModuleInterface* collide = (*m)->getCollide();
		if (!collide)
			continue;

		if( collide->wouldLikeToCollideWith( objectToHijack ) && collide->isHijackedVehicleCrateCollide() )
		{
			return TRUE;
		}
	}

	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canMakeObjectDefector( const Object *obj, const Object *objectToMakeDefector, CommandSourceType commandSource ) //LORENZEN
{
	// sanity
	if( obj == nullptr || objectToMakeDefector == nullptr )
	{
		return FALSE;
	}

	Relationship r = obj->getRelationship(objectToMakeDefector);

	//Only make defectors of enemy objects
	if( r != ENEMIES )
	{
		return FALSE;
	}

	//Make sure it's alive.
	if( objectToMakeDefector->isEffectivelyDead() )
	{
		return FALSE;
	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToMakeDefector, commandSource))
	{
		return FALSE;
	}


	return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

Bool ActionManager::canCaptureBuilding( const Object *obj, const Object *objectToCapture, CommandSourceType commandSource )
{

	// sanity
	if( obj == nullptr || objectToCapture == nullptr )
		return FALSE;

	//Make sure our object has the capability of performing this special ability.

	Bool isOwnerBlackLotus = obj->hasSpecialPower( SPECIAL_BLACKLOTUS_CAPTURE_BUILDING );

	if( !obj->hasSpecialPower( SPECIAL_INFANTRY_CAPTURE_BUILDING ) && !isOwnerBlackLotus)
	{
		return false;
	}

	if( objectToCapture->isKindOf( KINDOF_IMMUNE_TO_CAPTURE ) )
	{
		return false;
	}

//  This is the althernate way to one-at-a-time BlackLotus' specials; we'll keep it commented her until Dustin decides, or until 12/10/02
//	if ( isOwnerBlackLotus )
//	{
//		SpecialPowerModuleInterface *disableSPI = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK );
//		if ( disableSPI && disableSPI->isBusy() )
//			return FALSE;
//		SpecialPowerModuleInterface *cashSPI = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_STEAL_CASH_HACK );
//		if ( cashSPI && cashSPI->isBusy() )
//			return FALSE;
//	}


	SpecialPowerModuleInterface *spInterface = obj->findSpecialPowerModuleInterface( SPECIAL_INFANTRY_CAPTURE_BUILDING );
	if (!spInterface)
		spInterface = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_CAPTURE_BUILDING );
	if (!spInterface)
		return false;

	if( spInterface->getPercentReady() < 1.0f )
	{
		// Special not ready or non-existent.
		return false;
	}

	// can't capture dead things.
	if (objectToCapture->isEffectivelyDead())
	{
		return FALSE;
	}

	// Make sure we are targeting a building!
	if( !objectToCapture->isKindOf( KINDOF_STRUCTURE ) )
	{
		return FALSE;
	}

	// can't capture things that are under construction, or sold.
	if (objectToCapture->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ||
			objectToCapture->testStatus(OBJECT_STATUS_SOLD))
	{
		return FALSE;
	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToCapture, commandSource))
		return FALSE;

	Relationship r = obj->getRelationship(objectToCapture);

	// ensure that it's capturable, and not allied
	// exception: we can always capture enemy bldgs, regardless of kindof
	if (!(r == ENEMIES || (objectToCapture->isKindOf(KINDOF_CAPTURABLE) && r != ALLIES)))
		return false;

	//If the enemy unit is stealthed and not detected, then we can't capture it!
	if( objectToCapture->testStatus( OBJECT_STATUS_STEALTHED ) &&
			!objectToCapture->testStatus( OBJECT_STATUS_DETECTED ) )
	{
		return FALSE;
	}

	// if it's garrisoned already, we cannot capture it.
	// (unless it's just stealth-garrisoned.)
	ContainModuleInterface *contain = objectToCapture->getContain();
	if (contain != nullptr && contain->isGarrisonable())
	{
		Int containCount = contain->getContainCount();
		Int stealthContainCount = contain->getStealthUnitsContained();
		Int nonStealthContainCount = containCount - stealthContainCount;
		if (nonStealthContainCount > 0)
			return FALSE;
	}

	// Also check if the object is a container containing stealth units, tricking
	// the player into thinking it isn't actually an enemy.
	if (appearsToContainFriendlies(obj, objectToCapture))
		return FALSE;

  return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canDisableVehicleViaHacking( const Object *obj, const Object *objectToHack, CommandSourceType commandSource, Bool checkSourceRequirements)
{
	// sanity
	if( obj == nullptr || objectToHack == nullptr )
		return FALSE;

	if (checkSourceRequirements)
	{
		//Make sure our object has the capability of performing this special ability.
		if( !obj->hasSpecialPower( SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK ) )
		{
			return false;
		}
	}

//  This is the althernate way to one-at-a-time BlackLotus' specials; we'll keep it commented her until Dustin decides, or until 12/10/02
//	SpecialPowerModuleInterface *captureSPI = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_CAPTURE_BUILDING );
//	if ( captureSPI && captureSPI->isBusy() )
//		return FALSE;
//	SpecialPowerModuleInterface *cashSPI = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_STEAL_CASH_HACK );
//	if ( cashSPI && cashSPI->isBusy() )
//		return FALSE;


	SpecialPowerModuleInterface *spInterface = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK );
	if (checkSourceRequirements)
	{
		if( !spInterface || spInterface->getPercentReady() < 1.0f )
		{
			//Special not ready or non-existent.
			return FALSE;
		}
	}

	if( objectToHack->isEffectivelyDead() )
	{
		return FALSE;
	}

	if( objectToHack->isKindOf( KINDOF_AIRCRAFT ) || objectToHack->isAirborneTarget() )
	{
		return false;
	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToHack, commandSource))
		return FALSE;

	Relationship r = obj->getRelationship(objectToHack);

	// Make sure object is an enemy
	if( r == ENEMIES )
	{

		//Make sure we are targeting a building!
		if( !objectToHack->isKindOf( KINDOF_VEHICLE ) )
		{
			return FALSE;
		}

		//If the enemy unit is stealthed and not detected, then we can't attack it!
	if( objectToHack->testStatus( OBJECT_STATUS_STEALTHED ) &&
			!objectToHack->testStatus( OBJECT_STATUS_DETECTED ) )
		{
			return FALSE;
		}

		//Also check if the object is a container containing stealth units tricking
		//the player into thinking it isn't actually an enemy.
		if (appearsToContainFriendlies(obj, objectToHack))
			return FALSE;

		return TRUE;
	}
	return FALSE;
}

#ifdef ALLOW_SURRENDER
// ------------------------------------------------------------------------------------------------
/** Can 'obj' pick up the prisoner 'prisoner' */
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canPickUpPrisoner( const Object *obj, const Object *prisoner, CommandSourceType commandSource )
{

	// sanity
	if( obj == nullptr || prisoner == nullptr )
		return FALSE;

	// only pow trucks can pick up anything
	if( obj->isKindOf( KINDOF_POW_TRUCK ) == FALSE )
		return FALSE;

	// only infantry can be picked up
	if( prisoner->isKindOf( KINDOF_INFANTRY ) == FALSE )
		return FALSE;

	// prisoner cannot be contained inside anything
	if( prisoner->getContainedBy() )
		return FALSE;

	// prisoner must be in a surrendered state
	const AIUpdateInterface *ai = prisoner->getAI();
	if( ai == nullptr || ai->isSurrendered() == FALSE )
		return FALSE;

	// prisoner must have been put in a surrendered state by our own player
	// (or be surrendered to "everyone")
	Int idx = ai->getSurrenderedPlayerIndex();
	Player* surrenderedToPlayer = (idx >= 0) ? ThePlayerList->getNthPlayer(idx) : nullptr;
	if (surrenderedToPlayer != nullptr && surrenderedToPlayer != obj->getControllingPlayer())
		return FALSE;

	// we must be enemies
	if( obj->getRelationship( prisoner ) != ENEMIES )
		return FALSE;

	return TRUE;

}
#endif

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canStealCashViaHacking( const Object *obj, const Object *objectToHack, CommandSourceType commandSource )
{
	// sanity
	if( obj == nullptr || objectToHack == nullptr )
		return FALSE;

	//Make sure our object has the capability of performing this special ability.
	if( !obj->hasSpecialPower( SPECIAL_BLACKLOTUS_STEAL_CASH_HACK ) )
	{
		return false;
	}

//  This is the althernate way to one-at-a-time BlackLotus' specials; we'll keep it commented her until Dustin decides, or until 12/10/02
//	SpecialPowerModuleInterface *captureSPI = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_CAPTURE_BUILDING );
//	if ( captureSPI && captureSPI->isBusy() )
//		return FALSE;
//	SpecialPowerModuleInterface *disableSPI = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK );
//	if ( disableSPI && disableSPI->isBusy() )
//		return FALSE;


	SpecialPowerModuleInterface *spInterface = obj->findSpecialPowerModuleInterface( SPECIAL_BLACKLOTUS_STEAL_CASH_HACK );
	if( !spInterface || spInterface->getPercentReady() < 1.0f )
	{
		//Special not ready or non-existent.
		return false;
	}

	if( objectToHack->isEffectivelyDead() )
	{
		return FALSE;
	}

	if( objectToHack->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{
		return FALSE;
	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToHack, commandSource))
		return FALSE;

	Relationship r = obj->getRelationship(objectToHack);

	// Make sure object is an enemy
	if( r == ENEMIES )
	{

		//Make sure we are targeting something that contains cash!
		if( !objectToHack->isKindOf( KINDOF_CASH_GENERATOR ) )
		{
			return FALSE;
		}

		//Make sure object isn't under construction!
		if( objectToHack->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		{
			return FALSE;
		}

		//Make sure the building is considered hackable (temp: using capturable)
		if( !objectToHack->isKindOf( KINDOF_CAPTURABLE ) || objectToHack->isKindOf( KINDOF_REBUILD_HOLE ) )
		{
			return FALSE;
		}

		//If the enemy unit is stealthed and not detected, then we can't attack it!
	if( objectToHack->testStatus( OBJECT_STATUS_STEALTHED ) &&
			!objectToHack->testStatus( OBJECT_STATUS_DETECTED ) )
		{
			return FALSE;
		}

		//Also check if the object is a container containing stealth units tricking
		//the player into thinking it isn't actually an enemy.
		if (appearsToContainFriendlies(obj, objectToHack))
			return FALSE;

		return TRUE;
	}
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canDisableBuildingViaHacking( const Object *obj, const Object *objectToHack, CommandSourceType commandSource )
{
	// sanity
	if( obj == nullptr || objectToHack == nullptr )
		return FALSE;

	//Make sure our object has the capability of performing this special ability.
	if( !obj->hasSpecialPower( SPECIAL_HACKER_DISABLE_BUILDING ) )
	{
		return FALSE;
	}

	SpecialPowerModuleInterface *spInterface = obj->findSpecialPowerModuleInterface( SPECIAL_HACKER_DISABLE_BUILDING );
	if( !spInterface || spInterface->getPercentReady() < 1.0f )
	{
		//Special not ready or non-existent.
		return FALSE;
	}

	if( objectToHack->isEffectivelyDead() )
	{
		return FALSE;
	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToHack, commandSource))
		return FALSE;

	Relationship r = obj->getRelationship(objectToHack);

	// Make sure object is an enemy
	if( r != ENEMIES )
		return FALSE;

	//Make sure we are targeting a building!
	if( !objectToHack->isKindOf( KINDOF_STRUCTURE ) )
	{
		return FALSE;
	}

	//Make sure the building is considered hackable (temp: using capturable)
	// An exception is any TechFactionBuilding that is not explicitly immune to capture
	if( ( !objectToHack->isKindOf( KINDOF_CAPTURABLE ) || objectToHack->isKindOf( KINDOF_REBUILD_HOLE ) ) &&
		! (objectToHack->isKindOf(KINDOF_FS_TECHNOLOGY) && ! objectToHack->isKindOf(KINDOF_IMMUNE_TO_CAPTURE)) )
	{
		return FALSE;
	}


	if ( objectToHack->isKindOf( KINDOF_REBUILD_HOLE ) || objectToHack->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ))
		return FALSE;


	//If the enemy unit is stealthed and not detected, then we can't attack it!
	if( objectToHack->testStatus( OBJECT_STATUS_STEALTHED ) &&
			!objectToHack->testStatus( OBJECT_STATUS_DETECTED ) )
	{
		return FALSE;
	}

	//Also check if the object is a container containing stealth units tricking
	//the player into thinking it isn't actually an enemy.
	if (appearsToContainFriendlies(obj, objectToHack))
		return FALSE;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
Bool ActionManager::canBribeUnit( const Object *obj, const Object *objectToBribe, CommandSourceType commandSource )
{
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
Bool ActionManager::canCutBuildingPower( const Object *obj, const Object *building, CommandSourceType commandSource )
{
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canSnipeVehicle( const Object *obj, const Object *objectToSnipe, CommandSourceType commandSource )
{
	//Sanity check
	if( obj == nullptr || objectToSnipe == nullptr )
	{
		return FALSE;
	}

	//Make sure it's alive.
	if( objectToSnipe->isEffectivelyDead() )
	{
		return FALSE;
	}

	// if the target is in the shroud, we can't do anything
	if (isObjectShroudedForAction(obj, objectToSnipe, commandSource))
		return FALSE;

	Relationship r = obj->getRelationship(objectToSnipe);

	if( r == ENEMIES )
	{
		//Make sure target is a vehicle.
		if( !objectToSnipe->isKindOf( KINDOF_VEHICLE ) )
		{
			return FALSE;
		}

		//Make sure object is not flying
		if( objectToSnipe->isAirborneTarget() )
		{
			return false;
		}

		//Make sure the vehicle is manned!
		if( objectToSnipe->isDisabledByType( DISABLED_UNMANNED ) )
		{
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canDoSpecialPowerAtLocation( const Object *obj, const Coord3D *loc, CommandSourceType commandSource, const SpecialPowerTemplate *spTemplate, const Object *objectInWay, UnsignedInt commandOptions, Bool checkSourceRequirements )
{
	if (checkSourceRequirements)
	{
		//First check, if our object can do this special power.
		if( !obj->hasSpecialPower( spTemplate->getSpecialPowerType() ) )
		{
			return false;
		}
	}

	SpecialPowerModuleInterface *mod = obj->getSpecialPowerModule( spTemplate );
	if( mod )
	{
		if (checkSourceRequirements)
		{
			if( mod->getPercentReady() < 1.0f )
			{
				//Not fully ready
				return false;
			}
		}

		// First check terrain type, if it is cared about.  Don't return a true, since there are more checks.
		switch( spTemplate->getSpecialPowerType() )
		{
			case SPECIAL_PARADROP_AMERICA:
			case SPECIAL_CRATE_DROP:
			{
				if( TheTerrainLogic->isUnderwater( loc->x, loc->y ) )
					return FALSE;
			}
		}

		// Last check is shroudedness, if it is cared about
		switch( spTemplate->getSpecialPowerType() )
		{
			case SPECIAL_DAISY_CUTTER:
			case SPECIAL_PARADROP_AMERICA:
			case SPECIAL_CARPET_BOMB:
			case SPECIAL_CLUSTER_MINES:
			case SPECIAL_EMP_PULSE:
			case SPECIAL_CRATE_DROP:
			case SPECIAL_NAPALM_STRIKE:
			case SPECIAL_BLACK_MARKET_NUKE:
			case SPECIAL_ANTHRAX_BOMB:
			case SPECIAL_TERROR_CELL:
			case SPECIAL_AMBUSH:
			case SPECIAL_NEUTRON_MISSILE:
			case SPECIAL_SCUD_STORM:
#ifdef ALLOW_DEMORALIZE
			case SPECIAL_DEMORALIZE:
#endif
			case SPECIAL_A10_THUNDERBOLT_STRIKE:
			case SPECIAL_REPAIR_VEHICLES:
			case SPECIAL_ARTILLERY_BARRAGE:
			case SPECIAL_PARTICLE_UPLINK_CANNON:
			case SPECIAL_CLEANUP_AREA:
				//Don't allow "damaging" special powers in shrouded areas, but Fogged are okay.
				return ThePartitionManager->getShroudStatusForPlayer( obj->getControllingPlayer()->getPlayerIndex(), loc ) != CELLSHROUD_SHROUDED;

			case SPECIAL_SPY_SATELLITE:
			case SPECIAL_RADAR_VAN_SCAN:
			case SPECIAL_SPY_DRONE:
			case SPECIAL_LAUNCH_BAIKONUR_ROCKET:
				//These specials can be used anywhere!
				return true;

			//These special powers require object targets!
			case SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES:
			case SPECIAL_HACKER_DISABLE_BUILDING:
			case SPECIAL_TANKHUNTER_TNT_ATTACK:
			case SPECIAL_CASH_HACK:
			case SPECIAL_DEFECTOR:
			case SPECIAL_BLACKLOTUS_CAPTURE_BUILDING:
			case SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK:
			case SPECIAL_BLACKLOTUS_STEAL_CASH_HACK:
			case SPECIAL_INFANTRY_CAPTURE_BUILDING:
			case SPECIAL_DETONATE_DIRTY_NUKE:
			case SPECIAL_DISGUISE_AS_VEHICLE:
			case SPECIAL_REMOTE_CHARGES:
			case SPECIAL_TIMED_CHARGES:
			case SPECIAL_CASH_BOUNTY:
			case SPECIAL_CHANGE_BATTLE_PLANS:
				return false;
		}
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canDoSpecialPowerAtObject( const Object *obj, const Object *target, CommandSourceType commandSource, const SpecialPowerTemplate *spTemplate, UnsignedInt commandOptions, Bool checkSourceRequirements )
{
	if (checkSourceRequirements)
	{
		//First check, if our object can do this special power.
		if( !obj->hasSpecialPower( spTemplate->getSpecialPowerType() ) )
		{
			return false;
		}
	}

	if( target->isEffectivelyDead() )
	{
		return FALSE;
	}

	Relationship r = obj->getRelationship(target);

	SpecialPowerModuleInterface *mod = obj->getSpecialPowerModule( spTemplate );
	if( mod )
	{
		if (checkSourceRequirements)
		{
			if( mod->getPercentReady() < 1.0f )
			{
				//Not fully ready
				return false;
			}
		}

		// if the target is in the shroud, we can't do anything
		if (isObjectShroudedForAction(obj, target, commandSource))
			return FALSE;

		switch( spTemplate->getSpecialPowerType() )
		{
			case SPECIAL_CASH_BOUNTY:
				return false;

			case SPECIAL_TANKHUNTER_TNT_ATTACK:
				if( target->isKindOf( KINDOF_STRUCTURE ) || (target->isKindOf( KINDOF_VEHICLE ) && !target->isKindOf(KINDOF_AIRCRAFT)) )
				{
					return true;
				}
				break;

			case SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES:
				//Can only use laser guided missiles on vehicles!
				if( target->isKindOf( KINDOF_VEHICLE ) && r == ENEMIES )
				{
					return true;
				}
				break;

			case SPECIAL_HACKER_DISABLE_BUILDING:
				//Can only disable buildings...
				if( target->isKindOf( KINDOF_STRUCTURE ) && r == ENEMIES )
				{
					//Make sure the building is considered hackable (temp: using capturable)
					if( !target->isKindOf( KINDOF_CAPTURABLE ) || target->isKindOf( KINDOF_REBUILD_HOLE ) )
					{
						return FALSE;
					}
					return true;
				}
				break;

			case SPECIAL_INFANTRY_CAPTURE_BUILDING:
			case SPECIAL_BLACKLOTUS_CAPTURE_BUILDING:
				return canCaptureBuilding( obj, target, commandSource );

			case SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK:
				return canDisableVehicleViaHacking( obj, target, commandSource, false );

			case SPECIAL_BLACKLOTUS_STEAL_CASH_HACK:
				return canStealCashViaHacking( obj, target, commandSource );

			case SPECIAL_CASH_HACK:
				//Can only disable enemy supply centers.
				if( target->isKindOf( KINDOF_STRUCTURE ) && r == ENEMIES )
				{
					//Make sure the building is considered hackable (temp: using capturable)
					if( !target->isKindOf( KINDOF_CAPTURABLE ) || target->isKindOf( KINDOF_REBUILD_HOLE ) )
					{
						return FALSE;
					}

					//Can't cash hack a building that's under construction.
					if( target->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
					{
						return FALSE;
					}

					if (target->isKindOf( KINDOF_CASH_GENERATOR ) )
					{
						return true;
					}
				}
				break;

			case SPECIAL_DISGUISE_AS_VEHICLE:
				if( target->isKindOf( KINDOF_VEHICLE ) && !target->isKindOf( KINDOF_AIRCRAFT ) && !target->isKindOf( KINDOF_BOAT ) )
				{
					//Don't allow it to disguise as another bomb truck -- that's just plain dumb.
					//if( target->getTemplate() != obj->getTemplate() )
					{
						//Don't allow it to disguise as a train -- they don't have KINDOF_TRAIN yet, but
						//if added, please change this code so it'll be faster!
						static const NameKeyType key = NAMEKEY( "RailroadBehavior" );
						RailroadBehavior *rBehavior = (RailroadBehavior*)target->findUpdateModule( key );
						if( !rBehavior )
						{
							return true;
						}
					}
				}
				break;

			case SPECIAL_DEFECTOR:
				//buildings do not defect
				if( ! target->isKindOf( KINDOF_STRUCTURE ) )
				{
					// only attacking type things will defect (no dozers, supply trucks, workers...)
// srj sez: I don't know why this is commented out, but it should remain thus, because
// it is not necessarily the case that dozers, workers, etc. cannot attack; they may
// "attack" Mines to disarm them...
//				if ( target->isKindOf( KINDOF_CAN_ATTACK ) )
					{
						//neutral or same-team units are worthless defectors
						if( r == ENEMIES )
						{
							return canMakeObjectDefector( obj, target, commandSource );
						}
					}
				}
				break;

			//These special powers require locations, not objects!
			case SPECIAL_DAISY_CUTTER:
			case SPECIAL_PARADROP_AMERICA:
			case SPECIAL_CARPET_BOMB:
			case SPECIAL_CLUSTER_MINES:
			case SPECIAL_EMP_PULSE:
			case SPECIAL_CRATE_DROP:
			case SPECIAL_NAPALM_STRIKE:
			case SPECIAL_TERROR_CELL:
			case SPECIAL_AMBUSH:
			case SPECIAL_NEUTRON_MISSILE:
			case SPECIAL_DETONATE_DIRTY_NUKE:
			case SPECIAL_BLACK_MARKET_NUKE:
			case SPECIAL_ANTHRAX_BOMB:
			case SPECIAL_SPY_SATELLITE:
			case SPECIAL_SPY_DRONE:
			case SPECIAL_RADAR_VAN_SCAN:
			case SPECIAL_SCUD_STORM:
			case SPECIAL_A10_THUNDERBOLT_STRIKE:
			case SPECIAL_ARTILLERY_BARRAGE:
			case SPECIAL_REPAIR_VEHICLES:
			case SPECIAL_PARTICLE_UPLINK_CANNON:
			case SPECIAL_CHANGE_BATTLE_PLANS:
			case SPECIAL_CLEANUP_AREA:
			case SPECIAL_LAUNCH_BAIKONUR_ROCKET:
				return false;

			case SPECIAL_REMOTE_CHARGES:
			case SPECIAL_TIMED_CHARGES:
			{
				if( target->isEffectivelyDead() ||
						target->isKindOf( KINDOF_BRIDGE ) ||
						target->isKindOf( KINDOF_BRIDGE_TOWER ) )
					return FALSE;

				if( target->isKindOf( KINDOF_STRUCTURE ) || target->isKindOf( KINDOF_VEHICLE ) )
				{
					SpecialAbilityUpdate *spUpdate = obj->findSpecialAbilityUpdate( spTemplate->getSpecialPowerType() );
					if( spUpdate )
					{
						//Make sure we have enough equipment to place an additional charge.
						if( spUpdate->getSpecialObjectCount() < spUpdate->getSpecialObjectMax() )
						{
							//Also restrict the unit from placing more than one charge on the same building.
							//We accomplish this by having the stickybomb update store the target as the producer ID.
							if( spUpdate->findSpecialObjectWithProducerID( target ) )
							{
								return false;
							}

							//HERE'S THE TOUGH CASE...
							//We also don't want to allow a unit that can place timed charges on a building to be able to place
							//remote charges (or vice-versa). So we're going to look for the other special ability update and
							//reject if the other one has it planted...
							if( spTemplate->getSpecialPowerType() == SPECIAL_REMOTE_CHARGES )
							{
								spUpdate = obj->findSpecialAbilityUpdate( SPECIAL_TIMED_CHARGES );
							}
							else if( spTemplate->getSpecialPowerType() == SPECIAL_TIMED_CHARGES )
							{
								spUpdate = obj->findSpecialAbilityUpdate( SPECIAL_REMOTE_CHARGES );
							}

							//If we have a valid pointer at this point, we found the other special. Make sure it
							//isn't planted on the same target.
							if( spUpdate && spUpdate->findSpecialObjectWithProducerID( target ) )
							{
								return false;
							}

							return true;
						}
					}
				}
				break;
			}
		}
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool ActionManager::canDoSpecialPower( const Object *obj, const SpecialPowerTemplate *spTemplate, CommandSourceType commandSource, UnsignedInt commandOptions, Bool checkSourceRequirements )
{
	if (checkSourceRequirements)
	{
		//First check, if our object can do this special power.
		if( !obj->hasSpecialPower( spTemplate->getSpecialPowerType() ) )
		{
			return false;
		}
	}

	SpecialPowerModuleInterface *mod = obj->getSpecialPowerModule( spTemplate );
	if( mod )
	{
		if (checkSourceRequirements)
		{
			if( mod->getPercentReady() < 1.0f )
			{
				//Not fully ready
				return false;
			}
		}

		switch( spTemplate->getSpecialPowerType() )
		{
			case SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES:
			case SPECIAL_TANKHUNTER_TNT_ATTACK:
			case SPECIAL_DAISY_CUTTER:
			case SPECIAL_PARADROP_AMERICA:
			case SPECIAL_NAPALM_STRIKE:
			case SPECIAL_TERROR_CELL:
			case SPECIAL_NEUTRON_MISSILE:
			case SPECIAL_BLACK_MARKET_NUKE:
			case SPECIAL_ANTHRAX_BOMB:
			case SPECIAL_SPY_SATELLITE:
			case SPECIAL_SPY_DRONE:
			case SPECIAL_RADAR_VAN_SCAN:
			case SPECIAL_TIMED_CHARGES:
			case SPECIAL_SCUD_STORM:
			case SPECIAL_A10_THUNDERBOLT_STRIKE:
			case SPECIAL_ARTILLERY_BARRAGE:
			case SPECIAL_DISGUISE_AS_VEHICLE:
			case SPECIAL_REPAIR_VEHICLES:
			case SPECIAL_PARTICLE_UPLINK_CANNON:
			case SPECIAL_CASH_BOUNTY:
 			case SPECIAL_CLEANUP_AREA:
				//These all require object or location targets.
				return false;

			case SPECIAL_REMOTE_CHARGES:
			case SPECIAL_CIA_INTELLIGENCE:
			case SPECIAL_DETONATE_DIRTY_NUKE:
			case SPECIAL_CHANGE_BATTLE_PLANS:
			case SPECIAL_LAUNCH_BAIKONUR_ROCKET:
				//Detonate's any existing charges
				return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------------------------
Bool ActionManager::canFireWeaponAtLocation( const Object *obj, const Coord3D *loc, CommandSourceType commandSource, const WeaponSlotType slot, const Object *objectInWay )
{
	//Sanity check
	if( obj == nullptr || loc == nullptr )
	{
		return false;
	}

	//Make sure we have the right weapon.
	Weapon *weapon = obj->getWeaponInWeaponSlot( slot );
	if( !weapon )
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------------------------
Bool ActionManager::canFireWeaponAtObject( const Object *obj, const Object *target, CommandSourceType commandSource, const WeaponSlotType slot )
{
	//Sanity check
	if( obj == nullptr || target == nullptr )
	{
		return false;
	}

	//Make sure we have the right weapon.
	Weapon *weapon = obj->getWeaponInWeaponSlot( slot );
	if( !weapon )
	{
		return false;
	}

	//if( weapon->getDamageType() == DAMAGE_KILLPILOT )
	//{
	//	return canSnipeVehicle( obj, target, commandSource );
	//}

	CanAttackResult result = obj->getAbleToAttackSpecificObject( ATTACK_NEW_TARGET, target, commandSource );

	if( result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING )
	{
		return weapon->estimateWeaponDamage( obj, target ) != 0.0f;
	}
	return FALSE;
}

//------------------------------------------------------------------------------------------------
Bool ActionManager::canFireWeapon( const Object *obj, const WeaponSlotType slot, CommandSourceType commandSource )
{
	//Sanity check
	if( obj == nullptr )
	{
		return false;
	}

	//Make sure we have the right weapon.
	Weapon *weapon = obj->getWeaponInWeaponSlot( slot );
	if( !weapon )
	{
		return false;
	}

	return true;

}

//------------------------------------------------------------------------------------------------
Bool ActionManager::canGarrison( const Object *obj, const Object *target, CommandSourceType commandSource )
{
	if (!(obj && target))
		return false;

	// The object was not an infantry, or is disallowed from being allowed to garrison stuff.
	if (obj->isKindOf(KINDOF_INFANTRY) == false || obj->isKindOf(KINDOF_NO_GARRISON))
		return false;

	if (target->isKindOf(KINDOF_STRUCTURE) == false)
		return false;

	ContainModuleInterface *objCMI = obj->getContain();
	if (!objCMI)
		return false;

	ContainModuleInterface *cmi = target->getContain();
	if (cmi == nullptr)
		return false;

	if (cmi->isGarrisonable() == false)
		return false;

	if (obj->getControllingPlayer() == target->getControllingPlayer())
	{
		return cmi->isValidContainerFor(obj, true);
	}

	if (obj->getControllingPlayer()->getRelationship(target->getTeam()) == NEUTRAL)
	{
		// needs to be empty if its not already ours.
		return (cmi->getContainCount() == 0 && cmi->isValidContainerFor(obj, true));
	}

	return false;
}

//------------------------------------------------------------------------------------------------
Bool ActionManager::canPlayerGarrison( const Player *player, const Object *target, CommandSourceType commandSource )
{
	if (!(player && target))
		return false;

	if (target->isEffectivelyDead()) {
		return false;
	}

	if (target->isKindOf(KINDOF_STRUCTURE) == false)
		return false;

	ContainModuleInterface *cmi = target->getContain();
	if (cmi == nullptr)
		return false;

	if (cmi->isGarrisonable() == false)
		return false;

	if (player == target->getControllingPlayer())
	{
		return true;
	}

	if (player->getRelationship(target->getTeam()) == NEUTRAL)
	{
		// needs to be empty if its not already ours.
		return cmi->getContainCount() == 0;
	}

	return false;
}

//------------------------------------------------------------------------------------------------
Bool ActionManager::canOverrideSpecialPowerDestination( const Object *obj, const Coord3D *loc, SpecialPowerType spType, CommandSourceType commandSource )
{
	SpecialPowerUpdateInterface* spuInterface = obj->findSpecialPowerWithOverridableDestinationActive( spType );
	if( spuInterface )
	{
		//But so long as it's not in the black areas of the map.
		return ThePartitionManager->getShroudStatusForPlayer( obj->getControllingPlayer()->getPlayerIndex(), loc ) != CELLSHROUD_SHROUDED;
	}
	return false;
}
