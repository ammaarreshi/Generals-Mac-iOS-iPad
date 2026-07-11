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

// FILE: AcademyStats.h ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Los Angeles
//
//                       Confidential Information
//                Copyright (C) 2003 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    RTS3
//
// File name:  AcademyStats.h
//
// Created:    Kris Morness, July 2003
//
// Desc:			 Keeps track of various statistics in order to provide advice to
//             the player about how to improve playing.
//
//-----------------------------------------------------------------------------

#pragma once

#include "Lib/BaseType.h"
#include "Common/Debug.h"
#include "Common/Snapshot.h"
#include "Common/UnicodeString.h"

class Object;
class Player;
class SpecialPowerTemplate;
class UpgradeTemplate;
class CommandSet;
class ThingTemplate;

#define MAX_ADVICE_TIPS 1

struct AcademyAdviceInfo
{
	UnicodeString advice[ MAX_ADVICE_TIPS ];
	UnsignedInt numTips;
};

enum AcademyClassificationType CPP_11(: Int)
{
	//Don't forget to update the strings too!
	ACT_NONE,
	ACT_UPGRADE_RADAR,
	ACT_SUPERPOWER,

	ACT_COUNT
};
extern const char *const TheAcademyClassificationTypeNames[]; //Change above, change this!


// ----------------------------------------------------------------------------------------------
class AcademyStats : public Snapshot
{

public:

	AcademyStats();

	void init( const Player *player );
	void update();

	Bool isFirstUpdate() const { return m_firstUpdate; }
	void setFirstUpdate( Bool set ) { m_firstUpdate = set; }

	void recordProduction( const Object *obj, const Object *constructor );
	void recordUpgrade( const UpgradeTemplate *upgrade, Bool granted );
	void recordSpecialPowerUsed( const SpecialPowerTemplate *spTemplate );
	void recordIncome();

	void recordBuildingCapture() { m_structuresCaptured++; }
	void recordGeneralsPointsSpent( Int points ) { m_generalsPointsSpent += points; }
	void recordBuildingGarrisoned() { m_structuresGarrisoned++; }
	void recordDragSelection() { m_dragSelectUnits++; }
	void recordStrategyCenter() { m_hadAStrategyCenter = TRUE; }
	void recordBattlePlanSelected() { m_choseAStrategyForCenter = TRUE; }
	void recordUnitEnteredTunnelNetwork() { m_unitsEnteredTunnelNetwork++; }
	void recordControlGroupsUsed() { m_controlGroupsUsed++; }
	void recordClearedGarrisonedBuilding() { m_clearedGarrisonedBuildings++; }
	void recordVehicleDisguised() { m_vehiclesDisguised++; }
	void recordFirestormCreated() { m_firestormsCreated++; }
	void recordGuardAbilityUsed() { m_guardAbilityUsedCount++; }
	void recordSalvageCollected() { m_salvageCollected++;}
	void recordDoubleClickAttackMoveOrderGiven() { m_doubleClickAttackMoveOrdersGiven++; }
	void recordMineCleared() { m_minesCleared++; }

	//Returns the natural command center template (you may capture others in a game...)
	const ThingTemplate* getCommandCenterTemplate() const { return m_commandCenterTemplate; }

	//Use these functions for the Neutral player only!
	void recordVehicleSniped() { m_vehiclesSniped++; }
	UnsignedInt getVehiclesSniped() const { return m_vehiclesSniped; }
	void recordMine() { m_mines++; }
	UnsignedInt getMines() const { return m_mines; }

	const Player *getPlayer() { return m_player; }
	Bool hadASupplyCenter() const { return m_supplyCentersBuilt > 0; }

	Bool calculateAcademyAdvice( AcademyAdviceInfo *info );

protected:
	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

private:

	void evaluateTier1Advice( AcademyAdviceInfo *info, Int numAvailableTips = -1 );
	void evaluateTier2Advice( AcademyAdviceInfo *info, Int numAvailableTips = -1 );
	void evaluateTier3Advice( AcademyAdviceInfo *info, Int numAvailableTips = -1 );

	const Player *m_player;
	UnsignedInt m_nextUpdateFrame;
	Bool m_firstUpdate;
	const CommandSet *m_dozerCommandSet;
	Bool m_unknownSide;
	const ThingTemplate *m_commandCenterTemplate;

	//+-----------------------+
	//| Tier 1 (Basic advice) |
	//+-----------------------+

