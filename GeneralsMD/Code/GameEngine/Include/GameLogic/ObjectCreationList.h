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

// FILE: ObjectCreationList.h /////////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, December 2001
// Desc:   General Effects Descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

// Kris: August 23, 2003
	// All OCLs return the first object that is created (or null if not applicable).

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameMemory.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class ObjectCreationNugget;
class ObjectCreationList;
class ObjectCreationListStore;
class INI;
class Object;

//-------------------------------------------------------------------------------------------------
/**
	An ObjectCreationNugget encapsulates the creation of an Object. ObjectCreationNuggets are virtually
	never used on their own, but rather, as a component of an ObjectCreationList (see below).

	Important notes:

	-- ObjectCreationNugget is an ABC; all the implementations are (currently) located in ObjectCreationList.cpp,
	thought they will probably be spread out more as we add more implementations.

	-- As part of an ObjectCreationList, an ObjectCreationNugget is shared between multiple units. The implication is that
	an ObjectCreationNugget should not require private data storage to do what it needs to do, aside from stuff
	initialized at ObjectCreationNugget instantiation time (eg, parameters from an INI file). To help
	enforce this, all it's methods are declared 'const'. If you can't implement what you
	need within this framework, please *don't* simply de-const things, because it could lead to very
	strange side-effects. Instead, the system will have to be enhanced to allow for multiple instances
	of each ObjectCreationNugget.

	-- an individual ObjectCreationNugget is generally not directly accessible to anyone outside of the
	ObjectCreationList system; in fact, it could probably be a private class, but isn't, mainly for coding convenience.

	-- Unlike most other game systems, ObjectCreationNuggets can't be overridden by subsequent INI file
	loads. This isn't really a problem, because all you really need to do to "override" one is to
	specify a different one.
*/
class ObjectCreationNugget : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_ABC(ObjectCreationNugget)

public:

	ObjectCreationNugget() { }
	//virtual ~ObjectCreationNugget() { }

	/**
		The main guts of the system: actually perform the sound and/or video effects
		needed. Note that primary can be null, so you must check for this.
		Bool useOwner determines whether we are creating the the master object or not (for deliverpayload)
	*/
	virtual Object* create( const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Real angle, UnsignedInt lifetimeFrames = 0 ) const = 0;

	/**
		the object-based version... by default, just call the location-based implementation.
		Note that primary can be null, so you must check for this.
	*/
	virtual Object* create( const Object* primary, const Object* secondary, UnsignedInt lifetimeFrames = 0 ) const;

	/**
		A variation used by DeliverPayload -- the createOwner Bool specifies whether we are creating the transport
		object, or using the existing one.
	*/
	virtual Object* create( const Object* primaryObj, const Coord3D *primary, const Coord3D *secondary, Bool createOwner, UnsignedInt lifetimeFrames = 0 ) const;
};
EMPTY_DTOR(ObjectCreationNugget)

//-------------------------------------------------------------------------------------------------
/**
	An ObjectCreationList is a way of creating a particular set of Objects.

	Important notes:

	-- an ObjectCreationList is specified solely by name, and the only parameters it receives when performing
	its AV effects are a primary (and optional secondary) object position.

	-- There is no inheritance or overriding of ObjectCreationLists; if you need an ObjectCreationList that is nearly-but-not-quite
	identical to an existing one, you must simply make an entirely new ObjectCreationList. Realistically, this shouldn't
	be a problem, since they are pretty simple to specify, and don't consume a lot of memory.

	-- an ObjectCreationList is shared between multiple units. To help
	enforce this, all it's methods are declared 'const'. If you can't implement the stuff you
	need within this framework, please *don't* simply de-const things, because it could lead to very
	strange side-effects. Instead, the system will have to be enhanced to allow for multiple instances
	of each ObjectCreationNugget.

	-- Unlike most other game systems, ObjectCreationList can't be overridden by subsequent INI file
	loads. This isn't really a problem, because all you really need to do to "override" one is to
	specify a different one.
*/
class ObjectCreationList
{

public:

	/**
		Toss the contents.
	*/
	void clear();

	void addObjectCreationNugget(ObjectCreationNugget* nugget);

	// Kris: August 23, 2003
	// All OCLs return the first object that is created (or null if not applicable).
	static Object* create( const ObjectCreationList* ocl, const Object* primaryObj, const Coord3D *primary, const Coord3D *secondary, Bool createOwner, UnsignedInt lifetimeFrames = 0 )
	{
		if( ocl )
			return ocl->createInternal( primaryObj, primary, secondary, createOwner, lifetimeFrames );
		return nullptr;
	}

	// Kris: August 23, 2003
	// All OCLs return the first object that is created (or null if not applicable).
	/// inline convenience method to avoid having to check for null.
	static Object* create(const ObjectCreationList* ocl, const Object* primaryObj, const Coord3D *primary, const Coord3D *secondary, Real angle, UnsignedInt lifetimeFrames = 0 )
	{
		if (ocl)
			return ocl->createInternal( primaryObj, primary, secondary, angle, lifetimeFrames );
		return nullptr;
	}

	// Kris: August 23, 2003
	// All OCLs return the first object that is created (or null if not applicable).
	/// inline convenience method to avoid having to check for null.
	static Object* create( const ObjectCreationList* ocl, const Object* primary, const Object* secondary, UnsignedInt lifetimeFrames = 0 )
	{
		if (ocl)
			return ocl->createInternal( primary, secondary, lifetimeFrames );
		return nullptr;
	}

protected:

private:

	// Kris: August 23, 2003
	// All OCLs return the first object that is created (or null if not applicable).
	Object* createInternal(const Object* primaryObj, const Coord3D *primary, const Coord3D *secondary, Bool createOwner, UnsignedInt lifetimeFrames = 0 ) const;
	Object* createInternal(const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Real angle, UnsignedInt lifetimeFrames = 0 ) const;
	Object* createInternal(const Object* primary, const Object* secondary, UnsignedInt lifetimeFrames = 0 ) const;

	// note, this list doesn't own the nuggets; all nuggets are owned by the Store.
	typedef std::vector<ObjectCreationNugget*> ObjectCreationNuggetVector;
	ObjectCreationNuggetVector m_nuggets;

};

//-------------------------------------------------------------------------------------------------
/**
	The "store" used to hold all the ObjectCreationLists in existence.
*/
class ObjectCreationListStore : public SubsystemInterface
{

public:

	ObjectCreationListStore();
	virtual ~ObjectCreationListStore() override;

	virtual void init() override { }
	virtual void reset() override { }
	virtual void update() override { }

	/**
		return the ObjectCreationList with the given namekey.
		return nullptr if no such ObjectCreationList exists.
	*/
	const ObjectCreationList *findObjectCreationList(const char* name) const;

	static void parseObjectCreationListDefinition(INI* ini);

	void addObjectCreationNugget(ObjectCreationNugget* nugget);

private:

	typedef std::map< NameKeyType, ObjectCreationList, std::less<NameKeyType>/**/> ObjectCreationListMap;
	ObjectCreationListMap m_ocls;

	// note, this list doesn't own the nuggets; all nuggets are owned by the Store.
	typedef std::vector<ObjectCreationNugget*> ObjectCreationNuggetVector;
	ObjectCreationNuggetVector m_nuggets;

};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern ObjectCreationListStore *TheObjectCreationListStore;
