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

// AIUpdate.h //
// AI interface
// Author: Michael S. Booth, 2001-2002

#pragma once

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LocomotorSet.h"

class AIGroup;
class AIStateMachine;
class AttackPriorityInfo;
class DamageInfo;
class DozerAIInterface;
class Object;
class Path;
class PathfindServicesInterface;
class PathNode;
class PhysicsBehavior;
#ifdef ALLOW_SURRENDER
class POWTruckAIUpdateInterface;
#endif
class SupplyTruckAIInterface;
class TurretAI;
class TurretAIData;
class Waypoint;
class WorkerAIInterface;
class HackInternetAIInterface;
class AssaultTransportAIInterface;

enum AIStateType CPP_11(: Int);
enum HordeActionType CPP_11(: Int);
enum ObjectID CPP_11(: Int);


//-------------------------------------------------------------------------------------------------
const Real FAST_AS_POSSIBLE = 999999.0f;

//-------------------------------------------------------------------------------------------------
//
// Note: these values are saved in save files, so you MUST NOT REMOVE OR CHANGE
// existing values!
//
enum LocomotorSetType CPP_11(: Int)
{
	LOCOMOTORSET_INVALID = -1,

	LOCOMOTORSET_NORMAL = 0,
	LOCOMOTORSET_NORMAL_UPGRADED,
	LOCOMOTORSET_FREEFALL,
	LOCOMOTORSET_WANDER,
	LOCOMOTORSET_PANIC,
	LOCOMOTORSET_TAXIING,			// set used for normally-airborne items while taxiing on ground
	LOCOMOTORSET_SUPERSONIC,	// set used for high-speed attacks
	LOCOMOTORSET_SLUGGISH,		// set used for abnormally slow (but not damaged) speeds

	LOCOMOTORSET_COUNT
};

//-------------------------------------------------------------------------------------------------
enum GuardTargetType CPP_11(: Int)
{
	GUARDTARGET_LOCATION,	// Guard a coord3d
	GUARDTARGET_OBJECT,		// Guard an object
	GUARDTARGET_AREA,			// Guard a Polygon trigger
	GUARDTARGET_NONE				// Currently not guarding
};

#ifdef DEFINE_LOCOMOTORSET_NAMES
static const char *const TheLocomotorSetNames[] =
{
	"SET_NORMAL",
	"SET_NORMAL_UPGRADED",
	"SET_FREEFALL",
	"SET_WANDER",
	"SET_PANIC",
	"SET_TAXIING",
	"SET_SUPERSONIC",
	"SET_SLUGGISH",

	nullptr
};
static_assert(ARRAY_SIZE(TheLocomotorSetNames) == LOCOMOTORSET_COUNT + 1, "Incorrect array size");
#endif

enum AutoAcquireStates CPP_11(: Int)
{
	AAS_Idle											= 0x01,
	AAS_Idle_Stealthed						= 0x02,
	AAS_Idle_No										= 0x04,
	AAS_Idle_Not_While_Attacking	= 0x08,
	AAS_Idle_Attack_Buildings			= 0x10,
};

#ifdef DEFINE_AUTOACQUIRE_NAMES
static const char *const TheAutoAcquireEnemiesNames[] =
{
	"YES",
	"STEALTHED",
	"NO",
	"NOTWHILEATTACKING",
	"ATTACK_BUILDINGS",

	nullptr
};
#endif


//-------------------------------------------------------------------------------------------------
enum MoodMatrixParameters CPP_11(: Int)
{
	// Controller_Player and Controller_AI are mutually exclusive
	MM_Controller_Player		= 0x00000001,
	MM_Controller_AI				= 0x00000002,
	MM_Controller_Bitmask		= (MM_Controller_Player | MM_Controller_AI),

	// UnitTypes are mutually exclusive, ie: you cannot be turreted and air at the same time (we do
	// not make such a distinction, so air takes precedence over turretedness)
	MM_UnitType_NonTurreted = 0x00000010,
	MM_UnitType_Turreted		= 0x00000020,
	MM_UnitType_Air					= 0x00000040,
	MM_UnitType_Bitmask			= (MM_UnitType_NonTurreted | MM_UnitType_Turreted | MM_UnitType_Air),

	// A unit can be in only one mood at a time.
	MM_Mood_Sleep						= 0x00000100,
	MM_Mood_Passive					= 0x00000200,
	MM_Mood_Normal					= 0x00000400,
	MM_Mood_Alert						= 0x00000800,
	MM_Mood_Aggressive			= 0x00001000,
	MM_Mood_Bitmask					= (MM_Mood_Sleep | MM_Mood_Passive | MM_Mood_Normal | MM_Mood_Alert | MM_Mood_Aggressive)
};

//-------------------------------------------------------------------------------------------------
enum MoodMatrixAction CPP_11(: Int)
{
	MM_Action_Idle,
	MM_Action_Move,
	MM_Action_Attack,
	MM_Action_AttackMove
};

//-------------------------------------------------------------------------------------------------
enum MoodActionAdjustment CPP_11(: Int)
{
	MAA_Action_Ok										= 0x00000001,
	MAA_Action_To_Idle							= 0x00000002,
	MAA_Action_To_AttackMove				= 0x00000004,
	MAA_Action_To_Bitmask						= (MAA_Action_Ok | MAA_Action_To_Idle | MAA_Action_To_AttackMove),

