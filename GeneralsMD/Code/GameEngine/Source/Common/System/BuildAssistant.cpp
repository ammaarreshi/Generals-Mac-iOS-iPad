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

// FILE: BuildAssistant.cpp ///////////////////////////////////////////////////////////////////////
// Author: Colin Day, February 2002
// Desc:   Singleton class to encapsulate some of the more common functions or rules
//				 that apply to building structures and units
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/BuildAssistant.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/GameAudio.h"
#include "Common/ThingFactory.h"
#include "Common/Team.h"
#include "Common/TerrainTypes.h"
#include "Common/MapObject.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameClient/TerrainVisual.h"
#include "GameLogic/AI.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/ParkingPlaceBehavior.h"

// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
BuildAssistant *TheBuildAssistant = nullptr;


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
ObjectSellInfo::ObjectSellInfo()
{

	m_id = INVALID_ID;
	m_sellFrame = 0;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
ObjectSellInfo::~ObjectSellInfo()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** Is this object a dozer */
//-------------------------------------------------------------------------------------------------
static Bool isDozer( Object *obj )
{
	// sanity
	if( obj == nullptr )
		return FALSE;

	if( obj->isKindOf(KINDOF_DOZER))
		return TRUE;

	return FALSE;

}

// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
BuildAssistant::BuildAssistant()
{

	m_buildPositions = nullptr;
	m_buildPositionSize = 0;
	m_sellList.clear();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
BuildAssistant::~BuildAssistant()
{

	delete [] m_buildPositions;
	m_buildPositions = nullptr;
	m_buildPositionSize = 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void BuildAssistant::init()
{

	//
	// allocate our array of positions that we use to assist ourselves when constructing
	// a tiled array of locations to build things like walls
	//
	m_buildPositionSize = TheGlobalData->m_maxLineBuildObjects;
	m_buildPositions = NEW Coord3D[ m_buildPositionSize ];

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void BuildAssistant::reset()
{

	// clear all our data from the sell list
	ObjectSellInfo *sellInfo;
	ObjectSellListIterator it;
	for( it = m_sellList.begin(); it != m_sellList.end(); ++it )
	{

		// get sell info
		sellInfo = (*it);

		// delete our data and erase this entry from the list
		deleteInstance(sellInfo);

	}

	// clear the sell list
	m_sellList.clear();

}

static const Real FRAMES_TO_ALLOW_SCAFFOLD = static_cast<float>(LOGICFRAMES_PER_SECOND) * 1.5f;
static const Real TOTAL_FRAMES_TO_SELL_OBJECT = static_cast<float>(LOGICFRAMES_PER_SECOND) * 3.0f;
//-------------------------------------------------------------------------------------------------
/** Update phase for the build assistant */
//-------------------------------------------------------------------------------------------------
void BuildAssistant::update()
{
	ObjectSellInfo *sellInfo;
	Object *obj;

	// remove any objects from the sell list who's "sell time" is up
	ObjectSellListIterator it, thisIterator;
	for( it = m_sellList.begin(); it != m_sellList.end(); /*empty*/ )
	{

		// get this object info
		sellInfo = (*it);

		// increment the iterator to the next as we may remove it
		thisIterator = it;
		++it;

		// find the object
		obj = TheGameLogic->findObjectByID( sellInfo->m_id );

		//
		// if object is not found, remove it from the list immediately ... this is valid as
		// the object maybe was destroyed by other means during the sell process
		//
		if( obj == nullptr )
		{

			deleteInstance(sellInfo);
			m_sellList.erase( thisIterator );
			continue;

		}

		// decrement the construction percent
		if( TheGameLogic->getFrame() - sellInfo->m_sellFrame >= FRAMES_TO_ALLOW_SCAFFOLD )
		{
			Real previousConstructionPercent = obj->getConstructionPercent();

			// do the construction
			obj->setConstructionPercent( previousConstructionPercent - (100.0f / TOTAL_FRAMES_TO_SELL_OBJECT) );

			//
			// did we cross the threshold from positive to negative ... if so, we will clear
			// visual construction indicators
			//
			if( previousConstructionPercent > 0.0f && obj->getConstructionPercent() <= 0.0f )
			{

				obj->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PARTIALLY_CONSTRUCTED,
																																				MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED ),
																						 MAKE_MODELCONDITION_MASK( MODELCONDITION_SOLD ) );

				//
				// set the animation durations so that the regular build up loop animations can be
				// done a bit faster for selling
				//
				Drawable *draw = obj->getDrawable();
				if( draw )
					draw->setAnimationLoopDuration( TOTAL_FRAMES_TO_SELL_OBJECT / 2 );

			}

		}

		//
		// after we've reached zero ... the object has "sunk" back down into the ground ... but
		// we need to keep the object around for a little while longer so that the scaffold
		// animation can finish up and sink itself back into the ground
		//
		if( obj->getConstructionPercent() <= -50.0f )
		{
			// refund the money to the controlling player
			Player *player = obj->getControllingPlayer();
			if( player )
			{
				UnsignedInt sellValue;
				// 0 is the init, and means you have no override.  We would be marked unsellable if someone wanted us to not sell
				if( obj->getTemplate()->getRefundValue() != 0 )
					sellValue = obj->getTemplate()->getRefundValue();
				else
					sellValue = REAL_TO_UNSIGNEDINT( obj->getTemplate()->calcCostToBuild( player ) *
																										 TheGlobalData->m_sellPercentage );

				player->getMoney()->deposit( sellValue, TRUE, FALSE );
				// this money shouldn't be scored since it wasn't really "earned."
//				player->getScoreKeeper()->addMoneyEarned( sellValue );

			}

			// cancel any of the production items and refund to the controlling player
			ProductionUpdateInterface *pui = obj->getProductionUpdateInterface();
			if( pui )
				pui->cancelAndRefundAllProduction();

			// for debugging
			// UnicodeString msg;
			// msg.format( L"'%S' has been sold\n", obj->getTemplate()->getName().str() );
			// TheInGameUI->message( msg );

			// destroy the object
			TheGameLogic->destroyObject( obj );

			// remove this object from the sell list
			deleteInstance(sellInfo);
			m_sellList.erase( thisIterator );

		}

	}

}
//-------------------------------------------------------------------------------------------------
/** Xfer the sell list. */
//-------------------------------------------------------------------------------------------------
void BuildAssistant::xferTheSellList( Xfer *xfer )
{
	ObjectSellInfo *sellInfo;

	Int count=0;
	ObjectSellListIterator it, thisIterator;
 	for( it = m_sellList.begin(); it != m_sellList.end(); ++it ) {
		count++;
 	}
	xfer->xferInt(&count);

	if (xfer->getXferMode() == XFER_LOAD)	{
		m_sellList.clear();
		Int i;
		for (i=0; i<count; i++) {
			// add this object to the list of objects being sold
			sellInfo = newInstance(ObjectSellInfo);
			xfer->xferObjectID(&sellInfo->m_id);
			xfer->xferUnsignedInt(&sellInfo->m_sellFrame);
			m_sellList.push_back( sellInfo );

		}
	} else {
		for( it = m_sellList.begin(); it != m_sellList.end(); ++it ) {
			// get this object info
			sellInfo = (*it);
			xfer->xferObjectID(&sellInfo->m_id);
			xfer->xferUnsignedInt(&sellInfo->m_sellFrame);
			count--;
		}
		DEBUG_ASSERTCRASH(count==0, ("Inconsistent list size counts."));
	}

}

//-------------------------------------------------------------------------------------------------
/** Nice little method to wrap up creating an object from a build */
//-------------------------------------------------------------------------------------------------
Object *BuildAssistant::buildObjectNow( Object *constructorObject, const ThingTemplate *what,
																			  const Coord3D *pos, Real angle, Player *owningPlayer )
{

	// sanity
	if( what == nullptr || pos == nullptr )
		return nullptr;

	if( owningPlayer == nullptr )
		return nullptr;// Invalid pointer.  Won't happen.

	// sanity
	if( constructorObject )
	{
		DEBUG_ASSERTCRASH( constructorObject->getControllingPlayer() == owningPlayer,
											 ("buildObjectNow: Constructor object player is not the same as the controlling player passed in\n") );

	}

	// Need to validate that we can make this in case someone fakes their CommandSet
	// A nullptr constructor Object means a script built building so let it slide.
	if( (constructorObject != nullptr) && !isPossibleToMakeUnit(constructorObject, what) )
		return nullptr;

	// clear out any objects from the building area that are "auto-clearable" when building
	clearRemovableForConstruction( what, pos, angle );

	if ( moveObjectsForConstruction( what, pos, angle, owningPlayer ) == FALSE )
	{
		// totally bogus. We tried to move our units out of the way, but they wouldn't.
		// Chode-boys.
		if (owningPlayer->getPlayerType()==PLAYER_HUMAN) {
			return nullptr;	// ai gets to cheat.  jba.
		}
	}

	// do the build
	if( isDozer( constructorObject ) && isLineBuildTemplate( what ) == FALSE )
	{
		AIUpdateInterface *ai = constructorObject->getAIUpdateInterface();

		if( ai )
		{
			ai->aiIdle(CMD_FROM_AI); // stop any current behavior.
			return ai->construct( what, pos, angle, owningPlayer, FALSE );
		}
		return nullptr;

	}
	else
	{

		// create the new object.  We need to construct it with UnderConstruction set, since we are going to insta build it,
		// but we don't want to send double construction type events like power creation. onStructureConstructionComplete
		// is called below, so we need to simulate the proper object creation flow from the start.  Be like Dozer.
		ObjectStatusMaskType startingStatus;
		if( what->isKindOf( KINDOF_STRUCTURE ) )
			startingStatus.set( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );

		Object *obj = TheThingFactory->newObject( what, owningPlayer->getDefaultTeam(), startingStatus );
		obj->setProducer(constructorObject);

		// place on terrain surface
		Coord3D groundPos;
		groundPos.x = pos->x;
		groundPos.y = pos->y;
		groundPos.z = TheTerrainLogic->getGroundHeight( groundPos.x, groundPos.y );
		obj->setPosition( &groundPos );

		obj->setOrientation( angle );

		TheAI->pathfinder()->addObjectToPathfindMap( obj );

		// notify the player that this thing has come into existence
		if( obj->isKindOf( KINDOF_STRUCTURE ) )
		{

			//
			// this is a cheat since this method is currently used by the solo AI to make
			// buildings out of thin air
			//
			owningPlayer->onStructureCreated( constructorObject, obj );
			owningPlayer->onStructureConstructionComplete( constructorObject, obj, FALSE );

		}
		else
		{
			owningPlayer->onUnitCreated( constructorObject, obj );

			//Call the voice created for the 1st object -- because it's possible to create multiple objects like redguards!
			AudioEventRTS sound = *obj->getTemplate()->getPerUnitSound( "VoiceCreated" );
			sound.setObjectID( obj->getID() );
			TheAudio->addAudioEvent( &sound );
		}

		// Now onCreates were called at the constructor.  This magically created
		// thing needs to be considered as Built for Game specific stuff.
		for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
		{
			CreateModuleInterface* create = (*m)->getCreate();
			if (!create)
				continue;

			create->onBuildComplete();
		}

		// Creation is another valid and essential time to call this. This building now Looks.
		obj->handlePartitionCellMaintenance();

		return obj;

	}

  return nullptr;

}

//-------------------------------------------------------------------------------------------------
/** This method will create a line of objects end to end along the line defined in 3D
	* space from start to end.  This is especially useful in building walls */
//-------------------------------------------------------------------------------------------------
void BuildAssistant::buildObjectLineNow( Object *constructorObject, const ThingTemplate *what,
																				 const Coord3D *start, const Coord3D *end, Real angle,
																				 Player *owningPlayer )
{
	TileBuildInfo *tileBuildInfo;

	// sanity
	if( what == nullptr || start == nullptr || end == nullptr )
		return;

	// how big are each of our objects
	Real objectSize = what->getTemplateGeometryInfo().getMajorRadius() * 2.0f;

	// what is our max tiling length we can make
	Int maxObjects = TheGlobalData->m_maxLineBuildObjects;

	// build an array of locations that we want to build from start to end
	tileBuildInfo = buildTiledLocations( what, angle, start, end,
																			 objectSize, maxObjects, constructorObject );

	// create an object at each position
	for( Int i = 0; i < tileBuildInfo->tilesUsed; i++ )
		buildObjectNow( constructorObject, what, &tileBuildInfo->positions[ i ], angle, owningPlayer );

}

//-------------------------------------------------------------------------------------------------
/** This structure is passed along to the checkSampleBuildLocation while iterating the
	* footprint of an area in order to determine if the entire location is a legal place to
	* build */
//-------------------------------------------------------------------------------------------------
struct SampleBuildData
{
	Region3D mapRegion;
	Bool terrainRestricted; ///< one of the terrain tiles prevents building
	Real hiZ;								///< highest sample point used
	Real loZ;								///< lowest sample point used
};

//-------------------------------------------------------------------------------------------------
/** This will check the build conditions at the specified sample location point */
//-------------------------------------------------------------------------------------------------
static void checkSampleBuildLocation( const Coord3D *samplePoint, void *userData )
{
	TerrainType *terrain;
	SampleBuildData *sampleData = (SampleBuildData *)userData;

	// get the terrain tile here
	terrain = TheTerrainVisual->getTerrainTile( samplePoint->x, samplePoint->y );
	if( terrain )
	{

		// check for the restricts building flag
		if( terrain->getRestrictConstruction() )
			sampleData->terrainRestricted = TRUE;

	}

	Int cellX = REAL_TO_INT_FLOOR( samplePoint->x / PATHFIND_CELL_SIZE );
	Int cellY = REAL_TO_INT_FLOOR( samplePoint->y / PATHFIND_CELL_SIZE );

	PathfindCell* cell = TheAI->pathfinder()->getCell( LAYER_GROUND, cellX, cellY );
	if (!cell) {
		sampleData->terrainRestricted = TRUE;
	}	else {
		enum PathfindCell::CellType type = cell->getType();
		if ( (type == PathfindCell::CELL_WATER) || (type == PathfindCell::CELL_CLIFF) ||
			(type == PathfindCell::CELL_IMPASSABLE)) {
			sampleData->terrainRestricted = true;
		}
	}

	//
	// record the highest and lowest Z points from all the samples and do not allow
	// building when the difference between them is too great
	//
	if( samplePoint->z < sampleData->loZ )
		sampleData->loZ = samplePoint->z;
	if( samplePoint->z > sampleData->hiZ )
		sampleData->hiZ = samplePoint->z;

	// too close to edge of map?
	if (TheGlobalData->m_MinDistFromEdgeOfMapForBuild > 0.0f)
	{
		if (samplePoint->x < sampleData->mapRegion.lo.x + TheGlobalData->m_MinDistFromEdgeOfMapForBuild
				|| samplePoint->x > sampleData->mapRegion.hi.x - TheGlobalData->m_MinDistFromEdgeOfMapForBuild
				|| samplePoint->y < sampleData->mapRegion.lo.y + TheGlobalData->m_MinDistFromEdgeOfMapForBuild
				|| samplePoint->y > sampleData->mapRegion.hi.y - TheGlobalData->m_MinDistFromEdgeOfMapForBuild)
		{
			sampleData->terrainRestricted = TRUE;
		}
	}

}

//-------------------------------------------------------------------------------------------------
/** This function will call the user callback at each "sample point" across the footprint
	* of where this object would be in the world.  The smaller the sample resolution param is
	* the more samples will be taken across the footprint area */
//-------------------------------------------------------------------------------------------------
void BuildAssistant::iterateFootprint( const ThingTemplate *build,
																			 Real buildOrientation,
																			 const Coord3D *worldPos,
																			 Real sampleResolution,
																			 IterateFootprintFunc func,
																			 void *funcUserData )
{

	// sanity
	if( build == nullptr || worldPos == nullptr || func == nullptr )
		return;

	//
	// create a transform matrix for the object position at the specified angle, we will
	// iterate the object footprints in "local object extent space" and transform those
	// points using this matrix into the world coords for the object at the real
	// location and specified angle
	//
	Matrix3D transform;
	transform.Make_Identity();
	transform.Adjust_Translation( Vector3( worldPos->x, worldPos->y, worldPos->z ) );
	transform.Rotate_Z( buildOrientation );

	// get the bounding footprint rectangle for the geometry we're looking at
	Real halfFootprintHeight,
			 halfFootprintWidth;
	if( build->getTemplateGeometryInfo().getGeomType() == GEOMETRY_BOX )
	{

		halfFootprintHeight = build->getTemplateGeometryInfo().getMinorRadius();
		halfFootprintWidth = build->getTemplateGeometryInfo().getMajorRadius();

	}
	else if( build->getTemplateGeometryInfo().getGeomType() == GEOMETRY_SPHERE ||
					 build->getTemplateGeometryInfo().getGeomType() == GEOMETRY_CYLINDER )
	{

		halfFootprintHeight = build->getTemplateGeometryInfo().getBoundingCircleRadius();
		halfFootprintWidth = build->getTemplateGeometryInfo().getBoundingCircleRadius();

	}
	else
	{

		DEBUG_CRASH( ("iterateFootprint: Undefined geometry '%d' for '%s'",
											     build->getTemplateGeometryInfo().getGeomType(), build->getName().str()) );
		return;

	}

	//
	// start at a corner of the extent ... box geometries have a major radius down
	// the X axis with the minor radius on the Y axis.  Note that we are allowing
	// the sample process to go an "extra" sample resolution outside of the
	// real extent here.  This is so that we can be sure to sample all of the
	// footprint area, if our sample point is outside of the actual extent footprint
	// area because of this we snap it back to sample on the exact edge
	//
	Real x, y;
	Vector3 v;
	for( y = -halfFootprintHeight;
			 y < halfFootprintHeight + sampleResolution;
			 y += sampleResolution )
	{

		// snap it to the actual extent since we can go over by one sample resolution
		if( y > halfFootprintHeight )
			y = halfFootprintHeight;

		for( x = -halfFootprintWidth;
				 x < halfFootprintWidth + sampleResolution;
				 x += sampleResolution )
		{

			// snap it to the actual extent since we can go over by one sample resolution
			if( x > halfFootprintWidth )
				x = halfFootprintWidth;

			// transform to world
			v.Set( x, y, TheTerrainLogic->getGroundHeight( x, y ) );
			transform.Transform_Vector( transform, v, &v );

			// for circular geometries we must actually be within the circle
			if( build->getTemplateGeometryInfo().getGeomType() == GEOMETRY_SPHERE ||
					build->getTemplateGeometryInfo().getGeomType() == GEOMETRY_CYLINDER )
			{
				Coord2D vector;

				vector.x = v.X - worldPos->x;
				vector.y = v.Y - worldPos->y;
				if( vector.length() > halfFootprintWidth )  // could be height too, radius is all the same for circles
					continue;  // ignore this point

			}

			// call the user callback
			Coord3D pos;
			pos.x = v.X;
			pos.y = v.Y;
			pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );
			func( &pos, funcUserData );

		}

	}

}


//-------------------------------------------------------------------------------------------------
/** Check for objects preventing building at this location.
  * TheSuperHackers @tweak Stubbjax 05/09/2025 Return LBC_SHROUD for shrouded objects near the
	* edge of the shroud so that players cannot use this info to determine whether they exist. */
//-------------------------------------------------------------------------------------------------
LegalBuildCode BuildAssistant::isLocationClearOfObjects( const Coord3D *worldPos,
																											 const ThingTemplate *build,
																											 Real angle,
																											 const Object *builderObject,
																											 UnsignedInt options,
																											 Player *thePlayer)
{
	ObjectIterator *iter =
			ThePartitionManager->iteratePotentialCollisions( worldPos,
																											 build->getTemplateGeometryInfo(),
																											 angle );
	Object *them;
	Bool onlyCheckEnemies = (options == NO_ENEMY_OBJECT_OVERLAP);
	MemoryPoolObjectHolder hold(iter);
	for( them = iter->first(); them; them = iter->next() )
	{
		Bool feedbackWithFailure = TRUE;
		Relationship rel = builderObject ? builderObject->getRelationship( them ) : NEUTRAL;

		//Kris: If the object is stealthed and we can't see it, pretend we can build there.
		if( BitIsSet( options, IGNORE_STEALTHED ) )
		{
			if( rel != ALLIES )
			{
				if( them->testStatus( OBJECT_STATUS_STEALTHED ) && !them->testStatus( OBJECT_STATUS_DETECTED ) && !them->testStatus( OBJECT_STATUS_DISGUISED ) )
				{
					if( BitIsSet( options, FAIL_STEALTHED_WITHOUT_FEEDBACK ) )
					{
						feedbackWithFailure = FALSE; //We want to fail now but without feedback
					}
					else
					{
						continue;
					}
				}
			}
		}

		// ignore any kind of class of objects that we will "remove" for building
		if( isRemovableForConstruction( them ) == TRUE )
			continue;

		// ignore land mines, cluster mines and demo traps, since you can build on them
		// doing so will damage them during construction, by the way
		if (them->isKindOf( KINDOF_MINE ))
			continue;

		// same story for inert things: ie, radiation fields, pings, and projectile streams (!)...
		if (them->isKindOf(KINDOF_INERT))
			continue;

		if (them->isKindOf(KINDOF_IMMOBILE))
		{
			if (onlyCheckEnemies && builderObject && rel != ENEMIES)
				continue;
		}

#if !RETAIL_COMPATIBLE_CRC
		if (builderObject && them->getShroudedStatus(builderObject->getControllingPlayer()->getPlayerIndex()) >= OBJECTSHROUD_FOGGED)
			return LBC_SHROUD;
#endif

		//Kris: Patch 1.01 - November 5, 2003
		//Prevent busy units (black lotus hacking from being moved by trying to place a building -- exploit).
		if (rel == ALLIES)
		{
			if (them->testStatus(OBJECT_STATUS_IS_USING_ABILITY) || (them->getAI() && them->getAI()->isBusy()))
			{
				return LBC_OBJECTS_IN_THE_WAY;
			}
		}

		// an immobile object may obstruct our building depending on flags.
		if( them->isKindOf( KINDOF_IMMOBILE ) )
		{
			if( feedbackWithFailure )
			{
				TheTerrainVisual->addFactionBib( them, TRUE );
				return LBC_OBJECTS_IN_THE_WAY;
			}
			return LBC_GENERIC_FAILURE;
		}

		if( them->isDisabled() )
		{
			//Kris: If object is disabled, it can't move out of the way, thus you can't build there.
			if( feedbackWithFailure )
			{
				TheTerrainVisual->addFactionBib( them, TRUE );
				return LBC_OBJECTS_IN_THE_WAY;
			}
			return LBC_GENERIC_FAILURE;
		}

		//
		// if this is an enemy object of the builder object (and therefore the thing
		// that will be constructed) we can't build here
		//
		if( builderObject && rel == ENEMIES )
		{
			if( feedbackWithFailure )
			{
				TheTerrainVisual->addFactionBib( them, TRUE );
				return LBC_OBJECTS_IN_THE_WAY;
			}
			return LBC_GENERIC_FAILURE;
		}

	}

	if (onlyCheckEnemies)
	{
		return LBC_OK;
	}
	// Check for overlapping exit areas.

	Real range = 2*(build->getTemplateGeometryInfo().getMajorRadius()+build->getTemplateGeometryInfo().getMinorRadius());

	PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_STRUCTURE), KINDOFMASK_NONE);
	PartitionFilter *filters[] = { &f1, nullptr };

	ObjectIterator *iter2 = ThePartitionManager->iterateObjectsInRange(worldPos, range, FROM_BOUNDINGSPHERE_2D, filters);
	MemoryPoolObjectHolder hold2(iter2);

	Real myFactoryExitWidth = build->getFactoryExitWidth();
	Real myExtraWidth = build->getFactoryExtraBibWidth();

	if (thePlayer && thePlayer->isSkirmishAIPlayer()) {
		// Skirmish ai adds a little extra around the edges so it doesn't build itself into a corner.
		if (myExtraWidth < 3*PATHFIND_CELL_SIZE_F) {
			myExtraWidth = 3*PATHFIND_CELL_SIZE_F;
			myFactoryExitWidth -= myExtraWidth;
			if (myFactoryExitWidth<0) myFactoryExitWidth = 0;
		}
	}

	Bool checkMyExit = false;
	Coord3D myExitPos;
	GeometryInfo myBounds = build->getTemplateGeometryInfo();
	myBounds.setMajorRadius(myBounds.getMajorRadius()+myExtraWidth);
	if (myBounds.getGeomType() != GEOMETRY_BOX) {
		myBounds.set(GEOMETRY_BOX, false, 40, myBounds.getMajorRadius(), myBounds.getMajorRadius());
	} else {
		myBounds.setMinorRadius(myBounds.getMinorRadius()+myExtraWidth);
	}
	GeometryInfo myGeom = build->getTemplateGeometryInfo();
	if (myGeom.getGeomType() != GEOMETRY_BOX) {
		myGeom.setMinorRadius(myGeom.getMajorRadius());
	}
	myGeom.setMajorRadius(myFactoryExitWidth/2.0f);
	if (myFactoryExitWidth>0) {
		myExitPos = *worldPos;
		checkMyExit = true;
		Real c = (Real)WWMath::CosTrig(angle);
		Real s = (Real)WWMath::SinTrig(angle);
		Real offset = build->getTemplateGeometryInfo().getMajorRadius() + myFactoryExitWidth/2.0f;
		myExitPos.x += c*offset;
		myExitPos.y += s*offset;
	}


	for( them = iter2->first(); them; them = iter2->next() )
	{

		Relationship rel = builderObject ? builderObject->getRelationship( them ) : NEUTRAL;
		//Kris: If the building is stealthed and we can't see it, pretend we can build there.
		if( BitIsSet( options, IGNORE_STEALTHED ) )
		{
			if( rel != ALLIES )
			{
				if( them->testStatus( OBJECT_STATUS_STEALTHED ) && !them->testStatus( OBJECT_STATUS_DETECTED ) && !them->testStatus( OBJECT_STATUS_DISGUISED ) )
				{
					continue;
				}
			}
		}

		// ignore any kind of class of objects that we will "remove" for building
		if( isRemovableForConstruction( them ) == TRUE )
			continue;

		Real themFactoryExitWidth = them->getTemplate()->getFactoryExitWidth();
		Real hisExtraWidth = them->getTemplate()->getFactoryExtraBibWidth();

		Bool checkHisExit = false;
		Coord3D hisExitPos;
		GeometryInfo hisBounds = them->getGeometryInfo();
		hisBounds.setMajorRadius(hisBounds.getMajorRadius()+hisExtraWidth);
		if (hisBounds.getGeomType() != GEOMETRY_BOX) {
			hisBounds.set(GEOMETRY_BOX, false, 40, hisBounds.getMajorRadius(), hisBounds.getMajorRadius());
		} else {
			hisBounds.setMinorRadius(hisBounds.getMinorRadius()+myExtraWidth);
		}
		GeometryInfo hisGeom = them->getGeometryInfo();
		hisGeom.setMajorRadius(themFactoryExitWidth/2.0f);
		if (hisGeom.getGeomType() != GEOMETRY_BOX) {
			hisGeom.setMinorRadius(them->getGeometryInfo().getMajorRadius());
		}
		if (themFactoryExitWidth>0) {
			hisExitPos = *them->getPosition();
			checkHisExit = true;
			Real c = (Real)WWMath::CosTrig(them->getOrientation());
			Real s = (Real)WWMath::SinTrig(them->getOrientation());
			Real offset = them->getGeometryInfo().getMajorRadius() + themFactoryExitWidth/2.0f;
			hisExitPos.x += c*offset;
			hisExitPos.y += s*offset;
		}
		if (ThePartitionManager->geomCollidesWithGeom(them->getPosition(), hisBounds, them->getOrientation(),
			worldPos, myBounds, angle)) {
			TheTerrainVisual->addFactionBib(them, true);
			return LBC_OBJECTS_IN_THE_WAY;
		}
		if (!checkMyExit && !checkHisExit && !hisExtraWidth && !myExtraWidth)
		{
			continue; // neither has extra exit space.
		}

		// an immobile object will obstruct our building no matter what team it's on
		if ( them->isKindOf( KINDOF_IMMOBILE ) )	{
			Bool shrouded = builderObject && them->getShroudedStatus(builderObject->getControllingPlayer()->getPlayerIndex()) >= OBJECTSHROUD_FOGGED;
			/* Check for overlap of my exit rectangle to his geom info. */
			if (checkMyExit && ThePartitionManager->geomCollidesWithGeom(them->getPosition(), hisBounds, them->getOrientation(),
				&myExitPos, myGeom, angle)) {
				if (shrouded)
					return LBC_SHROUD;
				
				TheTerrainVisual->addFactionBib(them, true);
				return LBC_OBJECTS_IN_THE_WAY;
			}
			// Check for overlap of his exit rectangle with my geom info
			if (checkHisExit && ThePartitionManager->geomCollidesWithGeom(&hisExitPos, hisGeom, them->getOrientation(),
					worldPos, myBounds, angle)) {
				if (shrouded)
					return LBC_SHROUD;
				
				TheTerrainVisual->addFactionBib(them, true);
				return LBC_OBJECTS_IN_THE_WAY;
			}
			// Check both exit rectangles together.
			if (checkMyExit&&checkHisExit&&ThePartitionManager->geomCollidesWithGeom(&hisExitPos, hisGeom, them->getOrientation(),
					&myExitPos, myGeom, angle)) {
				if (shrouded)
					return LBC_SHROUD;
				
				TheTerrainVisual->addFactionBib(them, true);
				return LBC_OBJECTS_IN_THE_WAY;
			}
		}

	}
	return LBC_OK;
}

