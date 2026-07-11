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

// FILE: AcademyStats.cpp //////////////////////////////////////////////////////
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
// File name:  AcademyStats.cpp
//
// Created:    Kris Morness, July 2003
//
// Desc:			 Keeps track of various statistics in order to provide advice to
//             the player about how to improve playing.
//
//-----------------------------------------------------------------------------

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/AcademyStats.h"
#include "Common/Energy.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"

#include "GameClient/ControlBar.h"
#include "GameClient/GameText.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

#include "GameLogic/Module/ContainModule.h"


const char *const TheAcademyClassificationTypeNames[] =
{
	"ACT_NONE",
	"ACT_UPGRADE_RADAR",
	"ACT_SUPERPOWER",
	nullptr
};
static_assert(ARRAY_SIZE(TheAcademyClassificationTypeNames) == ACT_COUNT + 1, "Incorrect array size");

#define FRAMES_BETWEEN_UPDATES 30


//------------------------------------------------------------------------------------------------
AcademyStats::AcademyStats()
{
}

//------------------------------------------------------------------------------------------------
void findDozerCommandSet( Object *object, void *userData )
{
	const CommandSet *dozerCommandSet = (const CommandSet*)userData;
	if( dozerCommandSet )
	{
		return;
	}
	if( object->isKindOf( KINDOF_DOZER ) )
	{
		dozerCommandSet = TheControlBar->findCommandSet( object->getCommandSetString() );
	}
}

