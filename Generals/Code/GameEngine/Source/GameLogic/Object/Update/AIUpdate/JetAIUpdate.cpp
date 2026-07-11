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

// JetAIUpdate.cpp //////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_LOCOMOTORSET_NAMES

#include "Common/ActionManager.h"
#include "Common/GlobalData.h"
#include "Common/MiscAudio.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/JetAIUpdate.h"
#include "GameLogic/Module/ParkingPlaceBehavior.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"

const Real BIGNUM = 99999.0f;


//-------------------------------------------------------------------------------------------------
enum TaxiType CPP_11(: Int)
{
	FROM_HANGAR,
	FROM_PARKING,
	TO_PARKING
};

//-------------------------------------------------------------------------------------------------
enum JetAIStateType CPP_11(: Int)
{
	// note that these must be distinct (numerically) from AIStateType. ick.
	JETAISTATETYPE_FIRST = 1000,

	TAXI_FROM_HANGAR,
	TAKING_OFF_AWAIT_CLEARANCE,
	TAXI_TO_TAKEOFF,
	PAUSE_BEFORE_TAKEOFF,
	TAKING_OFF,
	LANDING_AWAIT_CLEARANCE,
	LANDING,
	TAXI_FROM_LANDING,
	ORIENT_FOR_PARKING_PLACE,
	RELOAD_AMMO,
	RETURNING_FOR_LANDING,
	RETURN_TO_DEAD_AIRFIELD,
	CIRCLING_DEAD_AIRFIELD,

	JETAISTATETYPE_LAST
};

//-------------------------------------------------------------------------------------------------
static Bool isOutOfSpecialReloadAmmo(Object* jet)
{
	// if we have at least one special reload weapon,
	// AND all such weapons are out of ammo,
	// return true.
	Int specials = 0;
	Int out = 0;
	for( Int i = 0; i < WEAPONSLOT_COUNT;	i++ )
	{
		Weapon* weapon = jet->getWeaponInWeaponSlot((WeaponSlotType)i);
		if (weapon == nullptr || weapon->getReloadType() != RETURN_TO_BASE_TO_RELOAD)
			continue;
		++specials;
		if (weapon->getStatus() == OUT_OF_AMMO)
			++out;
	}
	return specials > 0 && out == specials;
}

//-------------------------------------------------------------------------------------------------
static ParkingPlaceBehaviorInterface* getPP(ObjectID id, Object** airfieldPP = nullptr)
{
	if (airfieldPP)
		*airfieldPP = nullptr;

	Object* airfield = TheGameLogic->findObjectByID( id );
	if (airfield == nullptr || airfield->isEffectivelyDead() || !airfield->isKindOf(KINDOF_FS_AIRFIELD) || airfield->testStatus(OBJECT_STATUS_SOLD))
		return nullptr;

	if (airfieldPP)
		*airfieldPP = airfield;

	ParkingPlaceBehaviorInterface* pp = nullptr;
	for (BehaviorModule** i = airfield->getBehaviorModules(); *i; ++i)
	{
		if ((pp = (*i)->getParkingPlaceBehaviorInterface()) != nullptr)
			break;
	}

	return pp;
}

//-------------------------------------------------------------------------------------------------
class PartitionFilterHasParkingPlace : public PartitionFilter
{
private:
	ObjectID m_id;
public:
	PartitionFilterHasParkingPlace(ObjectID id) : m_id(id) { }
protected:
#if defined(RTS_DEBUG)
	virtual const char* debugGetName() override { return "PartitionFilterHasParkingPlace"; }
#endif
	virtual Bool allow(Object *objOther) override
	{
		ParkingPlaceBehaviorInterface* pp = getPP(objOther->getID());
		if (pp != nullptr && pp->reserveSpace(m_id, 0.0f, nullptr))
			return true;
		return false;
	}
};

//-------------------------------------------------------------------------------------------------
static Object* findSuitableAirfield(Object* jet)
{
	PartitionFilterAcceptByKindOf					filterKind(MAKE_KINDOF_MASK(KINDOF_FS_AIRFIELD), KINDOFMASK_NONE);
	PartitionFilterRejectByObjectStatus		filterStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ), OBJECT_STATUS_MASK_NONE );
	PartitionFilterRejectByObjectStatus		filterStatusTwo( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_SOLD ), OBJECT_STATUS_MASK_NONE ); // Independent to make it an OR
	PartitionFilterRelationship						filterTeam(jet, PartitionFilterRelationship::ALLOW_ALLIES);
	PartitionFilterAlive									filterAlive;
	PartitionFilterSameMapStatus					filterMapStatus(jet);
	PartitionFilterHasParkingPlace				filterPP(jet->getID());

	PartitionFilter *filters[16];
	Int numFilters = 0;
	filters[numFilters++] = &filterKind;
	filters[numFilters++] = &filterStatus;
	filters[numFilters++] = &filterStatusTwo;
	filters[numFilters++] = &filterTeam;
	filters[numFilters++] = &filterAlive;
	filters[numFilters++] = &filterPP;
	filters[numFilters++] = &filterMapStatus;
	filters[numFilters] = nullptr;

	return ThePartitionManager->getClosestObject( jet, HUGE_DIST, FROM_CENTER_2D, filters );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/*
	Success: we have runway clearance
	Failure: no runway clearance
*/
class JetAwaitingRunwayState : public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetAwaitingRunwayState, "JetAwaitingRunwayState")
protected:
	// snapshot interface STUBBED.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {XferVersion cv = 1;	XferVersion v = cv; xfer->xferVersion( &v, cv );}
	virtual void loadPostProcess() override {};
private:
	const Bool m_landing;

public:
	JetAwaitingRunwayState( StateMachine *machine, Bool landing ) : m_landing(landing), State( machine, "JetAwaitingRunwayState") { }

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

		jetAI->friend_setTakeoffInProgress(!m_landing);
		jetAI->friend_setLandingInProgress(m_landing);
		jetAI->friend_setAllowCircling(true);
		return STATE_CONTINUE;
	}

	virtual StateReturnType update() override
	{
		Object* jet = getMachineOwner();
		if (jet->isEffectivelyDead())
			return STATE_FAILURE;

		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
		if (pp == nullptr)
		{
			// no producer? just skip this step.
			return STATE_SUCCESS;
		}

		// gotta reserve a space in order to reserve a runway
		if (!pp->reserveSpace(jet->getID(), jetAI->friend_getParkingOffset(), nullptr))
		{
			DEBUG_ASSERTCRASH(m_landing, ("hmm, this should never happen for taking-off things"));
			return STATE_FAILURE;
		}

		if (pp->reserveRunway(jet->getID(), m_landing))
		{
			return STATE_SUCCESS;
		}

		// can't get a runway? gotta wait.
		jetAI->setLocomotorGoalNone();
		return STATE_CONTINUE;
	}

	virtual void onExit(StateExitType status) override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if (jetAI)
		{
			jetAI->friend_setTakeoffInProgress(false);
			jetAI->friend_setLandingInProgress(false);
			jetAI->friend_setAllowCircling(false);
		}
	}

};
EMPTY_DTOR(JetAwaitingRunwayState)

//-------------------------------------------------------------------------------------------------
/*
	Success: a new suitable airfield has appeared
	Failure: shouldn't normally happen
*/
class JetOrHeliCirclingDeadAirfieldState : public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetOrHeliCirclingDeadAirfieldState, "JetOrHeliCirclingDeadAirfieldState")
protected:
	// snapshot interface	 STUBBED.
	// The state will check immediately after a load game, but I think that's ok.  jba.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {XferVersion cv = 1;	XferVersion v = cv; xfer->xferVersion( &v, cv );}
	virtual void loadPostProcess() override {};

private:
	Int m_checkAirfield;

	enum
	{
		// only recheck for new airfields every second or so
		HOW_OFTEN_TO_CHECK = LOGICFRAMES_PER_SECOND
	};

public:
	JetOrHeliCirclingDeadAirfieldState( StateMachine *machine ) :
		State( machine, "JetOrHeliCirclingDeadAirfieldState"),
		m_checkAirfield(0) { }

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
		{
			return STATE_FAILURE;
		}

		// obscure case: if the jet wasn't spawned, but just placed directly on the map,
		// it might not have an owning airfield, and it might be trying to return
		// simply due to being idle, not out of ammo. so check and don't die in that
		// case, but just punt back out to idle.
		if (!isOutOfSpecialReloadAmmo(jet) && jet->getProducerID() == INVALID_ID)
		{
			return STATE_FAILURE;
		}

		// just stay where we are.
		jetAI->setLocomotorGoalNone();

		m_checkAirfield = HOW_OFTEN_TO_CHECK;

		//Play the "low fuel" voice whenever the craft is circling above the airfield.
		AudioEventRTS soundToPlay = *jet->getTemplate()->getPerUnitSound( "VoiceLowFuel" );
		soundToPlay.setObjectID( jet->getID() );
		TheAudio->addAudioEvent( &soundToPlay );

		return STATE_CONTINUE;
	}

	virtual StateReturnType update() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
		{
			return STATE_FAILURE;
		}

		// just stay where we are.
		jetAI->setLocomotorGoalNone();

		Real damageRate = jetAI->friend_getOutOfAmmoDamagePerSecond();
		if (damageRate > 0)
		{
			// convert to damage/sec to damage/frame
			damageRate *= SECONDS_PER_LOGICFRAME_REAL;
			// since it's a percentage, multiply times the max health
			damageRate *= jet->getBodyModule()->getMaxHealth();

			DamageInfo damageInfo;
			damageInfo.in.m_damageType = DAMAGE_UNRESISTABLE;
			damageInfo.in.m_deathType = DEATH_NORMAL;
			damageInfo.in.m_sourceID = INVALID_ID;
			damageInfo.in.m_amount = damageRate;
			jet->attemptDamage( &damageInfo );
		}

		if (--m_checkAirfield <= 0)
		{
			m_checkAirfield = HOW_OFTEN_TO_CHECK;
			Object* airfield = findSuitableAirfield( jet );
			if (airfield)
			{
				jet->setProducer(airfield);
				return STATE_SUCCESS;
			}
		}

		return STATE_CONTINUE;
	}

};
EMPTY_DTOR(JetOrHeliCirclingDeadAirfieldState)

//-------------------------------------------------------------------------------------------------
/*
	Success: we returned to the dead-airfield location
	Failure: shouldn't normally happen
*/
class JetOrHeliReturningToDeadAirfieldState : public AIInternalMoveToState
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetOrHeliReturningToDeadAirfieldState, "JetOrHeliReturningToDeadAirfieldState")
public:
	JetOrHeliReturningToDeadAirfieldState( StateMachine *machine ) : AIInternalMoveToState( machine, "JetOrHeliReturningToDeadAirfieldState") { }

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
		{
			return STATE_FAILURE;
		}

		setAdjustsDestination(true);
		m_goalPosition = *jetAI->friend_getProducerLocation();

		return AIInternalMoveToState::onEnter();
	}

};
EMPTY_DTOR(JetOrHeliReturningToDeadAirfieldState)

