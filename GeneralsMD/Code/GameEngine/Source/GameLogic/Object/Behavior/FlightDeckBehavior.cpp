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

// FILE: FlightDeckBehavior.cpp ///////////////////////////////////////////////////////////////////
// Author: Kris Morness, May 2003
// Desc: Handles aircraft movement and parking behavior for aircraft carriers.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/CRCDebug.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"

#include "GameClient/Drawable.h"
#include "GameClient/ParticleSys.h"

#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

#include "GameLogic/Module/FlightDeckBehavior.h"
#include "GameLogic/Module/JetAIUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"


FlightDeckBehaviorModuleData::FlightDeckBehaviorModuleData()
{
	//m_framesForFullHeal = 0;
	m_healAmount = 0;
	m_numRows = 0;
	m_numCols = 0;
	m_approachHeight = 0.0f;
	m_landingDeckHeightOffset = 0.0f;
	m_dockAnimationFrames = 0;
	m_catapultFireFrames = 0;
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehaviorModuleData::parseRunwayStrip( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	AsciiString *runwayNames = (AsciiString*)store;

	const char *token = ini->getNextTokenOrNull();
	if( token )
	{
		runwayNames[ RUNWAY_START_BONE ].format( token );
		token = ini->getNextTokenOrNull();
		if( token )
		{
			runwayNames[ RUNWAY_END_BONE ].format( token );
		}
	}
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	AIUpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "NumRunways",							INI::parseInt,										nullptr, offsetof( FlightDeckBehaviorModuleData, m_numCols ) },
		{ "NumSpacesPerRunway",			INI::parseInt,										nullptr, offsetof( FlightDeckBehaviorModuleData, m_numRows ) },

		{ "Runway1Spaces",					INI::parseAsciiStringVector,			nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 0 ].m_spacesBoneNames ) },
		{ "Runway1Takeoff",					parseRunwayStrip,									nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 0 ].m_takeoffBoneNames ) },
		{ "Runway1Landing",					parseRunwayStrip,									nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 0 ].m_landingBoneNames ) },
		{ "Runway1Taxi",						INI::parseAsciiStringVector,			nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 0 ].m_taxiBoneNames ) },
		{ "Runway1Creation",				INI::parseAsciiStringVector,			nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 0 ].m_creationBoneNames ) },
		{ "Runway1CatapultSystem",	INI::parseParticleSystemTemplate,	nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 0 ].m_catapultParticleSystem ) },

		{ "Runway2Spaces",					INI::parseAsciiStringVector,			nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 1 ].m_spacesBoneNames ) },
		{ "Runway2Takeoff",					parseRunwayStrip,									nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 1 ].m_takeoffBoneNames ) },
		{ "Runway2Landing",					parseRunwayStrip,									nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 1 ].m_landingBoneNames ) },
		{ "Runway2Taxi",						INI::parseAsciiStringVector,			nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 1 ].m_taxiBoneNames ) },
		{ "Runway2Creation",				INI::parseAsciiStringVector,			nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 1 ].m_creationBoneNames ) },
		{ "Runway2CatapultSystem",	INI::parseParticleSystemTemplate,	nullptr, offsetof( FlightDeckBehaviorModuleData, m_runwayInfo[ 1 ].m_catapultParticleSystem ) },

		{ "ApproachHeight",					INI::parseReal,										nullptr, offsetof( FlightDeckBehaviorModuleData, m_approachHeight ) },
		{ "LandingDeckHeightOffset",INI::parseReal,										nullptr, offsetof( FlightDeckBehaviorModuleData, m_landingDeckHeightOffset ) },
		{ "HealAmountPerSecond",		INI::parseReal,										nullptr, offsetof( FlightDeckBehaviorModuleData, m_healAmount ) },
		{ "ParkingCleanupPeriod",		INI::parseDurationUnsignedInt,		nullptr, offsetof( FlightDeckBehaviorModuleData, m_cleanupFrames ) },
		{ "HumanFollowPeriod",			INI::parseDurationUnsignedInt,		nullptr, offsetof( FlightDeckBehaviorModuleData, m_humanFollowFrames ) },
		{ "PayloadTemplate",				INI::parseAsciiString,				  	nullptr, offsetof( FlightDeckBehaviorModuleData, m_thingTemplateName ) },
		{ "ReplacementDelay",				INI::parseDurationUnsignedInt, 		nullptr, offsetof( FlightDeckBehaviorModuleData, m_replacementFrames ) },
		{ "DockAnimationDelay",			INI::parseDurationUnsignedInt,		nullptr, offsetof( FlightDeckBehaviorModuleData, m_dockAnimationFrames ) },
		{ "LaunchWaveDelay",				INI::parseDurationUnsignedInt,		nullptr, offsetof( FlightDeckBehaviorModuleData, m_launchWaveFrames ) },
		{ "LaunchRampDelay",				INI::parseDurationUnsignedInt,		nullptr, offsetof( FlightDeckBehaviorModuleData, m_launchRampFrames ) },
		{ "LowerRampDelay",					INI::parseDurationUnsignedInt,		nullptr, offsetof( FlightDeckBehaviorModuleData, m_lowerRampFrames ) },
		{ "CatapultFireDelay",			INI::parseDurationUnsignedInt,		nullptr, offsetof( FlightDeckBehaviorModuleData, m_catapultFireFrames ) },

		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FlightDeckBehavior::FlightDeckBehavior( Thing *thing, const ModuleData* moduleData ) : AIUpdateInterface( thing, moduleData )
{
	m_gotInfo = false;

	m_nextHealFrame = FOREVER;
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
	m_nextCleanupFrame = 0;
	m_startedProductionFrame = FOREVER;
	m_nextAllowedProductionFrame = 0;
	m_designatedTarget = INVALID_ID;
	m_designatedCommand = AICMD_NO_COMMAND;
	m_designatedPosition.zero();

	for( int i = 0; i < MAX_RUNWAYS; i++ )
	{
		m_nextLaunchWaveFrame[ i ] = 0;
		m_rampUpFrame[ i ] = 0;
		m_catapultSystemFrame[ i ] = 0;
		m_lowerRampFrame[ i ] = 0;
		m_rampUp[ i ] = FALSE;
	}

  m_thingTemplate = nullptr;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FlightDeckBehavior::~FlightDeckBehavior()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::buildInfo(Bool createUnits)
{
	if (m_gotInfo)
		return;

	const FlightDeckBehaviorModuleData* data = getFlightDeckBehaviorModuleData();

	m_thingTemplate = TheThingFactory->findTemplate( data->m_thingTemplateName );

	if (getObject()->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ||
			getObject()->testStatus(OBJECT_STATUS_SOLD))
		return;

	//ProductionUpdateInterface* pu = getObject()->getProductionUpdateInterface();

	m_spaces.reserve( data->m_numRows * data->m_numCols );

	//Initialize the spaces that planes will eventually be assigned to for parking purposes
	FlightDeckInfo flightDeckInfo;

	//We want to sort the spaces so that we have runway 1 space 1, runway 2 space 1, R1S2, R2S2, R1S3...
	for( Int row = 0; row < data->m_numRows; row++ )
	{
		for( Int col = 0; col < data->m_numCols; col++ )
		{
			std::vector<AsciiString> spaces = data->m_runwayInfo[ col ].m_spacesBoneNames;
			std::vector<AsciiString>::const_iterator it;

			Int counter = 0;
			for( it = spaces.begin(); it != spaces.end() && counter < row; it++, counter++ )
			{
				//just iterate to the spaces.
			}

			AsciiString tmp;
			Matrix3D mtx;

			//Convert the module data bone names into coordinates that we can use
			getObject()->getSingleLogicalBonePosition( it->str(), &flightDeckInfo.m_prep, &mtx );
			flightDeckInfo.m_orientation = mtx.Get_Z_Rotation();

			//Init basic runway stuff
			flightDeckInfo.m_runway = col;
			flightDeckInfo.m_objectInSpace = INVALID_ID;

			//Create the payload
			Object *jet = nullptr;
			if( m_thingTemplate && createUnits )
			{
				jet = TheThingFactory->newObject( m_thingTemplate, getObject()->getControllingPlayer()->getDefaultTeam() );
				if( jet )
				{
					jet->setProducer( getObject() );
					jet->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_DECK_HEIGHT_OFFSET ) );

					//Init positioning.
					jet->setPosition( &flightDeckInfo.m_prep );
					jet->setOrientation( flightDeckInfo.m_orientation );

					//Assign jet to space
					flightDeckInfo.m_objectInSpace = jet->getID();
					//validateAssignments();
				}
			}

			m_spaces.push_back( flightDeckInfo );
		}
	}

	//Now initialize the runway take-off and landing information.
	RunwayInfo info;
	m_runways.reserve(data->m_numCols);
	for( Int col = 0; col < data->m_numCols; ++col )
	{
		AsciiString tmp;

		getObject()->getSingleLogicalBonePosition( data->m_runwayInfo[ col ].m_takeoffBoneNames[ RUNWAY_START_BONE ].str(), &info.m_start, nullptr);
		getObject()->getSingleLogicalBonePosition( data->m_runwayInfo[ col ].m_takeoffBoneNames[ RUNWAY_END_BONE ].str(), &info.m_end, nullptr);
		getObject()->getSingleLogicalBonePosition( data->m_runwayInfo[ col ].m_landingBoneNames[ RUNWAY_START_BONE ].str(), &info.m_landingStart, nullptr);
		getObject()->getSingleLogicalBonePosition( data->m_runwayInfo[ col ].m_landingBoneNames[ RUNWAY_END_BONE ].str(), &info.m_landingEnd, nullptr);

		info.m_inUseByForTakeoff = INVALID_ID;
		info.m_inUseByForLanding = INVALID_ID;

		//Get the taxi bones and store them as well (possible to have none!)
		std::vector<AsciiString> locations = data->m_runwayInfo[ col ].m_taxiBoneNames;
		std::vector<AsciiString>::const_iterator it;
		info.m_taxi.clear();
		for( it = locations.begin(); it != locations.end(); it++ )
		{
			Coord3D taxiPos;
			Matrix3D mtx;

			//Get the position of the taxi bone.
			getObject()->getSingleLogicalBonePosition( it->str(), &taxiPos, &mtx );

			//Add it to the taxi vector
			info.m_taxi.push_back( taxiPos );
		}

		//Get the creation bones and store them as well
		locations = data->m_runwayInfo[ col ].m_creationBoneNames;
		info.m_creation.clear();
		Bool firstTime = TRUE;
		for( it = locations.begin(); it != locations.end(); it++ )
		{
			Coord3D pos;
			Matrix3D mtx;

			//Get the position of the creation bone.
			getObject()->getSingleLogicalBonePosition( it->str(), &pos, &mtx );

			if( firstTime )
			{
				firstTime = FALSE;
				info.m_startOrient = mtx.Get_Z_Rotation();
				info.m_startTransform = mtx;
			}

			//Add it to the taxi vector
			info.m_creation.push_back( pos );
		}

		m_runways.push_back(info);
	}

	m_gotInfo = true;
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::purgeDead()
{
	buildInfo();

	for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace != INVALID_ID)
		{
			Object* obj = TheGameLogic->findObjectByID(it->m_objectInSpace);
			if (obj == nullptr || obj->isEffectivelyDead())
			{
				it->m_objectInSpace = INVALID_ID;
			}
		}
	}

	{
		for (std::vector<RunwayInfo>::iterator it = m_runways.begin(); it != m_runways.end(); ++it)
		{
			if (it->m_inUseByForTakeoff != INVALID_ID)
			{
				Object* obj = TheGameLogic->findObjectByID(it->m_inUseByForTakeoff);
				if (obj == nullptr || obj->isEffectivelyDead())
				{
					it->m_inUseByForTakeoff = INVALID_ID;
				}
			}
			if (it->m_inUseByForLanding != INVALID_ID)
			{
				Object* obj = TheGameLogic->findObjectByID(it->m_inUseByForLanding);
				if (obj == nullptr || obj->isEffectivelyDead())
				{
					it->m_inUseByForLanding = INVALID_ID;
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
Bool FlightDeckBehavior::hasReservedSpace(ObjectID id) const
{
	if (!m_gotInfo)
		return false;

	if (id == INVALID_ID)	// shouldn't call this way, but Weapon mistakenly does sometimes, so check for it
		return false;

	for (std::vector<FlightDeckInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == id)
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Int FlightDeckBehavior::getSpaceIndex( ObjectID id ) const
{
	if( id == INVALID_ID )
	{
		return -1;
	}

	Int index = 0;
	for( std::vector<FlightDeckInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); it++, index++ )
	{
		if (it->m_objectInSpace == id)
		{
			return index;
		}
	}
	return -1;
}

//-------------------------------------------------------------------------------------------------
FlightDeckBehavior::FlightDeckInfo* FlightDeckBehavior::findPPI(ObjectID id)
{
	DEBUG_ASSERTCRASH(id != INVALID_ID, ("call findEmptyPPI instead"));

	if (!m_gotInfo || id == INVALID_ID)
		return nullptr;

	for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == id)
			return &(*it);
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
FlightDeckBehavior::FlightDeckInfo* FlightDeckBehavior::findEmptyPPI()
{
	if (!m_gotInfo)
		return nullptr;

	for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if( it->m_objectInSpace == INVALID_ID )
			return &(*it);
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
// note: called from client, so MUST NOT modify self in any way, or desyncs will occur
Bool FlightDeckBehavior::shouldReserveDoorWhenQueued(const ThingTemplate* thing) const
{
	return true;
}

//-------------------------------------------------------------------------------------------------
// note: called from client, so MUST NOT modify self in any way, or desyncs will occur
Bool FlightDeckBehavior::hasAvailableSpaceFor(const ThingTemplate* thing) const
{
	if (!m_gotInfo)	// degenerate case, shouldn't happen, but just in case...
		return false;

	for (std::vector<FlightDeckInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
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

		if( id == INVALID_ID )
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool FlightDeckBehavior::reserveSpace(ObjectID id, Real parkingOffset, ParkingPlaceBehaviorInterface::PPInfo* info)
{
	buildInfo();
	purgeDead();

	const FlightDeckBehaviorModuleData* d = getFlightDeckBehaviorModuleData();

	FlightDeckInfo* ppi = findPPI(id);
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
	//validateAssignments();

	if( d->m_landingDeckHeightOffset )
	{
		Object *obj = TheGameLogic->findObjectByID( id );
		if( obj )
		{
			obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_DECK_HEIGHT_OFFSET ) );
		}
	}

	if( info )
	{
		calcPPInfo( id, info );
		if (parkingOffset != 0.0f)
		{
			info->parkingSpace.x += parkingOffset * Cos(ppi->m_orientation);
			info->parkingSpace.y += parkingOffset * Sin(ppi->m_orientation);
		}
	}

	return true;
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::validateAssignments()
{
	Int index = 0, index2 = 0;
	for( std::vector<FlightDeckInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); it++, index++ )
	{
		ObjectID id = it->m_objectInSpace;
		if( id != INVALID_ID )
		{
			for( std::vector<FlightDeckInfo>::const_iterator it2 = it; it2 != m_spaces.end(); it2++, index2++ )
			{
				if( it == it2 )
				{
					continue;
				}
				ObjectID id2 = it2->m_objectInSpace;
				if( id == id2 )
				{
					DEBUG_CRASH( ("Aircraft %d assigned to multiple spaces %d and %d", id, index, index2 ) );
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::calcPPInfo( ObjectID id, PPInfo *info )
{
	FlightDeckInfo *ppi = findPPI( id );
	if( !ppi )
	{
		//Utter failure.
		return;
	}
	const RunwayInfo& rr = m_runways[ ppi->m_runway ];

	if( info )
	{
		const FlightDeckBehaviorModuleData* d = getFlightDeckBehaviorModuleData();
		const Real APPROACH_DIST = 0.75f;
		info->parkingSpace = ppi->m_prep;
		info->runwayPrep = ppi->m_prep;
		info->parkingOrientation = ppi->m_orientation;
		info->runwayStart = rr.m_start;
		info->runwayEnd = rr.m_end;
		info->runwayExit = rr.m_end;
		info->runwayExit.x += (rr.m_end.x - rr.m_start.x) * APPROACH_DIST;
		info->runwayExit.y += (rr.m_end.y - rr.m_start.y) * APPROACH_DIST;
		info->runwayExit.z = rr.m_end.z + d->m_approachHeight + d->m_landingDeckHeightOffset;

		info->runwayLandingStart = rr.m_landingStart;
		info->runwayLandingEnd = rr.m_landingEnd;
		info->runwayApproach = rr.m_landingStart;
		info->runwayApproach.x += (rr.m_landingStart.x - rr.m_landingEnd.x) * APPROACH_DIST;
		info->runwayApproach.y += (rr.m_landingStart.y - rr.m_landingEnd.y) * APPROACH_DIST;
		info->runwayApproach.z = rr.m_landingStart.z + d->m_approachHeight + d->m_landingDeckHeightOffset;

		//Cache the runway's takeoff distance used by JetAIUpdate for calculating lift.
		Coord3D vector = info->runwayStart;
		vector.sub( info->runwayEnd );
		info->runwayTakeoffDist = vector.length();

		for (std::vector<RunwayInfo>::iterator it = m_runways.begin(); it != m_runways.end(); ++it)
		{
			if (it->m_inUseByForTakeoff == id )
			{
				info->runwayStart = info->runwayPrep;
				break;
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::releaseSpace(ObjectID id)
{
	buildInfo();
	purgeDead();

	for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == id)
		{
			it->m_objectInSpace = INVALID_ID;
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
Int FlightDeckBehavior::getRunwayIndex(ObjectID id)
{
	purgeDead();

	for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
	{
		if (it->m_objectInSpace == id)
		{
			return it->m_runway;
		}
	}
	return InvalidRunway;
}

//-------------------------------------------------------------------------------------------------
ObjectID FlightDeckBehavior::getRunwayReservation( Int runway, RunwayReservationType type )
{
	buildInfo();
	purgeDead();
	switch( type )
	{
		case RESERVATION_TAKEOFF:
			return m_runways[runway].m_inUseByForTakeoff;
		case RESERVATION_LANDING:
			return m_runways[runway].m_inUseByForLanding;
		default:
			return INVALID_ID;
	}
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::transferRunwayReservationToNextInLineForTakeoff(ObjectID id)
{
	//Aircraft carrier controls this functionality with an iron fist.
}

//-------------------------------------------------------------------------------------------------
Bool FlightDeckBehavior::reserveRunway(ObjectID id, Bool forLanding)
{
	buildInfo();
	purgeDead();

	Int runway = -1;

	if( !forLanding )
	{
		//Only look at the front spaces for takeoff. You can't take off from the back!
		const FlightDeckBehaviorModuleData *data = getFlightDeckBehaviorModuleData();
		for( Int i = 0; i < data->m_numCols; i++ )
		{
			if( m_spaces[ i ].m_objectInSpace == id )
			{
				runway = m_spaces[ i ].m_runway;
				break;
			}
		}
	}
	else
	{
		//Look at all spaces for landing.
		for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
		{
			if (it->m_objectInSpace == id)
			{
				runway = it->m_runway;
				break;
			}
		}
	}
	if (runway == -1)
	{
		if( forLanding )
		{
			DEBUG_CRASH(("only planes with reserved spaces can reserve runways"));
		}
		return false;
	}

	RunwayInfo& info = m_runways[runway];
	if( (info.m_inUseByForTakeoff == id && !forLanding) || (info.m_inUseByForLanding == id && forLanding) )
	{
		return true;
	}
	else if( (info.m_inUseByForTakeoff == INVALID_ID && !forLanding) || (info.m_inUseByForLanding == INVALID_ID && forLanding) )
	{
		if( forLanding )
		{
			info.m_inUseByForLanding = id;
		}
		else
		{
			info.m_inUseByForTakeoff = id;
		}

		return true;
	}

	return false;
}


//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::releaseRunway(ObjectID id)
{
	buildInfo();
	purgeDead();

	for (std::vector<RunwayInfo>::iterator it = m_runways.begin(); it != m_runways.end(); ++it)
	{
		if( it->m_inUseByForTakeoff == id )
		{
			it->m_inUseByForTakeoff = INVALID_ID;
		}
		if( it->m_inUseByForLanding == id )
		{
			it->m_inUseByForLanding = INVALID_ID;
		}
	}
}

//-------------------------------------------------------------------------------------------------
const std::vector<Coord3D>* FlightDeckBehavior::getTaxiLocations( ObjectID id ) const
{
	//Find the runway the object is assigned to.
	Int runway = -1;
	for( std::vector<FlightDeckInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); it++ )
	{
		if( it->m_objectInSpace == id )
		{
			runway = it->m_runway;
			break;
		}
	}

	if( runway == -1 )
	{
		DEBUG_CRASH(("only planes with reserved spaces can reserve runways"));
		return nullptr;
	}

	//Now get the runway we're assigned to and return it's taxi vector
	return &(m_runways[ runway ].m_taxi);
}

//-------------------------------------------------------------------------------------------------
const std::vector<Coord3D>* FlightDeckBehavior::getCreationLocations( ObjectID id ) const
{
	//Find the runway the object is assigned to.
	Int runway = -1;
	for( std::vector<FlightDeckInfo>::const_iterator it = m_spaces.begin(); it != m_spaces.end(); it++ )
	{
		if( it->m_objectInSpace == id )
		{
			runway = it->m_runway;
			break;
		}
	}

	if( runway == -1 )
	{
		DEBUG_CRASH(("only planes with reserved spaces can reserve runways"));
		return nullptr;
	}

	//Now get the runway we're assigned to and return it's creation vector
	return &(m_runways[ runway ].m_creation);
}

//-------------------------------------------------------------------------------------------------
Bool FlightDeckBehavior::isAbleToGiveUpParkingSpace( Object *jet )
{
	//If we're airborne or non-existent, someone else can have my spot if they need it.
	if( !jet || jet->isAirborneTarget() )
	{
		return TRUE;
	}

	JetAIUpdate *ai = (JetAIUpdate*)jet->getAI();
	if( ai )
	{
		//If I'm not idle, I'm certainly not parked. But I could be on route to park in my space!
		if( !ai->isIdle() && !ai->isTaxiingToParking() )
		{
			//We're definitely not airborne, so if our pending command is to land at the carrier, nuke the pending
			//command.
			AICommandType command = ai->friend_getPendingCommandType();
			if( command == AICMD_ENTER || m_designatedCommand == AICMD_IDLE )
			{
				ai->friend_purgePendingCommand();
				return FALSE;
			}

			//But make sure we're not waiting to takeoff!
			std::vector< RunwayInfo >::const_iterator it;
			for( it = m_runways.begin(); it != m_runways.end(); it++ )
			{
				if( it->m_inUseByForTakeoff == jet->getID() )
				{
					return FALSE;
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool FlightDeckBehavior::isInPositionToTakeoff( const Object &jet ) const
{
	const AIUpdateInterface *ai = jet.getAI();
	const FlightDeckBehaviorModuleData *data = getFlightDeckBehaviorModuleData();
	if( ai )
	{
		for( int i = 0; i < data->m_numCols; i++ )
		{
			if( m_spaces[ i ].m_objectInSpace == jet.getID() )
			{
				//This code fixes a problem where there is a one frame lag between the
				//time a jet gets assigned to the front spot, and the time it's AI is able to
				//order it to taxi into the position. When this happens, the ramp triggers its
				//animation, and the jet drives through it. So to counter that, simply check
				//the distance between the jet and the space.
				Real distanceSqr = ThePartitionManager->getDistanceSquared( &jet, &m_spaces[ i ].m_prep, FROM_CENTER_2D );
				if( distanceSqr < 10.0f )
				{
					return TRUE;
				}
				return FALSE;
			}
		}
	}
	return FALSE;
}


//-------------------------------------------------------------------------------------------------
Bool FlightDeckBehavior::isAbleToMoveForward( const Object &jet ) const
{
	JetAIUpdate *jetAI = (JetAIUpdate*)jet.getAI();
	if( jetAI && !jet.isAirborneTarget() )
	{
		//We're not airborne.
		if( jetAI->isIdle() )
		{
			//We're idle (not moving)
			return TRUE;
		}

		//We might be rearming... if so, allow it.
		if( jetAI->isReloading() )
		{
			return TRUE;
		}
	}
	return FALSE;
}


//-------------------------------------------------------------------------------------------------
Bool FlightDeckBehavior::calcBestParkingAssignment( ObjectID id, Coord3D *pos, Int *oldIndex, Int *newIndex )
{
	//Find the runway the object is assigned to.
	Int runway = -1;
	Int myIndex = 0;
	std::vector<FlightDeckInfo>::iterator myIt = m_spaces.begin();
	for( ; myIt != m_spaces.end(); myIt++, myIndex++ )
	{
		if( myIt->m_objectInSpace == id )
		{
			runway = myIt->m_runway;

			if( pos )
			{
				pos->set( myIt->m_prep );
			}
			break;
		}
	}

	if( oldIndex )
	{
		*oldIndex = myIndex;
	}

	//Now iterate the runway again in search of an empty spot in front of it. If we reach the same
	//plane again, then we're done because there is no better spot!

	//Search for the front-most available space that doesn't have any planes blocking it. So start from
	//the back and keep looking at empty spaces until we find one with a plane blocking.

	Bool checkForPlaneInWay = FALSE;
	std::vector<FlightDeckInfo>::iterator bestIt = m_spaces.end();
	Object *bestJet = nullptr;
	Int bestIndex = 0, index = 0;
	for( std::vector<FlightDeckInfo>::iterator thatIt = m_spaces.begin(); thatIt != m_spaces.end(); thatIt++, index++ )
	{
		Object *nonIdleJet = TheGameLogic->findObjectByID( thatIt->m_objectInSpace );
		if( myIt == thatIt )
		{
			//Done, don't look at my spot, nor spots behind me.
			if( bestIt != m_spaces.end() )
			{
				myIt->m_objectInSpace = bestJet ? bestJet->getID() : INVALID_ID;
				bestIt->m_objectInSpace = id;
				//validateAssignments();

				/*if( bestJet )
				{
					JetAIUpdate *jetAI = (JetAIUpdate*)bestJet->getAI();
					reserveSpace( bestJet->getID(), jetAI->friend_getParkingOffset(), nullptr );
				}
				if( nonIdleJet )
				{
					JetAIUpdate *jetAI = (JetAIUpdate*)nonIdleJet->getAI();
					reserveSpace( nonIdleJet->getID(), jetAI->friend_getParkingOffset(), nullptr );
				}*/

				if( newIndex )
				{
					*newIndex = bestIndex;
				}

				//Promoted forward.
				return TRUE;
			}
			//Did not get promoted forward.
			return FALSE;
		}
		if( thatIt->m_runway == runway )
		{
			if( !nonIdleJet || isAbleToGiveUpParkingSpace( nonIdleJet ) )
			{
				if( !checkForPlaneInWay )
				{
					//We can take this spot! But first find the flight deck info entry for it.Now handle assignment swap.
					bestIt = thatIt;
					bestJet = nonIdleJet;
					bestIndex = index;
					checkForPlaneInWay = TRUE;
					if( pos )
					{
						pos->set( thatIt->m_prep );
					}
				}
			}
			else if( checkForPlaneInWay  )
			{
				//Ugh, there's a plane parked between us and the best spot!
 				checkForPlaneInWay = FALSE;
				if( pos )
				{
					pos->set( myIt->m_prep ); //reset the original position.
					bestIt = m_spaces.end();
				}
			}
		}
	}
	//Did not get promoted forward
	return FALSE;
}


//-------------------------------------------------------------------------------------------------
// don't really need to autoheal every frame....
const Int HEAL_RATE_FRAMES = LOGICFRAMES_PER_SECOND / 5;

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::resetWakeFrame()
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
void FlightDeckBehavior::setHealee(Object* healee, Bool add)
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
void FlightDeckBehavior::defectAllParkedUnits(Team* newTeam, UnsignedInt detectionTime)
{
	buildInfo();
	purgeDead();

	for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
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
void FlightDeckBehavior::killAllParkedUnits()
{
	buildInfo();
	purgeDead();

	for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
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
void FlightDeckBehavior::onDie( const DamageInfo *damageInfo )
{
	killAllParkedUnits();
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime FlightDeckBehavior::update()
{
	// alas, we need to keep the buildInfo and dead-purged stuff pretty much up to date, for
	// the client to be able to peek at. at this late date, the most expedient way is to ensure
	// our update is run every frame, and do this manually. the extra cost should be trivial, since
	// there are generally at most only a few airfields at any given time.
	buildInfo();
	purgeDead();

	const FlightDeckBehaviorModuleData* data = getFlightDeckBehaviorModuleData();
	UnsignedInt now = TheGameLogic->getFrame();
	if( now >= m_nextHealFrame )
	{
		m_nextHealFrame = now + HEAL_RATE_FRAMES;
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
					healInfo.in.m_amount = HEAL_RATE_FRAMES * data->m_healAmount * SECONDS_PER_LOGICFRAME_REAL;
					BodyModuleInterface *body = objToHeal->getBodyModule();
					body->attemptHealing( &healInfo );
					++it;
				}
			}
		}
	}

	//Periodically, we want to look at all the aircraft assignments and keep them near the front
	//of the carrier at all times. We do this with the following steps:
	//  1) Detect non-idle plane assigned to frontmost spaces (implying spot is empty)
	//  2) Promote next idle plane behind it to move up and take it's spot. The original plane will
	//     get assigned to it's old spot (thus bubblesorted eventually to the rear).
	if( now >= m_nextCleanupFrame )
	{
		m_nextCleanupFrame = now + data->m_cleanupFrames;
		std::vector<FlightDeckInfo>::iterator tempIt;
		Int spaceID = 0;

		Bool complete[ MAX_RUNWAYS ] = { FALSE, FALSE };
		for( std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); it++, spaceID++ )
		{
			Object *nonIdleJet = TheGameLogic->findObjectByID( it->m_objectInSpace );
			if( !nonIdleJet || isAbleToGiveUpParkingSpace( nonIdleJet ) )
			{
				//Either we don't have a jet, or the jet is busy (meaning it's not parked there). When a jet
				//isn't in his spot, we will look for jets behind him to move up to take up his spot.
				Int runwayCount = data->m_numCols;
				Int tempID = spaceID;
				for( tempIt = it; tempIt != m_spaces.end(); tempIt++, tempID++ )
				{
					if( runwayCount > 0 )
					{
						//Because the spaces are sorted Runway 1 Space 1, R2S1, R1S2, R2S2 etc, we simply iterate
						//every other space. If there were 3 or 4 runways, then we would jump the number of runways
						//before each check so we don't get aircraft from a different runway getting moved up!
						runwayCount--;
						continue;
					}

					if( complete[ it->m_runway ] )
					{
						continue;
					}

					//Now we have the correct runway. Check if that spot has an idle plane in it!
					Object *parkedJet = TheGameLogic->findObjectByID( tempIt->m_objectInSpace );
					if( parkedJet && isAbleToMoveForward( *parkedJet ) )
					{
						//We have found the best candidate to replace our empty space. Now handle assignment swap.
						it->m_objectInSpace = parkedJet->getID();
						tempIt->m_objectInSpace = nonIdleJet ? nonIdleJet->getID() : INVALID_ID;
						//validateAssignments();

						//Give the parkedJet a move order to taxi over to the new spot.
						//However, we need to set a status bit to tell the AI the difference between taxiing from
						//a hangar and reassigning a parking space.
						parkedJet->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_REASSIGN_PARKING ) );

						//Doesn't matter what we push in now, the JetOrHeliTaxiState::onEnter() will nuke it
						//and calculate the taxi point to move through.
						std::vector<Coord3D> exitPath;
						exitPath.push_back( it->m_prep );
						parkedJet->getAI()->aiFollowExitProductionPath( &exitPath, getObject(), CMD_FROM_AI );

						//Determine if the first plane in each row has completed a move. We want to lag the
						//additional planes instead of moving them all simultaneously. So this is the exit
						//condition if one plane per runway has been bumped.
						complete[ it->m_runway ] = TRUE;
						m_nextCleanupFrame = now + data->m_humanFollowFrames;
					}
					//Break through and advance to the next spot!
					break;
				}
			}
		}
	}

	//Handle automatic production of lost aircraft.

	//Reset timer if we just finished building an aircraft.
	if( m_nextAllowedProductionFrame <= now )
	{
		m_startedProductionFrame = FOREVER;
	}

	std::vector<FlightDeckInfo>::iterator it = m_spaces.begin();
	for( ; it != m_spaces.end(); it++ )
	{
		//Unassigned space?... so we can build a replacement.
		if( it->m_objectInSpace == INVALID_ID )
		{
			//But are we already building one?
			ProductionUpdateInterface *pu = getObject()->getProductionUpdateInterface();
			if( pu == nullptr )
			{

				DEBUG_CRASH( ("MSG_QUEUE_UNIT_CREATE: Producer '%s' doesn't have a unit production interface", getObject()->getTemplate()->getName().str()) );
				break;
			}
			DEBUG_ASSERTCRASH( m_thingTemplate != nullptr, ("flightdeck has a null thingtemplate... no jets for you!") );
			if( !pu->getProductionCount() && now >= m_nextAllowedProductionFrame && m_thingTemplate != nullptr )
			{
				//Queue the build
				pu->queueCreateUnit( m_thingTemplate, pu->requestUniqueUnitID() );

				m_startedProductionFrame = now;
				m_nextAllowedProductionFrame = now + data->m_replacementFrames + data->m_dockAnimationFrames;
			}

			break;
		}
	}

	//If the carrier has at least one aircraft, then allow it to attack.
	Bool hasAircraft = FALSE;
	for( it = m_spaces.begin(); it != m_spaces.end(); it++ )
	{
		if( it->m_objectInSpace != INVALID_ID )
		{
			hasAircraft = TRUE;
			break;
		}
	}
	getObject()->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_ATTACK ), !hasAircraft );
	Drawable *draw = getObject()->getDrawable();

	//Check for timer expiry -- are we allowed to launch the next wave yet?
	for( int i = 0; i < data->m_numCols; i++ )
	{
		Object *jet = TheGameLogic->findObjectByID( m_spaces[ i ].m_objectInSpace );
		if( jet && !isAbleToGiveUpParkingSpace( jet ) && isInPositionToTakeoff( *jet ) && hasTakeoffOrders() )
		{
			if( m_nextLaunchWaveFrame[ i ] <= now )
			{
				//Handle making the ramp going up and holding the launch until it's completely up.
				if( !m_rampUp[ i ] )
				{
					m_rampUp[ i ] = TRUE;
					m_rampUpFrame[ i ] = now + data->m_launchRampFrames;
					m_lowerRampFrame[ i ] = FOREVER;
					//****MAX_RUNWAYS***** if defined beyond 3, then this code will need to be rewritten or more modelcondition flags will
					//                     need to be added.
					ModelConditionFlagType opening = (ModelConditionFlagType)(MODELCONDITION_DOOR_2_OPENING + i * NUM_MODELCONDITION_DOOR_STATES);
					ModelConditionFlagType closing = (ModelConditionFlagType)(MODELCONDITION_DOOR_2_CLOSING + i * NUM_MODELCONDITION_DOOR_STATES);
					draw->clearAndSetModelConditionState( closing, opening );
				}

				//Handle launching the wave of fighters.
				if( m_rampUp[ i ] && m_rampUpFrame[ i ] <= now )
				{
					AIUpdateInterface *jetAI = jet->getAI();
					if( jetAI )
					{
						propagateOrderToSpecificPlane( jet );
						m_nextLaunchWaveFrame[ i ] = now + data->m_launchWaveFrames;
						m_catapultSystemFrame[ i ] = now + data->m_catapultFireFrames;
						m_lowerRampFrame[ i ] = now + data->m_lowerRampFrames;
					}
				}
			}
		}

		//Handle firing of the catapult steam effect upon launching planes.
		if( m_catapultSystemFrame[ i ] <= now && data->m_runwayInfo[ i ].m_catapultParticleSystem )
		{
			ParticleSystem *ps = TheParticleSystemManager->createParticleSystem( data->m_runwayInfo[ i ].m_catapultParticleSystem );
			m_catapultSystemFrame[ i ] = FOREVER;
			if( ps )
			{
				ps->setLocalTransform( &m_runways[ i ].m_startTransform );
				ps->setPosition( &m_runways[ i ].m_start );
			}
		}

		//Handle lowering the ramp after the fighter has been launched.
		if( m_rampUp[ i ] && m_lowerRampFrame[ i ] <= now )
		{
			m_rampUp[ i ] = FALSE;
			//****MAX_RUNWAYS***** if defined beyond 3, then this code will need to be rewritten or more modelcondition flags will
			//                     need to be added.
			ModelConditionFlagType opening = (ModelConditionFlagType)(MODELCONDITION_DOOR_2_OPENING + i * NUM_MODELCONDITION_DOOR_STATES);
			ModelConditionFlagType closing = (ModelConditionFlagType)(MODELCONDITION_DOOR_2_CLOSING + i * NUM_MODELCONDITION_DOOR_STATES);
			draw->clearAndSetModelConditionState( opening, closing );
		}
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
ExitDoorType FlightDeckBehavior::reserveDoorForExit( const ThingTemplate* objType, Object *specificObject )
{
	//Uses the same door for all production.
	return DOOR_1;
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::exitObjectViaDoor( Object *newObj, ExitDoorType exitDoor ) ///< Here is the thing I want you to exit
{
	FlightDeckInfo* ppi = nullptr;
	if (exitDoor != DOOR_NONE_NEEDED)
	{
		for (std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); ++it)
		{
			if( it->m_objectInSpace == INVALID_ID )
			{
				ppi = &(*it);
				break;
			}
		}
	}

	if (!ppi)
	{
		DEBUG_CRASH(("could not find the space. what?"));
		return;
	}

	ppi->m_objectInSpace = newObj->getID();
	//validateAssignments();

	/// @todo srj -- this is evil. fix.
	static NameKeyType jetKey = TheNameKeyGenerator->nameToKey( "JetAIUpdate" );
	JetAIUpdate* ju = (JetAIUpdate *)newObj->findUpdateModule( jetKey );
	Real parkingOffset = ju ? ju->friend_getParkingOffset() : 0.0f;

	PPInfo ppinfo;
	Matrix3D mtx;

	DUMPMATRIX3D(getObject()->getTransformMatrix());
	DUMPCOORD3D(getObject()->getPosition());
	CRCDEBUG_LOG(("Produced at hangar (door = %d)", exitDoor));
	DEBUG_ASSERTCRASH(exitDoor != DOOR_NONE_NEEDED, ("Hmm, unlikely"));
	if (!reserveSpace(newObj->getID(), parkingOffset, &ppinfo)) //&loc, &orient, nullptr, nullptr, nullptr, nullptr, &hangarInternal, &hangOrient))
	{
		DEBUG_CRASH(("no spaces available, how did we get here?"));
		ppinfo.parkingSpace = *getObject()->getPosition();
		ppinfo.parkingOrientation = getObject()->getOrientation();
	}

	const std::vector<Coord3D> *pCreationLocations = getCreationLocations( newObj->getID() );
	if( !pCreationLocations )
	{
		DEBUG_CRASH( ("No creation locations specified for runway for FlightDeckBehavior (Kris).") );
		return;
	}

	newObj->setPosition( &(*pCreationLocations)[0] );
	newObj->setOrientation( m_runways[ ppi->m_runway ].m_startOrient );
	TheAI->pathfinder()->addObjectToPathfindMap( newObj );

	AIUpdateInterface  *ai = newObj->getAIUpdateInterface();
	if( ai )
	{
		std::vector<Coord3D> exitPath;
		//Doesn't matter what we push in now, the JetOrHeliTaxiState::onEnter() will nuke it
		//and calculate the taxi points to move through.
		exitPath.push_back( ppi->m_prep );
		ai->aiFollowExitProductionPath( &exitPath, getObject(), CMD_FROM_AI );
	}

}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::unreserveDoorForExit( ExitDoorType exitDoor )
{
	//Aircraft carrier doesn't use the door reservation system.
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::aiDoCommand(const AICommandParms* parms)
{
	//Inspect the command and reset everything when necessary.
	if( parms->m_cmdSource != CMD_FROM_AI )
	{
		//Now the only time we care about anything is if we were ordered to attack something or attack move.
		switch( parms->m_cmd )
		{
			case AICMD_GUARD_POSITION:
				m_designatedTarget = INVALID_ID;
				m_designatedPosition.set( parms->m_pos );
				m_designatedCommand = parms->m_cmd;
				propagateOrdersToPlanes();
				break;
			case AICMD_ATTACK_POSITION:
				m_designatedTarget = INVALID_ID;
				m_designatedPosition.set( parms->m_pos );
				m_designatedCommand = parms->m_cmd;
				propagateOrdersToPlanes();
				break;
			case AICMD_FORCE_ATTACK_OBJECT:
			case AICMD_ATTACK_OBJECT:
				m_designatedTarget = parms->m_obj ? parms->m_obj->getID() : INVALID_ID;
				m_designatedPosition.zero();
				m_designatedCommand = parms->m_cmd;
				propagateOrdersToPlanes();
				break;
			case AICMD_ATTACKMOVE_TO_POSITION:
				m_designatedTarget = INVALID_ID;
				m_designatedPosition.set( parms->m_pos );
				m_designatedCommand = parms->m_cmd;
				propagateOrdersToPlanes();
				break;
			case AICMD_IDLE:
				m_designatedTarget = INVALID_ID;
				m_designatedPosition.zero();
				m_designatedCommand = parms->m_cmd;
				propagateOrdersToPlanes();
				break;
			default:
				m_designatedCommand = AICMD_NO_COMMAND;
				break;
		}
	}

	//Do NOT extend the base class AI. The carrier is a dumb stump that only propagates orders
	//to it's aircraft.
	//AIUpdateInterface::aiDoCommand( parms );
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::propagateOrdersToPlanes()
{
	//We just ordered the carrier to stop, so order all the planes that are out to return!
	for( std::vector<FlightDeckInfo>::iterator it = m_spaces.begin(); it != m_spaces.end(); it++ )
	{
		if( it->m_objectInSpace != INVALID_ID )
		{
			Object *jet = TheGameLogic->findObjectByID( it->m_objectInSpace );
			if( jet && isAbleToGiveUpParkingSpace( jet ) )
			{
				propagateOrderToSpecificPlane( jet );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool FlightDeckBehavior::hasTakeoffOrders()
{
	Object *target = TheGameLogic->findObjectByID( m_designatedTarget );
	switch( m_designatedCommand )
	{
		case AICMD_GUARD_POSITION:
			return TRUE;

		case AICMD_ATTACK_POSITION:
			return TRUE;

		case AICMD_ATTACKMOVE_TO_POSITION:
			return TRUE;

		case AICMD_FORCE_ATTACK_OBJECT:
		case AICMD_ATTACK_OBJECT:
			if( target )
			{
				return TRUE;
			}
			m_designatedCommand = AICMD_NO_COMMAND;
			m_designatedTarget = INVALID_ID;
			return FALSE;

		case AICMD_IDLE:
			return FALSE;

		default:
			return FALSE;
	}
}

//-------------------------------------------------------------------------------------------------
void FlightDeckBehavior::propagateOrderToSpecificPlane( Object *jet )
{
	if( jet )
	{
		AIUpdateInterface *ai = jet->getAI();
		if( ai )
		{
			Object *target = TheGameLogic->findObjectByID( m_designatedTarget );
			switch( m_designatedCommand )
			{
				case AICMD_GUARD_POSITION:
					ai->aiGuardPosition( &m_designatedPosition, GUARDMODE_NORMAL, CMD_FROM_AI );
					break;
				case AICMD_ATTACK_POSITION:
					ai->aiAttackPosition( &m_designatedPosition, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
					break;
				case AICMD_FORCE_ATTACK_OBJECT:
				case AICMD_ATTACK_OBJECT:
					ai->aiForceAttackObject( target, NO_MAX_SHOTS_LIMIT, CMD_FROM_PLAYER );
					break;
				case AICMD_ATTACKMOVE_TO_POSITION:
					ai->aiAttackMoveToPosition( &m_designatedPosition, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
					break;
				case AICMD_IDLE:
					ai->aiEnter( getObject(), CMD_FROM_AI );
					break;
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void FlightDeckBehavior::crc( Xfer *xfer )
{

	// extend base class
	AIUpdateInterface::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void FlightDeckBehavior::xfer( Xfer *xfer )
{
	Int i;

	// version
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	AIUpdateInterface::xfer( xfer );

	if( xfer->getXferMode() == XFER_LOAD )
	{
		// first, build our info, so it won't be overwritten later.
		buildInfo(FALSE); // False, because the planes are going to save themselves.  We don't re-create them
	}

	// spaces info count and data
	UnsignedByte spacesCount = m_spaces.size();
	xfer->xferUnsignedByte( &spacesCount );
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// save all elements
		std::vector< FlightDeckInfo >::iterator it;
		for( it = m_spaces.begin(); it != m_spaces.end(); ++it )
		{
			// object in this space
			xfer->xferObjectID( &((*it).m_objectInSpace) ); // This is the one thing not regenerated by buildInfo
		}

	}
	else if( xfer->getXferMode() == XFER_LOAD )
	{
		ObjectID objectID;

		// read all elements
		std::vector< FlightDeckInfo >::iterator it;
		it = m_spaces.begin();
		for( i = 0; i < spacesCount; ++i )
		{

			// read object id
			xfer->xferObjectID( &objectID );

			// store in vector if the vector does indeed still have room for this entry
			if( it != m_spaces.end() )
			{
				(*it).m_objectInSpace = objectID;
				//validateAssignments();
				++it;
			}

		}

	}

	// runways count and info
	UnsignedByte runwaysCount = m_runways.size();
	xfer->xferUnsignedByte( &runwaysCount );
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// save all elements
		std::vector< RunwayInfo >::iterator it;
		for( it = m_runways.begin(); it != m_runways.end(); ++it )
		{

			// save object ID
			xfer->xferObjectID( &((*it).m_inUseByForTakeoff ) );
			xfer->xferObjectID( &((*it).m_inUseByForLanding ) );

		}

	}
	else if( xfer->getXferMode() == XFER_LOAD )
	{
		// read all elements
		std::vector< RunwayInfo >::iterator it;
		it = m_runways.begin();
		for( i = 0; i < runwaysCount; ++i )
		{

			ObjectID inUseByForTakeoff, inUseByForLanding;
// Old?			Bool wasInLine;

			// read object ID
			xfer->xferObjectID( &inUseByForTakeoff );
			xfer->xferObjectID( &inUseByForLanding );
// Old?			xfer->xferBool( &wasInLine );

			// store in vector if the vector does indeed still have room for this entry
			if( it != m_runways.end() )
			{

				(*it).m_inUseByForTakeoff = inUseByForTakeoff;
				(*it).m_inUseByForLanding = inUseByForLanding;
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

	xfer->xferUnsignedInt( &m_nextHealFrame );
	xfer->xferUnsignedInt( &m_nextCleanupFrame );
	xfer->xferUnsignedInt( &m_startedProductionFrame );
	xfer->xferUnsignedInt( &m_nextAllowedProductionFrame );
	xfer->xferObjectID( &m_designatedTarget );
	Int commandType;
	xfer->xferInt( &commandType );
	m_designatedCommand = (AICommandType)commandType;
	xfer->xferCoord3D( &m_designatedPosition );

	UnsignedInt maxRunways = MAX_RUNWAYS;
	xfer->xferUnsignedInt( &maxRunways ); //If we're loading, we'll overwrite it. If we're saving, we save it!

	for( i = 0; i < MAX_RUNWAYS; i++ )
	{
		if( maxRunways <= MAX_RUNWAYS )
		{
			//Save and load everything.
			xfer->xferUnsignedInt( &m_nextLaunchWaveFrame[ i ] );
			xfer->xferUnsignedInt( &m_rampUpFrame[ i ] );
			xfer->xferUnsignedInt( &m_catapultSystemFrame[ i ] );
			xfer->xferUnsignedInt( &m_lowerRampFrame[ i ] );
			xfer->xferBool( &m_rampUp[ MAX_RUNWAYS ] );
		}
		else
		{
			//We must be loading... so load filler but don't assign it.
			UnsignedInt dummyInt;
			Bool dummyBool;
			xfer->xferUnsignedInt( &dummyInt );
			xfer->xferUnsignedInt( &dummyInt );
			xfer->xferUnsignedInt( &dummyInt );
			xfer->xferUnsignedInt( &dummyInt );
			xfer->xferBool( &dummyBool );
		}
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void FlightDeckBehavior::loadPostProcess()
{


	const FlightDeckBehaviorModuleData* data = getFlightDeckBehaviorModuleData();
	m_thingTemplate = TheThingFactory->findTemplate( data->m_thingTemplateName );


	// extend base class
	AIUpdateInterface::loadPostProcess();

	// no, this is bad.. it is NOT SAFE to call setWakeFrame from the xfer system. crap. (srj)
	// make sure we are awake... old save games let us sleep
	//setWakeFrame(getObject(), UPDATE_SLEEP_NONE);

}

