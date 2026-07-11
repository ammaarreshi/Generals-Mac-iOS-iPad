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

// FILE: ParkingPlaceBehavior.cpp ///////////////////////////////////////////////////////////////////////
// Author:	Steven Johnson, June 2002
// Desc:
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "Common/CRCDebug.h"
#include "Common/Xfer.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ParkingPlaceBehavior.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/JetAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "Common/Team.h"



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ParkingPlaceBehavior::ParkingPlaceBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_gotInfo = false;
	m_heliRallyPoint.zero();
	m_heliRallyPointExists = FALSE;
	m_nextHealFrame = FOREVER;
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ParkingPlaceBehavior::~ParkingPlaceBehavior()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::buildInfo()
{
	if (m_gotInfo)
		return;

	if (getObject()->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ||
			getObject()->testStatus(OBJECT_STATUS_SOLD))
		return;

	const ParkingPlaceBehaviorModuleData* d = getParkingPlaceBehaviorModuleData();
	ProductionUpdateInterface* pu = getObject()->getProductionUpdateInterface();

	m_spaces.reserve(d->m_numRows * d->m_numCols);

	Int door = 0;
	{
		ParkingPlaceInfo info;
		for (Int row = 0; row < d->m_numRows; ++row)
		{
			for (Int col = 0; col < d->m_numCols; ++col)
			{
				AsciiString tmp;
				Matrix3D mtx;

				tmp.format("Runway%dPark%dHan",col+1,row+1);
				getObject()->getSingleLogicalBonePosition(tmp.str(), &info.m_hangarStart, &mtx);
				info.m_hangarStartOrient = mtx.Get_Z_Rotation();

				tmp.format("Runway%dParking%d",col+1,row+1);
				getObject()->getSingleLogicalBonePosition(tmp.str(), &info.m_location, &mtx);
				info.m_orientation = mtx.Get_Z_Rotation();

				tmp.format("Runway%dPrep%d",col+1,row+1);
				getObject()->getSingleLogicalBonePosition(tmp.str(), &info.m_prep, nullptr);

				info.m_runway = col;
				info.m_door = (ExitDoorType)door++;
				info.m_objectInSpace = INVALID_ID;
				info.m_reservedForExit = false;
				if (pu)
					pu->setHoldDoorOpen(info.m_door, false);

				m_spaces.push_back(info);
			}
		}
	}

	if (d->m_hasRunways)
	{
		RunwayInfo info;
		m_runways.reserve(d->m_numCols);
		for (Int col = 0; col < d->m_numCols; ++col)
		{
			AsciiString tmp;

			tmp.format("RunwayStart%d",col+1);
			getObject()->getSingleLogicalBonePosition(tmp.str(), &info.m_start, nullptr);

			tmp.format("RunwayEnd%d",col+1);
			getObject()->getSingleLogicalBonePosition(tmp.str(), &info.m_end, nullptr);

			info.m_inUseBy = INVALID_ID;
			info.m_nextInLineForTakeoff = INVALID_ID;
			info.m_wasInLine = false;
			m_runways.push_back(info);
		}
	}

	m_gotInfo = true;
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::purgeDead()
{
	buildInfo();

	ProductionUpdateInterface* pu = getObject()->getProductionUpdateInterface();
	{
		for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
		{
			if (it->m_objectInSpace != INVALID_ID)
			{
				Object* obj = TheGameLogic->findObjectByID(it->m_objectInSpace);
				if (obj == nullptr || obj->isEffectivelyDead())
				{
					it->m_objectInSpace = INVALID_ID;
					it->m_reservedForExit = false;
					it->m_postponedRunwayReservationForTakeoff = false;
					if (pu)
						pu->setHoldDoorOpen(it->m_door, false);
				}
			}
		}
	}

	{
		for (std::vector<RunwayInfo>::iterator it = m_runways.begin(); it != m_runways.end(); ++it)
		{
			if (it->m_inUseBy != INVALID_ID)
			{
				Object* obj = TheGameLogic->findObjectByID(it->m_inUseBy);
				if (obj == nullptr || obj->isEffectivelyDead())
				{
					it->m_inUseBy = INVALID_ID;
					it->m_wasInLine = false;
				}
			}
			if (it->m_nextInLineForTakeoff != INVALID_ID)
			{
				Object* obj = TheGameLogic->findObjectByID(it->m_nextInLineForTakeoff);
				if (obj == nullptr || obj->isEffectivelyDead())
				{
					it->m_nextInLineForTakeoff = INVALID_ID;
				}
			}
		}
	}

	{
		Bool anythingPurged = false;
		for (std::list<HealingInfo>::iterator it = m_healing.begin(); it != m_healing.end(); /*++it*/)
		{
			if (it->m_gettingHealedID != INVALID_ID)
			{
				Object* objToHeal = TheGameLogic->findObjectByID(it->m_gettingHealedID);
				if (objToHeal == nullptr || objToHeal->isEffectivelyDead())
				{
					it = m_healing.erase(it);
					anythingPurged = true;
				}
				else
				{
					++it;
				}
			}
		}
		if (anythingPurged)
			resetWakeFrame();
	}
}

//-------------------------------------------------------------------------------------------------
// note: called from client, so MUST NOT modify self in any way, or desyncs will occur
Bool ParkingPlaceBehavior::hasReservedSpace(ObjectID id) const
{
	if (!m_gotInfo)
		return false;

	if (id == INVALID_ID)	// shouldn't call this way, but Weapon mistakenly does sometimes, so check for it
		return false;

	for (std::vector<ParkingPlaceInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == id)
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Int ParkingPlaceBehavior::getSpaceIndex( ObjectID id ) const
{
	if( id == INVALID_ID )
	{
		return -1;
	}

	Int index = 0;
	for( std::vector<ParkingPlaceInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); it++, index++ )
	{
		if (it->m_objectInSpace == id)
		{
			return index;
		}
	}
	return -1;
}

//-------------------------------------------------------------------------------------------------
ParkingPlaceBehavior::ParkingPlaceInfo* ParkingPlaceBehavior::findPPI(ObjectID id)
{
	DEBUG_ASSERTCRASH(id != INVALID_ID, ("call findEmptyPPI instead"));

	if (!m_gotInfo || id == INVALID_ID)
		return nullptr;

	for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == id)
			return &(*it);
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
ParkingPlaceBehavior::ParkingPlaceInfo* ParkingPlaceBehavior::findEmptyPPI()
{
	if (!m_gotInfo)
		return nullptr;

	for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == INVALID_ID && it->m_reservedForExit == false)
			return &(*it);
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
// note: called from client, so MUST NOT modify self in any way, or desyncs will occur
Bool ParkingPlaceBehavior::shouldReserveDoorWhenQueued(const ThingTemplate* thing) const
{
	if (thing->isKindOf(KINDOF_PRODUCED_AT_HELIPAD))
		return false;

	return true;
}

//-------------------------------------------------------------------------------------------------
// note: called from client, so MUST NOT modify self in any way, or desyncs will occur
Bool ParkingPlaceBehavior::hasAvailableSpaceFor(const ThingTemplate* thing) const
{
	if (!m_gotInfo)	// degenerate case, shouldn't happen, but just in case...
		return false;

	if (thing->isKindOf(KINDOF_PRODUCED_AT_HELIPAD))
		return true;

	for (std::vector<ParkingPlaceInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		ObjectID id = it->m_objectInSpace;

		// since this is const, and we can't purge the dead safely, just peek and see if we have a dead thing.
		if (id != INVALID_ID)
		{
			Object* obj = TheGameLogic->findObjectByID(id);
			if (obj == nullptr || obj->isEffectivelyDead())
			{
				id = INVALID_ID;
			}
		}

		if (id == INVALID_ID && it->m_reservedForExit == false)
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ParkingPlaceBehavior::reserveSpace(ObjectID id, Real parkingOffset, ParkingPlaceBehaviorInterface::PPInfo* info)
{
	buildInfo();
	purgeDead();

	const ParkingPlaceBehaviorModuleData* d = getParkingPlaceBehaviorModuleData();

	ParkingPlaceInfo* ppi = findPPI(id);
	if (ppi == nullptr)
	{
		ppi = findEmptyPPI();
		if (ppi == nullptr)
		{
			DEBUG_CRASH(("No parking places!"));
			return false;	// nothing available
		}
	}

	ppi->m_objectInSpace = id;
	ppi->m_reservedForExit = false;

	if( d->m_landingDeckHeightOffset )
	{
		Object *obj = TheGameLogic->findObjectByID( id );
		if( obj )
		{
			obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_DECK_HEIGHT_OFFSET ) );
		}
	}

	if (info)
	{
		calcPPInfo( id, info );
		if (parkingOffset != 0.0f)
		{
			info->parkingSpace.x += parkingOffset * Cos(ppi->m_orientation);
			info->parkingSpace.y += parkingOffset * Sin(ppi->m_orientation);
		}
	}

	ProductionUpdateInterface* pu = getObject()->getProductionUpdateInterface();
	if (pu)
		pu->setHoldDoorOpen(ppi->m_door, true);

	return true;
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::calcPPInfo( ObjectID id, PPInfo *info )
{

	ParkingPlaceInfo* ppi = findPPI( id );
	if( !ppi )
	{
		//Utter failure.
		return;
	}
	const RunwayInfo& rr = m_runways[ ppi->m_runway ];

	if( info )
	{
		const ParkingPlaceBehaviorModuleData* d = getParkingPlaceBehaviorModuleData();
		info->parkingSpace = d->m_parkInHangars ? ppi->m_hangarStart : ppi->m_location;
		info->runwayPrep = ppi->m_prep;
		info->parkingOrientation = d->m_parkInHangars ? ppi->m_hangarStartOrient : ppi->m_orientation;
		info->runwayStart = rr.m_start;
		info->runwayEnd = rr.m_end;
		info->runwayApproach = rr.m_end;
		//const Real APPROACH_DIST = 0.5f;	// no, too short, planes may not have enough space to drop altitude
		const Real APPROACH_DIST = 0.75f;
		info->runwayApproach.x += (rr.m_end.x - rr.m_start.x) * APPROACH_DIST;
		info->runwayApproach.y += (rr.m_end.y - rr.m_start.y) * APPROACH_DIST;
		info->runwayApproach.z = rr.m_end.z + d->m_approachHeight + d->m_landingDeckHeightOffset;
		info->runwayExit = info->runwayApproach;
		info->hangarInternal = ppi->m_hangarStart;
		info->hangarInternalOrient = ppi->m_hangarStartOrient;

		//Cache the runway's takeoff distance used by JetAIUpdate for calculating lift.
		Coord3D vector = info->runwayStart;
		vector.sub( info->runwayEnd );
		info->runwayTakeoffDist = vector.length();

		for (std::vector<RunwayInfo>::iterator it = m_runways.begin(); it != m_runways.end(); ++it)
		{
			if (it->m_inUseBy == id)
			{
				if (it->m_wasInLine)
				{
					info->runwayStart = info->runwayPrep;
				}
				break;
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::releaseSpace(ObjectID id)
{
	buildInfo();
	purgeDead();

	ProductionUpdateInterface* pu = getObject()->getProductionUpdateInterface();
	for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == id)
		{
			it->m_objectInSpace = INVALID_ID;
			it->m_reservedForExit = false;
			it->m_postponedRunwayReservationForTakeoff = false;
			if (pu)
				pu->setHoldDoorOpen(it->m_door, false);
			break;
		}
	}

	Object *obj = TheGameLogic->findObjectByID( id );
	if( obj )
	{
		obj->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_DECK_HEIGHT_OFFSET ) );
	}

}

//-------------------------------------------------------------------------------------------------
Int ParkingPlaceBehavior::getRunwayIndex(ObjectID id)
{
	purgeDead();

	for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == id)
		{
			return it->m_runway;
		}
	}
	return InvalidRunway;
}