//-------------------------------------------------------------------------------------------------
// This solution uses the
// http://www.faqs.org/faqs/graphics/algorithms-faq/
// Subject 1.03
static Bool intersectInfiniteLine2D
(
	Real ax, Real ay, Real ao,
	Real cx, Real cy, Real co,
	Real& ix, Real& iy
)
{
	Real bx = ax + Cos(ao);
	Real by = ay + Sin(ao);
	Real dx = cx + Cos(co);
	Real dy = cy + Sin(co);

	Real denom = ((bx - ax) * (dy - cy) - (by - ay) * (dx - cx));
	if (denom == 0.0f)
	{
		// the lines are parallel.
		return false;
	}

	// The lines intersect.
	Real r = ((ay - cy) * (dx - cx) - (ax - cx) * (dy - cy) ) / denom;
	ix = ax + r * (bx - ax);
	iy = ay + r * (by - ay);
	return true;
}

//-------------------------------------------------------------------------------------------------
/*
	Success: we are on the ground at the runway start
	Failure: we are unable to get on the ground
*/
class JetOrHeliTaxiState : public AIMoveOutOfTheWayState
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetOrHeliTaxiState, "JetOrHeliTaxiState")
private:
	TaxiType m_taxiMode;
public:
	JetOrHeliTaxiState( StateMachine *machine, TaxiType m ) : m_taxiMode(m), AIMoveOutOfTheWayState( machine ) { }

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

		jetAI->setCanPathThroughUnits(true);
		jetAI->friend_setTakeoffInProgress(m_taxiMode != TO_PARKING);
		jetAI->friend_setLandingInProgress(m_taxiMode == TO_PARKING);
		jetAI->friend_setTaxiInProgress(true);

		jetAI->friend_setAllowAirLoco(false);
		jetAI->chooseLocomotorSet(LOCOMOTORSET_TAXIING);
		DEBUG_ASSERTCRASH(jetAI->getCurLocomotor(), ("no loco"));

		Object* airfield;
		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID(), &airfield);
		if (pp == nullptr)
			return STATE_SUCCESS;	// no airfield? just skip this step.

		ParkingPlaceBehaviorInterface::PPInfo ppinfo;
		if (!pp->reserveSpace(jet->getID(), jetAI->friend_getParkingOffset(), &ppinfo))
			return STATE_FAILURE;	// full?

		Coord3D intermedPt;
		Bool intermed = false;
		Real orient = WWMath::Atan2Origin(ppinfo.runwayPrep.y - ppinfo.parkingSpace.y, ppinfo.runwayPrep.x - ppinfo.parkingSpace.x);
		if (fabs(stdAngleDiff(orient, ppinfo.parkingOrientation)) > PI/128)
		{
			intermedPt.z = (ppinfo.parkingSpace.z + ppinfo.runwayPrep.z) * 0.5f;
			intermed = intersectInfiniteLine2D(
				ppinfo.parkingSpace.x, ppinfo.parkingSpace.y, ppinfo.parkingOrientation,
				ppinfo.runwayPrep.x, ppinfo.runwayPrep.y, ppinfo.parkingOrientation + PI/2,
				intermedPt.x, intermedPt.y);
		}

		jetAI->destroyPath();
		Path *movePath;
		movePath = newInstance(Path);
		Coord3D pos = *jet->getPosition();
		movePath->prependNode( &pos, LAYER_GROUND );
		movePath->markOptimized();

		if (m_taxiMode == TO_PARKING)
		{
			movePath->appendNode( &ppinfo.runwayPrep, LAYER_GROUND );
			if (intermed)
				movePath->appendNode( &intermedPt, LAYER_GROUND );
			movePath->appendNode( &ppinfo.parkingSpace, LAYER_GROUND );
		}
		else if (m_taxiMode == FROM_PARKING)
		{
			if (intermed)
				movePath->appendNode( &intermedPt, LAYER_GROUND );
			movePath->appendNode( &ppinfo.runwayPrep, LAYER_GROUND );
			movePath->appendNode( &ppinfo.runwayStart, LAYER_GROUND );
		}
		else if (m_taxiMode == FROM_HANGAR)
		{
			movePath->appendNode( &ppinfo.parkingSpace, LAYER_GROUND );
		}

		m_waitingForPath = FALSE;
		TheAI->pathfinder()->setDebugPath(movePath);

		setAdjustsDestination(false);	// precision is necessary

		jetAI->friend_setPath( movePath );
		DEBUG_ASSERTCRASH(jetAI->getCurLocomotor(), ("no loco"));
		jetAI->getCurLocomotor()->setUsePreciseZPos(true);
		jetAI->getCurLocomotor()->setUltraAccurate(true);
		jetAI->getCurLocomotor()->setAllowInvalidPosition(true);
		jetAI->ignoreObstacleID(jet->getProducerID());

		StateReturnType ret = AIMoveOutOfTheWayState::onEnter();
		return ret;
	}

	virtual void onExit( StateExitType status ) override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if (jetAI)
		{
			jetAI->getCurLocomotor()->setUsePreciseZPos(false);
			jetAI->getCurLocomotor()->setUltraAccurate(false);
			jetAI->getCurLocomotor()->setAllowInvalidPosition(false);
			jetAI->friend_setTakeoffInProgress(false);
			jetAI->friend_setLandingInProgress(false);
			jetAI->friend_setTaxiInProgress(false);
			jetAI->setCanPathThroughUnits(false);
		}

		AIMoveOutOfTheWayState::onExit(status);
	}

};
EMPTY_DTOR(JetOrHeliTaxiState)

//-------------------------------------------------------------------------------------------------
/*
	Success: we are on the ground at the runway start
	Failure: we are unable to get on the ground
*/
class JetTakeoffOrLandingState : public AIFollowPathState
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetTakeoffOrLandingState, "JetTakeoffOrLandingState")
private:
	Real			m_maxLift;
	Real			m_maxSpeed;
#ifdef CIRCLE_FOR_LANDING
	Coord3D		m_circleForLandingPos;
#endif
	Bool			m_landing;
	Bool			m_landingSoundPlayed;

public:
	JetTakeoffOrLandingState( StateMachine *machine, Bool landing ) : m_landing(landing), AIFollowPathState( machine, "JetTakeoffOrLandingState" ) { }

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if (!jetAI)
			return STATE_FAILURE;

		if (jet->isEffectivelyDead())
			return STATE_FAILURE;

		jetAI->friend_setTakeoffInProgress(!m_landing);
		jetAI->friend_setLandingInProgress(m_landing);
		jetAI->friend_setAllowAirLoco(true);
		jetAI->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
		Locomotor* loco = jetAI->getCurLocomotor();
		DEBUG_ASSERTCRASH(loco, ("no loco"));
		loco->setMaxLift(BIGNUM);
		BodyDamageType bdt = jet->getBodyModule()->getDamageState();
		m_maxLift = loco->getMaxLift(bdt);
		m_maxSpeed = loco->getMaxSpeedForCondition(bdt);
		m_landingSoundPlayed = FALSE;
		if (m_landing)
		{
			loco->setMaxSpeed(loco->getMinSpeed());
		}
		else
		{
			loco->setMaxLift(0);
		}
		loco->setUsePreciseZPos(true);
		loco->setUltraAccurate(true);
		jetAI->ignoreObstacleID(jet->getProducerID());

		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
		if (pp == nullptr)
			return STATE_SUCCESS;	// no airfield? just skip this step

		ParkingPlaceBehaviorInterface::PPInfo ppinfo;
		if (!pp->reserveSpace(jet->getID(), jetAI->friend_getParkingOffset(), &ppinfo))
		{
			// it's full.
			return STATE_FAILURE;
		}

		// only check this for landing; we might have already given up the reservation to the guy behind us for takeoff
		if (m_landing)
		{
			if (!pp->reserveRunway(jet->getID(), m_landing))
			{
				DEBUG_CRASH(("we should never get to this state unless we have a runway available"));
				return STATE_FAILURE;
			}
		}

		std::vector<Coord3D> path;
		if (m_landing)
		{
#ifdef CIRCLE_FOR_LANDING
			m_circleForLandingPos = ppinfo.runwayApproach;
			m_circleForLandingPos.z = (ppinfo.runwayEnd.z + ppinfo.runwayApproach.z)*0.5f;
#else
			path.push_back(ppinfo.runwayApproach);
#endif
			path.push_back(ppinfo.runwayEnd);
			path.push_back(ppinfo.runwayStart);
		}
		else
		{
			ppinfo.runwayEnd.z = ppinfo.runwayApproach.z;
			path.push_back(ppinfo.runwayEnd);
			path.push_back(ppinfo.runwayApproach);
		}

		setAdjustsDestination(false);	// precision is necessary
		setAdjustFinalDestination(false); // especially at the endpoint!

		jetAI->friend_setGoalPath( &path );

		StateReturnType ret = AIFollowPathState::onEnter();

		return ret;
	}

	virtual StateReturnType update() override
	{
		Object* jet = getMachineOwner();
		if (jet->isEffectivelyDead())
			return STATE_FAILURE;

		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

		if (m_landing)
		{
#ifdef CIRCLE_FOR_LANDING
			if (jet->getPosition()->z > m_circleForLandingPos.z)
			{
				const Real THRESH = 4.0f;
				jetAI->getCurLocomotor()->setAltitudeChangeThresholdForCircling(THRESH);
				jetAI->setLocomotorGoalPositionExplicit(m_circleForLandingPos);
				return STATE_CONTINUE;
			}
			else
#endif
			{
				jetAI->getCurLocomotor()->setMaxLift(BIGNUM);
#ifdef CIRCLE_FOR_LANDING
				jetAI->getCurLocomotor()->setAltitudeChangeThresholdForCircling(0);
#endif
			}

			if( !m_landingSoundPlayed )
			{
				Real zPos = jet->getPosition()->z;
				Real zSlop = 0.25f;
				PathfindLayerEnum layer = TheTerrainLogic->getHighestLayerForDestination( jet->getPosition() );
				Real groundZ = TheTerrainLogic->getLayerHeight( jet->getPosition()->x, jet->getPosition()->y, layer );

				if( zPos - zSlop <= groundZ )
				{
					m_landingSoundPlayed = TRUE;
					AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_aircraftWheelScreech;
					soundToPlay.setPosition( jet->getPosition() );
					TheAudio->addAudioEvent( &soundToPlay );
				}
			}
		}
		else
		{
			ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
			if (pp)
				pp->transferRunwayReservationToNextInLineForTakeoff(jet->getID());

			PhysicsBehavior* physics = jet->getPhysics();
			Real ratio = physics->getVelocityMagnitude() / (m_maxSpeed * jetAI->friend_getTakeoffSpeedForMaxLift());
			if (ratio < 0.0f) ratio = 0.0f;
			if (ratio > 1.0f) ratio = 1.0f;
			jetAI->getCurLocomotor()->setMaxLift(m_maxLift * ratio);
		}

		StateReturnType ret = AIFollowPathState::update();
		return ret;
	}

	virtual void onExit( StateExitType status ) override
	{
		AIFollowPathState::onExit(status);

		// just in case.
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return;

		jetAI->friend_setTakeoffInProgress(false);
		jetAI->friend_setLandingInProgress(false);
		jetAI->friend_enableAfterburners(false);

		// Paranoia checks - sometimes onExit is called when we are
		// shutting down, and not all pieces are valid.  CurLocomotor
		// is definitely null in some cases. jba.
		Locomotor* loco = jetAI->getCurLocomotor();
		if (loco)
		{
			loco->setUsePreciseZPos(false);
			loco->setUltraAccurate(false);
			// don't restore lift if dead -- this may fight with JetSlowDeathBehavior!
			if (!jet->isEffectivelyDead())
				loco->setMaxLift(BIGNUM);
#ifdef CIRCLE_FOR_LANDING
			loco->setAltitudeChangeThresholdForCircling(0);
#endif
		}
		jetAI->ignoreObstacleID(INVALID_ID);
		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
		if (!m_landing)
		{
			if (pp && !jetAI->friend_keepsParkingSpaceWhenAirborne())
				pp->releaseSpace(jet->getID());
		}
		if (pp)
			pp->releaseRunway(jet->getID());
	}
};
EMPTY_DTOR(JetTakeoffOrLandingState)

