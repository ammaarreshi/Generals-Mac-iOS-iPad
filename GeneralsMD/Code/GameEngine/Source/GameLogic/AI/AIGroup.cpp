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

// AIGroup.cpp
// Encapsulation of a simple group of AI agents
// Author: Michael S. Booth, January 2002
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine


#include "Common/ActionManager.h"
#include "Common/BuildAssistant.h"
#include "Common/CRCDebug.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "Common/XferCRC.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/Line2D.h"

#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/OverchargeBehavior.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"
#include "GameLogic/ObjectIter.h"


/**
 * NOTE: Only AI objects (ie: having an AIUpdate module) can be in
 * AIGroups.  It is ASSUMED that an object cannot morph from having
 * an AIUpdate module to not having one... (MSB)

	NOTE: This comment has been wrong for about ten years now.  Any Object can be in an AIGroup
 */

/**
 * Constructor
 */
AIGroup::AIGroup()
{
//	DEBUG_LOG(("***AIGROUP %x is being constructed.", this));
	m_groundPath = nullptr;
	m_speed = 0.0f;
	m_dirty = false;
	m_id = TheAI->getNextGroupID();
	m_memberListSize = 0;
	m_memberList.clear();
	//DEBUG_LOG(( "AIGroup #%d created", m_id ));
}

/**
 * Destructor
 */
AIGroup::~AIGroup()
{
//	DEBUG_LOG(("***AIGROUP %x is being destructed.", this));
	// disassociate each member from the group

#if RETAIL_COMPATIBLE_AIGROUP

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); /* empty */ )
	{
		Object *member = *i;
		if (member)
		{
			member->leaveGroup();
			i = m_memberList.begin();	// jump back to the beginning, cause ai->leaveGroup will remove this element.
		}
		else
		{
			i = m_memberList.erase(i);
		}
	}

#else

	removeAll();

#endif

	deleteInstance(m_groundPath);
	m_groundPath = nullptr;

	//DEBUG_LOG(( "AIGroup #%d destroyed", m_id ));
}

/**
 * Return this group's unique ID
 */
UnsignedInt AIGroup::getID()
{
	return m_id;
}

/**
 * Return the group IDs for every member in this group
 */
const VecObjectID& AIGroup::getAllIDs() const
{
	m_lastRequestedIDList.clear();
	for (std::list<Object *>::const_iterator cit = m_memberList.begin(); cit != m_memberList.end(); ++cit)
	{
		if ((*cit) == nullptr)
			continue;

		m_lastRequestedIDList.push_back((*cit)->getID());
	}

	return m_lastRequestedIDList;
}


/**
 * Return the speed of the group's slowest member
 */
Real AIGroup::getSpeed()
{
	if (m_dirty)
		recompute();

	return m_speed;
}

/**
 * Return true if object is in this group
 */
Bool AIGroup::isMember( Object *obj )
{
	std::list<Object *>::iterator i = std::find( m_memberList.begin(), m_memberList.end(), obj );

	if (i == m_memberList.end())
		return false;

	return true;
}

/**
 * Add object to group.
 * Only allow AI agents into the group.
 */
void AIGroup::add( Object *obj )
{
//	DEBUG_LOG(("***AIGROUP %x is adding Object %x (%s).", this, obj, obj->getTemplate()->getName().str()));
	DEBUG_ASSERTCRASH(obj != nullptr, ("trying to add null obj to AIGroup"));
	if (obj == nullptr)
		return;

	AIUpdateInterface *ai = obj->getAIUpdateInterface();

	//If this object doesn't have an AIUpdateInterface, then
	//don't add it to the group UNLESS it is a structure! Structures
	//with AIUpdateInterfaces also issue similar commands, but those
	//commands don't need AI updates... they are instant commands like
	//evacuate or triggering certain special powers...
	KindOfMaskType validNonAIKindofs;
	validNonAIKindofs.set(KINDOF_STRUCTURE);
	validNonAIKindofs.set(KINDOF_ALWAYS_SELECTABLE);
	if( ai == nullptr && !obj->isAnyKindOf( validNonAIKindofs ) )
	{
		return;
	}

	// add to group's list of objects
	m_memberList.push_back( obj );
	++m_memberListSize;
//	DEBUG_LOG(("***AIGROUP %x has size %u now.", this, m_memberListSize));

	obj->enterGroup( this );

	// list has changed, properties need recomputation
	m_dirty = true;
}

/**
 * Remove object from group
 */
Bool AIGroup::remove( Object *obj )
{
#if !RETAIL_COMPATIBLE_AIGROUP
	// Defer deletion until the end of this function.
	AIGroupPtr refThis = AIGroupPtr::Create_AddRef(this);
#endif

//	DEBUG_LOG(("***AIGROUP %x is removing Object %x (%s).", this, obj, obj->getTemplate()->getName().str()));
	std::list<Object *>::iterator i = std::find( m_memberList.begin(), m_memberList.end(), obj );

	// make sure object is actually in the group
	if (i == m_memberList.end())
		return FALSE;

	// remove it
	m_memberList.erase( i );
	--m_memberListSize;
//	DEBUG_LOG(("***AIGROUP %x has size %u now.", this, m_memberListSize));

	// tell object to forget about group
	obj->leaveGroup();

	// list has changed, properties need recomputation
	m_dirty = true;

	if (isEmpty()) {
#if RETAIL_COMPATIBLE_AIGROUP
		// if the group is empty, no-one is using it any longer, so destroy it
		TheAI->destroyGroup( this );
#endif
		return TRUE;
	}

	return FALSE;
}

/**
 * Remove all objects from group
 */
void AIGroup::removeAll()
{
#if !RETAIL_COMPATIBLE_AIGROUP
	// Defer deletion until the end of this function.
	AIGroupPtr refThis = AIGroupPtr::Create_AddRef(this);
#endif

	std::list<Object *> memberList;
	memberList.swap(m_memberList);
	m_memberListSize = 0;

	std::list<Object *>::iterator i;
	for ( i = memberList.begin(); i != memberList.end(); ++i )
	{
		Object *member = *i;
		if (member)
			member->leaveGroup();
	}

	m_dirty = true;
}

/**
 * If the group contains any objects not owned by ownerPlayer, return TRUE.
 */
Bool AIGroup::containsAnyObjectsNotOwnedByPlayer( const Player *ownerPlayer )
{
	ListObjectPtrIt it;

	for (it = m_memberList.begin(); it != m_memberList.end(); ++it) {
		Object *obj = (*it);
		if (!obj) {
			continue;
		}

		if (obj->getControllingPlayer() != ownerPlayer) {
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Remove any objects that aren't owned by the player, and return true if the group was destroyed due to emptiness
 */
Bool AIGroup::removeAnyObjectsNotOwnedByPlayer( const Player *ownerPlayer )
{
	ListObjectPtrIt it;

	for (it = m_memberList.begin(); it != m_memberList.end(); /* empty */) {
		Object *obj = (*it);
		if (!obj) {
			continue;
		}

		if (obj->getControllingPlayer() != ownerPlayer) {
			// Advance the iterator first, its about to become invalid.
			++it;

			if (remove(obj)) {
				return TRUE;
			}
			continue;
		}

		++it;
	}

	return FALSE;
}


/**
 * Compute the centroid of the group
 */
Bool AIGroup::getCenter( Coord3D *center )
{
	Int count = 0;
	center->x = 0.0f;
	center->y = 0.0f;
	center->z = 0.0f;

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		if( (*i)->isDisabledByType( DISABLED_HELD) )
		{
			continue; // don't bother counting riders in the center calculation.
		}
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			const Coord3D *objPos = (*i)->getPosition();
			center->x += objPos->x;
			center->y += objPos->y;
			center->z += objPos->z;
			++count;
		}
	}

	if (count == 0 && !m_memberList.empty())
	{
		/*
			if there are no AIs (eg, the team consists of a faction bldg), we can get here.

			This was originally used to offset the centers of objects moving (still used for that) and non-ais can't move.
			So if you have a mix of ai's & not ai's, you want just the ais.
			But it seems reasonable that if there are no ai's, it returns the center of the other stuff.  Cause they won't be moving anyway.
		*/
		for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
		{
			if( (*i)->isDisabledByType( DISABLED_HELD) )
			{
				continue; // don't bother counting riders in the center calculation.
			}
			const Coord3D *objPos = (*i)->getPosition();
			center->x += objPos->x;
			center->y += objPos->y;
			center->z += objPos->z;
			++count;
		}
	}

	center->x /= count;
	center->y /= count;
	center->z /= count;

	return count > 0;
}

Bool AIGroup::getMinMaxAndCenter( Coord2D *min, Coord2D *max, Coord3D *center )
{
	Int count = 0;
	min->x = 1e10f;
	max->x = -1e10f;
	min->y = 1e10f;
	max->y = -1e10f;
	center->x = 0.0f;
	center->y = 0.0f;
	center->z = 0.0f;

	std::list<Object *>::iterator i;
	FormationID id= NO_FORMATION_ID;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		if( (*i)->isDisabledByType( DISABLED_HELD) )
		{
			continue; // don't bother counting riders in the center calculation.
		}
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			const Coord3D *objPos = (*i)->getPosition();
			center->x += objPos->x;
			center->y += objPos->y;
			center->z += objPos->z;

			//Calculate the bounding coordinates of all units
			min->x = min->x > objPos->x ? objPos->x : min->x;
			max->x = max->x < objPos->x ? objPos->x : max->x;
			min->y = min->y > objPos->y ? objPos->y : min->y;
			max->y = max->y < objPos->y ? objPos->y : max->y;
			FormationID curID = (*i)->getFormationID() ;
			if (count==0) {
				id = curID;
			} else {
				if (id == NO_FORMATION_ID) {
					id = NO_FORMATION_ID;
				}
			}

			count++;
		}
	}

	center->x /= count;
	center->y /= count;
	center->z /= count;
	Bool isFormation = (id!=NO_FORMATION_ID);
	if (count<2) isFormation = false;
	return isFormation;
}


/**
 * Compute the speed of the team (its slowest member's speed),
 * and find the leader (closest to center of group).
 */
void AIGroup::recompute()
{
	Real closeDist = 999999999.9f;
	Real dx, dy, dist;
	const Coord3D *objPos;
	Coord3D center;

	getCenter( &center );

	deleteInstance(m_groundPath);
	m_groundPath = nullptr;

	m_speed = 9999999999.9f;

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		// don't consider immobile things for leadership
		if ((*i)->isKindOf(KINDOF_IMMOBILE))
			continue;

		if( (*i)->isDisabledByType( DISABLED_HELD) )
		{
			continue; // don't bother counting riders in the max speed calculation.
		}
		Object *obj = (*i);
		AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if (ai)
		{

			//
			// speed is slowest speed ... we won't consider slow speeds from objects that
			// are being penalized by their damage state, if we did the whole group would slow
			// down to that slowest penalized speed, instead only those objects that are penalized
			// will fall out of "formation" ... bummer for them!
			//
			Real maxSpeed = ai->getCurLocomotorSpeed();
			if( m_speed > maxSpeed &&
				  IS_CONDITION_BETTER( obj->getBodyModule()->getDamageState(), TheGlobalData->m_movementPenaltyDamageState ) )
				m_speed = maxSpeed;

			// leader is closest to the group's center
			objPos = obj->getPosition();
			dx = objPos->x - center.x;
			dy = objPos->y - center.y;
			dist = dx*dx + dy*dy;
			if (dist < closeDist)
			{
				closeDist = dist;
			}
		}
	}
	// clear "dirty bit" - data is up-to-date
	m_dirty = false;
}

/**
 * Return the number of objects in the group
 */
Int AIGroup::getCount()
{
	return m_memberListSize;
}

/**
 * Returns true if the group has no members
 */
Bool AIGroup::isEmpty() const
{
	return m_memberList.empty();
}

/**
 * Given a destination location, compute the destination position for
 * this object such that it keeps its relative position with the group.
 */