//-------------------------------------------------------------------------------------------------
ObjectID ParkingPlaceBehavior::getRunwayReservation( Int runway, RunwayReservationType type )
{
	//Note: We don't care about type because these runways share the runway for taking off and landing.
	buildInfo();
	purgeDead();
	return m_runways[runway].m_inUseBy;
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::transferRunwayReservationToNextInLineForTakeoff(ObjectID id)
{
	buildInfo();
	purgeDead();
	for (std::vector<RunwayInfo>::iterator it = m_runways.begin(); it != m_runways.end(); ++it)
	{
		if (it->m_inUseBy == id)
		{
			if (it->m_nextInLineForTakeoff != INVALID_ID)
			{
				it->m_inUseBy = it->m_nextInLineForTakeoff;
				it->m_wasInLine = true;
				it->m_nextInLineForTakeoff = INVALID_ID;
			}
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool ParkingPlaceBehavior::postponeRunwayReservation(UnsignedInt spaceIndex, Bool forLanding)
{
	// TheSuperHackers @tweak Block the first attempt to reserve a runway for 'upper' space indices.
	// This allows 'lower' space indices to reserve a runway first to ensure deterministic takeoff ordering.

	if (m_spaces.size() > m_runways.size() && spaceIndex >= m_runways.size())
	{
		Bool& postponed = m_spaces[spaceIndex].m_postponedRunwayReservationForTakeoff;
		if (forLanding)
		{
			postponed = false;
		}
		else if (!postponed)
		{
			postponed = true;
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ParkingPlaceBehavior::reserveRunway(ObjectID id, Bool forLanding)
{
	buildInfo();
	purgeDead();

	Int runway = -1;
	for (UnsignedInt i = 0; i < m_spaces.size(); ++i)
	{
		if (m_spaces[i].m_objectInSpace == id)
		{
#if !RETAIL_COMPATIBLE_CRC
			if (postponeRunwayReservation(i, forLanding))
				return false;
#endif

			runway = m_spaces[i].m_runway;
			break;
		}
	}

	if (runway == -1)
	{
		DEBUG_CRASH(("only planes with reserved spaces can reserve runways"));
		return false;
	}

	RunwayInfo& info = m_runways[runway];
	if (info.m_inUseBy == id)
	{
		return true;
	}
	else if (info.m_inUseBy == INVALID_ID)
	{
		info.m_inUseBy = id;

		if (info.m_nextInLineForTakeoff == id)
		{
			info.m_nextInLineForTakeoff = INVALID_ID;
			info.m_wasInLine = true;
		}
		else
		{
			info.m_wasInLine = false;
		}

		return true;
	}
	else if (!forLanding && info.m_nextInLineForTakeoff == INVALID_ID)
	{
		info.m_nextInLineForTakeoff = id;
		return false;	// yes, that's right
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::releaseRunway(ObjectID id)
{
	buildInfo();
	purgeDead();

	for (std::vector<RunwayInfo>::iterator it = m_runways.begin(); it != m_runways.end(); ++it)
	{
		if (it->m_inUseBy == id)
		{
			it->m_inUseBy = INVALID_ID;
			it->m_wasInLine = false;
		}
		if (it->m_nextInLineForTakeoff == id)
		{
			it->m_nextInLineForTakeoff = INVALID_ID;
		}
	}
}

//-------------------------------------------------------------------------------------------------
// don't really need to autoheal every frame....
const Int HEAL_RATE_FRAMES = LOGICFRAMES_PER_SECOND / 5;

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::resetWakeFrame()
{
	if (m_healing.empty())
	{
		m_nextHealFrame = FOREVER;
	}
	else
	{
		m_nextHealFrame = TheGameLogic->getFrame() + HEAL_RATE_FRAMES;
	}
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::setHealee(Object* healee, Bool add)
{
	if (add)
	{
		for (std::list<HealingInfo>::const_iterator it = m_healing.begin(); it != m_healing.end(); ++it)
		{
			if (it->m_gettingHealedID == healee->getID())
				return;
		}
		HealingInfo info;
		info.m_gettingHealedID = healee->getID();
		info.m_healStartFrame = TheGameLogic->getFrame();
		m_healing.push_back(info);
		resetWakeFrame();
	}
	else
	{
		for (std::list<HealingInfo>::iterator it = m_healing.begin(); it != m_healing.end(); /*++it*/)
		{
			if (it->m_gettingHealedID == healee->getID())
			{
				it = m_healing.erase(it);
				resetWakeFrame();
			}
			else
			{
				++it;
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::defectAllParkedUnits(Team* newTeam, UnsignedInt detectionTime)
{
	buildInfo();
	purgeDead();

	for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace != INVALID_ID)
		{
			Object* obj = TheGameLogic->findObjectByID(it->m_objectInSpace);
			if (obj == nullptr || obj->isEffectivelyDead())
				continue;

			// srj sez: evil. fix better someday.
			static NameKeyType jetKey = TheNameKeyGenerator->nameToKey("JetAIUpdate");
			JetAIUpdate* ju = (JetAIUpdate *)obj->findUpdateModule(jetKey);
			Bool takeoffOrLanding = ju ? ju->friend_isTakeoffOrLandingInProgress() : false;

			if (obj->isAboveTerrain() && !takeoffOrLanding)
			{
				// if the new team is a different controlling player, this guys loses his space.
				if (newTeam->getControllingPlayer() != obj->getControllingPlayer())
				{
					releaseSpace(obj->getID());
					if (obj->getProducerID() == getObject()->getID())
						obj->setProducer(nullptr);
				}
			}
			else
			{
				obj->defect(newTeam, detectionTime);
			}
		}
	}

	purgeDead();
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::killAllParkedUnits()
{
	buildInfo();
	purgeDead();

	for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace != INVALID_ID)
		{
			Object* obj = TheGameLogic->findObjectByID(it->m_objectInSpace);
			if (obj == nullptr || obj->isEffectivelyDead())
				continue;

			// srj sez: evil. fix better someday.
			static NameKeyType jetKey = TheNameKeyGenerator->nameToKey("JetAIUpdate");
			JetAIUpdate* ju = (JetAIUpdate *)obj->findUpdateModule(jetKey);
			Bool takeoffOrLanding = ju ? ju->friend_isTakeoffOrLandingInProgress() : false;

			if (obj->isAboveTerrain() && !takeoffOrLanding)
				continue;

			obj->kill();
		}
	}

	purgeDead();
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::onDie( const DamageInfo *damageInfo )
{
	killAllParkedUnits();
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ParkingPlaceBehavior::update()
{
	// alas, we need to keep the buildInfo and dead-purged stuff pretty much up to date, for
	// the client to be able to peek at. at this late date, the most expedient way is to ensure
	// our update is run every frame, and do this manually. the extra cost should be trivial, since
	// there are generally at most only a few airfields at any given time.
	buildInfo();
	purgeDead();

	UnsignedInt now = TheGameLogic->getFrame();
	if (now >= m_nextHealFrame)
	{
		m_nextHealFrame = now + HEAL_RATE_FRAMES;
		const ParkingPlaceBehaviorModuleData* d = getParkingPlaceBehaviorModuleData();
		for (std::list<HealingInfo>::iterator it = m_healing.begin(); it != m_healing.end(); /*++it*/)
		{
			if (it->m_gettingHealedID != INVALID_ID)
			{
				Object* objToHeal = TheGameLogic->findObjectByID(it->m_gettingHealedID);
				if (objToHeal == nullptr || objToHeal->isEffectivelyDead())
				{
					it = m_healing.erase(it);
				}
				else
				{
					DamageInfo healInfo;
					healInfo.in.m_damageType = DAMAGE_HEALING;
					healInfo.in.m_deathType = DEATH_NONE;
					healInfo.in.m_sourceID = getObject()->getID();
  				healInfo.in.m_amount = HEAL_RATE_FRAMES * d->m_healAmount * SECONDS_PER_LOGICFRAME_REAL;

//          if ( objToHeal->isKindOf( KINDOF_PRODUCED_AT_HELIPAD ) )
//            healInfo.in.m_amount += HEAL_RATE_FRAMES * d->m_extraHealAmount4Helicopters * SECONDS_PER_LOGICFRAME_REAL;

					BodyModuleInterface *body = objToHeal->getBodyModule();
					body->attemptHealing( &healInfo );
					++it;
				}
			}
		}
	}

	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
ExitDoorType ParkingPlaceBehavior::reserveDoorForExit( const ThingTemplate* objType, Object *specificObject )
{
	buildInfo();
	purgeDead();

	if (objType->isKindOf(KINDOF_PRODUCED_AT_HELIPAD))
	{
		return DOOR_NONE_NEEDED;
	}

	// note that this never allocates a per-unit parking space, even if specificObject is null.
	// this is by design and is assumed by exitObjectViaDoor.
	ParkingPlaceInfo* ppi = findEmptyPPI();
	if (ppi)
	{
		ppi->m_objectInSpace = INVALID_ID;
		ppi->m_reservedForExit = true;
		return ppi->m_door;
	}

	return DOOR_NONE_AVAILABLE;
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::exitObjectViaDoor( Object *newObj, ExitDoorType exitDoor ) ///< Here is the thing I want you to exit
{
	if (exitDoor != DOOR_NONE_NEEDED)
	{
		ParkingPlaceInfo* ppi = nullptr;
		for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
		{
			if (it->m_objectInSpace == INVALID_ID && it->m_reservedForExit == TRUE && it->m_door == exitDoor)
			{
				ppi = &(*it);
				break;
			}
		}

		if (!ppi)
		{
			DEBUG_CRASH(("could not find the space. what?"));
			return;
		}

		ppi->m_objectInSpace = newObj->getID();
		ppi->m_reservedForExit = false;
	}

	/// @todo srj -- this is evil. fix.
	static NameKeyType jetKey = TheNameKeyGenerator->nameToKey( "JetAIUpdate" );
	JetAIUpdate* ju = (JetAIUpdate *)newObj->findUpdateModule( jetKey );
	Real parkingOffset = ju ? ju->friend_getParkingOffset() : 0.0f;
	Bool producedAtHelipad = newObj->isKindOf(KINDOF_PRODUCED_AT_HELIPAD);

	PPInfo ppinfo;
	DUMPMATRIX3D(getObject()->getTransformMatrix());
	DUMPCOORD3D(getObject()->getPosition());
	if (producedAtHelipad)
	{
		CRCDEBUG_LOG(("Produced at helipad (door = %d)", exitDoor));
		DEBUG_ASSERTCRASH(exitDoor == DOOR_NONE_NEEDED, ("Hmm, unlikely"));
		Matrix3D mtx;
		MAYBE_UNUSED Bool boneOk = getObject()->getSingleLogicalBonePosition("HeliPark01", &ppinfo.hangarInternal, &mtx);
		(void)boneOk;

		DEBUG_ASSERTCRASH(boneOk, ("Could not get bone!"));
		ppinfo.hangarInternalOrient = mtx.Get_Z_Rotation();
		ppinfo.parkingSpace = ppinfo.hangarInternal;
		ppinfo.parkingOrientation = ppinfo.hangarInternalOrient;
	}
	else
	{
		CRCDEBUG_LOG(("Produced at hangar (door = %d)", exitDoor));
		DEBUG_ASSERTCRASH(exitDoor != DOOR_NONE_NEEDED, ("Hmm, unlikely"));
		if (!reserveSpace(newObj->getID(), parkingOffset, &ppinfo)) //&loc, &orient, nullptr, nullptr, nullptr, nullptr, &hangarInternal, &hangOrient))
		{
			DEBUG_CRASH(("no spaces available, how did we get here?"));
			ppinfo.parkingSpace = *getObject()->getPosition();
			ppinfo.parkingOrientation = getObject()->getOrientation();
		}
	}
	DUMPCOORD3D(&ppinfo.hangarInternal);
	DUMPREAL(ppinfo.hangarInternalOrient);
	DUMPCOORD3D(&ppinfo.parkingSpace);
	DUMPREAL(ppinfo.parkingOrientation);

	newObj->setPosition( &ppinfo.hangarInternal );
	newObj->setOrientation( ppinfo.hangarInternalOrient );
	TheAI->pathfinder()->addObjectToPathfindMap( newObj );

	AIUpdateInterface  *ai = newObj->getAIUpdateInterface();
	if( ai )
	{
		Bool movedToRallyPoint = FALSE;
		if( producedAtHelipad )
		{
			const Coord3D *rallyPoint = getRallyPoint();
			if( rallyPoint )
			{
				ai->aiMoveToPosition( rallyPoint, CMD_FROM_AI );
				movedToRallyPoint = TRUE;
			}
		}
		if( !movedToRallyPoint )
		{
			if( !newObj->isKindOf( KINDOF_PRODUCED_AT_HELIPAD ) )
			{
				std::vector<Coord3D> exitPath;
				exitPath.push_back(ppinfo.parkingSpace);
				ai->aiFollowExitProductionPath( &exitPath, getObject(), CMD_FROM_AI );
			}
			else
			{
				// Lorenzen sez: aiMoveToPosition has an added benefit.
				// It invokes the pathfinder to find a vacant destination.
	      ai->aiMoveToPosition( &ppinfo.parkingSpace, CMD_FROM_AI );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::unreserveDoorForExit( ExitDoorType exitDoor )
{
	if (exitDoor != DOOR_NONE_NEEDED)
	{
		for (std::vector<ParkingPlaceInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
		{
			if (it->m_door == exitDoor)
			{
				//DEBUG_ASSERTCRASH(it->m_reservedForExit, ("ParkingPlaceBehavior::unreserveDoorForExit: door %d was not reserved",exitDoor));
				it->m_objectInSpace = INVALID_ID;
				it->m_reservedForExit = false;
				return;
			}
		}

		DEBUG_CRASH(("ParkingPlaceBehavior::unreserveDoorForExit: door %d was not found",exitDoor));
	}
}

//-------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::setRallyPoint( const Coord3D *pos )
{
	m_heliRallyPointExists = TRUE;
	m_heliRallyPoint.set( *pos );
	// nothing
}

//-------------------------------------------------------------------------------------------------
const Coord3D* ParkingPlaceBehavior::getRallyPoint() const
{
	if( m_heliRallyPointExists )
	{
		return &m_heliRallyPoint;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
//We only use this for the helipad -- therefore the exit position is the helipad creation point.
//-------------------------------------------------------------------------------------------------
Bool ParkingPlaceBehavior::getExitPosition( Coord3D& exitPosition ) const
{
	Matrix3D mtx;
	return getObject()->getSingleLogicalBonePosition("HeliPark01", &exitPosition, &mtx );
}

//-------------------------------------------------------------------------------------------------
Bool ParkingPlaceBehavior::getNaturalRallyPoint( Coord3D& rallyPoint, Bool offset ) const
{
	Matrix3D mtx;
	return getObject()->getSingleLogicalBonePosition("HeliPark01", &rallyPoint, &mtx );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::xfer( Xfer *xfer )
{
	Int i;

	// version
	const XferVersion currentVersion = 3;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	if( xfer->getXferMode() == XFER_LOAD )
	{
		// first, build our info, so it won't be overwritten later.
		buildInfo();
	}

	// spaces info count and data
	UnsignedByte spacesCount = m_spaces.size();
	xfer->xferUnsignedByte( &spacesCount );
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// save all elements
		std::vector< ParkingPlaceInfo >::iterator it;
		for( it = m_spaces.begin(); it != m_spaces.end(); ++it )
		{

			// object in this space
			xfer->xferObjectID( &((*it).m_objectInSpace) );

			// reserved for exit
			xfer->xferBool( &((*it).m_reservedForExit) );

		}

	}
	else if( xfer->getXferMode() == XFER_LOAD )
	{
		ObjectID objectID;
		Bool reservedForExit;

		// read all elements
		std::vector< ParkingPlaceInfo >::iterator it;
		it = m_spaces.begin();
		for( i = 0; i < spacesCount; ++i )
		{

			// read object id
			xfer->xferObjectID( &objectID );

			// read reserved flag
			xfer->xferBool( &reservedForExit );

			// store in vector if the vector does indeed still have room for this entry
			if( it != m_spaces.end() )
			{

				(*it).m_objectInSpace = objectID;
				(*it).m_reservedForExit = reservedForExit;
				++it;

			}

		}

	}

	// runways could and info
	UnsignedByte runwaysCount = m_runways.size();
	xfer->xferUnsignedByte( &runwaysCount );
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// save all elements
		std::vector< RunwayInfo >::iterator it;
		for( it = m_runways.begin(); it != m_runways.end(); ++it )
		{

			// save object ID
			xfer->xferObjectID( &((*it).m_inUseBy) );
			xfer->xferObjectID( &((*it).m_nextInLineForTakeoff) );
			xfer->xferBool( &((*it).m_wasInLine) );

		}

	}
	else if( xfer->getXferMode() == XFER_LOAD )
	{
		// read all elements
		std::vector< RunwayInfo >::iterator it;
		it = m_runways.begin();
		for( i = 0; i < runwaysCount; ++i )
		{

			ObjectID inUseBy;
			ObjectID nextInLineForTakeoff;
			Bool wasInLine;

			// read object ID
			xfer->xferObjectID( &inUseBy );
			xfer->xferObjectID( &nextInLineForTakeoff );
			xfer->xferBool( &wasInLine );

			// store in vector if the vector does indeed still have room for this entry
			if( it != m_runways.end() )
			{

				(*it).m_inUseBy = inUseBy;
				(*it).m_nextInLineForTakeoff = nextInLineForTakeoff;
				(*it).m_wasInLine = wasInLine;
				++it;

			}

		}

	}

	// healees
	UnsignedByte healCount = m_healing.size();
	xfer->xferUnsignedByte( &healCount );
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// save all elements
		std::list< HealingInfo >::iterator it;
		for( it = m_healing.begin(); it != m_healing.end(); ++it )
		{

			// save object ID
			xfer->xferObjectID( &((*it).m_gettingHealedID) );
			xfer->xferUnsignedInt( &((*it).m_healStartFrame) );

		}

	}
	else if( xfer->getXferMode() == XFER_LOAD )
	{
		// read all elements
		m_healing.clear();
		for( i = 0; i < healCount; ++i )
		{
			HealingInfo info;

			// read object ID
			xfer->xferObjectID( &info.m_gettingHealedID );
			xfer->xferUnsignedInt( &info.m_healStartFrame );
			m_healing.push_back(info);

		}

	}

	if (version >= 2)
	{
		xfer->xferCoord3D(&m_heliRallyPoint);
		xfer->xferBool(&m_heliRallyPointExists);
	}

	if (version >= 3)
	{
		xfer->xferUnsignedInt(&m_nextHealFrame);
	}
	else
	{
		// old file .. just wake it up right away, and it will fall into the correct pattern
		if (xfer->getXferMode() == XFER_LOAD)
		{
			m_nextHealFrame = 0;
		}
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ParkingPlaceBehavior::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

	// no, this is bad.. it is NOT SAFE to call setWakeFrame from the xfer system. crap. (srj)
	// make sure we are awake... old save games let us sleep
	//setWakeFrame(getObject(), UPDATE_SLEEP_NONE);

}