//-------------------------------------------------------------------------------------------------
static Real calcDistSqr(const Coord3D& a, const Coord3D& b)
{
	return sqr(a.x-b.x) + sqr(a.y-b.y) + sqr(a.z-b.z);
}

//-------------------------------------------------------------------------------------------------
/*
	Success: we are on the ground at the runway start
	Failure: we are unable to get on the ground
*/
class HeliTakeoffOrLandingState : public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HeliTakeoffOrLandingState, "HeliTakeoffOrLandingState")
protected:
	// snapshot interface
	virtual void crc( Xfer *xfer ) override
	{
		// empty. jba.
	}

	virtual void xfer( Xfer *xfer ) override
	{
		// version
		XferVersion currentVersion = 1;
		XferVersion version = currentVersion;
		xfer->xferVersion( &version, currentVersion );

		// set on create. xfer->xferBool(&m_landing);
		xfer->xferCoord3D(&m_path[0]);
		xfer->xferCoord3D(&m_path[1]);
		xfer->xferInt(&m_index);
		xfer->xferCoord3D(&m_parkingLoc);
		xfer->xferReal(&m_parkingOrientation);
	}
	virtual void loadPostProcess() override
	{
		// empty. jba.
	}

private:
	Coord3D		m_path[2];
	Int				m_index;
	Coord3D		m_parkingLoc;
	Real			m_parkingOrientation;
	Bool			m_landing;
public:
	HeliTakeoffOrLandingState( StateMachine *machine, Bool landing ) : m_landing(landing),
		State( machine, "HeliTakeoffOrLandingState" ), m_index(0)
		{
			m_parkingLoc.zero();
		}

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

		jetAI->friend_setTakeoffInProgress(!m_landing);
		jetAI->friend_setLandingInProgress(m_landing);
		jetAI->friend_setAllowAirLoco(true);
		jetAI->chooseLocomotorSet(LOCOMOTORSET_NORMAL);

		Locomotor* loco = jetAI->getCurLocomotor();
		DEBUG_ASSERTCRASH(loco, ("no loco"));
		loco->setUsePreciseZPos(true);
		loco->setUltraAccurate(true);
		jetAI->ignoreObstacleID(jet->getProducerID());

		Object* airfield;
		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID(), &airfield);
		if (pp == nullptr)
			return STATE_SUCCESS;	// no airfield? just skip this step

		Coord3D landingApproach;
		if (jet->isKindOf(KINDOF_PRODUCED_AT_HELIPAD))
		{
			if (m_landing)
			{
				m_parkingLoc = jetAI->friend_getLandingPosForHelipadStuff();
				m_parkingOrientation = jet->getOrientation();
			}
			else
			{
				m_parkingOrientation = jet->getOrientation();
				m_parkingLoc = *jet->getPosition();
			}
			landingApproach = m_parkingLoc;
			landingApproach.z += pp->getApproachHeight();
		}
		else
		{
			ParkingPlaceBehaviorInterface::PPInfo ppinfo;
			if (!pp->reserveSpace(jet->getID(), jetAI->friend_getParkingOffset(), &ppinfo))
				return STATE_FAILURE;
			m_parkingLoc = ppinfo.parkingSpace;
			m_parkingOrientation = ppinfo.parkingOrientation;
			landingApproach = m_parkingLoc;
			landingApproach.z += (ppinfo.runwayApproach.z - ppinfo.runwayEnd.z);
		}

		if (m_landing)
		{
			m_path[0] = landingApproach;
			m_path[1] = m_parkingLoc;
		}
		else
		{
			m_path[0] = m_parkingLoc;
			m_path[1] = landingApproach;
			m_path[1].z = landingApproach.z;
		}
		m_index = 0;

		return STATE_CONTINUE;
	}

	virtual StateReturnType update() override
	{
		Object* jet = getMachineOwner();
		if (jet->isEffectivelyDead())
			return STATE_FAILURE;

		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

// I have disabled this because it is no longer necessary and is a bit funky lookin' (srj)
#ifdef NOT_IN_USE
		// magically position it correctly.
		jet->getPhysics()->scrubVelocity2D(0);
		Coord3D hoverloc = m_path[m_index];
		hoverloc.z = jet->getPosition()->z;
#if 1
		Coord3D pos = *jet->getPosition();
		Real dx = hoverloc.x - pos.x;
		Real dy = hoverloc.y - pos.y;
		Real dSqr = dx*dx+dy*dy;
		const Real DARN_CLOSE = 0.25f;
		if (dSqr < DARN_CLOSE)
		{
			jet->setPosition(&hoverloc);
		}
		else
		{
			Real dist = WWMath::SqrtfOrigin(dSqr);
			if (dist<1) dist = 1;
			pos.x += PATHFIND_CELL_SIZE_F*dx/(dist*LOGICFRAMES_PER_SECOND);
			pos.y += PATHFIND_CELL_SIZE_F*dy/(dist*LOGICFRAMES_PER_SECOND);
			jet->setPosition(&pos);
		}
#else
		jet->setPosition(&hoverloc);
#endif
		jet->setOrientation(m_parkingOrientation);
#endif

		if (jet->isKindOf(KINDOF_PRODUCED_AT_HELIPAD) || !m_landing)
		{
			TheAI->pathfinder()->adjustDestination(jet, jetAI->getLocomotorSet(), &m_path[m_index]);
			TheAI->pathfinder()->updateGoal(jet, &m_path[m_index], LAYER_GROUND);
		}

		jetAI->setLocomotorGoalPositionExplicit(m_path[m_index]);

		const Real THRESH = 3.0f;
		const Real THRESH_SQR = THRESH*THRESH;
		const Coord3D* a = jet->getPosition();
		const Coord3D* b = &m_path[m_index];
		Real distSqr = calcDistSqr(*a, *b);
		if (distSqr <= THRESH_SQR)
			++m_index;

		if (m_index >= 2)
			return STATE_SUCCESS;

		return STATE_CONTINUE;
	}

	virtual void onExit( StateExitType status ) override
	{
		// just in case.
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return;

		jetAI->friend_setTakeoffInProgress(false);
		jetAI->friend_setLandingInProgress(false);

		// Paranoia checks - sometimes onExit is called when we are
		// shutting down, and not all pieces are valid.  CurLocomotor
		// is definitely null in some cases. jba.
		Locomotor* loco = jetAI->getCurLocomotor();
		if (loco)
		{
			loco->setUsePreciseZPos(false);
			loco->setUltraAccurate(false);
			// don't restore lift if dead -- this may fight with JetSlowDeathBehavior!
			if (!jet->isEffectivelyDead())
				loco->setMaxLift(BIGNUM);
		}

		jetAI->ignoreObstacleID(INVALID_ID);
		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
		if (m_landing)
		{
			jetAI->friend_setAllowAirLoco(false);
			jetAI->chooseLocomotorSet(LOCOMOTORSET_TAXIING);
		}
		else
		{
			if (pp && !jetAI->friend_keepsParkingSpaceWhenAirborne())
				pp->releaseSpace(jet->getID());
		}
	}

};
EMPTY_DTOR(HeliTakeoffOrLandingState)

//-------------------------------------------------------------------------------------------------
class JetOrHeliParkOrientState : public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetOrHeliParkOrientState, "JetOrHeliParkOrientState")
protected:
	// snapshot interface STUBBED.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {XferVersion cv = 1;	XferVersion v = cv; xfer->xferVersion( &v, cv );}
	virtual void loadPostProcess() override {};

public:
	JetOrHeliParkOrientState( StateMachine *machine ) : State( machine, "JetOrHeliParkOrientState") { }

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

		if (jet->isKindOf(KINDOF_PRODUCED_AT_HELIPAD))
		{
			return STATE_SUCCESS;
		}

		jetAI->friend_setTakeoffInProgress(false);
		jetAI->friend_setLandingInProgress(true);

		jetAI->ignoreObstacleID(jet->getProducerID());
		return STATE_CONTINUE;
	}

	virtual StateReturnType update() override
	{
		Object* jet = getMachineOwner();
		if (jet->isEffectivelyDead())
			return STATE_FAILURE;

		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
		{
			return STATE_FAILURE;
		}

		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
		if (pp == nullptr)
			return STATE_FAILURE;

		ParkingPlaceBehaviorInterface::PPInfo ppinfo;
		if (!pp->reserveSpace(jet->getID(), jetAI->friend_getParkingOffset(), &ppinfo))
			return STATE_FAILURE;

		const Real THRESH = 0.001f;
		if (fabs(stdAngleDiff(jet->getOrientation(), ppinfo.parkingOrientation)) <= THRESH)
			return STATE_SUCCESS;

		// magically position it correctly.
		jet->getPhysics()->scrubVelocity2D(0);
		Coord3D hoverloc = ppinfo.parkingSpace;
		hoverloc.z = jet->getPosition()->z;
		jet->setPosition(&hoverloc);

		jetAI->setLocomotorGoalOrientation(ppinfo.parkingOrientation);

		return STATE_CONTINUE;
	}

	virtual void onExit( StateExitType status ) override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return;

		jetAI->friend_setTakeoffInProgress(false);
		jetAI->friend_setLandingInProgress(false);
		jetAI->ignoreObstacleID(INVALID_ID);
	}
};
EMPTY_DTOR(JetOrHeliParkOrientState)

