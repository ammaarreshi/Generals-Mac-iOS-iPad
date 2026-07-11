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

// FILE: TerrainLogic.h ///////////////////////////////////////////////////////////////////////////
// Logical terrain representation for the game logic side
// Author: Colin Day, April 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameMemory.h"
#include "Common/Snapshot.h"
#include "Common/STLTypedefs.h"
#include "GameClient/TerrainRoads.h"

typedef std::vector<ICoord2D> VecICoord2D;

class DataChunkInput;
struct DataChunkInfo;
class MapObject;
class Object;
class Dict;
class PolygonTrigger;
class ThingTemplate;
class Vector3;
class Drawable;
class Matrix3D;
class WaterHandle;
class Xfer;

enum WaypointID CPP_11(: Int)
{
	INVALID_WAYPOINT_ID = 0x7FFFFFFF
};

//-------------------------------------------------------------------------------------------------
// Waypoint
/** Helper class for waypoint info in terrain logic.
*/
class Waypoint : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(Waypoint, "Waypoint")

// friends do not play well with MPO (srj)
//friend class TerrainLogic;
public:
	Waypoint(WaypointID id, AsciiString name, const Coord3D *pLoc, AsciiString label1,
						AsciiString label2, AsciiString label3, Bool biDirectional);
	//~Waypoint();
	enum {MAX_LINKS=8};

protected:
	WaypointID	m_id;								///< Unique integer identifier.
	AsciiString m_name;							///< Name.
	Coord3D			m_location;					///< Location.
	Waypoint*		m_pNext;						///< Linked list of all waypoints.
	Waypoint*		m_links[MAX_LINKS]; ///< Directed graph of waypoints.
	Int					m_numLinks;					///< Number of links in m_links.
	AsciiString m_pathLabel1;
	AsciiString m_pathLabel2;
	AsciiString m_pathLabel3;
	Bool				m_biDirectional;

public:
	// should be protected, but friendly access needed (srj)
	void setNext(Waypoint *pNext) {m_pNext = pNext; }
	//void setLink(Int ndx, Waypoint *pLink)
	//{
	//	if (ndx>=0 && ndx <=MAX_LINKS) m_links[ndx] = pLink;
	//}
	void addLink(Waypoint* pLink)
	{
		if (m_numLinks < MAX_LINKS)
		{
			m_links[m_numLinks] = pLink;
			++m_numLinks;
		}
	}

public:
	/// Enumerate all waypoints using getNext.
	Waypoint *getNext() const {return m_pNext; }
	/// Enumerate the directed links from a waypoint using this,a nd getLink.
	Int getNumLinks() const {return m_numLinks; }
	/// Get the n'th directed link.  (May be nullptr).
	Waypoint *getLink(Int ndx) const {if (ndx>=0 && ndx <= MAX_LINKS) return m_links[ndx]; return nullptr; }
	/// Get the waypoint's name.
	AsciiString getName() const {return m_name; }
	/// Get the integer id.
	WaypointID getID() const {return m_id; }
	/// Get the waypoint's position
	const Coord3D *getLocation() const { return &m_location;  }
	/// Get the waypoint's first path label
	const AsciiString& getPathLabel1() const { return m_pathLabel1;  }
	/// Get the waypoint's second path label
	const AsciiString& getPathLabel2() const { return m_pathLabel2;  }
	/// Get the waypoint's third path label
	const AsciiString& getPathLabel3() const { return m_pathLabel3;  }
	/// Get bi-directionality.
	Bool getBiDirectional() const { return m_biDirectional; }

	void setLocationZ(Real z) { m_location.z = z; }
};

