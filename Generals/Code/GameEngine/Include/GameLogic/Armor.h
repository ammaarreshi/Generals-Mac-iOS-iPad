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

// FILE: Armor.h /////////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, March 2002
// Desc:   Damage Multiplier Descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/NameKeyGenerator.h"
#include "Common/STLTypedefs.h"
#include "GameLogic/Damage.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class ArmorStore;

//-------------------------------------------------------------------------------------------------
/**
	An Armor encapsulates the a particular type of actual modifier to damage taken, in order
	to simulate different materials, and to help make game balance easier to adjust.
*/
//-------------------------------------------------------------------------------------------------
class ArmorTemplate
{
public:

	ArmorTemplate();

	void clear();

	/**
		This is the real "meat" of the class: given a damage type and amount, adjust the damage
		and return the amount that should be dealt.
	*/
	Real adjustDamage(DamageType t, Real damage) const;

	static void parseArmorCoefficients( INI* ini, void *instance, void* /* store */, const void* userData );

protected:

private:
	Real						m_damageCoefficient[DAMAGE_NUM_TYPES];	///< modifiers to damage
};

//-------------------------------------------------------------------------------------------------
class Armor
{
public:

	inline Armor(const ArmorTemplate* tmpl = nullptr) : m_template(tmpl)
	{
	}

	inline Real adjustDamage(DamageType t, Real damage) const
	{
		return m_template ? m_template->adjustDamage(t, damage) : damage;
	}

	inline void clear()
	{
		m_template = nullptr;
	}

private:
	const ArmorTemplate* m_template;		///< the kind of armor this is
};

//------------------------------------------------------------------------------------------------
/** Interface class for TheArmorStore, which is just a convenient place for us to
	* store each Armor we read from INI together in a list, with some access
	* methods for finding Armors */
//-------------------------------------------------------------------------------------------------
class ArmorStore : public SubsystemInterface
{

public:

	ArmorStore();
	virtual ~ArmorStore() override;

	virtual void init() override { }
	virtual void reset() override { }
	virtual void update() override { }

	const ArmorTemplate* findArmorTemplate(NameKeyType namekey) const;
	/**
		Find the Armor with the given name. If no such Armor exists, return null.
	*/
	const ArmorTemplate* findArmorTemplate(const AsciiString& name) const;
	const ArmorTemplate* findArmorTemplate(const char* name) const;

	inline Armor makeArmor(const ArmorTemplate *tmpl) const
	{
		return Armor(tmpl);	// my, that was easy
	}

	static void parseArmorDefinition(INI* ini);

private:

	typedef std::hash_map< NameKeyType, ArmorTemplate, rts::hash<NameKeyType>, rts::equal_to<NameKeyType>/**/> ArmorTemplateMap;
	ArmorTemplateMap m_armorTemplates;

};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern ArmorStore *TheArmorStore;
