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

// FILE: OCLSpecialPower.cpp //////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2002
// Desc:   Special powers that are driven by object creation lists
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/RandomValue.h"
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
	CREATE_ABOVE_LOCATION_HEIGHT = 300
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

	// call the base class action cause we are *EXTENDING* functionality
	SpecialPowerModule::doSpecialPowerAtLocation( loc, angle, commandOptions );

	const ObjectCreationList* ocl = findOCL();

	// get the module data
	const OCLSpecialPowerModuleData *modData = getOCLSpecialPowerModuleData();

	// at what point will the "deliverer" come in
	Coord3D creationCoord;
	switch (modData->m_createLoc)
	{
		case CREATE_AT_EDGE_NEAR_SOURCE:
			creationCoord = TheTerrainLogic->findClosestEdgePoint( getObject()->getPosition() );
			ObjectCreationList::create( ocl, getObject(), &creationCoord, loc );
			break;
		case CREATE_AT_EDGE_NEAR_TARGET:
			creationCoord = TheTerrainLogic->findClosestEdgePoint(loc);
			ObjectCreationList::create( ocl, getObject(), &creationCoord, loc );
			break;
		case CREATE_AT_EDGE_FARTHEST_FROM_TARGET:
			creationCoord = TheTerrainLogic->findFarthestEdgePoint(loc);
			creationCoord.z += CREATE_ABOVE_LOCATION_HEIGHT;
			ObjectCreationList::create( ocl, getObject(), &creationCoord, loc );
			break;
		case CREATE_AT_LOCATION:
			// this is the case where the special power stuff originates at the location of the mouse click
			creationCoord = *loc;
			ObjectCreationList::create( ocl, getObject(), &creationCoord, loc );
			break;
		case USE_OWNER_OBJECT:
			creationCoord.set( *loc );
			ObjectCreationList::create( ocl, getObject(), &creationCoord, loc, false );
			break;
		case CREATE_ABOVE_LOCATION:
			// this is the case where the special power stuff originates above the location of the mouse click
			creationCoord = *loc;
			creationCoord.z += CREATE_ABOVE_LOCATION_HEIGHT;
			ObjectCreationList::create( ocl, getObject(), &creationCoord, loc );
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