//-------------------------------------------------------------------------------------------------
/** Query if we can build at this location.  Note that 'build' may be null and is NOT required
	* to be valid to know if a location is legal to build at.  'builderObject' is used
	* for queries that require a pathfind check and should be null if not required */
//-------------------------------------------------------------------------------------------------
LegalBuildCode BuildAssistant::isLocationLegalToBuild( const Coord3D *worldPos,
																											 const ThingTemplate *build,
																											 Real angle,
																											 UnsignedInt options,
																											 const Object *builderObject,
																											 Player *player)
{

	/* You just can't never build off the map, regardless of options.  jba. */
	Region3D mapExtent;
	TheTerrainLogic->getMaximumPathfindExtent(&mapExtent);
	if (!mapExtent.isInRegionNoZ(*worldPos)) {
		return LBC_RESTRICTED_TERRAIN;
	}

	// check shroud level
	// This should be the first check, since returning other errors for shrouded areas could be used to game the system
	if( BitIsSet( options, SHROUD_REVEALED ) )
	{
		{
			Int x, y;
			ThePartitionManager->worldToCell(worldPos->x, worldPos->y, &x, &y);
			Int playerIndex = -1;
			if (builderObject && builderObject->getControllingPlayer())
				playerIndex = builderObject->getControllingPlayer()->getPlayerIndex();
			DEBUG_ASSERTCRASH(playerIndex >= 0, ("isLocationLegalToBuild() needs a builderObject with a team to check for shroud"));
			if( ThePartitionManager->getShroudStatusForPlayer(playerIndex, x, y) != CELLSHROUD_CLEAR )
			{
				return LBC_SHROUD;
			}
		}
	}

	//
	// if NO_OBJECT_OVERLAP is set, we are not allowed to construct 'build' if it would overlap
	// any immobile objects, or an enemy object.  Friendly objects should politely
	// "move out of the way" when you build something where they're standing
	//
	if( BitIsSet( options, NO_OBJECT_OVERLAP ) )
	{
		LegalBuildCode code = isLocationClearOfObjects(worldPos, build, angle, builderObject, options, player);
		if( code != LBC_OK )
		{
			return code;
		}

	}
	//
	// if NO_ENEMY_OBJECT_OVERLAP is set, we are not allowed to construct 'build' if it would overlap
	// any enemy objects.  Friendly objects are ignored.
	//
	if( BitIsSet( options, NO_ENEMY_OBJECT_OVERLAP ) )
	{
		LegalBuildCode code = isLocationClearOfObjects(worldPos, build, angle, builderObject, options, player);
		if( code != LBC_OK )
		{
			return code;
		}
	}

	if (build->isKindOf(KINDOF_CANNOT_BUILD_NEAR_SUPPLIES) && TheGlobalData->m_SupplyBuildBorder > 0)
	{
		// special case for supply centers: can't build too close to supply sources
		PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_SUPPLY_SOURCE), KINDOFMASK_NONE);
		PartitionFilter *filters[] = { &f1, nullptr };

		// see if there are any reasonably close by
		Real range = build->getTemplateGeometryInfo().getBoundingCircleRadius() + TheGlobalData->m_SupplyBuildBorder*2;
		Object* tooClose = ThePartitionManager->getClosestObject(worldPos, range, FROM_BOUNDINGSPHERE_2D, filters);
		if (tooClose != nullptr)
		{
			// yep, see if we would collide with an expanded version
			GeometryInfo tooCloseGeom = tooClose->getGeometryInfo();
			tooCloseGeom.expandFootprint(TheGlobalData->m_SupplyBuildBorder);
			if (ThePartitionManager->geomCollidesWithGeom(
						worldPos,
						build->getTemplateGeometryInfo(),
						angle,
						tooClose->getPosition(),
						tooCloseGeom,
						tooClose->getOrientation()))
			{
				TheTerrainVisual->addFactionBib(tooClose, true, TheGlobalData->m_SupplyBuildBorder);
				return LBC_TOO_CLOSE_TO_SUPPLIES;
			}
		}
	}

	// if clear path is requested check to see if the builder object can get there (unless it's a structure)
	if( BitIsSet( options, CLEAR_PATH ) && builderObject && !builderObject->isKindOf( KINDOF_IMMOBILE ) )
	{
		const AIUpdateInterface *ai = builderObject->getAIUpdateInterface();

		//
		// if there is no AI interface for this object, it cannot possible pass a clear path
		// check since it will never be able to move there
		//
		/**todo remove this if we need to change the semantics of this function of the builderObject
		// actually being able to get to the destination */
		//
		if( ai == nullptr )
			return LBC_NO_CLEAR_PATH;

		if( ai->isQuickPathAvailable( worldPos ) == FALSE )
				return LBC_NO_CLEAR_PATH;

	}

	// check basic terrain restrctions
	if( BitIsSet( options, TERRAIN_RESTRICTIONS ) )
	{

		//
		// we will take "samples" at this resolution across the footprint of where we are
		// going to build the structure
		//
		Real sampleResolution = MAP_XY_FACTOR;

		// get the terrain extents
		Region3D terrainExtent;
		TheTerrainLogic->getExtent( &terrainExtent );

		if (TheTerrainLogic->getLayerForDestination(worldPos) != LAYER_GROUND) {
			// we're on a bridge.   This is somewhat restricted.
			return LBC_RESTRICTED_TERRAIN;
		}

		//
		// check the footprint of where the structure would go to be clear of any non-buildable
		// tiles and to make sure there isn't a restricted tile and to make sure it's "flat" enough
		//
		SampleBuildData sampleData;
		TheTerrainLogic->getExtent( &sampleData.mapRegion );
		sampleData.terrainRestricted = FALSE;
		sampleData.hiZ = terrainExtent.lo.z;  // note we set hi point to lowest point
		sampleData.loZ = terrainExtent.hi.z;  // note we set lo point to highest point

		// quick check at triple res.
		iterateFootprint( build, angle, worldPos, 3*sampleResolution,
		                  checkSampleBuildLocation, &sampleData );
		if( sampleData.terrainRestricted == TRUE )
			return LBC_RESTRICTED_TERRAIN;
		// check if the height across the whole footprint area is too varied (not flat enough)
		if( sampleData.hiZ - sampleData.loZ > TheGlobalData->m_allowedHeightVariationForBuilding )
			return LBC_NOT_FLAT_ENOUGH;

		// careful check at full res.
		iterateFootprint( build, angle, worldPos, sampleResolution,
		                  checkSampleBuildLocation, &sampleData );
		if( sampleData.terrainRestricted == TRUE )
			return LBC_RESTRICTED_TERRAIN;
		// check if the height across the whole footprint area is too varied (not flat enough)
		if( sampleData.hiZ - sampleData.loZ > TheGlobalData->m_allowedHeightVariationForBuilding )
			return LBC_NOT_FLAT_ENOUGH;

	}

	// we passed all the checks
	return LBC_OK;

}

