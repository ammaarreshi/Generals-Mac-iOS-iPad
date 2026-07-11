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

// FILE: SpecialPower.h ///////////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2002
// Desc:   Special power templates and the system that holds them
// Edited: Kris Morness -- July 2002 (added BitFlag system)
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "Common/AudioEventRTS.h"
#include "Common/GameMemory.h"
#include "Common/SubsystemInterface.h"
#include "Lib/BaseType.h"
#include "Common/BitFlags.h"
#include "Common/Overridable.h"
#include "Common/Override.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class ObjectCreationList;
class Object;
enum ScienceType CPP_11(: Int);
struct FieldParse;
enum AcademyClassificationType CPP_11(: Int);

// For SpecialPowerType and SpecialPowerMaskType::s_bitNameList. Part of detangling.
#include "Common/SpecialPowerType.h"

// For SpecialPowerMaskType. Part of detangling.
#include "Common/SpecialPowerMaskType.h"

#define MAKE_SPECIALPOWER_MASK(k) SpecialPowerMaskType(SpecialPowerMaskType::kInit, (k))

inline Bool TEST_SPECIALPOWERMASK(const SpecialPowerMaskType& m, SpecialPowerType t)
{
	return m.test(t);
}
inline Bool TEST_SPECIALPOWERMASK_ANY(const SpecialPowerMaskType& m, const SpecialPowerMaskType& mask)
{
	return m.anyIntersectionWith(mask);
}
inline Bool TEST_SPECIALPOWERMASK_MULTI(const SpecialPowerMaskType& m, const SpecialPowerMaskType& mustBeSet, const SpecialPowerMaskType& mustBeClear)
{
	return m.testSetAndClear(mustBeSet, mustBeClear);
}
inline Bool SPECIALPOWERMASK_ANY_SET(const SpecialPowerMaskType& m)
{
	return m.any();
}
inline void CLEAR_SPECIALPOWERMASK(SpecialPowerMaskType& m)
{
	m.clear();
}
inline void SET_SPECIALPOWERMASK( SpecialPowerMaskType& m, SpecialPowerType t, Int val = 1 )
{
	m.set( t, val );
}
inline void SET_ALL_SPECIALPOWERMASK_BITS(SpecialPowerMaskType& m)
{
	m.clear();
	m.flip();
}
inline void FLIP_SPECIALPOWERMASK(SpecialPowerMaskType& m)
{
	m.flip();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class SpecialPowerTemplate : public Overridable
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( SpecialPowerTemplate, "SpecialPowerTemplate" )

public:

  SpecialPowerTemplate();
	// virtual destructor prototype provided by MemoryPoolObject

	static const FieldParse* getFieldParse() { return m_specialPowerFieldParse; }

	void friend_setNameAndID(const AsciiString& name, UnsignedInt id)
	{
		m_name = name;
		m_id = id;
	}

	AsciiString getName() const { return getFO()->m_name; }
	UnsignedInt getID() const { return getFO()->m_id; }
	SpecialPowerType getSpecialPowerType() const { return getFO()->m_type; }
	UnsignedInt getReloadTime() const { return getFO()->m_reloadTime; }
	ScienceType getRequiredScience() const { return getFO()->m_requiredScience; }
	const AudioEventRTS *getInitiateSound() const { return &getFO()->m_initiateSound; }
	const AudioEventRTS *getInitiateAtTargetSound() const { return &getFO()->m_initiateAtLocationSound; }
	Bool hasPublicTimer() const { return getFO()->m_publicTimer; }
	Bool isSharedNSync() const { return getFO()->m_sharedNSync; }
	UnsignedInt getDetectionTime() const { return getFO()->m_detectionTime; }
	UnsignedInt getViewObjectDuration() const { return getFO()->m_viewObjectDuration; }
	Real getViewObjectRange() const { return getFO()->m_viewObjectRange; }
	Real getRadiusCursorRadius() const { return getFO()->m_radiusCursorRadius; }
	Bool isShortcutPower() const { return getFO()->m_shortcutPower; }
	AcademyClassificationType getAcademyClassificationType() const { return m_academyClassificationType; }

private:

	const SpecialPowerTemplate* getFO() const { return (const SpecialPowerTemplate*)friend_getFinalOverride(); }

	AsciiString				m_name;								///< name
	UnsignedInt				m_id;									///< unique identifier
	SpecialPowerType	m_type;								///< enum allowing for fast type checking for ability processing.
	UnsignedInt				m_reloadTime;					///< (frames) after using special power, how long it takes to use again
	ScienceType				m_requiredScience;		///< science required (if any) to actually execute this power
	AudioEventRTS			m_initiateSound;			///< sound to play when initiated
	AudioEventRTS			m_initiateAtLocationSound;		///< sound to play at target location (if any)
	AcademyClassificationType m_academyClassificationType; ///< A value used by the academy to evaluate advice based on what players do.
	UnsignedInt				m_detectionTime;			///< (frames) after using infiltration power (defection, etc.),
																					///< how long it takes for ex comrades to realize it on their own
	UnsignedInt				m_viewObjectDuration;	///< Lifetime of a looking object we slap down so you can watch the effect
	Real							m_viewObjectRange;		///< And how far that object can see.
	Real							m_radiusCursorRadius;	///< size of radius cursor, if any
	Bool							m_publicTimer;				///< display a countdown timer for this special power for all to see
	Bool							m_sharedNSync;				///< If true, this is a special that is shared between all of a player's command centers
	Bool							m_shortcutPower;		///< Is this shortcut power capable of being fired by the side panel?

	static const FieldParse m_specialPowerFieldParse[];		///< the parse table

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class SpecialPowerStore : public SubsystemInterface
{

public:

	SpecialPowerStore();
	virtual ~SpecialPowerStore() override;

	virtual void init() override { };
	virtual void update() override { };
	virtual void reset() override;

	const SpecialPowerTemplate *findSpecialPowerTemplate( AsciiString name ) { return findSpecialPowerTemplatePrivate(name); }
	const SpecialPowerTemplate *findSpecialPowerTemplateByID( UnsignedInt id );
	const SpecialPowerTemplate *getSpecialPowerTemplateByIndex( UnsignedInt index ); // for WorldBuilder

	/// does the object (and therefore the player) meet all the requirements to use this power
	Bool canUseSpecialPower( Object *obj, const SpecialPowerTemplate *specialPowerTemplate );

	Int getNumSpecialPowers(); // for WorldBuilder

	static void parseSpecialPowerDefinition( INI *ini );

private:

protected:
	SpecialPowerTemplate *findSpecialPowerTemplatePrivate( AsciiString name );

	typedef std::vector<SpecialPowerTemplate *> SpecialPowerTemplatePtrVector;
	SpecialPowerTemplatePtrVector m_specialPowerTemplates;			///< the special power templates

	UnsignedInt m_nextSpecialPowerID;

};

// EXTERNAL ///////////////////////////////////////////////////////////////////////////////////////
extern SpecialPowerStore *TheSpecialPowerStore;