//-------------------------------------------------------------------------------------------------
// Bridge
/** Helper class for bridge info in terrain logic.
*/
class BridgeInfo
{
public:
	BridgeInfo();

public:
	Coord3D					from, to; /// The points that the bridge was drawn using.
	Real						bridgeWidth; /// Width of the bridge.
	Coord3D					fromLeft, fromRight, toLeft, toRight; /// The 4 corners of the rectangle that the bridge covers.
	Int							bridgeIndex;	///< The index to the drawable bridges.
	BodyDamageType	curDamageState;
	ObjectID				bridgeObjectID;
	ObjectID				towerObjectID[ BRIDGE_MAX_TOWERS ];
	Bool						damageStateChanged;

};

//-------------------------------------------------------------------------------------------------
// Bridge
/** Helper class for bridge info in terrain logic.
*/
struct TBridgeAttackInfo
{
public:
	Coord3D attackPoint1, attackPoint2; /// The points that can be attacked..
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class Bridge : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(Bridge, "Bridge")
// friends do not play well with MPO (srj)
//friend class TerrainLogic;
public:

public: // ctor/dtor.
	Bridge(BridgeInfo &theInfo, Dict *props, AsciiString bridgeTemplateName);
	Bridge(Object *bridgeObj);
	//~Bridge();

protected:
	Bridge*						m_next;		///< Link for traversing all bridges in the current map.
	AsciiString				m_templateName;			///< bridge template name
	BridgeInfo				m_bridgeInfo;
	Region2D					m_bounds; /// 2d bounds for quick screening.
	PathfindLayerEnum	m_layer;  ///< Pathfind layer for this bridge.

public:
	// should be protected, but friendly access needed (srj)
	void setNext(Bridge *pNext) {m_next = pNext; }
	Object *createTower( Coord3D *worldPos, BridgeTowerType towerPos,
											 const ThingTemplate *towerTemplate, Object *bridge );

public:
	/// return the bridge template name
	AsciiString getBridgeTemplateName() { return m_templateName; }
	/// Enumerate all bridges using getNext;
	Bridge	*getNext() {return m_next; }
	/// Get the height for an object on bridge.  Note - assumes object is on bridge. Use isPointOnBridge to check.
	Real getBridgeHeight(const Coord3D *pLoc, Coord3D* normal);
	/// Get the bridges logical info.
	void getBridgeInfo(class BridgeInfo *pInfo) {*pInfo = m_bridgeInfo; }
	/// See if the point is on the bridge.
	Bool isPointOnBridge(const Coord3D *pLoc);
	Drawable *pickBridge(const Vector3 &from, const Vector3 &to, Vector3 *pos);
	void updateDamageState(); ///< Updates a bridge's damage info.
	const BridgeInfo *peekBridgeInfo() const {return &m_bridgeInfo;}
	PathfindLayerEnum getLayer() const {return m_layer;}
	void setLayer(PathfindLayerEnum layer) {m_layer = layer;}
	const Region2D *getBounds() const {return &m_bounds;}
	Bool isCellOnEnd(const Region2D *cell);	 // Is pathfind cell on the sides of the bridge
	Bool isCellOnSide(const Region2D *cell); // Is pathfind cell on the end of the bridge
	Bool isCellEntryPoint(const Region2D *cell); // Is pathfind cell an entry point to the bridge

	void setBridgeObjectID( ObjectID id ) { m_bridgeInfo.bridgeObjectID = id; }
	void setTowerObjectID( ObjectID id, BridgeTowerType which ) { m_bridgeInfo.towerObjectID[ which ] = id; }

};

//-------------------------------------------------------------------------------------------------
/** Device independent implementation for some functionality of the
  * logical terrain singleton */