//-------------------------------------------------------------------------------------------------
/** Adds bibs to structures near to worldPos */
//-------------------------------------------------------------------------------------------------
void BuildAssistant::addBibs(const Coord3D *worldPos,
																	const ThingTemplate *build  )
{

	Real range = build->friend_calcVisionRange();
	range += 3*build->getTemplateGeometryInfo().getMajorRadius();

	PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_STRUCTURE), KINDOFMASK_NONE);
	PartitionFilter *filters[] = { &f1, nullptr };

	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange(worldPos, range, FROM_BOUNDINGSPHERE_2D, filters);

	MemoryPoolObjectHolder hold(iter);
	for( Object *them = iter->first(); them; them = iter->next() )
	{

		// ignore any kind of class of objects that we will "remove" for building
		if( isRemovableForConstruction( them ) == TRUE )
			continue;

		if( them->isKindOf( KINDOF_IMMOBILE ) )	{
			TheTerrainVisual->addFactionBib(them, true);
		}

	}

}

//-------------------------------------------------------------------------------------------------
/** Tiling wall object helper function, we can use this to "tile" walls when building.
	* This function will fill out the ARRAY world positions named 'positions' starting
	* at the 'start' point in the world and going to the 'end' point in the world.  No
	* more than 'maxTiles' will be considered and if the position of any one of
	* the objects in the chain is illegal the chain stops there.  The number of objects
	* actually used for the tiling is returned along with a pointer to the array
	* of positions in the tile build info.
	*
	* REQUIRES: Note that the array at 'positions' must be large enough to hold 'maxTiles'
	* entries of positions
	*/
