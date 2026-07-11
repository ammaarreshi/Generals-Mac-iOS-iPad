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

// ChinookAIUpdate.cpp //////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_VETERANCY_NAMES				// for TheVeterancyNames[]

#include "Common/ActionManager.h"
#include "Common/DrawModule.h"
#include "Common/GameState.h"
#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/RandomValue.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/ChinookAIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/PartitionManager.h"

const Real BIGNUM = 99999.0f;


//-------------------------------------------------------------------------------------------------
enum ChinookAIStateType CPP_11(: Int)
{
	// note that these must be distinct (numerically) from AIStateType. ick.
	ChinookAIStateType_FIRST = 1000,

	TAKING_OFF,
	LANDING,

	MOVE_TO_AND_LAND,

	MOVE_TO_AND_EVAC,
	LAND_AND_EVAC,
	EVAC_AND_TAKEOFF,

	MOVE_TO_AND_EVAC_AND_EXIT,
	LAND_AND_EVAC_AND_EXIT,
	EVAC_AND_EXIT,
	TAKEOFF_AND_EXIT,
	HEAD_OFF_MAP,

	MOVE_TO_COMBAT_DROP,
	DO_COMBAT_DROP
};


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
static Real calcDistSqr(const Coord3D& a, const Coord3D& b)
{
	return sqr(a.x-b.x) + sqr(a.y-b.y) + sqr(a.z-b.z);
}

//-------------------------------------------------------------------------------------------------
static Object* getPotentialRappeller(Object* obj)
{
	const ContainedItemsList* items = obj->getContain() ? obj->getContain()->getContainedItemsList() : nullptr;
	if (items)
	{
		for (ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it )
		{
			Object* rider = *it;
			if (rider->isKindOf(KINDOF_CAN_RAPPEL))
			{
				return rider;
			}
		}
	}
	return nullptr;
}

//----------------------------------------------------------------------------------------------------------
class ChinookEvacuateState : public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ChinookEvacuateState, "ChinookEvacuateState")
protected:
	// snapshot interface	STUBBED - no member vars to save. jba.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {};
	virtual void loadPostProcess() override {};
public:
	ChinookEvacuateState( StateMachine *machine ) : State( machine, "ChinookEvacuateState" ) { }

	virtual StateReturnType onEnter() override
	{
		Object* obj = getMachineOwner();
		if( obj->getContain() )
		{
			obj->getContain()->removeAllContained(FALSE);
		}
		obj->getTeam()->setActive();	// why? I don't know.
		return STATE_SUCCESS;
	}

	virtual StateReturnType update() override
	{
		return STATE_SUCCESS;
	}
};
EMPTY_DTOR(ChinookEvacuateState)

//-------------------------------------------------------------------------------------------------
class ChinookHeadOffMapState :  public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ChinookHeadOffMapState, "ChinookHeadOffMapState")
	//I'm outta here
protected:
	// snapshot interface	STUBBED - no member vars to save. jba.
	virtual void crc( Xfer *xfer ) override {};
	virtual void xfer( Xfer *xfer ) override {};
	virtual void loadPostProcess() override {};
public:
	ChinookHeadOffMapState( StateMachine *machine ) : State( machine, "ChinookHeadOffMapState" ) {}

	virtual StateReturnType onEnter() override // Give move order out of town
	{
		Object *owner = getMachineOwner();
		AIUpdateInterface *ai = owner->getAIUpdateInterface();

		// just keep moving straight ahead till we exit the map.
		Coord3D exitCoord = *owner->getPosition();
		const Coord3D* dir = owner->getUnitDirectionVector2D();

		Region3D terrainExtent;
		TheTerrainLogic->getExtent( &terrainExtent );
		const Real FUDGE = 1.2f;
		Real HUGE_DIST = FUDGE * WWMath::SqrtOrigin(sqr(terrainExtent.hi.x - terrainExtent.lo.x) + sqr(terrainExtent.hi.y - terrainExtent.lo.y));

		exitCoord.x += dir->x * HUGE_DIST;
		exitCoord.y += dir->y * HUGE_DIST;

		ai->aiMoveToPosition( &exitCoord, CMD_FROM_AI );
		ai->getCurLocomotor()->setAllowInvalidPosition(true);

		return STATE_CONTINUE;
	}

	virtual StateReturnType update() override
	{
		Object *owner = getMachineOwner();

		Region3D mapRegion;
		TheTerrainLogic->getExtentIncludingBorder( &mapRegion );
		if (!mapRegion.isInRegionNoZ( *owner->getPosition() ))
		{
			TheGameLogic->destroyObject(owner);
			return STATE_SUCCESS;
		}

		return STATE_CONTINUE;
	}
};
EMPTY_DTOR(ChinookHeadOffMapState)

//-------------------------------------------------------------------------------------------------
class ChinookTakeoffOrLandingState : public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ChinookTakeoffOrLandingState, "ChinookTakeoffOrLandingState")
private:
	Coord3D		m_destLoc;
	Bool			m_landing;

protected:
	// snapshot interface
	virtual void crc( Xfer *xfer ) override
	{
		// empty
	}

	virtual void xfer( Xfer *xfer ) override
	{
		// version
		XferVersion currentVersion = 1;
		XferVersion version = currentVersion;
		xfer->xferVersion( &version, currentVersion );

		xfer->xferCoord3D(&m_destLoc);
		xfer->xferBool(&m_landing);
	}

	virtual void loadPostProcess() override
	{
		// empty
	}

