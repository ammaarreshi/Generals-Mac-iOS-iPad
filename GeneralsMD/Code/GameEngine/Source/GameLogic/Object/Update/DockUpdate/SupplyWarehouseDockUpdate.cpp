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

// FILE: SupplyWarehouseDockUpdate.h /////////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood Feb 2002
// Desc:   The action of this dock update is identifying who is docking and either taking Boxes away or giving them
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/AIPathfind.h"

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
SupplyWarehouseDockUpdateModuleData::SupplyWarehouseDockUpdateModuleData()
{
	m_startingBoxesData = 1;
	m_deleteWhenEmpty = FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void SupplyWarehouseDockUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{

	DockUpdateModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "StartingBoxes",	INI::parseInt,	nullptr, offsetof( SupplyWarehouseDockUpdateModuleData, m_startingBoxesData ) },
		{ "DeleteWhenEmpty",	INI::parseBool,	nullptr, offsetof( SupplyWarehouseDockUpdateModuleData, m_deleteWhenEmpty ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  p.add(dataFieldParse);

}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
SupplyWarehouseDockUpdate::SupplyWarehouseDockUpdate( Thing *thing, const ModuleData* moduleData ) : DockUpdate( thing, moduleData )
{
	m_boxesStored = getSupplyWarehouseDockUpdateModuleData()->m_startingBoxesData;
}

SupplyWarehouseDockUpdate::~SupplyWarehouseDockUpdate()
{
}

void SupplyWarehouseDockUpdate::onObjectCreated()
{
	Drawable *draw = getObject()->getDrawable();
	if( draw )
	{
		draw->updateDrawableSupplyStatus( getSupplyWarehouseDockUpdateModuleData()->m_startingBoxesData, m_boxesStored );
	}
}

Bool SupplyWarehouseDockUpdate::action( Object* docker, Object *drone )
{
	if( m_boxesStored == 0 )
		return FALSE;

	// Make sure that the docker is at least reasonably close to the dock.
	// Basically, one bounding diameter of space or less between us.
	Real closeEnoughSqr = sqr(docker->getGeometryInfo().getBoundingCircleRadius()*2);
	Real curDistSqr = ThePartitionManager->getDistanceSquared(docker, getObject(), FROM_BOUNDINGSPHERE_2D);
	if (curDistSqr > closeEnoughSqr) {
		DEBUG_LOG(("Failing dock, dist %f, not close enough(%f).", WWMath::SqrtOrigin(curDistSqr), WWMath::SqrtOrigin(closeEnoughSqr)));
		// Make it twitch a little.
		Coord3D newPos = *docker->getPosition();
		Real range = 0.4*PATHFIND_CELL_SIZE_F;
		newPos.x += GameLogicRandomValue(-range, range);
		newPos.y += GameLogicRandomValue(-range, range);
		docker->setPosition(&newPos);
		return FALSE;  //not close enough.
	}

	--m_boxesStored;// so the docker sees that I am shy by one box (or empty) from within his gainOneBox()

	SupplyTruckAIInterface *ai = docker->getAIUpdateInterface()->getSupplyTruckAIInterface();
	if( ai && ai->gainOneBox( m_boxesStored ) )
	{
		if( m_boxesStored == 0 && getSupplyWarehouseDockUpdateModuleData()->m_deleteWhenEmpty )
		{
			TheGameLogic->destroyObject( getObject() );
			return FALSE; //Yer done.  And so am I.
		}
		else
		{
			Drawable *draw = getObject()->getDrawable();
			if( draw )
			{
				draw->updateDrawableSupplyStatus( getSupplyWarehouseDockUpdateModuleData()->m_startingBoxesData, m_boxesStored );
			}
		}

		return TRUE;
	}
	else
		++m_boxesStored; //take it back, since there was noone to gain the box
  									 //this is important so that I have one less boxes as perceived by the docker when he gains one


	return FALSE;
}

void SupplyWarehouseDockUpdate::setDockCrippled( Bool setting )
{
	// At this level, Crippling means I kill any activeDocker between enter and exit.
	if( setting )
	{
		if( m_activeDocker != INVALID_ID )
		{
			Object *victim = TheGameLogic->findObjectByID( m_activeDocker );
			if( victim )
			{
				if( m_dockerInside )
				{
					if( !victim->isUsingAirborneLocomotor() )
						victim->kill();
				}
				else
				{
					// Else, he was between Approach and Enter.  Lucky guy.  Tell him to stop, but then
					// remind him that he wants to try again later
					SupplyTruckAIInterface* supplyTruckAI = victim->getAI()->getSupplyTruckAIInterface();
					if( supplyTruckAI )
					{
						victim->getAI()->aiIdle( CMD_FROM_AI );
						supplyTruckAI->setForceWantingState( TRUE );
					}
				}
			}
		}
	}

	DockUpdate::setDockCrippled( setting );
}

void SupplyWarehouseDockUpdate::setCashValue( Int cashValue )
{
	// A script can tell us our set value, and we need to figure out the boxes needed to provide that.
	m_boxesStored = WWMath::Ceil(cashValue / (float)TheGlobalData->m_baseValuePerSupplyBox);
	Drawable *draw = getObject()->getDrawable();
	if( draw )
	{
		draw->updateDrawableSupplyStatus( getSupplyWarehouseDockUpdateModuleData()->m_startingBoxesData, m_boxesStored );
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SupplyWarehouseDockUpdate::crc( Xfer *xfer )
{

	// extend base class
	DockUpdate::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SupplyWarehouseDockUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DockUpdate::xfer( xfer );

	// boxes stored
	xfer->xferInt( &m_boxesStored );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SupplyWarehouseDockUpdate::loadPostProcess()
{

	// extend base class
	DockUpdate::loadPostProcess();

	// update the drawable supply status
	const SupplyWarehouseDockUpdateModuleData *modData = getSupplyWarehouseDockUpdateModuleData();
	Object *us = getObject();
	Drawable *draw = us->getDrawable();
	if( draw )
		draw->updateDrawableSupplyStatus( modData->m_startingBoxesData, m_boxesStored );

}
