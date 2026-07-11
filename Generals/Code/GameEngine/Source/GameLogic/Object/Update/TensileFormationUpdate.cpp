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

// FILE: TensileFormationUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Author: Mark Lorenzen, November 2002
// Desc:   Springy formation movement like that of say, an avalanche
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "Common/AudioEventRTS.h"

#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/TensileFormationUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"

#include "GameClient/TerrainVisual.h"
#include "GameClient/Drawable.h"





#define MAP_XY_FACTOR			(10.0f)	 //How wide and tall each height map square is in world space.
#define MAP_HEIGHT_SCALE	(MAP_XY_FACTOR/16.0f)		//divide all map heights by 8.





//---------------------------------------------------------------------------------
TensileFormationUpdate* getTFU( Object *obj )
{
	static NameKeyType theTFUKey = TheNameKeyGenerator->nameToKey( "TensileFormationUpdate" );
	TensileFormationUpdate* tfu = (TensileFormationUpdate *)obj->findUpdateModule( theTFUKey );
	return tfu;
}

class PartitionFilterTensileFormationMember : public PartitionFilter
{
private:
	Object* m_obj;

public:
	PartitionFilterTensileFormationMember( Object* obj ) : m_obj( obj ) { }
#if defined(RTS_DEBUG)
	virtual const char* debugGetName() override { return "PartitionFilterTensileFormationMember"; }
#endif
	virtual Bool allow( Object *objOther ) override
	{
		return ( getTFU( objOther ) != nullptr );
	}
};


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
TensileFormationUpdateModuleData::TensileFormationUpdateModuleData()
{

	m_enabled = FALSE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void TensileFormationUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{

	UpdateModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "Enabled",	    INI::parseBool,	nullptr, offsetof( TensileFormationUpdateModuleData, m_enabled ) },
		{ "CrackSound",	INI::parseAudioEventRTS,	nullptr,	offsetof( TensileFormationUpdateModuleData, m_crackSound) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
TensileFormationUpdate::TensileFormationUpdate( Thing *thing, const ModuleData *moduleData )
						:UpdateModule( thing, moduleData )
{


	const TensileFormationUpdateModuleData *modData = getTensileFormationUpdateModuleData();

	// save our initial enabled status based on INI settings
	m_enabled = ((TensileFormationUpdateModuleData *)moduleData)->m_enabled;
	m_inertia.set(0,0,0);
	m_linksInited = FALSE;
	m_motionlessCounter = 0;
	m_life = 0;
	m_lowestSlideElevation = 255.0f;

	for ( int t = 0; t < 4; ++t)
	{
		m_links[ t ].id = INVALID_ID;
		m_links[ t ].tensor.set(0,0,0);
	}

	m_crackSound = modData->m_crackSound;
	m_crackSound.setObjectID( getObject()->getID() ) ;

	TheAI->pathfinder()->createAWallFromMyFootprint( getObject() ); // Temporarily treat this object as an obstacle.


	//Coord3D pos = *getObject()->getPosition();
	//if ( pos.z > 80)
	//{
	//	pos.z = 80 + ( ( pos.z - 80 ) * 2);
	//	getObject()->setPosition( &pos );
	//}



}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
TensileFormationUpdate::~TensileFormationUpdate()
{

}



void TensileFormationUpdate::initLinks()
{

	m_linksInited = TRUE;

	Object *obj = getObject();
	if ( !obj )
		return;

	PartitionFilterTensileFormationMember tfmFilter( getObject() );
	PartitionFilter *filters[] = { &tfmFilter, nullptr };
	SimpleObjectIterator *iter = nullptr;
	iter = ThePartitionManager->iterateObjectsInRange(getObject(), 1000.0f, FROM_BOUNDINGSPHERE_3D, filters);
	MemoryPoolObjectHolder hold(iter);

	Real closestDistance = 99999.9f;
	Real thisDistance    = 99999.9f;
	const Coord3D *myPos = obj->getPosition();

	for (Object* other = iter->first(); other; other = iter->next())
	{
		const Coord3D *theirPos = other->getPosition();
		Coord3D delta = { theirPos->x - myPos->x, theirPos->y - myPos->y, theirPos->z - myPos->z };

		thisDistance = delta.length();
		if ( closestDistance > thisDistance )
		{
			closestDistance = thisDistance;

			//push a new link
			for ( int t = 3; t > 0; --t )
				m_links[ t ] = m_links[ t-1 ];

			m_links[ 0 ].id = other->getID();
			m_links[ 0 ].tensor = delta;
		}
	}

	//ICoord2D gridPos;
	//gridPos.x = REAL_TO_INT_FLOOR(getObject()->getPosition()->x/MAP_XY_FACTOR);
	//gridPos.y = REAL_TO_INT_FLOOR(getObject()->getPosition()->y/MAP_XY_FACTOR);
	//TheTerrainVisual->setRawMapHeight(&gridPos, 500);

	getObject()->setOrientation(GameLogicRandomValueReal(-PI,PI));

}






// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime TensileFormationUpdate::update()
{

	if ( ! m_linksInited )
		initLinks();



	if( m_enabled == FALSE )
	{
	//We are all going to sit here idle and do not much of anything until one of us gets hurt
		BodyModuleInterface *body = getObject()->getBodyModule();
		BodyDamageType bdt = body->getDamageState();

		if ( bdt >= BODY_DAMAGED )
		{
			//THen the hurt one (maybe me) will tell every other member of the formation to enable
			//So we will start moving
			setEnabled( TRUE );

			TheAI->pathfinder()->removeWallFromMyFootprint( getObject() );  // Undo createAWallFromMyFootprint.


			//ALert players of disaster happening
			if ( ! m_crackSound.isCurrentlyPlaying())
				m_crackSound.setPlayingHandle(TheAudio->addAudioEvent( &m_crackSound ));

		}
		else
			return UPDATE_SLEEP(30);
	}


	Drawable *draw = getObject()->getDrawable();
	if ( ! draw )
		return UPDATE_SLEEP_NONE;

	if (++m_life > 300)
	{
		draw->clearModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_MOVING));
		draw->clearModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_FREEFALL));
		draw->clearModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_POST_COLLAPSE));

		BodyModuleInterface *body = getObject()->getBodyModule();
		body->setDamageState( BODY_RUBBLE );


		TheAI->pathfinder()->createAWallFromMyFootprint( getObject() ); // Temporarily treat this object as an obstacle.

		return UPDATE_SLEEP_FOREVER;
	}


	if ( m_life%30 == 29 )
		propagateDislodgement( TRUE );


	//APPLY PHYSICS===========================
	const Coord3D *pos = getObject()->getPosition();

	Coord3D normal = { 0.0f, 0.0f, 1.0f };
	TheTerrainLogic->getGroundHeight(pos->x, pos->y, &normal); // which way does the ground slope?

	Coord3D slope = { normal.x, normal.y, 0.0f};
	Real steepness = 1.0f - normal.z;
	slope.scale( 0.3f + steepness);

	m_inertia.add( slope );

	Real friction = 0.95f;
	m_inertia.scale( friction );



	Coord3D newPos;
	newPos.x = pos->x + m_inertia.x;//flow down the slope
	newPos.y = pos->y + m_inertia.y;
	newPos.z = TheTerrainLogic->getGroundHeight(newPos.x, newPos.y);//rest on surface here



	Object *tree = ThePartitionManager->getClosestObject( &newPos, getObject()->getGeometryInfo().getMajorRadius(), FROM_CENTER_2D );
	if (tree->isKindOf( KINDOF_SHRUBBERY ))
		tree->topple( &m_inertia, m_inertia.length(), 1 );//No Bounce



	//APPLY TENSORS===========================
	Coord3D tensorSum = { 0, 0, 0 };
	for ( int t = 0; t < 4; ++t )
	{
		Object *other = TheGameLogic->findObjectByID( m_links[ t ].id );

		if ( other )
		{
			Coord3D desiredPos = *other->getPosition();

			Coord3D tensor = m_links[ t ].tensor;

			desiredPos.sub( tensor );

			//Coord3D desiredPos = { theirPos->x - m_links[ t ].tensor.x, theirPos->y - m_links[ t ].tensor.y, theirPos->z - m_links[ t ].tensor.z };

			newPos.x = newPos.x*0.93f + desiredPos.x*0.07f;
			newPos.y = newPos.y*0.93f + desiredPos.y*0.07f;
			newPos.z = MIN( m_lowestSlideElevation, TheTerrainLogic->getGroundHeight(newPos.x, newPos.y) );//rest on surface here

			tensor.normalize();
			tensorSum.add( tensor );

		}

	}

	tensorSum.normalize();

	Coord3D inertiaNormal = m_inertia;
	inertiaNormal.normalize();









	draw->setModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_POST_COLLAPSE));

	if ( m_life < 200 )
		draw->setModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_MOVING));
	else
		draw->clearModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_MOVING));

	if ( fabs( pos->z - newPos.z ) > 0.2f && m_life < 100)
		draw->setModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_FREEFALL));
	else
		draw->clearModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_FREEFALL));