	MAA_Affect_Range_IgnoreAll			= 0x00000010,
	MAA_Affect_Range_WaitForAttack	= 0x00000020,
	// Normal doesn't affect ranges.
	MAA_Affect_Range_Alert					= 0x00000040,
	MAA_Affect_Range_Aggressive			= 0x00000080,
	MAA_Affect_Range_Bitmask				= (MAA_Affect_Range_IgnoreAll | MAA_Affect_Range_WaitForAttack | MAA_Affect_Range_Alert | MAA_Affect_Range_Aggressive)
};

//-------------------------------------------------------------------------------------------------
typedef std::vector< const LocomotorTemplate* > LocomotorTemplateVector;
typedef std::map< LocomotorSetType, LocomotorTemplateVector, std::less<LocomotorSetType>/**/> LocomotorTemplateMap;

//-------------------------------------------------------------------------------------------------
class AIUpdateModuleData : public UpdateModuleData
{
public:
	LocomotorTemplateMap	m_locomotorTemplates;					///< locomotors for object
	const TurretAIData*		m_turretData[MAX_TURRETS];
	UnsignedInt						m_moodAttackCheckRate;				///< how frequently we should recheck for enemies due to moods, when idle
	UnsignedInt						m_autoAcquireEnemiesWhenIdle;
#ifdef ALLOW_SURRENDER
 	UnsignedInt						m_surrenderDuration;					///< when we surrender, how long we stay surrendered.
#endif

	AIUpdateModuleData();
	virtual ~AIUpdateModuleData() override;

	virtual Bool isAiModuleData() const override { return true; }

	const LocomotorTemplateVector* findLocomotorTemplateVector(LocomotorSetType t) const;
	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseLocomotorSet( INI* ini, void *instance, void *store, const void* /*userData*/ );

private:
	static void parseTurret( INI* ini, void *instance, void *store, const void* /*userData*/ );

};

//-------------------------------------------------------------------------------------------------
enum AIFreeToExitType CPP_11(: Int) // Note - written out in save/load xfer, don't change these numbers.  jba.
{
	FREE_TO_EXIT=0,
	NOT_FREE_TO_EXIT=1,
	WAIT_TO_EXIT=2
};

//-------------------------------------------------------------------------------------------------
/**
 * The AIUpdateInterface module contains the interface to the AI system,
 * and performs the actual AI behaviors.
 */
class AIUpdateInterface : public UpdateModule, public AICommandInterface
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AIUpdateInterface, "AIUpdateInterface" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AIUpdateInterface, AIUpdateModuleData )

protected:

	// yes, protected, NOT public.
	virtual void privateMoveToPosition( const Coord3D *pos, CommandSourceType cmdSource );			///< move to given position(s) tightening the formation.
	virtual void privateMoveToObject( Object *obj, CommandSourceType cmdSource );			///< move to given object
	virtual void privateMoveToAndEvacuate( const Coord3D *pos, CommandSourceType cmdSource );			///< move to given position(s)
	virtual void privateMoveToAndEvacuateAndExit( const Coord3D *pos, CommandSourceType cmdSource );			///< move to given position & unload transport.
	virtual void privateIdle(CommandSourceType cmdSource);						///< Enter idle state.
	virtual void privateTightenToPosition( const Coord3D *pos, CommandSourceType cmdSource );			///< move to given position(s) tightening the formation.
	virtual void privateFollowWaypointPath( const Waypoint *way, CommandSourceType cmdSource );///< start following the path from the given point
	virtual void privateFollowWaypointPathAsTeam( const Waypoint *way, CommandSourceType cmdSource );///< start following the path from the given point
	virtual void privateFollowWaypointPathExact( const Waypoint *way, CommandSourceType cmdSource );///< start following the path from the given point
	virtual void privateFollowWaypointPathAsTeamExact( const Waypoint *way, CommandSourceType cmdSource );///< start following the path from the given point
	virtual void privateFollowPath( std::vector<Coord3D>* path, Object *ignoreObject, CommandSourceType cmdSource, Bool exitProduction );///< follow the path defined by the given array of points
	virtual void privateFollowPathAppend( const Coord3D *pos, CommandSourceType cmdSource );
	virtual void privateAttackObject( Object *victim, Int maxShotsToFire, CommandSourceType cmdSource );					///< attack given object
	virtual void privateForceAttackObject( Object *victim, Int maxShotsToFire, CommandSourceType cmdSource );					///< attack given object
	virtual void privateAttackTeam( const Team *team, Int maxShotsToFire, CommandSourceType cmdSource );							///< attack the given team
	virtual void privateAttackPosition( const Coord3D *pos, Int maxShotsToFire, CommandSourceType cmdSource );						///< attack given spot
	virtual void privateAttackMoveToPosition( const Coord3D *pos, Int maxShotsToFire, CommandSourceType cmdSource );			///< attack move to the given location
	virtual void privateAttackFollowWaypointPath( const Waypoint *way, Int maxShotsToFire, Bool asTeam, CommandSourceType cmdSource );			///< attack move along the following waypoint path, potentially as a team
	virtual void privateHunt( CommandSourceType cmdSource );														///< begin "seek and destroy"
	virtual void privateRepair( Object *obj, CommandSourceType cmdSource );						///< repair the given object
