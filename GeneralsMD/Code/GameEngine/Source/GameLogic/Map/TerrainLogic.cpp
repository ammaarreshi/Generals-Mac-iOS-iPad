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

// FILE: TerrainLogic.cpp /////////////////////////////////////////////////////////////////////////
// Logical terrain representation for the game logic side
// Author: Colin Day, April 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine


#include "Common/DataChunk.h"
#include "Common/GameState.h"
#include "Common/MapObject.h"
#include "Common/Radar.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/WellKnownKeys.h"
#include "Common/Xfer.h"

#include "GameClient/TerrainVisual.h"

#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/BridgeBehavior.h"
#include "GameLogic/Module/BridgeTowerBehavior.h"
#include "GameLogic/GhostObject.h"

#include "WWMath/plane.h"
#include "WWMath/tri.h"


// GLOBALS ////////////////////////////////////////////////////////////////////////////////////////
TerrainLogic *TheTerrainLogic = nullptr;

// STATIC /////////////////////////////////////////////////////////////////////////////////////////
WaterHandle TerrainLogic::m_gridWaterHandle;

// Waypoint ///////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Waypoint::Waypoint(WaypointID id, AsciiString name, const Coord3D *pLoc, AsciiString label1, AsciiString label2,
									 AsciiString label3, Bool biDirectional) :