//	{
//		RGBColor cyan = {0,1,1};
//		draw->flashAsSelected(&cyan);
//	}



	//else
		//m_motionlessCounter = 0;


	//if ( newPos.z > 80)
	//	newPos.z = 80 + ( ( newPos.z - 80 ) * 2);


	m_lowestSlideElevation = newPos.z;
	getObject()->setPosition( &newPos );



	return UPDATE_SLEEP_NONE;
}





//-------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------
void TensileFormationUpdate::propagateDislodgement ( Bool enabled )
{
	PartitionFilterTensileFormationMember tfmFilter( getObject() );
	PartitionFilter *filters[] = { &tfmFilter, nullptr };
	SimpleObjectIterator *iter = nullptr;
	iter = ThePartitionManager->iterateObjectsInRange(getObject(), 100.0f, FROM_BOUNDINGSPHERE_3D, filters);
	MemoryPoolObjectHolder hold(iter);
	for (Object* other = iter->first(); other; other = iter->next())
	{

		//Object *other = TheGameLogic->findObjectByID( m_links[ t ].id );
		if ( other )
		{
			BodyModuleInterface *body = other->getBodyModule();
			if ( body )
			{
				body->setDamageState( BODY_DAMAGED );
			}
		}


		//TensileFormationUpdate* tfu = getTFU(other);
		//if ( tfu != nullptr )
		//{
		//	tfu->setEnabled( enabled );
		//}
	}

	for ( int t = 0; t < 4; ++t )
	{
		Object *other = TheGameLogic->findObjectByID( m_links[ t ].id );
		if ( other )
		{
			BodyModuleInterface *body = other->getBodyModule();
			if ( body )
			{
				body->setDamageState( BODY_DAMAGED );
			}
		}
	}



}




// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TensileFormationUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void TensileFormationUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// enabled
	xfer->xferBool( &m_enabled );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TensileFormationUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