#ifdef ALLOW_SURRENDER
	virtual void privatePickUpPrisoner( Object *prisoner, CommandSourceType cmdSource );			///< pick up prisoner
	virtual void privateReturnPrisoners( Object *prison, CommandSourceType cmdSource );			///< return picked up prisoners to the 'prison'
#endif
	virtual void privateResumeConstruction( Object *obj, CommandSourceType cmdSource );	///< resume construction of object
	virtual void privateGetHealed( Object *healDepot, CommandSourceType cmdSource );		///< get healed at heal depot
	virtual void privateGetRepaired( Object *repairDepot, CommandSourceType cmdSource );///< get repaired at repair depot
	virtual void privateEnter( Object *obj, CommandSourceType cmdSource );							///< enter the given object
	virtual void privateDock( Object *obj, CommandSourceType cmdSource );							///< get near given object and wait for enter clearance
	virtual void privateExit( Object *objectToExit, CommandSourceType cmdSource );			///< get out of this Object
	virtual void privateEvacuate( Int exposeStealthUnits, CommandSourceType cmdSource );												///< empty its contents
	virtual void privateExecuteRailedTransport( CommandSourceType cmdSource );					///< execute next leg in railed transport sequence
	virtual void privateGoProne( const DamageInfo *damageInfo, CommandSourceType cmdSource );												///< life altering state change, if this AI can do it
	virtual void privateGuardTunnelNetwork( GuardMode guardMode, CommandSourceType cmdSource );			///< guard the given spot
	virtual void privateGuardPosition( const Coord3D *pos, GuardMode guardMode, CommandSourceType cmdSource );			///< guard the given spot
	virtual void privateGuardObject( Object *objectToGuard, GuardMode guardMode, CommandSourceType cmdSource );		///< guard the given object
	virtual void privateGuardArea( const PolygonTrigger *areaToGuard, GuardMode guardMode, CommandSourceType cmdSource );	///< guard the given area
	virtual void privateAttackArea( const PolygonTrigger *areaToGuard, CommandSourceType cmdSource );	///< guard the given area
	virtual void privateHackInternet( CommandSourceType cmdSource );	///< Hack money from the heavens (free money)
	virtual void privateFaceObject( Object *target, CommandSourceType cmdSource );
	virtual void privateFacePosition( const Coord3D *pos, CommandSourceType cmdSource );
	virtual void privateRappelInto( Object *target, const Coord3D& pos, CommandSourceType cmdSource );
	virtual void privateCombatDrop( Object *target, const Coord3D& pos, CommandSourceType cmdSource );
	virtual void privateCommandButton( const CommandButton *commandButton, CommandSourceType cmdSource );
	virtual void privateCommandButtonPosition( const CommandButton *commandButton, const Coord3D *pos, CommandSourceType cmdSource );
	virtual void privateCommandButtonObject( const CommandButton *commandButton, Object *obj, CommandSourceType cmdSource );
	virtual void privateWander( const Waypoint *way, CommandSourceType cmdSource );	///< Wander around the waypoint path.
	virtual void privateWanderInPlace( CommandSourceType cmdSource );	///< Wander around the current position.
	virtual void privatePanic( const Waypoint *way, CommandSourceType cmdSource );	///< Run screaming down the waypoint path.
	virtual void privateBusy( CommandSourceType cmdSource );	///< Transition to the busy state
	virtual void privateMoveAwayFromUnit( Object *unit, CommandSourceType cmdSource );	///< Move out of the way of a unit.


public:
	AIUpdateInterface( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual AIUpdateInterface* getAIUpdateInterface() override { return this; }

	// Disabled conditions to process (AI will still process held status)
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return MAKE_DISABLED_MASK( DISABLED_HELD ); }

	// Some very specific, complex behaviors are used by more than one AIUpdate.  Here are their interfaces.
	virtual DozerAIInterface* getDozerAIInterface() {return nullptr;}
	virtual SupplyTruckAIInterface* getSupplyTruckAIInterface() {return nullptr;}
	virtual const DozerAIInterface* getDozerAIInterface() const {return nullptr;}
	virtual const SupplyTruckAIInterface* getSupplyTruckAIInterface() const {return nullptr;}
#ifdef ALLOW_SURRENDER
	virtual POWTruckAIUpdateInterface *getPOWTruckAIUpdateInterface() { return nullptr; }
#endif
	virtual WorkerAIInterface* getWorkerAIInterface() { return nullptr; }
	virtual const WorkerAIInterface* getWorkerAIInterface() const { return nullptr; }
	virtual HackInternetAIInterface* getHackInternetAIInterface() { return nullptr; }
	virtual const HackInternetAIInterface* getHackInternetAIInterface() const { return nullptr; }
	virtual AssaultTransportAIInterface* getAssaultTransportAIInterface() { return nullptr; }
	virtual const AssaultTransportAIInterface* getAssaultTransportAIInterface() const { return nullptr; }