//------------------------------------------------------------------------------------------------
void AcademyStats::init( const Player *player )
{
	if( !TheGameLogic )
	{
		return; // GUIEdit crashes on this, so bail
	}

	m_nextUpdateFrame = TheGameLogic->getFrame() + FRAMES_BETWEEN_UPDATES;
	m_firstUpdate = TRUE;
	m_unknownSide = FALSE;

	//Init the player and cache some important information.
	m_player = player;
	const PlayerTemplate *plyrTemplate = player->getPlayerTemplate();

	if( !plyrTemplate ||
			( plyrTemplate->getBaseSide().compareNoCase( "USA" ) &&
			plyrTemplate->getBaseSide().compareNoCase( "China" ) &&
			plyrTemplate->getBaseSide().compareNoCase( "GLA" ) ) )
	{
		//Admittedly, this is a massive violation of data driven design. Shame on me!
		//Unknown side... don't provide ANY advice. Simplicity reasons.
		m_unknownSide = TRUE;
	}

	//Find the command set for our dozer... so we can extract information about things
	//we can build.
	m_dozerCommandSet = nullptr;
	player->iterateObjects( findDozerCommandSet, (void*)m_dozerCommandSet );

	m_commandCenterTemplate = nullptr;
	m_supplyCenterTemplate = nullptr;

	if( m_dozerCommandSet )
	{
		for( int i = 0; i < MAX_COMMANDS_PER_SET; i++ )
		{
			const CommandButton *button = m_dozerCommandSet->getCommandButton( i );
			if( button )
			{
				const ThingTemplate *thing = button->getThingTemplate();
				if( thing )
				{
					if( thing->isKindOf( KINDOF_FS_SUPPLY_CENTER ) )
					{
						m_supplyCenterTemplate = thing;
						m_supplyCenterCost = m_supplyCenterTemplate->calcCostToBuild( player );
					}
					else if( thing->isKindOf( KINDOF_COMMANDCENTER ) )
					{
						//Wow, this is bogus... but we need this template pointer in order to fire special powers from the
						//shortcut... ugh.
						m_commandCenterTemplate = thing;
					}
				}
			}
		}
	}

	//+-----------------------+
	//| Tier 1 (Basic advice) |
	//+-----------------------+

	//1) Did player build at least one of each structure type available?
	//CUT!!!

	//2) Did player run out of money before building a supply center?
	m_spentCashBeforeBuildingSupplyCenter = FALSE;
	m_supplyCentersBuilt = 0;
	if( !m_supplyCenterTemplate )
	{
		m_supplyCenterCost = 1000;
	}

	//3) Did player build radar (if applicable)?
	m_researchedRadar = FALSE;

	//4) Did player build any dozers/workers?
	m_peonsBuilt = 0;

	//5) Did player ever capture a structure?
	m_structuresCaptured = 0;

	//6) Did player spend any generals points?
	m_generalsPointsSpent = 0;

	//7) Did player ever use a generals power or superweapon?
	m_specialPowersUsed = 0;

	//8) Did player garrison any structures?
	m_structuresGarrisoned = 0;

	//9) How idle was the player in building military units?
	m_idleBuildingUnitsMaxFrames = 0;
	m_lastUnitBuiltFrame = 0;

	//10) Did player drag select units?
	m_dragSelectUnits = 0;

	//11) Did player upgrade anything?
	m_upgradesPurchased = 0;

	//12) Was player out of power for more than 10 minutes?
	m_powerOutMaxFrames = 0;
	m_oldestPowerOutFrame = 0;
	m_hadPowerLastCheck = 0;

	//13) Extra gatherers built?
	m_gatherersBuilt = 0;

	//14) Heroes built?
	m_heroesBuilt = 0;

	//+------------------------------+
	//| Tier 2 (Intermediate advice) |
	//+------------------------------+

	//15) Selected a strategy center battle plan?
	m_hadAStrategyCenter = FALSE;
	m_choseAStrategyForCenter = FALSE;

	//16) Placed units inside tunnel network?
	m_hadATunnelNetwork = FALSE;
	m_unitsEnteredTunnelNetwork = 0;

	//17) Player used control groups?
	m_controlGroupsUsed = 0;

	//18) Built secondary income building?
	m_secondaryIncomeUnitsBuilt = 0;

	//19) Cleared out garrisoned buildings?
	m_clearedGarrisonedBuildings = 0;

	//20) Did the Player pick up salvage (as GLA)?
	m_salvageCollected = 0;

	//21) Did the player ever use the "Guard" ability?
	m_guardAbilityUsedCount = 0;

	//22) Did the player build more than one Supply Center (that is, did he expand out)?
	//Uses m_supplyCentersBuilt!

	//+--------------------------+
	//| Tier 3 (Advanced advice) |
	//+--------------------------+

	//25) Did the player use the new alternate interface in the options?
	//Uses TheGlobalData->m_useAlternateMouse

	//26) Player did not use the new "double click location attack move/guard"
	m_doubleClickAttackMoveOrdersGiven = 0;

  //27) Built barracks within 5 minutes?
	m_builtBarracksWithinFiveMinutes = FALSE;

	//28) Built war factory within 10 minutes?
	m_builtWarFactoryWithinTenMinutes = FALSE;

	//29) Built tech structure within 15 minutes?
	m_builtTechStructureWithinFifteenMinutes = FALSE;

	//30) No income for 2 minutes?
	m_lastIncomeFrame = 0;
	m_maxFramesBetweenIncome = 0;

	//31) Did the Player ever use Dozers/Workers to clear out traps/mines/booby traps?
	m_mines = 0;
	m_minesCleared = 0;

	//32) Captured any sniped vehicles?
	m_vehiclesRecovered = 0;
	m_vehiclesSniped = 0; //Neutral team stat

	//33) Did the player ever build a "disguisable" unit and never used the disguise ability?
	m_disguisableVehiclesBuilt = 0;
	m_vehiclesDisguised = 0;

	//34) Did the player never build a "stealth" upgrade?
	//CUT!!!

	//35) Did the player ever create a "Firestorm" with his MiGs or Inferno Cannons?
	m_firestormsCreated = 0;
}

//------------------------------------------------------------------------------------------------
static void updateAcademyStats( Object *obj, void *userData )
{
	AcademyStats *academy = (AcademyStats*)userData;

	if( academy->isFirstUpdate() )
	{
		academy->recordProduction( obj, nullptr );
	}
}