void AIGroup::computeIndividualDestination( Coord3D *dest, const Coord3D *groupDest,
																					 Object *obj, const Coord3D *center, Bool isFormation )
{
	Coord2D v;

	// compute vector from "group center" to self
	const Coord3D *pos = obj->getPosition();
	if (isFormation) {
		obj->getFormationOffset(&v);
	}	else {
		v.x = pos->x - center->x;
		v.y = pos->y - center->y;
	}
	Real length = v.length();
	if (length > 6*obj->getGeometryInfo().getBoundingCircleRadius()) {
		length = 6*obj->getGeometryInfo().getBoundingCircleRadius();
	}
	v.normalize();
	v.x *= length;
	v.y *= length;
	PathfindLayerEnum layer = TheTerrainLogic->getLayerForDestination(groupDest);

	// move to same offset at destination
	/// @todo use fast int->real type cast here later
	dest->x = groupDest->x + v.x;
	dest->y = groupDest->y + v.y;
	dest->z = TheTerrainLogic->getLayerHeight( dest->x, dest->y, layer );
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if (ai && ai->isDoingGroundMovement()) {
		if (isFormation) {
			TheAI->pathfinder()->adjustDestination(obj, ai->getLocomotorSet(), dest, nullptr);
		}	else {
			TheAI->pathfinder()->adjustDestination(obj, ai->getLocomotorSet(), dest, groupDest);
		}
		TheAI->pathfinder()->updateGoal(obj, dest, LAYER_GROUND);
	}

}

static const Int PATH_DIAMETER_IN_CELLS = 6;

//-------------------------------------------------------------------------------------------------
// Internal function for moving a group of infantry as a column.
//

/**
 * Move to given position(s)
 */
Bool AIGroup::friend_computeGroundPath( const Coord3D *pos, CommandSourceType cmdSource )

{

	if (m_dirty)
		recompute();

	std::list<Object *>::iterator i;
	// compute current centroid of the team
	Coord3D center;
	Coord2D min;
	Coord2D max;
	Real dx, dy;

	if (TheGlobalData->m_debugAI==AI_DEBUG_TERRAIN) return false;

	Bool closeEnough = false;
	getMinMaxAndCenter( &min, &max, &center );
	Real distSqr = 4*sqr(TheAI->getAiData()->m_distanceRequiresGroup);

	Int numInfantry = 0;
	Int numVehicles = 0;
	Object *centerVehicle = nullptr;
	Real distSqrCenterVeh = distSqr*10;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *obj = (*i);
		TheAI->pathfinder()->removeGoal(obj);
		if (obj->isDisabledByType( DISABLED_HELD ) )
		{
			continue; // don't bother telling the occupants to move.
		}
		if( obj->getAI()==nullptr )
		{
			continue;
		}
		if( obj->isKindOf( KINDOF_INFANTRY ) )
		{
 			numInfantry++;
		} else if (obj->isKindOf( KINDOF_VEHICLE)) {
			if (obj->isKindOf(KINDOF_AIRCRAFT)) {
				continue;
			}
			numVehicles++;
		} else {
			continue;
		}
		// Note - we are getting the closest of ANY type of unit for later testing intentionally. jba.
		Coord3D unitPos = *((*i)->getPosition());

		dx = unitPos.x-pos->x;
		dy = unitPos.y-pos->y;
		if (dx*dx+dy*dy<distSqr) {
			distSqr = dx*dx+dy*dy;
		}

		// find object closest to the center.
		dx = unitPos.x-center.x;
		dy = unitPos.y-center.y;
 		if (centerVehicle==nullptr || dx*dx+dy*dy<distSqrCenterVeh) {
			centerVehicle = (*i);
			distSqrCenterVeh = dx*dx+dy*dy;
		}
	}

	if(centerVehicle==nullptr) return false;
	center = *centerVehicle->getPosition();

	dx = max.x - min.x;
	dy = max.y - min.y;
	if (dx*dx + dy*dy > sqr(TheAI->getAiData()->m_distanceRequiresGroup)) {
		distSqr = dx*dx+dy*dy;
	}
	if (distSqr < sqr(TheAI->getAiData()->m_minDistanceForGroup)) {
		return false;
	}
	if (distSqr>sqr(TheAI->getAiData()->m_distanceRequiresGroup)) {
		closeEnough = true;
	}
	if (numInfantry>6) {
		closeEnough = true;
	}
	if (numVehicles>4) {
		closeEnough = true;
	}

	if (!closeEnough) {
		Bool isPassable = true;
		// see if all units have an unobstructed path to the center.
		// If so, then they are close enough.
		for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
		{
			Object *obj = (*i);
			if (!obj->isKindOf(KINDOF_INFANTRY)) {
				continue;
			}
			AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
			if (ai)
			{
				if (!TheAI->pathfinder()->isLinePassable(obj,
								ai->getLocomotorSet().getValidSurfaces(), obj->getLayer(), *obj->getPosition(),
								center, false, true)) {
					isPassable = false;
				}
			}
		}
		if (isPassable) closeEnough = true;
	}
	if (!closeEnough) return false;

	m_groundPath = TheAI->pathfinder()->findGroundPath(&center, pos, PATH_DIAMETER_IN_CELLS, false);
	return m_groundPath!=nullptr;

}