public:
	ChinookTakeoffOrLandingState( StateMachine *machine, Bool landing ) : m_landing(landing), State( machine, "ChinookTakeoffOrLandingState" )
	{
		m_destLoc.zero();
	}

	virtual StateReturnType onEnter() override
	{
		Object* obj = getMachineOwner();
		ChinookAIUpdate* ai = (ChinookAIUpdate*)obj->getAIUpdateInterface();

		ai->friend_setFlightStatus(m_landing ? CHINOOK_LANDING : CHINOOK_TAKING_OFF);

		if( m_landing )
		{
			// A chinook given transport duty loses his supplies.
			while( ai->loseOneBox() );
		}

		// kill any drift...
		obj->getPhysics()->scrubVelocity2D(0);

		ai->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
		Locomotor* loco = ai->getCurLocomotor();
		loco->setUsePreciseZPos(true);
		loco->setUltraAccurate(true);

		m_destLoc = *obj->getPosition();
		const Bool onlyHealthyBridges = true;	// ignore dead bridges.
		PathfindLayerEnum layerAtDest = TheTerrainLogic->getHighestLayerForDestination(&m_destLoc, onlyHealthyBridges);
		m_destLoc.z = TheTerrainLogic->getLayerHeight(m_destLoc.x, m_destLoc.y, layerAtDest);
		if (m_landing)
		{
			Coord3D tmp;
			FindPositionOptions options;
			options.maxRadius = obj->getGeometryInfo().getBoundingCircleRadius() * 100.0f;
			if (ThePartitionManager->findPositionAround(&m_destLoc, &options, &tmp))
			{
				m_destLoc = tmp;
				TheAI->pathfinder()->adjustToLandingDestination(obj, &m_destLoc);
			}
			// recalc, since it may have changed. note that findPositionAround() will ALWAYS
			// return a position on the ground proper, so if our initial search start pos was
			// above a bridge, this will put us below the bridge, which would be unfortunate.
			// so recheck to be sure. (note that the partitionmgr is 2d-only, so if it sez that
			// the position on the ground at that xy is clear, it will be clear for both the
			// ground proper and the bridge itself.) also note: don't call objectInteractsWithBridgeLayer(),
			// since it assumes that things that aren't close in z shouldn't interact.
			tmp = m_destLoc;
			tmp.z = obj->getPosition()->z;
			layerAtDest = TheTerrainLogic->getHighestLayerForDestination(&tmp, onlyHealthyBridges);
			m_destLoc.z = TheTerrainLogic->getLayerHeight(m_destLoc.x, m_destLoc.y, layerAtDest);
			obj->setLayer(layerAtDest);
		}
		else
		{
			m_destLoc.z += loco->getPreferredHeight();
			obj->setLayer(LAYER_GROUND);
		}

		return STATE_CONTINUE;
	}

	virtual StateReturnType update() override
	{
		Object* obj = getMachineOwner();
		if (obj->isEffectivelyDead())
			return STATE_FAILURE;

		ChinookAIUpdate* ai = (ChinookAIUpdate*)obj->getAIUpdateInterface();

		ai->setLocomotorGoalPositionExplicit(m_destLoc);

		const Real THRESH = 3.0f;
		const Real THRESH_SQR = THRESH*THRESH;
		if (calcDistSqr(*obj->getPosition(), m_destLoc) <= THRESH_SQR)
			return STATE_SUCCESS;

		return STATE_CONTINUE;
	}

	virtual void onExit( StateExitType status ) override
	{
		Object* obj = getMachineOwner();
		ChinookAIUpdate* ai = (ChinookAIUpdate*)obj->getAIUpdateInterface();

		ai->friend_setFlightStatus(m_landing ? CHINOOK_LANDED : CHINOOK_FLYING);

		// Paranoia checks - sometimes onExit is called when we are
		// shutting down, and not all pieces are valid.  CurLocomotor
		// is definitely null in some cases. jba.
		Locomotor* loco = ai->getCurLocomotor();
		if (loco)
		{
			loco->setUsePreciseZPos(false);
			loco->setUltraAccurate(false);
			// don't restore lift if dead -- this may fight with JetSlowDeathBehavior!
			if (!obj->isEffectivelyDead())
				loco->setMaxLift(BIGNUM);
		}

		if (m_landing)
		{
			ai->chooseLocomotorSet(LOCOMOTORSET_TAXIING);
		}
		else
		{
			// when takeoff is complete, always go back to layer-ground, rather than
			// some bridge layer.
			obj->setLayer(LAYER_GROUND);
		}
	}

};
EMPTY_DTOR(ChinookTakeoffOrLandingState)

//-------------------------------------------------------------------------------------------------
class ChinookCombatDropState : public State
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ChinookCombatDropState, "ChinookCombatDropState")

private:

	struct RopeInfo
	{
		Drawable*								ropeDrawable;
		DrawableID							ropeID;	// used only during save-load process
		Matrix3D								dropStartMtx;
		Real										ropeSpeed;
		Real										ropeLen;
		Real										ropeLenMax;
		UnsignedInt							nextDropTime;
		std::list<ObjectID>			rappellerIDs;
	};
	std::vector<RopeInfo>			m_ropes;

	void removeDoneRappellers()
	{
		for (std::vector<RopeInfo>::iterator it = m_ropes.begin(); it != m_ropes.end(); ++it)
		{
			for (std::list<ObjectID>::iterator oit = it->rappellerIDs.begin(); oit != it->rappellerIDs.end(); )
			{
				Object* rappeller = TheGameLogic->findObjectByID(*oit);
#if RETAIL_COMPATIBLE_CRC
				if (rappeller == nullptr || rappeller->isEffectivelyDead() || !rappeller->isAboveTerrain())
#else
				if (rappeller == nullptr || rappeller->isEffectivelyDead() || !rappeller->isAboveTerrain() || rappeller->isContained())
#endif
				{
					oit = it->rappellerIDs.erase(oit);
				}
				else
				{
					++oit;
				}
			}
		}
	}

	static void initRopeParms(Drawable* rope, Real length, Real width, const RGBColor& color, Real wobbleLen, Real wobbleAmp, Real wobbleRate)
	{
		RopeDrawInterface* tdi = nullptr;
		for (DrawModule** d = rope->getDrawModules(); *d; ++d)
		{
			if ((tdi = (*d)->getRopeDrawInterface()) != nullptr)
			{
				tdi->initRopeParms(length, width, color, wobbleLen, wobbleAmp, wobbleRate);
			}
		}
	}

	static void setRopeCurLen(Drawable* rope, Real length)
	{
		RopeDrawInterface* tdi = nullptr;
		for (DrawModule** d = rope->getDrawModules(); *d; ++d)
		{
			if ((tdi = (*d)->getRopeDrawInterface()) != nullptr)
			{
				tdi->setRopeCurLen(length);
			}
		}
	}

	static void setRopeSpeed(Drawable* rope, Real curSpeed, Real maxSpeed, Real accel)
	{
		RopeDrawInterface* tdi = nullptr;
		for (DrawModule** d = rope->getDrawModules(); *d; ++d)
		{
			if ((tdi = (*d)->getRopeDrawInterface()) != nullptr)
			{
				tdi->setRopeSpeed(curSpeed, maxSpeed, accel);
			}
		}
	}