//------------------------------------------------------------------------------------------------
void AcademyStats::update()
{
	if( m_unknownSide )
	{
		return;
	}

	UnsignedInt now = TheGameLogic->getFrame();

	if( m_nextUpdateFrame >= now )
	{
		m_nextUpdateFrame = now + FRAMES_BETWEEN_UPDATES;
		m_player->iterateObjects( updateAcademyStats, (void*)this );

		//2) Did player run out of money before building a supply center?
		if( !m_supplyCentersBuilt && !m_spentCashBeforeBuildingSupplyCenter )
		{
			const Money *money = m_player->getMoney();
			if( money )
			{
				UnsignedInt amount = money->countMoney();
				if( amount < m_supplyCenterCost )
				{
					m_spentCashBeforeBuildingSupplyCenter = TRUE;
				}
			}
		}

		//12) Was player out of power for more than 10 minutes?
		const Energy *power = m_player->getEnergy();
		if( power )
		{
			Bool hasPower = power->hasSufficientPower();

			if( hasPower != m_hadPowerLastCheck )
			{
				if( !hasPower )
				{
					//We just lost power
					m_oldestPowerOutFrame = now;
				}
				else
				{
					//We just gained power... see how long we were without.
					UnsignedInt frames = now - m_oldestPowerOutFrame;
					if( frames > m_powerOutMaxFrames )
					{
						m_powerOutMaxFrames = frames;
					}
				}
				m_hadPowerLastCheck = hasPower;
			}
		}

		if( isFirstUpdate() )
		{
			setFirstUpdate( FALSE );
		}

	}
}

//------------------------------------------------------------------------------------------------
void AcademyStats::recordProduction( const Object *obj, const Object *constructor )
{
	UnsignedInt now = TheGameLogic->getFrame();

	//2) Did player run out of money before building a supply center?
	//22) Did the player build more than one Supply Center (that is, did he expand out)?
	if( obj->isKindOf( KINDOF_FS_SUPPLY_CENTER ) )
	{
		m_supplyCentersBuilt++;
	}

	//4) Did player build any dozers/workers?
	if( obj->isKindOf( KINDOF_DOZER ) )
	{
		m_peonsBuilt++;
	}

	//9) How idle was the player in building military units?
	if( obj->isKindOf( KINDOF_INFANTRY ) || obj->isKindOf( KINDOF_VEHICLE ) )
	{
		if( !obj->isKindOf( KINDOF_DOZER ) && !obj->isKindOf( KINDOF_HARVESTER ) )
		{
			//How long has it been since we built our last unit?
			UnsignedInt idleFrames = now - m_lastUnitBuiltFrame;

			//If it was longer than our max time... then record it.
			if( idleFrames > m_idleBuildingUnitsMaxFrames )
			{
				m_idleBuildingUnitsMaxFrames = idleFrames;
			}

			//Record the frame we built our unit.
			m_lastUnitBuiltFrame = now;
		}
	}

	//13) Extra gatherers built?
	if( obj->isKindOf( KINDOF_HARVESTER ) )
	{
		m_gatherersBuilt++;
	}

	//14) Heroes built?
	if( obj->isKindOf( KINDOF_HERO ) )
	{
		m_heroesBuilt++;
	}

	//15) Selected a strategy center battle plan?
	if( obj->isKindOf( KINDOF_FS_STRATEGY_CENTER ) )
	{
		m_hadAStrategyCenter = TRUE;
	}

	//16) Placed units inside tunnel network?
	if( obj->getContain() && obj->getContain()->isTunnelContain() )
	{
		m_hadATunnelNetwork = TRUE;
	}

	//18) Built secondary income building?
	if( obj->isKindOf( KINDOF_MONEY_HACKER ) || obj->isKindOf( KINDOF_FS_BLACK_MARKET ) || obj->isKindOf( KINDOF_FS_SUPPLY_DROPZONE ) )
	{
		m_secondaryIncomeUnitsBuilt++;
	}

	//27) Built tech structure within 15 minutes?
	if( obj->isKindOf( KINDOF_FS_BARRACKS ) )
	{
		if( TheGameLogic->getFrame() <= 300 * LOGICFRAMES_PER_SECOND )
		{
			m_builtBarracksWithinFiveMinutes = TRUE;
		}
	}

	//28) Built tech structure within 15 minutes?
	if( obj->isKindOf( KINDOF_FS_WARFACTORY ) )
	{
		if( TheGameLogic->getFrame() <= 600 * LOGICFRAMES_PER_SECOND )
		{
			m_builtWarFactoryWithinTenMinutes = TRUE;
		}
	}

	//29) Built tech structure within 15 minutes?
	if( obj->isKindOf( KINDOF_FS_ADVANCED_TECH ) )
	{
		if( TheGameLogic->getFrame() <= 900 * LOGICFRAMES_PER_SECOND )
		{
			m_builtTechStructureWithinFifteenMinutes = TRUE;
		}
	}

	//33) Did the player ever build a "disguisable" unit and never used the disguise ability?
	if( obj->isKindOf( KINDOF_DISGUISER ) )
	{
		m_disguisableVehiclesBuilt++;
	}

}