static void clampToMap(Coord3D *dest, PlayerType pt)
// Clamps to the player's current visible map area. jba. [8/28/2003]
{
	Region3D extent;
	if (pt==PLAYER_COMPUTER) {
		// AI gets to operate inside the pathable shrouded area. [8/28/2003]
		TheTerrainLogic->getMaximumPathfindExtent(&extent);
	} else {
		// Human player has to stay within the visible map.
		TheTerrainLogic->getExtent(&extent);
	}

	extent.hi.x -= PATHFIND_CELL_SIZE_F;
	extent.hi.y -= PATHFIND_CELL_SIZE_F;
	extent.lo.x += PATHFIND_CELL_SIZE_F;
	extent.lo.y += PATHFIND_CELL_SIZE_F;
	if (!extent.isInRegionNoZ(*dest)) {
		// clamp to in region. [8/28/2003]
		if (dest->x < extent.lo.x) {
			dest->x = extent.lo.x;
		}
		if (dest->y < extent.lo.y) {
			dest->y = extent.lo.y;
		}
		if (dest->x > extent.hi.x) {
			dest->x = extent.hi.x;
		}
		if (dest->y > extent.hi.y) {
			dest->y = extent.hi.y;
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Internal function for moving a group of infantry as a column.
//

/**
 * Move to given position(s)
 */
Bool AIGroup::friend_moveInfantryToPos( const Coord3D *pos, CommandSourceType cmdSource )

{
	if (m_groundPath==nullptr) return false;

	Int numColumns = 3;
	Int halfNumColumns = numColumns/2;
	Real dx, dy;
	Coord3D center;
	if (!getCenter( &center )) return false;

	// Get the start & end vectors for the path.
	Coord3D startPoint = *m_groundPath->getFirstNode()->getPosition();
	Real farEnoughSqr = sqr(PATH_DIAMETER_IN_CELLS*PATHFIND_CELL_SIZE_F);
	PathNode *startNode = nullptr;
	PathNode *node;
	for (node = m_groundPath->getFirstNode(); node; node=node->getNextOptimized()) {
		dx = node->getPosition()->x - startPoint.x;
		dy = node->getPosition()->y - startPoint.y;
		if (dx*dx+dy*dy>farEnoughSqr) {
			startNode = node;
			break;
		}
	}
	Coord3D endPoint = *m_groundPath->getLastNode()->getPosition();
	PathNode *endNode = nullptr;
	for (node = m_groundPath->getFirstNode(); node; node=node->getNextOptimized()) {
		Real dx = node->getPosition()->x - endPoint.x;
		Real dy = node->getPosition()->y - endPoint.y;
		if (dx*dx+dy*dy>farEnoughSqr) {
			endNode = node;
		}
	}
	if (startNode==nullptr || endNode==nullptr) {
		deleteInstance(m_groundPath);
		m_groundPath = nullptr;
		return false;
	}

	Coord2D startVector;
	startVector.x = startNode->getPosition()->x - startPoint.x;
	startVector.y = startNode->getPosition()->y - startPoint.y;
	startVector.normalize();

	Coord2D endVector;
	endVector.x = endPoint.x - endNode->getPosition()->x;
	endVector.y = endPoint.y - endNode->getPosition()->y;
	endVector.normalize();

	Coord2D startVectorNormal;
	startVectorNormal.x = -startVector.y;
	startVectorNormal.y = startVector.x;
	startVectorNormal.normalize();

	Coord2D endVectorNormal;
	endVectorNormal.x = -endVector.y;
	endVectorNormal.y = endVector.x;
	endVectorNormal.normalize();

	Bool useEndVector = false;
	Int unitsToPath = 0;
	// Move.
	MemoryPoolObjectHolder iterHolder;
	SimpleObjectIterator *iter = newInstance(SimpleObjectIterator);
	iterHolder.hold(iter);
	MemoryPoolObjectHolder iterHolder2;
	SimpleObjectIterator *iter2 = newInstance(SimpleObjectIterator);
	iterHolder2.hold(iter2);
	std::list<Object *>::iterator i;
	PlayerType controllingPlayerType = PLAYER_COMPUTER;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		if ((*i)->isDisabledByType( DISABLED_HELD ) )
		{
			continue; // don't bother telling the occupants to move.
		}
		if( !(*i)->isKindOf( KINDOF_INFANTRY ) )
		{
			continue;
		}
		if( (*i)->getAI()==nullptr )
		{
			continue;
		}
		if ( (*i)->isKindOf( KINDOF_MOB_NEXUS ) )
		{
			return FALSE;// means I did NOT do a column group pathfind,
			//so the nexus will have a far-away goal position for the mobsters to aim at
		}
		if ((*i)->getControllingPlayer()) {
			controllingPlayerType = (*i)->getControllingPlayer()->getPlayerType();
		}
		Coord3D unitPos = *((*i)->getPosition());
		TheAI->pathfinder()->removeGoal(*i);
		dx = unitPos.x - center.x;
		dy = unitPos.y - center.y;
		// Sort by the dot product of normal.
		iter->insert((*i), dx*startVectorNormal.x+dy*startVectorNormal.y);
		unitsToPath++;

		// If units are closer to the end vector than the start vector, use the end vector.
		Real distToEndSqr;
		Real distToStartSqr;
		dx = unitPos.x - endPoint.x;
		dy = unitPos.y - endPoint.y;
		distToEndSqr = dx*dx + dy*dy;
		dx = unitPos.x - startPoint.x;
		dy = unitPos.y - startPoint.y;
		distToStartSqr = dx*dx + dy*dy;
		if (distToStartSqr>distToEndSqr) {
			useEndVector = true;
		}

	}
	if (unitsToPath<TheAI->getAiData()->m_minInfantryForGroup) {
		return false;
	}

	Object *theUnit;
	if (useEndVector) {
		// resort using the end vector.
		startVector = endVector;
		startVectorNormal =	endVectorNormal;
		for (theUnit = iter->first(); theUnit; theUnit = iter->next()) iter2->insert(theUnit);
		iter->makeEmpty();
		for (theUnit = iter2->first(); theUnit; theUnit = iter2->next())
		{
			Coord3D unitPos = *(theUnit->getPosition());
			dx = unitPos.x - center.x;
			dy = unitPos.y - center.y;
			// Sort by the dot product of normal.
			iter->insert(theUnit, dx*startVectorNormal.x+dy*startVectorNormal.y);
		}
		iter2->makeEmpty();
	}


	iter->sort(ITER_SORTED_FAR_TO_NEAR);
	Int curIndex = 0;
	for (theUnit = iter->first(); theUnit; theUnit = iter->next())
	{
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		Int divisor = ((unitsToPath+1)/numColumns);
		if (divisor<1) divisor=1;
		Int columnDelta = 1-(curIndex/divisor);  // 1, 0, -1 from left to right across column. jba.
		if (columnDelta<-halfNumColumns) columnDelta=-halfNumColumns;
		divisor = ((unitsToPath+3)/5);
		if (divisor<1) divisor=1;
		Int fiveColumnDelta = 2-(curIndex/divisor);  // 2, 1, 0, -1, -2 from left to right across column. jba.
		if (fiveColumnDelta<-2) fiveColumnDelta=-2;
		if (unitsToPath<16) {
			fiveColumnDelta = columnDelta;
		}

		ai->setTmpValue( (fiveColumnDelta<<16)|(columnDelta&0x00ffff));
		// Sort next pass by the dot product of start vector.
		dx, dy;
		dx = theUnit->getPosition()->x - center.x;
		dy = theUnit->getPosition()->y - center.y;
		Int adjust = 0;
		LocomotorPriority movePriority = LOCO_MOVES_FRONT;
		if (ai->getCurLocomotor()) {
			movePriority = ai->getCurLocomotor()->getMovePriority();
			if (movePriority == LOCO_MOVES_MIDDLE) {
				adjust = -100*PATHFIND_CELL_SIZE_F;
			} else if (movePriority == LOCO_MOVES_BACK) {
				adjust = -200*PATHFIND_CELL_SIZE_F;
			}
		}
		iter2->insert(theUnit, adjust + dx*startVector.x + dy*startVector.y);
		curIndex++;

	}

	iter2->sort(ITER_SORTED_FAR_TO_NEAR);
	// Even out columns by priority.
	Int group;
	Int column3[3] = {0,0,0};
	Int column5[5] = {0,0,0,0,0};
	for (group = LOCO_MOVES_FRONT; group>=LOCO_MOVES_BACK; group--) {
		for (theUnit = iter2->first(); theUnit; theUnit = iter2->next())
		{
			AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
			Int tmp = ai->getTmpValue();
			LocomotorPriority movePriority = LOCO_MOVES_MIDDLE;
			if (ai->getCurLocomotor()) {
				movePriority = ai->getCurLocomotor()->getMovePriority();
			}
			if (group!=movePriority) continue;
			Int fiveColumnDelta = tmp>>16;
			Int columnDelta = (Short)(tmp & 0xFFFF);

			Int i;
			Int min3 = 10000;
			Int min5 = 10000;
			for (i=0; i<3; i++) if (column3[i]<min3) min3 = column3[i];
			for (i=0; i<5; i++) if (column5[i]<min5) min5 = column5[i];
			Int delta = 10000;
			Int best = -1;
			for (i=0; i<3; i++) {
				if (column3[i]==min3) {
					Int dx = (1+columnDelta)-i;
					if (dx<0) dx = -dx;
					if (dx<delta) {
						delta = dx;
						best = i;
					}
				}
			}
			if (best >= 0) {
				column3[best]++;
				columnDelta = best-1;
			}

			delta = 10000;
			best = -1;
			for (i=0; i<5; i++) {
				if (column5[i]==min5) {
					Int dx = (2+fiveColumnDelta)-i;
					if (dx<0) dx = -dx;
					if (dx<delta) {
						delta = dx;
						best = i;
					}
				}
			}
			if (best >= 0) {
				column5[best]++;
				fiveColumnDelta = best-2;
			}

			if (unitsToPath<16) {
				fiveColumnDelta = columnDelta;
			}
			ai->setTmpValue( (fiveColumnDelta<<16)|(columnDelta&0x00ffff));
		}
	}

	curIndex = 0;
	Int columnFactor[5] = {0,0,0,0,0};
	PathfindLayerEnum layer = TheTerrainLogic->getLayerForDestination(pos);
	for (theUnit = iter2->first(); theUnit; theUnit = iter2->next())
	{
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		Int tmp = ai->getTmpValue();
		Int fiveColumnDelta = tmp>>16;
		Int columnDelta = (Short)(tmp & 0xFFFF);
 		Int factor = columnFactor[fiveColumnDelta+2];
		columnFactor[fiveColumnDelta+2] = factor+1;

		std::vector<Coord3D> path;
		PathNode *node = startNode;
		PathNode *previousNode = m_groundPath->getFirstNode();
		Coord3D prevPos = *theUnit->getPosition();
		while (node) {
			Coord3D dest = *node->getPosition();
			PathNode *tmpNode;
			PathNode *nextNode=nullptr;
			for (tmpNode = node->getNextOptimized(); tmpNode; tmpNode=tmpNode->getNextOptimized()) {
				Real dx = tmpNode->getPosition()->x - dest.x;
				Real dy = tmpNode->getPosition()->y - dest.y;
				if (dx*dx+dy*dy>farEnoughSqr) {
					nextNode = tmpNode;
					break;
				}
			}
			if (nextNode==nullptr) break;
			Coord2D cornerVectorNormal;
			cornerVectorNormal.y = nextNode->getPosition()->x - previousNode->getPosition()->x;
			cornerVectorNormal.x = -(nextNode->getPosition()->y - previousNode->getPosition()->y);
			cornerVectorNormal.normalize();

			Coord2D cornerVector;
			cornerVector.x = nextNode->getPosition()->x - previousNode->getPosition()->x;
			cornerVector.y = nextNode->getPosition()->y - previousNode->getPosition()->y;

			Real offset = PATHFIND_CELL_SIZE_F*2.1f/halfNumColumns;
			dest.x += offset * columnDelta * cornerVectorNormal.x;
			dest.y += offset * columnDelta * cornerVectorNormal.y;
 			if (factor&1) {
				dest.x += 0.5f*PATHFIND_CELL_SIZE_F * cornerVectorNormal.x;
				dest.y += 0.5f*PATHFIND_CELL_SIZE_F * cornerVectorNormal.y;
			} else {
				dest.x -= 0.5f*PATHFIND_CELL_SIZE_F * cornerVectorNormal.x;
				dest.y -= 0.5f*PATHFIND_CELL_SIZE_F * cornerVectorNormal.y;
			}

			Coord2D curVector;
			curVector.x = dest.x-prevPos.x;
			curVector.y = dest.y-prevPos.y;

			clampToMap(&dest, controllingPlayerType);
			// Make sure that this dest is going in the same direction as the vector.
			if (cornerVector.x*curVector.x + cornerVector.y*curVector.y > 0) {
				path.push_back( dest );
				prevPos = dest;
			}
			node=node->getNextOptimized();

			for (tmpNode = previousNode->getNextOptimized(); tmpNode && tmpNode!=node; tmpNode=tmpNode->getNextOptimized()) {
				Real dx = tmpNode->getPosition()->x - node->getPosition()->x;
				Real dy = tmpNode->getPosition()->y - node->getPosition()->y;
				if (dx*dx+dy*dy>farEnoughSqr) {
					previousNode = tmpNode;
				}
			}
		}

		Coord3D dest = *pos;
		if (fiveColumnDelta<-2) fiveColumnDelta=-2;
		if (fiveColumnDelta>2) fiveColumnDelta=2;
		Real offset = PATHFIND_CELL_SIZE_F*2.2f;

		dest.x += offset * fiveColumnDelta * endVectorNormal.x;
		dest.y += offset * fiveColumnDelta * endVectorNormal.y;
		if (factor&1) {
			dest.x += PATHFIND_CELL_SIZE_F * endVectorNormal.x;
			dest.y += PATHFIND_CELL_SIZE_F * endVectorNormal.y;
		}

		LocomotorPriority movePriority = LOCO_MOVES_MIDDLE;
		if (ai->getCurLocomotor()) {
			movePriority = ai->getCurLocomotor()->getMovePriority();
		}
		Int delta = movePriority - LOCO_MOVES_FRONT;
		dest.x += delta*PATHFIND_CELL_SIZE_F*endVector.x;
		dest.y += delta*PATHFIND_CELL_SIZE_F*endVector.y;

		dest.x -= factor*offset*endVector.x;
		dest.y -= factor*offset*endVector.y;
		dest.z = TheTerrainLogic->getLayerHeight( dest.x, dest.y, layer );

		while (!path.empty()) {
			Coord2D curVector;
			prevPos = path[path.size()-1];
			curVector.x = dest.x-prevPos.x;
			curVector.y = dest.y-prevPos.y;

			// Make sure that this dest is going in the same direction as the vector.
			if (endVector.x*curVector.x + endVector.y*curVector.y <= 0) {
				path.pop_back();
			}	else {
				break;
			}
		}
		clampToMap(&dest, controllingPlayerType);
		TheAI->pathfinder()->adjustDestination(theUnit, ai->getLocomotorSet(), &dest, nullptr);
		TheAI->pathfinder()->updateGoal(theUnit, &dest, LAYER_GROUND);
		path.push_back(dest);
		ai->aiFollowPath( &path, nullptr, cmdSource );
	}
	return true;
}

/**
 * Move to given position(s)
 */
void AIGroup::friend_moveFormationToPos( const Coord3D *pos, CommandSourceType cmdSource )
{
	Real dx, dy;
	Coord3D center;
	if (!getCenter( &center )) return;


	PathNode *startNode = nullptr;
	PathNode *endNode = nullptr;
	Coord3D endPoint = *pos;
	if (m_groundPath) {
		// Get the start & end vectors for the path.
		Coord3D startPoint = *m_groundPath->getFirstNode()->getPosition();
		Real farEnoughSqr = sqr(PATH_DIAMETER_IN_CELLS*PATHFIND_CELL_SIZE_F);
		PathNode *node;
		for (node = m_groundPath->getFirstNode(); node; node=node->getNextOptimized()) {
			dx = node->getPosition()->x - startPoint.x;
			dy = node->getPosition()->y - startPoint.y;
			if (dx*dx+dy*dy>farEnoughSqr) {
				startNode = node;
				break;
			}
		}
		endPoint = *m_groundPath->getLastNode()->getPosition();
		for (node = m_groundPath->getFirstNode(); node; node=node->getNextOptimized()) {
			dx = node->getPosition()->x - endPoint.x;
			dy = node->getPosition()->y - endPoint.y;
			if (dx*dx+dy*dy>farEnoughSqr) {
				endNode = node;
			}
		}
		PathNode *tmpNode = endNode;
		while (tmpNode) {
			if (tmpNode == startNode) {
				endNode = nullptr;
			}
			tmpNode = tmpNode->getNextOptimized();
		}
		if (startNode==nullptr || endNode==nullptr) {
			deleteInstance(m_groundPath);
			m_groundPath = nullptr;
			startNode = nullptr;
			endNode = nullptr;
		}
	}


	// Move.
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		if ((*i)->isDisabledByType( DISABLED_HELD ) )
		{
			continue; // don't bother telling the occupants to move.
		}
		Object *theUnit = (*i);
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		if (ai == nullptr)
		{
			continue;
		}

		Bool isDifferentFormation = false;
		Coord2D offset;
		if (isDifferentFormation) {
			Coord3D pos = *theUnit->getPosition();
			offset.x = pos.x - center.x;
			offset.y = pos.y - center.y;
			theUnit->setFormationOffset(offset);
		}
		theUnit->getFormationOffset(&offset);
		if (startNode) {
			std::vector<Coord3D> path;
			PathNode *node = startNode;
			while (node) {
				Coord3D dest = *node->getPosition();
				dest.x += offset.x;
				dest.y += offset.y;

				path.push_back( dest );
				if (node==endNode) break;
				node=node->getNextOptimized();
			}

			Coord3D dest = endPoint;
			dest.x += offset.x;
			dest.y += offset.y;

			TheAI->pathfinder()->adjustDestination(theUnit, ai->getLocomotorSet(), &dest, nullptr);
			TheAI->pathfinder()->updateGoal(theUnit, &dest, LAYER_GROUND);
			path.push_back(dest);
			ai->aiFollowPath( &path, nullptr, cmdSource );
		}	else {
			Coord3D dest = endPoint;
			dest.x += offset.x;
			dest.y += offset.y;
			ai->aiMoveToPosition( &dest, cmdSource );
		}

	}
}
//-------------------------------------------------------------------------------------------------
// Internal function for moving a group of vehicles as a column.
//

/**
 * Move to given position(s)
 */
Bool AIGroup::friend_moveVehicleToPos( const Coord3D *pos, CommandSourceType cmdSource )

{

	if (m_groundPath==nullptr) return false;

	Real dx, dy;
	Coord3D center;
	if (!getCenter( &center )) return false;

	if (!m_groundPath) {
		return false;
	}

	Int numColumns = 2;


	// Get the start & end vectors for the path.
	Coord3D startPoint = *m_groundPath->getFirstNode()->getPosition();
	Real farEnoughSqr = sqr(PATH_DIAMETER_IN_CELLS*PATHFIND_CELL_SIZE_F);
	PathNode *startNode = nullptr;
	PathNode *node;
	for (node = m_groundPath->getFirstNode(); node; node=node->getNextOptimized()) {
		Real dx = node->getPosition()->x - startPoint.x;
		Real dy = node->getPosition()->y - startPoint.y;
		if (dx*dx+dy*dy>farEnoughSqr) {
			startNode = node;
			break;
		}
	}
	Coord3D endPoint = *m_groundPath->getLastNode()->getPosition();
	PathNode *endNode = nullptr;
	for (node = m_groundPath->getFirstNode(); node; node=node->getNextOptimized()) {
		Real dx = node->getPosition()->x - endPoint.x;
		Real dy = node->getPosition()->y - endPoint.y;
		if (dx*dx+dy*dy>farEnoughSqr) {
			endNode = node;
		}
	}
	if (endNode == m_groundPath->getFirstNode()) {
		endNode = nullptr;
	}
	if (startNode==nullptr || endNode==nullptr) {
		deleteInstance(m_groundPath);
		m_groundPath = nullptr;
		return false;
	}

	Coord2D startVector;
	startVector.x = startNode->getPosition()->x - startPoint.x;
	startVector.y = startNode->getPosition()->y - startPoint.y;
	startVector.normalize();

	Coord2D endVector;
	endVector.x = endPoint.x - endNode->getPosition()->x;
	endVector.y = endPoint.y - endNode->getPosition()->y;
	endVector.normalize();

	Coord2D startVectorNormal;
	startVectorNormal.x = -startVector.y;
	startVectorNormal.y = startVector.x;
	startVectorNormal.normalize();

	Coord2D endVectorNormal;
	endVectorNormal.x = -endVector.y;
	endVectorNormal.y = endVector.x;
	endVectorNormal.normalize();

	Int unitsToPath = 0;
	Bool useEndVector = false;
	// Move.
	MemoryPoolObjectHolder iterHolder;
	SimpleObjectIterator *iter = newInstance(SimpleObjectIterator);
	iterHolder.hold(iter);
	MemoryPoolObjectHolder iterHolder2;
	SimpleObjectIterator *iter2 = newInstance(SimpleObjectIterator);
	iterHolder2.hold(iter2);
	PlayerType controllingPlayerType = PLAYER_COMPUTER;
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		if ((*i)->isDisabledByType( DISABLED_HELD ) )
		{
			continue; // don't bother telling the occupants to move.
		}
		if( !(*i)->isKindOf( KINDOF_VEHICLE ) )
		{
			continue;
		}
		if( (*i)->getAI()==nullptr )
		{
			continue;
		}
		if( !(*i)->getAI()->isDoingGroundMovement() )
		{
			continue;
		}
		if ((*i)->getControllingPlayer()) {
			controllingPlayerType = (*i)->getControllingPlayer()->getPlayerType();
		}
		Coord3D unitPos = *((*i)->getPosition());
		TheAI->pathfinder()->removeGoal(*i);
		Real dx, dy;
		dx = unitPos.x - center.x;
		dy = unitPos.y - center.y;
		// Sort by the dot product of normal.
		iter->insert((*i), dx*startVectorNormal.x+dy*startVectorNormal.y);
		unitsToPath++;

		// If units are closer to the end vector than the start vector, use the end vector.
		Real distToEndSqr;
		Real distToStartSqr;
		dx = unitPos.x - endPoint.x;
		dy = unitPos.y - endPoint.y;
		distToEndSqr = dx*dx + dy*dy;
		dx = unitPos.x - startPoint.x;
		dy = unitPos.y - startPoint.y;
		distToStartSqr = dx*dx + dy*dy;
		if (distToStartSqr>distToEndSqr) {
			useEndVector = true;
		}
	}

	if (unitsToPath<TheAI->getAiData()->m_minVehiclesForGroup) {
		return false;
	}

	Object *theUnit;
	if (useEndVector) {
		// resort using the end vector.
		startVector = endVector;
		startVectorNormal =	endVectorNormal;
		for (theUnit = iter->first(); theUnit; theUnit = iter->next()) iter2->insert(theUnit);
		iter->makeEmpty();
		for (theUnit = iter2->first(); theUnit; theUnit = iter2->next())
		{
			Coord3D unitPos = *(theUnit->getPosition());
			dx = unitPos.x - center.x;
			dy = unitPos.y - center.y;
			// Sort by the dot product of normal.
			iter->insert(theUnit, dx*startVectorNormal.x+dy*startVectorNormal.y);
		}
		iter2->makeEmpty();
	}

	iter->sort(ITER_SORTED_FAR_TO_NEAR);
	Int curIndex = 0;
	for (theUnit = iter->first(); theUnit; theUnit = iter->next())
	{
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		Int divisor = ((unitsToPath+1)/numColumns);
		if (divisor<1) divisor=1;
		Int columnDelta = 1-(curIndex/divisor);  // 0, 1 from left to right across column. jba.
		if (columnDelta==0) columnDelta=-1;
		divisor = ((unitsToPath+1)/3);
		if (divisor<1) divisor=1;
		Int threeColumnDelta = (curIndex/divisor);  // 0, 1, 2 from left to right across column. jba.
		threeColumnDelta = 1-threeColumnDelta; // 1, 0, -1
		if (threeColumnDelta<-1) threeColumnDelta=-1;
		if (unitsToPath<5) {
			threeColumnDelta = columnDelta;
		}

		ai->setTmpValue( (threeColumnDelta<<16)|(columnDelta&0x00ffff));
		// Sort next pass by the dot product of start vector.
		Real dx, dy;
		dx = theUnit->getPosition()->x - center.x;
		dy = theUnit->getPosition()->y - center.y;
		Int adjust = 0;
#if 0
		LocomotorPriority movePriority = LOCO_MOVES_FRONT;
		if (ai->getCurLocomotor()) {
			movePriority = ai->getCurLocomotor()->getMovePriority();
			if (movePriority == LOCO_MOVES_MIDDLE) {
				adjust = -100*PATHFIND_CELL_SIZE_F;
			} else if (movePriority == LOCO_MOVES_BACK) {
				adjust = -200*PATHFIND_CELL_SIZE_F;
			}
		}
#endif
		iter2->insert(theUnit, adjust + dx*startVector.x + dy*startVector.y);
		curIndex++;

	}

	iter2->sort(ITER_SORTED_FAR_TO_NEAR);
	// Even out columns by priority.
	Int group;
	Int column2[3] = {0,0,0};
	Int column3[3] = {0,0,0};
	for (group = LOCO_MOVES_FRONT; group>=LOCO_MOVES_BACK; group--) {
		for (theUnit = iter2->first(); theUnit; theUnit = iter2->next())
		{
			AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
			Int tmp = ai->getTmpValue();
#if 0
			LocomotorPriority movePriority = LOCO_MOVES_MIDDLE;
			if (ai->getCurLocomotor()) {
				movePriority = ai->getCurLocomotor()->getMovePriority();
			}
			if (group!=movePriority) continue;
#endif
			Int threeColumnDelta = tmp>>16;
			Int columnDelta = (Short)(tmp & 0xFFFF);

			Int i;
			Int min2 = 10000;
			Int min3 = 10000;
			for (i=0; i<3; i+=2) if (column2[i]<min2) min2 = column2[i];
			for (i=0; i<3; i++) if (column3[i]<min3) min3 = column3[i];
			Int delta = 10000;
			Int best = -1;
			for (i=0; i<3; i+=2) {
				if (column2[i]==min2) {
					Int dx = (1+columnDelta)-i;
					if (dx<0) dx = -dx;
					if (dx<delta) {
						delta = dx;
						best = i;
					}
				}
			}
			if (best >= 0) {
				column2[best]++;
				columnDelta = best-1;
			}

			delta = 10000;
			best = -1;
			for (i=0; i<3; i++) {
				if (column3[i]==min3) {
					Int dx = (1+threeColumnDelta)-i;
					if (dx<0) dx = -dx;
					if (dx<delta) {
						delta = dx;
						best = i;
					}
				}
			}
			if (best >= 0) {
				column3[best]++;
				threeColumnDelta = best-1;
			}

			if (unitsToPath<5) {
				threeColumnDelta = columnDelta;
			}
			ai->setTmpValue( (threeColumnDelta<<16)|(columnDelta&0x00ffff));
		}
	}




	curIndex = 0;
	Int columnFactor[5] = {0,0,0,0,0};
	PathfindLayerEnum layer = TheTerrainLogic->getLayerForDestination(pos);
	for (theUnit = iter2->first(); theUnit; theUnit = iter2->next())
	{
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		Int tmp = ai->getTmpValue();
		Int threeColumnDelta = tmp>>16;
		Int columnDelta = (Short)(tmp & 0xFFFF);
 		Int factor = columnFactor[threeColumnDelta+2];
		columnFactor[threeColumnDelta+2] = factor+1;

		std::vector<Coord3D> path;
		PathNode *node = startNode;
		PathNode *previousNode = m_groundPath->getFirstNode();
		Coord3D prevPos = *theUnit->getPosition();
		while (node) {
			Coord3D dest = *node->getPosition();
			PathNode *tmpNode;
			PathNode *nextNode=nullptr;
			for (tmpNode = node->getNextOptimized(); tmpNode; tmpNode=tmpNode->getNextOptimized()) {
				Real dx = tmpNode->getPosition()->x - dest.x;
				Real dy = tmpNode->getPosition()->y - dest.y;
				if (dx*dx+dy*dy>farEnoughSqr) {
					nextNode = tmpNode;
					break;
				}
			}
			if (nextNode==nullptr) break;
			Coord2D cornerVectorNormal;
			cornerVectorNormal.y = nextNode->getPosition()->x - previousNode->getPosition()->x;
			cornerVectorNormal.x = -(nextNode->getPosition()->y - previousNode->getPosition()->y);
			cornerVectorNormal.normalize();

			Coord2D cornerVector;
			cornerVector.x = nextNode->getPosition()->x - previousNode->getPosition()->x;
			cornerVector.y = nextNode->getPosition()->y - previousNode->getPosition()->y;

			Real offset = PATHFIND_CELL_SIZE_F*1.5f;
			dest.x += offset * columnDelta * cornerVectorNormal.x;
			dest.y += offset * columnDelta * cornerVectorNormal.y;
 			if (factor&1) {
				dest.x += 0.5f*PATHFIND_CELL_SIZE_F * cornerVectorNormal.x;
				dest.y += 0.5f*PATHFIND_CELL_SIZE_F * cornerVectorNormal.y;
			} else {
				dest.x -= 0.5f*PATHFIND_CELL_SIZE_F * cornerVectorNormal.x;
				dest.y -= 0.5f*PATHFIND_CELL_SIZE_F * cornerVectorNormal.y;
			}

			Coord2D curVector;
			curVector.x = dest.x-prevPos.x;
			curVector.y = dest.y-prevPos.y;
			clampToMap(&dest, controllingPlayerType);
			// Make sure that this dest is going in the same direction as the vector.
			if (cornerVector.x*curVector.x + cornerVector.y*curVector.y > 0) {
				path.push_back( dest );
				prevPos = dest;
			}

			node=node->getNextOptimized();

			for (tmpNode = previousNode->getNextOptimized(); tmpNode && tmpNode!=node; tmpNode=tmpNode->getNextOptimized()) {
				Real dx = tmpNode->getPosition()->x - node->getPosition()->x;
				Real dy = tmpNode->getPosition()->y - node->getPosition()->y;
				if (dx*dx+dy*dy>farEnoughSqr) {
					previousNode = tmpNode;
				}
			}
		}

		Coord3D dest = *pos;
		if (threeColumnDelta<-3) threeColumnDelta=-3;
		if (threeColumnDelta>3) threeColumnDelta=3;
		Real offset = PATHFIND_CELL_SIZE_F*3.2f;
		if (unitsToPath<5) {
			offset = PATHFIND_CELL_SIZE_F*1.5f;
		}
		dest.x += offset * threeColumnDelta * endVectorNormal.x;
		dest.y += offset * threeColumnDelta * endVectorNormal.y;
		if (factor&1) {
			dest.x += PATHFIND_CELL_SIZE_F * endVectorNormal.x;
			dest.y += PATHFIND_CELL_SIZE_F * endVectorNormal.y;
		}
#if 0
		LocomotorPriority movePriority = LOCO_MOVES_MIDDLE;
		if (ai->getCurLocomotor()) {
			movePriority = ai->getCurLocomotor()->getMovePriority();
		}
		Int delta = movePriority - LOCO_MOVES_FRONT;
		dest.x += delta*PATHFIND_CELL_SIZE_F*endVector.x;
		dest.y += delta*PATHFIND_CELL_SIZE_F*endVector.y;
#endif
		dest.x -= factor*offset*endVector.x;
		dest.y -= factor*offset*endVector.y;
		dest.z = TheTerrainLogic->getLayerHeight( dest.x, dest.y, layer );

		while (!path.empty()) {
			Coord2D curVector;
			prevPos = path[path.size()-1];
			curVector.x = dest.x-prevPos.x;
			curVector.y = dest.y-prevPos.y;

			// Make sure that this dest is going in the same direction as the vector.
			if (endVector.x*curVector.x + endVector.y*curVector.y <= 0) {
				path.pop_back();
			}	else {
				break;
			}
		}
		clampToMap(&dest, controllingPlayerType);
		TheAI->pathfinder()->adjustDestination(theUnit, ai->getLocomotorSet(), &dest, nullptr);
		TheAI->pathfinder()->updateGoal(theUnit, &dest, LAYER_GROUND);
		path.push_back(dest);
		ai->aiFollowPath( &path, nullptr, cmdSource );
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
// AI Command Interface implementation for AIGroup
//
const Int STD_WAYPOINT_CLAMP_MARGIN = ( PATHFIND_CELL_SIZE_F * 4.0f );
const Int STD_AIRCRAFT_EXTRA_MARGIN = ( PATHFIND_CELL_SIZE_F * 10.0f );

void clampWaypointPosition( Coord3D &position, Int margin )
{
	Region3D mapExtent;
	TheTerrainLogic->getExtent(&mapExtent);

  // trim some fat off of all sides,
  mapExtent.hi.x -= margin;
  mapExtent.hi.y -= margin;
  mapExtent.lo.x += margin;
  mapExtent.lo.y += margin;

	if ( mapExtent.isInRegionNoZ( position ) == FALSE )
  {
    if ( position.x > mapExtent.hi.x )
      position.x = mapExtent.hi.x;
    else if ( position.x < mapExtent.lo.x )
      position.x = mapExtent.lo.x;

    if ( position.y > mapExtent.hi.y )
      position.y = mapExtent.hi.y;
    else if ( position.y < mapExtent.lo.y )
      position.y = mapExtent.lo.y;

    position.z = TheTerrainLogic->getGroundHeight( position.x, position.y );
  }
}


/**
 * Move to given position(s)
 */
void AIGroup::groupMoveToPosition( const Coord3D *p_posIn, Bool addWaypoint, CommandSourceType cmdSource )
{

  Coord3D position = *p_posIn;
  Coord3D *pos = &position;

	Bool didInfantry = false;
	Bool didVehicles = false;
	// compute current centroid of the team
	Coord3D center;
	Coord2D min;
	Coord2D max;
	Coord3D dest;
	Bool tightenGroup = FALSE;

	Bool isFormation = getMinMaxAndCenter( &min, &max, &center );
	if (addWaypoint)
  {
    isFormation = false;
  }


	if (!addWaypoint && !isFormation) {
		friend_computeGroundPath(pos, cmdSource);
		didInfantry = friend_moveInfantryToPos(pos, cmdSource);
		didVehicles = friend_moveVehicleToPos(pos, cmdSource);
	}
	if (m_dirty)
		recompute();

	std::list<Object *>::iterator i;
	if( !isFormation && cmdSource == CMD_FROM_PLAYER && TheGlobalData->m_groupMoveClickToGatherFactor > 0.0f )
	{
		ScaleRect2D( &min, &max, TheGlobalData->m_groupMoveClickToGatherFactor );

		if( Coord3DInsideRect2D( pos, &min, &max ) )
		{
			tightenGroup = TRUE;
		}
	}

  Real extraMargin = 0.0f;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
    const Object *groupMember = (*i);

    if ( groupMember->isKindOf( KINDOF_PRODUCED_AT_HELIPAD ) )//helicopter
    {
      isFormation = FALSE;
      extraMargin = MAX( extraMargin, groupMember->getGeometryInfo().getMajorRadius() );
    }
    else if ( groupMember->isKindOf( KINDOF_AIRCRAFT ) )// fixed wing aircraft only
    {
			if ( groupMember->getAI() && groupMember->getAI()->isDoingGroundMovement() == FALSE ) //if unit is airborne
      {
				tightenGroup = FALSE;	// Don't tighten aircraft.  It is a bad idea. jba.
				isFormation = FALSE;//then keep spread formation after move
      }

      extraMargin = MAX( extraMargin, STD_AIRCRAFT_EXTRA_MARGIN );
		}
	}

  Int margin = STD_WAYPOINT_CLAMP_MARGIN + extraMargin;
  clampWaypointPosition( position, margin );




	if (tightenGroup)
	{
		isFormation = false;
		if (!addWaypoint) {
			Int dx = (max.x-min.x)/PATHFIND_CELL_SIZE_F;
			Int dy = (max.x-min.x)/PATHFIND_CELL_SIZE_F;
			Int cells = (dx*dy);
			if (cells<2000) {
				groupTightenToPosition(pos, false, cmdSource);
				return;
			}
		}
	}

	if (isFormation) {
		friend_computeGroundPath(pos, cmdSource);
		friend_moveFormationToPos(pos, cmdSource);
		return;
	}

	// Move.
	MemoryPoolObjectHolder iterHolder;
	SimpleObjectIterator *iter = newInstance(SimpleObjectIterator);
	iterHolder.hold(iter);
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Real dx, dy;
		if ((*i)->isDisabledByType( DISABLED_HELD ) )
		{
			continue; // don't bother telling the occupants to move.
		}
		if( (*i)->isKindOf( KINDOF_IMMOBILE ) )
		{
			continue;
		}
		if( (*i)->getAI()==nullptr )
		{
			continue;
		}
		if ((*i)->isKindOf(KINDOF_INFANTRY) && didInfantry) {
			continue;
		}
		if ((*i)->isKindOf(KINDOF_VEHICLE) && didVehicles)
		{
			if( (*i)->getAI()->isDoingGroundMovement() )
			{
				Object *obj = (*i);
				if( !obj->isKindOf( KINDOF_CLIFF_JUMPER ) )
				{
					//Not a cliff-jumper-offer unit.
					continue;
				}
			}
		}
		Coord3D unitPos = *((*i)->getPosition());
		TheAI->pathfinder()->removeGoal(*i);
		dx = unitPos.x - pos->x;
		dy = unitPos.y - pos->y;
		// adjust so units are sorted first by move priority.
		Real adjust = 0;
#if 0	 // Nope.  jba.
		LocomotorPriority movePriority = LOCO_MOVES_FRONT;
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai->getCurLocomotor()) {
			movePriority = ai->getCurLocomotor()->getMovePriority();
			if (movePriority == LOCO_MOVES_MIDDLE) {
				adjust = 100*100*PATHFIND_CELL_SIZE_F*PATHFIND_CELL_SIZE_F;
			} else if (movePriority == LOCO_MOVES_BACK) {
				adjust = 200*200*PATHFIND_CELL_SIZE_F*PATHFIND_CELL_SIZE_F;
			}
		}
#endif
		iter->insert((*i), adjust + dx*dx+dy*dy);
	}

	Coord3D goalPos = *pos;
	iter->sort(ITER_SORTED_NEAR_TO_FAR);
	// Works better if you let the near units get the first paths... jba.
	// Move the ones nearest the goal first.  Reduces collision problems later.
	Object *theUnit;
	Bool firstUnit = true;
	for (theUnit = iter->first(); theUnit; theUnit = iter->next())
	{
		theUnit->setFormationID(NO_FORMATION_ID);
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		if (firstUnit) {
			if (isFormation) {
				Coord2D v;
				theUnit->getFormationOffset(&v);
				goalPos.x -= v.x;
				goalPos.y -= v.y;
			}	else {
				center = *theUnit->getPosition();
			}
			firstUnit = false;
		}
		computeIndividualDestination( &dest, &goalPos, theUnit, &center, isFormation );

		if( cmdSource == CMD_FROM_PLAYER && theUnit->getStatusBits().test( OBJECT_STATUS_CAN_STEALTH ) && ai->canAutoAcquire() )
		{
			//When ordering a combat stealth unit to move, there is a single special case we want to handle.
			//When a stealth unit is currently not stealthed and doesn't autoacquire while stealthed,
			//then when the player specifically orders the unit to stop, we want to not autoacquire until
			//he is able to stealth again. Of course, if he's detected, then don't bother trying.
			if( !theUnit->getStatusBits().test( OBJECT_STATUS_STEALTHED ) && !theUnit->getStatusBits().test( OBJECT_STATUS_DETECTED ) )
			{
				//Not stealthed, not detected -- so do auto-acquire while stealthed?
				if( !ai->canAutoAcquireWhileStealthed() )
				{
          StealthUpdate *stealth = theUnit->getStealth();
					if( stealth )
					{
						//Delay the mood check time (for autoacquire) until after the unit can stealth again.
						UnsignedInt stealthFrames = stealth->getStealthDelay();
						//Skew it a little due to having a large group selected.
						UnsignedInt randomFrames = GameLogicRandomValue( 0, LOGICFRAMES_PER_SECOND );
						ai->setNextMoodCheckTime( TheGameLogic->getFrame() + stealthFrames + randomFrames );
					}
				}
			}
		}

		if( !addWaypoint )
		{
			ai->aiMoveToPosition( &dest, cmdSource );
		}
		else
		{
			ai->aiFollowPathAppend(&dest, cmdSource);
		}
	}
}