//-------------------------------------------------------------------------------------------------
class JetPauseBeforeTakeoffState : public AIFaceState
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetPauseBeforeTakeoffState, "JetPauseBeforeTakeoffState")
protected:
	// snapshot interface
	virtual void crc( Xfer *xfer ) override
	{
		// empty. jba.
	}

	virtual void xfer( Xfer *xfer ) override
	{
		// version
#if RETAIL_COMPATIBLE_CRC || RETAIL_COMPATIBLE_XFER_SAVE
		XferVersion currentVersion = 1;
#else
		XferVersion currentVersion = 2;
#endif
		XferVersion version = currentVersion;
		xfer->xferVersion( &version, currentVersion );

		// set on create. xfer->xferBool(&m_landing);
		xfer->xferUnsignedInt(&m_whenTakeoff);
		xfer->xferUnsignedInt(&m_whenTransfer);

#if RETAIL_COMPATIBLE_CRC || RETAIL_COMPATIBLE_XFER_SAVE
		if (version <= 1)
		{
			xfer->xferBool(&m_afterburners);
			xfer->xferBool(&m_resetTimer);
		}
#else
		if (version <= 1)
		{
			Bool afterburners = false;
			Bool resetTimer = false;
			xfer->xferBool(&afterburners);
			xfer->xferBool(&resetTimer);
		}
#endif

		xfer->xferObjectID(&m_waitedForTaxiID);
	}

	virtual void loadPostProcess() override
	{
		// empty. jba.
	}

private:
	UnsignedInt		m_whenTakeoff;
	UnsignedInt		m_whenTransfer;
	ObjectID			m_waitedForTaxiID;
#if RETAIL_COMPATIBLE_CRC || RETAIL_COMPATIBLE_XFER_SAVE
	Bool					m_resetTimer;
	Bool					m_afterburners;
#endif

#if RETAIL_COMPATIBLE_CRC || RETAIL_COMPATIBLE_XFER_SAVE
	Bool findWaiter()
	{
		Object* jet = getMachineOwner();
		ParkingPlaceBehaviorInterface* pp = getPP(getMachineOwner()->getProducerID());
		if (pp)
		{
			Int count = pp->getRunwayCount();
			for (Int i = 0; i < count; ++i)
			{
				Object* otherJet = TheGameLogic->findObjectByID(pp->getRunwayReservation(i));
				if (otherJet == nullptr || otherJet == jet)
					continue;

				AIUpdateInterface* ai = otherJet->getAIUpdateInterface();
				if (ai == nullptr)
					continue;

				if (ai->getCurrentStateID() == TAXI_TO_TAKEOFF)
				{
					if (m_waitedForTaxiID == INVALID_ID)
					{
						m_waitedForTaxiID = otherJet->getID();
					}
					return true;
				}
			}
		}
		return false;
	}
#else
	Object* findJetToWaitFor() const
	{
		const Object* thisJet = getMachineOwner();
		ParkingPlaceBehaviorInterface* pp = getPP(getMachineOwner()->getProducerID());

		if (pp != nullptr)
		{
			const Int thisJetRunway = pp->getRunwayIndex(thisJet->getID());
			const Int runwayCount = pp->getRunwayCount();

			for (Int runway = 0; runway < runwayCount; ++runway)
			{
				if (runway == thisJetRunway)
					continue;

				Object* otherJet = TheGameLogic->findObjectByID(pp->getRunwayReservation(runway));
				if (otherJet == nullptr)
					continue;

				AIUpdateInterface* ai = otherJet->getAIUpdateInterface();
				if (ai == nullptr)
					continue;

				if (ai->getCurrentStateID() != TAXI_TO_TAKEOFF)
					continue;

				return otherJet;
			}
		}
		return nullptr;
	}
#endif

public:
	JetPauseBeforeTakeoffState( StateMachine *machine )
		: AIFaceState(machine, false)
		, m_whenTakeoff(0)
		, m_whenTransfer(0)
		, m_waitedForTaxiID(INVALID_ID)
#if RETAIL_COMPATIBLE_CRC || RETAIL_COMPATIBLE_XFER_SAVE
		, m_resetTimer(false)
		, m_afterburners(false)
#endif
	{
		// nothing
	}

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

		jetAI->friend_setTakeoffInProgress(true);
		jetAI->friend_setLandingInProgress(false);

		m_whenTakeoff = 0;
		m_whenTransfer = 0;
		m_waitedForTaxiID = INVALID_ID;
#if RETAIL_COMPATIBLE_CRC || RETAIL_COMPATIBLE_XFER_SAVE
		m_resetTimer = false;
		m_afterburners = false;
#endif

		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
		if (pp == nullptr)
			return STATE_SUCCESS;	// no airfield? just skip this step.

		ParkingPlaceBehaviorInterface::PPInfo ppinfo;
		if (!pp->reserveSpace(jet->getID(), jetAI->friend_getParkingOffset(), &ppinfo))
			return STATE_SUCCESS;	// full?

		getMachine()->setGoalPosition(&ppinfo.runwayEnd);

		return AIFaceState::onEnter();
	}

#if RETAIL_COMPATIBLE_CRC || RETAIL_COMPATIBLE_XFER_SAVE
	virtual StateReturnType update() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if (jet->isEffectivelyDead())
			return STATE_FAILURE;

		// always call this.
		StateReturnType superStatus = AIFaceState::update();

		if (findWaiter())
			return STATE_CONTINUE;

		UnsignedInt now = TheGameLogic->getFrame();
		if (!m_resetTimer)
		{
			// we had to wait, but now everyone else is ready, so restart our countdown.
			m_whenTakeoff = now + jetAI->friend_getTakeoffPause();
			if (m_waitedForTaxiID == INVALID_ID)
			{
				m_waitedForTaxiID = jet->getID();	// just so we don't pick up anyone else
				m_whenTransfer = now + 1;
			}
			else
			{
				m_whenTransfer = now + 2;	// 2 seems odd, but is correct
			}
			m_resetTimer = true;
		}

		if (!m_afterburners)
		{
			jetAI->friend_enableAfterburners(true);
			m_afterburners = true;
		}

		DEBUG_ASSERTCRASH(m_whenTakeoff != 0, ("hmm"));
		DEBUG_ASSERTCRASH(m_whenTransfer != 0, ("hmm"));

			// once we start the final wait, release the runways for guys behind us, so they can start taxiing
		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
		if (pp && now >= m_whenTransfer)
		{
			pp->transferRunwayReservationToNextInLineForTakeoff(jet->getID());
		}

		if (now >= m_whenTakeoff)
			return superStatus;

		return STATE_CONTINUE;
	}
#else
	// TheSuperHackers @bugfix Reimplements the update to wait for another Jet on another runway.
	// If this must work with more than 2 runways, then this logic needs another look.
	virtual StateReturnType update() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();

		if (jet->isEffectivelyDead())
			return STATE_FAILURE;

		// always call this.
		StateReturnType superStatus = AIFaceState::update();

		if (Object* otherJet = findJetToWaitFor())
		{
			// Found a jet on another runway to wait for.
			if (m_waitedForTaxiID == INVALID_ID)
			{
				// Save the other Jet to wait for.
				m_waitedForTaxiID = otherJet->getID();
			}

			if (m_waitedForTaxiID == otherJet->getID())
			{
				// Wait for the other Jet to get ready.
				return STATE_CONTINUE;
			}
		}

		const UnsignedInt now = TheGameLogic->getFrame();

		if (m_whenTakeoff == 0)
		{
			// The other Jet (if any) is ready. Prepare runway transfer and takeoff.
			// Transfer the runway after one or two frames earliest to give the other
			// Jet a chance to update as well before the runway is transferred.
			if (m_waitedForTaxiID == INVALID_ID)
			{
				// Do not wait for any other Jet from now on.
				m_waitedForTaxiID = jet->getID();
				m_whenTransfer = now + 1;
			}
			else
			{
				m_whenTransfer = now + 2; // 2 seems odd, but is correct
			}

			// Take off soon, but not before the runway transfer.
			m_whenTakeoff = std::max(m_whenTransfer, now + jetAI->friend_getTakeoffPause());
			jetAI->friend_enableAfterburners(true);
		}
		else
		{
			// once we start the final wait, release the runways for guys behind us, so they can start taxiing
			if (now >= m_whenTransfer)
			{
				if (ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID()))
				{
					pp->transferRunwayReservationToNextInLineForTakeoff(jet->getID());
				}
				// Do not transfer the runway again in the next update.
				m_whenTransfer = ~0u;
			}

			if (now >= m_whenTakeoff)
			{
				// Ready to take off!
				return superStatus;
			}
		}

		return STATE_CONTINUE;
	}
#endif

	virtual void onExit(StateExitType status) override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		jetAI->friend_setTakeoffInProgress(false);
		jetAI->friend_setLandingInProgress(false);
		AIFaceState::onExit(status);
	}

};
EMPTY_DTOR(JetPauseBeforeTakeoffState)

//-------------------------------------------------------------------------------------------------
class JetOrHeliReloadAmmoState : public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetOrHeliReloadAmmoState, "JetOrHeliReloadAmmoState")
private:
	UnsignedInt		m_reloadTime;
	UnsignedInt		m_reloadDoneFrame;

protected:

	// snapshot interface
	virtual void crc( Xfer *xfer ) override
	{
		// empty. jba.
	}

	virtual void xfer( Xfer *xfer ) override
	{
		// version
		XferVersion currentVersion = 1;
		XferVersion version = currentVersion;
		xfer->xferVersion( &version, currentVersion );

		// set on create. xfer->xferBool(&m_landing);
		xfer->xferUnsignedInt(&m_reloadTime);
		xfer->xferUnsignedInt(&m_reloadDoneFrame);
	}
	virtual void loadPostProcess() override
	{
		// empty. jba.
	}