//------------------------------------------------------------------------------------------------
void AcademyStats::recordUpgrade( const UpgradeTemplate *upgrade, Bool granted )
{
	//3) Did player build radar (if applicable)?
	if( upgrade->getAcademyClassificationType() == ACT_UPGRADE_RADAR )
	{
		m_researchedRadar = TRUE;
	}

	//11) Did player upgrade anything?
	if( !granted )
	{
		m_upgradesPurchased++;
	}
}

//------------------------------------------------------------------------------------------------
void AcademyStats::recordSpecialPowerUsed( const SpecialPowerTemplate *spTemplate )
{
	if( spTemplate->getAcademyClassificationType() == ACT_SUPERPOWER )
	{
		m_specialPowersUsed++;
	}
}

//------------------------------------------------------------------------------------------------
void AcademyStats::recordIncome()
{
	UnsignedInt now = TheGameLogic->getFrame();

	UnsignedInt delta = m_lastIncomeFrame - now;
	if( delta > m_maxFramesBetweenIncome )
	{
		m_maxFramesBetweenIncome = delta;
	}

	m_lastIncomeFrame = now;
}

//------------------------------------------------------------------------------------------------
void AcademyStats::evaluateTier1Advice( AcademyAdviceInfo *info, Int numAvailableTips )
{
	UnsignedInt maxAdviceTips = MAX_ADVICE_TIPS;

	//-allAdvice feature
	//if( !TheGlobalData->m_allAdvice )
	//{
	//	maxAdviceTips = 1;
	//}

	UnsignedInt now = TheGameLogic->getFrame();
	//numAvailableTips is used to determine if we are going to randomly choose a tip
	//or determine if a tip is available (if -1)
	Bool choosing = numAvailableTips != -1;
	Int availableTips = 0;

	//1) Did player build at least one of each structure type available?
	//CUT!!!

	//2) Did player run out of money before building a supply center?
	if( m_spentCashBeforeBuildingSupplyCenter )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:BuildSupplyCenterEarlier" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//3) Did player build radar (if applicable)?
	if( !m_researchedRadar )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:TryBuildingRadar" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//4) Did player build any dozers/workers?
	if( m_peonsBuilt < 2 )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:BuildMorePeons" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//5) Did player ever capture a structure?
	if( !m_structuresCaptured )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:TryCapturingStructures" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//6) Did player spend any generals points?
	if( !m_generalsPointsSpent )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:SpendGeneralsPoints" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//7) Did player ever use a generals power or superweapon?
	if( !m_specialPowersUsed )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:TryUsingSuperweapons" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//8) Did player garrison any structures?
	if( !m_structuresGarrisoned )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:TryGarrisoningAStructure" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//9) How idle was the player in building military units?
	UnsignedInt idleFrames = now - m_lastUnitBuiltFrame;
	if( idleFrames > m_idleBuildingUnitsMaxFrames )
	{
		m_idleBuildingUnitsMaxFrames = idleFrames;
	}
	if( m_idleBuildingUnitsMaxFrames > 300 * LOGICFRAMES_PER_SECOND || !m_lastUnitBuiltFrame )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:IdleBuildingUnits" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//10) Did player drag select units?
	if( !m_dragSelectUnits )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:TryDragSelectingUnits" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//11) Did player upgrade anything?
	if( !m_upgradesPurchased )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:ResearchUpgrades" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//12) Was player out of power for more than 10 minutes?

	if( !m_hadPowerLastCheck )
	{
		UnsignedInt frames = now - m_oldestPowerOutFrame;
		if( frames > m_powerOutMaxFrames )
		{
			m_powerOutMaxFrames = frames;
		}
	}
	if( m_powerOutMaxFrames > 600 * LOGICFRAMES_PER_SECOND )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:RanOutOfPower" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//13) Extra gatherers built?
	if( !m_gatherersBuilt )
	{
		//Don't count free ones that come with supply centers!
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:BuildMoreGatherers" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//14) Heroes built?
	if( !m_heroesBuilt )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:BuildAHero" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	if( !choosing && availableTips > 0 )
	{
		//We're done counting available tips, so now that we know the
		//number, we can randomly choose.
		evaluateTier1Advice( info, availableTips );
	}
}

//------------------------------------------------------------------------------------------------
void AcademyStats::evaluateTier2Advice( AcademyAdviceInfo *info, Int numAvailableTips )
{
	UnsignedInt maxAdviceTips = MAX_ADVICE_TIPS;

	//-allAdvice feature
	//if( !TheGlobalData->m_allAdvice )
	//{
	//	maxAdviceTips = 1;
	//}

	//numAvailableTips is used to determine if we are going to randomly choose a tip
	//or determine if a tip is available (if -1)
	Bool choosing = numAvailableTips != -1;
	Int availableTips = 0;

	//15) Selected a strategy center battle plan?
	if( m_hadAStrategyCenter && !m_choseAStrategyForCenter )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:PickStrategyCenterPlan" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//16) Placed units inside tunnel network?
	if( m_hadATunnelNetwork && !m_unitsEnteredTunnelNetwork )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:UseTunnelNetwork" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//17) Player used control groups?
	if( !m_controlGroupsUsed )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:UseControlGroups" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//18) Built secondary income building?
	if( !m_secondaryIncomeUnitsBuilt )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:UseSecondaryIncomeMethods" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//19) Cleared out garrisoned buildings?
	if( !m_clearedGarrisonedBuildings )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:ClearBuildings" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//20) Did the Player pick up salvage (as GLA)?
	if( m_player->getPlayerTemplate() && !m_player->getPlayerTemplate()->getBaseSide().compareNoCase( "GLA" ) )
	{
		if( !m_salvageCollected )
		{
			availableTips++;
			Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
			Int limit = maxAdviceTips - info->numTips;
			if( choosing &&  rand < limit )
			{
				info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:PickUpSalvage" ) );
				info->numTips++;
			}
			numAvailableTips--;
		}
	}

	//21) Did the player ever use the "Guard" ability?
	if( !m_guardAbilityUsedCount )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:UseGuardAbility" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//22) Did the player build more than one Supply Center (that is, did he expand out)?
	if( m_supplyCentersBuilt < 2 )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:MultipleSupplyCenters" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//23) Did the player ever garrison a vehicle?
	//CUT!!!

	//24) Did the player ever use the hotkey to grab all of one unit type (change to use any hotkeys)?
	//CUT!!!

	if( !choosing && availableTips > 0 )
	{
		//We're done counting available tips, so now that we know the
		//number, we can randomly choose.
		evaluateTier2Advice( info, availableTips );
	}

}

//------------------------------------------------------------------------------------------------
void AcademyStats::evaluateTier3Advice( AcademyAdviceInfo *info, Int numAvailableTips )
{
	UnsignedInt maxAdviceTips = MAX_ADVICE_TIPS;

	//-allAdvice feature
	//if( !TheGlobalData->m_allAdvice )
	//{
	//	maxAdviceTips = 1;
	//}

	UnsignedInt now = TheGameLogic->getFrame();

	//numAvailableTips is used to determine if we are going to randomly choose a tip
	//or determine if a tip is available (if -1)
	Bool choosing = numAvailableTips != -1;
	Int availableTips = 0;

	//25) Did the player use the new alternate interface in the options?
	if( !TheGlobalData->m_useAlternateMouse )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:AlternateMouseInterface" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//26) Player did not use the new "double click location attack move/guard"
	if( !m_doubleClickAttackMoveOrdersGiven )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:DoubleClickAttackMoveGuard" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

  //27) Built barracks within 5 minutes?
	if( !m_builtBarracksWithinFiveMinutes )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:BuildBarracksSooner" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//28) Built warfactory within 10 minutes?
	if( !m_builtWarFactoryWithinTenMinutes )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:BuildWarFactorySooner" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//29) Built tech structure within 15 minutes?
	if( !m_builtTechStructureWithinFifteenMinutes )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:BuildTechStructureSooner" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//30) No income for 2 minutes?
	UnsignedInt delta = m_lastIncomeFrame - now;
	if( delta > m_maxFramesBetweenIncome )
	{
		m_maxFramesBetweenIncome = delta;
	}
	if( m_maxFramesBetweenIncome > LOGICFRAMES_PER_SECOND * 120 || !m_lastIncomeFrame )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:NoIncome" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//31) Did the Player ever use Dozers/Workers to clear out traps/mines/booby traps?
	if( ThePlayerList->getLocalPlayer()->getAcademyStats()->getMines() > 0 && !m_minesCleared )
	{
		availableTips++;
		Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
		Int limit = maxAdviceTips - info->numTips;
		if( choosing &&  rand < limit )
		{
			info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:NoIncome" ) );
			info->numTips++;
		}
		numAvailableTips--;
	}

	//32) Captured any sniped vehicles?
	if( ThePlayerList->getNeutralPlayer()->getAcademyStats()->getVehiclesSniped() > 0 )
	{
		if( !m_vehiclesRecovered )
		{
			availableTips++;
			Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
			Int limit = maxAdviceTips - info->numTips;
			if( choosing &&  rand < limit )
			{
				info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:UnmannedVehicles" ) );
				info->numTips++;
			}
			numAvailableTips--;
		}
	}

	//33) Did the player ever build a "disguisable" unit and never used the disguise ability?
	if( m_disguisableVehiclesBuilt )
	{
		if( !m_vehiclesDisguised )
		{
			availableTips++;
			Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
			Int limit = maxAdviceTips - info->numTips;
			if( choosing &&  rand < limit )
			{
				info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:DisguisedUnits" ) );
				info->numTips++;
			}
			numAvailableTips--;
		}
	}

	//35) Did the player ever create a "Firestorm" with his MiGs or Inferno Cannons?
	if( m_player->getPlayerTemplate() && !m_player->getPlayerTemplate()->getBaseSide().compareNoCase( "China" ) )
	{
		if( !m_firestormsCreated )
		{
			availableTips++;
			Int rand = GameClientRandomValue( 0, numAvailableTips - 1 );
			Int limit = maxAdviceTips - info->numTips;
			if( choosing &&  rand < limit )
			{
				info->advice[ info->numTips ].concat( TheGameText->fetch( "ACADEMY:Firestorm" ) );
				info->numTips++;
			}
			numAvailableTips--;
		}
	}

	if( !choosing && availableTips > 0 )
	{
		//We're done counting available tips, so now that we know the
		//number, we can randomly choose.
		evaluateTier3Advice( info, availableTips );
	}
}

//------------------------------------------------------------------------------------------------
Bool AcademyStats::calculateAcademyAdvice( AcademyAdviceInfo *info )
{
	UnsignedInt maxAdviceTips = MAX_ADVICE_TIPS;

	//-allAdvice feature
	//if( !TheGlobalData->m_allAdvice )
	//{
	//	maxAdviceTips = 1;
	//}

	if( m_unknownSide )
	{
		return FALSE;
	}

	//Sanity
	if( !info )
	{
		DEBUG_CRASH( ("AcademyStats::calculateAcademyAdvice() was passed in null AcademyAdviceInfo.") );
		return FALSE;
	}

	//It's possible to play a perfect game and not need any advice.
	info->numTips = 0;

	//Build the header for each string.
	for( UnsignedInt i = 0; i < maxAdviceTips; i++ )
	{
		info->advice[ i ].format( L"\n\n" );
	}

	//First look at tier 1 basic advice and pick any advice we could benefit from.
	evaluateTier1Advice( info );

	//Now look at tier 2 intermediate advice.
	if( info->numTips < maxAdviceTips )
	{
		evaluateTier2Advice( info );

		if( info->numTips < maxAdviceTips )
		{
			//Finally look at tier 3 advanced advice.
			evaluateTier3Advice( info );
		}
	}

	//Do we have any advice to give?
	return info->numTips != 0;
}

//------------------------------------------------------------------------------------------------
// CRC
//------------------------------------------------------------------------------------------------
void AcademyStats::crc( Xfer *xfer )
{

}

//------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
//------------------------------------------------------------------------------------------------
void AcademyStats::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	xfer->xferUnsignedInt( &m_nextUpdateFrame );
	xfer->xferBool( &m_firstUpdate );
	xfer->xferBool( &m_unknownSide );

	//+-----------------------+
	//| Tier 1 (Basic advice) |
	//+-----------------------+

	//1) Did player build at least one of each structure type available?
	//CUT!!!

	//2) Did player run out of money before building a supply center?
	xfer->xferBool( &m_spentCashBeforeBuildingSupplyCenter );
	xfer->xferUnsignedInt( &m_supplyCentersBuilt );
	xfer->xferUnsignedInt( &m_supplyCenterCost );

	//3) Did player build radar (if applicable)?
	xfer->xferBool( &m_researchedRadar );

	//4) Did player build any dozers/workers?
	xfer->xferUnsignedInt( &m_peonsBuilt );

	//5) Did player ever capture a structure?
	xfer->xferUnsignedInt( &m_structuresCaptured );

	//6) Did player spend any generals points?
	xfer->xferUnsignedInt( &m_generalsPointsSpent );

	//7) Did player ever use a generals power or superweapon?
	xfer->xferUnsignedInt( &m_specialPowersUsed );

	//8) Did player garrison any structures?
	xfer->xferUnsignedInt( &m_structuresGarrisoned );

	//9) How idle was the player in building military units?
	xfer->xferUnsignedInt( &m_idleBuildingUnitsMaxFrames );
	xfer->xferUnsignedInt( &m_lastUnitBuiltFrame );

	//10) Did player drag select units?
	xfer->xferUnsignedInt( &m_dragSelectUnits );

	//11) Did player upgrade anything?
	xfer->xferUnsignedInt( &m_upgradesPurchased );

	//12) Was player out of power for more than 10 minutes?
	xfer->xferUnsignedInt( &m_powerOutMaxFrames );
	xfer->xferUnsignedInt( &m_oldestPowerOutFrame );
	xfer->xferBool			(	&m_hadPowerLastCheck );

	//13) Extra gathers built?
	xfer->xferUnsignedInt( &m_gatherersBuilt );

	//14) Heroes built?
	xfer->xferUnsignedInt( &m_heroesBuilt );

	//+------------------------------+
	//| Tier 2 (Intermediate advice) |
	//+------------------------------+

	//15) Selected a strategy center battle plan?
	xfer->xferBool( &m_hadAStrategyCenter );
	xfer->xferBool( &m_choseAStrategyForCenter );

	//16) Placed units inside tunnel network?
	xfer->xferUnsignedInt( &m_unitsEnteredTunnelNetwork );
	xfer->xferBool( &m_hadATunnelNetwork );

	//17) Player used control groups?
	xfer->xferUnsignedInt( &m_controlGroupsUsed );

	//18) Built secondary income unit (hacker, dropzone, blackmarket)?
	xfer->xferUnsignedInt( &m_secondaryIncomeUnitsBuilt );

	//19) Cleared out garrisoned buildings?
	xfer->xferUnsignedInt( &m_clearedGarrisonedBuildings );

	//20) Did the Player pick up salvage (as GLA)?
	xfer->xferUnsignedInt( &m_salvageCollected );

	//21) Did the player ever use the "Guard" ability?
	xfer->xferUnsignedInt( &m_guardAbilityUsedCount );

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
	xfer->xferUnsignedInt( &m_doubleClickAttackMoveOrdersGiven );

  //27) Built barracks within 5 minutes?
	xfer->xferBool( &m_builtBarracksWithinFiveMinutes );

	//28) Built war factory within 10 minutes?
	xfer->xferBool( &m_builtWarFactoryWithinTenMinutes );

	//29) Built tech structure within 15 minutes?
	xfer->xferBool( &m_builtTechStructureWithinFifteenMinutes );

	//30) No income for 2 minutes?
	xfer->xferUnsignedInt( &m_lastIncomeFrame );
	xfer->xferUnsignedInt( &m_maxFramesBetweenIncome );

	//31) Did the Player ever use Dozers/Workers to clear out traps/mines/booby traps?
	xfer->xferUnsignedInt( &m_mines ); //Neutral player stat
	xfer->xferUnsignedInt( &m_minesCleared );

	//32) Captured any sniped vehicles?
	xfer->xferUnsignedInt( &m_vehiclesRecovered );
	xfer->xferUnsignedInt( &m_vehiclesSniped );

	//33) Did the player ever build a "disguisable" unit and never used the disguise ability?
	xfer->xferUnsignedInt( &m_disguisableVehiclesBuilt );
	xfer->xferUnsignedInt( &m_vehiclesDisguised );

	//34) Did the player never build a "stealth" upgrade?
	//CUT!!!

	//35) Did the player ever create a "Firestorm" with his MiGs or Inferno Cannons?
	xfer->xferUnsignedInt( &m_firestormsCreated );

}

//------------------------------------------------------------------------------------------------
// Load post process
//------------------------------------------------------------------------------------------------
void AcademyStats::loadPostProcess()
{

}
