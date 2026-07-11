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

// FILE: ThingFactory.h ///////////////////////////////////////////////////////////////////////////
// Created: Colin Day, April 2001
// Desc:		This is how we go and make our things, we make our things, we make our things!
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Lib/BaseType.h"

#include "Common/STLTypedefs.h"
#include "Common/SubsystemInterface.h"
#include "Common/GameMemory.h"
#include "Common/AsciiString.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"

class ThingTemplate;
class Object;
class Drawable;
class INI;

typedef std::hash_map<AsciiString, ThingTemplate*, rts::hash<AsciiString>, rts::equal_to<AsciiString>/**/> ThingTemplateHashMap;
typedef ThingTemplateHashMap::iterator ThingTemplateHashMapIt;
//-------------------------------------------------------------------------------------------------
/** Implementation of the thing manager interface singleton */
//-------------------------------------------------------------------------------------------------
class ThingFactory : public SubsystemInterface
{
public:

	ThingFactory();
	virtual ~ThingFactory() override;

	// From the subsystem interface =================================================================
	virtual void init() override;
	virtual void postProcessLoad() override;
	virtual void reset() override;
	virtual void update() override;
	//===============================================================================================

	/// create a new template with name 'name' and add to template list
	ThingTemplate *newTemplate( const AsciiString& name );

	// get the first template in our list
	const ThingTemplate *firstTemplate() { return m_firstTemplate; }

	/**
		get a template given template database name. return null if not found.
		note, this is now substantially faster (does a hash-table lookup)
	*/
	const ThingTemplate *findTemplate( const AsciiString& name, Bool check = TRUE ) { return findTemplateInternal( name, check ); }

	/**
		get a template given ID. return null if not found.
		note, this is not particularly fast (does a linear search).
	*/
	const ThingTemplate *findByTemplateID( UnsignedShort id );

	/** request a new object using the given template.
		this will throw an exception on failure; it will never return null.
	*/
	Object *newObject( const ThingTemplate *tmplate, Team *team, ObjectStatusMaskType statusMask = OBJECT_STATUS_MASK_NONE );

	/** request a new drawable using the given template.
		this will throw an exception on failure; it will never return null.
	*/
	Drawable *newDrawable(const ThingTemplate *tmplate, DrawableStatusBits statusBits = DRAWABLE_STATUS_DEFAULT );

	static void parseObjectDefinition( INI* ini, const AsciiString& name, const AsciiString& reskinFrom );

private:

	/// free all template database data
	void freeDatabase();

	void addTemplate( ThingTemplate *thing );		///< add the template to the DB

	/**
		create a new template with name 'name', do *NOT* add to template, but instead
		add as m_override of template 'template'

		Note, this is private, and should always remain so. It should never, ever, ever
		be called outside of ini parsing code, because if you do, you will tempted to make
		changes to the override at runtime, and end up with changes that can't be saved in savegames,
		since ThingTemplates aren't saved (and should not be). (srj)
	*/
	ThingTemplate* newOverride( ThingTemplate *thingTemplate );


	/**
		This now does a search through the hashmap, and returns the associated thing template
		This should speed up load times dramatically, as we've been wasting lots of time in this
		O(N^2) insertion.

		NOTE: this is protected since it returns a NON-CONST template, and
		folks outside of the template system itself shouldn't get access...
	*/
	ThingTemplate *findTemplateInternal( const AsciiString& name, Bool check = TRUE );

	ThingTemplate					*m_firstTemplate;			///< head of linked list
	UnsignedShort					m_nextTemplateID;			///< next available ID for templates

	ThingTemplateHashMap	m_templateHashMap;		///< all thing templates, for fast lookup.

};

// EXTERN /////////////////////////////////////////////////////////////////////////////////////////
extern ThingFactory *TheThingFactory;  ///< the template singleton