protected:
	// snapshot interface
	virtual void crc( Xfer *xfer ) override
	{
		// empty
	}

	virtual void xfer( Xfer *xfer ) override
	{
		// version
		const XferVersion currentVersion = 2;
		XferVersion version = currentVersion;
		xfer->xferVersion( &version, currentVersion );

		Int numRopes = m_ropes.size();
		xfer->xferInt(&numRopes);

		if (version >= 2)
		{
			if (xfer->getXferMode() == XFER_LOAD)
			{
				if (!m_ropes.empty())
				{
					DEBUG_CRASH(( "ChinookCombatDropState - ropes should be empty" ));
					throw SC_INVALID_DATA;
				}
				m_ropes.resize(numRopes);
			}

			for (Int i = 0; i < numRopes; ++i)
			{
				RopeInfo info;
				if (xfer->getXferMode() == XFER_SAVE)
				{
					info = m_ropes[i];
					// always overwrite this, since it's probably stale
					info.ropeID = info.ropeDrawable ? info.ropeDrawable->getID() : INVALID_DRAWABLE_ID;
				}
				xfer->xferDrawableID(&info.ropeID);
				xfer->xferMatrix3D(&info.dropStartMtx);
				xfer->xferReal(&info.ropeSpeed);
				xfer->xferReal(&info.ropeLen);
				xfer->xferReal(&info.ropeLenMax);
				xfer->xferUnsignedInt(&info.nextDropTime);
				xfer->xferSTLObjectIDList(&info.rappellerIDs);
				if (xfer->getXferMode() == XFER_LOAD)
				{
					info.ropeDrawable = nullptr;	// filled in via loadPostProcess
					m_ropes[i] = info;
				}
			}
		}
	}

	virtual void loadPostProcess() override
	{
		for (std::vector<RopeInfo>::iterator it = m_ropes.begin(); it != m_ropes.end(); ++it)
		{
			it->ropeDrawable = TheGameClient->findDrawableByID(it->ropeID);
			// always nuke this, since we're done with it till we save/load again
			it->ropeID = INVALID_DRAWABLE_ID;
		}
	}

