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

// SupplyTruckAIUpdate.cpp ////////////
// Author: Graham Smallwood, February 2002
// Desc:   State machine that controls when and with who a Truck docks

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/ResourceGatheringManager.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Module/SupplyCenterDockUpdate.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/Module/WorkerAIUpdate.h"

#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"


#define NO_DEBUG_SUPPLY_STATE

#ifdef DEBUG_SUPPLY_STATE
static const char* statenames[] =
{
	"ST_IDLE",						///< Not doing anything.  Should I autopilot?
	"ST_BUSY",						///< Direct player involvement (move) has taken me off autopilot
	"ST_WANTING",					///< Search for warehouse or center and dock with it
	"ST_REGROUPING",			///< Wanting failed, so hang out at base until something changes.  Still on autopilot, but resting.
	"ST_DOCKING"					///< Docking substates are running, wait for them to finish
};
#endif

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
AIStateMachine* SupplyTruckAIUpdate::makeStateMachine()
{
	return newInstance(AIStateMachine)( getObject(), "SupplyTruckAIUpdateMachine");
}

//-------------------------------------------------------------------------------------------------
SupplyTruckAIUpdate::SupplyTruckAIUpdate( Thing *thing, const ModuleData* moduleData ) : AIUpdateInterface( thing, moduleData )
{
	m_supplyTruckStateMachine = nullptr;
	m_preferredDock = INVALID_ID;
	m_numberBoxes = 0;
	m_forcePending = FALSE;
	m_forcedBusyPending = FALSE;
	m_supplyTruckStateMachine = newInstance(SupplyTruckStateMachine)( getObject() );
	m_supplyTruckStateMachine->initDefaultState();

	m_suppliesDepletedVoice = getSupplyTruckAIUpdateModuleData()->m_suppliesDepletedVoice;

}

//-------------------------------------------------------------------------------------------------
SupplyTruckAIUpdate::~SupplyTruckAIUpdate()
{
	deleteInstance(m_supplyTruckStateMachine);
}


//-------------------------------------------------------------------------------------------------
UpdateSleepTime SupplyTruckAIUpdate::update()
{

	StateReturnType stRet = m_supplyTruckStateMachine->updateStateMachine();

	UpdateSleepTime mine = IS_STATE_SLEEP(stRet) ? UPDATE_SLEEP(GET_STATE_SLEEP_FRAMES(stRet)) : UPDATE_SLEEP_NONE;

	// extend
	UpdateSleepTime ret = AIUpdateInterface::update();
	return (mine < ret) ? mine : ret;
}

//-------------------------------------------------------------------------------------------------
Bool SupplyTruckAIUpdate::isCurrentlyFerryingSupplies() const
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
Bool SupplyTruckAIUpdate::isAvailableForSupplying() const
{
	return true;
}

//-------------------------------------------------------------------------------------------------
Bool SupplyTruckAIUpdate::loseOneBox()
{
	if( m_numberBoxes == 0 )
		return FALSE;
	--m_numberBoxes;

	Drawable *draw = getObject()->getDrawable();
	if( draw )
	{
		draw->updateDrawableSupplyStatus( getSupplyTruckAIUpdateModuleData()->m_maxBoxesData, m_numberBoxes );
	}

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool SupplyTruckAIUpdate::gainOneBox( Int remainingStock )
{
	if( getSupplyTruckAIUpdateModuleData() && m_numberBoxes >= getSupplyTruckAIUpdateModuleData()->m_maxBoxesData )
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
		draw->updateDrawableSupplyStatus( getSupplyTruckAIUpdateModuleData()->m_maxBoxesData, m_numberBoxes );
	}

	return TRUE;
}

//----------------------------------------------------------------------------------------
void SupplyTruckAIUpdate::privateIdle(CommandSourceType cmdSource)
{
	// If the user gives a stop command, I have to turn off autopilot
	if( cmdSource == CMD_FROM_PLAYER )
		setForceBusyState(TRUE);

	AIUpdateInterface::privateIdle(cmdSource);
}