//-------------------------------------------------------------------------------------------------
// AI Command Interface implementation for AIGroup
//

/**
 * Scatter
 */
void AIGroup::groupScatter( CommandSourceType cmdSource )
{
	if (m_dirty)
		recompute();

	std::list<Object *>::iterator i;
	// compute current centroid of the team
	Coord3D center;
	Coord2D min;
	Coord2D max;
	Coord3D dest;

	getMinMaxAndCenter( &min, &max, &center );

	// Move.
	MemoryPoolObjectHolder iterHolder;
	SimpleObjectIterator *iter = newInstance(SimpleObjectIterator);
	iterHolder.hold(iter);
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Real dx, dy;
		if ((*i)->isDisabledByType( DISABLED_HELD ) )
		{
			continue; // don't bother telling the occupants to move.
		}
		if( (*i)->isKindOf( KINDOF_IMMOBILE ) )
		{
			continue;
		}
		if( (*i)->getAI()==nullptr )
		{
			continue;
		}
		Coord3D unitPos = *((*i)->getPosition());
		TheAI->pathfinder()->removeGoal(*i);
		dx = unitPos.x - center.x;
		dy = unitPos.y - center.y;
		iter->insert((*i), dx*dx+dy*dy);
	}

	iter->sort(ITER_SORTED_FAR_TO_NEAR);
	Object *theUnit;
	for (theUnit = iter->first(); theUnit; theUnit = iter->next())
	{
		center.x -= 0.01f;
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		Coord3D unitPos = *theUnit->getPosition();
		Coord2D delta;
		dest = unitPos;
		delta.x = unitPos.x - center.x;
		delta.y = unitPos.y - center.y;
		delta.normalize();
		dest.x += delta.x*4*theUnit->getGeometryInfo().getBoundingCircleRadius();
		dest.y += delta.y*4*theUnit->getGeometryInfo().getBoundingCircleRadius();
		ai->aiMoveToPosition( &dest, cmdSource );
	}
}