//-------------------------------------------------------------------------------------------------
BuildAssistant::TileBuildInfo *BuildAssistant::buildTiledLocations( const ThingTemplate *thingBeingTiled,
																																		Real angle,
																																		const Coord3D *start,
																																		const Coord3D *end,
																																		Real tilingSize,
																																		Int maxTiles,
																																		Object *builderObject )
{

	// sanity
	if( start == nullptr || end == nullptr )
		return nullptr;

	//
	// we will fill out our own internal array of positions, it better be big enough to
	// accommodate max tiles, if it's not lets make it bigger!
	//
	if( maxTiles > m_buildPositionSize )
	{

		// delete the old array
		delete [] m_buildPositions;

		// create a new one
		m_buildPositions = NEW Coord3D[ maxTiles ];
		m_buildPositionSize = maxTiles;

		//
		// lets try to at least keep sanity here so that we don't have a completely unbounded
		// allocation spot in the code here
		//
		DEBUG_ASSERTCRASH( m_buildPositionSize < 200, ("Do you really need to tile this many objects!!!") );

	}
	Coord3D *positions = m_buildPositions;

	// compute a vector from the start of the line in the world to the end
	Coord3D placementVector;
	placementVector.x = end->x - start->x;
	placementVector.y = end->y - start->y;
	placementVector.z = 0.0f;  //end->z - start->z;

	//
	// get the length of the placement vector in the world, we'll use this to see how
	// many objects we'll need to fill the entire line
	//
	Real placementLength = placementVector.length();

	//
	// first see how many objects we're going to need to go from the placement
	// line start to the placement line end, we are guaranteed to have one thing (+1)
	//
	Int tilesNeeded = REAL_TO_INT(placementLength / tilingSize) + 1;

	// we have a max that we are allowed to build to
	if( tilesNeeded > maxTiles )
		tilesNeeded = maxTiles;

	// we're guaranteed to have at least one object so lets take care of that position now
	positions[ 0 ] = *start;

	//
	// march down each object and set the position based on its position in the line.
	// note that we're skipping the first one because we've already set it explicitly
	// and are guaranteed to have at least one object
	//
	Int tilesUsed = 1;  // actual objects "tiled", we are guaranteed to have at least one
	Coord3D pos;
	Coord3D v = placementVector;
	v.normalize();
	for( Int i = 1; i < tilesNeeded; i++ )
	{

		// compute position of object
		pos.x = v.x * (tilingSize * i) + start->x;
		pos.y = v.y * (tilingSize * i) + start->y;
		pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );

		// check for a legal position to be at and stop the tiling process if that becomes broken
		if( isLocationLegalToBuild( &pos, thingBeingTiled, angle,
																BuildAssistant::USE_QUICK_PATHFIND |
																BuildAssistant::TERRAIN_RESTRICTIONS |
																BuildAssistant::CLEAR_PATH |
																BuildAssistant::NO_OBJECT_OVERLAP |
																BuildAssistant::SHROUD_REVEALED,
																builderObject,
																nullptr) != LBC_OK )
			break;

		// save the position in the output array
		positions[ i ] = pos;

		// we have now actually used one more "tile"
		tilesUsed++;

	}

	// return a struct filled out with the actual tiles used and the array of locations
	static TileBuildInfo tileInfo;
	tileInfo.tilesUsed = tilesUsed;
	tileInfo.positions = positions;
	return &tileInfo;

}