public:
	ChinookCombatDropState( StateMachine *machine ): State( machine, "ChinookCombatDropState" ) { }

	// --------------
	virtual StateReturnType onEnter() override
	{
		Object* obj = getMachineOwner();
		Drawable* draw = obj->getDrawable();
		if (draw == nullptr)
			return STATE_FAILURE;

		ChinookAIUpdate* ai = (ChinookAIUpdate*)obj->getAIUpdateInterface();
		const ChinookAIUpdateModuleData* d = ai->friend_getData();

		obj->setDisabled( DISABLED_HELD );
		ai->friend_setFlightStatus(CHINOOK_DOING_COMBAT_DROP);

		// A chinook given combat drop duty also loses his supplies.
		while( ai->loseOneBox() );

		UnsignedInt now = TheGameLogic->getFrame();

		const ThingTemplate* ropeTmpl = TheThingFactory->findTemplate(d->m_ropeName);

		const Int MAX_BONES = 32;
    Coord3D ropePos[MAX_BONES];
    Matrix3D dropMtx[MAX_BONES];

		Int ropeCount = draw->getPristineBonePositions("RopeStart", 1, ropePos, nullptr, MAX_BONES);
		Int dropCount = draw->getPristineBonePositions("RopeEnd", 1, nullptr, dropMtx, MAX_BONES);

		Int numRopes = d->m_numRopes;
		if (numRopes > ropeCount) numRopes = ropeCount;
		if (numRopes > dropCount) numRopes = dropCount;
		if (numRopes <= 0)
			return STATE_FAILURE;

		m_ropes.clear();
		for (Int i = 0; i < numRopes; ++i)
		{
			RopeInfo info;

			obj->convertBonePosToWorldPos( nullptr, &dropMtx[i], nullptr, &info.dropStartMtx );

			info.ropeDrawable = ropeTmpl ? TheThingFactory->newDrawable(ropeTmpl) : nullptr;
			if (info.ropeDrawable)
			{
				obj->convertBonePosToWorldPos( &ropePos[i], nullptr, &ropePos[i], nullptr );
				info.ropeDrawable->setPosition(&ropePos[i]);
				info.ropeSpeed = 0.0f;
				info.ropeLen = 1.0f;

				const Bool onlyHealthyBridges = true;	// ignore dead bridges.
				PathfindLayerEnum layerAtDest = TheTerrainLogic->getHighestLayerForDestination(&ropePos[i], onlyHealthyBridges);

				info.ropeLenMax = ropePos[i].z - TheTerrainLogic->getLayerHeight(ropePos[i].x, ropePos[i].y, layerAtDest) - d->m_ropeFinalHeight;

				initRopeParms(info.ropeDrawable, info.ropeLenMax, d->m_ropeWidth, d->m_ropeColor, d->m_ropeWobbleLen, d->m_ropeWobbleAmp, d->m_ropeWobbleRate);
			}

			info.nextDropTime = now + GameLogicRandomValue(d->m_perRopeDelayMin, d->m_perRopeDelayMax) - d->m_perRopeDelayMin;
			info.rappellerIDs.clear();

			m_ropes.push_back(info);
		}

		return STATE_CONTINUE;
	}

	// --------------
	virtual StateReturnType update() override
	{
		Object* obj = getMachineOwner();
		ChinookAIUpdate* ai = (ChinookAIUpdate*)obj->getAIUpdateInterface();
		const ChinookAIUpdateModuleData* d = ai->friend_getData();

		if (obj->isEffectivelyDead())
		{
			return STATE_FAILURE;
		}

		// first, eliminate "done" rappellers
		removeDoneRappellers();

		UnsignedInt now = TheGameLogic->getFrame();

		// ok, now check each rope: if it's empty, or we're at the next drop time, spawn a new rappeller
		Int numRopesInUse = 0;
		for (std::vector<RopeInfo>::iterator it = m_ropes.begin(); it != m_ropes.end(); ++it)
		{
			if (it->ropeLen < it->ropeLenMax)
			{
				it->ropeSpeed += fabs(TheGlobalData->m_gravity);
				if (it->ropeSpeed > d->m_ropeDropSpeed)
					it->ropeSpeed = d->m_ropeDropSpeed;
				it->ropeLen += it->ropeSpeed;
				setRopeCurLen(it->ropeDrawable, it->ropeLen);
				if (d->m_waitForRopesToDrop)
				{
					// can't use this rope till it's dropped all the way
					++it->nextDropTime;
					continue;
				}
			}

			if (now >= it->nextDropTime)
			{
				Object* rappeller = getPotentialRappeller(obj);
				if (rappeller != nullptr)
				{
#if RETAIL_COMPATIBLE_CRC
					ExitInterface *exitInterface = obj->getObjectExitInterface();
					ExitDoorType exitDoor = exitInterface ? exitInterface->reserveDoorForExit(rappeller->getTemplate(), rappeller) : DOOR_NONE_AVAILABLE;
					if(exitDoor != DOOR_NONE_AVAILABLE)
					{
						exitInterface->exitObjectViaDoor(rappeller, exitDoor);
					}
					else
					{
						DEBUG_CRASH(("rappeller is not free to exit... what?"));
					}
#else
					// TheSuperHackers @bugfix 03/01/2026 Bypass door reservation as rappellers are always
					// expected to be free to exit. This avoids prior rappel conditions in getAiFreeToExit
					// from allowing rappellers to be freely 'dropped' from the Chinook during this state.
					if (ExitInterface* exitInterface = obj->getObjectExitInterface())
					{
						exitInterface->exitObjectViaDoor(rappeller, DOOR_1);
					}
#endif

					rappeller->setTransformMatrix(&it->dropStartMtx);

					AIUpdateInterface* rappellerAI = rappeller ? rappeller->getAIUpdateInterface() : nullptr;
					if (rappellerAI)
					{
						rappellerAI->setDesiredSpeed(d->m_rappelSpeed);
						rappellerAI->aiRappelInto(getMachineGoalObject(), *getMachineGoalPosition(), CMD_FROM_AI);
					}

					it->rappellerIDs.push_back(rappeller->getID());

					it->nextDropTime = now + GameLogicRandomValue(d->m_perRopeDelayMin, d->m_perRopeDelayMax);
				}
			}

			if (!it->rappellerIDs.empty())
			{
				++numRopesInUse;
			}
		}

		if (numRopesInUse == 0 && getPotentialRappeller(obj) == nullptr)
		{
			// we're done!
			return STATE_SUCCESS;
		}

		return STATE_CONTINUE;
	}

	// --------------
	virtual void onExit( StateExitType status ) override
	{
		Object* obj = getMachineOwner();
		ChinookAIUpdate* ai = (ChinookAIUpdate*)obj->getAIUpdateInterface();
		const ChinookAIUpdateModuleData* d = ai->friend_getData();

		obj->clearDisabled( DISABLED_HELD );
		ai->friend_setFlightStatus(CHINOOK_FLYING);

		if (obj->isEffectivelyDead())
		{
			// oops. drop the rangers.
			for (std::vector<RopeInfo>::iterator it = m_ropes.begin(); it != m_ropes.end(); ++it)
			{
				for (std::list<ObjectID>::iterator oit = it->rappellerIDs.begin(); oit != it->rappellerIDs.end(); ++oit)
				{
					Object* rappeller = TheGameLogic->findObjectByID(*oit);
					AIUpdateInterface* rappellerAI = rappeller ? rappeller->getAIUpdateInterface() : nullptr;
					if (rappellerAI != nullptr)
					{
						rappellerAI->aiIdle(CMD_FROM_AI);
					}
				}
			}
		}

		UnsignedInt now = TheGameLogic->getFrame();
		for (size_t i = 0; i < m_ropes.size(); ++i)
		{
			if (m_ropes[i].ropeDrawable)
			{
				const UnsignedInt ROPE_EXPIRATION_TIME = LOGICFRAMES_PER_SECOND * 5;
				const Real initialSpeed = TheGlobalData->m_gravity * 30;	// give it a little kick
				setRopeSpeed(m_ropes[i].ropeDrawable, initialSpeed, d->m_ropeDropSpeed, TheGlobalData->m_gravity);
				m_ropes[i].ropeDrawable->setExpirationDate(now + ROPE_EXPIRATION_TIME);
				m_ropes[i].ropeDrawable = nullptr; // we're done with it, so null it so we won't save it
			}
		}

		m_ropes.clear();
	}

};
EMPTY_DTOR(ChinookCombatDropState)

