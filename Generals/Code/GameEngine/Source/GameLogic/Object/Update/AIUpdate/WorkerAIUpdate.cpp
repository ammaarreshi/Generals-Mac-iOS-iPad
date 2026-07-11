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

// FILE: WorkerAIUpdate.cpp ///////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, June 2002
// Desc:   A Worker is a unit that is both a Dozer and a Supply Truck.
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ActionManager.h"
#include "Common/Team.h"
#include "Common/StateMachine.h"
#include "Common/BuildAssistant.h"
#include "Common/GameState.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Player.h"
#include "Common/Money.h"
#include "Common/Radar.h"
#include "Common/RandomValue.h"
#include "Common/GlobalData.h"
#include "Common/ResourceGatheringManager.h"
#include "Common/Upgrade.h"

#include "GameClient/Drawable.h"
#include "GameClient/GameText.h"
#include "GameClient/InGameUI.h"

#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/BridgeBehavior.h"
#include "GameLogic/Module/BridgeTowerBehavior.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Module/SupplyCenterDockUpdate.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/Module/WorkerAIUpdate.h"




// FORWARD DECLARATIONS ///////////////////////////////////////////////////////////////////////////
enum
{
	AS_DOZER,				///< When not actively Gathering, or when actively building, I am a dozer
	AS_SUPPLY_TRUCK	///< When told explicitly by player or other object, I become a supply truck
};


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WorkerAIUpdate::WorkerAIUpdate( Thing *thing, const ModuleData* moduleData ) :
							 AIUpdateInterface( thing, moduleData )