//-------------------------------------------------------------------------------------------------
/** Is the template passed in one of those wall type structures that we "build" in
	* the world after drawing a line of where we want the object(s) placed.  The objects
	* are placed "tiled" in a line */
//-------------------------------------------------------------------------------------------------
Bool BuildAssistant::isLineBuildTemplate( const ThingTemplate *tTemplate )
{
	// sanity
	if( tTemplate == nullptr )
		return FALSE;

	if( tTemplate->isKindOf(KINDOF_LINEBUILD))
		return TRUE;

	return FALSE;  // not a line build object

}

//-------------------------------------------------------------------------------------------------
/** This method will check to make sure it is possible to build the requested unit.  The
	* builder object MUST be present since all construction comes comes from either a Dozer or
	* a production building.  The player must have satisfied the prereqs for 'whatToBuild'
  * This does NOT check available money (see canMakeUnit) */
//-------------------------------------------------------------------------------------------------
Bool BuildAssistant::isPossibleToMakeUnit( Object *builder, const ThingTemplate *whatToBuild ) const
{

	// sanity
	if( builder == nullptr || whatToBuild == nullptr )
		return FALSE;

	// get the command set for the producer object
	const CommandSet *commandSet = TheControlBar->findCommandSet( builder->getCommandSetString() );

	// if no command set we cannot build anything
	if( commandSet == nullptr )
	{

		DEBUG_ASSERTLOG( 0, ("Can't build a '%s' from the builder '%s' because '%s' doesn't have any command set defined",
													whatToBuild->getName().str(),
													builder->getTemplate()->getName().str(),
													builder->getTemplate()->getName().str()) );
		return FALSE;

	}

	//
	// scan the command set, we must find whatToBuild as one of the "build" commands available
	// in the command set.  We want to have all players run this logic on all their machines
	// so that nobody can hack one game and cheat to make stuff that they can't usually make
	//
	const CommandButton *commandButton;
	const CommandButton *foundCommand = nullptr;
	Int i;
	for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
	{

		// get this button
		commandButton = commandSet->getCommandButton(i);
		if( commandButton &&
				(commandButton->getCommandType() == GUI_COMMAND_UNIT_BUILD || commandButton->getCommandType() == GUI_COMMAND_DOZER_CONSTRUCT) &&
				commandButton->getThingTemplate() && commandButton->getThingTemplate()->isEquivalentTo(whatToBuild) )
			foundCommand = commandButton;

	}
	if( foundCommand == nullptr )
		return FALSE;

	// make sure that the player can actually make this unit by checking prereqs and such
	Player *player = builder->getControllingPlayer();
	if( player->canBuild( foundCommand->getThingTemplate() ) == FALSE )
		return FALSE;

	// all is well
	return TRUE;

}