#ifdef ALLOW_SURRENDER
	void setSurrendered( const Object *objWeSurrenderedTo, Bool surrendered );
	inline Bool isSurrendered() const { return m_surrenderedFramesLeft > 0; }
	inline Int getSurrenderedPlayerIndex() const { return m_surrenderedPlayerIndex; }
#endif

	virtual void joinTeam();			///< This unit just got added to a team & needs to catch up.


	// this is present solely for some transports to override, so that they can land before
	// allowing people to exit...
	virtual AIFreeToExitType getAiFreeToExit(const Object* exiter) const { return FREE_TO_EXIT; }

	// this is present solely to allow some special-case things to override, like landed choppers.
	virtual Bool isAllowedToAdjustDestination() const { return true; }
	virtual Bool isAllowedToMoveAwayFromUnit() const { return true; }
	// this is used for Chinooks, so the pathfinder doesn't make 'em avoid 'em... for combat drops!
	virtual ObjectID getBuildingToNotPathAround() const { return INVALID_ID; }

	// AI Interface implementation -----------------------------------------------------------------------
	virtual Bool isIdle() const;
	virtual Bool isAttacking() const;
	virtual Bool isClearingMines() const;
	//Definition of busy -- when explicitly in the busy state. Moving or attacking is not considered busy!
	virtual Bool isBusy() const;

	virtual void onObjectCreated() override;
	virtual void doQuickExit( std::vector<Coord3D>* path );			///< get out of this Object

	virtual void aiDoCommand(const AICommandParms* parms) override;

	virtual const Coord3D *getGuardLocation() const { return &m_locationToGuard;	}
	virtual ObjectID getGuardObject() const { return m_objectToGuard; }
	virtual const PolygonTrigger *getAreaToGuard() const { return m_areaToGuard; }
	virtual GuardTargetType getGuardTargetType() const { return m_guardTargetType[1]; }
	virtual void clearGuardTargetType() { m_guardTargetType[1] = m_guardTargetType[0]; m_guardTargetType[0] = GUARDTARGET_NONE; }
	virtual GuardMode getGuardMode() const { return m_guardMode; }

	virtual Object* construct( const ThingTemplate *what,
														 const Coord3D *pos, Real angle,
														 Player *owningPlayer,
														 Bool isRebuild ) { return nullptr; }///< construct a building


	void ignoreObstacle( const Object *obj );			///< tell the pathfinder to ignore the given object as an obstacle
	void ignoreObstacleID( ObjectID id );			///< tell the pathfinder to ignore the given object as an obstacle

	AIStateType getAIStateType() const;							///< What general state is the AIState Machine in?

	AsciiString getCurrentStateName() const { return m_stateMachine->getCurrentStateName(); }

	virtual Object* getEnterTarget();					///< if we are trying to enter something (see enter() above), what is the thing in question? (or null)

	/**
		This exists solely to be overridden by children... the idea is that when a unit is targeted by someone else, it
		asks for an offset to add to its targeting position... if this returns true, the offset should be added to the
		true target pos. This is primarily for the Aurora Bomber, which uses this technique to make itself "unhittable"
		when in Supersonic Attack Mode -- things still target it, they just aim "behind" it!
	*/
	virtual Bool getSneakyTargetingOffset(Coord3D* offset) const { return false; }

	/**
		Exists solely to be overridden by JetAIUpdate...
	*/
	virtual void addTargeter(ObjectID id, Bool add) { return; }
	virtual Bool isTemporarilyPreventingAimSuccess() const { return false; }


	void setPriorWaypointID( UnsignedInt id )   { m_priorWaypointID = id; };
	void setCurrentWaypointID( UnsignedInt id ) { m_currentWaypointID = id; };

	// Group ----------------------------------------------------------------------------------------------
	// these three methods allow a group leader's path to be communicated to the other group members

	AIGroup *getGroup();

	// it's VERY RARE you want to call this function; you should normally use Object::isEffectivelyDead()
	// instead. the exception would be for things that need to know whether to call markIsDead or not.
	Bool isAiInDeadState() const { return m_isAiDead; }				///< return true if we are dead
	void markAsDead();

	Bool isRecruitable() const {return m_isRecruitable;}
	void setIsRecruitable(Bool isRecruitable) {m_isRecruitable = isRecruitable;}

	Real getDesiredSpeed() const { return m_desiredSpeed; }
	void setDesiredSpeed( Real speed ) { m_desiredSpeed = speed; }	///< how fast we want to go

	// these are virtual because subclasses might need to override them. (srj)
	virtual void setLocomotorGoalPositionOnPath();
	virtual void setLocomotorGoalPositionExplicit(const Coord3D& newPos);
	virtual void setLocomotorGoalOrientation(Real angle);
	virtual void setLocomotorGoalNone();
	virtual Bool isDoingGroundMovement() const;  ///< True if moving along ground.

	Bool isValidLocomotorPosition(const Coord3D* pos) const;
	Bool isAircraftThatAdjustsDestination() const;  ///< True if is aircraft that doesn't stack destinations (missles for example do stack destinations.)
	Real getCurLocomotorSpeed() const;
	Real getLocomotorDistanceToGoal();
	const Locomotor *getCurLocomotor() const {return m_curLocomotor;}
	Locomotor *getCurLocomotor() { return m_curLocomotor; }
	LocomotorSetType getCurLocomotorSetType() const { return m_curLocomotorSet; }
	Bool hasLocomotorForSurface(LocomotorSurfaceType surfaceType);

	// turret stuff.
	WhichTurretType getWhichTurretForWeaponSlot(WeaponSlotType wslot, Real* turretAngle, Real* turretPitch = nullptr) const;
	WhichTurretType getWhichTurretForCurWeapon() const;
	/**
		return true iff the weapon is on a turret, that turret is trying to aim at the victim,
		BUT is not yet pointing in the right dir.
	*/
	Bool isWeaponSlotOnTurretAndAimingAtTarget(WeaponSlotType wslot, const Object* victim) const;
	Bool getTurretRotAndPitch(WhichTurretType tur, Real* turretAngle, Real* turretPitch) const;
	Real getTurretTurnRate(WhichTurretType tur) const;
	void setTurretTargetObject(WhichTurretType tur, Object* o, Bool isForceAttacking = FALSE);
	Object *getTurretTargetObject( WhichTurretType tur );
	void setTurretTargetPosition(WhichTurretType tur, const Coord3D* pos);
	void setTurretEnabled(WhichTurretType tur, Bool enabled);
	void recenterTurret(WhichTurretType tur);
	Bool isTurretEnabled( WhichTurretType tur ) const;
	Bool isTurretInNaturalPosition(WhichTurretType tur) const;

	// "Planning Mode" -----------------------------------------------------------------------------------
	Bool queueWaypoint( const Coord3D *pos );				///< add waypoint to end of move list. return true if success, false if queue was full and those the waypoint not added
	void clearWaypointQueue();								///< reset the waypoint queue to empty
	void executeWaypointQueue();							///< start moving along queued waypoints

	// Pathfinding ---------------------------------------------------------------------------------------