//----------------------------------------------------------------------------------------
void SupplyTruckAIUpdate::privateDock( Object *dock, CommandSourceType cmdSource )
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

//----------------------------------------------------------------------------------------
UnsignedInt SupplyTruckAIUpdate::getActionDelayForDock( Object *dock )
{
	// Decide whether to use my Center or Warehouse delay time
	static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
	SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*) dock->findUpdateModule( key_warehouseUpdate );
	if (warehouseModule) {
		return getSupplyTruckAIUpdateModuleData()->m_warehouseDelay;
	}
	static const NameKeyType key_centerUpdate = NAMEKEY("SupplyCenterDockUpdate");
	SupplyCenterDockUpdate *centerModule = (SupplyCenterDockUpdate*) dock->findUpdateModule( key_centerUpdate );
	if (centerModule) {
		return getSupplyTruckAIUpdateModuleData()->m_centerDelay;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Real SupplyTruckAIUpdate::getWarehouseScanDistance() const
{
	// Ai players get larger scan range.  jba.
	if (getObject()->getControllingPlayer()->getPlayerType() == PLAYER_COMPUTER) {
		return 2 * getSupplyTruckAIUpdateModuleData()->m_warehouseScanDistance;
	}
	return getSupplyTruckAIUpdateModuleData()->m_warehouseScanDistance;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SupplyTruckAIUpdate::crc( Xfer *xfer )
{
	// extend base class
	AIUpdateInterface::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SupplyTruckAIUpdate::xfer( Xfer *xfer )
{
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);

	xfer->xferSnapshot(m_supplyTruckStateMachine);
	xfer->xferObjectID(&m_preferredDock);
	xfer->xferInt(&m_numberBoxes);
	xfer->xferBool(&m_forcePending);

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SupplyTruckAIUpdate::loadPostProcess()
{
 // extend base class
	AIUpdateInterface::loadPostProcess();
}

//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------

class SupplyTruckBusyState :  public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SupplyTruckBusyState, "SupplyTruckBusyState")
protected:
	// snapshot interface STUBBED.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {};
	virtual void loadPostProcess() override {};

public:
	SupplyTruckBusyState( StateMachine *machine ) : State( machine, "SupplyTruckBusyState" ) { }
	virtual StateReturnType onEnter() override
	{
		if( getMachineOwner() && getMachineOwner()->getAI() )
		{
			// Have to check, since constructor sets a state.  Phhbbt. Constructor = set up, init = do first thing.
			SupplyTruckAIInterface *update = getMachineOwner()->getAI()->getSupplyTruckAIInterface();
			if( update )
			{
				// Turn off the Busy latch when we make it to Busy
				update->setForceBusyState( FALSE );
			}
		}

#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("entering busy state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
		return STATE_CONTINUE;
	}
	virtual StateReturnType update() override
	{
		return STATE_CONTINUE;
	}
	virtual void onExit(StateExitType status) override
	{
#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("exiting busy state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
	}

};
EMPTY_DTOR(SupplyTruckBusyState)

//-----------------------------------------------------------------------------------------------------------
class SupplyTruckIdleState :  public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SupplyTruckIdleState, "SupplyTruckIdleState")
protected:
	// snapshot interface STUBBED.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {};
	virtual void loadPostProcess() override {};

public:
	SupplyTruckIdleState( StateMachine *machine ) : State( machine, "SupplyTruckIdleState" ) { }
	virtual StateReturnType onEnter() override;
	virtual StateReturnType update() override
	{
		return STATE_CONTINUE;
	}
	virtual void onExit(StateExitType status) override
	{
#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("exiting idle state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
	}

};
EMPTY_DTOR(SupplyTruckIdleState)