//-------------------------------------------------------------------------------------------------
/** This method will check to make sure it is possible to build the requested unit. and
  * that the player has enough money for 'whatToBuild' */
//-------------------------------------------------------------------------------------------------
CanMakeType BuildAssistant::canMakeUnit( Object *builder, const ThingTemplate *whatToBuild ) const
{

	// sanity
	if( builder == nullptr || whatToBuild == nullptr )
		return CANMAKE_NO_PREREQ;

	if (builder->testScriptStatusBit(OBJECT_STATUS_SCRIPT_DISABLED) || builder->testScriptStatusBit(OBJECT_STATUS_SCRIPT_UNPOWERED))
		return CANMAKE_FACTORY_IS_DISABLED;

	ProductionUpdateInterface* pu = builder->getProductionUpdateInterface();

	//If our builder is actually constructing an object via a special power, then allow it if the templates match.
	//It's possible they won't match because a GLA command center could be in "place sneak attack" mode, and queue
	//up a worker in the meantime.
	if( pu && pu->getSpecialPowerConstructionCommandButton() && pu->getSpecialPowerConstructionCommandButton()->getThingTemplate() == whatToBuild )
	{
		return CANMAKE_OK;
	}

  Player *player = builder->getControllingPlayer();

  // make sure we're not maxed out for this type of unit.
  // Warning: isPossibleToMakeUnit() now implicitly calls
  // canBuildMoreOfType(), so do this check first
  if ( player && !player->canBuildMoreOfType( whatToBuild ) )
    return CANMAKE_MAXED_OUT_FOR_PLAYER;

	if (!isPossibleToMakeUnit(builder, whatToBuild))
		return CANMAKE_NO_PREREQ;

	if (pu != nullptr)
	{
		CanMakeType cmt = pu->canQueueCreateUnit(whatToBuild);
		if (cmt != CANMAKE_OK)
			return cmt;
	}

	// make sure we have enough money to build this
	Money *money = player->getMoney();
	if( whatToBuild->calcCostToBuild( player ) > money->countMoney() )
		return CANMAKE_NO_MONEY;

	// get the command set for the producer object
	return CANMAKE_OK;

}