//-----------------------------------------------------------------------------------------------------------
/**
 * Move to the GoalPosition, or GoalObject.
 */
class ChinookMoveToBldgState : public AIMoveToState
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ChinookMoveToBldgState, "ChinookMoveToBldgState")
private:
	Real m_oldPreferredHeight;
	Real m_newPreferredHeight;
	Real m_destZ;
protected:
	// snapshot interface
	virtual void crc( Xfer *xfer ) override
	{
		// empty
	}

	virtual void xfer( Xfer *xfer ) override
	{
		// version
		XferVersion currentVersion = 1;
		XferVersion version = currentVersion;
		xfer->xferVersion( &version, currentVersion );

		xfer->xferReal(&m_oldPreferredHeight);
		xfer->xferReal(&m_newPreferredHeight);
		xfer->xferReal(&m_destZ);
	}

	virtual void loadPostProcess() override
	{
		// empty
	}

public:
	ChinookMoveToBldgState( StateMachine *machine ): AIMoveToState( machine ) { }

	virtual StateReturnType onEnter() override
	{
		Object* obj = getMachineOwner();
		ChinookAIUpdate* ai = (ChinookAIUpdate*)obj->getAIUpdateInterface();
		const ChinookAIUpdateModuleData* d = ai->friend_getData();
		Locomotor* loco = ai->getCurLocomotor();
		loco->setUltraAccurate(true);
		m_oldPreferredHeight = loco->getPreferredHeight();
		m_newPreferredHeight = m_oldPreferredHeight;

		const Coord3D* destPos;
		Object* bldg = getMachineGoalObject();
		if (bldg != nullptr && !bldg->isEffectivelyDead() && bldg->isKindOf(KINDOF_STRUCTURE))
		{
			destPos = bldg->getPosition();
			m_newPreferredHeight = bldg->getGeometryInfo().getMaxHeightAbovePosition() + d->m_minDropHeight;
			if (m_newPreferredHeight < m_oldPreferredHeight)
				m_newPreferredHeight = m_oldPreferredHeight;
		}
		else
		{
			destPos = getMachineGoalPosition();
		}

		loco->setPreferredHeight(m_newPreferredHeight);

		m_destZ = TheTerrainLogic->getGroundHeight(destPos->x, destPos->y) + m_newPreferredHeight;

		return AIMoveToState::onEnter();
	}

	virtual StateReturnType update() override
	{
		Object* obj = getMachineOwner();

		// the normal moveto state will bail when 2d pos matches; we need z, too
		StateReturnType status = AIMoveToState::update();

		const Real THRESH = 3.0f;
		if (status != STATE_CONTINUE && fabs(obj->getPosition()->z - m_destZ) > THRESH)
			status = STATE_CONTINUE;

		return status;
	}

	virtual void onExit( StateExitType status ) override
	{
		Object* obj = getMachineOwner();
		ChinookAIUpdate* ai = (ChinookAIUpdate*)obj->getAIUpdateInterface();
		Locomotor* loco = ai->getCurLocomotor();
		loco->setPreferredHeight(m_oldPreferredHeight);
		loco->setUltraAccurate(false);
		AIMoveToState::onExit(status);
	}

};
EMPTY_DTOR(ChinookMoveToBldgState)


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
class ChinookAIStateMachine : public AIStateMachine
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ChinookAIStateMachine, "ChinookAIStateMachine" );

public:
	ChinookAIStateMachine( Object *owner, AsciiString name );

};

//-------------------------------------------------------------------------------------------------
ChinookAIStateMachine::ChinookAIStateMachine(Object *owner, AsciiString name) : AIStateMachine(owner, name)
{
	defineState( TAKING_OFF, newInstance(ChinookTakeoffOrLandingState)( this, false ), AI_IDLE, AI_IDLE );
	defineState( LANDING, newInstance(ChinookTakeoffOrLandingState)( this, true ), AI_IDLE, AI_IDLE );
	defineState( MOVE_TO_COMBAT_DROP, newInstance(ChinookMoveToBldgState)( this ), DO_COMBAT_DROP, AI_IDLE );
	defineState( DO_COMBAT_DROP, newInstance(ChinookCombatDropState)( this ), AI_IDLE, AI_IDLE );

	defineState( MOVE_TO_AND_LAND, newInstance(AIMoveToState)( this ), LANDING, AI_IDLE );

	defineState( MOVE_TO_AND_EVAC, newInstance(AIMoveToState)( this ), LAND_AND_EVAC, AI_IDLE );
	defineState( LAND_AND_EVAC, newInstance(ChinookTakeoffOrLandingState)( this, true ), EVAC_AND_TAKEOFF, AI_IDLE );
	defineState( EVAC_AND_TAKEOFF, newInstance(ChinookEvacuateState)( this ), TAKING_OFF, AI_IDLE );

	defineState( MOVE_TO_AND_EVAC_AND_EXIT, newInstance(AIMoveToState)( this ), LAND_AND_EVAC_AND_EXIT, AI_IDLE );
	defineState( LAND_AND_EVAC_AND_EXIT, newInstance(ChinookTakeoffOrLandingState)( this, true ), EVAC_AND_EXIT, AI_IDLE );
	defineState( EVAC_AND_EXIT, newInstance(ChinookEvacuateState)( this ), TAKEOFF_AND_EXIT, AI_IDLE );
	defineState( TAKEOFF_AND_EXIT, newInstance(ChinookTakeoffOrLandingState)( this, false ), HEAD_OFF_MAP, AI_IDLE );
	defineState( HEAD_OFF_MAP, newInstance(ChinookHeadOffMapState)( this ), AI_IDLE, AI_IDLE );
}

