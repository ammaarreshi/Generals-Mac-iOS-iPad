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

// FILE: SpectreGunshipDeploymentUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Author: Mark Lorenzen, April 2003
// Desc:   Update module to handle deployment of the SpectreGunship Generals special power.from command center
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_DEATH_NAMES

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Xfer.h"
#include "Common/ClientUpdateModule.h"

#include "GameClient/ControlBar.h"
#include "GameClient/GameClient.h"
#include "GameClient/Drawable.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/FXList.h"

#include "GameLogic/Locomotor.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpectreGunshipDeploymentUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/LaserUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"




//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpectreGunshipDeploymentUpdateModuleData::SpectreGunshipDeploymentUpdateModuleData()
{
	m_specialPowerTemplate			   = nullptr;
	m_extraRequiredScience				 = SCIENCE_INVALID;
/******BOTH*******//*BOTH*//******BOTH*******//******BOTH*******/  m_attackAreaRadius             = 200.0f;
	m_createLoc = CREATE_GUNSHIP_AT_EDGE_FARTHEST_FROM_TARGET;

}


static const char* const TheGunshipCreateLocTypeNames[] =
{
	"CREATE_AT_EDGE_NEAR_SOURCE",
  "CREATE_AT_EDGE_FARTHEST_FROM_SOURCE",
	"CREATE_AT_EDGE_NEAR_TARGET",
	"CREATE_AT_EDGE_FARTHEST_FROM_TARGET",
	nullptr
};
static_assert(ARRAY_SIZE(TheGunshipCreateLocTypeNames) == GUNSHIP_CREATE_LOC_COUNT + 1, "Wrong array size");


static Real zero = 0.0f;
//-------------------------------------------------------------------------------------------------
/*static*/ void SpectreGunshipDeploymentUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "GunshipTemplateName",	    INI::parseAsciiString,				    nullptr, offsetof( SpectreGunshipDeploymentUpdateModuleData, m_gunshipTemplateName ) },
		{ "RequiredScience",					INI::parseScience,								nullptr, offsetof( SpectreGunshipDeploymentUpdateModuleData, m_extraRequiredScience ) },