StateReturnType SupplyTruckIdleState::onEnter()
{
#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("entering idle state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif

 	Object *owner = getMachineOwner();
 	if (owner != nullptr) {
 		AIUpdateInterface * ownerAI = owner->getAIUpdateInterface();
 		if (ownerAI != nullptr) {
 			// This is to get idle workers to always show up on the
 			// "idle worker button."
 			// Basically if you have a worker interface, and we are entering
 			// the idle state for the supply truck, we let the worker interface
 			// know so it can decide which idle state it wants us to actually
 			// be in from its perspective.
 			WorkerAIInterface *workerAI = ownerAI->getWorkerAIInterface();
 			if (workerAI != nullptr) {
 				workerAI->exitingSupplyTruckState();
 			}
 		}
 	}

	return STATE_CONTINUE;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
SupplyTruckStateMachine::SupplyTruckStateMachine( Object *owner ) : StateMachine( owner, "SupplyTruckStateMachine" )
{
	static const StateConditionInfo busyConditions[] =
	{
		StateConditionInfo(ownerIdle, ST_IDLE, nullptr),
		StateConditionInfo(ownerDocking, ST_DOCKING, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	static const StateConditionInfo idleConditions[] =
	{
		StateConditionInfo(isForcedIntoBusyState, ST_BUSY, nullptr),
		StateConditionInfo(isForcedIntoWantingState, ST_WANTING, nullptr),
		StateConditionInfo(ownerDocking, ST_DOCKING, nullptr),
		StateConditionInfo(ownerNotDockingOrIdle, ST_BUSY, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	static const StateConditionInfo wantingConditions[] =
	{
		StateConditionInfo(ownerDocking, ST_DOCKING, nullptr),
		StateConditionInfo(ownerNotDockingOrIdle, ST_BUSY, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	static const StateConditionInfo regroupingConditions[] =
	{
		StateConditionInfo(ownerIdle, ST_IDLE, nullptr),
		StateConditionInfo(ownerDocking, ST_DOCKING, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	static const StateConditionInfo dockingConditions[] =
	{
		StateConditionInfo(isForcedIntoBusyState, ST_BUSY, nullptr),
		StateConditionInfo(ownerAvailableForSupplying, ST_WANTING, nullptr),
		StateConditionInfo(ownerNotDockingOrIdle, ST_BUSY, nullptr),
		StateConditionInfo(nullptr, INVALID_STATE_ID, nullptr)
	};

	// order matters: first state is the default state.
	defineState( ST_BUSY,							newInstance(SupplyTruckBusyState)( this ),												ST_BUSY,		ST_BUSY,					busyConditions );
	defineState( ST_IDLE,							newInstance(SupplyTruckIdleState)( this ),												ST_BUSY,		ST_BUSY,					idleConditions );
	defineState( ST_WANTING,					newInstance(SupplyTruckWantsToPickUpOrDeliverBoxesState)( this ),	ST_BUSY,		ST_REGROUPING,		wantingConditions );
	defineState( ST_REGROUPING,				newInstance(RegroupingState)( this ),															ST_WANTING, ST_BUSY,					regroupingConditions );
	defineState( ST_DOCKING,					newInstance(DockingState)( this ),																ST_BUSY,		ST_BUSY,					dockingConditions );
}

//-------------------------------------------------------------------------------------------------
SupplyTruckStateMachine::~SupplyTruckStateMachine()
{
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SupplyTruckStateMachine::crc( Xfer *xfer )
{
	StateMachine::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer Method */
// ------------------------------------------------------------------------------------------------
void SupplyTruckStateMachine::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );

	StateMachine::xfer(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SupplyTruckStateMachine::loadPostProcess()
{
	StateMachine::loadPostProcess();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
StateReturnType SupplyTruckWantsToPickUpOrDeliverBoxesState::onEnter()
{
#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("entering wanting state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
void SupplyTruckWantsToPickUpOrDeliverBoxesState::onExit(StateExitType status)
{
#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("exiting wanting state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
}

//-------------------------------------------------------------------------------------------------
StateReturnType SupplyTruckWantsToPickUpOrDeliverBoxesState::update()
{
	Object* owner = getMachineOwner();
	AIUpdateInterface* ownerAI = owner->getAIUpdateInterface();
	if (!ownerAI)
		return STATE_FAILURE;

	Player* ownerPlayer = owner->getControllingPlayer();
	ResourceGatheringManager *manager = ownerPlayer->getResourceGatheringManager();

	SupplyTruckAIInterface* update = ownerAI->getSupplyTruckAIInterface();
	if (!update)
		return STATE_FAILURE;

	if (!update->isAvailableForSupplying())
	{
		DEBUG_CRASH(("SupplyTruckWantsToPickUpOrDeliverBoxesState: isAvailableForSupplying==false; should not get here"));
		return STATE_FAILURE;
	}

	Int numBoxes = update->getNumberBoxes();
	if (numBoxes > 0)
	{
		// want a center.
		Object *bestCenter = manager->findBestSupplyCenter( owner );
		if( bestCenter )
		{
			ownerAI->aiDock( bestCenter, CMD_FROM_AI );
			return STATE_SUCCESS;
		}
	}
	else
	{
		// want a warehouse.
		Object* bestWarehouse = manager->findBestSupplyWarehouse( owner );
		if( bestWarehouse )
		{
			ownerAI->aiDock( bestWarehouse, CMD_FROM_AI );
			return STATE_SUCCESS;
		}
	}
	return STATE_FAILURE;// we aren't going to wait right here, we will go back to base and wait for
	// wanting to succeed some place safe.
}

//-------------------------------------------------------------------------------------------------
StateReturnType RegroupingState::onEnter()
{
#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("entering regrouping state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
	// I have failed to find a dock, so my first choice is to go hang out at a Supply Center (I may have
	// failed to find a Warehouse).  My second choices is to go to a ConYard.  My last choice is just to
	// go to a friendly building.

	Object* owner = getMachineOwner();
	AIUpdateInterface* ownerAI = owner->getAIUpdateInterface();
	Player* ownerPlayer = owner->getControllingPlayer();
	if( !ownerPlayer || !ownerAI )
		return STATE_FAILURE;

	ownerAI->ignoreObstacle( nullptr );
	SupplyTruckAIInterface *update = owner->getAIUpdateInterface()->getSupplyTruckAIInterface();
	if( !update )
	{
		return STATE_FAILURE;
	}
	Int numBoxes = update->getNumberBoxes();
	// If we are forced to regroup, and we have boxes, we want to wait for the player to
	// rebuild a supply center & go to it.
	Bool wanting = numBoxes > 0;

	update->setForceWantingState( wanting );

	Object *destinationObject = nullptr;

	KindOfMaskType kindof;
	KindOfMaskType kindofnot;
	kindof.set(KINDOF_CASH_GENERATOR);
	kindofnot.clear();
	// can't do best supply center of the player's resource brain, because that adds canTransfer checks.
	destinationObject = ownerPlayer->findClosestByKindOf( owner, kindof, kindofnot );
	if( !destinationObject )
	{
		kindof.clear();
		kindof.set(KINDOF_COMMANDCENTER);
		kindofnot.clear();
		destinationObject = ownerPlayer->findClosestByKindOf( owner, kindof, kindofnot );
	}
	if( !destinationObject )
	{
		kindof.clear();
		kindof.set( KINDOF_STRUCTURE );
		kindofnot.clear();
		destinationObject = ownerPlayer->findClosestByKindOf( owner, kindof, kindofnot );
	}
	if( !destinationObject )
	{
		return STATE_FAILURE;
	}

	Coord3D destination;
	FindPositionOptions fpOptions;
	fpOptions.minRadius = 0.0f;
	fpOptions.maxRadius = 100.0f;

	if( ! ThePartitionManager->findPositionAround( destinationObject->getPosition(), &fpOptions, &destination ) )
		return STATE_FAILURE;

	ownerAI->aiMoveToPosition( &destination, CMD_FROM_AI );
	return STATE_CONTINUE;// Remember to say continue when you change ai command inside a state
}

//-------------------------------------------------------------------------------------------------
void RegroupingState::onExit(StateExitType status)
{
#ifdef DEBUG_SUPPLY_STATE
	TheInGameUI->DEBUG_addFloatingText("exiting regroup state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
}


//-------------------------------------------------------------------------------------------------
StateReturnType DockingState::onEnter()
{
#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("entering docking state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
	Object *owner = getMachineOwner();
	SupplyTruckAIInterface *update = owner->getAIUpdateInterface()->getSupplyTruckAIInterface();
	if( !update )
	{
		return STATE_FAILURE;
	}

	// after we dock the first time, we clear this, and then follow our normal state machine path
	update->setForceWantingState(false);

	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
StateReturnType DockingState::update()
{
	return STATE_CONTINUE;
}

//-------------------------------------------------------------------------------------------------
void DockingState::onExit(StateExitType status)
{
#ifdef DEBUG_SUPPLY_STATE
TheInGameUI->DEBUG_addFloatingText("exiting docking state", getMachineOwner()->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/* static */ Bool SupplyTruckStateMachine::isForcedIntoWantingState( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	AIUpdateInterface *ai = owner->getAIUpdateInterface();
	if( !ai )
		return false;
	SupplyTruckAIInterface *update = ai->getSupplyTruckAIInterface();
	if( !update )
		return false;

	if (update->isForcedIntoWantingState())
	{
#ifdef DEBUG_SUPPLY_STATE
AsciiString tmp;
tmp.format("isForcedIntoWantingState returns true (%s)",statenames[thisState->getID()]);
TheInGameUI->DEBUG_addFloatingText(tmp, owner->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/* static */ Bool SupplyTruckStateMachine::isForcedIntoBusyState( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	AIUpdateInterface *ai = owner->getAIUpdateInterface();
	if( !ai )
		return false;
	SupplyTruckAIInterface *update = ai->getSupplyTruckAIInterface();
	if( !update )
		return false;

	if (update->isForcedIntoBusyState())
	{
#ifdef DEBUG_SUPPLY_STATE
AsciiString tmp;
tmp.format("isForcedIntoBusytate returns true (%s)",statenames[thisState->getID()]);
TheInGameUI->DEBUG_addFloatingText(tmp, owner->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/* static */ Bool SupplyTruckStateMachine::ownerDocking( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	AIUpdateInterface *ai = owner->getAIUpdateInterface();
	if( !ai )
		return false;

	AIStateType masterState = ai->getAIStateType();

	if (masterState == AI_DOCK)
	{
#ifdef DEBUG_SUPPLY_STATE
AsciiString tmp;
tmp.format("ownerDocking returns true (%s)",statenames[thisState->getID()]);
TheInGameUI->DEBUG_addFloatingText(tmp, owner->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/* static */ Bool SupplyTruckStateMachine::ownerIdle( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	AIUpdateInterface *ai = owner->getAIUpdateInterface();
	if( !ai )
		return false;

	if (ai->isIdle())
	{
#ifdef DEBUG_SUPPLY_STATE
AsciiString tmp;
tmp.format("ownerIdle returns true (%s)",statenames[thisState->getID()]);
TheInGameUI->DEBUG_addFloatingText(tmp, owner->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/* static */ Bool SupplyTruckStateMachine::ownerAvailableForSupplying( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	AIUpdateInterface *ai = owner->getAIUpdateInterface();
	if( !ai )
		return false;
	SupplyTruckAIInterface *update = ai->getSupplyTruckAIInterface();
	if( !update )
		return false;

	if (update->isAvailableForSupplying() && ai->isIdle())
	{
#ifdef DEBUG_SUPPLY_STATE
AsciiString tmp;
tmp.format("ownerAvailableForSupplying returns true (%s)",statenames[thisState->getID()]);
TheInGameUI->DEBUG_addFloatingText(tmp, owner->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/* static */ Bool SupplyTruckStateMachine::ownerNotDockingOrIdle( State *thisState, void* userData )
{
	Object *owner = thisState->getMachineOwner();
	AIUpdateInterface *ai = owner->getAIUpdateInterface();
	if( !ai )
		return false;

	if (!ai->isIdle() && ai->getAIStateType() != AI_DOCK)
	{
#ifdef DEBUG_SUPPLY_STATE
AsciiString tmp;
tmp.format("ownerNotDockingOrIdle returns true (%s)",statenames[thisState->getID()]);
TheInGameUI->DEBUG_addFloatingText(tmp, owner->getPosition(), GameMakeColor(255, 0, 0, 255));
#endif
		return true;
	}

	return false;
}