//-------------------------------------------------------------------------------------------------
ChinookAIStateMachine::~ChinookAIStateMachine()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
ChinookAIUpdateModuleData::ChinookAIUpdateModuleData()
{
	m_numRopes = 4;
	m_ropeWidth = 0.5f;
	m_ropeColor.red = 0.9f;
	m_ropeColor.green = 0.8f;
	m_ropeColor.blue = 0.7f;
	m_perRopeDelayMin = 0x7fffffff;
	m_perRopeDelayMax = 0x7fffffff;
	m_ropeName = "GenericRope";
	m_waitForRopesToDrop = true;
	m_minDropHeight = 30.0f;
	m_ropeFinalHeight = 0.0f;
	m_ropeDropSpeed = 1e10f;		// um, fast.
	m_rappelSpeed = fabs(TheGlobalData->m_gravity) * LOGICFRAMES_PER_SECOND * 0.5f;
	m_ropeWobbleLen = 10.0f;
	m_ropeWobbleAmp = 1.0f;
	m_ropeWobbleRate = 0.1f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void ChinookAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  SupplyTruckAIUpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "RappelSpeed", INI::parseVelocityReal, 0, offsetof(ChinookAIUpdateModuleData, m_rappelSpeed) },
		{ "RopeDropSpeed", INI::parseVelocityReal, 0, offsetof(ChinookAIUpdateModuleData, m_ropeDropSpeed) },
		{ "RopeName", INI::parseAsciiString, 0, offsetof(ChinookAIUpdateModuleData, m_ropeName) },
		{ "RopeFinalHeight", INI::parseReal, 0, offsetof(ChinookAIUpdateModuleData, m_ropeFinalHeight) },
		{ "RopeWidth", INI::parseReal, 0, offsetof(ChinookAIUpdateModuleData, m_ropeWidth) },
		{ "RopeWobbleLen", INI::parseReal, 0, offsetof(ChinookAIUpdateModuleData, m_ropeWobbleLen) },
		{ "RopeWobbleAmplitude", INI::parseReal, 0, offsetof(ChinookAIUpdateModuleData, m_ropeWobbleAmp) },
		{ "RopeWobbleRate", INI::parseAngularVelocityReal, 0, offsetof(ChinookAIUpdateModuleData, m_ropeWobbleRate) },
		{ "RopeColor", INI::parseRGBColor, 0, offsetof(ChinookAIUpdateModuleData, m_ropeColor) },
		{ "NumRopes", INI::parseUnsignedInt, 0, offsetof(ChinookAIUpdateModuleData, m_numRopes) },
		{ "PerRopeDelayMin", INI::parseDurationUnsignedInt, 0, offsetof(ChinookAIUpdateModuleData, m_perRopeDelayMin) },
		{ "PerRopeDelayMax", INI::parseDurationUnsignedInt, 0, offsetof(ChinookAIUpdateModuleData, m_perRopeDelayMax) },
		{ "MinDropHeight", INI::parseReal, 0, offsetof(ChinookAIUpdateModuleData, m_minDropHeight) },
		{ "WaitForRopesToDrop", INI::parseBool, 0, offsetof(ChinookAIUpdateModuleData, m_waitForRopesToDrop) },
		{ 0, 0, 0, 0 }
	};
  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
AIStateMachine* ChinookAIUpdate::makeStateMachine()
{
	return newInstance(ChinookAIStateMachine)( getObject(), "ChinookAIStateMachine");
}