const Real CIRCLE = ( 2.0f * PI );

void getHelicopterOffset( Coord3D& posOut, Int idx )
{
  if (idx == 0)
    return;

  Real assumedHeliDiameter = 70.0f;
  Real radius = assumedHeliDiameter;
  Real circumference = radius * CIRCLE;
  Real angle = 0;
  Real angleBetweenEachChopper = assumedHeliDiameter / circumference * CIRCLE;
  for (Int h = 1; h < idx; ++h )
  {
    angle += angleBetweenEachChopper;

    if ( angle > CIRCLE )
    {
      radius += assumedHeliDiameter;
      circumference = radius * CIRCLE;
      angleBetweenEachChopper = assumedHeliDiameter / circumference * CIRCLE;
      angle -= CIRCLE;
    }
  }

  Coord3D tempCtr = posOut;
  posOut.x = tempCtr.x + (WWMath::SinTrig(angle) * radius);
  posOut.y = tempCtr.y + (WWMath::CosTrig(angle) * radius);

}


/**
 * Move to given position(s), tightening the formation
 */
void AIGroup::groupTightenToPosition( const Coord3D *pos, Bool addWaypoint, CommandSourceType cmdSource )
{
	//Kris: Disabled (because its not used to make a logical difference)
	//Bool outsideOfBounds = true;
	Coord3D center;
	Coord2D min;
	Coord2D max;
	if( cmdSource == CMD_FROM_PLAYER && TheGlobalData->m_groupMoveClickToGatherFactor > 0.0f )
	{
		getMinMaxAndCenter( &min, &max, &center );
		//Kris: Disabled (because its not used to make a logical difference)
		//if( Coord3DInsideRect2D( pos, &min, &max ) )
		//{
		//	outsideOfBounds = FALSE;
		//}
	}
	// Tighten.
	MemoryPoolObjectHolder iterHolder;
	SimpleObjectIterator *iter = newInstance(SimpleObjectIterator);
	iterHolder.hold(iter);

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )	{
		Real dx, dy;
		Coord3D unitPos = *((*i)->getPosition());
		if ((*i)->isDisabledByType( DISABLED_HELD ) )
		{
			continue; // don't bother telling the occupants to move.
		}
		if( (*i)->isKindOf( KINDOF_IMMOBILE ) )
		{
			continue;
		}
		if( (*i)->getAI()==nullptr )
		{
			continue;
		}
		dx = unitPos.x - pos->x;
		dy = unitPos.y - pos->y;
		iter->insert((*i), dx*dx+dy*dy);
	}

	iter->sort(ITER_SORTED_NEAR_TO_FAR);
	// Works better if you let the near units get the first paths... jba.

  // Need a special case for helicopters, which do tighten when in groups
  // but who do not reserve ground when they pathfind
  // so we will send each new helicopter found in this list to a discrete
  // offset from 'pos' from the s_helicopterFormation table
  // a more elegant solution should have been added to AIPathfind, but given
  // the late date, this is much safer.

  Int heliIdx = 0;
	Object *theUnit;
	for (theUnit = iter->first(); theUnit; theUnit = iter->next())
	{
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		if( !addWaypoint )
		{
      if ( theUnit->isKindOf( KINDOF_PRODUCED_AT_HELIPAD ) ) //NEW
      {
        Coord3D heliOffs = *pos;
        getHelicopterOffset( heliOffs, heliIdx++ );
        ai->aiTightenToPosition( &heliOffs, CMD_FROM_AI );//NEW
      }
      else
  			ai->aiTightenToPosition( pos, cmdSource );
		}
		else
		{
			ai->aiFollowPathAppend(pos, cmdSource);
		}
	}

	for (theUnit = iter->first(); theUnit; theUnit = iter->next())
	{
		Coord3D unitPos = *theUnit->getPosition();
		TheAI->pathfinder()->updatePos(theUnit, &unitPos);
	}
}