private:
	Bool computePath( PathfindServicesInterface *pathfinder, Coord3D *destination );	///< computes path to destination, returns false if no path
	Bool computeAttackPath(PathfindServicesInterface *pathfinder,  const Object *victim, const Coord3D* victimPos );	///< computes path to attack the current target, returns false if no path
#ifdef ALLOW_SURRENDER
	void doSurrenderUpdateStuff();
#endif

public:
	void doPathfind( PathfindServicesInterface *pathfinder );
	void requestPath( Coord3D *destination, Bool isGoalDestination );	///< Queues a request to pathfind to destination.
	void requestAttackPath( ObjectID victimID, const Coord3D* victimPos );	///< computes path to attack the current target, returns false if no path
	void requestApproachPath( Coord3D *destination );	///< computes path to attack the current target, returns false if no path
	void requestSafePath( ObjectID repulsor1 );	///< computes path to attack the current target, returns false if no path

	Bool isWaitingForPath() const {return m_waitingForPath;}
	Bool isAttackPath() const {return m_isAttackPath;} ///< True if we have a path to an attack location.
	void cancelPath(); ///< Called if we no longer need the path.
	Path* getPath() { return m_path; }				///< return the agent's current path
	const Path* getPath() const { return m_path; }				///< return the agent's current path
	void destroyPath();												///< destroy the current path, setting it to null
	UnsignedInt getPathAge() const { return TheGameLogic->getFrame() - m_pathTimestamp; }	///< return the "age" of the path
	Bool isPathAvailable( const Coord3D *destination ) const; ///< does a path exist between us and the destination
	Bool isQuickPathAvailable( const Coord3D *destination ) const;  ///< does a path (using quick pathfind) exist between us and the destination
	Int getNumFramesBlocked() const {return m_blockedFrames;}
	Bool isBlockedAndStuck() const {return m_isBlockedAndStuck;}
	Bool canComputeQuickPath(); ///< Returns true if we can quickly comput a path.  Usually missiles & the like that just move straight to the destination.
	Bool computeQuickPath(const Coord3D *destination); ///< Computes a quick path to the destination.

	Bool isMoving() const;
	Bool isMovingAwayFrom(Object *obj) const;

	// the following routines should only be called by the AIInternalMoveToState.
	// They are used to determine when we are really through moving.  Due to the nature of the beast,
	// we often exit one move state & immediately enter another.  This should not cause a "stop start".
	// jba.
	void friend_startingMove();
	void friend_endingMove();

	void friend_setPath(Path *newPath);
	Path* friend_getPath() { return m_path; }

	void friend_setGoalObject(Object *obj);

	virtual Bool processCollision(PhysicsBehavior *physics, Object *other); ///< Returns true if the physics collide should apply the force.  Normally not.  jba.
	ObjectID getIgnoredObstacleID() const;

	// "Waypoint Mode" -----------------------------------------------------------------------------------
	const Waypoint *getCompletedWaypoint() const {return m_completedWaypoint;}
	void setCompletedWaypoint(const Waypoint *pWay) {m_completedWaypoint = pWay;}

	const LocomotorSet& getLocomotorSet() const {return m_locomotorSet;}
	void setPathExtraDistance(Real dist) {m_pathExtraDistance = dist;}
	Real getPathExtraDistance() const { return m_pathExtraDistance; }

	virtual Bool chooseLocomotorSet(LocomotorSetType wst);

	virtual CommandSourceType getLastCommandSource() const { return m_lastCommandSource; }

	const AttackPriorityInfo *getAttackInfo() {return m_attackInfo;}
	void setAttackInfo(const AttackPriorityInfo *info) {m_attackInfo = info;}

	void setCurPathfindCell(const ICoord2D &cell) {m_pathfindCurCell = cell;}
	void setPathfindGoalCell(const ICoord2D &cell) {m_pathfindGoalCell = cell;}

	void setPathFromWaypoint(const Waypoint *way, const Coord2D *offset);

	const ICoord2D *getCurPathfindCell() const {return &m_pathfindCurCell;}
	const ICoord2D *getPathfindGoalCell() const {return &m_pathfindGoalCell;}

	/// Return true if our path has higher priority.
	Bool hasHigherPathPriority(AIUpdateInterface *otherAI) const;
	void setFinalPosition(const Coord3D *pos) { m_finalPosition = *pos; m_doFinalPosition = false;}

	virtual UpdateSleepTime update() override;	///< update this object's AI

	/// if we are attacking "fromID", stop that and attack "toID" instead
	void transferAttack(ObjectID fromID, ObjectID toID);

	void setCurrentVictim( const Object *nemesis );			///<  Current victim.
	Object *getCurrentVictim() const;
	virtual void notifyVictimIsDead() { }

	// if we are attacking a position (and NOT an object), return it. otherwise return null.
	const Coord3D *getCurrentVictimPos() const;

	void setLocomotorUpgrade(Bool set);

	// This function is used to notify the unit that it may have a target of opportunity to attack.
	void wakeUpAndAttemptToTarget();

	void resetNextMoodCheckTime();

	//Specifically set a frame to check next mood time -- added for the purpose of ordering a stealth combat unit that can't
	//autoacquire while stealthed, but isn't stealthed and can stealth and is not detected, and the player specifically orders
	//that unit to stop. In this case, instead of the unit autoacquiring another unit, and preventing him from stealthing,
	//we will instead delay the autoacquire until later to give him enough time to stealth properly.
	void setNextMoodCheckTime( UnsignedInt frame );

	///< States should call this with calledByAI set true to prevent them from checking every frame
	///< States that are doing idle checks should call with calledDuringIdle set true so that they check their

	Object *getNextMoodTarget( Bool calledByAI, Bool calledDuringIdle );
	UnsignedInt getNextMoodCheckTime() const { return m_nextMoodCheckTime; }

	// This function will return a combination of MoodMatrixParameter flags.
	UnsignedInt getMoodMatrixValue() const;
	UnsignedInt getMoodMatrixActionAdjustment( MoodMatrixAction action ) const;
	void setAttitude( AttitudeType tude );	///< set the behavior modifier for this agent

	// Common AI "status" effects -------------------------------------------------------------------
	Bool hasNationalism() const;
	Bool hasFanaticism() const;
	void evaluateMoraleBonus( Bool inHorde, Bool allowNationalism, HordeActionType type );
	void evaluateNationalismBonusClassic( Bool inHorde, Bool allowNationalism );
	void evaluateNationalismBonus( Bool inHorde, Bool allowNationalism );