	//1) Did player build at least one of each structure type available?
	//CUT!!!

	//2) Did player run out of money before building a supply center?
	Bool m_spentCashBeforeBuildingSupplyCenter;
	UnsignedInt m_supplyCentersBuilt;
	const ThingTemplate *m_supplyCenterTemplate;
	UnsignedInt m_supplyCenterCost;

	//3) Did player build radar (if applicable)?
	Bool m_researchedRadar;

	//4) Did player build any dozers/workers?
	UnsignedInt m_peonsBuilt;

	//5) Did player ever capture a structure?
	UnsignedInt m_structuresCaptured;

	//6) Did player spend any generals points?
	UnsignedInt m_generalsPointsSpent;

	//7) Did player ever use a generals power or superweapon?
	UnsignedInt m_specialPowersUsed;

	//8) Did player garrison any structures?
	UnsignedInt m_structuresGarrisoned;

	//9) How idle was the player in building military units?
	UnsignedInt m_idleBuildingUnitsMaxFrames;
	UnsignedInt m_lastUnitBuiltFrame;

	//10) Did player drag select units?
	UnsignedInt m_dragSelectUnits;

	//11) Did player upgrade anything?
	UnsignedInt m_upgradesPurchased;

	//12) Was player out of power for more than 10 minutes?
	UnsignedInt m_powerOutMaxFrames;
	UnsignedInt m_oldestPowerOutFrame;
	Bool				m_hadPowerLastCheck;

	//13) Extra gathers built?
	UnsignedInt m_gatherersBuilt;

	//14) Heroes built?
	UnsignedInt m_heroesBuilt;

	//+------------------------------+
	//| Tier 2 (Intermediate advice) |
	//+------------------------------+

	//15) Selected a strategy center battle plan?
	Bool m_hadAStrategyCenter;
	Bool m_choseAStrategyForCenter;

	//16) Placed units inside tunnel network?
	UnsignedInt m_unitsEnteredTunnelNetwork;
	Bool m_hadATunnelNetwork;

	//17) Player used control groups?
	UnsignedInt m_controlGroupsUsed;

	//18) Built secondary income unit (hacker, dropzone, blackmarket)?
	UnsignedInt m_secondaryIncomeUnitsBuilt;

	//19) Cleared out garrisoned buildings?
	UnsignedInt m_clearedGarrisonedBuildings;

	//20) Did the Player pick up salvage (as GLA)?
	UnsignedInt m_salvageCollected;

	//21) Did the player ever use the "Guard" ability?
	UnsignedInt m_guardAbilityUsedCount;

	//22) Did the player build more than one Supply Center (that is, did he expand out)?
	//Uses m_supplyCentersBuilt!

	//23) Did the player ever garrison a vehicle?
	//CUT!!!

	//24) Did the player ever use the hotkey to grab all of one unit type (change to use any hotkeys)?
	//CUT!!!

	//+--------------------------+
	//| Tier 3 (Advanced advice) |
	//+--------------------------+

	//25) Did the player use the new alternate interface in the options?
	//Uses TheGlobalData->m_useAlternateMouse

	//26) Player did not use the new "double click location attack move/guard"
	UnsignedInt m_doubleClickAttackMoveOrdersGiven;

  //27) Built barracks within 5 minutes?
	Bool m_builtBarracksWithinFiveMinutes;

	//28) Built war factory within 10 minutes?
	Bool m_builtWarFactoryWithinTenMinutes;

	//29) Built tech structure within 15 minutes?
	Bool m_builtTechStructureWithinFifteenMinutes;

	//30) No income for 2 minutes?
	UnsignedInt m_lastIncomeFrame;
	UnsignedInt m_maxFramesBetweenIncome;

	//31) Did the Player ever use Dozers/Workers to clear out traps/mines/booby traps?
	UnsignedInt m_mines; //Neutral player stat
	UnsignedInt m_minesCleared;

	//32) Captured any sniped vehicles?
	UnsignedInt m_vehiclesRecovered;
	UnsignedInt m_vehiclesSniped;

	//33) Did the player ever build a "disguisable" unit and never used the disguise ability?
	UnsignedInt m_disguisableVehiclesBuilt;
	UnsignedInt m_vehiclesDisguised;

	//34) Did the player never build a "stealth" upgrade?
	//CUT!!!

	//35) Did the player ever create a "Firestorm" with his MiGs or Inferno Cannons?
	UnsignedInt m_firestormsCreated;
};