m_name(name),
m_pNext(nullptr),
m_location(*pLoc),
m_id(id),
m_pathLabel1(label1),
m_pathLabel2(label2),
m_pathLabel3(label3),
m_numLinks(0),
m_biDirectional(biDirectional)
{
	Int i;
	for (i=0; i<MAX_LINKS; i++) {
		m_links[i] = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Waypoint::~Waypoint()
{

}

// Bridge ////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
BridgeInfo::BridgeInfo()
{

	from.zero();
	to.zero();
	bridgeWidth = 0.0f;
	fromLeft.zero();
	fromRight.zero();
	toLeft.zero();
	toRight.zero();
  bridgeIndex = 0;
	curDamageState = BODY_PRISTINE;
	damageStateChanged = FALSE;
	bridgeObjectID = INVALID_ID;
	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
		towerObjectID[ i ] = INVALID_ID;

}

// ------------------------------------------------------------------------------------------------
/** Create a tower object for the bridge of the specified type (and therefore position) */
// ------------------------------------------------------------------------------------------------
Object *Bridge::createTower( Coord3D *worldPos,
														 BridgeTowerType towerType,
														 const ThingTemplate *towerTemplate,
														 Object *bridge )
{

	// sanity
	if( towerTemplate == nullptr || bridge == nullptr )
	{

		DEBUG_CRASH(( "Bridge::createTower(): Invalid params" ));
		return nullptr;

	}

	// create the tower object
	Object *tower = TheThingFactory->newObject( towerTemplate, bridge->getTeam() );

	// location information
	Real angle = 0;
	switch( towerType )
	{

		// --------------------------------------------------------------------------------------------
		case BRIDGE_TOWER_FROM_LEFT:
			angle = bridge->getOrientation() + PI;
			break;

		// --------------------------------------------------------------------------------------------
		case BRIDGE_TOWER_FROM_RIGHT:
			angle = bridge->getOrientation() + PI;
			break;

		// --------------------------------------------------------------------------------------------
		case BRIDGE_TOWER_TO_LEFT:
			angle = bridge->getOrientation();
			break;

		// --------------------------------------------------------------------------------------------
		case BRIDGE_TOWER_TO_RIGHT:
			angle = bridge->getOrientation();
			break;

		// --------------------------------------------------------------------------------------------
		default:
			DEBUG_CRASH(( "Bridge::createTower - Unknown bridge tower type '%d'", towerType ));
			return nullptr;

	}

	// set the position and angle
	tower->setPosition( worldPos );
	tower->setOrientation( angle );

	// tie it to the bridge
	BridgeBehaviorInterface *bridgeInterface = BridgeBehavior::getBridgeBehaviorInterfaceFromObject( bridge );
	DEBUG_ASSERTCRASH( bridgeInterface != nullptr, ("Bridge::createTower - no 'BridgeBehaviorInterface' found") );
	if( bridgeInterface )
		bridgeInterface->setTower( towerType, tower );

	// tie the bridge to us
	BridgeTowerBehaviorInterface *bridgeTowerInterface = BridgeTowerBehavior::getBridgeTowerBehaviorInterfaceFromObject( tower );
	DEBUG_ASSERTCRASH( bridgeTowerInterface != nullptr, ("Bridge::createTower - no 'BridgeTowerBehaviorInterface' found") );
	if( bridgeTowerInterface )
	{

		// set bridge object
		bridgeTowerInterface->setBridge( bridge );

		// save our position type
		bridgeTowerInterface->setTowerType( towerType );

	}

	// if the bridge is indestructible, so is this tower
	BodyModuleInterface *bridgeBody = bridge->getBodyModule();
	if( bridgeBody->isIndestructible() )
	{
		BodyModuleInterface *towerBody = tower->getBodyModule();

		towerBody->setIndestructible( TRUE );

	}

	// return the newly created tower
	return tower;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bridge::Bridge(BridgeInfo &theInfo, Dict *props, AsciiString bridgeTemplateName) :
m_bridgeInfo(theInfo)
{

	// save the template name
	m_templateName = bridgeTemplateName;

	//Coord3D fromLeft, fromRight, toLeft, toRight; /// The 4 corners of the rectangle that the bridge covers.
	m_bounds.lo.x = m_bridgeInfo.fromLeft.x;
	m_bounds.lo.y = m_bridgeInfo.fromLeft.y;
	m_bounds.hi = m_bounds.lo;
	if (m_bounds.lo.x > m_bridgeInfo.fromRight.x) m_bounds.lo.x = m_bridgeInfo.fromRight.x;
	if (m_bounds.lo.y > m_bridgeInfo.fromRight.y) m_bounds.lo.y = m_bridgeInfo.fromRight.y;
	if (m_bounds.hi.x < m_bridgeInfo.fromRight.x) m_bounds.hi.x = m_bridgeInfo.fromRight.x;
	if (m_bounds.hi.y < m_bridgeInfo.fromRight.y) m_bounds.hi.y = m_bridgeInfo.fromRight.y;
	if (m_bounds.lo.x > m_bridgeInfo.toLeft.x) m_bounds.lo.x = m_bridgeInfo.toLeft.x;
	if (m_bounds.lo.y > m_bridgeInfo.toLeft.y) m_bounds.lo.y = m_bridgeInfo.toLeft.y;
	if (m_bounds.hi.x < m_bridgeInfo.toLeft.x) m_bounds.hi.x = m_bridgeInfo.toLeft.x;
	if (m_bounds.hi.y < m_bridgeInfo.toLeft.y) m_bounds.hi.y = m_bridgeInfo.toLeft.y;
	if (m_bounds.lo.x > m_bridgeInfo.toRight.x) m_bounds.lo.x = m_bridgeInfo.toRight.x;
	if (m_bounds.lo.y > m_bridgeInfo.toRight.y) m_bounds.lo.y = m_bridgeInfo.toRight.y;
	if (m_bounds.hi.x < m_bridgeInfo.toRight.x) m_bounds.hi.x = m_bridgeInfo.toRight.x;
	if (m_bounds.hi.y < m_bridgeInfo.toRight.y) m_bounds.hi.y = m_bridgeInfo.toRight.y;

	m_bridgeInfo.curDamageState = BODY_PRISTINE;


	static const ThingTemplate* genericBridgeTemplate = TheThingFactory->findTemplate("GenericBridge");
	if (!genericBridgeTemplate) {
		DEBUG_LOG(("*** GenericBridge template not found."));
		return;
	}
	Object *bridge = TheThingFactory->newObject(genericBridgeTemplate, nullptr);
	Coord3D center;
	center.x = (m_bridgeInfo.fromLeft.x + m_bridgeInfo.toRight.x)/2.0f;
	center.y = (m_bridgeInfo.fromLeft.y + m_bridgeInfo.toRight.y)/2.0f;
	center.z = (m_bridgeInfo.fromLeft.z + m_bridgeInfo.toRight.z)/2.0f;
	bridge->setPosition(&center);
	m_bridgeInfo.bridgeObjectID = bridge->getID();
	bridge->updateObjValuesFromMapProperties(props);

	//
	// we'll say the angle of this object representing the bridge is from the 'from' side
	// to the 'to' side.
	//
	Coord2D v;
	v.x = m_bridgeInfo.toLeft.x - m_bridgeInfo.fromLeft.x;
	v.y = m_bridgeInfo.toLeft.y - m_bridgeInfo.fromLeft.y;
	bridge->setOrientation( v.toAngle() );

	v.x = m_bridgeInfo.toLeft.x - m_bridgeInfo.toRight.x;
	v.y = m_bridgeInfo.toLeft.y - m_bridgeInfo.toRight.y;
	v.normalize();

	// get the template of the bridge
	TerrainRoadType *bridgeTemplate = TheTerrainRoads->findBridge( bridgeTemplateName );
	if( bridgeTemplate == nullptr ) {
		DEBUG_LOG(( "*** Bridge Template Not Found '%s'.", bridgeTemplateName.str() ));
		return;
	}

#define no_BRIDGE_TOWERS // since they aren't destructable, don't need towers.
#if BRIDGE_TOWERS
	// initialize each of the tower positions to that of the bridge info bounding rect
	Coord3D towerPos[ BRIDGE_MAX_TOWERS ];
	towerPos[ BRIDGE_TOWER_FROM_LEFT ] = m_bridgeInfo.fromLeft;
	towerPos[ BRIDGE_TOWER_FROM_RIGHT ] = m_bridgeInfo.fromRight;
	towerPos[ BRIDGE_TOWER_TO_LEFT ] = m_bridgeInfo.toLeft;
	towerPos[ BRIDGE_TOWER_TO_RIGHT ] = m_bridgeInfo.toRight;

	// create objects targetable objects for the 4 tower pieces
	const ThingTemplate *towerTemplate;
	BridgeTowerType type;
	Object *tower;
	Real offset = PATHFIND_CELL_SIZE_F/2.0f;
	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
	{

		// create the tower
		type = (BridgeTowerType)i;
		towerTemplate = TheThingFactory->findTemplate( bridgeTemplate->getTowerObjectName( type ) );
		if (towerTemplate) {
			offset = towerTemplate->getTemplateGeometryInfo().getMajorRadius();
		}
		Coord3D pos = towerPos[type];
		switch( type )
		{
			case BRIDGE_TOWER_FROM_LEFT:
			case BRIDGE_TOWER_TO_LEFT:
				pos.x += v.x*offset;
				pos.y += v.y*offset;
				break;
			case BRIDGE_TOWER_FROM_RIGHT:
			case BRIDGE_TOWER_TO_RIGHT:
				pos.x -= v.x*offset;
				pos.y -= v.y*offset;
				break;

		}
		tower = createTower( &pos, type, towerTemplate, bridge );

		// store the tower object ID
		m_bridgeInfo.towerObjectID[ i ] = tower->getID();

	}
#endif

	m_next = nullptr;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bridge::Bridge(Object *bridgeObj)
{

	// save the template name
	m_templateName = bridgeObj->getTemplate()->getName();

	DEBUG_ASSERTLOG( bridgeObj->getGeometryInfo().getGeomType()==GEOMETRY_BOX, ("Bridges need to be rectangles."));

	const Coord3D *pos = bridgeObj->getPosition();
	Real angle = bridgeObj->getOrientation();

	Real halfsizeX = bridgeObj->getGeometryInfo().getMajorRadius();
	Real halfsizeY = bridgeObj->getGeometryInfo().getMinorRadius();
	m_bridgeInfo.bridgeWidth = 2*halfsizeY;

	Real c = (Real)Cos(angle);
	Real s = (Real)Sin(angle);

	m_bridgeInfo.fromLeft.set(pos->x-halfsizeX*c-halfsizeY*s, pos->y + halfsizeY*c - halfsizeX*s, pos->z);
	m_bridgeInfo.toLeft.set(pos->x+halfsizeX*c-halfsizeY*s, pos->y + halfsizeY*c + halfsizeX*s, pos->z);
	m_bridgeInfo.fromRight.set(pos->x-halfsizeX*c+halfsizeY*s, pos->y - halfsizeY*c - halfsizeX*s, pos->z);
	m_bridgeInfo.toRight.set(pos->x+halfsizeX*c+halfsizeY*s, pos->y - halfsizeY*c + halfsizeX*s, pos->z);

	m_bridgeInfo.from.x = (m_bridgeInfo.fromLeft.x + m_bridgeInfo.fromRight.x)/2.0f;
	m_bridgeInfo.from.y = (m_bridgeInfo.fromLeft.y + m_bridgeInfo.fromRight.y)/2.0f;
	m_bridgeInfo.from.z = (m_bridgeInfo.fromLeft.z + m_bridgeInfo.fromRight.z)/2.0f;

	m_bridgeInfo.to.x = (m_bridgeInfo.toLeft.x + m_bridgeInfo.toRight.x)/2.0f;
	m_bridgeInfo.to.y = (m_bridgeInfo.toLeft.y + m_bridgeInfo.toRight.y)/2.0f;
	m_bridgeInfo.to.z = (m_bridgeInfo.toLeft.z + m_bridgeInfo.toRight.z)/2.0f;

	//Coord3D fromLeft, fromRight, toLeft, toRight; /// The 4 corners of the rectangle that the bridge covers.
	m_bounds.lo.x = m_bridgeInfo.fromLeft.x;
	m_bounds.lo.y = m_bridgeInfo.fromLeft.y;
	m_bounds.hi = m_bounds.lo;
	if (m_bounds.lo.x > m_bridgeInfo.fromRight.x) m_bounds.lo.x = m_bridgeInfo.fromRight.x;
	if (m_bounds.lo.y > m_bridgeInfo.fromRight.y) m_bounds.lo.y = m_bridgeInfo.fromRight.y;
	if (m_bounds.hi.x < m_bridgeInfo.fromRight.x) m_bounds.hi.x = m_bridgeInfo.fromRight.x;
	if (m_bounds.hi.y < m_bridgeInfo.fromRight.y) m_bounds.hi.y = m_bridgeInfo.fromRight.y;
	if (m_bounds.lo.x > m_bridgeInfo.toLeft.x) m_bounds.lo.x = m_bridgeInfo.toLeft.x;
	if (m_bounds.lo.y > m_bridgeInfo.toLeft.y) m_bounds.lo.y = m_bridgeInfo.toLeft.y;
	if (m_bounds.hi.x < m_bridgeInfo.toLeft.x) m_bounds.hi.x = m_bridgeInfo.toLeft.x;
	if (m_bounds.hi.y < m_bridgeInfo.toLeft.y) m_bounds.hi.y = m_bridgeInfo.toLeft.y;
	if (m_bounds.lo.x > m_bridgeInfo.toRight.x) m_bounds.lo.x = m_bridgeInfo.toRight.x;
	if (m_bounds.lo.y > m_bridgeInfo.toRight.y) m_bounds.lo.y = m_bridgeInfo.toRight.y;
	if (m_bounds.hi.x < m_bridgeInfo.toRight.x) m_bounds.hi.x = m_bridgeInfo.toRight.x;
	if (m_bounds.hi.y < m_bridgeInfo.toRight.y) m_bounds.hi.y = m_bridgeInfo.toRight.y;

	m_bridgeInfo.curDamageState = BODY_PRISTINE;

	m_bridgeInfo.bridgeObjectID = bridgeObj->getID();

	// get the template of the bridge
	AsciiString bridgeTemplateName = bridgeObj->getTemplate()->getName();
	TerrainRoadType *bridgeTemplate = TheTerrainRoads->findBridge( bridgeTemplateName );
	if( bridgeTemplate == nullptr ) {
		DEBUG_LOG(( "*** Bridge Template Not Found '%s'.", bridgeTemplateName.str() ));
		return;
	}

	Coord2D v;
	v.x = m_bridgeInfo.toLeft.x - m_bridgeInfo.toRight.x;
	v.y = m_bridgeInfo.toLeft.y - m_bridgeInfo.toRight.y;
	v.normalize();

	// initialize each of the tower positions to that of the bridge info bounding rect
	Coord3D towerPos[ BRIDGE_MAX_TOWERS ];
	towerPos[ BRIDGE_TOWER_FROM_LEFT ] = m_bridgeInfo.fromLeft;
	towerPos[ BRIDGE_TOWER_FROM_RIGHT ] = m_bridgeInfo.fromRight;
	towerPos[ BRIDGE_TOWER_TO_LEFT ] = m_bridgeInfo.toLeft;
	towerPos[ BRIDGE_TOWER_TO_RIGHT ] = m_bridgeInfo.toRight;

	Real offset = PATHFIND_CELL_SIZE_F/2.0f;
	// create objects targetable objects for the 4 tower pieces
	const ThingTemplate *towerTemplate;
	BridgeTowerType type;
	Object *tower;
	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
	{

		type = (BridgeTowerType)i;
		towerTemplate = TheThingFactory->findTemplate( bridgeTemplate->getTowerObjectName( type ) );
		if (towerTemplate) {
			offset = towerTemplate->getTemplateGeometryInfo().getMajorRadius();
		}
		Coord3D pos = towerPos[type];
		switch( type )
		{
			case BRIDGE_TOWER_FROM_LEFT:
			case BRIDGE_TOWER_TO_LEFT:
				pos.x += v.x*offset;
				pos.y += v.y*offset;
				break;
			case BRIDGE_TOWER_FROM_RIGHT:
			case BRIDGE_TOWER_TO_RIGHT:
				pos.x -= v.x*offset;
				pos.y -= v.y*offset;
				break;

		}
		tower = createTower( &pos, type, towerTemplate, bridgeObj );
		if( tower )
		{
			// store the tower object ID
			m_bridgeInfo.towerObjectID[ i ] = tower->getID();
		}

	}

	m_next = nullptr;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bridge::~Bridge()
{

}


//-------------------------------------------------------------------------------------------------
/** isPointOnBridge - see if point is on bridge. */
//-------------------------------------------------------------------------------------------------
Bool Bridge::isPointOnBridge(const Coord3D *pLoc)
{
	if (pLoc->x < m_bounds.lo.x) return(false);
	if (pLoc->x > m_bounds.hi.x) return(false);
	if (pLoc->y < m_bounds.lo.y) return(false);
	if (pLoc->y > m_bounds.hi.y) return(false);

	Vector3 testPt(pLoc->x, pLoc->y, pLoc->z);
	Vector3 left1(m_bridgeInfo.fromLeft.x, m_bridgeInfo.fromLeft.y, m_bridgeInfo.fromLeft.z);
	Vector3 right1(m_bridgeInfo.fromRight.x, m_bridgeInfo.fromRight.y, m_bridgeInfo.fromRight.z);
	Vector3 left2(m_bridgeInfo.toLeft.x, m_bridgeInfo.toLeft.y, m_bridgeInfo.toLeft.z);
	Vector3 right2(m_bridgeInfo.toRight.x, m_bridgeInfo.toRight.y, m_bridgeInfo.toRight.z);

	unsigned char flags;

	if (Point_In_Triangle_2D(left1, right1, left2, testPt, 0, 1, flags)) {
		return true;
	}
	if (Point_In_Triangle_2D(right1, left2, right2, testPt, 0, 1, flags)) {
		return true;
	}
	return(false);
}

/*-------------------------------------------------------------------------------------------------
/** Clip a floating point line to the region provided.  The source line runs from p1 to p2, and is clipped
	* using the clipRegion.
	*
	* Return values:
	*				TRUE  - Line intersects the region
	*				FALSE - Line does not intersect the region
	*/
//-------------------------------------------------------------------------------------------------
Bool LineInRegion( const Coord2D *p1, const Coord2D *p2, const Region2D *clipRegion )
{
	enum { CLIP_LEFT  = 0x01,
				CLIP_RIGHT  = 0x02,
				CLIP_BOTTOM = 0x04,
				CLIP_TOP	  = 0x08 };
	Real x1, y1, x2, y2;
	Real clipLeft;
	Real clipRight;
	Real clipTop;
	Real clipBottom;
	Int clipCode1;
	Int clipCode2;
	Real diff;

	// Use clip window that includes bottom right pixel
	clipLeft = clipRegion->lo.x;
	clipRight = clipRegion->hi.x;
	clipTop = clipRegion->lo.y;
	clipBottom = clipRegion->hi.y;

	x1 = p1->x;
	y1 = p1->y;
	x2 = p2->x;
	y2 = p2->y;

	// Test first point
	clipCode1 = 0;

	if (x1 < clipLeft)
		clipCode1 = CLIP_LEFT;
	else
	if (x1 > clipRight)
		clipCode1 = CLIP_RIGHT;

	if (y1 < clipTop)
		clipCode1 |= CLIP_TOP;
	else
	if (y1 > clipBottom)
		clipCode1 |= CLIP_BOTTOM;


	// Test second point
	clipCode2 = 0;

	if (x2 < clipLeft)
		clipCode2 = CLIP_LEFT;
	else
	if (x2 > clipRight)
		clipCode2 = CLIP_RIGHT;

	if (y2 < clipTop)
		clipCode2 |= CLIP_TOP;
	else
	if (y2 > clipBottom)
		clipCode2 |= CLIP_BOTTOM;


	// Both points inside window?
	if ((clipCode1 | clipCode2) == 0)
	{
		return TRUE;
	}

	// Both points outside window?
	if (clipCode1 & clipCode2)
		return FALSE;

	// First point outside window?
	if (clipCode1)
	{
		if (clipCode1 & CLIP_TOP)
		{
			if ((diff = (y2 - y1)) == 0)
				return FALSE;
			x1 += (x2 - x1) * (clipTop - y1) / diff;
			y1 = clipTop;
		}
		else
		if (clipCode1 & CLIP_BOTTOM)
		{
			if ((diff = (y2 - y1)) == 0)
				return FALSE;
			x1 += (x2 - x1) * (clipBottom - y1) / diff;
			y1 = clipBottom;
		}

		if (x1 > clipRight)
		{
			if ((diff = (x2 - x1)) == 0)
				return FALSE;
			y1 += (y2 - y1) * (clipRight - x1) / diff;
			x1 = clipRight;
		}
		else
		if (x1 < clipLeft)
		{
			if ((diff = (x2 - x1)) == 0)
				return FALSE;
			y1 += (y2 - y1) * (clipLeft - x1) / diff;
			x1 = clipLeft;
		}
	}

	// Second point outside window?
	if (clipCode2)
	{
		if (clipCode2 & CLIP_TOP)
		{
			if ((diff = (y2 - y1)) == 0)
				return FALSE;
			x2 += (x2 - x1) * (clipTop - y2) / diff;
			y2 = clipTop;
		}
		else
		if (clipCode2 & CLIP_BOTTOM)
		{
			if ((diff = (y2 - y1)) == 0)
				return FALSE;
			x2 += (x2 - x1) * (clipBottom - y2) / diff;
			y2 = clipBottom;
		}

		if (x2 > clipRight)
		{
			if ((diff = (x2 - x1)) == 0)
				return FALSE;
			y2 += (y2 - y1) * (clipRight - x2) / diff;
			x2 = clipRight;
		}
		else
		if (x2 < clipLeft)
		{
			if ((diff = (x2 - x1)) == 0)
				return FALSE;
			y2 += (y2 - y1) * (clipLeft - x2) / diff;
			x2 = clipLeft;
		}
	}

	// Line is visible
	return (x1 >= clipLeft && x1 <= clipRight &&
		    y1 >= clipTop && y1 <= clipBottom &&
			x2 >= clipLeft && x2 <= clipRight &&
			y2 >= clipTop && y2 <= clipBottom);

}

static Bool PointInRegion2D( const Coord3D *pt, const Region2D *clipRegion )
{
	return (pt->x>=clipRegion->lo.x &&
					pt->y>=clipRegion->lo.y &&
					pt->x<=clipRegion->hi.x &&
					pt->y<=clipRegion->hi.y);
}


//-------------------------------------------------------------------------------------------------
/** isCellOnEnd - see if cell is on the end of the bridge. */
//-------------------------------------------------------------------------------------------------
Bool Bridge::isCellOnEnd(const Region2D *cell)
{
	Coord3D endVector;
	endVector.x = m_bridgeInfo.fromRight.x - m_bridgeInfo.fromLeft.x;
	endVector.y = m_bridgeInfo.fromRight.y - m_bridgeInfo.fromLeft.y;
	endVector.z = m_bridgeInfo.fromRight.z - m_bridgeInfo.fromLeft.z;
	endVector.normalize();
	// Offset by 1 pathfind cell.
	endVector.x *= PATHFIND_CELL_SIZE;
	endVector.y *= PATHFIND_CELL_SIZE;

	Coord3D fromLeft = m_bridgeInfo.fromLeft;
	fromLeft.x += endVector.x;
	fromLeft.y += endVector.y;

	Coord3D fromRight = m_bridgeInfo.fromRight;
	fromRight.x -= endVector.x;
	fromRight.y -= endVector.y;

	Coord3D toLeft = m_bridgeInfo.toLeft;
	toLeft.x += endVector.x;
	toLeft.y += endVector.y;

	Coord3D toRight = m_bridgeInfo.toRight;
	toRight.x -= endVector.x;
	toRight.y -= endVector.y;

/*	if (PointInRegion2D(&fromLeft, cell)) return false;
	if (PointInRegion2D(&fromRight, cell)) return false;
	if (PointInRegion2D(&toLeft, cell)) return false;
	if (PointInRegion2D(&toRight, cell)) return false; */
	Coord2D line1, line2;
	line1.x = fromLeft.x;
	line1.y = fromLeft.y;
	line2.x = fromRight.x;
	line2.y = fromRight.y;
	if (LineInRegion(&line1, &line2, cell)) {
		return true;
	}
	line1.x = toLeft.x;
	line1.y = toLeft.y;
	line2.x = toRight.x;
	line2.y = toRight.y;
	if (LineInRegion(&line1, &line2, cell)) {
		return true;
	}
	return(false);
}

//-------------------------------------------------------------------------------------------------
/** isCellOnSide - see if cell is on the end of the bridge. */
//-------------------------------------------------------------------------------------------------
Bool Bridge::isCellOnSide(const Region2D *cell)
{
	Coord3D endVector;
	endVector.x = m_bridgeInfo.fromRight.x - m_bridgeInfo.fromLeft.x;
	endVector.y = m_bridgeInfo.fromRight.y - m_bridgeInfo.fromLeft.y;
	endVector.z = m_bridgeInfo.fromRight.z - m_bridgeInfo.fromLeft.z;
	endVector.normalize();
	// Offset by 1 pathfind cell.
	endVector.x *= PATHFIND_CELL_SIZE*0.51f;
	endVector.y *= PATHFIND_CELL_SIZE*0.51f;

	Coord3D fromLeft = m_bridgeInfo.fromLeft;
	fromLeft.x -= endVector.x;
	fromLeft.y -= endVector.y;

	Coord3D fromRight = m_bridgeInfo.fromRight;
	fromRight.x += endVector.x;
	fromRight.y += endVector.y;

	Coord3D toLeft = m_bridgeInfo.toLeft;
	toLeft.x -= endVector.x;
	toLeft.y -= endVector.y;

	Coord3D toRight = m_bridgeInfo.toRight;
	toRight.x += endVector.x;
	toRight.y += endVector.y;

	Coord2D line1, line2;
	line1.x = fromLeft.x;
	line1.y = fromLeft.y;
	line2.x = toLeft.x;
	line2.y = toLeft.y;
	if (LineInRegion(&line1, &line2, cell)) {
		return true;
	}
	line1.x = fromRight.x;
	line1.y = fromRight.y;
	line2.x = toRight.x;
	line2.y = toRight.y;
	if (LineInRegion(&line1, &line2, cell)) {
		return true;
	}
	fromLeft.x -= endVector.x;
	fromLeft.y -= endVector.y;

	fromRight.x += endVector.x;
	fromRight.y += endVector.y;

	toLeft.x -= endVector.x;
	toLeft.y -= endVector.y;

	toRight.x += endVector.x;
	toRight.y += endVector.y;

	line1.x = fromLeft.x;
	line1.y = fromLeft.y;
	line2.x = toLeft.x;
	line2.y = toLeft.y;
	if (LineInRegion(&line1, &line2, cell)) {
		return true;
	}
	line1.x = fromRight.x;
	line1.y = fromRight.y;
	line2.x = toRight.x;
	line2.y = toRight.y;
	if (LineInRegion(&line1, &line2, cell)) {
		return true;
	}
	return(false);
}

//-------------------------------------------------------------------------------------------------
/** isCellEntryPoint - Is a pathfind cell a spot to move onto the bridge. */
//-------------------------------------------------------------------------------------------------
Bool Bridge::isCellEntryPoint(const Region2D *cell)
{
	Coord3D endVector;
	endVector.x = m_bridgeInfo.fromRight.x - m_bridgeInfo.fromLeft.x;
	endVector.y = m_bridgeInfo.fromRight.y - m_bridgeInfo.fromLeft.y;
	endVector.z = m_bridgeInfo.fromRight.z - m_bridgeInfo.fromLeft.z;
	endVector.normalize();
	// Offset by 1 pathfind cell.
	endVector.x *= PATHFIND_CELL_SIZE;
	endVector.y *= PATHFIND_CELL_SIZE;
	Coord3D bridgeVector;
	bridgeVector.x = m_bridgeInfo.to.x - m_bridgeInfo.from.x;
	bridgeVector.y = m_bridgeInfo.to.y - m_bridgeInfo.from.y;
	bridgeVector.z = m_bridgeInfo.to.z - m_bridgeInfo.from.z;
	bridgeVector.normalize();
	// Offset by 1/2 pathfind cell.
	bridgeVector.x *= PATHFIND_CELL_SIZE/2;
	bridgeVector.y *= PATHFIND_CELL_SIZE/2;

	Coord3D fromLeft = m_bridgeInfo.fromLeft;
	fromLeft.x -= bridgeVector.x;
	fromLeft.y -= bridgeVector.y;
	fromLeft.x += endVector.x;
	fromLeft.y += endVector.y;

	Coord3D fromRight = m_bridgeInfo.fromRight;
	fromRight.x -= bridgeVector.x;
	fromRight.y -= bridgeVector.y;
	fromRight.x -= endVector.x;
	fromRight.y -= endVector.y;

	Coord3D toLeft = m_bridgeInfo.toLeft;
	toLeft.x += bridgeVector.x;
	toLeft.y += bridgeVector.y;
 	toLeft.x += endVector.x;
	toLeft.y += endVector.y;

	Coord3D toRight = m_bridgeInfo.toRight;
	toRight.x += bridgeVector.x;
	toRight.y += bridgeVector.y;
 	toRight.x -= endVector.x;
	toRight.y -= endVector.y;

/*	if (PointInRegion2D(&fromLeft, cell)) return false;
	if (PointInRegion2D(&fromRight, cell)) return false;
	if (PointInRegion2D(&toLeft, cell)) return false;
	if (PointInRegion2D(&toRight, cell)) return false;
	*/
	Coord2D line1, line2;
	line1.x = fromLeft.x;
	line1.y = fromLeft.y;
	line2.x = fromRight.x;
	line2.y = fromRight.y;
	if (LineInRegion(&line1, &line2, cell)) {
		return true;
	}
	line1.x = toLeft.x;
	line1.y = toLeft.y;
	line2.x = toRight.x;
	line2.y = toRight.y;
	if (LineInRegion(&line1, &line2, cell)) {
		return true;
	}
	return(false);

}

//-------------------------------------------------------------------------------------------------
/** pickBridge - see if point is on bridge. */
//-------------------------------------------------------------------------------------------------
Drawable *Bridge::pickBridge(const Vector3 &from, const Vector3 &to, Vector3 *pos)
{

	Vector3 left1(m_bridgeInfo.fromLeft.x, m_bridgeInfo.fromLeft.y, m_bridgeInfo.fromLeft.z);
	Vector3 right1(m_bridgeInfo.fromRight.x, m_bridgeInfo.fromRight.y, m_bridgeInfo.fromRight.z);
	Vector3 left2(m_bridgeInfo.toLeft.x, m_bridgeInfo.toLeft.y, m_bridgeInfo.toLeft.z);

	PlaneClass plane(left1, right1, left2);
	Real t;
	plane.Compute_Intersection(from, to, &t);
	Vector3 intersectPos;
	intersectPos = from + (to-from) * t;

	Coord3D loc;
	loc.x = intersectPos.X;
	loc.y = intersectPos.Y;
	loc.z = intersectPos.Z;

	if (isPointOnBridge(&loc)) {
		*pos = intersectPos;
		//DEBUG_LOG(("Picked bridge %.2f, %.2f, %.2f", intersectPos.X, intersectPos.Y, intersectPos.Z));
		Object *bridge = TheGameLogic->findObjectByID(m_bridgeInfo.bridgeObjectID);
		if (bridge) {
			return bridge->getDrawable();
		}
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** updateDamageState - Update the damage state. */
//-------------------------------------------------------------------------------------------------
void Bridge::updateDamageState()
{
	m_bridgeInfo.damageStateChanged = false;
	if (m_bridgeInfo.bridgeObjectID==0) return;
	Object *bridge = TheGameLogic->findObjectByID(m_bridgeInfo.bridgeObjectID);
	if (bridge) {
		// get object damage state
		{
			BodyDamageType damageState = bridge->getBodyModule()->getDamageState();
			BodyDamageType curState = m_bridgeInfo.curDamageState;
			if (damageState != curState) {
				m_bridgeInfo.curDamageState = damageState;
				if (damageState == BODY_RUBBLE) {
					TheAI->pathfinder()->changeBridgeState(m_layer, false);
					m_bridgeInfo.damageStateChanged = true;
					Object *obj;
					for (obj = TheGameLogic->getFirstObject(); obj; obj=obj->getNextObject()) {
						if (obj->getLayer() == m_layer) {
							// don't consider the bridge health, 'cuz it's already dead. (srj)
							const Bool considerBridgeHealth = false;
							if (TheTerrainLogic->objectInteractsWithBridgeLayer(obj, obj->getLayer(), considerBridgeHealth))
							{
								// srj sez: if we use this threshold, then stuff on the bridge apron doesn't die but
								// might sink thru the eyecandy of bridge drbris, looking funny. so now we just indiscriminately
								// kill everything that was on the bridge, regardless of height they might fall.
								//Real deltaHeight = obj->getPosition()->z - TheTerrainLogic->getGroundHeight(obj->getPosition()->x, obj->getPosition()->y);
								//if (deltaHeight>PATHFIND_CELL_SIZE_F * 0.5f)
								{
									// The object fell off the bridge.
									// Destroy it.
									DamageInfo extraDamageInfo;
									extraDamageInfo.in.m_damageType = DAMAGE_FALLING;
									extraDamageInfo.in.m_deathType = DEATH_SPLATTED;
									extraDamageInfo.in.m_sourceID = obj->getID();
									extraDamageInfo.in.m_amount = HUGE_DAMAGE_AMOUNT;
									obj->attemptDamage(&extraDamageInfo);
								}
							}
						}
					}
				}
				if (curState==BODY_RUBBLE) {

					//
					// we do not set the bridge as usable if scaffolding is up ... the scaffolding
					// code will take care of that
					//
					BridgeBehaviorInterface *bbi = BridgeBehavior::getBridgeBehaviorInterfaceFromObject( bridge );
					if( bbi == nullptr || bbi->isScaffoldPresent() == FALSE )
						TheAI->pathfinder()->changeBridgeState(m_layer, true);
					m_bridgeInfo.damageStateChanged = true;
				}
			}
		}
	}	else {
		m_bridgeInfo.bridgeObjectID = INVALID_ID;
		DEBUG_CRASH(("Bridge object disappeared - unexpected. jba."));
	}

}


//-------------------------------------------------------------------------------------------------
/** getHeight - Get the height for an object on bridge.. */
//-------------------------------------------------------------------------------------------------
Real Bridge::getBridgeHeight(const Coord3D *pLoc, Coord3D* normal)
{
	Vector3 left1(m_bridgeInfo.fromLeft.x, m_bridgeInfo.fromLeft.y, m_bridgeInfo.fromLeft.z);
	Vector3 right1(m_bridgeInfo.fromRight.x, m_bridgeInfo.fromRight.y, m_bridgeInfo.fromRight.z);
	Vector3 left2(m_bridgeInfo.toLeft.x, m_bridgeInfo.toLeft.y, m_bridgeInfo.toLeft.z);
	PlaneClass plane(left1, right1, left2);
	const Real factor = 1000.0f;
	Vector3 bottom(pLoc->x, pLoc->y, 0);
	Vector3 top(pLoc->x, pLoc->y, factor);
	Real t;
	plane.Compute_Intersection(bottom, top, &t);
	if (normal) {
		normal->x = plane.N.X;
		normal->y = plane.N.Y;
		normal->z = plane.N.Z;
	}

	return t*factor;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TerrainLogic::TerrainLogic()
{
	Int i;

	m_activeBoundary = 0;
	m_waterGridEnabled = FALSE;
	for( i = 0; i < MAX_DYNAMIC_WATER; ++i )
	{

		m_waterToUpdate[ i ].waterTable = nullptr;
		m_waterToUpdate[ i ].changePerFrame = 0.0f;
		m_waterToUpdate[ i ].targetHeight = 0.0f;
		m_waterToUpdate[ i ].damageAmount = 0.0f;
		m_waterToUpdate[ i ].currentHeight = 0.0f;

	}
	m_numWaterToUpdate = 0;

	m_waypointListHead = nullptr;
	m_bridgeListHead = nullptr;
	m_mapData = nullptr;
	m_bridgeDamageStatesChanged = FALSE;
	m_mapDX = 0;
	m_mapDY = 0;


}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TerrainLogic::~TerrainLogic()
{

	reset(); // just in case

}

//-------------------------------------------------------------------------------------------------
/** Init */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::init()
{

}

//-------------------------------------------------------------------------------------------------
/** Reset */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::reset()
{

	deleteWaypoints();
	deleteBridges();
	PolygonTrigger::deleteTriggers();
	m_numWaterToUpdate = 0;

}

//-------------------------------------------------------------------------------------------------
/** Update */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::update()
{

	// bridge damage states have not changed this frame now
	m_bridgeDamageStatesChanged = false;

	// update any water tables that we need to
	if( m_numWaterToUpdate )
	{
		const WaterHandle *water;
		Real changePerFrame,
				 damageAmount,
				 targetHeight,
				 currentHeight;
		Bool finalTransition,
				 doDamageThisFrame = (TheGameLogic->getFrame() % LOGICFRAMES_PER_SECOND) == 0;

		for( Int i = m_numWaterToUpdate - 1; i >= 0; --i )
		{

			// get the water info
			water = m_waterToUpdate[ i ].waterTable;
			changePerFrame = m_waterToUpdate[ i ].changePerFrame;
			targetHeight = m_waterToUpdate[ i ].targetHeight;
			damageAmount = m_waterToUpdate[ i ].damageAmount;
			currentHeight = m_waterToUpdate[ i ].currentHeight;

			//
			// check to see if this change per frame will get us to our target height, if so
			// we adjust the changePerFrame to make us be exactly at our target height, and after
			// the change we will remove our entry from this update phase
			//
			finalTransition = FALSE;
			if( changePerFrame > 0 )
			{

				if( currentHeight + changePerFrame >= targetHeight )
					finalTransition = TRUE;

			}
			else
			{

				if( currentHeight + changePerFrame <= targetHeight )
					finalTransition = TRUE;

			}

			if( finalTransition == TRUE )
			{

				//
				// make the final water height change, note we do damage on the final transition
				// in all situations by passing a valid damage amount
				//
				setWaterHeight( water, targetHeight, damageAmount, TRUE );

				//
				// remove our water entry from the per frame water list, we're processing this array
				// backwards which makes cleanup easy, we just move everything after our index
				// position up one
				//
				for( Int j = i; j < m_numWaterToUpdate; j++ )
					m_waterToUpdate[ i ] = m_waterToUpdate[ j ];
				m_numWaterToUpdate -= 1;

			}
			else
			{

				//
				// we're not doing damage every frame (0 damage) from the water
				// because it's an expensive process
				//
				if( doDamageThisFrame == FALSE )
					damageAmount = 0.0f;

				//
				// because some water implementation store the height as integers, some changes
				// are too small to keep track of in the actual water data structures so we have to
				// keep track of it ourselves
				//
				currentHeight += changePerFrame;
				m_waterToUpdate[ i ].currentHeight = currentHeight;

				// update actual water
				setWaterHeight( water, currentHeight, damageAmount, FALSE );

			}

		}

	}

}

//-------------------------------------------------------------------------------------------------
/** newMap */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::newMap( Bool saveGame )
{

	// Set waypoint's z value, now that the height map is loaded.
	for( Waypoint *way = m_waypointListHead; way; way = way->getNext() )
	{
		const Coord3D* loc = way->getLocation();
		way->setLocationZ(getGroundHeight(loc->x, loc->y));
	}
	//
	// until we have a real way to specify different water planes in the map, we will check
	// for a special waypoint name that we will put in maps that we want to have a
	// water grid
	/// @todo Mark W, remove this when you have water plane placements in the map done (Colin)
	//
	Waypoint *waypoint = getWaypointByName( "WaveGuide1" );
	Bool enable = FALSE;
	if( waypoint )
		enable = TRUE;
	enableWaterGrid( enable );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TerrainLogic::enableWaterGrid( Bool enable )
{

	// set our internal variable we can query
	m_waterGridEnabled = enable;

	//
	// set the vertex animated water properties, that is, the clamps, the water position,
	// the grid resolution etc ...
	//
	if( enable == TRUE )
	{

		/** @todo we should have this stuff stored with the map and have a real interface for
		design to edit such things so that people can put gridded water in any map without all
		this hard coded nasty stuff, but this is what "they" want for now */

		Int waterSettingIndex = -1;
		for( Int i = 0; i < GlobalData::MAX_WATER_GRID_SETTINGS; i++ )
		{

			if( TheGlobalData->m_mapName.compareNoCase( TheGlobalData->m_vertexWaterAvailableMaps[ i ].str() ) == 0 )
			{

				waterSettingIndex = i;
				break;  // exit for i

			}

			//
			// no exact map name (including path) was found, try to look for a match in just the
			// mapname.map without any path information.  This is necessary for save/load due to
			// the fact that the map Data\CHI01\CHI01.map will turn into Save\CHI01.map when
			// loading the map from a save game file
			//
			AsciiString strippedMapNameOnly;
			AsciiString strippedCompareMapNameOnly;
			const char *c;

			// create stripped map name
			c = strrchr( TheGlobalData->m_mapName.str(), '\\' );
			if( c )
				strippedMapNameOnly.set( c );
			else
				strippedMapNameOnly = TheGlobalData->m_mapName;

			// create stripped compare name
			c = strrchr( TheGlobalData->m_vertexWaterAvailableMaps[ i ].str(), '\\' );
			if( c )
				strippedCompareMapNameOnly.set( c );
			else
				strippedCompareMapNameOnly = TheGlobalData->m_vertexWaterAvailableMaps[ i ];

			// now try this compare
			if( strippedMapNameOnly.compareNoCase( strippedCompareMapNameOnly.str() ) == 0 )
			{

				waterSettingIndex = i;
				break;  // exit for i

			}

		}

		// check for no match found
		if( waterSettingIndex == -1 )
		{

			DEBUG_CRASH(( "!!!!!! Deformable water won't work because there was no group of vertex water data defined in GameData.INI for this map name '%s' !!!!!! (C. Day)",
										TheGlobalData->m_mapName.str() ));
			return;

		}

		TheTerrainVisual->setWaterGridHeightClamps( nullptr,
																								TheGlobalData->m_vertexWaterHeightClampLow[ waterSettingIndex ],
																								TheGlobalData->m_vertexWaterHeightClampHi[ waterSettingIndex ] );
		TheTerrainVisual->setWaterTransform( nullptr,
																				 TheGlobalData->m_vertexWaterAngle[ waterSettingIndex ],
																				 TheGlobalData->m_vertexWaterXPosition[ waterSettingIndex ],
																				 TheGlobalData->m_vertexWaterYPosition[ waterSettingIndex ],
																				 TheGlobalData->m_vertexWaterZPosition[ waterSettingIndex ] );
		TheTerrainVisual->setWaterGridResolution( nullptr,
																							TheGlobalData->m_vertexWaterXGridCells[ waterSettingIndex ],
																							TheGlobalData->m_vertexWaterYGridCells[ waterSettingIndex ],
																							TheGlobalData->m_vertexWaterGridSize[ waterSettingIndex ] );
		TheTerrainVisual->setWaterAttenuationFactors( nullptr,
																									TheGlobalData->m_vertexWaterAttenuationA[ waterSettingIndex ],
																									TheGlobalData->m_vertexWaterAttenuationB[ waterSettingIndex ],
																									TheGlobalData->m_vertexWaterAttenuationC[ waterSettingIndex ],
																									TheGlobalData->m_vertexWaterAttenuationRange[ waterSettingIndex ] );

	}

	// notify the terrain visual of the change
	TheTerrainVisual->enableWaterGrid( enable );

}

//-------------------------------------------------------------------------------------------------
/** device independent terrain logic load.  If query is true, we are just loading it to get
look at some data rather than running a game, so don't pass this load to the client. */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::loadMap( AsciiString filename, Bool query )
{

	// sanity
	if( filename.isEmpty() )
		return FALSE;

	// copy filename
	m_filenameString = filename;

	// Add waypoint objects.
	MapObject *pObj;
	for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		if (pObj->isWaypoint()) {
			addWaypoint(pObj);
		}
	}

	CachedFileInputStream theInputStream;
	if (theInputStream.open(AsciiString(m_filenameString.str())))
	try {
		ChunkInputStream *pStrm = &theInputStream;
		pStrm->absoluteSeek(0);
		DataChunkInput file( pStrm );
		if (file.isValidFileType()) {	// Backwards compatible files aren't valid data chunk files.
			// Read the waypoints.
			file.registerParser( "WaypointsList", AsciiString::TheEmptyString, parseWaypointDataChunk );
			if (!file.parse(this)) {
				DEBUG_CRASH(("Unable to read waypoint info."));
				return false;
			}
		}
		theInputStream.close();
	} catch (...) {
		// Eat the error - legacy files are not valid chunk format (and don't have waypoint info.)
		DEBUG_LOG(("Unable to read waypoint info."));
	}
#if 0 //def DEBUG_LOGGING
	// Dump out the waypoint links.
	Waypoint *pWay;
	// Traverse all waypoints.
	int count = 0;
	for (pWay = getFirstWaypoint(); pWay; pWay = pWay->getNext()) {
		count++;
		Coord3D loc;
		pWay->getLocation(&loc);
		DEBUG_LOG_RAW(("Waypoint %d - '%s' id=%d ", count, pWay->getName().str(), pWay->getID()));
		DEBUG_LOG_RAW(("{%.2f, %.2f, %.2f} ", loc.x, loc.y, loc.z));
		Int i;
		if (pWay->getNumLinks()) {
			DEBUG_LOG_RAW(("Links to: "));
			for (i=0; i<pWay->getNumLinks(); i++) {
				Waypoint *pLink = pWay->getLink(i);
				DEBUG_LOG_RAW(("'%s' id=%d ", pLink->getName().str(), pLink->getID()));
			}
		} else {
			DEBUG_LOG_RAW(("No links."));
		}
		DEBUG_LOG_RAW(("\n"));
	}
	DEBUG_LOG(("Total of %d waypoints.", count));
#endif

	if (!query) {
		// tell the game interface a new terrain file has been loaded up
		TheTerrainVisual->load( getSourceFilename() );
	}

	return TRUE;  // success

}

//-------------------------------------------------------------------------------------------------
/** Reads in the waypoint chunk */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::parseWaypointDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	TerrainLogic *pThis = (TerrainLogic *)userData;
	return pThis->parseWaypointData(file, info, userData);
}

//-------------------------------------------------------------------------------------------------
/** Reads in the waypoint chunk */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::parseWaypointData(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	Int numWaypointLinks = file.readInt();
	Int i;
	for (i=0; i<numWaypointLinks; i++) {
		Int waypoint1 = file.readInt();
		Int waypoint2 = file.readInt();
		addWaypointLink(waypoint1, waypoint2);
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

//-------------------------------------------------------------------------------------------------
/** Adds one waypoint. */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::addWaypoint(MapObject *pMapObj)
{
	Coord3D loc = *pMapObj->getLocation();
	// Snap the waypoint down to the terrain.
	loc.z = getGroundHeight(loc.x, loc.y);
	Bool exists;
	AsciiString label1, label2, label3;
	label1 = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel1, &exists);
	label2 = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel2, &exists);
	label3 = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel3, &exists);
	Bool biDirectional;
	biDirectional = pMapObj->getProperties()->getBool(TheKey_waypointPathBiDirectional, &exists);
	DEBUG_ASSERTCRASH(pMapObj->isWaypoint(), ("not a waypoint"));
	Waypoint *pWay = newInstance(Waypoint)(pMapObj->getWaypointID(), pMapObj->getWaypointName(),
																&loc, label1, label2, label3, biDirectional);
	pWay->setNext(m_waypointListHead);
	m_waypointListHead = pWay;
}

//-------------------------------------------------------------------------------------------------
/** Links 2 waypoints. */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::addWaypointLink(Int id1, Int id2)
{
	Waypoint *pWay1 = nullptr;
	Waypoint *pWay2 = nullptr;
	Waypoint *pWay;
	// Traverse all waypoints.
	/// @todo ID's should be UnsignedInts (MSB)
	for (pWay = getFirstWaypoint(); pWay; pWay = pWay->getNext()) {
		if (pWay->getID() == (UnsignedInt)id1) {
			pWay1 = pWay;
		}
		if (pWay->getID() == (UnsignedInt)id2) {
			pWay2 = pWay;
		}
	}
	if (pWay1 && pWay2 && (pWay1 != pWay2)) {
		Int i;
		for (i=0; i<pWay1->getNumLinks(); i++) {
			if (pWay1->getLink(i) == pWay2) {
				return; // already linked;
			}
		}
		pWay1->addLink(pWay2);

		if (pWay1->getBiDirectional()) {
			// Link the other way.
			for (i=0; i<pWay2->getNumLinks(); i++) {
				if (pWay2->getLink(i) == pWay1) {
					return; // already linked;
				}
			}
			pWay2->addLink(pWay1);
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Deletes the waypoints list. */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::deleteWaypoints()
{
	Waypoint *pNext = nullptr;
	Waypoint *pWay;
	// Traverse all waypoints.
	for (pWay = getFirstWaypoint(); pWay; pWay = pNext) {
		pNext = pWay->getNext();
		pWay->setNext(nullptr);
		deleteInstance(pWay);
	}
	m_waypointListHead = nullptr;
}

//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::isClearLineOfSight(const Coord3D& pos, const Coord3D& posOther) const
{
	DEBUG_CRASH(("implement ME"));
	return false;
}

//-------------------------------------------------------------------------------------------------
/** default get height for terrain logic */
//-------------------------------------------------------------------------------------------------
Real TerrainLogic::getGroundHeight( Real x, Real y, Coord3D* normal ) const
{
	if( normal )
		normal->zero();

	return 0;

}

//-------------------------------------------------------------------------------------------------
/** default get height for terrain logic */
//-------------------------------------------------------------------------------------------------
Real TerrainLogic::getLayerHeight( Real x, Real y, PathfindLayerEnum layer, Coord3D* normal, Bool clip ) const
{
	if( normal )
		normal->zero();

	return 0;

}

//-------------------------------------------------------------------------------------------------
/** default isCliffCell for terrain logic */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::isCliffCell( Real x, Real y) const
{

	return false;

}

//-------------------------------------------------------------------------------------------------
void makeAlignToNormalMatrix( Real angle, const Coord3D& pos, const Coord3D& normal, Matrix3D& mtx)
{
	Coord3D x, y, z;

	z = normal;

	/*
		It is extremely important that the resulting matrix is such that
		the xvector points in the angle we specified; specifically,
		that WWMath::Atan2Origin(xvec.y, xvec.x) == angle. So we must construct
		the matrix carefully to ensure this!
	*/
	x.x = Cos( angle );
	x.y = Sin( angle );
	x.z = 0.0f;
//x.normalize();	-- redundant; is normalized by definition

	// dot of two unit vectors is cos of angle between them;
	// we want there to be a 90-deg angle between the x and z
	// vectors, so calc x.z to satisfy this (ie, cos==0)
	/*
		xx*zx + xy*zy + xz*zz = 0
		xz = (-xx*zz - xy*zy)/zz
	*/
	if (z.z != 0.0f)
	{
		x.z = -(x.x*z.x + x.y*z.y) / z.z;
		x.normalize();
	}

	DEBUG_ASSERTCRASH(fabs(x.x*z.x + x.y*z.y + x.z*z.z)<0.0001,("dot is not zero (%f)",fabs(x.x*z.x + x.y*z.y + x.z*z.z)));

	// now computing the y vector is trivial.
	y.crossProduct( z, x, y );
	y.normalize();

	mtx.Set(  x.x, y.x, z.x, pos.x,
							x.y, y.y, z.y, pos.y,
							x.z, y.z, z.z, pos.z );
}

//-------------------------------------------------------------------------------------------------
/** given angle and position, return the matrix aligning this
	* position with the ground */
//-------------------------------------------------------------------------------------------------
PathfindLayerEnum TerrainLogic::alignOnTerrain( Real angle, const Coord3D& pos, Bool stickToGround, Matrix3D& mtx)
{
	Coord3D terrainNormal;
	PathfindLayerEnum layer;

	layer = getLayerForDestination(&pos);

	// get the normal of the terrain at our position
	Real terrainAtPos = getLayerHeight(pos.x, pos.y, layer, &terrainNormal );
	if (layer != LAYER_GROUND) {
		/// @todo - fix brutal hack for bridges that are too high. jba
		terrainAtPos += 2.5f;
	}
	makeAlignToNormalMatrix(angle, pos, terrainNormal, mtx);
	if (stickToGround)
		mtx.Set_Z_Translation(terrainAtPos);

	return layer;
}

//-------------------------------------------------------------------------------------------------
/** Adds a bridge's info get height function for logical terrain */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::addBridgeToLogic(BridgeInfo *pInfo, Dict *props, AsciiString bridgeTemplateName)
{
	Bridge *pBridge = newInstance(Bridge)(*pInfo, props, bridgeTemplateName);
	pBridge->setNext(m_bridgeListHead);
	m_bridgeListHead = pBridge;
	PathfindLayerEnum layer = TheAI->pathfinder()->addBridge(pBridge);
	pBridge->setLayer(layer);

}

//-------------------------------------------------------------------------------------------------
/** Adds a bridge's info get height function for logical terrain */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::addLandmarkBridgeToLogic(Object *bridgeObj)
{

	Bridge *pBridge = newInstance(Bridge)(bridgeObj);
	pBridge->setNext(m_bridgeListHead);
	m_bridgeListHead = pBridge;
	PathfindLayerEnum layer = TheAI->pathfinder()->addBridge(pBridge);
	pBridge->setLayer(layer);

}

//-------------------------------------------------------------------------------------------------
/** Given a name, return the associated waypoint. */
//-------------------------------------------------------------------------------------------------
Waypoint *TerrainLogic::getWaypointByName( AsciiString name )
{
	for( Waypoint *way = m_waypointListHead; way; way = way->getNext() )
		if (way->getName() == name)
			return way;

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Given a unique integer ID, return the associated waypoint. */
//-------------------------------------------------------------------------------------------------
Waypoint *TerrainLogic::getWaypointByID( UnsignedInt id )
{
	for( Waypoint *way = m_waypointListHead; way; way = way->getNext() )
		if (way->getID() == id)
			return way;

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Return the closest waypoint on the labeled path. */
//-------------------------------------------------------------------------------------------------
Waypoint *TerrainLogic::getClosestWaypointOnPath( const Coord3D *pos, AsciiString label )
{
	Real distSqr = 0;
	Waypoint *pClosestWay = nullptr;
	if (label.isEmpty()) {
		DEBUG_LOG(("***Warning - asking for empty path label."));
		return nullptr;
	}

	for( Waypoint *way = m_waypointListHead; way; way = way->getNext() ) {
		Bool match = false;
		if (label.compareNoCase(way->getPathLabel1())==0) match = true;
		if (label.compareNoCase(way->getPathLabel2())==0) match = true;
		if (label.compareNoCase(way->getPathLabel3())==0) match = true;
		if (match) {
			Coord3D curPos = *way->getLocation();
			Real newDistSqr = (curPos.x-pos->x)*(curPos.x-pos->x) + (curPos.y-pos->y)*(curPos.y-pos->y);
			if (pClosestWay==nullptr) {
				pClosestWay = way;
				distSqr = newDistSqr;
			} else if (newDistSqr < distSqr) {
				pClosestWay = way;
				distSqr = newDistSqr;
			}
		}
	}

	return pClosestWay;
}

//-------------------------------------------------------------------------------------------------
/** Return true if the waypoint path containing pWay is labeled with the label. */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::isPurposeOfPath( Waypoint *pWay, AsciiString label )
{
	if (label.isEmpty() || pWay==nullptr) {
		DEBUG_LOG(("***Warning - asking for empth path label."));
		return false;
	}

	Bool match = false;
	if (label == pWay->getPathLabel1()) match = true;
	if (label == pWay->getPathLabel2()) match = true;
	if (label == pWay->getPathLabel3()) match = true;

	return match;
}


//-------------------------------------------------------------------------------------------------
/** Given a name, return the associated trigger area, or null if one doesn't exist. */
//-------------------------------------------------------------------------------------------------
PolygonTrigger *TerrainLogic::getTriggerAreaByName( AsciiString name )
{
	for (PolygonTrigger* pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
		const AsciiString& trigName = pTrig->getTriggerName();
		if (name == trigName)
			return pTrig;
	}
	return nullptr;
}


//-------------------------------------------------------------------------------------------------
/** Finds the bridge at a given x/y coordinate.  */
//-------------------------------------------------------------------------------------------------
Bridge * TerrainLogic::findBridgeAt( const Coord3D *pLoc) const
{

	Bridge *pBridge = getFirstBridge();
	while (pBridge) {
		if (pBridge->isPointOnBridge(pLoc)) {
			return(pBridge);
		}
		pBridge = pBridge->getNext();
	}
	return(nullptr);
}

//-------------------------------------------------------------------------------------------------
/** Finds the bridge at a given x/y coordinate.  On a layer. */
//-------------------------------------------------------------------------------------------------
Bridge * TerrainLogic::findBridgeLayerAt( const Coord3D *pLoc, PathfindLayerEnum layer, Bool clip) const
{
	if (layer == LAYER_GROUND)
		return nullptr;

	Bridge *pBridge = getFirstBridge();
	while (pBridge)
	{
		if (pBridge->getLayer() == layer && (!clip || pBridge->isPointOnBridge(pLoc)))
		{
			return(pBridge);
		}
		pBridge = pBridge->getNext();
	}
	return(nullptr);
}

//-------------------------------------------------------------------------------------------------
/** Returns the layer id for the bridge, if any, at this destination.  Otherwisee
return LAYER_GROUND. */
//-------------------------------------------------------------------------------------------------
PathfindLayerEnum TerrainLogic::getLayerForDestination(const Coord3D *pos)
{
	Bridge *pBridge = getFirstBridge();
	PathfindLayerEnum bestLayer = LAYER_GROUND;
	Real bestDistance = fabs(pos->z - getGroundHeight(pos->x, pos->y));

	if (bestDistance > TheAI->pathfinder()->getWallHeight()/2) {
		// check wall.
		if (TheAI->pathfinder()->isPointOnWall(pos)) {
			Real delta = fabs(pos->z-TheAI->pathfinder()->getWallHeight());
			if (delta<bestDistance) {
				bestLayer = (PathfindLayerEnum)LAYER_WALL;
				bestDistance = delta;
			}
		}
	}

	while (pBridge ) {
		if (pBridge->isPointOnBridge(pos) ) {
			Real delta = fabs(pos->z-pBridge->getBridgeHeight(pos, nullptr));
			if (delta<bestDistance) {
				bestLayer = pBridge->getLayer();
				bestDistance = delta;
			}
		}
		pBridge = pBridge->getNext();
	}
	return(bestLayer);
}

//-------------------------------------------------------------------------------------------------
// this is just like getLayerForDestination, but always return the highest layer that will be <= z at that point
// (unlike getLayerForDestination, which will return the closest layer)
PathfindLayerEnum TerrainLogic::getHighestLayerForDestination(const Coord3D *pos, Bool onlyHealthyBridges)
{
	PathfindLayerEnum bestLayer = LAYER_GROUND;
	Real bestDistance = pos->z - getGroundHeight(pos->x, pos->y);	// NOT fabs in this case.

	if (bestDistance > TheAI->pathfinder()->getWallHeight()/2) {
		// check wall.
		if (TheAI->pathfinder()->isPointOnWall(pos)) {
			Real delta = pos->z - TheAI->pathfinder()->getWallHeight();
			// must be ABOVE (or on) the wall for this call. (srj)
			if (delta >= 0 && fabs(delta) < fabs(bestDistance)) {
				bestLayer = (PathfindLayerEnum)LAYER_WALL;
				bestDistance = delta;
			}
		}
	}

	for (Bridge *pBridge = getFirstBridge(); pBridge != nullptr; pBridge = pBridge->getNext()) {

		if (onlyHealthyBridges && pBridge->peekBridgeInfo()->curDamageState == BODY_RUBBLE)
			continue;

		if (pBridge->isPointOnBridge(pos) ) {
			Real delta = pos->z - pBridge->getBridgeHeight(pos, nullptr);
			// must be ABOVE (or on) the bridge for this call. (srj)
			if (delta >= 0 && fabs(delta) < fabs(bestDistance)) {
				bestLayer = pBridge->getLayer();
				bestDistance = delta;
			}
		}
	}
	return(bestLayer);
}

//-------------------------------------------------------------------------------------------------
/** Determines whether the object interacts with the bridge on specified layer. */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::objectInteractsWithBridgeLayer(Object *obj, Int layer, Bool considerBridgeHealth) const
{
	if (layer == LAYER_GROUND) return false;
	if (layer == LAYER_WALL) {
		if (obj->getLayer() == LAYER_WALL) {
			return true; // objects on the wall can't fall off :)
		}
		if (TheAI->pathfinder()->isPointOnWall(obj->getPosition())) {
			return true;
		}
		return false;
	}
	Bridge *pBridge = getFirstBridge();

	while (pBridge ) {
		if (pBridge->getLayer() == layer) {
			Bool match = false;
			if (pBridge->isPointOnBridge(obj->getPosition()) ) {
				match = true;
			}

			Real radius = obj->getGeometryInfo().getMinorRadius();
			radius += PATHFIND_CELL_SIZE_F/2.0f;
			Region2D bounds;
			bounds.lo.x = obj->getPosition()->x;
			bounds.lo.y = obj->getPosition()->y;
			bounds.hi = bounds.lo;
			bounds.lo.x -= radius;
			bounds.lo.y -= radius;
			bounds.hi.x += radius;
			bounds.hi.y += radius;
			if (pBridge->isCellOnEnd(&bounds)) {
				match = true;
			}

			if (match) {
				Real bridgeHeight = pBridge->getBridgeHeight(obj->getPosition(), nullptr);
				Real delta = fabs(obj->getPosition()->z-bridgeHeight);
				if (delta>LAYER_Z_CLOSE_ENOUGH_F) {
					return false;
				}

				// make sure it's not destroyed. can't interact with dead bridges.
				if (considerBridgeHealth && pBridge->peekBridgeInfo()->curDamageState == BODY_RUBBLE)
				{
					return false;
				}

				return true;
			}
			return false;
		}

		pBridge = pBridge->getNext();
	}
	return(false);
}

//-------------------------------------------------------------------------------------------------
/** Determines whether the object interacts with the bridge on specified layer. */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::objectInteractsWithBridgeEnd(Object *obj, Int layer) const
{
	if (layer == LAYER_GROUND) return false;
	Bridge *pBridge = getFirstBridge();

	while (pBridge ) {
		if (pBridge->getLayer() == layer) {
			Bool match = false;

			Real radius = obj->getGeometryInfo().getMinorRadius();
			radius += PATHFIND_CELL_SIZE_F/2.0f;
			Region2D bounds;
			bounds.lo.x = obj->getPosition()->x;
			bounds.lo.y = obj->getPosition()->y;
			bounds.hi = bounds.lo;
			bounds.lo.x -= radius;
			bounds.lo.y -= radius;
			bounds.hi.x += radius;
			bounds.hi.y += radius;
			if (pBridge->isCellOnEnd(&bounds)) {
				match = true;
			}

			if (match) {
				Real bridgeHeight = pBridge->getBridgeHeight(obj->getPosition(), nullptr);
				Real delta = fabs(obj->getPosition()->z-bridgeHeight);
				if (delta>LAYER_Z_CLOSE_ENOUGH_F)
				{
					return false;
				}
				return true;
			}
			return false;
		}

		pBridge = pBridge->getNext();
	}
	return(false);
}

//-------------------------------------------------------------------------------------------------
/** Updates the damage state of the bridge from the logic. */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::updateBridgeDamageStates()
{
	Bridge *pBridge = getFirstBridge();
	while (pBridge) {
		pBridge->updateDamageState();
		pBridge = pBridge->getNext();
	}
	m_bridgeDamageStatesChanged = true;
}

//-------------------------------------------------------------------------------------------------
/** Checks if a bridge is repaired. */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::isBridgeRepaired(const Object *bridge)
{
	if (!bridge) return false;
	ObjectID id = bridge->getID();
	Bridge *pBridge = getFirstBridge();
	while (pBridge) {
		const BridgeInfo *info = pBridge->peekBridgeInfo();
		if (info->bridgeObjectID == id) {
			// found the right bridge.
			if (info->damageStateChanged) {
				// Damage state just changed.
				if (info->curDamageState != BODY_RUBBLE) {
					return true;
				}
			}
			return false;
		}
		pBridge = pBridge->getNext();
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** Checks if a bridge is broken. */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::isBridgeBroken( const Object *bridge )
{
	if (!bridge) return false;
	ObjectID id = bridge->getID();
	Bridge *pBridge = getFirstBridge();
	while (pBridge) {
		const BridgeInfo *info = pBridge->peekBridgeInfo();
		if (info->bridgeObjectID == id) {
			// found the right bridge.
			if (info->damageStateChanged) {
				// Damage state just changed.
				if (info->curDamageState == BODY_RUBBLE) {
					return true;
				}
			}
			return false;
		}
		pBridge = pBridge->getNext();
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** Gets the attack points for a bridge. */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::getBridgeAttackPoints(const Object *bridge, TBridgeAttackInfo *attackInfo)
{
	ObjectID id = bridge->getID();
	Bridge *pBridge = getFirstBridge();
	while (pBridge) {
		const BridgeInfo *info = pBridge->peekBridgeInfo();
		if (info->bridgeObjectID == id) {
			// found the right bridge.
			Coord3D delta;
			delta.x = info->to.x - info->from.x;
			delta.y = info->to.y - info->from.y;
			delta.z = info->to.z - info->from.z;
			delta.normalize();
			Coord3D width;
			width.x = info->fromRight.x - info->fromLeft.x;
			width.y = info->fromRight.y - info->fromLeft.y;
			width.z = info->fromRight.z - info->fromLeft.z;
			Real len = width.length();
			len /= 2.0f;
			attackInfo->attackPoint1.x = info->from.x + delta.x*len;
			attackInfo->attackPoint1.y = info->from.y + delta.y*len;
			attackInfo->attackPoint1.z = info->from.z + delta.z*len;

			attackInfo->attackPoint2.x = info->to.x - delta.x*len;
			attackInfo->attackPoint2.y = info->to.y - delta.y*len;
			attackInfo->attackPoint2.z = info->to.z - delta.z*len;

			return;
		}
		pBridge = pBridge->getNext();
	}
	attackInfo->attackPoint1 = *bridge->getPosition();
	attackInfo->attackPoint2 = *bridge->getPosition();
}

//-------------------------------------------------------------------------------------------------
/** Picks a bridge, and returns it's drawable. */
//-------------------------------------------------------------------------------------------------
Drawable *TerrainLogic::pickBridge(const Vector3 &from, const Vector3 &to, Vector3 *pos)
{
	Drawable *curDraw = nullptr;
	Vector3 curPos(0,0,0);

	Bridge *pBridge = getFirstBridge();
	while (pBridge) {
		Vector3 thisPos;
		Drawable *thisDraw = pBridge->pickBridge(from, to , &thisPos);
		if (!curDraw) {
			curDraw = thisDraw;
			curPos = thisPos;
		}
		pBridge = pBridge->getNext();
	}
	*pos = curPos;
	return(curDraw);
}

//-------------------------------------------------------------------------------------------------
/** Deletes the bridges list. */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::deleteBridges()
{
	Bridge *pNext = nullptr;
	Bridge *pBridge;
	// Traverse all waypoints.
	for (pBridge = getFirstBridge(); pBridge; pBridge = pNext) {
		pNext = pBridge->getNext();
		pBridge->setNext(nullptr);
		deleteInstance(pBridge);
	}
	m_bridgeListHead = nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Delete the bridge specified */
//-------------------------------------------------------------------------------------------------
void TerrainLogic::deleteBridge( Bridge *bridge )
{

	// sanity
	if( bridge == nullptr )
		return;

	// check for removing the head
	if( m_bridgeListHead == bridge )
	{

		m_bridgeListHead = bridge->getNext();

	}
	else
	{

		for( Bridge *otherBridge = getFirstBridge();
				 otherBridge;
				 otherBridge = otherBridge->getNext() )
		{

			//
			// if the next bridge is the one in question to delete, set this bridge to point
			// to the next pointer of the bridge we are deleting
			//
			if( otherBridge->getNext() == bridge )
			{

				otherBridge->setNext( bridge->getNext() );
				break;  // exit for

			}

		}

	}

	// delete object associated with bridge if present
	BridgeInfo bridgeInfo;
	bridge->getBridgeInfo( &bridgeInfo );
	TheAI->pathfinder()->changeBridgeState(bridge->getLayer(), false);

	Object *bridgeObj = TheGameLogic->findObjectByID( bridgeInfo.bridgeObjectID );
	if( bridgeObj )
		TheGameLogic->destroyObject( bridgeObj );

	// delete the bridge in question
	deleteInstance(bridge);

}

//-------------------------------------------------------------------------------------------------
/** Returns the ground aligned point on the bounding box closest to the given point*/
//-------------------------------------------------------------------------------------------------
Coord3D TerrainLogic::findClosestEdgePoint ( const Coord3D *closestTo ) const
{
	Region3D mapExtent;
	getExtent( &mapExtent );

	Real distances[4];
	distances[0] = fabs( closestTo->y - mapExtent.lo.y );//top
	distances[1] = fabs( closestTo->x - mapExtent.hi.x );//right
	distances[2] = fabs( closestTo->y - mapExtent.hi.y );//bottom
	distances[3] = fabs( closestTo->x - mapExtent.lo.x );//left
	Real bestDistance = distances[0];
	Int bestDistanceIndex = 0;
	for( Int lameIndex = 1; lameIndex < 4; lameIndex++ )
	{
		if( distances[lameIndex] < bestDistance )
		{
			bestDistance = distances[lameIndex];
			bestDistanceIndex = lameIndex;
		}
	}

	Coord3D retVal = *closestTo;
	if( bestDistanceIndex == 0 )
	{
		retVal.y = mapExtent.lo.y;
	}
	else if( bestDistanceIndex == 1 )
	{
		retVal.x = mapExtent.hi.x;
	}
	else if( bestDistanceIndex == 2 )
	{
		retVal.y = mapExtent.hi.y;
	}
	else
	{
		retVal.x = mapExtent.lo.x;
	}

	retVal.z = getGroundHeight( retVal.x, retVal.y );

	return retVal;

}




//-------------------------------------------------------------------------------------------------
/** Returns the ground aligned point on the bounding box farthest from the given point*/
//-------------------------------------------------------------------------------------------------
// Lorenzen was here
Coord3D TerrainLogic::findFarthestEdgePoint( const Coord3D *farthestFrom ) const
{
	Region3D mapExtent;
	getExtent( &mapExtent );

	Coord3D retVal = *farthestFrom;

	if (farthestFrom->x < (mapExtent.width()/2) )
		retVal.x = mapExtent.hi.x;
	else
		retVal.x = mapExtent.lo.x;

	if (farthestFrom->y < (mapExtent.height()/2) )
		retVal.y = mapExtent.hi.y;
	else
		retVal.y = mapExtent.lo.y;


	retVal.z = getGroundHeight( retVal.x, retVal.y );

	return retVal;

}





//-------------------------------------------------------------------------------------------------
/** See if a location is underwater, and what the water height is. */
//-------------------------------------------------------------------------------------------------
Bool TerrainLogic::isUnderwater( Real x, Real y, Real *waterZ, Real *terrainZ )
{

	// get the water handle at this location
	const WaterHandle *waterHandle = getWaterHandle( x, y );

	// if no water here, no height, no nuttin
	if( waterHandle == nullptr )
  {
    // but we have to return the terrain Z if requested!
    if (terrainZ)
      *terrainZ=getGroundHeight(x,y);
		return FALSE;
  }

	//
	// if this water handle is a grid water use the grid height function, otherwise look into
	// the polygon trigger
	//
	Real wZ = 0.0f;
	if( waterHandle == &m_gridWaterHandle )
		TheTerrainVisual->getWaterGridHeight( x, y, &wZ );
	else
		wZ = getWaterHeight( waterHandle );

	// fill out the waterZ parameter with the water height
	if( waterZ )
		*waterZ = wZ;

	// see if the terrain height here is below the water
	Real terrainHeight = getGroundHeight( x, y );
	if (terrainZ)
		*terrainZ = terrainHeight;

	return terrainHeight < wZ;

}

// ------------------------------------------------------------------------------------------------
/** Get the water table with the highest water Z value at the location */
// ------------------------------------------------------------------------------------------------
const WaterHandle* TerrainLogic::getWaterHandle( Real x, Real y )
{
	const WaterHandle *waterHandle = nullptr;
	Real waterZ = 0.0f;
	ICoord3D iLoc;

	iLoc.x = REAL_TO_INT_FLOOR( x + 0.5f );
	iLoc.y = REAL_TO_INT_FLOOR( y + 0.5f );
	iLoc.z = 0;

	// Look for water areas in the polygon triggers
	for( PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger();
			 pTrig;
			 pTrig = pTrig->getNext() )
	{

		if( !pTrig->isWaterArea() )
			continue;

		// See if point is in a water area
		if( pTrig->pointInTrigger( iLoc ) )
		{

			if( pTrig->getPoint( 0 )->z >= waterZ )
			{

				waterZ = pTrig->getPoint( 0 )->z;
				waterHandle = pTrig->getWaterHandle();

			}

		}

	}

	/**@todo: Remove this after we have all water types included
		in water triggers.  For now do special check for water grid mesh. */
	// TheSuperHackers @logic-client-separation helmutbuhler 11/04/2025
	// We shouldn't depend on TerrainVisual here.
	Real meshZ;
	if( TheTerrainVisual->getWaterGridHeight( x, y, &meshZ ) )
	{

		//
		// point falls on water grid, return the special handle for the grid water, since we
		// only have one of them and don't yet support multiple gridded water sections
		//
		if( meshZ >= waterZ )
		{

			waterZ = meshZ;
			waterHandle = &m_gridWaterHandle;

		}

	}

	return waterHandle;

}

// ------------------------------------------------------------------------------------------------
/** Get water handle by name assigned from the editor */
// ------------------------------------------------------------------------------------------------
const WaterHandle* TerrainLogic::getWaterHandleByName( AsciiString name )
{
	if (name.compare(WATER_GRID) == 0)
		return &TerrainLogic::m_gridWaterHandle;

	PolygonTrigger *trig = PolygonTrigger::getFirstPolygonTrigger();
	while (trig)
	{
		if (trig->getTriggerName().compare(name) == 0 && trig->isWaterArea())
			return trig->getWaterHandle();
		trig = trig->getNext();
	}

	return nullptr;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Real TerrainLogic::getWaterHeight( const WaterHandle *water )
{

	// sanity
	if( water == nullptr )
		return 0.0f;

	//
	// when querying the water height given a handle, we cannot query gridded water in this
	// way because it's variable across the whole surface of the water
	//
	if( water == &m_gridWaterHandle )
	{

		DEBUG_CRASH(( "TerrainLogic::getWaterHeight( WaterHandle *water ) - water is a grid handle, cannot make this query" ));
		return 0.0f;

	}

	// sanity
	DEBUG_ASSERTCRASH( water->m_polygon != nullptr, ("getWaterHeight: polygon trigger in water handle is null") );

	// return the height of the water using the polygon trigger
	return water->m_polygon->getPoint( 0 )->z;

}

// ------------------------------------------------------------------------------------------------
/** Set the water height.  If the water rises, then any objects that now find themselves
	* underwater will be damaged by the amount provided in the parameter 'damageAmount' */
// ------------------------------------------------------------------------------------------------
void TerrainLogic::setWaterHeight( const WaterHandle *water, Real height, Real damageAmount,
																	 Bool forcePathfindUpdate )
{

	// sanity
	if( water == nullptr )
		return;

	//
	// if this is a handle to gridded water simple change the transform to raise/lower the whole
	// water table.  Note: The other meaning this *could* have if we want it to is to leave
	// the water table transform where it is and actually change the height of the water at
	// every grid point
	//
	Real previousHeight = 0.0f;
	if( water == &m_gridWaterHandle )
	{

		// get transform information
		Matrix3D transform;
		TheTerrainVisual->getWaterTransform( water, &transform );

		// save the old height
		previousHeight = transform.Get_Z_Translation();

		// set the new height
		transform.Set_Z_Translation( height );
		TheTerrainVisual->setWaterTransform( &transform );

	}
	else
	{

		// save the previous height
		previousHeight = getWaterHeight( water );

		// set the new height at all the points in the polygon trigger
		const ICoord3D *p;
		ICoord3D newPoint;
		Int numPoints = water->m_polygon->getNumPoints();
		for( Int i = 0; i < numPoints; ++i )
		{

			p = water->m_polygon->getPoint( i );
			newPoint.x = p->x;
			newPoint.y = p->y;
			newPoint.z = height;
			water->m_polygon->setPoint( newPoint, i );

		}
		height = getWaterHeight(water);

	}

	// find the bounding rectangle of this water area
	Region3D affectedRegion;
	affectedRegion.zero();
	findAxisAlignedBoundingRect( water, &affectedRegion );

	// changes in the water level force us to recalculate the pathfinding map
	if( forcePathfindUpdate || previousHeight != height )
	{

		// do the pathfind remapping
		TheAI->pathfinder()->forceMapRecalculation();

	}

	//
	// if the water height has risen, we need apply water damage to things that are now
	// under the water
	//
	if( damageAmount > 0.0f && height > previousHeight )
	{

		// find the center of the water "area" given the bounding region
		Coord3D center;
		center.x = affectedRegion.lo.x + affectedRegion.width() / 2.0f;
		center.y = affectedRegion.lo.y + affectedRegion.height() / 2.0f;
		center.z = 0.0f;  // irrelevant

		// the max radius to scan around us is the diagonal of the bounding region
		Real maxDist = WWMath::SqrtOrigin( affectedRegion.width() * affectedRegion.width() +
												 affectedRegion.height() * affectedRegion.height() );

		// scan the objects in the area of the water affected
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( &center,
																																			 maxDist,
																																			 FROM_CENTER_2D,
																																			 nullptr );
		MemoryPoolObjectHolder hold( iter );
		Object *obj;
		const Coord3D *objPos;
		for( obj = iter->first(); obj; obj = iter->next() )
		{

			// get other object position
			objPos = obj->getPosition();

			// if this object is underwater, do some damage
			if( isUnderwater( objPos->x, objPos->y ) )
			{

				// do a lot of water damage
				DamageInfo damageInfo;
				damageInfo.in.m_damageType = DAMAGE_WATER;
				damageInfo.in.m_deathType = DEATH_NORMAL;
				damageInfo.in.m_sourceID = INVALID_ID;
				damageInfo.in.m_amount = damageAmount;
				obj->attemptDamage( &damageInfo );

			}

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Change the height of a water table over time */
// ------------------------------------------------------------------------------------------------
void TerrainLogic::changeWaterHeightOverTime( const WaterHandle *water,
																							Real finalHeight,
																							Real transitionTimeInSeconds,
																							Real damageAmount )
{

	// if we don't have room, oops!
	if( m_numWaterToUpdate >= MAX_DYNAMIC_WATER )
	{

		DEBUG_CRASH(( "Only '%d' simultaneous water table changes are supported", MAX_DYNAMIC_WATER ));
		return;

	}

	// sanity
	if( water == nullptr )
		return;

	// if this water table already has an entry in the array to update, remove it
	for( Int i = 0; i < m_numWaterToUpdate; i++ )
	{

		if( m_waterToUpdate[ i ].waterTable == water )
		{

			// put the entry at the end of the list here
			m_waterToUpdate[ i ] = m_waterToUpdate[ m_numWaterToUpdate - 1 ];

			// we now have one less entry
			--m_numWaterToUpdate;

			//
			// process this index over again just to be complete, but we should never find "another"
			// duplicate water entry
			//
			--i;

		}

	}

	// get the current height of the water
	Real currentHeight = getWaterHeight( water );

	// add the entry into the array of water to update
	m_waterToUpdate[ m_numWaterToUpdate ].waterTable = water;
	m_waterToUpdate[ m_numWaterToUpdate ].changePerFrame = (finalHeight - currentHeight) /
																												 (static_cast<float>(LOGICFRAMES_PER_SECOND) * transitionTimeInSeconds);
	m_waterToUpdate[ m_numWaterToUpdate ].targetHeight = finalHeight;
	m_waterToUpdate[ m_numWaterToUpdate ].damageAmount = damageAmount;
	m_waterToUpdate[ m_numWaterToUpdate ].currentHeight = currentHeight;

	// we now have one more entry to update
	++m_numWaterToUpdate;

}

// ------------------------------------------------------------------------------------------------
/** Find the axis aligned bounding region around a water table */
// ------------------------------------------------------------------------------------------------
void TerrainLogic::findAxisAlignedBoundingRect( const WaterHandle *water, Region3D *region )
{

	// sanity
	if( water == nullptr || region == nullptr )
		return;

	// setup the lo and high of the region to the *opposite* side of the map plus some big number
	#define BUFFER 99999.9f  /// just to have extreme regions outside of the map
	Region3D mapExtent;
	getExtent( &mapExtent );
	region->lo.x = mapExtent.hi.x + BUFFER;
	region->lo.y = mapExtent.hi.y + BUFFER;
	region->hi.x = mapExtent.lo.x - BUFFER;
	region->hi.y = mapExtent.lo.y - BUFFER;
	// for water grid we must access the transform
	if( water == &m_gridWaterHandle )
	{
		Int i;
		ICoord3D p[ 4 ];

		// compute the 4 corners of the table according to the grids and grid spacing
		Real gridX, gridY, cellSize;
		TheTerrainVisual->getWaterGridResolution( water, &gridX, &gridY, &cellSize );
		p[ 0 ].x = 0;
		p[ 0 ].y = 0;
		p[ 1 ].x = gridX * cellSize;
		p[ 1 ].y = 0;
		p[ 2 ].x = gridX * cellSize;
		p[ 2 ].y = gridY * cellSize;
		p[ 3 ].x = 0;
		p[ 3 ].y = gridY * cellSize;

		// transform the 4 points using the transform matrix of the water
		Vector3 v;
		Matrix3D transform;
		TheTerrainVisual->getWaterTransform( water, &transform );
		for( i = 0; i < 4; i++ )
		{

			v.Set( p[ i ].x, p[ i ].y, p[ i ].z );
			transform.Transform_Vector( transform, v, &v );

			// do the region compares
			if( v.X < region->lo.x )
				region->lo.x = v.X;
			if( v.X > region->hi.x )
				region->hi.x = v.X;
			if( v.Y < region->lo.y )
				region->lo.y = v.Y;
			if( v.Y > region->hi.y )
				region->hi.y = v.Y;

		}

	}
	else
	{

		// go through each polygon point and find the extents
		const ICoord3D *p;
		Int numPoints = water->m_polygon->getNumPoints();
		for( Int i = 0; i < numPoints; i++ )
		{

			// get this point
			p = water->m_polygon->getPoint( i );

			// compare to our region
			if( p->x < region->lo.x )
				region->lo.x = p->x;
			if( p->x > region->hi.x )
				region->hi.x = p->x;

			if( p->y < region->lo.y )
				region->lo.y = p->y;
			if( p->y > region->hi.y )
				region->hi.y = p->y;

			if( p->z < region->lo.z )
				region->lo.z = p->z;
			if( p->z > region->hi.z )
				region->hi.z = p->z;

		}

	}

}

void TerrainLogic::setActiveBoundary(Int newActiveBoundary)
{
	if (newActiveBoundary < 0 || newActiveBoundary >= m_boundaries.size()) {
		// probably should DEBUG_ASSERT here
		return;
	}

	if (newActiveBoundary == m_activeBoundary) {
		// since this is fairly expensive (causes reset of PartitionManager as well as pathfinding),
		// we should probably return
		return;
	}

	if (m_boundaries[newActiveBoundary].x == 0 || m_boundaries[newActiveBoundary].y == 0) {
		return;
	}

	ShroudStatusStoreRestore partitionStore;

	// Can't have any lingering looks persist over the resize, so flush the queue now
	ThePartitionManager->processEntirePendingUndoShroudRevealQueue();

	//Store fogged cells
	ThePartitionManager->storeFoggedCells(partitionStore, TRUE);

	m_activeBoundary = newActiveBoundary;

	//Remove ghost objects from partition manager so that their partition data
	//can be released when parent object detaches.
	TheGhostObjectManager->releasePartitionData();

	//Remove objects from partition manager so that any remaining cleared
	//cells must be permanently revealed.
	Object *obj = TheGameLogic->getFirstObject();
	while (obj) {
		obj->friend_prepareForMapBoundaryAdjust();
		obj = obj->getNextObject();
	}

	//Store permanently revealed cells.
	ThePartitionManager->storeFoggedCells(partitionStore, FALSE);

	ThePartitionManager->reset();
	ThePartitionManager->init();
	TheRadar->newMap(TheTerrainLogic);

	ThePartitionManager->restoreFoggedCells(partitionStore, FALSE);

	//Tell the ghost object manager to not allow creation/modification of
	//ghost objects.  This will prevent new ones from being recreated and allow
	//us to restore the existing ones.
	TheGhostObjectManager->lockGhostObjects(TRUE);

	obj = TheGameLogic->getFirstObject();
	while (obj) {
		obj->friend_notifyOfNewMapBoundary();
		obj = obj->getNextObject();
	}

	ThePartitionManager->restoreFoggedCells(partitionStore, TRUE);
	//reinsert ghost objects into the partition manager.
	TheGhostObjectManager->restorePartitionData();

	//Allow creation of new ghost objects since we restored all existing ones.
	TheGhostObjectManager->lockGhostObjects(FALSE);

	// Don't do a newMap on the pathfinder - It uses the largest active boundary to start.  jba.
	//TheAI->pathfinder()->newMap();

	TheTacticalView->forceCameraAreaConstraintRecalc();
}

// ------------------------------------------------------------------------------------------------
/** Flatten the terrain beneath a structure. */
// ------------------------------------------------------------------------------------------------
void TerrainLogic::flattenTerrain(Object *obj)
{
	if (obj->getGeometryInfo().getIsSmall()) {
		return;
	}

	const Coord3D *pos = obj->getPosition();
	switch(obj->getGeometryInfo().getGeomType())
	{
		case GEOMETRY_BOX:
		{
			Real angle = obj->getOrientation();

			Real halfsizeX = obj->getGeometryInfo().getMajorRadius();
			Real halfsizeY = obj->getGeometryInfo().getMinorRadius();


			Real c = (Real)Cos(angle);
			Real s = (Real)Sin(angle);

			Vector3 topLeft(pos->x-halfsizeX*c-halfsizeY*s, pos->y + halfsizeY*c - halfsizeX*s, 0);
			Vector3 topRight(pos->x+halfsizeX*c-halfsizeY*s, pos->y + halfsizeY*c + halfsizeX*s, 0);
			Vector3 bottomRight(pos->x+halfsizeX*c+halfsizeY*s, pos->y - halfsizeY*c + halfsizeX*s, 0);
			Vector3 bottomLeft(pos->x-halfsizeX*c+halfsizeY*s, pos->y - halfsizeY*c - halfsizeX*s, 0);

			Real minX = topLeft.X;
			if (minX>topRight.X) minX = topRight.X;
			if (minX>bottomRight.X) minX = bottomRight.X;
			if (minX>bottomLeft.X) minX = bottomLeft.X;
			Real maxX = topLeft.X;
			if (maxX<topRight.X) maxX = topRight.X;
			if (maxX<bottomRight.X) maxX = bottomRight.X;
			if (maxX<bottomLeft.X) maxX = bottomLeft.X;

			Real minY = topLeft.Y;
			if (minY>topRight.Y) minY = topRight.Y;
			if (minY>bottomRight.Y) minY = bottomRight.Y;
			if (minY>bottomLeft.Y) minY = bottomLeft.Y;
			Real maxY = topLeft.Y;
			if (maxY<topRight.Y) maxY = topRight.Y;
			if (maxY<bottomRight.Y) maxY = bottomRight.Y;
			if (maxY<bottomLeft.Y) maxY = bottomLeft.Y;

			ICoord2D iMin, iMax;
			iMin.x = REAL_TO_INT_FLOOR(minX/MAP_XY_FACTOR);
			iMin.y = REAL_TO_INT_FLOOR(minY/MAP_XY_FACTOR);
			iMax.x = REAL_TO_INT_FLOOR(maxX/MAP_XY_FACTOR);
			iMax.y = REAL_TO_INT_FLOOR(maxY/MAP_XY_FACTOR);

			Int i, j;
			Real totalHeight = 0;
			Int numSamples = 0;
			for (i=iMin.x; i<=iMax.x; i++) {
				for (j=0; j<=iMax.y; j++) {
					Vector3	testPt(i*MAP_XY_FACTOR, j*MAP_XY_FACTOR, 0);
					Bool match = false;
					unsigned char flags;
					if (Point_In_Triangle_2D(topLeft, topRight, bottomLeft, testPt, 0, 1, flags)) {
						match = true;
					}
					if (Point_In_Triangle_2D(topRight, bottomRight, bottomLeft, testPt, 0, 1, flags)) {
						match = true;
					}
					if (match) {
						totalHeight += getGroundHeight(testPt.X, testPt.Y);
						numSamples++;
					}
				}
			}
			if (numSamples == 0) return;
			Real avgHeight = totalHeight/numSamples;
			Int rawDataHeight = REAL_TO_INT_FLOOR(0.5f + avgHeight/MAP_HEIGHT_SCALE);

			// Compare to the height at the building's origin, because setRawMapHeight will only lower,
			// not raise.  jba
			Int centerHeight = REAL_TO_INT_FLOOR(getGroundHeight(pos->x, pos->y)/MAP_HEIGHT_SCALE);
			if (rawDataHeight>centerHeight) rawDataHeight = centerHeight;

			for (i=iMin.x; i<=iMax.x; i++) {
				for (j=0; j<=iMax.y; j++) {
					Vector3	testPt(i*MAP_XY_FACTOR, j*MAP_XY_FACTOR, 0);
					Bool match = false;
					unsigned char flags;
					if (Point_In_Triangle_2D(topLeft, topRight, bottomLeft, testPt, 0, 1, flags)) {
						match = true;
					}
					if (Point_In_Triangle_2D(topRight, bottomRight, bottomLeft, testPt, 0, 1, flags)) {
						match = true;
					}
					if (match) {
						ICoord2D gridPos;
						gridPos.x = i;
						gridPos.y = j;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i-1;
						gridPos.y = j;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i+1;
						gridPos.y = j;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i;
						gridPos.y = j-1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i;
						gridPos.y = j+1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);

						//Added the corners so it does a whole 3X3 square... ML
						gridPos.x = i-1;
						gridPos.y = j-1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i+1;
						gridPos.y = j+1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i+1;
						gridPos.y = j-1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i-1;
						gridPos.y = j+1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);

					}
				}
			}


		break;
		}
		case GEOMETRY_SPHERE:	// not quite right, but close enough
		case GEOMETRY_CYLINDER:
		{
			// fill in all cells that overlap as obstacle cells
			Real radius = obj->getGeometryInfo().getMajorRadius();
			Real radiusSqr = sqr(radius);
			ICoord2D iMin, iMax;
			iMin.x = REAL_TO_INT_FLOOR((pos->x-radius)/MAP_XY_FACTOR);
			iMin.y = REAL_TO_INT_FLOOR((pos->y-radius)/MAP_XY_FACTOR);
			iMax.x = REAL_TO_INT_FLOOR((pos->x+radius)/MAP_XY_FACTOR);
			iMax.y = REAL_TO_INT_FLOOR((pos->y+radius)/MAP_XY_FACTOR);

			Int i, j;
			Real totalHeight = 0;
			Int numSamples = 0;
			for (i=iMin.x; i<=iMax.x; i++) {
				for (j=0; j<=iMax.y; j++) {
					Vector3	testPt(i*MAP_XY_FACTOR, j*MAP_XY_FACTOR, 0);
					Bool match = false;
					Real dx = testPt.X - pos->x;
					Real dy = testPt.Y - pos->y;
					if ( dx*dx+dy*dy<radiusSqr) {
						match = true;
					}
					if (match) {
						totalHeight += getGroundHeight(testPt.X, testPt.Y);
						numSamples++;
					}
				}
			}
			if (numSamples == 0) return;
			Real avgHeight = totalHeight/numSamples;
			Int rawDataHeight = REAL_TO_INT_FLOOR(0.5f + avgHeight/MAP_HEIGHT_SCALE);
			for (i=iMin.x; i<=iMax.x; i++) {
				for (j=0; j<=iMax.y; j++) {
					Vector3	testPt(i*MAP_XY_FACTOR, j*MAP_XY_FACTOR, 0);
					Bool match = false;
					Real dx = testPt.X - pos->x;
					Real dy = testPt.Y - pos->y;
					if ( dx*dx+dy*dy<radiusSqr) {
						match = true;
					}
					if (match) {
						ICoord2D gridPos;
						gridPos.x = i;
						gridPos.y = j;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i-1;
						gridPos.y = j;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i+1;
						gridPos.y = j;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i;
						gridPos.y = j-1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i;
						gridPos.y = j+1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);

						//Added the corners so it does a whole 3X3 square... ML
						gridPos.x = i-1;
						gridPos.y = j-1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i+1;
						gridPos.y = j+1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i+1;
						gridPos.y = j-1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);
						gridPos.x = i-1;
						gridPos.y = j+1;
						TheTerrainVisual->setRawMapHeight(&gridPos, rawDataHeight);


					}
				}
			}

		}
		break;
	}

}



// ------------------------------------------------------------------------------------------------
/** Dig a deep circular gorge into the terrain beneath an object. */
// ------------------------------------------------------------------------------------------------
void TerrainLogic::createCraterInTerrain(Object *obj)
{
	if (obj->getGeometryInfo().getIsSmall())
		return;

	const Coord3D *pos = obj->getPosition();
  Real radius = obj->getGeometryInfo().getMajorRadius();

  if ( radius <= 0.0f )
    return; // sanity

  ICoord2D iMin, iMax;
  iMin.x = REAL_TO_INT_FLOOR( ( pos->x - radius ) / MAP_XY_FACTOR );
  iMin.y = REAL_TO_INT_FLOOR( ( pos->y - radius ) / MAP_XY_FACTOR );
  iMax.x = REAL_TO_INT_FLOOR( ( pos->x + radius ) / MAP_XY_FACTOR );
	iMax.y = REAL_TO_INT_FLOOR( ( pos->y + radius ) / MAP_XY_FACTOR );

  Real deltaX, deltaY;

	for (Int i = iMin.x; i <= iMax.x; i++ )
  {
		for ( Int j=0; j <= iMax.y; j++ )
    {
			deltaX = ( i * MAP_XY_FACTOR ) - pos->x;
			deltaY = ( j * MAP_XY_FACTOR ) - pos->y;

      Real distance = WWMath::SqrtOrigin( sqr( deltaX ) + sqr( deltaY ) );

			if ( distance < radius ) //inside circle
      {
				ICoord2D gridPos;
				gridPos.x = i;
				gridPos.y = j;


        Real displacementAmount = radius * (1.0f - distance / radius );

        Int targetHeight = MAX( 1, TheTerrainVisual->getRawMapHeight( &gridPos ) - displacementAmount );

				TheTerrainVisual->setRawMapHeight( &gridPos, targetHeight );
			}
    }
  }

}







// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TerrainLogic::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer
	* Version Info:
	* 1: Initial version
	* 2: Added water updates over time (CBD)
	*/
// ------------------------------------------------------------------------------------------------
void TerrainLogic::xfer( Xfer *xfer )
{

	// version
	const XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// active boundary
	Int activeBoundary = m_activeBoundary;
	xfer->xferInt( &activeBoundary );
	if( xfer->getXferMode() == XFER_LOAD )
		setActiveBoundary( activeBoundary );

	// updatable water tables
	if( version >= 2 )
	{

		// number of water entries in our update array
		xfer->xferInt( &m_numWaterToUpdate );

		// water update entry data
		for( UnsignedInt i = 0; i < m_numWaterToUpdate; ++i )
		{

			// water handle
			if( xfer->getXferMode() == XFER_SAVE )
			{

				// write ID of polygon trigger that this water handle is representing
				Int triggerID = m_waterToUpdate[ i ].waterTable->m_polygon->getID();
				xfer->xferInt( &triggerID );

			}
			else if (xfer->getXferMode() == XFER_LOAD)
			{

				// read trigger id
				Int triggerID;
				xfer->xferInt( &triggerID );

				// find polygon trigger
				PolygonTrigger *poly = PolygonTrigger::getPolygonTriggerByID( triggerID );

				// sanity
				if( poly == nullptr )
				{

					DEBUG_CRASH(( "TerrainLogic::xfer - Unable to find polygon trigger for water table with trigger ID '%d'",
												triggerID ));
					throw SC_INVALID_DATA;

				}

				// set water handle
				m_waterToUpdate[ i ].waterTable = poly->getWaterHandle();

				// sanity
				if( m_waterToUpdate[ i ].waterTable == nullptr )
				{

					DEBUG_CRASH(( "TerrainLogic::xfer - Polygon trigger to use for water handle has no water handle!" ));
					throw SC_INVALID_DATA;

				}

			}

			// change per frame
			xfer->xferReal( &m_waterToUpdate[ i ].changePerFrame );

			// target height
			xfer->xferReal( &m_waterToUpdate[ i ].targetHeight );

			// damage amount
			xfer->xferReal( &m_waterToUpdate[ i ].damageAmount );

			// current height
			xfer->xferReal( &m_waterToUpdate[ i ].currentHeight );

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TerrainLogic::loadPostProcess()
{
	Bridge* pBridge = getFirstBridge();
	Bridge* pNext;
	while (pBridge)
	{
		pNext = pBridge->getNext();
		Object* obj = TheGameLogic->findObjectByID(pBridge->peekBridgeInfo()->bridgeObjectID);
		if (obj == nullptr)
		{
			deleteBridge(pBridge);
		}
		pBridge = pNext;
	}

}