#ifdef ALLOW_DEMORALIZE
	// demoralization ... what a nifty word to write.
	Bool isDemoralized() const { return m_demoralizedFramesLeft > 0; }
	void setDemoralized( UnsignedInt durationInFrames );
#endif

	Bool canPathThroughUnits() const { return m_canPathThroughUnits; }
	void setCanPathThroughUnits( Bool canPath ) { m_canPathThroughUnits = canPath; if (canPath) m_isBlockedAndStuck=false;}

	// Notify the ai that it has caused a crate to be created (usually by killing something.)
	void notifyCrate(ObjectID id) { m_crateCreated = id; }
	ObjectID getCrateID() const { return m_crateCreated; }
	Object* checkForCrateToPickup();

	void setIgnoreCollisionTime(Int frames) { m_ignoreCollisionsUntil = TheGameLogic->getFrame() + frames; }

	void setQueueForPathTime(Int frames);

	// For the attack move, that switches from move to attack, and the attack is CMD_FROM_AI,
	// while the move is the original command source.  John A.
	void friend_setLastCommandSource( CommandSourceType source ) {m_lastCommandSource = source;}

	Bool canAutoAcquire() const { return getAIUpdateModuleData()->m_autoAcquireEnemiesWhenIdle; }
	Bool canAutoAcquireWhileStealthed() const { return getAIUpdateModuleData()->m_autoAcquireEnemiesWhenIdle & AAS_Idle_Stealthed; }