// ------------------------------------------------------------------------------------------------
/** Some objects will be automatically removed when something is built on them so they
	* aren't considered as obstacles when placing them */
// ------------------------------------------------------------------------------------------------
Bool BuildAssistant::isRemovableForConstruction( Object *obj )
{

	// sanity
	if( obj == nullptr )
		return FALSE;

	if (obj->isKindOf(KINDOF_INERT))
	{
		DEBUG_CRASH(("should not have gotten here."));
		return FALSE;
	}

	// all shrubbery can be removed
	if( obj->isKindOf( KINDOF_SHRUBBERY ) )
		return TRUE;

	// anything with a kindof marked as cleared by build can be removed
	if( obj->isKindOf( KINDOF_CLEARED_BY_BUILD ) )
		return TRUE;

	// Rubble, scrap & dead units can be cleared.
	if( obj->isEffectivelyDead() )
		return TRUE;

	// not removable
	return FALSE;

}

// ------------------------------------------------------------------------------------------------
/** Given that we are about to build 'whatToBuild' remove all the objects that are in the
	* 'footprint' of where the build will take place that can be auto-removed for construction */
// ------------------------------------------------------------------------------------------------
void BuildAssistant::clearRemovableForConstruction( const ThingTemplate *whatToBuild,
																										const Coord3D *pos,
																										Real angle )
{
	ObjectIterator *iter =
			ThePartitionManager->iteratePotentialCollisions( pos,
																											 whatToBuild->getTemplateGeometryInfo(),
																											 angle );
	MemoryPoolObjectHolder hold( iter );
	for( Object *them = iter->first(); them; them = iter->next() )
	{

		// UI feedback objects (always selectable) never get destroyed by construction, but also should never
		// trip any of the other side effects of placing them in isRemovableForConstruction()
		if( isRemovableForConstruction( them ) == TRUE && !them->isKindOf( KINDOF_ALWAYS_SELECTABLE ) )
			TheGameLogic->destroyObject( them );

	}
	TheTerrainVisual->removeTreesAndPropsForConstruction(pos, whatToBuild->getTemplateGeometryInfo(), angle);

}

// ------------------------------------------------------------------------------------------------
/** clearRemovable is set up to delete objects that should cease to exist (for instance, trees).
	* moveObjects will move objects that are owned by the player. It will also return false if
    the association with the object is enemy. (We can move neutral things) */
