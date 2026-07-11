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

// FILE: AssistedTargetingUpdate.h /////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, September 2002
// Desc:   Outside influences can tell me to attack something out of my normal targeting range
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class AssistedTargetingUpdateModuleData : public UpdateModuleData
{
public:
	Int m_clipSize;
	WeaponSlotType m_weaponSlot;

	AsciiString m_laserFromAssistedName;
	AsciiString m_laserToTargetName;

	AssistedTargetingUpdateModuleData()
	{
		m_clipSize = 1;
		m_weaponSlot = PRIMARY_WEAPON;
	}

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class AssistedTargetingUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AssistedTargetingUpdate, "AssistedTargetingUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AssistedTargetingUpdate, AssistedTargetingUpdateModuleData )

public:

	AssistedTargetingUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	Bool isFreeToAssist() const;
	void assistAttack( const Object *requestingObject, Object *victimObject );

private:
	void makeFeedbackLaser( const ThingTemplate *laserTemplate, const Object *from, const Object *to );

	const ThingTemplate *m_laserFromAssisted;
	const ThingTemplate *m_laserToTarget;
};