protected:

	/*
		AIUpdates run in the initial phase.

		It's actually quite important that AI (the thing that drives Locomotors) and Physics
		run in the same order, relative to each other, for a given object; otherwise,
		interesting oscillations can occur in some situations, with friction being applied
		either before or after the locomotive force, making for huge stuttery messes. (srj)
	*/
	virtual SleepyUpdatePhase getUpdatePhase() const override { return PHASE_INITIAL; }

	void setGoalPositionClipped(const Coord3D* in, CommandSourceType cmdSource);

	virtual Bool isAllowedToRespondToAiCommands(const AICommandParms* parms) const;

	// getAttitude is protected because other places should call getMoodMatrixValue to get all the facts they need to consider.
	AttitudeType getAttitude() const;				///< get the current behavior modifier state.

	Bool blockedBy(Object *other); ///< Returns true if we are blocked by "other"
	Bool needToRotate(); ///< Returns true if we are not pointing in the right direction for movement.
	Real calculateMaxBlockedSpeed(Object *other) const;

	virtual UpdateSleepTime doLocomotor();	// virtual so subclasses can override
	void chooseGoodLocomotorFromCurrentSet();

	void setLastCommandSource( CommandSourceType source );

	// subclasses may want to override this, to use a subclass of AIStateMachine.
	virtual AIStateMachine* makeStateMachine();

	virtual Bool getTreatAsAircraftForLocoDistToGoal() const;

	// yes, protected, NOT public... try to avoid exposing the state machine directly, please
	AIStateMachine* getStateMachine() { return m_stateMachine; }
	const AIStateMachine* getStateMachine() const { return m_stateMachine; }

	void wakeUpNow();

public:

	StateID getCurrentStateID() const { return getStateMachine()->getCurrentStateID(); }	///< return the id of the current state of the machine
/// @ todo -- srj sez: JBA NUKE THIS CODE, IT IS EVIL
	void friend_addToWaypointGoalPath( const Coord3D *pathPoint ) { getStateMachine()->addToGoalPath(pathPoint); }

	// this is intended for use ONLY by W3dWaypointBuffer and AIFollowPathState.
	const Coord3D* friend_getGoalPathPosition( Int index ) const { return getStateMachine()->getGoalPathPosition( index ); }

	// this is intended for use ONLY by W3dWaypointBuffer.
	Int friend_getWaypointGoalPathSize() const;

	// this is intended for use ONLY by W3dWaypointBuffer.
	Int friend_getCurrentGoalPathIndex() const { return m_nextGoalPathIndex; }

	// this is intended for use ONLY by AIFollowPathState.
	void friend_setCurrentGoalPathIndex( Int index ) { m_nextGoalPathIndex = index; }
#ifdef DEBUG_LOGGING
	inline const Coord3D *friend_getRequestedDestination() const { return &m_requestedDestination; }
	inline const Coord3D *friend_getRequestedDestination2() const { return &m_requestedDestination2; }
#endif

	Object* getGoalObject() { return getStateMachine()->getGoalObject(); }	///< return the id of the current state of the machine
	const Coord3D* getGoalPosition() const { return getStateMachine()->getGoalPosition(); }	///< return the id of the current state of the machine

	WhichTurretType friend_getTurretSync() const { return m_turretSyncFlag; }
	void friend_setTurretSync(WhichTurretType t) { m_turretSyncFlag = t; }

	UnsignedInt getPriorWaypointID () { return m_priorWaypointID; };
	UnsignedInt getCurrentWaypointID () { return m_currentWaypointID; };

	void clearMoveOutOfWay() {m_moveOutOfWay1 = INVALID_ID; m_moveOutOfWay2 = INVALID_ID;}

	void setTmpValue(Int val) {m_tmpInt = val;}
	Int getTmpValue() {return m_tmpInt;}

	Bool getRetryPath() {return m_retryPath;}

	void setAllowedToChase( Bool allow ) { m_allowedToChase = allow; }
	Bool isAllowedToChase() const { return m_allowedToChase; }

	// only for AIStateMachine.
	virtual void friend_notifyStateMachineChanged();

private:
	// this should only be called by load/save, or by chooseLocomotorSet.
	// it does no sanity checking; it just jams it in.
	Bool chooseLocomotorSetExplicit(LocomotorSetType wst);

