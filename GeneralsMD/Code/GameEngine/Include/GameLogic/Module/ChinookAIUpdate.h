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

// ChinookAIUpdate.h //////////
// Author: Steven Johnson, June 2002

#pragma once

#include "GameLogic/AIStateMachine.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
class ChinookAIUpdateModuleData : public SupplyTruckAIUpdateModuleData
{
public:
	AsciiString		m_ropeName;
  AsciiString   m_rotorWashParticleSystem;
  Real					m_rappelSpeed;
	Real					m_ropeDropSpeed;
	Real					m_ropeWidth;
	Real					m_ropeFinalHeight;
	Real					m_ropeWobbleLen;
	Real					m_ropeWobbleAmp;
	Real					m_ropeWobbleRate;
	RGBColor			m_ropeColor;
	UnsignedInt		m_numRopes;
	UnsignedInt		m_perRopeDelayMin;
	UnsignedInt		m_perRopeDelayMax;
	Real					m_minDropHeight;
	Bool					m_waitForRopesToDrop;
	Int						m_upgradedSupplyBoost;

	ChinookAIUpdateModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
enum ChinookFlightStatus CPP_11(: Int) // Stored in save file, don't renumber.  jba.
{
	CHINOOK_TAKING_OFF				= 0,
	CHINOOK_FLYING						= 1,
	CHINOOK_DOING_COMBAT_DROP	= 2,
	CHINOOK_LANDING						= 3,
	CHINOOK_LANDED						= 4
};

//-------------------------------------------------------------------------------------------------
class ChinookAIUpdate : public SupplyTruckAIUpdate
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ChinookAIUpdate, "ChinookAIUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ChinookAIUpdate, ChinookAIUpdateModuleData )

public:

	ChinookAIUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;
 	virtual void aiDoCommand(const AICommandParms* parms) override;
	virtual Bool chooseLocomotorSet(LocomotorSetType wst) override;
	// this is present solely for some transports to override, so that they can land before
	// allowing people to exit...
	virtual AIFreeToExitType getAiFreeToExit(const Object* exiter) const override;
	virtual Bool isAllowedToAdjustDestination() const override;
	virtual ObjectID getBuildingToNotPathAround() const override;

	// this is present for subclasses (eg, Chinook) to override, to
	// prevent supply-ferry behavior in some cases (eg, when toting passengers)
	virtual Bool isAvailableForSupplying() const override;
	virtual Bool isCurrentlyFerryingSupplies() const override;

	virtual Bool isIdle() const override;

	const ChinookAIUpdateModuleData* friend_getData() const { return getChinookAIUpdateModuleData(); }
	void friend_setFlightStatus(ChinookFlightStatus a) { m_flightStatus = a; }

	void recordOriginalPosition( const Coord3D &pos ) { m_originalPos.set( pos ); }
	const Coord3D* getOriginalPosition() const { return &m_originalPos; }

	virtual Int getUpgradedSupplyBoost() const override;

protected:

	virtual AIStateMachine* makeStateMachine() override;

	virtual void privateCombatDrop( Object *target, const Coord3D& pos, CommandSourceType cmdSource ) override;
	virtual void privateGetRepaired( Object *repairDepot, CommandSourceType cmdSource ) override;///< get repaired at repair depot


	virtual void privateAttackObject( Object *victim, Int maxShotsToFire, CommandSourceType cmdSource ) override;///< Extension.  Also tell occupants to attackObject
	virtual void privateAttackPosition( const Coord3D *pos, Int maxShotsToFire, CommandSourceType cmdSource ) override;///< Extension.  Also tell occupants to attackPosition
	virtual void privateForceAttackObject( Object *victim, Int maxShotsToFire, CommandSourceType cmdSource ) override;///< Extension.  Also tell occupants to forceAttackObject

  virtual void privateIdle(CommandSourceType cmdSource) override;

  void private___TellPortableStructureToAttackWithMe( Object *victim, Int maxShotsToFire, CommandSourceType cmdSource );


private:

	void setMyState( StateID cmd, Object* target, const Coord3D* pos, CommandSourceType cmdSource );
	void setAirfieldForHealing(ObjectID id);

	AICommandParmsStorage		m_pendingCommand;
	ChinookFlightStatus			m_flightStatus;
	ObjectID								m_airfieldForHealing;
	Coord3D									m_originalPos;
	Bool										m_hasPendingCommand;
};