/**
 * Start following the path from the given point
 */
void AIGroup::groupFollowWaypointPath( const Waypoint *way, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiFollowWaypointPath( way, cmdSource );
		}
	}
}

/**
 * Start following the path from the given point
 */
void AIGroup::groupFollowWaypointPathExact( const Waypoint *way, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiFollowWaypointPathExact( way, cmdSource );
		}
	}
}

/**
 * Move to given position and unload transports.
 */
void AIGroup::groupMoveToAndEvacuate( const Coord3D *pos, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiMoveToAndEvacuate( pos, cmdSource );
		}
	}
}

/**
 * Move to given position and unload transports.
 * transport returns and deletes itself.
 */
void AIGroup::groupMoveToAndEvacuateAndExit( const Coord3D *pos, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiMoveToAndEvacuateAndExit( pos, cmdSource );
		}
	}
}

/**
 * Start following the path from the given point
 */
void AIGroup::groupFollowWaypointPathAsTeam( const Waypoint *way, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiFollowWaypointPathAsTeam( way, cmdSource );
		}
	}
}

/**
 * Start following the path from the given point
 */
void AIGroup::groupFollowWaypointPathAsTeamExact( const Waypoint *way, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiFollowWaypointPathExactAsTeam( way, cmdSource );
		}
	}
}

//Callback for groupIdle -- contained buildings.
void makeMemberStop( Object *obj, void* userData )
{
	CommandSourceType cmdSource = *((CommandSourceType*)userData);
	if( obj )
	{
		AIUpdateInterface *ai = obj->getAI();
		if( ai )
		{
			ai->aiIdle( cmdSource );
		}
	}
}

/**
 * Enter the idle state.
 */
void AIGroup::groupIdle(CommandSourceType cmdSource)
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *obj = *i;

		AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if (ai)
		{
			ai->aiIdle(cmdSource);

			if( cmdSource == CMD_FROM_PLAYER && obj->getStatusBits().test( OBJECT_STATUS_CAN_STEALTH ) && ai->canAutoAcquire() )
			{
				//When ordering a combat stealth unit to stop, there is a single special case we want to handle.
				//When a stealth unit is currently not stealthed and doesn't autoacquire while stealthed,
				//then when the player specifically orders the unit to stop, we want to not autoacquire until
				//he is able to stealth again. Of course, if he's detected, then don't bother trying.
				if( !obj->getStatusBits().test( OBJECT_STATUS_STEALTHED ) && !obj->getStatusBits().test( OBJECT_STATUS_DETECTED ) )
				{
					//Not stealthed, not detected -- so do auto-acquire while stealthed?
					if( !ai->canAutoAcquireWhileStealthed() )
					{
            StealthUpdate *stealth = obj->getStealth();
						if( stealth )
						{
							//Delay the mood check time (for autoacquire) until after the unit can stealth again.
							UnsignedInt stealthFrames = stealth->getStealthDelay();
							//Skew it a little due to having a large group selected.
							UnsignedInt randomFrames = GameLogicRandomValue( 0, LOGICFRAMES_PER_SECOND );
							ai->setNextMoodCheckTime( TheGameLogic->getFrame() + stealthFrames + randomFrames );
						}
					}
				}
			}
		}
		else
		{
			//Handle garrisoned buildings.
			ContainModuleInterface *contain = obj->getContain();
			if( contain )
			{
				contain->iterateContained( makeMemberStop, &cmdSource, false );
			}
		}

		//Also handle slaves. If we have slaves, then order them to stop too!
		SpawnBehaviorInterface *spawnInterface = obj->getSpawnBehaviorInterface();
		if( spawnInterface )
		{
			spawnInterface->orderSlavesToGoIdle( cmdSource );
			//Do we need to delay mood check?
		}

	}
}

/**
 * Follow the path defined by the given array of points
 */
void AIGroup::groupFollowPath( const std::vector<Coord3D>* path, Object *ignoreObject, CommandSourceType cmdSource )
{
}

/**
 * Attack given object
 */
/**
 * Attack given object
 */
void AIGroup::groupAttackObjectPrivate( Bool forced, Object *victim, Int maxShotsToFire, CommandSourceType cmdSource )
{
	if (!victim) {
		// Hard to kill em if they're already dead.  jba
		return;
	}
	Coord3D victimPos = *victim->getPosition();
	MemoryPoolObjectHolder iterHolder;
	SimpleObjectIterator *iter = newInstance(SimpleObjectIterator);
	iterHolder.hold(iter);

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )	{
		Real dx, dy;
		Coord3D unitPos = *((*i)->getPosition());
#if RETAIL_COMPATIBLE_CRC
		// TheSuperHackers @bugfix Stubbjax 03/08/2025 This logic block erroneously prevents the
		// occupants of DISABLED_HELD units (e.g. undead Battle Buses) from responding to attack commands.
		if ((*i)->isDisabledByType( DISABLED_HELD ) )
		{
			continue; // don't bother telling the occupants to move.
		}
#endif
		dx = unitPos.x - victimPos.x;
		dy = unitPos.y - victimPos.y;
		iter->insert((*i), dx*dx+dy*dy);
	}

	iter->sort(ITER_SORTED_NEAR_TO_FAR);
	// Works better if you let the near units get the first paths... jba.
	Object *theUnit;
	for (theUnit = iter->first(); theUnit; theUnit = iter->next())
	{
		//Determine if this object is a garrisoned container capable of firing!
		//If so, order everyone inside to attack as well!
		ContainModuleInterface *contain = theUnit->getContain();
		if( contain && contain->isPassengerAllowedToFire() )
		{
			//Loop through each member and order them to attack the same target (if possible)
			const ContainedItemsList* items = contain->getContainedItemsList();
			if (items)
			{
				for( ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it )
				{
					Object* garrisonedMember = *it;
					CanAttackResult result = garrisonedMember->getAbleToAttackSpecificObject( forced ? ATTACK_NEW_TARGET_FORCED : ATTACK_NEW_TARGET, victim, cmdSource );
					if( result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING )
					{
						AIUpdateInterface *memberAI = garrisonedMember->getAI();
						if( memberAI )
						{
							if (forced)
								memberAI->aiForceAttackObject( victim, maxShotsToFire, cmdSource );
							else
								memberAI->aiAttackObject( victim, maxShotsToFire, cmdSource );
						}
					}
				}
			}
		}

		//Do a check to see if we have a hive object that has slaved objects.
		SpawnBehaviorInterface *spawnInterface = theUnit->getSpawnBehaviorInterface();
		if( spawnInterface && !spawnInterface->doSlavesHaveFreedom() )
		{
			spawnInterface->orderSlavesToAttackTarget( victim, maxShotsToFire, cmdSource );
		}

		//Order the specific group object to attack!
		AIUpdateInterface *ai = theUnit->getAIUpdateInterface();
		if( ai && theUnit != victim )
		{
			if (forced)
				ai->aiForceAttackObject( victim, maxShotsToFire, cmdSource );
			else
				ai->aiAttackObject( victim, maxShotsToFire, cmdSource );
		}
	}
}

/**
 * Attack the given team
 */
void AIGroup::groupAttackTeam( const Team *team, Int maxShotsToFire, CommandSourceType cmdSource )
{
	if (!team) {
		// Hard to kill em if they're already dead.  jba
		return;
	}
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiAttackTeam( team, maxShotsToFire, cmdSource );
		}
	}
}

/**
 * Attack given spot
 */
void AIGroup::groupAttackPosition( const Coord3D *pos, Int maxShotsToFire, CommandSourceType cmdSource )
{
	Coord3D attackPos;
	if( pos )
	{
		attackPos = *pos;
	}
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		if( !pos )
		{
			//If you specify a nullptr position, it means you are attacking your own location.
			attackPos.set( *(*i)->getPosition() );
		}

		//This code allows garrisoned buildings to force attack a ground position
		//-----------------------------------------------------------------------
		//Determine if this object is a garrisoned container capable of firing!
		//If so, order everyone inside to attack as well!
		ContainModuleInterface *contain = (*i)->getContain();
		if( contain && contain->isPassengerAllowedToFire() )
		{
			//Loop through each member and order them to attack the same target (if possible)
			const ContainedItemsList* items = contain->getContainedItemsList();
			if (items)
			{
				for( ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it )
				{
					Object* garrisonedMember = *it;
					CanAttackResult result = garrisonedMember->getAbleToUseWeaponAgainstTarget( ATTACK_NEW_TARGET, nullptr, &attackPos, cmdSource ) ;
					if( result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING )
					{
						AIUpdateInterface *memberAI = garrisonedMember->getAI();
						if( memberAI )
						{
							memberAI->aiAttackPosition( &attackPos, maxShotsToFire, cmdSource );
						}
					}
				}
			}
		}

		//Also handle slaves. If we have slaves, then order them to stop too!
		SpawnBehaviorInterface *spawnInterface = (*i)->getSpawnBehaviorInterface();
		if( spawnInterface && !spawnInterface->doSlavesHaveFreedom() )
		{
			spawnInterface->orderSlavesToAttackPosition( &attackPos, maxShotsToFire, cmdSource );
		}

		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiAttackPosition( &attackPos, maxShotsToFire, cmdSource );
		}
	}
}