public:
	JetOrHeliReloadAmmoState( StateMachine *machine ) : State( machine, "JetOrHeliReloadAmmoState") { }

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		if( !jetAI )
			return STATE_FAILURE;

		jetAI->friend_setTakeoffInProgress(false);
		jetAI->friend_setLandingInProgress(false);
		jetAI->friend_setUseSpecialReturnLoco(false);

		m_reloadTime = 0;
		for (Int i = 0; i < WEAPONSLOT_COUNT;	++i)
		{
			const Weapon* w = jet->getWeaponInWeaponSlot((WeaponSlotType)i);
			if (w == nullptr)
				continue;

			Int remaining = w->getRemainingAmmo();
			Int clipSize = w->getClipSize();
			UnsignedInt rt = w->getClipReloadTime(jet);
			if (clipSize > 0)
			{
				// bias by amount empty.
				Int needed = clipSize - remaining;
				rt = (rt * needed) / clipSize;
			}
			if (rt > m_reloadTime)
				m_reloadTime = rt;
		}

		if (m_reloadTime < 1)
			m_reloadTime = 1;
		m_reloadDoneFrame = m_reloadTime + TheGameLogic->getFrame();
		return STATE_CONTINUE;
	}

	virtual StateReturnType update() override
	{
		Object* jet = getMachineOwner();
		UnsignedInt now = TheGameLogic->getFrame();
		Bool allDone = true;
		for (Int i = 0; i < WEAPONSLOT_COUNT;	++i)
		{
			Weapon* w = jet->getWeaponInWeaponSlot((WeaponSlotType)i);
			if (w == nullptr)
				continue;

			if (now >= m_reloadDoneFrame)
				w->setClipPercentFull(1.0f, false);
			else
				w->setClipPercentFull((Real)(m_reloadTime - (m_reloadDoneFrame - now)) / m_reloadTime, false);

			if (w->getRemainingAmmo() != w->getClipSize())
				allDone = false;
		}

		if (allDone)
			return STATE_SUCCESS;

		return STATE_CONTINUE;
	}

	virtual void onExit(StateExitType status) override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();
		jetAI->friend_setTakeoffInProgress(false);
		jetAI->friend_setLandingInProgress(false);
	}

};
EMPTY_DTOR(JetOrHeliReloadAmmoState)

//-------------------------------------------------------------------------------------------------
/*
	Success: we are close enough to a friendly airfield to land
	Failure: we are unable to get close enough to a friendly airfield to land
*/
class JetOrHeliReturnForLandingState : public AIInternalMoveToState
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JetOrHeliReturnForLandingState, "JetOrHeliReturnForLandingState")
public:
	JetOrHeliReturnForLandingState( StateMachine *machine ) : AIInternalMoveToState( machine, "JetOrHeliReturnForLandingState") { }

	virtual StateReturnType onEnter() override
	{
		Object* jet = getMachineOwner();
		JetAIUpdate* jetAI = (JetAIUpdate*)jet->getAIUpdateInterface();

		ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
		if (pp == nullptr)
		{
			// nuke the producer id, since it's dead
			jet->setProducer(nullptr);

			Object* airfield = findSuitableAirfield( jet );
			pp = airfield ? getPP(airfield->getID()) : nullptr;
			if (airfield && pp)
			{
				jet->setProducer(airfield);
			}
			else
			{
				return STATE_FAILURE;
			}
		}

		if (jet->isKindOf(KINDOF_PRODUCED_AT_HELIPAD))
		{
			m_goalPosition = jetAI->friend_getLandingPosForHelipadStuff();
		}
		else
		{
			ParkingPlaceBehaviorInterface::PPInfo ppinfo;
			if (!pp->reserveSpace(jet->getID(), jetAI->friend_getParkingOffset(), &ppinfo))
				return STATE_FAILURE;

			m_goalPosition = jetAI->friend_needsRunway() ? ppinfo.runwayApproach : ppinfo.parkingSpace;
		}
		setAdjustsDestination(false);		// precision is necessary

		return AIInternalMoveToState::onEnter();
	}
};
EMPTY_DTOR(JetOrHeliReturnForLandingState)

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
class JetAIStateMachine : public AIStateMachine
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( JetAIStateMachine, "JetAIStateMachine" );

public:
	JetAIStateMachine( Object *owner, AsciiString name );

};

//-------------------------------------------------------------------------------------------------
JetAIStateMachine::JetAIStateMachine(Object *owner, AsciiString name) : AIStateMachine(owner, name)
{
	defineState( RETURNING_FOR_LANDING, newInstance(JetOrHeliReturnForLandingState)( this ), LANDING_AWAIT_CLEARANCE, RETURN_TO_DEAD_AIRFIELD );
	defineState( TAKING_OFF_AWAIT_CLEARANCE, newInstance(JetAwaitingRunwayState)( this, false ), TAXI_TO_TAKEOFF, AI_IDLE );
	defineState( TAXI_TO_TAKEOFF, newInstance(JetOrHeliTaxiState)( this, FROM_PARKING ), PAUSE_BEFORE_TAKEOFF, AI_IDLE );
	defineState( PAUSE_BEFORE_TAKEOFF, newInstance(JetPauseBeforeTakeoffState)( this ), TAKING_OFF, AI_IDLE );
	defineState( TAKING_OFF, newInstance(JetTakeoffOrLandingState)( this, false ), AI_IDLE, AI_IDLE );
	defineState( LANDING_AWAIT_CLEARANCE, newInstance(JetAwaitingRunwayState)( this, true ), LANDING, AI_IDLE );
	defineState( LANDING, newInstance(JetTakeoffOrLandingState)( this, true ), TAXI_FROM_LANDING, AI_IDLE );
	defineState( TAXI_FROM_LANDING, newInstance(JetOrHeliTaxiState)( this, TO_PARKING ), ORIENT_FOR_PARKING_PLACE, AI_IDLE );
	defineState( TAXI_FROM_HANGAR, newInstance(JetOrHeliTaxiState)( this, FROM_HANGAR ), ORIENT_FOR_PARKING_PLACE, AI_IDLE );
	defineState( ORIENT_FOR_PARKING_PLACE, newInstance(JetOrHeliParkOrientState)( this ), RELOAD_AMMO, AI_IDLE );
	defineState( RELOAD_AMMO, newInstance(JetOrHeliReloadAmmoState)( this ), AI_IDLE, AI_IDLE );
	defineState( RETURN_TO_DEAD_AIRFIELD, newInstance(JetOrHeliReturningToDeadAirfieldState)( this ), CIRCLING_DEAD_AIRFIELD, RETURN_TO_DEAD_AIRFIELD );
	defineState( CIRCLING_DEAD_AIRFIELD, newInstance(JetOrHeliCirclingDeadAirfieldState)( this ), AI_IDLE, AI_IDLE );
}

//-------------------------------------------------------------------------------------------------
JetAIStateMachine::~JetAIStateMachine()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
class HeliAIStateMachine : public AIStateMachine
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HeliAIStateMachine, "HeliAIStateMachine" );

public:
	HeliAIStateMachine( Object *owner, AsciiString name );

};

//-------------------------------------------------------------------------------------------------
HeliAIStateMachine::HeliAIStateMachine(Object *owner, AsciiString name) : AIStateMachine(owner, name)
{
	defineState( RETURNING_FOR_LANDING, newInstance(JetOrHeliReturnForLandingState)( this ), LANDING_AWAIT_CLEARANCE, RETURN_TO_DEAD_AIRFIELD );
	defineState( TAKING_OFF_AWAIT_CLEARANCE, newInstance(SuccessState)( this ), TAKING_OFF, AI_IDLE );
	defineState( TAKING_OFF, newInstance(HeliTakeoffOrLandingState)( this, false ), AI_IDLE, AI_IDLE );
	defineState( LANDING_AWAIT_CLEARANCE, newInstance(SuccessState)( this ), ORIENT_FOR_PARKING_PLACE, AI_IDLE );
	defineState( ORIENT_FOR_PARKING_PLACE, newInstance(JetOrHeliParkOrientState)( this ), LANDING, AI_IDLE );
	defineState( LANDING, newInstance(HeliTakeoffOrLandingState)( this, true ), RELOAD_AMMO, AI_IDLE );
	defineState( RELOAD_AMMO, newInstance(JetOrHeliReloadAmmoState)( this ), AI_IDLE, AI_IDLE );
	defineState( RETURN_TO_DEAD_AIRFIELD, newInstance(JetOrHeliReturningToDeadAirfieldState)( this ), CIRCLING_DEAD_AIRFIELD, RETURN_TO_DEAD_AIRFIELD );
	defineState( CIRCLING_DEAD_AIRFIELD, newInstance(JetOrHeliCirclingDeadAirfieldState)( this ), AI_IDLE, AI_IDLE );
	defineState( TAXI_FROM_HANGAR, newInstance(JetOrHeliTaxiState)( this, FROM_HANGAR ), AI_IDLE, AI_IDLE );
}