//-------------------------------------------------------------------------------------------------
ChinookAIUpdate::ChinookAIUpdate( Thing *thing, const ModuleData* moduleData ) : SupplyTruckAIUpdate( thing, moduleData )
{
	m_hasPendingCommand = false;
	m_flightStatus = CHINOOK_FLYING;	// yep, that's right, even if we start "on ground"
	m_airfieldForHealing = INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
ChinookAIUpdate::~ChinookAIUpdate()
{

}

//-------------------------------------------------------------------------------------------------
static ParkingPlaceBehaviorInterface* getPP(ObjectID id)
{
	Object* airfield = TheGameLogic->findObjectByID( id );
	if (airfield == nullptr || airfield->isEffectivelyDead() || !airfield->isKindOf(KINDOF_FS_AIRFIELD))
		return nullptr;

	ParkingPlaceBehaviorInterface* pp = nullptr;
	for (BehaviorModule** i = airfield->getBehaviorModules(); *i; ++i)
	{
		if ((pp = (*i)->getParkingPlaceBehaviorInterface()) != nullptr)
			break;
	}

	return pp;
}

//-------------------------------------------------------------------------------------------------
void ChinookAIUpdate::setAirfieldForHealing(ObjectID id)
{
	// make sure we de-register with current one, if any
	if (m_airfieldForHealing != INVALID_ID && m_airfieldForHealing != id)
	{
		ParkingPlaceBehaviorInterface* pp = getPP(m_airfieldForHealing);
		if (pp != nullptr)
		{
			pp->setHealee(getObject(), false);
		}
	}
	m_airfieldForHealing = id;
}

//-------------------------------------------------------------------------------------------------
Bool ChinookAIUpdate::isIdle() const
{
	// we need to do this because we enter an idle state briefly between takeoff/landing in these cases,
	// but scripting relies on us never claiming to be "idle"...
	if (m_hasPendingCommand)
		return false;

	Bool result = SupplyTruckAIUpdate::isIdle();

	if (result && m_flightStatus == CHINOOK_LANDED)
	{
		// ditto: if we are waiting to disgorge some folks, we aren't 'idle'
		ContainModuleInterface* contain = getObject()->getContain();
		if (contain && contain->hasObjectsWantingToEnterOrExit())
			result = false;
	}

	return result;
}

//-------------------------------------------------------------------------------------------------
Bool ChinookAIUpdate::isCurrentlyFerryingSupplies() const
{
	return SupplyTruckAIUpdate::isCurrentlyFerryingSupplies();
}

//-------------------------------------------------------------------------------------------------
Bool ChinookAIUpdate::isAvailableForSupplying() const
{
	if (!SupplyTruckAIUpdate::isAvailableForSupplying())
		return false;

	ContainModuleInterface* contain = getObject()->getContain();
	if( !contain || contain->hasObjectsWantingToEnterOrExit() || contain->getContainCount())
		return false;

	return true;
}

//-------------------------------------------------------------------------------------------------
Bool ChinookAIUpdate::isAllowedToAdjustDestination() const
{
	 if (m_flightStatus == CHINOOK_LANDED)
		return false;

	return SupplyTruckAIUpdate::isAllowedToAdjustDestination();
}

//-------------------------------------------------------------------------------------------------
ObjectID ChinookAIUpdate::getBuildingToNotPathAround() const
{
	if (getAIStateType() == MOVE_TO_COMBAT_DROP || getAIStateType() == DO_COMBAT_DROP)
	{
		const Object* goalObj = getStateMachine()->getGoalObject();
		if (goalObj)
			return goalObj->getID();
	}

	return INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
AIFreeToExitType ChinookAIUpdate::getAiFreeToExit(const Object* exiter) const
{
#if RETAIL_COMPATIBLE_CRC
	 if (m_flightStatus == CHINOOK_LANDED
				|| (m_flightStatus == CHINOOK_DOING_COMBAT_DROP && exiter->isKindOf(KINDOF_CAN_RAPPEL)))
#else
	if (m_flightStatus == CHINOOK_LANDED)
#endif
		return FREE_TO_EXIT;

	return WAIT_TO_EXIT;
}

//-------------------------------------------------------------------------------------------------
Bool ChinookAIUpdate::chooseLocomotorSet(LocomotorSetType wst)
{
	if (m_flightStatus == CHINOOK_LANDED)
		wst = LOCOMOTORSET_TAXIING;
	return SupplyTruckAIUpdate::chooseLocomotorSet(wst);
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ChinookAIUpdate::update()
{
	ParkingPlaceBehaviorInterface* pp = getPP(m_airfieldForHealing);
	const ContainModuleInterface* contain = getObject()->getContain();
	const Bool waitingToEnterOrExit = contain && contain->hasObjectsWantingToEnterOrExit();

	if (pp != nullptr)
	{
		if (m_flightStatus == CHINOOK_LANDED &&
#if !RETAIL_COMPATIBLE_CRC
				// TheSuperHackers @bugfix Stubbjax 03/11/2025 Prevent Chinooks from taking off while there are still units wanting to enter or exit.
				!waitingToEnterOrExit &&
#endif
				!m_hasPendingCommand &&
				getObject()->getBodyModule()->getHealth() == getObject()->getBodyModule()->getMaxHealth())
		{
			// we're completely healed, so take off again
			pp->setHealee(getObject(), false);
			setMyState(TAKING_OFF, nullptr, nullptr, CMD_FROM_AI);

#if !RETAIL_COMPATIBLE_CRC
			// TheSuperHackers @bugfix arcticdolphin 02/03/2026 Move healed Chinook to rally point if present.
			if (Object *airfield = TheGameLogic->findObjectByID( m_airfieldForHealing ))
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
			pp->setHealee(getObject(), m_flightStatus == CHINOOK_LANDED);
		}
	}
	else
	{
		setAirfieldForHealing(INVALID_ID);
	}

	// have to call our parent's isIdle, because we override it to never return true
	// when we have a pending command...
	if (SupplyTruckAIUpdate::isIdle())
	{
		if( contain )
		{
			if (m_hasPendingCommand)
			{
				AICommandParms parms(AICMD_MOVE_TO_POSITION, CMD_FROM_AI);	// values don't matter, will be wiped by next line
				m_pendingCommand.reconstitute(parms);
				m_hasPendingCommand = false;
				aiDoCommand(&parms);
			}
			else if (waitingToEnterOrExit && m_flightStatus != CHINOOK_LANDED)
			{
				setMyState(LANDING, nullptr, nullptr, CMD_FROM_AI);
			}
			else if (!waitingToEnterOrExit && m_flightStatus == CHINOOK_LANDED && m_airfieldForHealing == INVALID_ID)
			{
				setMyState(TAKING_OFF, nullptr, nullptr, CMD_FROM_AI);
			}
		}
	}

	return SupplyTruckAIUpdate::update();
}

//-------------------------------------------------------------------------------------------------
void ChinookAIUpdate::setMyState( StateID cmd, Object* target, const Coord3D* pos, CommandSourceType cmdSource )
{
	getStateMachine()->clear();
	getStateMachine()->setGoalObject( target );
	setGoalPositionClipped(pos, cmdSource); // yeah, null is ok here.
	setLastCommandSource( cmdSource );
	getStateMachine()->setState( cmd );
}

//----------------------------------------------------------------------------------------
/**
 * Get repaired at the repair depot
 */
void ChinookAIUpdate::privateGetRepaired( Object *repairDepot, CommandSourceType cmdSource )
{
	// we are already landing. just ignore it.
	if (m_flightStatus == CHINOOK_LANDING || m_flightStatus == CHINOOK_LANDED)
		return;

	// sanity, if we can't get repaired from here get out of here
	if( TheActionManager->canGetRepairedAt( getObject(), repairDepot, cmdSource ) == FALSE )
		return;

	setAirfieldForHealing(repairDepot->getID());

	Coord3D pos = *repairDepot->getPosition();
	Coord3D tmp;
	FindPositionOptions options;
	options.maxRadius = repairDepot->getGeometryInfo().getBoundingCircleRadius() * 100.0f;
	if (ThePartitionManager->findPositionAround(&pos, &options, &tmp))
		pos = tmp;

	setMyState(MOVE_TO_AND_LAND, nullptr, &pos, cmdSource);

}

//-------------------------------------------------------------------------------------------------
void ChinookAIUpdate::privateCombatDrop( Object* target, const Coord3D& pos, CommandSourceType cmdSource )
{

	//
	// when there is a target present, we must verify that we can logically do the action when
	// we get commands from players (we'll assume AI knows what its doing)
	//
	if( target != nullptr && cmdSource == CMD_FROM_PLAYER &&
			TheActionManager->canEnterObject( getObject(), target, cmdSource, COMBATDROP_INTO ) == FALSE )
		return;

	Coord3D localPos = pos;
	if (target == nullptr)
	{
		// if target is null, we are dropping at a pos, not into a bldg.
		// in this case, ensure there is no structure at the pos... this can happen
		// if you combat-drop into a spot in the fog-of-war.
		Coord3D tmp;
		FindPositionOptions options;
		options.maxRadius = getObject()->getGeometryInfo().getBoundingCircleRadius() * 100.0f;
		if (ThePartitionManager->findPositionAround(&localPos, &options, &tmp))
		{
			localPos = tmp;
		}
	}

	// start the combat drop process
	setMyState(MOVE_TO_COMBAT_DROP, target, &localPos, cmdSource);

}

//-------------------------------------------------------------------------------------------------
void ChinookAIUpdate::aiDoCommand(const AICommandParms* parms)
{
#if RETAIL_COMPATIBLE_CRC
	// this gets reset every time a command is issued.
	setAirfieldForHealing(INVALID_ID);
#else
	// TheSuperHackers @bugfix Stubbjax 31/10/2025 Don't leave healing state for evacuation commands.
	if (parms->m_cmd != AICMD_EVACUATE && parms->m_cmd != AICMD_EXIT)
		setAirfieldForHealing(INVALID_ID);
#endif

	if (!isAllowedToRespondToAiCommands(parms))
		return;

	if (m_flightStatus == CHINOOK_TAKING_OFF ||
				m_flightStatus == CHINOOK_LANDING ||
				m_flightStatus == CHINOOK_DOING_COMBAT_DROP)
	{
		// have to wait for takeoff or landing (or rappel) to complete, just store the sucker.
		m_pendingCommand.store(*parms);
		m_hasPendingCommand = true;
		return;
	}

	Bool passItThru = true;
	switch (parms->m_cmd)
	{
		case AICMD_IDLE:
		case AICMD_BUSY:
		case AICMD_FOLLOW_EXITPRODUCTION_PATH:
		{
			// don't need (or want) to take off for these...
			// just pass it thru.
		}
		break;

		case AICMD_MOVE_TO_POSITION_AND_EVACUATE:
		case AICMD_MOVE_TO_POSITION_AND_EVACUATE_AND_EXIT:
		{
			const Real THRESH = 3.0f;
			const Real THRESH_SQR = THRESH*THRESH;
#if RETAIL_COMPATIBLE_CRC || PRESERVE_CHINOOK_PASSENGER_DUMPING
			const bool allowExit = true;
#else
			// TheSuperHackers @bugfix Stubbjax 04/11/2025 Passengers are no longer all dumped in a single frame.
			const ContainModuleInterface* contain = getObject()->getContain();
			const bool allowExit = contain && contain->hasObjectsWantingToEnterOrExit();
#endif

			if (calcDistSqr(*getObject()->getPosition(), parms->m_pos) > THRESH_SQR &&
					m_flightStatus == CHINOOK_LANDED)
			{
				// gotta take off first!
				m_pendingCommand.store(*parms);
				m_hasPendingCommand = true;

				setMyState(TAKING_OFF, nullptr, nullptr, CMD_FROM_AI);
				passItThru = false;
			}
			else if (allowExit)
			{
				// do this INSTEAD of the standard stuff
				setMyState(
					(parms->m_cmd == AICMD_MOVE_TO_POSITION_AND_EVACUATE) ? MOVE_TO_AND_EVAC : MOVE_TO_AND_EVAC_AND_EXIT,
					nullptr, &parms->m_pos, CMD_FROM_AI);
				passItThru = false;
			}
		}
		break;

		case AICMD_EXIT:
		case AICMD_EVACUATE:
		{
			if (m_flightStatus != CHINOOK_LANDED)
			{
				// gotta land first!
				m_pendingCommand.store(*parms);
				m_hasPendingCommand = true;

				setMyState(LANDING, nullptr, nullptr, CMD_FROM_AI);
				passItThru = false;
			}
		}
		break;

		default:
		{
			if (m_flightStatus != CHINOOK_FLYING)
			{
				// gotta take off first!
				m_pendingCommand.store(*parms);
				m_hasPendingCommand = true;

				setMyState(TAKING_OFF, nullptr, nullptr, CMD_FROM_AI);
				passItThru = false;
			}
		}
		break;
	}

	if (passItThru)
	{
		m_hasPendingCommand = false;
		SupplyTruckAIUpdate::aiDoCommand(parms);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ChinookAIUpdate::crc( Xfer *xfer )
{
	SupplyTruckAIUpdate::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ChinookAIUpdate::xfer( Xfer *xfer )
{

  // version
  XferVersion currentVersion = 1;
  XferVersion version = currentVersion;
  xfer->xferVersion( &version, currentVersion );
	SupplyTruckAIUpdate::xfer(xfer);

	xfer->xferBool(&m_hasPendingCommand);
	if (m_hasPendingCommand) {
		m_pendingCommand.doXfer(xfer);
	}
	xfer->xferUser(&m_flightStatus, sizeof(m_flightStatus));
	xfer->xferObjectID(&m_airfieldForHealing);
 // extend base class
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ChinookAIUpdate::loadPostProcess()
{
	SupplyTruckAIUpdate::loadPostProcess();
}