// ------------------------------------------------------------------------------------------------
Bool BuildAssistant::moveObjectsForConstruction( const ThingTemplate *whatToBuild,
																								 const Coord3D *pos,
																								 Real angle,
																								 Player *playerToBuild )
{
	GeometryInfo gi (GEOMETRY_BOX, false, 10, whatToBuild->getTemplateGeometryInfo().getMajorRadius(),
		whatToBuild->getTemplateGeometryInfo().getMajorRadius());
	if (whatToBuild->getTemplateGeometryInfo().getGeomType()==GEOMETRY_BOX) {
		gi = whatToBuild->getTemplateGeometryInfo();
	}
	ObjectIterator *iter =
			ThePartitionManager->iteratePotentialCollisions( pos,
																											 gi,
																											 angle );
	Bool anyUnmovables = false;
	MemoryPoolObjectHolder hold( iter );

	Real radius = WWMath::SqrtOrigin(WWMath::PowOrigin(gi.getMajorRadius(), 2) + WWMath::PowOrigin(gi.getMinorRadius(), 2));
	radius *= 1.4f;	// Fudge the distance,

	for( Object *them = iter->first(); them; them = iter->next() )
	{
		//ignore land mines, cluster mines and demo traps, since you can build on them
		// doing so will damage them during construction, by the way
		if (them->isKindOf( KINDOF_MINE ))
			continue;

		// same story for inert things: ie, radiation fields, pings, and projectile streams (!)...
		if (them->isKindOf(KINDOF_INERT))
			continue;

		// Skip KINDOF_ALWAYS_SELECTABLE and isRemovableForConstruction, because if it is
		// RemovableForConstruction, it just got destroyed, and won't actually be gone until
		// the end of the frame.  jba.
		if ( !them->isKindOf( KINDOF_ALWAYS_SELECTABLE ) && !isRemovableForConstruction(them) )
		{
			Relationship rel = playerToBuild->getRelationship(them->getTeam());

			if (rel == NEUTRAL || rel == ALLIES)
			{
				// Pick a point outside of the construction and tell those fools to move there.
				// Pick an arbitrary direction and tell the unit to move there, at a distance greater
				// than the object's radius.
				AIUpdateInterface *ai = them->getAIUpdateInterface();
				if (ai)
				{
					// Vary the distance to move between one half the diameter of the building (roughly)
					// and 1.5 times the diameter of the building
					Real variedRadius = GameLogicRandomValueReal(0.5, 1.5) * radius;

					Coord3D destPos;
					Real dir = GameLogicRandomValueReal(-PI, PI);
					Vector3 vec(variedRadius, 0, 0);
					vec.Rotate_Z(dir);

					destPos.x = pos->x + vec.X;
					destPos.y = pos->y + vec.Y;
					destPos.z = pos->z + vec.Z;

					// note that this is an extra-special case... even if the unit's mood is "sleep"
					// it still needs to move here. (units with an ai mood of "sleep" won't respond to
					// any AI commands. this is a special case that ignores the "sleep" ai mood.) (srj)
					ai->aiMoveToPositionEvenIfSleeping(&destPos, CMD_FROM_AI);
				}
				else
				{
					anyUnmovables = true;
				}

			}
			else
			{
				anyUnmovables = true;
			}
		}
	}

	return !anyUnmovables;
}

// ------------------------------------------------------------------------------------------------
/** Start the process for selling the object in question */
// ------------------------------------------------------------------------------------------------
void BuildAssistant::sellObject( Object *obj )
{

	// sanity
	if( obj == nullptr )
		return;

	// we can only sell structures ... sanity check this
	if( obj->isKindOf( KINDOF_STRUCTURE ) == FALSE )
		return;

	// if object already has an entry in the sell list, we shouldn't try to sell it again
	ObjectSellInfo *sellInfo = nullptr;
	ObjectSellListIterator it;
	for( it = m_sellList.begin(); it != m_sellList.end(); ++it )
	{

		sellInfo = (*it);
		if( sellInfo->m_id == obj->getID() )
			break;
		else
			sellInfo = nullptr;

	}
	if( sellInfo != nullptr )
		return;

	// set the construction percent of this object just below 100.0% so we can start counting down
	obj->setConstructionPercent( 99.9f );

	// add this object to the list of objects being sold
	sellInfo = newInstance(ObjectSellInfo);
	sellInfo->m_id = obj->getID();
	sellInfo->m_sellFrame = TheGameLogic->getFrame();
	m_sellList.push_front( sellInfo );

	//
	// set the model condition in the drawable for this object that will show the buildup
	// scaffold and adjust the model height by construction percent
	//
	obj->setModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED) );

#if RETAIL_COMPATIBLE_AIGROUP
	//
	// set this object as under de-construction (sold).  It is still a legal target, since you get the money at
	// the completion of sale.
	//
	obj->setStatus( MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_SOLD, OBJECT_STATUS_UNSELECTABLE ) );
#endif

	// TheSuperHackers @bugfix Mauller 27/06/2025 we need to deselect the object before setting it as unselectable
	// If the object is set unselectable too soon, it fails to be removed from the selection group

	// for everybody, unselect them at this time.  You can't just deselect a drawable.  Selection is a logic property.
	TheGameLogic->deselectObject(obj, PLAYERMASK_ALL, TRUE);

#if !RETAIL_COMPATIBLE_AIGROUP
	//
	// set this object as under de-construction (sold).  It is still a legal target, since you get the money at
	// the completion of sale.
	//
	obj->setStatus( MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_SOLD, OBJECT_STATUS_UNSELECTABLE ) );
#endif

	//
	// set the animation durations so that the regular build up loop animations can be
	// done a bit faster for selling
	//
	Drawable *draw = obj->getDrawable();
	if( draw )
		draw->setAnimationLoopDuration( TOTAL_FRAMES_TO_SELL_OBJECT / 2 );

	// We also need to refund all production for the object at start-of-sell time
	ProductionUpdateInterface *production = obj->getProductionUpdateInterface();
	if ( production )
	{
		production->cancelAndRefundAllProduction();
	}

	// Tell the contain module so it can decide what to do.
	ContainModuleInterface* contain = obj->getContain();
	if( contain )
	{
		contain->onSelling();
	}

	// Tell it to stop attacking or anything else it is doing
	if( obj->getAI() )
		obj->getAI()->aiIdle(CMD_FROM_AI);

	// if it has parking places, kill anything parked there.
	for (BehaviorModule** i = obj->getBehaviorModules(); *i; ++i)
	{
		ParkingPlaceBehaviorInterface* pp = (*i)->getParkingPlaceBehaviorInterface();
		if (pp)
		{
			pp->killAllParkedUnits();
			break;
		}
	}

	// destroy any mines that are owned by this structure, right now.
	// unfortunately, structures don't keep list of mines they own, so we must do
	// this the hard way :-( [fortunately, this doesn't happen very often, so this
	// is probably an acceptable, if icky, solution.] (srj)
	for (Object* mine = TheGameLogic->getFirstObject(); mine; mine = mine->getNextObject())
	{
		if (mine->isKindOf(KINDOF_MINE))
		{
			if (mine->getProducerID() == obj->getID())
			{
				TheGameLogic->destroyObject(mine);
			}
		}
	}

}