//-------------------------------------------------------------------------------------------------
HeliAIStateMachine::~HeliAIStateMachine()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
JetAIUpdateModuleData::JetAIUpdateModuleData()
{
	m_outOfAmmoDamagePerSecond = 0;
	m_needsRunway = true;
	m_keepsParkingSpaceWhenAirborne = true;
	m_takeoffSpeedForMaxLift = 1.0f;
	m_minHeight = 0.0f;
	m_parkingOffset = 0.0f;
	m_sneakyOffsetWhenAttacking = 0.0f;
	m_takeoffPause = 0;
	m_attackingLoco = LOCOMOTORSET_NORMAL;
	m_returningLoco = LOCOMOTORSET_NORMAL;
	m_attackLocoPersistTime = 0;
	m_attackersMissPersistTime = 0;
	m_lockonTime = 0;
	m_lockonCursor.clear();
	m_lockonInitialDist = 100;
	m_lockonFreq = 0.5;
	m_lockonAngleSpin = 720;
	m_returnToBaseIdleTime = 0;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void JetAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  AIUpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "OutOfAmmoDamagePerSecond",			INI::parsePercentToReal, nullptr, offsetof( JetAIUpdateModuleData, m_outOfAmmoDamagePerSecond ) },
		{ "NeedsRunway",									INI::parseBool, nullptr, offsetof( JetAIUpdateModuleData, m_needsRunway ) },
		{ "KeepsParkingSpaceWhenAirborne",INI::parseBool, nullptr, offsetof( JetAIUpdateModuleData, m_keepsParkingSpaceWhenAirborne ) },
		{ "TakeoffSpeedForMaxLift",				INI::parsePercentToReal, nullptr, offsetof( JetAIUpdateModuleData, m_takeoffSpeedForMaxLift ) },
		{ "TakeoffPause",									INI::parseDurationUnsignedInt, nullptr, offsetof( JetAIUpdateModuleData, m_takeoffPause ) },
		{ "MinHeight",										INI::parseReal, nullptr, offsetof( JetAIUpdateModuleData, m_minHeight ) },
		{ "ParkingOffset",								INI::parseReal, nullptr, offsetof( JetAIUpdateModuleData, m_parkingOffset ) },
		{ "SneakyOffsetWhenAttacking",		INI::parseReal, nullptr, offsetof( JetAIUpdateModuleData, m_sneakyOffsetWhenAttacking ) },
		{ "AttackLocomotorType",					INI::parseIndexList, TheLocomotorSetNames, offsetof( JetAIUpdateModuleData, m_attackingLoco ) },
		{ "AttackLocomotorPersistTime",		INI::parseDurationUnsignedInt, nullptr, offsetof( JetAIUpdateModuleData, m_attackLocoPersistTime ) },
		{ "AttackersMissPersistTime",			INI::parseDurationUnsignedInt, nullptr, offsetof( JetAIUpdateModuleData, m_attackersMissPersistTime ) },
		{ "ReturnForAmmoLocomotorType",		INI::parseIndexList, TheLocomotorSetNames, offsetof( JetAIUpdateModuleData, m_returningLoco ) },
		{ "LockonTime",										INI::parseDurationUnsignedInt, nullptr, offsetof( JetAIUpdateModuleData, m_lockonTime ) },
		{ "LockonCursor",									INI::parseAsciiString, nullptr, offsetof( JetAIUpdateModuleData, m_lockonCursor ) },
		{ "LockonInitialDist",						INI::parseReal, nullptr, offsetof( JetAIUpdateModuleData, m_lockonInitialDist ) },
		{ "LockonFreq",										INI::parseReal, nullptr, offsetof( JetAIUpdateModuleData, m_lockonFreq ) },
		{ "LockonAngleSpin",							INI::parseAngleReal, nullptr, offsetof( JetAIUpdateModuleData, m_lockonAngleSpin ) },
		{ "LockonBlinky",									INI::parseBool, nullptr, offsetof( JetAIUpdateModuleData, m_lockonBlinky ) },
		{ "ReturnToBaseIdleTime",					INI::parseDurationUnsignedInt, nullptr, offsetof( JetAIUpdateModuleData, m_returnToBaseIdleTime ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
AIStateMachine* JetAIUpdate::makeStateMachine()
{
	if (getJetAIUpdateModuleData()->m_needsRunway)
		return newInstance(JetAIStateMachine)( getObject(), "JetAIStateMachine");
	else
		return newInstance(HeliAIStateMachine)( getObject(), "HeliAIStateMachine");
}

//-------------------------------------------------------------------------------------------------
JetAIUpdate::JetAIUpdate( Thing *thing, const ModuleData* moduleData ) : AIUpdateInterface( thing, moduleData )
{
	m_flags = 0;
	m_afterburnerSound = *(getObject()->getTemplate()->getPerUnitSound("Afterburner"));
	m_afterburnerSound.setObjectID(getObject()->getID());
	m_attackLocoExpireFrame = 0;
	m_attackersMissExpireFrame = 0;
	m_untargetableExpireFrame = 0;
	m_returnToBaseFrame = 0;
	m_lockonDrawable = nullptr;
	m_landingPosForHelipadStuff.zero();

	m_producerLocation.zero();
	m_enginesOn = TRUE;
}

//-------------------------------------------------------------------------------------------------
JetAIUpdate::~JetAIUpdate()
{
	if (m_lockonDrawable)
	{
		TheGameClient->destroyDrawable(m_lockonDrawable);
		m_lockonDrawable = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
Bool JetAIUpdate::isIdle() const
{
	// we need to do this because we enter an idle state briefly between takeoff/landing in these cases,
	// but scripting relies on us never claiming to be "idle"...
	if (getFlag(HAS_PENDING_COMMAND))
		return false;

	return AIUpdateInterface::isIdle();
}

//-------------------------------------------------------------------------------------------------
void JetAIUpdate::onObjectCreated()
{
	AIUpdateInterface::onObjectCreated();
	friend_setAllowAirLoco(false);
	chooseLocomotorSet(LOCOMOTORSET_TAXIING);
}

//-------------------------------------------------------------------------------------------------
void JetAIUpdate::onDelete()
{
	AIUpdateInterface::onDelete();
	ParkingPlaceBehaviorInterface* pp = getPP(getObject()->getProducerID());
	if (pp)
		pp->releaseSpace(getObject()->getID());
}

//-------------------------------------------------------------------------------------------------
void JetAIUpdate::getProducerLocation()
{
	if (getFlag(HAS_PRODUCER_LOCATION))
		return;

	Object* jet = getObject();
	Object* airfield = TheGameLogic->findObjectByID( jet->getProducerID() );
	if (airfield == nullptr)
		m_producerLocation = *jet->getPosition();
	else
		m_producerLocation = *airfield->getPosition();

	/*
		if we aren't allowed to fly, then we should be parked (or at least taxiing),
		which implies we have a parking place reserved. If we don't, it's probably
		because we were directly spawned via script (or directly placed on the map).
		So, check to see if we have no parking place, and if not, quietly enable flight.
	*/
	ParkingPlaceBehaviorInterface* pp = getPP(jet->getProducerID());
	if (!pp || !pp->hasReservedSpace(jet->getID()))
	{
		friend_setAllowAirLoco(true);
		chooseLocomotorSet(LOCOMOTORSET_NORMAL);
	}
	else
	{
		friend_setAllowAirLoco(false);
		chooseLocomotorSet(LOCOMOTORSET_TAXIING);
	}

	setFlag(HAS_PRODUCER_LOCATION, true);

}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime JetAIUpdate::update()
{
	const JetAIUpdateModuleData* d = getJetAIUpdateModuleData();

	getProducerLocation();

	Object* jet = getObject();

	ParkingPlaceBehaviorInterface* pp = getPP(getObject()->getProducerID());

	// If idle & out of ammo, return
	// have to call our parent's isIdle, because we override it to never return true
	// when we have a pending command...
	UnsignedInt now = TheGameLogic->getFrame();

	// srj sez: not 100% sure on this. calling RELOAD_AMMO "idle" allows us to get healed while reloading,
	// but might have other side effects we didn't want. if this does prove to cause a bug, be sure
	// that jets (and ESPECIALLY comanches) are still getting healed at airfields.
	if (AIUpdateInterface::isIdle() || getStateMachine()->getCurrentStateID() == RELOAD_AMMO)
	{
		if (pp != nullptr)
		{
			if (!getFlag(ALLOW_AIR_LOCO) &&
					!getFlag(HAS_PENDING_COMMAND) &&
						jet->isKindOf(KINDOF_PRODUCED_AT_HELIPAD) &&
						jet->getBodyModule()->getHealth() == jet->getBodyModule()->getMaxHealth())
			{
				// we're completely healed, so take off again
				pp->setHealee(jet, false);
				friend_setAllowAirLoco(true);
				getStateMachine()->clear();
				setLastCommandSource( CMD_FROM_AI );
				getStateMachine()->setState( TAKING_OFF_AWAIT_CLEARANCE );

#if !RETAIL_COMPATIBLE_CRC
				// TheSuperHackers @bugfix arcticdolphin 02/03/2026 Move healed helicopter to rally point if present.
				if (Object *airfield = TheGameLogic->findObjectByID( jet->getProducerID() ))
				{
					if (ExitInterface *exitInterface = airfield->getObjectExitInterface())
					{
						if (const Coord3D *rallyPoint = exitInterface->getRallyPoint())
						{
							aiMoveToPosition( rallyPoint, CMD_FROM_AI );
						}
					}
				}
#endif
			}
			else
			{
				pp->setHealee(jet, !getFlag(ALLOW_AIR_LOCO));
			}
		}

		// note that we might still have weapons with ammo, but still be forced to return to reload.
		if (isOutOfSpecialReloadAmmo(jet) && getFlag(ALLOW_AIR_LOCO))
		{
			m_returnToBaseFrame = 0;

			// this is really a "just-in-case" to ensure the targeter list doesn't spin out of control (srj)
			pruneDeadTargeters();

			setFlag(USE_SPECIAL_RETURN_LOCO, true);
			setLastCommandSource( CMD_FROM_AI );
			getStateMachine()->setState(RETURNING_FOR_LANDING);
		}
		else if (getFlag(HAS_PENDING_COMMAND)
			// srj sez: if we are reloading ammo, wait will we are done before processing the pending command.
			&& getStateMachine()->getCurrentStateID() != RELOAD_AMMO)
		{
			m_returnToBaseFrame = 0;

			AICommandParms parms(AICMD_MOVE_TO_POSITION, CMD_FROM_AI);	// values don't matter, will be wiped by next line
			m_mostRecentCommand.reconstitute(parms);
			setFlag(HAS_PENDING_COMMAND, false);

 			aiDoCommand(&parms);
		}
		else if (m_returnToBaseFrame != 0 && now >= m_returnToBaseFrame && getFlag(ALLOW_AIR_LOCO))
		{
			m_returnToBaseFrame = 0;
			DEBUG_ASSERTCRASH(isOutOfSpecialReloadAmmo(jet) == false, ("Hmm, this seems unlikely -- isOutOfSpecialReloadAmmo(jet)==false"));
			setFlag(USE_SPECIAL_RETURN_LOCO, false);
			setLastCommandSource( CMD_FROM_AI );
			getStateMachine()->setState(RETURNING_FOR_LANDING);
		}
		else if (m_returnToBaseFrame  == 0 && d->m_returnToBaseIdleTime > 0 && getFlag(ALLOW_AIR_LOCO))
		{
			m_returnToBaseFrame = now + d->m_returnToBaseIdleTime;
		}
	}
	else
	{
		if (pp != nullptr)
		{
			pp->setHealee(getObject(), false);
		}
		m_returnToBaseFrame = 0;
		if (getFlag(ALLOW_INTERRUPT_AND_RESUME_OF_CUR_STATE_FOR_RELOAD) &&
						isOutOfSpecialReloadAmmo(jet) && getFlag(ALLOW_AIR_LOCO))
		{
			setFlag(USE_SPECIAL_RETURN_LOCO, true);
			setFlag(HAS_PENDING_COMMAND, true);
			setFlag(ALLOW_INTERRUPT_AND_RESUME_OF_CUR_STATE_FOR_RELOAD, false);
			setLastCommandSource( CMD_FROM_AI );
			getStateMachine()->setState(RETURNING_FOR_LANDING);
		}
	}

	Real minHeight = friend_getMinHeight();
	Drawable* draw = jet->getDrawable();
	if (draw != nullptr)
	{
		StateID id = getStateMachine()->getCurrentStateID();
		Bool needToCheckMinHeight = (id >= JETAISTATETYPE_FIRST && id <= JETAISTATETYPE_LAST) ||
																	!jet->isAboveTerrain() ||
																	!getFlag(ALLOW_AIR_LOCO);
		if (needToCheckMinHeight)
		{
			Real ht = jet->isAboveTerrain() ? jet->getHeightAboveTerrain() : 0;
			if (ht < minHeight)
			{
				Matrix3D tmp(1);
				tmp.Set_Z_Translation(minHeight - ht);
				draw->setInstanceMatrix(&tmp);
			}
			else
			{
				draw->setInstanceMatrix(nullptr);
			}
		}
		else
		{
			draw->setInstanceMatrix(nullptr);
		}
	}

	PhysicsBehavior* physics = jet->getPhysics();
	if (physics->getVelocityMagnitude() > 0 && getFlag(ALLOW_AIR_LOCO))
		jet->setModelConditionState(MODELCONDITION_JETEXHAUST);
	else
		jet->clearModelConditionState(MODELCONDITION_JETEXHAUST);

	if (jet->testStatus(OBJECT_STATUS_IS_ATTACKING))
	{
		m_attackLocoExpireFrame = now + d->m_attackLocoPersistTime;
		m_attackersMissExpireFrame = now + d->m_attackersMissPersistTime;
	}
	else
	{
		if (m_attackLocoExpireFrame != 0 && now >= m_attackLocoExpireFrame)
		{
			m_attackLocoExpireFrame = 0;
#if !RETAIL_COMPATIBLE_CRC
			// TheSuperHackers @bugfix arcticdolphin 04/03/2026 Reset locomotor when attack locomotor timer expires.
			// Relevant for the Supersonic Mode of the USA Aurora.
			chooseLocomotorSet(LOCOMOTORSET_NORMAL);
#endif
		}
		if (m_attackersMissExpireFrame != 0 && now >= m_attackersMissExpireFrame)
		{
			m_attackersMissExpireFrame = 0;
		}
	}

	if (m_untargetableExpireFrame != 0 && now >= m_untargetableExpireFrame)
	{
		m_untargetableExpireFrame = 0;
	}

	positionLockon();

	if (m_attackLocoExpireFrame != 0)
	{
		chooseLocomotorSet(d->m_attackingLoco);
	}
	else if (getFlag(USE_SPECIAL_RETURN_LOCO))
	{
		chooseLocomotorSet(d->m_returningLoco);
	}


	if( !jet->isKindOf( KINDOF_PRODUCED_AT_HELIPAD ) )
	{
		Drawable *draw = jet->getDrawable();
		if( draw )
		{
			if( getFlag(TAKEOFF_IN_PROGRESS)
					|| getFlag(LANDING_IN_PROGRESS)
					|| getObject()->isSignificantlyAboveTerrain()
					|| isMoving()
					|| isWaitingForPath() )
			{
				if( !m_enginesOn )
				{
					//We just started moving, therefore turn on the engines!
					draw->enableAmbientSound( TRUE );
					m_enginesOn = TRUE;
				}
			}
			else if( m_enginesOn )
			{
				//We're no longer moving, so turn off the engines!
				draw->enableAmbientSound( FALSE );
				m_enginesOn = FALSE;
			}
		}
	}


	/*UpdateSleepTime ret =*/ AIUpdateInterface::update();
	//return (mine < ret) ? mine : ret;
	/// @todo srj -- someday, make sleepy. for now, must not sleep.
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
Bool JetAIUpdate::chooseLocomotorSet(LocomotorSetType wst)
{
	const JetAIUpdateModuleData* d = getJetAIUpdateModuleData();
	if (!getFlag(ALLOW_AIR_LOCO))
	{
		wst = LOCOMOTORSET_TAXIING;
	}
	else if (m_attackLocoExpireFrame != 0)
	{
		wst = d->m_attackingLoco;
	}
	else if (getFlag(USE_SPECIAL_RETURN_LOCO))
	{
		wst = d->m_returningLoco;
	}
	return AIUpdateInterface::chooseLocomotorSet(wst);
}

//-------------------------------------------------------------------------------------------------
void JetAIUpdate::setLocomotorGoalNone()
{
	if ((getFlag(TAKEOFF_IN_PROGRESS) || getFlag(LANDING_IN_PROGRESS))
			&& getFlag(ALLOW_AIR_LOCO) && !getFlag(ALLOW_CIRCLING))
	{
		Object* jet = getObject();
		Coord3D desiredPos = *jet->getPosition();
		const Coord3D* dir = jet->getUnitDirectionVector2D();
		desiredPos.x += dir->x * 1000.0f;
		desiredPos.y += dir->y * 1000.0f;
		setLocomotorGoalPositionExplicit(desiredPos);
	}
	else
	{
		AIUpdateInterface::setLocomotorGoalNone();
	}
}

//----------------------------------------------------------------------------------------
Bool JetAIUpdate::getSneakyTargetingOffset(Coord3D* offset) const
{
	if (m_attackersMissExpireFrame != 0 && TheGameLogic->getFrame() < m_attackersMissExpireFrame)
	{
		if (offset)
		{
			const JetAIUpdateModuleData* d = getJetAIUpdateModuleData();
			const Object* jet = getObject();
			const Coord3D* dir = jet->getUnitDirectionVector2D();
			offset->x = dir->x * d->m_sneakyOffsetWhenAttacking;
			offset->y = dir->y * d->m_sneakyOffsetWhenAttacking;
			offset->z = 0.0f;
		}
		return true;
	}
	else
	{
		return false;
	}
}

//----------------------------------------------------------------------------------------
void JetAIUpdate::pruneDeadTargeters()
{
	if (!m_targetedBy.empty())
	{
		for (std::list<ObjectID>::iterator it = m_targetedBy.begin(); it != m_targetedBy.end(); /* empty */ )
		{
			if (TheGameLogic->findObjectByID(*it) == nullptr)
			{
				it = m_targetedBy.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
}

//----------------------------------------------------------------------------------------
void JetAIUpdate::positionLockon()
{
	if (!m_lockonDrawable)
		return;

	if (m_untargetableExpireFrame == 0)
	{
		TheGameClient->destroyDrawable(m_lockonDrawable);
		m_lockonDrawable = nullptr;
		return;
	}

	const JetAIUpdateModuleData* d = getJetAIUpdateModuleData();
	UnsignedInt now = TheGameLogic->getFrame();
	UnsignedInt remaining = m_untargetableExpireFrame - now;
	UnsignedInt elapsed = d->m_lockonTime - remaining;

	Coord3D pos = *getObject()->getPosition();
	Real frac = (Real)remaining / (Real)d->m_lockonTime;
	Real finalDist = getObject()->getGeometryInfo().getBoundingCircleRadius();
	Real dist = finalDist + (d->m_lockonInitialDist - finalDist) * frac;
	Real angle = d->m_lockonAngleSpin * frac;

	pos.x += Cos(angle) * dist;
	pos.y += Sin(angle) * dist;
	// pos.z is untouched

	m_lockonDrawable->setPosition(&pos);
	Real dx = getObject()->getPosition()->x - pos.x;
	Real dy = getObject()->getPosition()->y - pos.y;
	if (dx || dy)
		m_lockonDrawable->setOrientation(WWMath::Atan2Origin(dy, dx));

	// the Gaussian sum, to avoid keeping a running total:
	//
	//		1+2+3+...n = n*(n+1)/2
	//
	Real elapsedTimeSumPrev = 0.5f * (elapsed-1) * (elapsed);
	Real elapsedTimeSumCurr = elapsedTimeSumPrev + elapsed;
	Real factor = d->m_lockonFreq / d->m_lockonTime;
	Bool lastPhase = ((Int)(factor * elapsedTimeSumPrev) & 1) != 0;
	Bool thisPhase = ((Int)(factor * elapsedTimeSumCurr) & 1) != 0;

	if (lastPhase && (!thisPhase))
	{
		AudioEventRTS lockonSound = TheAudio->getMiscAudio()->m_lockonTickSound;
		lockonSound.setObjectID(getObject()->getID());
		TheAudio->addAudioEvent(&lockonSound);
		if (d->m_lockonBlinky)
			m_lockonDrawable->setDrawableHidden(false);
	}
	else
	{
		if (d->m_lockonBlinky)
			m_lockonDrawable->setDrawableHidden(true);
	}
}

//----------------------------------------------------------------------------------------
void JetAIUpdate::buildLockonDrawableIfNecessary()
{
	if (m_untargetableExpireFrame == 0)
		return;

	const JetAIUpdateModuleData* d = getJetAIUpdateModuleData();
	if (d->m_lockonCursor.isNotEmpty() && m_lockonDrawable == nullptr)
	{
		const ThingTemplate* tt = TheThingFactory->findTemplate(d->m_lockonCursor);
		if (tt)
		{
			m_lockonDrawable = TheThingFactory->newDrawable(tt);
		}
	}
	positionLockon();
}

//----------------------------------------------------------------------------------------
void JetAIUpdate::addTargeter(ObjectID id, Bool add)
{
	const JetAIUpdateModuleData* d = getJetAIUpdateModuleData();
	UnsignedInt lockonTime = d->m_lockonTime;
	if (lockonTime != 0)
	{
		std::list<ObjectID>::iterator it = std::find(m_targetedBy.begin(), m_targetedBy.end(), id);
		if (add)
		{
			if (it == m_targetedBy.end())
			{
				m_targetedBy.push_back(id);
				if (m_untargetableExpireFrame == 0 && m_targetedBy.size() == 1)
				{
					m_untargetableExpireFrame = TheGameLogic->getFrame() + lockonTime;
					buildLockonDrawableIfNecessary();
				}
			}
		}
		else
		{
			if (it != m_targetedBy.end())
			{
				m_targetedBy.erase(it);
				if (m_targetedBy.empty())
				{
					m_untargetableExpireFrame = 0;
				}
			}
		}
	}
}

//----------------------------------------------------------------------------------------
Bool JetAIUpdate::isTemporarilyPreventingAimSuccess() const
{
	return m_untargetableExpireFrame != 0 && (TheGameLogic->getFrame() < m_untargetableExpireFrame);
}

//----------------------------------------------------------------------------------------
Bool JetAIUpdate::isAllowedToMoveAwayFromUnit() const
{
	// parked (or landing) units don't get to do this.
	if (!getFlag(ALLOW_AIR_LOCO) || getFlag(TAKEOFF_IN_PROGRESS) || getFlag(LANDING_IN_PROGRESS))
		return false;

	return AIUpdateInterface::isAllowedToMoveAwayFromUnit();
}

//-------------------------------------------------------------------------------------------------
Bool JetAIUpdate::isDoingGroundMovement() const
{
	// srj per jba: Air units should never be doing ground movement, even when taxiing...
	// (exception: see getTreatAsAircraftForLocoDistToGoal)
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool JetAIUpdate::getTreatAsAircraftForLocoDistToGoal() const
{
	// exception to isDoingGroundMovement: should never treat as aircraft for dist-to-goal when taxiing.
	if (getFlag(TAXI_IN_PROGRESS))
	{
		return false;
	}
	else
	{
		return AIUpdateInterface::getTreatAsAircraftForLocoDistToGoal();
	}
}

//----------------------------------------------------------------------------------------
/**
 * Follow the path defined by the given array of points
 */
void JetAIUpdate::privateFollowPath( std::vector<Coord3D>* path, Object *ignoreObject, CommandSourceType cmdSource, Bool exitProduction )
{
	if (exitProduction)
	{
		getStateMachine()->clear();
		if( ignoreObject )
			ignoreObstacle( ignoreObject );
		setLastCommandSource( cmdSource );
		if (getObject()->isKindOf(KINDOF_PRODUCED_AT_HELIPAD))
			getStateMachine()->setState( TAKING_OFF_AWAIT_CLEARANCE );
		else
			getStateMachine()->setState( TAXI_FROM_HANGAR );
	}
	else
	{
		AIUpdateInterface::privateFollowPath(path, ignoreObject, cmdSource, exitProduction);
	}
}

//----------------------------------------------------------------------------------------
void JetAIUpdate::privateFollowPathAppend( const Coord3D *pos, CommandSourceType cmdSource )
{
	// nothing yet... might need to override. not sure. (srj)
	AIUpdateInterface::privateFollowPathAppend(pos, cmdSource);
}

//----------------------------------------------------------------------------------------
void JetAIUpdate::doLandingCommand(Object *airfield, CommandSourceType cmdSource)
{
	if (getObject()->isKindOf(KINDOF_PRODUCED_AT_HELIPAD))
	{
		m_landingPosForHelipadStuff = *airfield->getPosition();

		Coord3D tmp;
		FindPositionOptions options;
		options.maxRadius = airfield->getGeometryInfo().getBoundingCircleRadius() * 10.0f;
		if (ThePartitionManager->findPositionAround(&m_landingPosForHelipadStuff, &options, &tmp))
			m_landingPosForHelipadStuff = tmp;
	}

	for (BehaviorModule** i = airfield->getBehaviorModules(); *i; ++i)
	{
		ParkingPlaceBehaviorInterface* pp = (*i)->getParkingPlaceBehaviorInterface();
		if (pp == nullptr)
			continue;

		if (getObject()->isKindOf(KINDOF_PRODUCED_AT_HELIPAD) ||
				pp->reserveSpace(getObject()->getID(), friend_getParkingOffset(), nullptr))
		{
			// if we had a space at another airfield, release it
			ParkingPlaceBehaviorInterface* oldPP = getPP(getObject()->getProducerID());
			if (oldPP != nullptr && oldPP != pp)
			{
				oldPP->releaseSpace(getObject()->getID());
			}

			getObject()->setProducer(airfield);
			DEBUG_ASSERTCRASH(isOutOfSpecialReloadAmmo(getObject()) == false, ("Hmm, this seems unlikely -- isOutOfSpecialReloadAmmo(jet)==false"));
			setFlag(USE_SPECIAL_RETURN_LOCO, false);
			setFlag(ALLOW_INTERRUPT_AND_RESUME_OF_CUR_STATE_FOR_RELOAD, false);
			setLastCommandSource( cmdSource );
			getStateMachine()->setState(RETURNING_FOR_LANDING);
			return;
		}
	}
}

//----------------------------------------------------------------------------------------
void JetAIUpdate::notifyVictimIsDead()
{
	if (getJetAIUpdateModuleData()->m_needsRunway)
		m_returnToBaseFrame = TheGameLogic->getFrame();
}

//----------------------------------------------------------------------------------------
/**
 * Enter the given object
 */
void JetAIUpdate::privateEnter( Object *objectToEnter, CommandSourceType cmdSource )
{
	// we are already landing. just ignore it.
	if (getFlag(LANDING_IN_PROGRESS))
		return;

	if( !TheActionManager->canEnterObject( getObject(), objectToEnter, cmdSource, DONT_CHECK_CAPACITY ) )
		return;

	doLandingCommand(objectToEnter, cmdSource);
}

//----------------------------------------------------------------------------------------
/**
 * Get repaired at the repair depot
 */
void JetAIUpdate::privateGetRepaired( Object *repairDepot, CommandSourceType cmdSource )
{
	// we are already landing. just ignore it.
	if (getFlag(LANDING_IN_PROGRESS))
		return;

	// sanity, if we can't get repaired from here get out of here
	if( TheActionManager->canGetRepairedAt( getObject(), repairDepot, cmdSource ) == FALSE )
		return;

	// dock with the repair depot
	doLandingCommand( repairDepot, cmdSource );

}

//-------------------------------------------------------------------------------------------------
Bool JetAIUpdate::isParkedAt(const Object* obj) const
{
	if (!getFlag(ALLOW_AIR_LOCO) &&
			!getObject()->isKindOf(KINDOF_PRODUCED_AT_HELIPAD) &&
			obj != nullptr)
	{
		Object* airfield;
		ParkingPlaceBehaviorInterface* pp = getPP(getObject()->getProducerID(), &airfield);
		if (pp != nullptr && airfield != nullptr && airfield == obj)
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
void JetAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	// call this from aiDoCommand as well as update, because this can
	// be called before update ever is... if the unit is placed on a map,
	// and a script tells it to do something with a condition of TRUE!
	getProducerLocation();

	if (!isAllowedToRespondToAiCommands(parms))
		return;

	// note that we always store this, even if nothing will be "pending".
	m_mostRecentCommand.store(*parms);

	if (getFlag(TAKEOFF_IN_PROGRESS) || getFlag(LANDING_IN_PROGRESS))
	{
		// have to wait for takeoff or landing to complete, just store the sucker
		setFlag(HAS_PENDING_COMMAND, true);
		return;
	}
	else if (parms->m_cmd == AICMD_IDLE && getStateMachine()->getCurrentStateID() == RELOAD_AMMO)
	{
		// uber-special-case... if we are told to idle, but are reloading ammo, ignore it for now,
		// since we're already doing "nothing" and responding to this will cease our reload...
		// don't just return, tho, in case we were (say) reloading during a guard stint.
		setFlag(HAS_PENDING_COMMAND, true);
		return;
	}
	else if (!getFlag(ALLOW_AIR_LOCO))
	{
		switch (parms->m_cmd)
		{
			case AICMD_IDLE:
			case AICMD_BUSY:
			case AICMD_FOLLOW_EXITPRODUCTION_PATH:
				// don't need (or want) to take off for these
				break;

#if !RETAIL_COMPATIBLE_CRC
			// TheSuperHackers @tweak Stubbjax 23/01/2026 Parked jets now defer offensive commands when out of ammo.
			case AICMD_ATTACKMOVE_TO_POSITION:
			case AICMD_ATTACK_AREA:
			case AICMD_ATTACK_OBJECT:
			case AICMD_ATTACK_POSITION:
			case AICMD_ATTACK_TEAM:
			case AICMD_FORCE_ATTACK_OBJECT:
			case AICMD_GUARD_AREA:
			case AICMD_GUARD_OBJECT:
			case AICMD_GUARD_POSITION:
			case AICMD_HUNT:
				if (isOutOfSpecialReloadAmmo(getObject()))
				{
					setFlag(HAS_PENDING_COMMAND, true);
					return;
				}

				FALLTHROUGH;
#endif

			case AICMD_ENTER:
			case AICMD_GET_REPAIRED:

				// if we're already parked at the airfield in question, just ignore.
				if (isParkedAt(parms->m_obj))
					return;

				FALLTHROUGH; // else fall thru to the default case!

			default:
			{
				// nuke any existing pending cmd
				m_mostRecentCommand.store(*parms);
				setFlag(HAS_PENDING_COMMAND, true);

				getStateMachine()->clear();
				setLastCommandSource( CMD_FROM_AI );
				getStateMachine()->setState( TAKING_OFF_AWAIT_CLEARANCE );

				return;
			}
		}
	}

	switch (parms->m_cmd)
	{
		case AICMD_GUARD_POSITION:
		case AICMD_GUARD_OBJECT:
		case AICMD_GUARD_AREA:
		case AICMD_HUNT:
			setFlag(ALLOW_INTERRUPT_AND_RESUME_OF_CUR_STATE_FOR_RELOAD, true);
			break;
		default:
			setFlag(ALLOW_INTERRUPT_AND_RESUME_OF_CUR_STATE_FOR_RELOAD, false);
			break;
	}

	setFlag(HAS_PENDING_COMMAND, false);
	AIUpdateInterface::aiDoCommand(parms);
}

//-------------------------------------------------------------------------------------------------
void JetAIUpdate::friend_setAllowAirLoco(Bool allowAirLoco)
{
	setFlag(ALLOW_AIR_LOCO, allowAirLoco);
}

//-------------------------------------------------------------------------------------------------
void JetAIUpdate::friend_enableAfterburners(Bool v)
{
	Object* jet = getObject();
	if (v)
	{
		jet->setModelConditionState(MODELCONDITION_JETAFTERBURNER);
		if (!m_afterburnerSound.isCurrentlyPlaying())
		{
			m_afterburnerSound.setObjectID(jet->getID());
			m_afterburnerSound.setPlayingHandle(TheAudio->addAudioEvent(&m_afterburnerSound));
		}
	}
	else
	{
		jet->clearModelConditionState(MODELCONDITION_JETAFTERBURNER);
		if (m_afterburnerSound.isCurrentlyPlaying())
		{
			TheAudio->removeAudioEvent(m_afterburnerSound.getPlayingHandle());
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void JetAIUpdate::crc( Xfer *xfer )
{
	// extend base class
	AIUpdateInterface::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void JetAIUpdate::xfer( Xfer *xfer )
{

  // version
  XferVersion currentVersion = 2;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );

 // extend base class
	AIUpdateInterface::xfer(xfer);


	xfer->xferCoord3D(&m_producerLocation);
	m_mostRecentCommand.doXfer(xfer);
	xfer->xferUnsignedInt(&m_attackLocoExpireFrame);
	xfer->xferUnsignedInt(&m_attackersMissExpireFrame);
	xfer->xferUnsignedInt(&m_returnToBaseFrame);
	xfer->xferSTLObjectIDList(&m_targetedBy);

	xfer->xferUnsignedInt(&m_untargetableExpireFrame);

	// Set on create.
	//AudioEventRTS						m_afterburnerSound;		///< Sound when afterburners on

	AsciiString drawName;
	if (m_lockonDrawable) {
		drawName = m_lockonDrawable->getTemplate()->getName();
	}
	xfer->xferAsciiString(&drawName);
	if (drawName.isNotEmpty() && m_lockonDrawable==nullptr)
	{
		const ThingTemplate* tt = TheThingFactory->findTemplate(drawName);
		if (tt)
		{
			m_lockonDrawable = TheThingFactory->newDrawable(tt);
		}
	}
	xfer->xferInt(&m_flags);

	if( version >= 2 )
	{
		xfer->xferBool( &m_enginesOn );
	}
	else
	{
		//We don't have to be accurate -- this is a patch.
		if( getFlag(TAKEOFF_IN_PROGRESS) || getFlag(LANDING_IN_PROGRESS) || getObject()->isSignificantlyAboveTerrain() || getObject()->isKindOf( KINDOF_PRODUCED_AT_HELIPAD ) )
		{
			m_enginesOn = TRUE;
		}
		else
		{
			m_enginesOn = FALSE;
		}
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void JetAIUpdate::loadPostProcess()
{
	//When drawables are created, so are their ambient sounds. After loading, only turn off the
	//ambient sound if the engine is off.
	if( !m_enginesOn )
	{
		Drawable *draw = getObject()->getDrawable();
		if( draw )
		{
			draw->stopAmbientSound();
		}
	}

	// extend base class
	AIUpdateInterface::loadPostProcess();
}