{

	//
	// initialize the dozer machine to nullptr, we want to do this and create it during the update
	// implementation because at this point we don't have the object all setup
	m_isRebuild = FALSE;
	m_dozerMachine = nullptr;
	for( Int i = 0; i < DOZER_NUM_TASKS; i++ )
	{
		m_task[ i ].m_targetObjectID = INVALID_ID;
		m_task[ i ].m_taskOrderFrame = 0;
		for( Int j = 0; j < DOZER_NUM_DOCK_POINTS; j++ )
		{
			m_dockPoint[ i ][ j ].valid = FALSE;
			m_dockPoint[ i ][ j ].location.zero();
		}
	}
	m_currentTask = DOZER_TASK_INVALID;
	m_previousTask = DOZER_TASK_INVALID;
	m_buildSubTask = DOZER_SELECT_BUILD_DOCK_LOCATION;  // irrelavant, but I want non-garbage value

	m_supplyTruckStateMachine = nullptr;
	m_numberBoxes = 0;
	m_forcePending = FALSE;
	m_forcedBusyPending = FALSE;

	m_workerMachine = nullptr;

 	m_suppliesDepletedVoice = getWorkerAIUpdateModuleData()->m_suppliesDepletedVoice;

	createMachines();

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
WorkerAIUpdate::~WorkerAIUpdate()
{

	// delete our behavior state machine
	deleteInstance(m_dozerMachine);
	deleteInstance(m_supplyTruckStateMachine);
	deleteInstance(m_workerMachine);

}

//-------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isCurrentlyFerryingSupplies() const
{
	if (m_supplyTruckStateMachine)
	{
		switch (m_supplyTruckStateMachine->getCurrentStateID())
		{
			case ST_IDLE:
			case ST_BUSY:
			case ST_REGROUPING:
				return false;
			case ST_WANTING:
			case ST_DOCKING:
				return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isAvailableForSupplying() const
{
	return true;
}

// ------------------------------------------------------------------------------------------------
Real WorkerAIUpdate::getRepairHealthPerSecond() const
{
	return getWorkerAIUpdateModuleData()->m_repairHealthPercentPerSecond;
}
// ------------------------------------------------------------------------------------------------
Real WorkerAIUpdate::getBoredTime() const
{
	return getWorkerAIUpdateModuleData()->m_boredTime;
}
// ------------------------------------------------------------------------------------------------
Real WorkerAIUpdate::getBoredRange() const
{
	return getWorkerAIUpdateModuleData()->m_boredRange;
}

// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::createMachines()
{

	if( m_workerMachine == nullptr )
	{
		m_workerMachine = newInstance(WorkerStateMachine)( getObject() );

		if( m_dozerMachine == nullptr )
		{
			m_dozerMachine = newInstance(DozerPrimaryStateMachine)( getObject() );
			m_dozerMachine->initDefaultState();
		}

		if( m_supplyTruckStateMachine == nullptr )
		{
			m_supplyTruckStateMachine = newInstance(SupplyTruckStateMachine)( getObject() );
			m_supplyTruckStateMachine->initDefaultState();
		}

		m_workerMachine->initDefaultState();// this has to wait until all three are in place since
		// an immediate transition check will ask questions of the machines.

//#ifdef RTS_DEBUG
//		m_workerMachine->setDebugOutput(TRUE);
//		m_dozerMachine->setDebugOutput(TRUE);
//		m_supplyTruckStateMachine->setDebugOutput(TRUE);
//#endif
	}

}

//-------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
UnsignedInt WorkerAIUpdate::getActionDelayForDock( Object *dock )
{
	// Decide whether to use my Center or Warehouse delay time
	static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
	SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*) dock->findUpdateModule( key_warehouseUpdate );
	if (warehouseModule) {
		return getWorkerAIUpdateModuleData()->m_warehouseDelay;
	}
	static const NameKeyType key_centerUpdate = NAMEKEY("SupplyCenterDockUpdate");
	SupplyCenterDockUpdate *centerModule = (SupplyCenterDockUpdate*) dock->findUpdateModule( key_centerUpdate );
	if (centerModule) {
		return getWorkerAIUpdateModuleData()->m_centerDelay;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Real WorkerAIUpdate::getWarehouseScanDistance() const
{
	// Ai players get larger scan range.  jba.
	if (getObject()->getControllingPlayer()->getPlayerType() == PLAYER_COMPUTER) {
		return 2 * getWorkerAIUpdateModuleData()->m_warehouseScanDistance;
	}
	return getWorkerAIUpdateModuleData()->m_warehouseScanDistance;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime WorkerAIUpdate::update()
{

	//
	// NOTE: Any changes to DozerAIUpdate::* you probably want to reflect and copy into
	// WorkerAIUPdate:* as well ... sigh
	//

	//
	// now that we're really executing we have all the necessary object modules in place to
	// correctly create a state machine and set the default state
	//
	// create all the machines if they don't yet exist
	createMachines();

	// DO NOT set us as being to able to move with super precision off grid locations
	// Causes workers to get stuck.  jba.
	//if( getCurLocomotor() )
		//getCurLocomotor()->setUltraAccurate( TRUE );

	// extend the normal AI system
	AIUpdateInterface::update();

	// do nothing if we're dead
	///@todo shouldn't this be at a higher level?
	if( getObject()->isEffectivelyDead() )
		return UPDATE_SLEEP_NONE;

	// run our own state machine, and the appropriate sub machine
	m_workerMachine->updateStateMachine();

	if( m_workerMachine->getCurrentStateID() == AS_DOZER )
	{

		// get and validate our current task
		DozerTask currentTask = getCurrentTask();
		if( currentTask != DOZER_TASK_INVALID )
		{
			ObjectID taskTarget = getTaskTarget( currentTask );
			Object *targetObject = TheGameLogic->findObjectByID( taskTarget );
			Bool invalidTask = FALSE;

			// validate the task and the target
			// TheSuperHackers @bugfix Stubbjax 16/11/2025 Invalidate the task when the build scaffold is destroyed.
			if( currentTask == DOZER_TASK_REPAIR &&
					TheActionManager->canRepairObject( getObject(), targetObject, getLastCommandSource() ) == FALSE )
				invalidTask = TRUE;
#if !RETAIL_COMPATIBLE_CRC
			else if (currentTask == DOZER_TASK_BUILD && targetObject == nullptr)
				invalidTask = TRUE;
#endif

			// cancel the task if it's now invalid
			if( invalidTask == TRUE )
				cancelTask( currentTask );

		}

		// update dozer behavior
		m_dozerMachine->updateStateMachine();

	}
	else
	{
		m_supplyTruckStateMachine->updateStateMachine();
		// If we are harvesting, we can be diverted to clear mines.  jba.
		getObject()->setWeaponSetFlag(WEAPONSET_MINE_CLEARING_DETAIL);//maybe go clear some mines, if I feel like it
	}
	return UPDATE_SLEEP_NONE;
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/** The entry point of a construct command to the Dozer */
//-------------------------------------------------------------------------------------------------
Object *WorkerAIUpdate::construct( const ThingTemplate *what,
																	 const Coord3D *pos,
																	 Real angle,
																	 Player *owningPlayer,
																	 Bool isRebuild )
{

	// !!! NOTE: If you modify this you must modify the dozer too !!!
	// !!! Graham: Please please please have inspiration for how to *not* duplicate this code
	// GS - Construct needs to be an AI primitive.  Inheriting off of AIUpdate means you are writing a
	// master brain that will call AI primitives on the object, not something that does stuff itself.
  // SupplyTruckAI decides who to call AIDock on.  Worker should decide to AIDock or AIConstruct
	// or AIRepair.  Dozer should just use the latter two.  No construction logic should be in
	// the inherited AIUpdates at all.

	// create our machines if they don't yet exist
	///@todo make 'construct' a real AI command and you won't need a special case

	m_isRebuild = isRebuild;

	createMachines();

	// sanity
	if( what == nullptr || pos == nullptr || owningPlayer == nullptr )
		return nullptr;

	// sanity
	DEBUG_ASSERTCRASH( getObject()->getControllingPlayer() == owningPlayer,
										 ("Dozer::Construct - The controlling player of the Dozer is not the owning player passed in") );

	// if we're not rebuilding, we have a few checks to pass first for sanity
	if( isRebuild == FALSE )
	{

		// AI has weaker restriction on building
		Bool dozerIsAI = owningPlayer->getPlayerType() == PLAYER_COMPUTER;
		if( dozerIsAI )
		{

			// validate the the position to build at is valid
			if( TheBuildAssistant->isLocationLegalToBuild( pos, what, angle,
																										 BuildAssistant::CLEAR_PATH |
																										 BuildAssistant::NO_OBJECT_OVERLAP,
																										 getObject(), nullptr ) != LBC_OK )
				return nullptr;

		}
		else
		{

			// make sure the player is capable of building this
			if( TheBuildAssistant->canMakeUnit( getObject(), what ) != CANMAKE_OK )
				return nullptr;

			// validate the the position to build at is valid
			if( TheBuildAssistant->isLocationLegalToBuild( pos, what, angle,
																										 BuildAssistant::TERRAIN_RESTRICTIONS |
																										 BuildAssistant::CLEAR_PATH |
																										 BuildAssistant::NO_OBJECT_OVERLAP |
																										 BuildAssistant::SHROUD_REVEALED,
																										 getObject(), nullptr ) != LBC_OK )
				return nullptr;

		}

	}

	// what will our initial status bits
	ObjectStatusMaskType statusBits = MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION );
	if( isRebuild )
		statusBits.set( OBJECT_STATUS_RECONSTRUCTING );

	// create an object at the destination location
	Object *obj = TheThingFactory->newObject( what, owningPlayer->getDefaultTeam(), statusBits );

	// even though we haven't actually built anything yet, this keeps things tidy
	obj->setProducer( getObject() );
	obj->setBuilder( getObject() );

	// leave the supply truck state and now behave like a dozer.
	exitingSupplyTruckState();

	// take the required money away from the player
	if( isRebuild == FALSE )
	{
		Money *money = owningPlayer->getMoney();

		money->withdraw( what->calcCostToBuild( owningPlayer ) );

	}

	//
	// set a bit that this object is under construction, it is important to do this early
	// before the hooks add/subtract power from a player are executed
	//
	obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );

	// initialize object
	obj->setPosition( pos );
	obj->setOrientation( angle );

	// Flatten the terrain underneath the object, then adjust to the flattened height. jba.
	TheTerrainLogic->flattenTerrain(obj);
	Coord3D adjustedPos = *pos;
	adjustedPos.z = TheTerrainLogic->getGroundHeight(pos->x, pos->y);
	obj->setPosition(&adjustedPos);

	// Note - very important that we add to map AFTER we flatten terrain. jba.
	TheAI->pathfinder()->addObjectToPathfindMap( obj );

	// "callback" event for structure created (note that it's not yet "complete")
	owningPlayer->onStructureCreated( getObject(), obj );

	// set a construction percent for the new object to zero and a status for under construction
	obj->setConstructionPercent( 0.0 );

	// newly constructed objects start at one hit point
	BodyModuleInterface *body = obj->getBodyModule();
	body->internalChangeHealth( -body->getHealth() + 1.0f );

	// set the model action state to awaiting construction
	obj->clearAndSetModelConditionFlags(
		MAKE_MODELCONDITION_MASK2(MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED),
		MAKE_MODELCONDITION_MASK(MODELCONDITION_AWAITING_CONSTRUCTION)
	);

	// we have a construction pending
	newTask( DOZER_TASK_BUILD, obj );

	return obj;

}

// ------------------------------------------------------------------------------------------------
/** We just exited from a supply truck task and are now idle, we should go back to Dozer idle
		mode */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::exitingSupplyTruckState()
{
	if( m_workerMachine->getCurrentStateID() == AS_SUPPLY_TRUCK )
	{
 		// We've been given a Dozer specific order that the Supply Truck machine doesn't recognize
 		// as BUSY (because this command also recognizes its own busy and is likewise waiting).
 		// Explicitly slap it upside the head.
 		if( getObject()->getAIUpdateInterface() )
 		{
 			getObject()->getAIUpdateInterface()->aiIdle(CMD_FROM_AI);
 		}
 		m_workerMachine->setState( AS_DOZER );
 		// To clarify, I leave supply truck mode when I notice I am doing something not supply
 		// truck related.  When given a construct command, I wait to do anything until I notice
 		// I'm not busy.  Both states are being polite, so I must force the switch.
	}
}


// ------------------------------------------------------------------------------------------------
/** Given our current task and repair target, can we accept this as a new repair target */
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::canAcceptNewRepair( Object *obj )
{

	// sanity
	if( obj == nullptr )
		return FALSE;

	// if we're not repairing right now, we don't have any accept restrictions
	if( getCurrentTask() != DOZER_TASK_REPAIR )
		return TRUE;

	// get current repair target
	Object *currentRepair = TheGameLogic->findObjectByID( m_task[ DOZER_TASK_REPAIR ].m_targetObjectID );

	if( currentRepair )
	{

		// check for same object
		if( currentRepair == obj )
			return FALSE;

		// check for repairing any tower on the same bridge
		if( currentRepair->isKindOf( KINDOF_BRIDGE_TOWER ) &&
				obj->isKindOf( KINDOF_BRIDGE_TOWER ) )
		{
			BridgeTowerBehaviorInterface *currentTowerInterface = nullptr;
			BridgeTowerBehaviorInterface *newTowerInterface = nullptr;

			currentTowerInterface = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( currentRepair );
			newTowerInterface = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( obj );

			// sanity
			if( currentTowerInterface == nullptr || newTowerInterface == nullptr )
			{

				DEBUG_CRASH(( "Unable to find bridge tower interface on object" ));
				return FALSE;

			}

			// if they are part of the same bridge, ignore this repair command
			if( currentTowerInterface->getBridgeID() == newTowerInterface->getBridgeID() )
				return FALSE;

		}

	}

	// all is well
	return TRUE;

}

//----------------------------------------------------------------------------------------
void WorkerAIUpdate::privateIdle(CommandSourceType cmdSource)
{
// Leaving this commented out to show that although the regular supply truck does this, the
	// worker's dozer brain will get completely screwed.

	// If the user gives a stop command, I have to turn off autopilot
//	if( cmdSource == CMD_FROM_PLAYER )
//		setForceBusyState(TRUE);

	AIUpdateInterface::privateIdle(cmdSource);
}

//----------------------------------------------------------------------------------------
void WorkerAIUpdate::privateDock( Object *dock, CommandSourceType cmdSource )
{
	AIUpdateInterface::privateDock( dock, cmdSource );

	// If this is a command from a player, I will remember this as my favorite dock to override
	// ResourceManager searches.
	if ((cmdSource == CMD_FROM_PLAYER) && dock)
	{
		// Please note, there is not a separate Warehouse and Center memory by Design.  Because
		// we lack a UI way to click Warehouse and drag to center to set up a specific path, the
		// practical realization has been made that you do not want separate memory.
		m_preferredDock = dock->getID();
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::privateRepair( Object *obj, CommandSourceType cmdSource )
{
	Object *dozer = getObject();

	// sanity, if we can't repair the object then get out of there
	if( TheActionManager->canRepairObject( dozer, obj, cmdSource ) == FALSE )
		return;

	// if we are already repairing this target do nothing
	if( canAcceptNewRepair( obj ) == FALSE )
		return;

	//
	// if this object has already been targeted for repair by an object we won't also try to
	// go repair it
	ObjectID currentRepairer = obj->getSoleHealingBenefactor();
	if( currentRepairer != INVALID_ID && currentRepairer != dozer->getID() )
		return;

	// start the new task
	newTask( DOZER_TASK_REPAIR, obj );

}

// ------------------------------------------------------------------------------------------------
/** Resume construction on a building */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::privateResumeConstruction( Object *obj, CommandSourceType cmdSource )
{

	// sanity
	if( obj == nullptr )
		return;

	// make sure we can resume construction on this
	if( TheActionManager->canResumeConstructionOf( getObject(), obj, cmdSource ) == FALSE )
		return;

	// start the new task for construction
	newTask( DOZER_TASK_BUILD, obj );

}

//-------------------------------------------------------------------------------------------------
/** Issue and order to the dozer */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::newTask( DozerTask task, Object* target )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	// sanity
	if( target == nullptr )
		return;

	m_preferredDock = INVALID_ID; // If we are dozing, we don't want any supply truck stuff going on. jba.

	//
	// special check for the build task, we should never be given more than one of them ...
	// for the other tasks we just forget what we were doing and the new target takes
	// precedence for the task
	//
	if( task == DOZER_TASK_BUILD || task == DOZER_TASK_REPAIR )
	{

		// handle getting two tasks
		if( isTaskPending( task ) == TRUE )
			cancelTask( task );

		// get our object
		Object *me = getObject();

		Coord3D position;
		target = DozerAIUpdate::findGoodBuildOrRepairPositionAndTarget(me, target, position);
		if (target == nullptr)
			return;	// could happen for some bridges

		//
		// for building, we say that even "thinking" about building or rebuilding an object
		// sets us as the current builder of that object.  this allows any dozers that are
		// ordered later to resume construction on something to see that somebody is already taking
		// care of it and then they won't be even try to resume a build since we don't allow
		// multiple dozers/workers to double up on construction efforts
		//
		if( task == DOZER_TASK_BUILD )
			target->setBuilder( me );

		m_dockPoint[ task ][ DOZER_DOCK_POINT_START ].valid			= TRUE;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_START ].location	= position;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_ACTION ].valid		= TRUE;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_ACTION ].location = position;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_END ].valid				= TRUE;
		m_dockPoint[ task ][ DOZER_DOCK_POINT_END ].location		= position;

	}

	// set the new task target and the frame in which we got this order
	m_task[ task ].m_targetObjectID = target->getID();
	m_task[ task ].m_taskOrderFrame = TheGameLogic->getFrame();

	// reset the dozer behavior so that it can re-evaluate which task to continue working on
	m_dozerMachine->resetToDefaultState();

	// reset the workermachine, if we've been acting like a supply truck
	if( m_workerMachine->getCurrentStateID() == AS_SUPPLY_TRUCK )
	{
		// We've been given a Dozer specific order that the Supply Truck machine doesn't recognize
		// as BUSY (because this command also recognizes its own busy and is likewise waiting).
		// Explicitly slap it upside the head.
		if( getObject()->getAIUpdateInterface() )
		{
			getObject()->getAIUpdateInterface()->aiIdle(CMD_FROM_AI);
		}
		m_workerMachine->setState( AS_DOZER );
		// To clarify, I leave supply truck mode when I notice I am doing something not supply
		// truck related.  When given a construct command, I wait to do anything until I notice
		// I'm not busy.  Both states are being polite, so I must force the switch.
	}

}

//-------------------------------------------------------------------------------------------------
/** Cancel a task and reset the dozer behavior state machine so that it can
	* re-evaluate what it wants to do if it was working on the task being
	* cancelled */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::cancelTask( DozerTask task )
{

	// clear the order
	internalCancelTask( task );

	// reset the machine to we can re-evaluate what we want to do
	m_dozerMachine->resetToDefaultState();

}

void WorkerAIUpdate::cancelAllTasks()
{
	for (UnsignedInt task = DOZER_TASK_FIRST; task < DOZER_NUM_TASKS; ++task)
		internalCancelTask((DozerTask)task);

	m_dozerMachine->resetToDefaultState();
}

//-------------------------------------------------------------------------------------------------
/** Attempt to resume the previous task */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::resumePreviousTask()
{
	if (m_previousTask != DOZER_TASK_INVALID)
	{
		newTask(m_previousTask, TheGameLogic->findObjectByID(m_previousTaskInfo.m_targetObjectID));
		m_previousTask = DOZER_TASK_INVALID;
		m_previousTaskInfo = DozerTaskInfo();
	}
}

//-------------------------------------------------------------------------------------------------
/** Is there a given task waiting to be done */
//-------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isTaskPending( DozerTask task )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	return m_task[ task ].m_targetObjectID != 0 ? TRUE : FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Is there any task pending */
//-------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isAnyTaskPending()
{

	for( Int i = 0; i < DOZER_NUM_TASKS; i++ )
		if( isTaskPending( (DozerTask)i ) )
			return TRUE;

	return FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Get the target object of a given task */
//-------------------------------------------------------------------------------------------------
ObjectID WorkerAIUpdate::getTaskTarget( DozerTask task )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	return m_task[ task ].m_targetObjectID;

}

//-------------------------------------------------------------------------------------------------
/** Set a task as successfully completed */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::internalTaskComplete( DozerTask task )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	// call the single method that gets called for completing and canceling tasks
	internalTaskCompleteOrCancelled( task );

	// remove the info for this task
	m_task[ task ].m_targetObjectID = INVALID_ID;
	m_task[ task ].m_taskOrderFrame = 0;

	m_previousTask = DOZER_TASK_INVALID;
	m_previousTaskInfo = DozerTaskInfo();

	// remove dock point info for this task
	for( Int i = 0; i < DOZER_NUM_DOCK_POINTS; i++ )
		m_dockPoint[ task ][ i ].valid = FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Clear a task from the Dozer for consideration, we can use this when a goal object becomes
	* invalid/destroyed etc. */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::internalCancelTask( DozerTask task )
{

	// sanity
	DEBUG_ASSERTCRASH( task >= 0 && task < DOZER_NUM_TASKS, ("Illegal dozer task '%d'", task) );

	if(task < 0 || task >= DOZER_NUM_TASKS)
		return;  //DAMNIT!  You CANNOT assert and then not handle the damn error!  The.  Code.  Must.  Not.  Crash.

	// call the single method that gets called for completing and canceling tasks
	internalTaskCompleteOrCancelled( task );

	m_previousTask = task;
	m_previousTaskInfo = m_task[task];

	// remove the info for this task
	m_task[ task ].m_targetObjectID = INVALID_ID;
	m_task[ task ].m_taskOrderFrame = 0;

	// remove dock point info for this task
	for( Int i = 0; i < DOZER_NUM_DOCK_POINTS; i++ )
		m_dockPoint[ task ][ i ].valid = FALSE;

	// stop the dozer from moving
	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if( !ai )
	{
		return;
	}
	/// @todo we really need a stop command instead of making it move to it's current location
	ai->aiMoveToPosition( getObject()->getPosition(), CMD_FROM_AI );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::internalTaskCompleteOrCancelled( DozerTask task )
{

	switch( task )
	{

		// --------------------------------------------------------------------------------------------
		case DOZER_TASK_INVALID:
		{

			break;  // do nothing, this is really no task

		}

		// --------------------------------------------------------------------------------------------
		case DOZER_TASK_BUILD:
		{

			// the builder is no longer actively building something
			getObject()->clearModelConditionState( MODELCONDITION_ACTIVELY_CONSTRUCTING );

			// And the thing we were working on is no longer being actively built

			///@todo This would be correct except that we don't have idle crane animations and it is December.
//			Object* goalObject = TheGameLogic->findObjectByID(m_task[task].m_targetObjectID);
//			if (goalObject != nullptr)
//			{
//				goalObject->clearModelConditionState(MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED);
//			}
			break;

		}

		// --------------------------------------------------------------------------------------------
		case DOZER_TASK_REPAIR:
		{
			Object *obj = nullptr;

			// the builder is no longer actively repairing something
			getObject()->clearModelConditionState( MODELCONDITION_ACTIVELY_CONSTRUCTING );

			// get object to reapir (if present)
			obj = TheGameLogic->findObjectByID( m_task[ task ].m_targetObjectID );

			if( obj )
			{
 				// when we're done repairing bridges, tell the scaffolding to go away
 				if( obj->isKindOf( KINDOF_BRIDGE_TOWER ) )
 					removeBridgeScaffolding( obj );

			}

			break;

		}

		// --------------------------------------------------------------------------------------------
		case DOZER_TASK_FORTIFY:
		{

			break;

		}

		// --------------------------------------------------------------------------------------------
		default:
		{

			DEBUG_CRASH(( "internalTaskCompleteOrCancelled: Unknown Dozer task '%d'", task ));
			break;

		}

	}

}

//-------------------------------------------------------------------------------------------------
/** If we were building something, kill the active-construction flag on it */
//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::onDelete()
{
	Int i;

	// cancel any of the tasks we had queued up
	for( i = DOZER_TASK_FIRST; i < DOZER_NUM_TASKS; ++i )
	{

		if( isTaskPending( (DozerTask)i ) )
			cancelTask( (DozerTask)i );

	}

	for( i = 0; i < DOZER_NUM_TASKS; i++ )
	{
		Object* goalObject = TheGameLogic->findObjectByID(m_task[i].m_targetObjectID);
		if (goalObject != nullptr)
		{
			goalObject->clearModelConditionState(MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED);
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Get the most recently issued task */
//-------------------------------------------------------------------------------------------------
DozerTask WorkerAIUpdate::getMostRecentCommand()
{
	Int i;
	DozerTask mostRecentTask = DOZER_TASK_INVALID;
	UnsignedInt mostRecentFrame = 0;

	for( i = 0; i < DOZER_NUM_TASKS; i++ )
	{
		if( isTaskPending( (DozerTask)i ) )
		{
			if( m_task[ i ].m_taskOrderFrame > mostRecentFrame )
			{
				mostRecentTask = (DozerTask)i;
				mostRecentFrame = m_task[ i ].m_taskOrderFrame;
			}
		}
	}
	return mostRecentTask;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
const Coord3D* WorkerAIUpdate::getDockPoint( DozerTask task, DozerDockPoint point )
{

	// sanity
	if( task < 0 || task >= DOZER_NUM_TASKS )
		return nullptr;

	// sanity
	if( point < 0 || point >= DOZER_NUM_DOCK_POINTS )
		return nullptr;

	// if the point has been set (is valid) then return it
	if( m_dockPoint[ task ][ point ].valid )
		return &m_dockPoint[ task ][ point ].location;

	// no valid point has been set for this dock point on this task
	return nullptr;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
void WorkerAIUpdate::aiDoCommand(const AICommandParms* parms)
{

	//
	// anytime we get a command, just remove any model condition that has us actively building
	// if we need to show that, that bit will be set anyway again during the build process
	//
	getObject()->clearModelConditionState( MODELCONDITION_ACTIVELY_CONSTRUCTING );

	if (!isAllowedToRespondToAiCommands(parms))
		return;

	// create our machines if they don't yet exist
	createMachines();

	switch( parms->m_cmd )
	{

		// --------------------------------------------------------------------------------------------
		case AICMD_REPAIR:
		{

			// if we have no task right now, go idle so we can immediately respond to this
			if( getCurrentTask() == DOZER_TASK_INVALID )
				aiIdle( CMD_FROM_AI );

			// do the repair
			privateRepair(parms->m_obj, parms->m_cmdSource);
			break;

		}

		// --------------------------------------------------------------------------------------------
		case AICMD_RESUME_CONSTRUCTION:
		{

			// if we have no task right now, go idle so we can immediately respond to this
			if( getCurrentTask() == DOZER_TASK_INVALID )
				aiIdle( CMD_FROM_AI );

			// do the command
			privateResumeConstruction( parms->m_obj, parms->m_cmdSource );
			break;

		}

		// --------------------------------------------------------------------------------------------
		default:
		{

			// if this is from the player, cancel our current task
			if( parms->m_cmdSource == CMD_FROM_PLAYER && getCurrentTask() != DOZER_TASK_INVALID )
				cancelTask( getCurrentTask() );

			// issue the command
			AIUpdateInterface::aiDoCommand(parms);

			// when a player issues commands, this will cause the dozer to re-evaluate what it's doing
			if( parms->m_cmdSource == CMD_FROM_PLAYER )
				m_dozerMachine->resetToDefaultState();
			break;

		}

	}

	if (isClearingMines() && m_numberBoxes > 0 )
	{
		// if clearing mines, we drop any boxes we were carrying
		m_numberBoxes = 0;
		Drawable *draw = getObject()->getDrawable();
		if( draw )
		{
			draw->updateDrawableSupplyStatus( getWorkerAIUpdateModuleData()->m_maxBoxesData, m_numberBoxes );
		}
	}

}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// SupplyTruck stuff

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::loseOneBox()
{
	if( m_numberBoxes == 0 )
		return FALSE;

	m_numberBoxes--;

	Drawable *draw = getObject()->getDrawable();
	if( draw )
	{
		draw->updateDrawableSupplyStatus( getWorkerAIUpdateModuleData()->m_maxBoxesData, m_numberBoxes );
	}

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::gainOneBox( Int remainingStock )
{
	if( getWorkerAIUpdateModuleData() && m_numberBoxes >= getWorkerAIUpdateModuleData()->m_maxBoxesData )
		return FALSE;

	++m_numberBoxes;

	//if I just took the last box,
	//i will announce that this supply source is now empty
	if (remainingStock == 0)
	{
		Object* bestWarehouse = getObject()->getControllingPlayer()->getResourceGatheringManager()->findBestSupplyWarehouse( getObject() );

		Bool playDepleted = FALSE;
		if ( bestWarehouse )
		{
			//figure out whether the best one is considerably far from the previous one (current position)
			Coord3D delta = *getObject()->getPosition();
			delta.sub( *bestWarehouse->getPosition() );
			if ( delta.length() > getWarehouseScanDistance()/4)
			playDepleted = TRUE;
		}
		else
			playDepleted = TRUE;

		if (playDepleted && m_suppliesDepletedVoice.getEventName().isEmpty() == false)
		{
			m_suppliesDepletedVoice.setObjectID(getObject()->getID());
			m_suppliesDepletedVoice.setPlayingHandle(TheAudio->addAudioEvent(&m_suppliesDepletedVoice));
		}
	}


	Drawable *draw = getObject()->getDrawable();
	if( draw )
	{
		draw->updateDrawableSupplyStatus( getWorkerAIUpdateModuleData()->m_maxBoxesData, m_numberBoxes );
	}

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerAIUpdate::isSupplyTruckBrainActiveAndBusy()
{
	return (m_workerMachine->getCurrentStateID() == AS_SUPPLY_TRUCK)
				&& (m_supplyTruckStateMachine->getCurrentStateID() == ST_BUSY);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::resetSupplyTruckBrain()
{
	m_supplyTruckStateMachine->resetToDefaultState();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::resetDozerBrain()
{
	m_dozerMachine->resetToDefaultState();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// Worker's master state machine, that controls Dozerness or Supplytruckness
class ActAsDozerState :  public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ActAsDozerState, "ActAsDozerState")
protected:
	// snapshot interface STUBBED.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {};
	virtual void loadPostProcess() override {};

public:
	ActAsDozerState( StateMachine *machine ) :State( machine, "ActAsDozerState" ){}
	virtual StateReturnType onEnter() override;
	virtual StateReturnType update() override;
	virtual StateReturnType onExit();
};
EMPTY_DTOR(ActAsDozerState)

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
class ActAsSupplyTruckState :  public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ActAsSupplyTruckState, "ActAsSupplyTruckState")
protected:
	// snapshot interface STUBBED.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {};
	virtual void loadPostProcess() override {};

public:
	ActAsSupplyTruckState( StateMachine *machine ) :State( machine, "ActAsSupplyTruckState" ){}
	virtual StateReturnType onEnter() override;
	virtual StateReturnType update() override;
	virtual StateReturnType onExit();
};
EMPTY_DTOR(ActAsSupplyTruckState)

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WorkerStateMachine::WorkerStateMachine( Object *owner ) : StateMachine( owner, "WorkerStateMachine" )
{
	static const StateConditionInfo asDozerConditions[] =
	{
		StateConditionInfo(supplyTruckSubMachineWantsToEnter, AS_SUPPLY_TRUCK, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	static const StateConditionInfo asTruckConditions[] =
	{
		StateConditionInfo(supplyTruckSubMachineReadyToLeave, AS_DOZER, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	// order matters: first state is the default state.
	defineState( AS_DOZER,					newInstance(ActAsDozerState)( this ), INVALID_STATE_ID, INVALID_STATE_ID, asDozerConditions );
	defineState( AS_SUPPLY_TRUCK,		newInstance(ActAsSupplyTruckState)( this ), INVALID_STATE_ID, INVALID_STATE_ID, asTruckConditions );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WorkerStateMachine::~WorkerStateMachine()
{
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WorkerStateMachine::crc( Xfer *xfer )
{
	StateMachine::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void WorkerStateMachine::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );

	StateMachine::xfer(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WorkerStateMachine::loadPostProcess()
{
	StateMachine::loadPostProcess();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerStateMachine::supplyTruckSubMachineWantsToEnter( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	WorkerAIUpdate *update = (WorkerAIUpdate*)owner->getAIUpdateInterface();
	if( !update )
	{
		return false;
	}
	AIStateType masterState = update->getAIStateType();

	//If I detect a Supply force message, or if I have been put straight in dock,
	//then the worker master part of me wants to switch to the Supply sub-brain

	return update->isForcedIntoWantingState() || (masterState == AI_DOCK);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool WorkerStateMachine::supplyTruckSubMachineReadyToLeave( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	WorkerAIUpdate *update = (WorkerAIUpdate*)owner->getAIUpdateInterface();
	if( !update )
	{
		return false;
	}
//	AIStateType masterState = update->getAIStateType();

	// It isn't ready to leave if it is on its way in.  The first clause
	// allow a latch for a moment
	// so there is no transition out on the way in.  Active and Busy means it isn't doing
	// anything Supply related.

	return !supplyTruckSubMachineWantsToEnter( thisState, nullptr )
				&& update->isSupplyTruckBrainActiveAndBusy();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsDozerState::onEnter()
{
	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsDozerState::update()
{
	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsDozerState::onExit()
{
	Object *owner = getMachineOwner();
	WorkerAIUpdate *update = (WorkerAIUpdate*)owner->getAIUpdateInterface();
	if( !update )
	{
		return STATE_FAILURE;
	}

	update->resetDozerBrain();

	return STATE_CONTINUE;
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsSupplyTruckState::onEnter()
{
	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsSupplyTruckState::update()
{
	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
StateReturnType ActAsSupplyTruckState::onExit()
{
	Object *owner = getMachineOwner();
	WorkerAIUpdate *update = (WorkerAIUpdate*)owner->getAIUpdateInterface();
	if( !update )
	{
		return STATE_FAILURE;
	}

	update->resetSupplyTruckBrain();

	return STATE_CONTINUE;
}

// ------------------------------------------------------------------------------------------------
/** Create the bridge scaffolding if necessary for the bridge that is attached to this tower */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::createBridgeScaffolding( Object *bridgeTower )
{

	// sanity
	if( bridgeTower == nullptr )
		return;

	// get the bridge behavior interface from the bridge object that this tower is a part of
	BridgeTowerBehaviorInterface *btbi = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( bridgeTower );
	if( btbi == nullptr )
		return;
	Object *bridgeObject = TheGameLogic->findObjectByID( btbi->getBridgeID() );
	if( bridgeObject == nullptr )
		return;
	BridgeBehaviorInterface *bbi = BridgeBehavior::getBridgeBehaviorInterfaceFromObject( bridgeObject );
	if( bbi == nullptr )
		return;

	// tell the bridge to create scaffolding if necessary
	bbi->createScaffolding();

}

// ------------------------------------------------------------------------------------------------
/** Remove the bridge scaffolding from the bridge object that is attached to this tower */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::removeBridgeScaffolding( Object *bridgeTower )
{

	// sanity
	if( bridgeTower == nullptr )
		return;

	// get the bridge behavior interface from the bridge object that this tower is a part of
	BridgeTowerBehaviorInterface *btbi = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( bridgeTower );
	if( btbi == nullptr )
		return;
	Object *bridgeObject = TheGameLogic->findObjectByID( btbi->getBridgeID() );
	if( bridgeObject == nullptr )
		return;
	BridgeBehaviorInterface *bbi = BridgeBehavior::getBridgeBehaviorInterfaceFromObject( bridgeObject );
	if( bbi == nullptr )
		return;

	// tell the bridge to end any scaffolding from repairing
	bbi->removeScaffolding();

}

//------------------------------------------------------------------------------------------------
void WorkerAIUpdate::startBuildingSound( const AudioEventRTS *sound, ObjectID constructionSiteID )
{
	m_buildingSound = *sound;
	m_buildingSound.setObjectID( constructionSiteID );
	m_buildingSound.setPlayingHandle( TheAudio->addAudioEvent( &m_buildingSound ) );
}

//------------------------------------------------------------------------------------------------
void WorkerAIUpdate::finishBuildingSound()
{
	TheAudio->removeAudioEvent( m_buildingSound.getPlayingHandle() );
}




// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::crc( Xfer *xfer )
{
	// extend base class
	AIUpdateInterface::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: TheSuperHackers @tweak Stubbjax 17/11/2025 Save the worker's previous task
	*/
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::xfer( Xfer *xfer )
{
#if RETAIL_COMPATIBLE_XFER_SAVE
	XferVersion currentVersion = 1;
#else
	XferVersion currentVersion = 2;
#endif
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);


	//------------------------- xfer Dozer info

	Int numTasks = DOZER_NUM_TASKS;
	xfer->xferInt(&numTasks);
	if (numTasks != DOZER_NUM_TASKS) {
		DEBUG_CRASH(("DOZER_NUM_TASKS changed unexpectedly."));
		throw SC_INVALID_DATA;
	}
	Int i, j;
	for (i=0; i<DOZER_NUM_TASKS; i++) {
		xfer->xferObjectID(&m_task[i].m_targetObjectID);
		xfer->xferUnsignedInt(&m_task[i].m_taskOrderFrame);
	}
	xfer->xferSnapshot(m_dozerMachine);
	xfer->xferUser(&m_currentTask, sizeof(m_currentTask));

	if (currentVersion >= 2)
	{
		xfer->xferUser(&m_previousTask, sizeof(m_previousTask));
		xfer->xferUser(&m_previousTaskInfo, sizeof(m_previousTaskInfo));
	}

	Int dockPoints = DOZER_NUM_DOCK_POINTS;
	xfer->xferInt(&dockPoints);
	if (dockPoints!=DOZER_NUM_DOCK_POINTS) {
		DEBUG_CRASH(("DOZER_NUM_DOCK_POINTS changed unexpectedly."));
		throw SC_INVALID_DATA;
	}
	for (i=0; i<DOZER_NUM_TASKS; i++) {
		for (j=0; j<DOZER_NUM_DOCK_POINTS; j++) {
			xfer->xferBool(&m_dockPoint[i][j].valid);
			xfer->xferCoord3D(&m_dockPoint[i][j].location);
		}
	}
	xfer->xferUser(&m_buildSubTask, sizeof(m_buildSubTask));


	//------------------------- xfer Supply Truck info
	xfer->xferSnapshot(m_supplyTruckStateMachine);
	xfer->xferObjectID(&m_preferredDock);
	xfer->xferInt(&m_numberBoxes);
	xfer->xferBool(&m_forcePending);

	//-------------------------- xfer Worker info
	xfer->xferSnapshot(m_workerMachine);

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WorkerAIUpdate::loadPostProcess()
{
 // extend base class
	AIUpdateInterface::loadPostProcess();
}
