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

// FILE: ModuleFactory.h //////////////////////////////////////////////////////////////////////////
// Author: Colin Day, September 2001
// Desc:	 TheModuleFactory is where we actually instance modules for objects
//				 and drawables.  Those modules are things such as an UpdateModule
//			   or DamageModule or DrawModule etc.
//
//				 TheModuleFactory will contain a list of ModuleTemplates, when we
//				 request a new module, we will look for that template in our
//				 list and create it
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>

#include "Common/GameMemory.h"
#include "Common/SubsystemInterface.h"
#include "Common/AsciiString.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Module.h"
#include "Common/STLTypedefs.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;
class Object;
class Drawable;
class INI;

// TYPE DEFINITIONS ///////////////////////////////////////////////////////////////////////////////
typedef Module *(*NewModuleProc)(Thing *thing, const ModuleData* moduleData);
typedef ModuleData* (*NewModuleDataProc)(INI* ini);

//-------------------------------------------------------------------------------------------------
/** We use TheModuleFactory to register classes that will be attached
	* to objects and drawables which will be executed or "called back" in the
	* correct situations ... such as Die, Damage, Update etc or just as
	* a place to store data specific to that type of thing */
//-------------------------------------------------------------------------------------------------
class ModuleFactory : public SubsystemInterface, public Snapshot
{

public:

	ModuleFactory();
	virtual ~ModuleFactory() override;

	virtual void init() override;
	virtual void reset() override { }					///< We don't reset during the lifetime of the app
	virtual void update() override { }					///< As of now, we don't have a need for an update

	Module *newModule( Thing *thing, const AsciiString& name, const ModuleData* data, ModuleType type );  ///< allocate a new module

	// module-data
	ModuleData* newModuleDataFromINI(INI* ini, const AsciiString& name, ModuleType type, const AsciiString& moduleTag);

	Int findModuleInterfaceMask(const AsciiString& name, ModuleType type);

	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

protected:


	class ModuleTemplate
	{
	public:
		ModuleTemplate() : m_createProc(nullptr), m_createDataProc(nullptr), m_whichInterfaces(0)
		{
		}

		NewModuleProc m_createProc;					///< creation method
		NewModuleDataProc m_createDataProc;	///< creation method
		Int m_whichInterfaces;
	};

	const ModuleTemplate* findModuleTemplate(const AsciiString& name, ModuleType type);

	/// adding a new module template to the factory, and assisting macro to make it easier
	void addModuleInternal( NewModuleProc proc, NewModuleDataProc dataproc, ModuleType type, const AsciiString& name, Int whichIntf );
	#define addModule( classname )											\
		addModuleInternal( classname::friend_newModuleInstance,  \
											 classname::friend_newModuleData,			\
											 classname::getModuleType(),		\
											 AsciiString( #classname ),			\
											 classname::getInterfaceMask())

	static NameKeyType makeDecoratedNameKey(const AsciiString& name, ModuleType type);

	typedef std::map< NameKeyType, ModuleTemplate, std::less<NameKeyType>/**/> ModuleTemplateMap;
	typedef std::vector<const ModuleData*> ModuleDataList;

	ModuleTemplateMap			m_moduleTemplateMap;
	ModuleDataList				m_moduleDataList;

};

// EXTERN /////////////////////////////////////////////////////////////////////////////////////////
extern ModuleFactory *TheModuleFactory;  ///< singleton definition