/**
 * Attack move to a location
 */
void AIGroup::groupAttackMoveToPosition( const Coord3D *pos, Int maxShotsToFire, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			if ((*i)->isAbleToAttack())
				ai->aiAttackMoveToPosition( pos, maxShotsToFire, cmdSource );
			else
				ai->aiMoveToPosition( pos, cmdSource );
		}
	}
}

/**
 * Begin "seek and destroy"
 */
void AIGroup::groupHunt( CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiHunt( cmdSource );
		}
	}
}


/**
 * Repair the given object
 */
void AIGroup::groupRepair( Object *obj, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiRepair( obj, cmdSource );
		}
	}
}

/**
	* Resume construction on object
	*/
void AIGroup::groupResumeConstruction( Object *obj, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiResumeConstruction( obj, cmdSource );
		}
	}
}

/**
 * Get healed at the heal depot
 */
void AIGroup::groupGetHealed( Object *healDepot, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiGetHealed( healDepot, cmdSource );
		}
	}
}

/**
 * Get repaired at the repair depot
 */
void AIGroup::groupGetRepaired( Object *repairDepot, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiGetRepaired( repairDepot, cmdSource );
		}
	}
}

/**
 * Enter the given object
 */
void AIGroup::groupEnter( Object *obj, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiEnter( obj, cmdSource );
		}
	}
}

/**
 * Get near given object and wait for enter clearance
 */
void AIGroup::groupDock( Object *obj, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiDock( obj, cmdSource );
		}
	}
}

/**
 * Get out of whatever it is inside of
 */
void AIGroup::groupExit( Object *objectToExit,  CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiExit( objectToExit, cmdSource );
		}
	}
}

/**
 * Empty its contents
 */
void AIGroup::groupEvacuate( CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			if( (*i)->isKindOf( KINDOF_AIRCRAFT ) && (*i)->isAirborneTarget() )
			{
				//Calculate the highest point on the ground to drop off troops (chinook or other air transports)
				Coord3D pos = *((*i)->getPosition());
				PathfindLayerEnum layerAtDest = TheTerrainLogic->getHighestLayerForDestination( &pos );
				pos.z = TheTerrainLogic->getLayerHeight( pos.x, pos.y, layerAtDest );
				ai->aiMoveToAndEvacuate( &pos, cmdSource );
			}
			else
			{
				ai->aiEvacuate( FALSE, cmdSource );
			}
		}
		else if( (*i)->isKindOf( KINDOF_STRUCTURE ) )
		{
			//Buildings don't normally have AIUpdateInterfaces. In this special
			//case, simple call the function directly. Special powers work in a similar
			//manner.
			ContainModuleInterface *contain = (*i)->getContain();
			if( contain )
			{
				contain->orderAllPassengersToExit( cmdSource, FALSE );
			}
		}
	}
}

/**
	* Execute railed transport behavior
	*/
void AIGroup::groupExecuteRailedTransport( CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();

		if( ai )
			ai->aiExecuteRailedTransport( cmdSource );

	}

}

///< life altering state change, if this AI can do it
void AIGroup::groupGoProne( const DamageInfo *damageInfo, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiGoProne( damageInfo, cmdSource );
		}
	}
}

/**
 * Guard the given spot
 */
void AIGroup::groupGuardPosition( const Coord3D *pos, GuardMode guardMode, CommandSourceType cmdSource )
{
	if (!pos) {
		return;
	}

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiGuardPosition( pos, guardMode, cmdSource );
		}
	}
}

/**
 * Guard the given object
 */
void AIGroup::groupGuardObject( Object *objToGuard, GuardMode guardMode, CommandSourceType cmdSource )
{
	if (!objToGuard) {
		return;
	}

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiGuardObject( objToGuard, guardMode, cmdSource );
		}
	}
}

/**
 * Guard the given area
 */
void AIGroup::groupGuardArea( const PolygonTrigger *areaToGuard, GuardMode guardMode, CommandSourceType cmdSource )
{
	if (!areaToGuard) {
		return;
	}

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiGuardArea( areaToGuard, guardMode, cmdSource );
		}
	}
}

/**
 * Attack the given area
 */
void AIGroup::groupAttackArea( const PolygonTrigger *areaToGuard, CommandSourceType cmdSource )
{
	if (!areaToGuard) {
		return;
	}

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiAttackArea( areaToGuard, cmdSource );
		}
	}
}

void AIGroup::groupHackInternet( CommandSourceType cmdSource )				///< Begin hacking the internet for free cash from the heavens.
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->aiHackInternet( cmdSource );
		}
	}
}


void AIGroup::groupCreateFormation( CommandSourceType cmdSource )				///< Create a formation.
{
	Coord3D center;
	Coord2D min;
	Coord2D max;
	Bool isFormation = getMinMaxAndCenter( &min, &max, &center );
	std::list<Object *>::iterator i;
	FormationID id = TheAI->getNextFormationID();

	Int count = 0;
	FormationID countID = NO_FORMATION_ID;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		count++;
		countID = (*i)->getFormationID();
	}
	if (count==1 && countID!=NO_FORMATION_ID) {
		isFormation = true;
	}

	if (isFormation) {
		id = NO_FORMATION_ID;
	}

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *obj = (*i);
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			Coord3D pos = *obj->getPosition();
			Coord2D offset;
			offset.x = pos.x - center.x;
			offset.y = pos.y - center.y;
			obj->setFormationID(id);
			obj->setFormationOffset(offset);
		}
	}
}

/**
 * The unit(s)/structure will perform it's special power -- special powers triggered by buildings
 * don't use AIUpdateInterfaces!!! No special power uses an AIUpdateInterface immediately, but special
 * abilities, which are derived from special powers do... and are unit triggered. Those do have AI.
 */
void AIGroup::groupDoSpecialPower( UnsignedInt specialPowerID, UnsignedInt commandOptions )
{
	//This is the no target, no position version.
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		//Special powers do a lot of different things, but the top level stuff doesn't use
		//ai interface code. It finds the special power module and calls it directly for each object.
		Object *object = (*i);
		const SpecialPowerTemplate *spTemplate = TheSpecialPowerStore->findSpecialPowerTemplateByID( specialPowerID );
		if( spTemplate )
		{
			// Have to justify the execution in case someone changed their button
			if( spTemplate->getRequiredScience() != SCIENCE_INVALID )
			{
				if( !object->getControllingPlayer()->hasScience(spTemplate->getRequiredScience()) )
					continue;// Nice try, smacktard.
			}

			SpecialPowerModuleInterface *mod = object->getSpecialPowerModule( spTemplate );
			if( mod )
			{
				if( TheActionManager->canDoSpecialPower( object, spTemplate, CMD_FROM_PLAYER, commandOptions ) )
				{
					mod->doSpecialPower( commandOptions );

					object->friend_setUndetectedDefector( FALSE );// My secret is out
				}
			}
		}
	}
}

/**
 * The unit(s)/structure will perform it's special power -- special powers triggered by buildings
 * don't use AIUpdateInterfaces!!! No special power uses an AIUpdateInterface immediately, but special
 * abilities, which are derived from special powers do... and are unit triggered. Those do have AI.
 */
void AIGroup::groupDoSpecialPowerAtLocation( UnsignedInt specialPowerID, const Coord3D *location, Real angle, const Object *objectInWay, UnsignedInt commandOptions )
{


	//This one requires a position
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); )
	{
		//Special powers do a lot of different things, but the top level stuff doesn't use
		//ai interface code. It finds the special power module and calls it directly for each object.

		Object *object = (*i);

    ++i; // just in case the act of specialpowering changes this list,
         // like when the rebelambush happens over the ocean, and all the rebels drown
         // and, of course, their slowdeath behavior calls deselect(), which naturally
         // destroys the AIGroup list, in order to keep the selection sync'ed with the group.
         // M Lorenzen... 8/23/03

    const SpecialPowerTemplate *spTemplate = TheSpecialPowerStore->findSpecialPowerTemplateByID( specialPowerID );
		if( spTemplate )
		{
			// Have to justify the execution in case someone changed their button
			if( spTemplate->getRequiredScience() != SCIENCE_INVALID )
			{
				if( !object->getControllingPlayer()->hasScience(spTemplate->getRequiredScience()) )
					continue;// Nice try, smacktard.
			}

			SpecialPowerModuleInterface *mod = object->getSpecialPowerModule( spTemplate );
			if( mod )
			{
				if( TheActionManager->canDoSpecialPowerAtLocation( object, location, CMD_FROM_PLAYER, spTemplate, objectInWay, commandOptions ) )
				{
					mod->doSpecialPowerAtLocation( location, angle, commandOptions );

					object->friend_setUndetectedDefector( FALSE );// My secret is out
				}
			}
		}

	}
}

/**
 * The unit(s)/structure will perform it's special power -- special powers triggered by buildings
 * don't use AIUpdateInterfaces!!! No special power uses an AIUpdateInterface immediately, but special
 * abilities, which are derived from special powers do... and are unit triggered. Those do have AI.
 */
void AIGroup::groupDoSpecialPowerAtObject( UnsignedInt specialPowerID, Object *target, UnsignedInt commandOptions )
{
	//This one requires a target
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		//Special powers do a lot of different things, but the top level stuff doesn't use
		//ai interface code. It finds the special power module and calls it directly for each object.

		Object *object = (*i);
		const SpecialPowerTemplate *spTemplate = TheSpecialPowerStore->findSpecialPowerTemplateByID( specialPowerID );
		if( spTemplate )
		{
			// Have to justify the execution in case someone changed their button
			if( spTemplate->getRequiredScience() != SCIENCE_INVALID )
			{
				if( !object->getControllingPlayer()->hasScience(spTemplate->getRequiredScience()) )
					continue;// Nice try, smacktard.
			}

			SpecialPowerModuleInterface *mod = object->getSpecialPowerModule( spTemplate );
			if( mod )
			{
				if( TheActionManager->canDoSpecialPowerAtObject( object, target, CMD_FROM_PLAYER, spTemplate, commandOptions ) )
				{
					mod->doSpecialPowerAtObject( target, commandOptions );

					object->friend_setUndetectedDefector( FALSE );// My secret is out
				}
			}
		}
	}
}

#ifdef ALLOW_SURRENDER
void AIGroup::groupSurrender( const Object *objWeSurrenderedTo, Bool surrender, CommandSourceType cmdSource )
{
	//This is currently only activated via test key
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->setSurrendered(objWeSurrenderedTo, surrender);
		}
	}
}
#endif

void AIGroup::groupCheer( CommandSourceType cmdSource )
{
	//This is currently only activated via test key
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *object = (*i);
		//This allows all special conditions states to reset after a specified delay. Assume
		//only one works at a time (it'll clear any others).
		object->setSpecialModelConditionState( MODELCONDITION_SPECIAL_CHEERING, LOGICFRAMES_PER_SECOND * 3 );
	}
}

/**
	* Sell all things in the group ... if possible
	*/
void AIGroup::groupSell( CommandSourceType cmdSource )
{
#if !RETAIL_COMPATIBLE_AIGROUP
	// Defer deletion until the end of this function.
	AIGroupPtr refThis = AIGroupPtr::Create_AddRef(this);
#endif

	std::list<Object *>::iterator i;
	std::vector<Object *> groupObjectsCopy;
	groupObjectsCopy.reserve(m_memberListSize);

	// TheSuperHackers @bugfix Mauller 26/06/2025 when sellObject is called, the member list objects in this AIGroup get removed from it. This happens within the Object::deselectObject() function.
	// This deletes the local AIGroup object from under the call to groupSell, therefore we need to make a local copy of the objects that need selling.
	for( i = m_memberList.begin(); i != m_memberList.end(); /*empty*/ )
	{
		groupObjectsCopy.push_back( *i++ );
	}

	for( size_t j = 0; j < groupObjectsCopy.size(); ++j )
	{
		TheBuildAssistant->sellObject( groupObjectsCopy[j] );
	}

}

