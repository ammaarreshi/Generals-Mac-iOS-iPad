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

// FILE: AutoDepositUpdate.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Aug 2002
//
//	Filename: 	AutoDepositUpdate.cpp
//
//	author:		Chris Huybregts
//
//	purpose:	The meat of the auto deposit update module
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/BuildAssistant.h"
#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AutoDepositUpdate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Color.h"
#include "GameClient/GameText.h"

//-------------------------------------------------------------------------------------------------
void parseUpgradePair( INI *ini, void *instance, void *store, const void *userData )
{
	upgradePair info;
	info.amount = 0;

	const char *token = ini->getNextToken( ini->getSepsColon() );
	if ( stricmp(token, "UpgradeType") == 0 )
	{
		token = ini->getNextToken( ini->getSepsColon() );
		info.type = token;
	}
	else
		throw INI_INVALID_DATA;


	token = ini->getNextToken( ini->getSepsColon() );
	if ( stricmp(token, "Boost") == 0 )
		info.amount = INI::scanInt(ini->getNextToken( ini->getSepsColon() ));
	else
		throw INI_INVALID_DATA;

	// Insert the info into the upgrade list
	std::list<upgradePair> * theList = (std::list<upgradePair>*)store;
	theList->push_back(info);

}

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
AutoDepositUpdate::AutoDepositUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_depositOnFrame = TheGameLogic->getFrame() + getAutoDepositUpdateModuleData()->m_depositFrame;
	m_awardInitialCaptureBonus = FALSE;
	m_initialized = FALSE;
}
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AutoDepositUpdate::~AutoDepositUpdate()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void AutoDepositUpdate::awardInitialCaptureBonus( Player *player )
{
	m_depositOnFrame = TheGameLogic->getFrame() + getAutoDepositUpdateModuleData()->m_depositFrame;
	if(!player || !m_awardInitialCaptureBonus || getAutoDepositUpdateModuleData()->m_initialCaptureBonus <= 0)
		return;

	player->getMoney()->deposit( getAutoDepositUpdateModuleData()->m_initialCaptureBonus );
	player->getScoreKeeper()->addMoneyEarned( getAutoDepositUpdateModuleData()->m_initialCaptureBonus );

	//Display cash income floating over the blacklotus
	if(getAutoDepositUpdateModuleData()->m_initialCaptureBonus > 0)
	{
		UnicodeString moneyString;
		moneyString.format( TheGameText->fetch( "GUI:AddCash" ), getAutoDepositUpdateModuleData()->m_initialCaptureBonus );
		Coord3D pos;
		pos.set( *getObject()->getPosition() );
		pos.z += 10.0f; //add a little z to make it show up above the unit.
		Color color = player->getPlayerColor() | GameMakeColor( 0, 0, 0, 230 );
		TheInGameUI->addFloatingText( moneyString, &pos, color );
	}

	m_awardInitialCaptureBonus = FALSE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime AutoDepositUpdate::update()
{
	const AutoDepositUpdateModuleData *modData = getAutoDepositUpdateModuleData();
/// @todo srj use SLEEPY_UPDATE here
	if( TheGameLogic->getFrame() >= m_depositOnFrame)
	{
		if (!m_initialized) {
			// Note - we have to set these in update, because during load the team is set,
			// and we don't want to award initial bonus on load.  jba :)
			m_awardInitialCaptureBonus = TRUE;
			m_initialized = TRUE;
		}
		m_depositOnFrame = TheGameLogic->getFrame() + modData->m_depositFrame;

		if(getObject()->isNeutralControlled() || modData->m_depositAmount <= 0 )
			return UPDATE_SLEEP_NONE;

		// makes sure that buildings under construction do not get a bonus CCB
		if( getObject()->getConstructionPercent() != CONSTRUCTION_COMPLETE )
			return UPDATE_SLEEP_NONE;

		int moneyAmount = modData->m_depositAmount + getUpgradedSupplyBoost();

		if( modData->m_isActualMoney )
		{
			getObject()->getControllingPlayer()->getMoney()->deposit( moneyAmount );
			getObject()->getControllingPlayer()->getScoreKeeper()->addMoneyEarned( modData->m_depositAmount);
		}

		if (moneyAmount > 0 && getObject()->isLogicallyVisible())
		{

      const Object *owner = getObject();
      if ( owner )
      {

			  // OY LOOK!  I AM USING LOCAL PLAYER.  Do not put anything other than TheInGameUI->addFloatingText in the block this controls!!!
			  UnicodeString moneyString;
			  moneyString.format( TheGameText->fetch( "GUI:AddCash" ), moneyAmount );
			  Coord3D pos;
			  pos.set( *getObject()->getPosition() );
			  pos.z += 10.0f; //add a little z to make it show up above the unit.

        if ( owner->isKindOf( KINDOF_STRUCTURE ) )
        {
          Real width = owner->getGeometryInfo().getMajorRadius() * 0.3f;
          Real depth = owner->getGeometryInfo().getMinorRadius() * 0.3f;
          pos.x += GameClientRandomValue(-width,width);
          pos.y += GameClientRandomValue(-depth,depth);
        }


        Color color = getObject()->getControllingPlayer()->getPlayerColor() | GameMakeColor( 0, 0, 0, 230 );
			  TheInGameUI->addFloatingText( moneyString, &pos, color );
      }
		}
	}

	return UPDATE_SLEEP_NONE;
}

//------------------------------------------------------------------------------------------------
Int AutoDepositUpdate::getUpgradedSupplyBoost() const
{
	Player *player = getObject()->getControllingPlayer();
	if (!player) return 0;

	// Loop through the upgrade pairs and see if an upgrade is present that adds supply boost
	std::list<upgradePair>::const_iterator it = getAutoDepositUpdateModuleData()->m_upgradeBoost.begin();
	while (it != getAutoDepositUpdateModuleData()->m_upgradeBoost.end())
	{
		upgradePair info = *it;

		// Check if the player has the desired upgrade. If so return the boost
		static const UpgradeTemplate *upgradeTemplate = TheUpgradeCenter->findUpgrade( info.type.c_str() );
		if (player && upgradeTemplate && player->hasUpgradeComplete(upgradeTemplate))
		{
			return info.amount;
		}

		// check next
		++it;
	}

	return 0;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AutoDepositUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void AutoDepositUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// deposit on frame
	xfer->xferUnsignedInt( &m_depositOnFrame );

	// award initial capture bonus
	xfer->xferBool( &m_awardInitialCaptureBonus );
	if (version>1) {
		xfer->xferBool(&m_initialized);
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AutoDepositUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
