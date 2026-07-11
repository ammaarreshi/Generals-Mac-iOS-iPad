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

// FILE: OpenContain.h ////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, November 2001
// Desc:   The OpenContainer ContainModule allows objects to be contained inside of other
//				 objects.  There is a set of functionality that will be common to
//				 all container modules that provides the actual containment
//				 implementations, those implementations are found here
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/CollideModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/DamageModule.h"
#include "Common/AudioEventRTS.h"
#include "Common/KindOf.h"
#include "Common/GameMemory.h"
#include "Common/ModelState.h"

// ------------------------------------------------------------------------------------------------
enum { CONTAIN_MAX_UNKNOWN = -1 };  // means we don't care, infinite, unassigned, whatever

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class OpenContainModuleData : public UpdateModuleData
{

public:
	DieMuxData m_dieMuxData;
	Int m_containMax;								///< how many things we can have inside (-1 = "I don't care")
	AudioEventRTS m_enterSound;			///< sound to play on entering
	AudioEventRTS m_exitSound;			///< sound to play on exiting
	Bool m_passengersAllowedToFire;	///< Can the passengers shoot out of us?
	Bool m_passengersInTurret;			///< The Firepoint bones are in our turret, not our chassis
	Int m_numberOfExitPaths;				///< Will alternate through ExitStart/End paths as we exit people.
	Real m_damagePercentageToUnits;
	UnsignedInt m_doorOpenTime;
	KindOfMaskType m_allowInsideKindOf;			///< objects must have at least one of these kind of bits set to be contained by us
	KindOfMaskType m_forbidInsideKindOf;		///< objects must have NONE of these kind of bits set to be contained by us
 	Bool m_allowAlliesInside;				///< allow allies inside us
 	Bool m_allowEnemiesInside;			///< allow enemies inside us
 	Bool m_allowNeutralInside;			///< allow neutral inside us

	OpenContainModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
/** An open container can actually contain other objects */
//-------------------------------------------------------------------------------------------------
class OpenContain : public UpdateModule,
										public ContainModuleInterface,
										public CollideModuleInterface,
										public DieModuleInterface,
										public DamageModuleInterface,
										public ExitInterface
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( OpenContain, "OpenContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( OpenContain, OpenContainModuleData )

public:

	OpenContain( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual ContainModuleInterface* getContain() override { return this; }
	virtual CollideModuleInterface* getCollide() override { return this; }
	virtual DieModuleInterface* getDie() override { return this; }
	virtual DamageModuleInterface* getDamage() override { return this; }
	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | (MODULEINTERFACE_CONTAIN) | (MODULEINTERFACE_COLLIDE) | (MODULEINTERFACE_DIE) | (MODULEINTERFACE_DAMAGE); }

	virtual void onDie( const DamageInfo *damageInfo ) override;  ///< the die callback
	virtual void onDelete() override;	///< Last possible moment cleanup
	virtual void onCapture( Player *oldOwner, Player *newOwner ) override {}

	// CollideModuleInterface
	virtual void onCollide( Object *other, const Coord3D *loc, const Coord3D *normal ) override;
	virtual Bool wouldLikeToCollideWith(const Object* other) const override { return false; }
	virtual Bool isCarBombCrateCollide() const override { return false; }
	virtual Bool isHijackedVehicleCrateCollide() const override { return false; }
	virtual Bool isRailroad() const override { return false;}
	virtual Bool isSalvageCrateCollide() const override { return false; }

	// UpdateModule
	virtual UpdateSleepTime update() override;				///< called once per frame

	// ContainModuleInterface
	virtual OpenContain *asOpenContain() override { return this; }  ///< treat as open container

	// DamageModuleInterface
	virtual void onDamage( DamageInfo *damageInfo ) override {};	///< damage callback
	virtual void onHealing( DamageInfo *damageInfo ) override {};	///< healing callback
	virtual void onBodyDamageStateChange( const DamageInfo* damageInfo,
																				BodyDamageType oldState,
																				BodyDamageType newState) override {};  ///< state change callback


	// our object changed position... react as appropriate.
	virtual void containReactToTransformChange() override;

	virtual Bool calcBestGarrisonPosition( Coord3D *sourcePos, const Coord3D *targetPos ) override { return FALSE; }
	virtual Bool attemptBestFirePointPosition( Object *source, Weapon *weapon, Object *victim ) override { return FALSE; }
	virtual Bool attemptBestFirePointPosition( Object *source, Weapon *weapon, const Coord3D *targetPos ) override { return FALSE; }

	///< if my object gets selected, then my visible passengers should, too
	///< this gets called from
	virtual void clientVisibleContainedFlashAsSelected() override {};

	virtual const Player* getApparentControllingPlayer(const Player* observingPlayer) const override { return nullptr; }
	virtual void recalcApparentControllingPlayer() override { }

	virtual void onContaining( Object *obj ) override;		///< object now contains 'obj'
	virtual void onRemoving( Object *obj ) override;			///< object no longer contains 'obj'
	virtual void onSelling() override;///< Container is being sold.  Open responds by kicking people out

	virtual void orderAllPassengersToExit( CommandSourceType commandSource ) override; ///< All of the smarts of exiting are in the passenger's AIExit. removeAllFrommContain is a last ditch system call, this is the game Evacuate
	virtual void markAllPassengersDetected() override;										///< Cool game stuff got added to the system calls since this layer didn't exist, so this regains that functionality

	// default OpenContain has unlimited capacity...!
	virtual Bool isValidContainerFor(const Object* obj, Bool checkCapacity) const override;
	virtual void addToContain( Object *obj ) override;				///< add 'obj' to contain list
	virtual void addToContainList( Object *obj ) override;		///< The part of AddToContain that inheritors can override (Can't do whole thing because of all the private stuff involved)
	virtual void removeFromContain( Object *obj, Bool exposeStealthUnits = FALSE ) override;	///< remove 'obj' from contain list
	virtual void removeAllContained( Bool exposeStealthUnits = FALSE ) override;				///< remove all objects on contain list
	virtual Bool isEnclosingContainerFor( const Object *obj ) const override;	///< Does this type of Contain Visibly enclose its contents?
	virtual Bool isPassengerAllowedToFire() const override;	///< Hey, can I shoot out of this container?
	virtual void setOverrideDestination( const Coord3D * ) override {} ///< Instead of falling peacefully towards a clear spot, I will now aim here
	virtual Bool isDisplayedOnControlBar() const override {return FALSE;}///< Does this container display its contents on the ControlBar?
	virtual Int getExtraSlotsInUse() override { return 0; }
	virtual Bool isKickOutOnCapture() override { return TRUE; }///< By default, yes, all contain modules kick passengers out on capture

	// contain list access
	virtual void iterateContained( ContainIterateFunc func, void *userData, Bool reverse ) override;
	virtual UnsignedInt getContainCount() const override { return m_containListSize; }
	virtual const ContainedItemsList* getContainedItemsList() const override { return &m_containList; }
	virtual const Object *friend_getRider() const override {return nullptr;} ///< Damn.  The draw order dependency bug for riders means that our draw module needs to cheat to get around it.
	virtual Real getContainedItemsMass() const override;
	virtual UnsignedInt getStealthUnitsContained() const override { return m_stealthUnitsContained; }
	virtual UnsignedInt getHeroUnitsContained() const override { return m_heroUnitsContained; }

	virtual PlayerMaskType getPlayerWhoEntered() const override { return m_playerEnteredMask; }

	virtual Int getContainMax() const override;

	// ExitInterface
	virtual Bool isExitBusy() const override {return FALSE;}	///< Contain style exiters are getting the ability to space out exits, so ask this before reserveDoor as a kind of no-commitment check.
	virtual ExitDoorType reserveDoorForExit( const ThingTemplate* objType, Object *specificObject ) override { return DOOR_1; }
	virtual void exitObjectViaDoor( Object *newObj, ExitDoorType exitDoor ) override;
	virtual void exitObjectInAHurry( Object *newObj ) override;


	virtual void unreserveDoorForExit( ExitDoorType exitDoor ) override { /*nothing*/ }
	virtual void exitObjectByBudding( Object *newObj, Object *budHost ) override { return; };

	virtual void setRallyPoint( const Coord3D *pos ) override;				///< define a "rally point" for units to move towards
	virtual const Coord3D *getRallyPoint() const override;			///< define a "rally point" for units to move towards
	virtual Bool getExitPosition(Coord3D& exitPosition ) const override { return FALSE; };					///< access to the "Door" position of the production object
	virtual Bool getNaturalRallyPoint( Coord3D& rallyPoint, Bool offset = TRUE ) const override;			///< get the natural "rally point" for units to move towards

	virtual ExitInterface* getContainExitInterface() override { return this; }

	virtual Bool isGarrisonable() const override { return false; }		///< can this unit be Garrisoned? (ick)
	virtual Bool isHealContain() const override { return false; } ///< true when container only contains units while healing (not a transport!)
	virtual Bool isTunnelContain() const override { return FALSE; }
	virtual Bool isSpecialZeroSlotContainer() const override { return false; }
	virtual Bool isImmuneToClearBuildingAttacks() const override { return true; }

	/**
		this is used for containers that must do something to allow people to enter or exit...
		eg, land (for Chinook), open door (whatever)... it's called with wants=WANTS_TO_ENTER
		when something is in the enter state, and wants=ENTS_NOTHING when the unit has
		either entered, or given up...
	*/
	virtual void onObjectWantsToEnterOrExit(Object* obj, ObjectEnterExitType wants) override;

	// returns true iff there are objects currently waiting to enter.
	virtual Bool hasObjectsWantingToEnterOrExit() const override;

	virtual void processDamageToContained(Real percentDamage) override; ///< Do our % damage to units now.
#if RETAIL_COMPATIBLE_CRC
	void processDamageToContainedInternal(Object* const* objects, size_t size, Real percentDamage);
#endif

	virtual void enableLoadSounds( Bool enable ) override { m_loadSoundsEnabled = enable; }

  virtual Object* getClosestRider ( const Coord3D *pos ) override;
protected:

	virtual void monitorConditionChanges();				///< check to see if we need to update our occupant postions from a model change or anything else
	virtual void putObjAtNextFirePoint( Object *obj );	///< place object at position of the next fire point to use
	virtual void redeployOccupants();							///< redeploy any objects at firepoints due to a model condition change

	const ContainedItemsList& getContainList() const { return m_containList; }

	void scatterToNearbyPosition(Object* obj);
	void removeFromContainViaIterator( ContainedItemsList::iterator it, Bool exposeStealthUnits = FALSE );  ///< remove item from contain list
	void removeFromPassengerViaIterator( ContainedItemsList::iterator it );///< remove item from passenger list

	virtual void doLoadSound();
	virtual void doUnloadSound();
	virtual void positionContainedObjectsRelativeToContainer(){}

	virtual void addOrRemoveObjFromWorld(Object* obj, Bool add);

	// exists primarily for TransportContain to override
	virtual void killRidersWhoAreNotFreeToExit() { }

	void pruneDeadWanters();

	ContainedItemsList	m_containList;						///< the list of contained objects
	UnsignedInt					m_containListSize;							///< size of contained list
private:

	typedef std::map< ObjectID, ObjectEnterExitType, std::less<ObjectID>/**/> ObjectEnterExitMap;

	ObjectEnterExitMap	m_objectEnterExitInfo;
	UnsignedInt					m_stealthUnitsContained;				///< number of stealth units that can't be seen by enemy players.
	UnsignedInt					m_heroUnitsContained;						///< cached hero count
	Int									m_whichExitPath; ///< Cycles from 1 to n and is used only in modules whose data has numberOfExitPaths > 1.
	UnsignedInt					m_doorCloseCountdown;						///< When should I shut my door.

	std::list<ObjectID>	m_xferContainIDList;		///< for loading m_containList from a save game
	PlayerMaskType			m_playerEnteredMask;					///< Mask of player that entered last, if any.
	UnsignedInt					m_lastUnloadSoundFrame;					///< last frame we did an un-loading sound
	UnsignedInt					m_lastLoadSoundFrame;						///< last frame we did a loading sound

/// @todo srj -- move this to a lazily-allocated subobject
	enum { MAX_FIRE_POINTS = 32 };
	ModelConditionFlags	m_conditionState;				///< The Drawables current behavior state
	Matrix3D						m_firePoints[ MAX_FIRE_POINTS ];
	Int									m_firePointStart;												///< start firepoint index to use when building becomes occupied
	Int									m_firePointNext;												///< next index to place objects at
	Int									m_firePointSize;												///< how many entries in m_firePoint are valid
	Bool								m_noFirePointsInArt;										///< TRUE when no fire point bones exist in the art

	Coord3D							m_rallyPoint;												///< Where units should move to after they have reached the "natural" rally point
	Bool								m_rallyPointExists;										///< Only move to the rally point if this is true
	Bool								m_loadSoundsEnabled;								///< Don't serialize -- used for disabling sounds during payload creation.

};