private:
	UnsignedInt					m_priorWaypointID;						///< ID of the path we follwed to before the most recent one
	UnsignedInt					m_currentWaypointID;					///< ID of the most recent one...

	AIStateMachine*			m_stateMachine;							///< the state machine
	UnsignedInt					m_nextEnemyScanTime;				///< how long until the next enemy scan
	ObjectID						m_currentVictimID;					///< if not INVALID_ID, this agent's current victim.
	Real								m_desiredSpeed;							///< the desired speed of the tank
	CommandSourceType		m_lastCommandSource;			/**< Keep track of the source of the last command we got.
																									This is set immediately before the SetState that goes
																									to the state machine, so onEnter in there can know where
																									the command came from.
																								*/

	GuardMode							m_guardMode;
	GuardTargetType				m_guardTargetType[2];
	Coord3D								m_locationToGuard;
	ObjectID							m_objectToGuard;
	const PolygonTrigger*	m_areaToGuard;

	// Attack Info --------------------------------------------------------------------------------------------
	const AttackPriorityInfo*	m_attackInfo;

	// "Planning Mode" -----------------------------------------------------------------------------------------
	enum { MAX_WAYPOINTS = 16 };
	Coord3D						m_waypointQueue[ MAX_WAYPOINTS ];		///< the waypoint queue
	Int								m_waypointCount;										///< number of waypoints in the queue
	Int								m_waypointIndex;										///< current waypoint we are moving to
	const Waypoint*		m_completedWaypoint;								///< Set to the last waypoint in a path when the FollowWaypointPath is complete.

	// Pathfinding ---------------------------------------------------------------------------------------------
	Path*				m_path;											///< current path to follow (for moving)
	ObjectID		m_requestedVictimID;
	Coord3D			m_requestedDestination;
	Coord3D			m_requestedDestination2;
	UnsignedInt	m_pathTimestamp;						///< time of path construction
	ObjectID		m_ignoreObstacleID;					///< ignore this obstacle when pathfinding
	Real				m_pathExtraDistance;				///< If we are following a waypoint path, there will be extra distance beyond the current path.
	ICoord2D		m_pathfindGoalCell;					///< Cell we are moving towards.
	ICoord2D		m_pathfindCurCell;					///< Cell we are currently occupying.
	Int					m_blockedFrames;						///< Number of frames we've been blocked.
	Real				m_curMaxBlockedSpeed;				///< Max speed we can have and not run into blocking things.
	Real				m_bumpSpeedLimit;						///< Max speed after bumping a unit.
	UnsignedInt	m_ignoreCollisionsUntil;		///< Timer to cheat if we get stuck.
	/**
		If nonzero and >= now, frame when we should queueForPath.

		NEVER EVER EVER set this directly. you must call setQueueForPathTime() in ALL CASES.
		NEVER EVER EVER set this directly. you must call setQueueForPathTime() in ALL CASES.
		NEVER EVER EVER set this directly. you must call setQueueForPathTime() in ALL CASES.
		NEVER EVER EVER set this directly. you must call setQueueForPathTime() in ALL CASES.
	*/
	UnsignedInt	m_queueForPathFrame;
	Coord3D			m_finalPosition;						///< Final position for the moveto states.
	ObjectID		m_repulsor1;								///< First object we are running away from.
	ObjectID		m_repulsor2;								///< Second object we are running away from.
	Int					m_nextGoalPathIndex;				///< The simple goal path index we are moving to next.
	ObjectID		m_moveOutOfWay1;
	ObjectID		m_moveOutOfWay2;

	// Locomotors -------------------------------------------------------------------------------------------------
	enum LocoGoalType	 // Note - written out in save/load xfer, don't change these numbers.  jba.
	{
		NONE = 0,
		POSITION_ON_PATH = 1,
		POSITION_EXPLICIT = 2,
		ANGLE = 3
	};
	LocomotorSet			m_locomotorSet;
	Locomotor*				m_curLocomotor;
	LocomotorSetType	m_curLocomotorSet;
	LocoGoalType			m_locomotorGoalType;
	Coord3D						m_locomotorGoalData;

	// Turrets -------------------------------------------------------------------------------------------------
	TurretAI*					m_turretAI[MAX_TURRETS];		// ai for our turret (or null if no turret)
	WhichTurretType		m_turretSyncFlag;						///< for private use by multiturreted units where the turrets must sync with each other

	// AI -------------------------------------------------------------------------------------------
	AttitudeType	m_attitude;
	UnsignedInt		m_nextMoodCheckTime;

	// Common AI "status" effects -------------------------------------------------------------------
#ifdef ALLOW_DEMORALIZE
	UnsignedInt   m_demoralizedFramesLeft;
#endif
#ifdef ALLOW_SURRENDER
	UnsignedInt		m_surrenderedFramesLeft;				///< Non-zero when in a surrendered state
	Int						m_surrenderedPlayerIndex;				///< if surrendered, the playerindex to whom we are surrendered, or -1 if available for everyone
#endif

	// Note the id of a crate we caused to be created.
	ObjectID			m_crateCreated;

	Int						m_tmpInt;

	// -------------------------------------------------------------------
	Bool				m_doFinalPosition;					///< True if we are moving towards final position in a non-physics kind of way.
	Bool				m_waitingForPath;						///< True if we have a pathfind request outstanding.
	Bool				m_isAttackPath;							///< True if we have an attack path.
	Bool				m_isFinalGoal;							///< True if this path ends at our destination (as opposed to an intermediate point on a waypoint path).
	Bool				m_isApproachPath;						///< True if we are just approaching to tighten.
	Bool				m_isSafePath;								///< True if we are just approaching to tighten.
	Bool				m_movementComplete;					///< True if we finished an AIInternalMoveToState.
	Bool				m_isMoving;									///< True if we are in an AIInternalMoveToState.
	Bool				m_isBlocked;
	Bool				m_isBlockedAndStuck;				///< True if we are stuck & need to recompute path.
	Bool				m_upgradedLocomotors;
	Bool				m_canPathThroughUnits;			///< Can path through units.
	Bool				m_randomlyOffsetMoodCheck;	///< If true, randomly offset the mood check rate next time, to avoid "spiking" of ai checks
	Bool				m_isAiDead;									///< TRUE if dead
	Bool				m_isRecruitable;						///< TRUE if recruitable by the ai.
	Bool				m_executingWaypointQueue;						///< if true, we are moving thru the waypoints
	Bool				m_retryPath;								///< If true, we need to try the path a second time.  jba.
	Bool				m_allowedToChase;						///< Allowed to pursue targets.
	Bool				m_isInUpdate;								///< If true, we are inside our update method.
	Bool				m_fixLocoInPostProcess;
};

//------------------------------------------------------------------------------------------------------------
// Inlines
//
