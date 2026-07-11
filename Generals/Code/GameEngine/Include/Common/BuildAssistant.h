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

// FILE: BuildAssistant.h /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, February 2002
// Desc:   Singleton class to encapsulate some of the more common functions or rules
//				 that apply to building structures and units
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "Common/STLTypedefs.h"
#include "Lib/BaseType.h"
#include "Common/SubsystemInterface.h"
#include "GameLogic/Object.h"

// FORWARD DECLARATIONS ///////////////////////////////////////////////////////////////////////////
class ThingTemplate;
class Player;
class Object;

// ------------------------------------------------------------------------------------------------
// this enum is used for the construction percent of objects
enum { CONSTRUCTION_COMPLETE = -1 };
typedef void (*IterateFootprintFunc)( const Coord3D *samplePoint, void *userData );

// ------------------------------------------------------------------------------------------------
class ObjectSellInfo : public MemoryPoolObject
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ObjectSellInfo, "ObjectSellInfo" )

public:

	ObjectSellInfo();
	// virtual destructor prototypes provided by memory pool object

	ObjectID m_id;									///< id of object to sell
	UnsignedInt m_sellFrame;				///< frame the sell occurred on

};

// ------------------------------------------------------------------------------------------------
typedef std::list<ObjectSellInfo *> ObjectSellList;
typedef ObjectSellList::iterator ObjectSellListIterator;

//-------------------------------------------------------------------------------------------------
/** Return codes for queries about being able to build */
//-------------------------------------------------------------------------------------------------
enum CanMakeType CPP_11(: Int)
{
	CANMAKE_OK,
	CANMAKE_NO_PREREQ,
	CANMAKE_NO_MONEY,
	CANMAKE_FACTORY_IS_DISABLED,
	CANMAKE_QUEUE_FULL,						// production bldg has full production queue
	CANMAKE_PARKING_PLACES_FULL,	// production bldg has finite slots for existing units
	CANMAKE_MAXED_OUT_FOR_PLAYER	// player has as many as they are allowed at once (eg, Black Lotus
};

//-------------------------------------------------------------------------------------------------
/** Return codes for queries about legal build locations */
//-------------------------------------------------------------------------------------------------
enum LegalBuildCode CPP_11(: Int)
{
	LBC_OK = 0,
	LBC_RESTRICTED_TERRAIN,
	LBC_NOT_FLAT_ENOUGH,
	LBC_OBJECTS_IN_THE_WAY,
	LBC_NO_CLEAR_PATH,
	LBC_SHROUD,
	LBC_TOO_CLOSE_TO_SUPPLIES,
	LBC_GENERIC_FAILURE,
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class BuildAssistant : public SubsystemInterface
{

public:

	struct TileBuildInfo
	{
		Int tilesUsed;
		Coord3D *positions;
	};

	enum LocalLegalToBuildOptions
	{
		TERRAIN_RESTRICTIONS		= 0x00000001,	///< Check for basic terrain restrictions
		CLEAR_PATH							= 0x00000002,	///< Must be able to path find to location
		NO_OBJECT_OVERLAP				= 0X00000004,	///< Can't overlap enemy objects, or locally controlled objects that can't move out of the way
		USE_QUICK_PATHFIND			= 0x00000008, ///< Use the quick pathfind method for CLEAR_PATH
		SHROUD_REVEALED					= 0x00000010,	///< Check to make sure the shroud is revealed
		NO_ENEMY_OBJECT_OVERLAP	= 0x00000020,	///< Can't overlap enemy objects only.
		IGNORE_STEALTHED				= 0x00000040, ///< Units that we can't see are legal to "build" on. (when moving mouse around)
		FAIL_STEALTHED_WITHOUT_FEEDBACK = 0x00000080 ///< USE WITH IGNORE_STEALTHED except it will fail without BIB feedback (when clicking to place).
	};

public:

	BuildAssistant();
	virtual ~BuildAssistant() override;

	virtual void init() override;					///< for subsytem
	virtual void reset() override;					///< for subsytem
	virtual void update() override;				///< for subsytem

	/// iterate the "footprint" area of a structure at the given "sample resolution"
	void iterateFootprint( const ThingTemplate *build,
												 Real buildOrientation,
												 const Coord3D *worldPos,
												 Real sampleResolution,
												 IterateFootprintFunc func,
												 void *funcUserData );

	/// create object from a build and put it in the world now
	virtual Object *buildObjectNow( Object *constructorObject, const ThingTemplate *what,
																	const Coord3D *pos, Real angle, Player *owningPlayer );

	/// using the "line placement" for objects (like walls etc) create that line of objects line
	virtual void buildObjectLineNow( Object *constructorObject, const ThingTemplate *what,
																	 const Coord3D *start, const Coord3D *end, Real angle,
																	 Player *owningPlayer );

	/// query if we can build at this location
	virtual LegalBuildCode isLocationLegalToBuild( const Coord3D *worldPos,
																								 const ThingTemplate *build,
																								 Real angle,  // angle to construct 'build' at
																								 UnsignedInt options,		// use LocationLegalToBuildOptions
																								 const Object *builderObject,
																								 Player *player);

	/// query if we can build at this location
	virtual Bool isLocationClearOfObjects( const Coord3D *worldPos,
																								 const ThingTemplate *build,
																								 Real angle,  // angle to construct 'build' a
																								 const Object *builderObject,
																								 UnsignedInt options,
																								 Player *thePlayer);

	/// Adds bib highlighting for this location.
	virtual void addBibs( const Coord3D *worldPos,
																	const ThingTemplate *build );

	/// tiling wall object helper function, we can use this to "tile" walls when building
	virtual TileBuildInfo *buildTiledLocations( const ThingTemplate *thingBeingTiled,
																							Real angle, // angle to construct thing being tiled
																						  const Coord3D *start, const Coord3D *end,
																						  Real tilingSize, Int maxTiles,
																							Object *builderObject );

	/// return the "scratch pad" array that can be used to create a line of build locations
	virtual Coord3D *getBuildLocations() { return m_buildPositions; }

	/// is the template a line build object, like a wall
	virtual Bool isLineBuildTemplate( const ThingTemplate *tTemplate );

	/// are all the requirements for making this unit satisfied
	virtual CanMakeType canMakeUnit( Object *builder, const ThingTemplate *whatToBuild ) const;

	/// are all the requirements for making this unit (except available cash) are satisfied
	virtual Bool isPossibleToMakeUnit( Object *builder, const ThingTemplate *whatToBuild ) const;

	/// sell an object
	virtual void sellObject( Object *obj );

	void xferTheSellList(Xfer *xfer );

protected:

	/// some objects will be "cleared" automatically when constructing
	Bool isRemovableForConstruction( Object *obj );

	/// clear the area of removable objects for construction
	void clearRemovableForConstruction( const ThingTemplate *whatToBuild,
																			const Coord3D *pos, Real angle );

	/// will move objects that can move out of the way.
	/// will return false if there are objects that cannot be moved out of the way.
	Bool moveObjectsForConstruction( const ThingTemplate *whatToBuild,
																	 const Coord3D *pos, Real angle, Player *playerToBuild );

	Coord3D *m_buildPositions;			///< array used to create a line of build locations (think walls)
	Int m_buildPositionSize;				///< number of elements in the build position array
	ObjectSellList m_sellList;			///< list of objects currently going through the "sell process"

};

// EXTERN /////////////////////////////////////////////////////////////////////////////////////////
extern BuildAssistant *TheBuildAssistant;