/**
	* Tell all things in the group to toggle overcharge ... if possible
	*/
void AIGroup::groupToggleOvercharge( CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	Object *obj;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{

		// get object
		obj = *i;

		OverchargeBehaviorInterface *obi;
		for( BehaviorModule **bmi = obj->getBehaviorModules(); *bmi; ++bmi )
		{

			obi = (*bmi)->getOverchargeBehaviorInterface();
			if( obi )
				obi->toggle();

		}

	}

}

#ifdef ALLOW_SURRENDER
/**
	* Pick up prisoners of war
	*/
void AIGroup::groupPickUpPrisoner( Object *prisoner, enum CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	Object *obj;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{

		// get object
		obj = *i;

		AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if( ai )
			ai->aiPickUpPrisoner( prisoner, cmdSource );

	}

}
#endif

#ifdef ALLOW_SURRENDER
/**
	* Return to prison
	*/
void AIGroup::groupReturnToPrison( Object *prison, enum CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	Object *obj;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{

		// get object
		obj = *i;

		AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if( ai )
			ai->aiReturnPrisoners( prison, cmdSource );

	}
}
#endif

/**
	* Combat drop
	*/
void AIGroup::groupCombatDrop( Object *target, const Coord3D &pos, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	Object *obj;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{

		// get object
		obj = *i;

		// do action
		AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if( ai )
			ai->aiCombatDrop( target, pos, cmdSource );

	}

}

//-------------------------------------------------------------------------------------
// Used by scripts to issue a command button order - Note that it's possible that some
// commands are not AI commands!
//-------------------------------------------------------------------------------------
void AIGroup::groupDoCommandButton( const CommandButton *commandButton, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	Object *source;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{

		// get object
		source = *i;

		source->doCommandButton( commandButton, cmdSource );
	}
}


//-------------------------------------------------------------------------------------
// Used by scripts to issue a command button order - Note that it's possible that some
// commands are not AI commands!
//-------------------------------------------------------------------------------------
void AIGroup::groupDoCommandButtonAtPosition( const CommandButton *commandButton, const Coord3D *pos, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	Object *source;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{

		// get object
		source = *i;

		source->doCommandButtonAtPosition( commandButton, pos, cmdSource );
	}
}

//-------------------------------------------------------------------------------------
// Used by scripts to issue a command button order - Note that it's possible that some
// commands are not AI commands!
//-------------------------------------------------------------------------------------
void AIGroup::groupDoCommandButtonUsingWaypoints( const CommandButton *commandButton, const Waypoint *way, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	Object *source;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{

		// get object
		source = *i;

		source->doCommandButtonUsingWaypoints( commandButton, way, cmdSource );
	}
}

//-------------------------------------------------------------------------------------
// Used by scripts to issue a command button order - Note that it's possible that some
// commands are not AI commands!
//-------------------------------------------------------------------------------------
void AIGroup::groupDoCommandButtonAtObject( const CommandButton *commandButton, Object *obj, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	Object *source;

	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{

		// get object
		source = *i;

		source->doCommandButtonAtObject( commandButton, obj, cmdSource );
	}
}


/**
 * Set the behavior modifier for this agent
 */
void AIGroup::setAttitude( AttitudeType tude )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		AIUpdateInterface *ai = (*i)->getAIUpdateInterface();
		if (ai)
		{
			ai->setAttitude( tude );
		}
	}
}

/**
 * Get the current behavior modifier state
 */
AttitudeType AIGroup::getAttitude() const
{
	return ATTITUDE_PASSIVE;
}

void AIGroup::setMineClearingDetail( Bool set )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		if (set)
			(*i)->setWeaponSetFlag(WEAPONSET_MINE_CLEARING_DETAIL);
		else
			(*i)->clearWeaponSetFlag(WEAPONSET_MINE_CLEARING_DETAIL);
	}
}

Bool AIGroup::setWeaponLockForGroup( WeaponSlotType weaponSlot, WeaponLockType lockType )
{
	Bool any = false;
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		if ((*i)->setWeaponLock( weaponSlot, lockType ))
			any = true;
	}
	return any;
}

void AIGroup::releaseWeaponLockForGroup(WeaponLockType lockType)
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		(*i)->releaseWeaponLock(lockType);
	}
}

//This function loops through the AIGroup setting the weaponset only for those units that
//have the specified weaponset. If a member doesn't have the weaponset, nothing happens for
//that unit.
void AIGroup::setWeaponSetFlag( WeaponSetType wst )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *obj = (*i);
		//First check to see if our object even has the specified weaponset. It's very
		//likely that a selected group won't all have the same weaponset options, so
		//only set it for those members that have it.
		WeaponSetFlags flags;
		flags.set( wst );
		const WeaponTemplateSet* set = obj->getTemplate()->findWeaponTemplateSet( flags );
		if( set )
		{
			obj->setWeaponSetFlag( wst );
		}
	}
}

void AIGroup::queueUpgrade( const UpgradeTemplate *upgrade )
{
	if (!upgrade)
		return;

	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *thisMember = (*i);
		// make sure that the this object can actually build the upgrade
		// There is an extra check for Object type only.  These are the same checks as in
		// ControlCommandProcessing when the message was going out.  We are just revalidating on the
		// way in to stop cheaters.
		if( ! TheUpgradeCenter->canAffordUpgrade( thisMember->getControllingPlayer(), upgrade, FALSE ) )
		{
			continue;
		}
		if( upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT )
		{
			if( thisMember->hasUpgrade( upgrade )  || !thisMember->affectedByUpgrade( upgrade ) )
				continue;
		}

		// Ever think to check if this thing can actually build the upgrade to "stop cheaters"?
		if( !thisMember->canProduceUpgrade(upgrade) )
			continue;// They have faked their button; go out of sync. (Cheater will execute it, non cheater will not execute it.)

		// producer must have a production update
		ProductionUpdateInterface *pu = thisMember->getProductionUpdateInterface();
		if( pu == nullptr )
			continue;

		if ( pu->canQueueUpgrade( upgrade ) == CANMAKE_QUEUE_FULL )
			continue;//So we don't charge them for something that we can't build... happy happy


		// queue the upgrade "research"
		pu->queueUpgrade( upgrade );
	}
}

//------------------------------------------------------------------------------------------------------------
Bool AIGroup::isIdle() const
{
	Bool isIdle = true;
	std::list<Object *>::const_iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *obj = *i;
		if (!obj) {
			continue;
		}

		const AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if (!ai) {
			continue;
		}

		//Kris: Optimization
		isIdle = ai->isIdle() || obj->isEffectivelyDead();
		if( !isIdle )
		{
			//Don't bother continuing if even one of our members is not idle.
			return false;
		}
	}

	return isIdle;
}

//------------------------------------------------------------------------------------------------------------
//Definition of busy -- when explicitly in the busy state. Moving or attacking is not considered busy!
//------------------------------------------------------------------------------------------------------------
Bool AIGroup::isBusy() const
{
	Bool isBusy = true;
	std::list<Object *>::const_iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *obj = *i;
		if( !obj )
		{
			continue;
		}

		const AIUpdateInterface *ai = obj->getAIUpdateInterface();
		if( !ai )
		{
			continue;
		}


		//Kris: Optimization
		isBusy = ai->isBusy() && !obj->isEffectivelyDead();
		if( !isBusy )
		{
			//Don't bother continuing if even one of our members is not busy.
			return false;
		}
	}

	return isBusy;
}

//------------------------------------------------------------------------------------------------------------
// return true iff all group members are dead
//------------------------------------------------------------------------------------------------------------
Bool AIGroup::isGroupAiDead() const
{
	Bool isDead = true;
	std::list<Object *>::const_iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *obj = *i;
		if (!obj) {
			continue;
		}

		isDead = (isDead && obj->isEffectivelyDead());
	}

	return isDead;
}

// Returns an object that can perform the special power. Useful for making queries on the Action Manager
//------------------------------------------------------------------------------------------------------------
Object *AIGroup::getSpecialPowerSourceObject( UnsignedInt specialPowerID )
{
	std::list<Object *>::iterator i;
	const SpecialPowerTemplate *spTemplate = TheSpecialPowerStore->findSpecialPowerTemplateByID( specialPowerID );
	if( spTemplate )
	{
		for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
		{
			Object *object = (*i);
			SpecialPowerModuleInterface *mod = object->getSpecialPowerModule( spTemplate );
			if( mod )
				return object;
		}
	}
	return nullptr;
}

// Returns an object that has a command button for the GUI command type.
//------------------------------------------------------------------------------------------------------------
Object *AIGroup::getCommandButtonSourceObject( GUICommandType type )
{
	std::list<Object *>::iterator it;

	for( it = m_memberList.begin(); it != m_memberList.end(); ++it )
	{
		Object *object = (*it);
		if (!object) {
			continue;
		}

		const CommandSet *commandSet = TheControlBar->findCommandSet( object->getCommandSetString() );
		if (!commandSet) {
			continue;
		}

		const CommandButton *commandButton;
		for(Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
		{
			commandButton = commandSet->getCommandButton(i);
			if(commandButton && (commandButton->getCommandType() == type)) {
				return object;
			}
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------------------------------------
void AIGroup::groupSetEmoticon( const AsciiString &name, Int duration )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *object = (*i);
		Drawable *draw = object->getDrawable();
		if( draw )
		{
			draw->setEmoticon( name, duration );
		}
	}
}

//-----------------------------------------------------------------------------
void AIGroup::groupOverrideSpecialPowerDestination( SpecialPowerType spType, const Coord3D *loc, CommandSourceType cmdSource )
{
	std::list<Object *>::iterator i;
	for( i = m_memberList.begin(); i != m_memberList.end(); ++i )
	{
		Object *object = (*i);
		if( object )
		{
			SpecialPowerUpdateInterface *spuInterface = object->findSpecialPowerWithOverridableDestinationActive( spType );
			if( spuInterface )
			{
				spuInterface->setSpecialPowerOverridableDestination( loc );
			}
		}
	}
}

//-----------------------------------------------------------------------------
void AIGroup::crc( Xfer *xfer )
{
	ObjectID id = INVALID_ID;
	for (std::list<Object *>::iterator it = m_memberList.begin(); it != m_memberList.end(); ++it)
	{
		if (*it)
			id = (*it)->getID();
		xfer->xferUser(&id, sizeof(ObjectID));
		CRCGEN_LOG(("CRC after AI AIGroup m_memberList for frame %d is 0x%8.8X", TheGameLogic->getFrame(), ((XferCRC *)xfer)->getCRC()));
	}

	xfer->xferUnsignedInt( &m_memberListSize );
	CRCGEN_LOG(("CRC after AI AIGroup m_memberListSize for frame %d is 0x%8.8X", TheGameLogic->getFrame(), ((XferCRC *)xfer)->getCRC()));

	id = INVALID_ID;	// Used to be leader id, unused now. jba.
	xfer->xferObjectID( &id );
	CRCGEN_LOG(("CRC after AI AIGroup m_leader for frame %d is 0x%8.8X", TheGameLogic->getFrame(), ((XferCRC *)xfer)->getCRC()));
	xfer->xferReal( &m_speed );
	CRCGEN_LOG(("CRC after AI AIGroup m_speed for frame %d is 0x%8.8X", TheGameLogic->getFrame(), ((XferCRC *)xfer)->getCRC()));
	xfer->xferBool( &m_dirty );
	CRCGEN_LOG(("CRC after AI AIGroup m_dirty for frame %d is 0x%8.8X", TheGameLogic->getFrame(), ((XferCRC *)xfer)->getCRC()));

	xfer->xferUnsignedInt( &m_id );
	CRCGEN_LOG(("CRC after AI AIGroup m_id (%d) for frame %d is 0x%8.8X", m_id, TheGameLogic->getFrame(), ((XferCRC *)xfer)->getCRC()));

}

//-----------------------------------------------------------------------------
void AIGroup::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

}

//-----------------------------------------------------------------------------
void AIGroup::loadPostProcess()
{

}