//-------------------------------------------------------------------------------------------------
class TerrainLogic : public Snapshot,
										 public SubsystemInterface
{

public:

	TerrainLogic();
	virtual ~TerrainLogic() override;

	virtual void init() override;		///< Init
	virtual void reset() override;		///< Reset
	virtual void update() override;	///< Update

	virtual Bool loadMap( AsciiString filename, Bool query );
	virtual void newMap( Bool saveGame );	///< Initialize the logic for new map.

	virtual Real getGroundHeight( Real x, Real y, Coord3D* normal = nullptr )  const;
	virtual Real getLayerHeight(Real x, Real y, PathfindLayerEnum layer, Coord3D* normal = nullptr, Bool clip = true) const;
	virtual void getExtent( Region3D *extent ) const { DEBUG_CRASH(("not implemented"));  }		///< @todo This should not be a stub - this should own this functionality
	virtual void getExtentIncludingBorder( Region3D *extent ) const { DEBUG_CRASH(("not implemented"));  }		///< @todo This should not be a stub - this should own this functionality
	virtual void getMaximumPathfindExtent( Region3D *extent ) const { DEBUG_CRASH(("not implemented"));  }		///< @todo This should not be a stub - this should own this functionality
	virtual Coord3D findClosestEdgePoint( const Coord3D *closestTo ) const ;
	virtual Coord3D findFarthestEdgePoint( const Coord3D *farthestFrom ) const ;
	virtual Bool isClearLineOfSight(const Coord3D& pos, const Coord3D& posOther) const;

	virtual AsciiString getSourceFilename() { return m_filenameString; }

	virtual PathfindLayerEnum alignOnTerrain( Real angle, const Coord3D& pos, Bool stickToGround, Matrix3D& mtx);

	virtual Bool isUnderwater( Real x, Real y, Real *waterZ = nullptr, Real *terrainZ = nullptr );			///< is point under water
	virtual Bool isCliffCell( Real x, Real y) const;			///< is point cliff cell
	virtual const WaterHandle* getWaterHandle( Real x, Real y );					///< get water handle at this location
	virtual const WaterHandle* getWaterHandleByName( AsciiString name );	///< get water handle by name
	virtual Real getWaterHeight( const WaterHandle *water );							///< get height of water table
	virtual void setWaterHeight( const WaterHandle *water,
															 Real height,
															 Real damageAmount,
															 Bool forcePathfindUpdate );	///< set height of water table
	virtual void changeWaterHeightOverTime( const WaterHandle *water,
																					Real finalHeight,
																					Real transitionTimeInSeconds,
																					Real damageAmount );///< change water height over time

	virtual Waypoint *getFirstWaypoint() { return m_waypointListHead; }

	/// Return the waypoint with the given name
	virtual Waypoint *getWaypointByName( AsciiString name );

	/// Return the waypoint with the given ID
	virtual Waypoint *getWaypointByID( UnsignedInt id );

	/// Return the closest waypoint on the labeled path
	virtual Waypoint *getClosestWaypointOnPath( const Coord3D *pos, AsciiString label );

	/// Return true if the waypoint path containing pWay is labeled with the label.
	virtual Bool isPurposeOfPath( Waypoint *pWay, AsciiString label );

	/// Return the trigger area with the given name
	virtual PolygonTrigger *getTriggerAreaByName( AsciiString name );

	///Gets the first bridge.  Traverse all bridges using bridge->getNext();
	virtual Bridge *getFirstBridge() const { return m_bridgeListHead; }

	/// Find the bridge at a location.  null means no bridge.
	virtual Bridge *findBridgeAt(const Coord3D *pLoc) const;

	/// Find the bridge at a location.  null means no bridge. Note that the layer value will be used to resolve crossing bridges.
	virtual Bridge *findBridgeLayerAt(const Coord3D *pLoc, PathfindLayerEnum layer, Bool clip = true) const;

	///  Returns true if the object is close enough to interact with the bridge for pathfinding.
	virtual Bool objectInteractsWithBridgeLayer(Object *obj, Int layer, Bool considerBridgeHealth = true) const;

	///  Returns true if the object is close to one or the other end of the bridge.
	virtual Bool objectInteractsWithBridgeEnd(Object *obj, Int layer) const;

	virtual Drawable *pickBridge(const Vector3 &from, const Vector3 &to, Vector3 *pos);

	virtual void addBridgeToLogic(BridgeInfo *pInfo, Dict *props, AsciiString bridgeTemplateName); ///< Adds a bridge's logical info.
	virtual void addLandmarkBridgeToLogic(Object *bridgeObj); ///< Adds a bridge's logical info.
	virtual void deleteBridge( Bridge *bridge );	///< remove a bridge

	virtual void updateBridgeDamageStates(); ///< Updates bridge's damage info.

	Bool anyBridgesDamageStatesChanged() {return m_bridgeDamageStatesChanged; } ///< Bridge damage states updated.
	Bool isBridgeRepaired(const Object *bridge); ///< Is bridge repaired?
	Bool isBridgeBroken(const Object *bridge); ///< Is bridge Broken?
	void getBridgeAttackPoints(const Object *bridge, TBridgeAttackInfo *info); ///< Get bridge attack points.

	PathfindLayerEnum getLayerForDestination(const Coord3D *pos);

	// this is just like getLayerForDestination, but always return the highest layer that will be <= z at that point
	// (unlike getLayerForDestination, which will return the closest layer)
	PathfindLayerEnum getHighestLayerForDestination(const Coord3D *pos, Bool onlyHealthyBridges = false);

	void enableWaterGrid( Bool enable );			///< enable/disable the water grid

	// This is stuff to get the currently active boundary information
	Int getActiveBoundary() { return m_activeBoundary; }
	void setActiveBoundary(Int newActiveBoundary);

  void flattenTerrain(Object *obj);  ///< Flatten the terrain under a building.
  void createCraterInTerrain(Object *obj);  ///< Flatten the terrain under a building.

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

	/// Chunk parser callback.
 	static Bool parseWaypointDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	/// Chunk parser callback.
	Bool parseWaypointData(DataChunkInput &file, DataChunkInfo *info, void *userData);
	/// Add a waypoint to the list.
	void addWaypoint(MapObject *pMapObj);
	/// Add a directed link between waypoints.
	void addWaypointLink(Int id1, Int id2);
	/// Deletes all waypoints.
	void deleteWaypoints();
	/// Deletes all bridges.
	void deleteBridges();

	/// find the axis aligned region bounding the water table
	void findAxisAlignedBoundingRect( const WaterHandle *waterHandle, Region3D *region );

	UnsignedByte	*m_mapData;									///< array of height samples
	Int	m_mapDX;															///< width of map samples
	Int	m_mapDY;															///< height of map samples

	VecICoord2D m_boundaries;
	Int m_activeBoundary;

	Waypoint *m_waypointListHead;
	Bridge *m_bridgeListHead;

	Bool		m_bridgeDamageStatesChanged;

	AsciiString m_filenameString;  ///< filename for terrain data

	Bool m_waterGridEnabled;			 ///< TRUE when water grid is enabled

	static WaterHandle m_gridWaterHandle;		///< water handle for the grid water (we only presently have one)

	//
	// we will force a limit of MAX_DYNAMIC_WATER as the max dynamically changeable water
	// tables for a map.  We could use a list, but eh, this is fine and small anyway
	//
	enum { MAX_DYNAMIC_WATER = 64 };
	struct DynamicWaterEntry
	{
		const WaterHandle *waterTable;	///< handle to water table to edit
		Real changePerFrame;						///< how much height to add to the water each frame (negative=lowering)
		Real targetHeight;							///< the target height we want to be at
		Real damageAmount;							///< amount of damage to do to objects that are underwater
		Real currentHeight;							///< we need to keep track of this ourselves cause some water height are represented with ints
	} m_waterToUpdate[ MAX_DYNAMIC_WATER ];  ///< water tables to dynamicall update
	Int m_numWaterToUpdate;						///< how many valid entries are in m_waterToUpdate

};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern TerrainLogic *TheTerrainLogic;   ///< singleton definition

extern void makeAlignToNormalMatrix( Real angle, const Coord3D& pos, const Coord3D& normal, Matrix3D& mtx);
extern Bool LineInRegion( const Coord2D *p1, const Coord2D *p2, const Region2D *clipRegion );