/******BOTH*******/   { "SpecialPowerTemplate",     INI::parseSpecialPowerTemplate,   nullptr, offsetof( SpectreGunshipDeploymentUpdateModuleData, m_specialPowerTemplate ) },
/*******BOTH******/		{ "AttackAreaRadius",	        INI::parseReal,				            nullptr, offsetof( SpectreGunshipDeploymentUpdateModuleData, m_attackAreaRadius ) },
		{ "CreateLocation", INI::parseIndexList, TheGunshipCreateLocTypeNames, offsetof( SpectreGunshipDeploymentUpdateModuleData, m_createLoc ) },

    { nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
SpectreGunshipDeploymentUpdate::SpectreGunshipDeploymentUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_specialPowerModule = nullptr;
  m_gunshipID  = INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
SpectreGunshipDeploymentUpdate::~SpectreGunshipDeploymentUpdate()
{
}

//-------------------------------------------------------------------------------------------------
// Validate that we have the necessary data from the ini file.
//-------------------------------------------------------------------------------------------------
void SpectreGunshipDeploymentUpdate::onObjectCreated()
{
	const SpectreGunshipDeploymentUpdateModuleData *data = getSpectreGunshipDeploymentUpdateModuleData();
	Object *obj = getObject();

	if( !data->m_specialPowerTemplate )
	{
		DEBUG_CRASH( ("%s object's SpectreGunshipDeploymentUpdate lacks access to the SpecialPowerTemplate. Needs to be specified in ini.", obj->getTemplate()->getName().str() ) );
		return;
	}

	m_specialPowerModule = obj->getSpecialPowerModule( data->m_specialPowerTemplate );
}

//-------------------------------------------------------------------------------------------------
Bool SpectreGunshipDeploymentUpdate::initiateIntentToDoSpecialPower(const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const SpectreGunshipDeploymentUpdateModuleData *data = getSpectreGunshipDeploymentUpdateModuleData();

	if( m_specialPowerModule->getSpecialPowerTemplate() != specialPowerTemplate )
	{
		//Check to make sure our modules are connected.
		return FALSE;
	}

//	getObject()->getControllingPlayer()->getAcademyStats()->recordSpecialPowerUsed( specialPowerTemplate );

	if( !BitIsSet( commandOptions, COMMAND_FIRED_BY_SCRIPT ) )
	{
/******CHANGE*******/		m_initialTargetPosition.set( *targetPos );
	}
	else
	{
		UnsignedInt now = TheGameLogic->getFrame();
		m_specialPowerModule->setReadyFrame( now );
/******CHANGE*******/   	m_initialTargetPosition.set( *targetPos );
//		setLogicalStatus( GUNSHIPDEPLOY_STATUS_INSERTING );
	}

   Object *newGunship = TheGameLogic->findObjectByID( m_gunshipID );
	const ThingTemplate *gunshipTemplate = TheThingFactory->findTemplate( data->m_gunshipTemplateName );
	if( newGunship != nullptr )
  {
//    disengageAndDepartAO( newGunship );
    m_gunshipID = INVALID_ID;
    newGunship = nullptr;
  }


  // Lets make a gunship, since we have none.
	{
		newGunship = TheThingFactory->newObject( gunshipTemplate, getObject()->getTeam() );
  }

  DEBUG_ASSERTCRASH( newGunship, ("SpecterGunshipUpdate failed to find or create a gunship object"));
  if ( newGunship )
  {
    //PRODUCER
    newGunship->setProducer( getObject() );

    //POSITION
    Coord3D creationCoord;
	  switch (data->m_createLoc)
	  {
		  case CREATE_GUNSHIP_AT_EDGE_NEAR_SOURCE:
			  creationCoord = TheTerrainLogic->findClosestEdgePoint( getObject()->getPosition() );
			  break;
		  case CREATE_GUNSHIP_AT_EDGE_FARTHEST_FROM_SOURCE:
			  creationCoord = TheTerrainLogic->findFarthestEdgePoint( getObject()->getPosition() );
			  break;
		  case CREATE_GUNSHIP_AT_EDGE_NEAR_TARGET:
			  creationCoord = TheTerrainLogic->findClosestEdgePoint(targetPos);
			  break;
		  case CREATE_GUNSHIP_AT_EDGE_FARTHEST_FROM_TARGET:
      default:
			  creationCoord = TheTerrainLogic->findFarthestEdgePoint(targetPos);
			  break;
	  }




      // HERE WE NEED TO CREATE THE POINT FURTHER OFF THE MAP SO WE CANT SEE THE LAME HOVER AND ACCELERATE BEHAVIOR
    Coord3D deltaToCreationPoint = m_initialTargetPosition;
    deltaToCreationPoint.sub( creationCoord );
    Real distanceFromTarget = deltaToCreationPoint.length();
    deltaToCreationPoint.normalize();
    deltaToCreationPoint.x *= ( distanceFromTarget + data->m_gunshipOrbitRadius );
    deltaToCreationPoint.y *= ( distanceFromTarget + data->m_gunshipOrbitRadius );
    creationCoord.x = m_initialTargetPosition.x - deltaToCreationPoint.x;
    creationCoord.y = m_initialTargetPosition.y - deltaToCreationPoint.y;

    Real preferredElevation = newGunship->getAI()->getCurLocomotor()->getPreferredHeight();
    creationCoord.z = preferredElevation;
    newGunship->setPosition( &creationCoord );

    //ORIENTATION
		Real orient = WWMath::Atan2Origin( m_initialTargetPosition.y - creationCoord.y, m_initialTargetPosition.x - creationCoord.x);
    newGunship->setOrientation( orient );

    // ID
    m_gunshipID = newGunship->getID();

    // FIRE THE SPECIAL POWER OF THE GUNSHIP
	  SpecialPowerModuleInterface *shipSPMInterface = newGunship->getSpecialPowerModule( specialPowerTemplate );
	  if( shipSPMInterface )
	  {
		  SpecialPowerModule *spModule = (SpecialPowerModule*)shipSPMInterface;
		  spModule->markSpecialPowerTriggered( &m_initialTargetPosition );
      spModule->doSpecialPowerAtLocation( &m_initialTargetPosition, INVALID_ANGLE, commandOptions );

	  }

    // MAKE THE GUNSHIP SELECTED
    // TheSuperHackers @bugfix arcticdolphin 04/03/2026 Only select the gunship on the local client that controls the unit.
    TheGameLogic->selectObject( newGunship, TRUE, getObject()->getControllingPlayer()->getPlayerMask(), newGunship->isLocallyControlled() );


  }


	SpecialPowerModuleInterface *spmInterface = getObject()->getSpecialPowerModule( specialPowerTemplate );
	if( spmInterface )
	{
		SpecialPowerModule *spModule = (SpecialPowerModule*)spmInterface;
		spModule->markSpecialPowerTriggered( &m_initialTargetPosition );
	}

  return TRUE;
}




//-------------------------------------------------------------------------------------------------
/** The update callback. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime SpectreGunshipDeploymentUpdate::update()
{
//	const SpectreGunshipDeploymentUpdateModuleData *data = getSpectreGunshipDeploymentUpdateModuleData();


	Object *me = getObject();
	// Abort conditions.
	if( me->testStatus(OBJECT_STATUS_SOLD)
    || me->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION)
    || me->isEffectivelyDead() )
	{

		return UPDATE_SLEEP_FOREVER;
	}

	return UPDATE_SLEEP_NONE;

}










// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SpectreGunshipDeploymentUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SpectreGunshipDeploymentUpdate::xfer( Xfer *xfer )
{
//	const SpectreGunshipDeploymentUpdateModuleData *data = getSpectreGunshipDeploymentUpdateModuleData();

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );
  xfer->xferObjectID( &m_gunshipID );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SpectreGunshipDeploymentUpdate::loadPostProcess()
{
	// extend base class
	UpdateModule::loadPostProcess();

}
