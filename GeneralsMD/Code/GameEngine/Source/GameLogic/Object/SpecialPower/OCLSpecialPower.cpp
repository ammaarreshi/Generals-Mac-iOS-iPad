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

// FILE: OCLSpecialPower.cpp //////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2002
// Desc:   Special powers that are driven by object creation lists
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/ThingFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/OCLSpecialPower.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// MODULE DATA ////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


enum
{
	CREATE_ABOVE_LOCATION_HEIGHT = 300,
	MAX_ADJUST_RADIUS = 500
};

static const char* const TheOCLCreateLocTypeNames[] =
{
	"CREATE_AT_EDGE_NEAR_SOURCE",
	"CREATE_AT_EDGE_NEAR_TARGET",
	"CREATE_AT_LOCATION",
	"USE_OWNER_OBJECT",
	"CREATE_ABOVE_LOCATION",
	"CREATE_AT_EDGE_FARTHEST_FROM_TARGET",
	nullptr
};
static_assert(ARRAY_SIZE(TheOCLCreateLocTypeNames) == OCL_CREATE_LOC_COUNT + 1, "Incorrect array size");

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
OCLSpecialPowerModuleData::OCLSpecialPowerModuleData()
{
	m_defaultOCL = nullptr;
	m_upgradeOCL.clear();
	m_createLoc = CREATE_AT_EDGE_NEAR_SOURCE;
	m_isOCLAdjustPositionToPassable = FALSE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static void parseOCLUpgradePair( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	OCLSpecialPowerModuleData::Upgrades up;

	INI::parseScience(ini, nullptr, &up.m_science, nullptr);
	INI::parseObjectCreationList(ini, nullptr, &up.m_ocl, nullptr);

	std::vector<OCLSpecialPowerModuleData::Upgrades>* s = (std::vector<OCLSpecialPowerModuleData::Upgrades>*)store;
	s->push_back(up);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */ void OCLSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialPowerModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "UpgradeOCL", parseOCLUpgradePair, nullptr, offsetof( OCLSpecialPowerModuleData, m_upgradeOCL ) },
		{ "OCL", INI::parseObjectCreationList, nullptr, offsetof( OCLSpecialPowerModuleData, m_defaultOCL ) },
		{ "CreateLocation", INI::parseIndexList, TheOCLCreateLocTypeNames, offsetof( OCLSpecialPowerModuleData, m_createLoc ) },
		{ "ReferenceObject", INI::parseAsciiString, nullptr, offsetof( OCLSpecialPowerModuleData, m_referenceThingName ) },
		{ "OCLAdjustPositionToPassable", INI::parseBool, nullptr, offsetof( OCLSpecialPowerModuleData, m_isOCLAdjustPositionToPassable ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// MODULE /////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
OCLSpecialPower::OCLSpecialPower( Thing *thing, const ModuleData *moduleData )
							 : SpecialPowerModule( thing, moduleData )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const ObjectCreationList* OCLSpecialPower::findOCL() const
{
	const OCLSpecialPowerModuleData* d = getOCLSpecialPowerModuleData();
	const Player* controller = getObject()->getControllingPlayer();
	if (controller != nullptr)
	{
		for (std::vector<OCLSpecialPowerModuleData::Upgrades>::const_iterator it = d->m_upgradeOCL.begin();
					it != d->m_upgradeOCL.end();
					++it)
		{
			if (controller->hasScience(it->m_science))
				return it->m_ocl;
		}
	}
	return d->m_defaultOCL;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
OCLSpecialPower::~OCLSpecialPower()
{

}

//-------------------------------------------------------------------------------------------------
/** Execute the power */
//-------------------------------------------------------------------------------------------------
void OCLSpecialPower::doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions )
{
	if (getObject()->isDisabled())
		return;

	// sanity
	if( loc == nullptr )
		return;

	// get the module data
	const OCLSpecialPowerModuleData *modData = getOCLSpecialPowerModuleData();

	Coord3D targetCoord = *loc;

	if( modData->m_isOCLAdjustPositionToPassable )
	{
		FindPositionOptions fpOptions;
		fpOptions.flags = FPF_CLEAR_CELLS_ONLY;
		fpOptions.maxRadius = MAX_ADJUST_RADIUS;
		if ( ! ThePartitionManager->findPositionAround(&targetCoord, &fpOptions, &targetCoord) )
    { // if findPosition() fails, then don't monkey with target Coord!
    	targetCoord = *loc;
    }


	}

	// call the base class action cause we are *EXTENDING* functionality
	SpecialPowerModule::doSpecialPowerAtLocation( &targetCoord, angle, commandOptions );

#if RETAIL_COMPATIBLE_CRC
	// TheSuperHackers @info we need to leave early if we are in the MissileLauncherBuildingUpdate crash fix codepath
	if (m_availableOnFrame == 0xFFFFFFFF)
		return;
#endif

	const ObjectCreationList* ocl = findOCL();

	// at what point will the "deliverer" come in
	Coord3D creationCoord;
	switch (modData->m_createLoc)
	{
		case CREATE_AT_EDGE_NEAR_SOURCE:
			creationCoord = TheTerrainLogic->findClosestEdgePoint( getObject()->getPosition() );
			ObjectCreationList::create( ocl, getObject(), &creationCoord, &targetCoord, angle );
			break;
		case CREATE_AT_EDGE_NEAR_TARGET:
			creationCoord = TheTerrainLogic->findClosestEdgePoint(&targetCoord);
			ObjectCreationList::create( ocl, getObject(), &creationCoord, &targetCoord, angle );
			break;
		case CREATE_AT_EDGE_FARTHEST_FROM_TARGET:
			creationCoord = TheTerrainLogic->findFarthestEdgePoint(&targetCoord);
			creationCoord.z += CREATE_ABOVE_LOCATION_HEIGHT;
			ObjectCreationList::create( ocl, getObject(), &creationCoord, &targetCoord, angle );
			break;
		case CREATE_AT_LOCATION:
			// this is the case where the special power stuff originates at the location of the mouse click
			creationCoord = targetCoord;
			ObjectCreationList::create( ocl, getObject(), &creationCoord, &targetCoord, angle );
			break;
		case USE_OWNER_OBJECT:
			creationCoord.set( targetCoord );
			ObjectCreationList::create( ocl, getObject(), &creationCoord, &targetCoord, angle, false );
			break;
		case CREATE_ABOVE_LOCATION:
			// this is the case where the special power stuff originates above the location of the mouse click
			creationCoord = targetCoord;
			creationCoord.z += CREATE_ABOVE_LOCATION_HEIGHT;
			ObjectCreationList::create( ocl, getObject(), &creationCoord, &targetCoord, angle );
			break;
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void OCLSpecialPower::doSpecialPowerAtObject( Object *obj, UnsignedInt commandOptions )
{
	if (getObject()->isDisabled())
		return;

	// convert to a location
	if( !obj )
		return;
	doSpecialPowerAtLocation( obj->getPosition(), INVALID_ANGLE, commandOptions );
}

// ------------------------------------------------------------------------------------------------
void OCLSpecialPower::doSpecialPower( UnsignedInt commandOptions )
{
	if (getObject()->isDisabled())
		return;

	Coord3D creationCoord;
	creationCoord.set( *getObject()->getPosition() );

	// call the base class action cause we are *EXTENDING* functionality
	SpecialPowerModule::doSpecialPowerAtLocation( &creationCoord, INVALID_ANGLE, commandOptions );

	const ObjectCreationList* ocl = findOCL();
	ObjectCreationList::create( ocl, getObject(), &creationCoord, &creationCoord, false );
}

// ------------------------------------------------------------------------------------------------
const ThingTemplate* OCLSpecialPower::getReferenceThingTemplate() const
{
	return TheThingFactory->findTemplate( getOCLSpecialPowerModuleData()->m_referenceThingName );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void OCLSpecialPower::crc( Xfer *xfer )
{

	// extend base class
	SpecialPowerModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void OCLSpecialPower::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	SpecialPowerModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void OCLSpecialPower::loadPostProcess()
{

	// extend base class
	SpecialPowerModule::loadPostProcess();

}

