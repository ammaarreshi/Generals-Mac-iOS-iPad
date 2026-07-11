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

// FILE: DamageFX.h /////////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, November 2001
// Desc:   Damage Effects Descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameCommon.h"
#include "Common/NameKeyGenerator.h"
#include "Common/STLTypedefs.h"
#include "GameLogic/Damage.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class DamageFXStore;
class FXList;
class INI;

//-------------------------------------------------------------------------------------------------
typedef const FXList* ConstFXListPtr;

//-------------------------------------------------------------------------------------------------
/**
	A DamageFX is an object used to describe how an object reacts to taking a particular type
	of damage. (Note that "reacts" here implies only audio-video effects, not logic effects,
	damage modifiers, etc.)

	Conceptually speaking, every unit with a Body module has a DamageFX object. When it receives damage,
	it asks its DamageFX module to produce an appropriate a/v effect, which can vary by type of damage
	(eg, explosion, armor-piercing, flame, etc) and amount ("minor" or "major").

	Notes:

	-- Every particular damage-type within a DamageFX can have a "minor" and/or "major" effect;
	basically, if the damage done exceeds a specified threshold (or if there isn't a "minor"
	effect), the major effect is used.

	-- DamageFX is shared between multiple units; there should generally only be one instance
	of any particular class. The implication is that it should not require private data storage
	to do what it needs to do, aside from stuff initialized at object instantiation time. To help
	enforce this, all it's methods are declared 'const'. If you can't implement the damage you
	need within this framework, please *don't* simply de-const things, because it could lead to very
	strange side-effects. Instead, the system will have to be enhanced to allow for multiple instances
	of each DamageFX.
*/
//-------------------------------------------------------------------------------------------------
class DamageFX
{
public:

	DamageFX();

	void clear();

	/**
		This is the main public access point to the system: when you want to execute the
		a/v fx used for a specific damage type, call this method. (It is OK for source and/or
		victim to be null, but you should pass them when available.)
	*/
	void doDamageFX(DamageType t, Real damageAmount, const Object* source, const Object* victim) const;

	UnsignedInt getDamageFXThrottleTime(DamageType t, const Object* source) const;

	const FieldParse* getFieldParse() const;

private:

	ConstFXListPtr getDamageFXList(DamageType t, Real damageAmount, const Object* source) const;

	static void parseAmount( INI* ini, void *instance, void*, const void* );
	static void parseMajorFXList( INI* ini, void *instance, void*, const void* );
	static void parseMinorFXList( INI* ini, void *instance, void*, const void* );
	static void parseTime( INI* ini, void *instance, void*, const void* );

	/*
		this isn't terribly efficient since this is pretty sparsely populated
		and with lots of redundancy, but since we allocate very few of these,
		it's not worth more effort at this time...
	*/
	struct DFX
	{
		Real									m_amountForMajorFX;				///< if damage done is >= this, use major fx
		ConstFXListPtr				m_majorDamageFXList;			///< fx to make
		ConstFXListPtr				m_minorDamageFXList;			///< fx to make
		UnsignedInt						m_damageFXThrottleTime;

		DFX()
		{
			clear();
		}

		void clear()
		{
			m_amountForMajorFX = 0.0f;
			m_majorDamageFXList = nullptr;
			m_minorDamageFXList = nullptr;
			m_damageFXThrottleTime = 0;
		}
	};

	DFX m_dfx[DAMAGE_NUM_TYPES][LEVEL_COUNT];
};

//-------------------------------------------------------------------------------------------------
/**
	The "store" used to hold all the DamageFXs in existence. This is usually used when creating
	an Object (actually, a Body module), but can be used at any time after that. (It is explicitly
	OK to swap an Object's DamageFX out at any given time.)
*/
//-------------------------------------------------------------------------------------------------
class DamageFXStore : public SubsystemInterface
{

public:

	DamageFXStore();
	virtual ~DamageFXStore() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;

	/**
		Find the DamageFX with the given name. If no such DamageFX exists, return null.
	*/
	const DamageFX *findDamageFX( NameKeyType namekey ) const;
	const DamageFX *findDamageFX( const AsciiString& name ) const;
	const DamageFX *findDamageFX( const char* name ) const;

	static void parseDamageFXDefinition(INI* ini);


private:

	typedef std::hash_map< NameKeyType, DamageFX, rts::hash<NameKeyType>, rts::equal_to<NameKeyType>/**/> DamageFXMap;
	DamageFXMap m_dfxmap;

};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern DamageFXStore *TheDamageFXStore;
