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

// FILE: CommandButtonHuntUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Author: John Ahlquist, Sept. 2002
// Desc:   Update module to handle "Hunting" using a special power.  If the unit is idle, and the
// power is not active, it targets a new unit with the power.	 Note that this is an update rather than
// an AI state because many of the special abilities use the ai to perform portions of the special
// ability.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ActionManager.h"
#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/Module/CommandButtonHuntUpdate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/CollideModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/ScriptEngine.h"


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CommandButtonHuntUpdateModuleData::CommandButtonHuntUpdateModuleData()
{
	m_scanFrames				= LOGICFRAMES_PER_SECOND;
	m_scanRange					= 9999.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void CommandButtonHuntUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "ScanRate",							INI::parseDurationUnsignedInt,	nullptr, offsetof( CommandButtonHuntUpdateModuleData, m_scanFrames ) },
		{ "ScanRange",						INI::parseReal,									nullptr, offsetof( CommandButtonHuntUpdateModuleData, m_scanRange ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
CommandButtonHuntUpdate::CommandButtonHuntUpdate( Thing *thing, const ModuleData* moduleData ) :
UpdateModule( thing, moduleData ),
m_commandButton(nullptr)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
	m_commandButtonName = AsciiString::TheEmptyString;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CommandButtonHuntUpdate::~CommandButtonHuntUpdate()
{

}


//-------------------------------------------------------------------------------------------------
void CommandButtonHuntUpdate::onObjectCreated()
{

}

//-------------------------------------------------------------------------------------------------
void CommandButtonHuntUpdate::setCommandButton(const AsciiString& buttonName)
{
	Object *obj = getObject();
	m_commandButtonName = buttonName;
	m_commandButton = nullptr;
	const CommandSet *commandSet = TheControlBar->findCommandSet( obj->getCommandSetString() );
	if( commandSet )
	{
		for( Int i = 0; i < MAX_COMMANDS_PER_SET; i++ )
		{
			//Get the command button.
			m_commandButton = commandSet->getCommandButton(i);

			if( m_commandButton )
			{
				if( !m_commandButton->getName().isEmpty() )
				{
					if( m_commandButton->getName() == m_commandButtonName )
					{
						break;
					}
				}
			}
			m_commandButton = nullptr;
		}
	}
	if (m_commandButton==nullptr) {
		return;
	}
	AIUpdateInterface *ai = obj->getAI();
	if (ai==nullptr ) return;

	// Stop whatever we're doing.
	ai->aiIdle(CMD_FROM_AI);
	update();
	setWakeFrame(obj, UPDATE_SLEEP_NONE); // wake up.
}

//-------------------------------------------------------------------------------------------------
/** The update callback. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime CommandButtonHuntUpdate::update()
{

	Object *obj = getObject();

	AIUpdateInterface *ai = obj->getAI();
	if (ai==nullptr || m_commandButton==nullptr) return UPDATE_SLEEP_FOREVER;

	if (ai->getLastCommandSource() != CMD_FROM_AI) {
		// If a script or the player (in this case should only be script, but either way)
		// we quit hunting.
		m_commandButton = nullptr;
		m_commandButtonName.clear();
		return UPDATE_SLEEP_FOREVER;
	}

	switch( m_commandButton->getCommandType() )
	{
		case GUI_COMMAND_SPECIAL_POWER:
			return huntSpecialPower(ai);
		case GUI_COMMAND_SWITCH_WEAPON:
		case GUI_COMMAND_FIRE_WEAPON:
			return huntWeapon(ai);
		default:
			return UPDATE_SLEEP_FOREVER;
	}
	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
/** Do a special power weapon. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime CommandButtonHuntUpdate::huntWeapon(AIUpdateInterface *ai)
{
	Object *obj = getObject();

	if(  ai->isIdle() )
	{
		ai->aiHunt(CMD_FROM_AI);
	}
	WeaponSlotType weaponSlot = m_commandButton->getWeaponSlot();
	// lock it just till the weapon is empty or the attack is "done"
	obj->setWeaponLock( weaponSlot, LOCKED_TEMPORARILY );
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
/** Do a special power hunt. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime CommandButtonHuntUpdate::huntSpecialPower(AIUpdateInterface *ai)
{
	Object *obj = getObject();
	const CommandButtonHuntUpdateModuleData *data = getCommandButtonHuntUpdateModuleData();
	if(  !ai->isIdle() )
	{
		return UPDATE_SLEEP(data->m_scanFrames);
	}
	const SpecialPowerTemplate *spTemplate = m_commandButton->getSpecialPowerTemplate();
	if( spTemplate )
	{
		SpecialAbilityUpdate* spUpdate = obj->findSpecialAbilityUpdate( spTemplate->getSpecialPowerType() );
		if (spUpdate == nullptr) return UPDATE_SLEEP_FOREVER;
		if (spUpdate->isActive()) {
			return UPDATE_SLEEP(data->m_scanFrames);
		}
	}
	//Periodic scanning (expensive)
	Object *victim = scanClosestTarget();
	if(victim)
	{
		obj->doCommandButtonAtObject( m_commandButton, victim, CMD_FROM_AI );
	}
	return UPDATE_SLEEP(data->m_scanFrames);
}


//-------------------------------------------------------------------------------------------------
Object* CommandButtonHuntUpdate::scanClosestTarget()
{
	const CommandButtonHuntUpdateModuleData *data = getCommandButtonHuntUpdateModuleData();
	Object *me = getObject();

	PartitionFilterAlive aliveFilter;

	// only consider enemies.
	PartitionFilterRelationship	filterTeam(me, PartitionFilterRelationship::ALLOW_ENEMIES);
	PartitionFilterSameMapStatus filterMapStatus(getObject());

	PartitionFilter *filters[4];
	filters[0] = &aliveFilter;
	filters[1] = &filterMapStatus;
	filters[2] = &filterTeam;
	filters[3] = nullptr;

 	const SpecialPowerTemplate *spTemplate = m_commandButton->getSpecialPowerTemplate();
	if( !spTemplate ) return nullptr;  // isn't going to happen.
	Bool isBlackLotusVehicleHack = 	(spTemplate->getSpecialPowerType() == SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK);
	Bool isCaptureBuilding = 	(spTemplate->getSpecialPowerType() == SPECIAL_INFANTRY_CAPTURE_BUILDING);
	if (isCaptureBuilding) {
		filters[2] = nullptr;  // It's ok (in fact necessary for oil derricks) to capture special buildings.
	}

	Bool isPlaceExplosive = false;
	if (spTemplate->getSpecialPowerType() == SPECIAL_TIMED_CHARGES) isPlaceExplosive = true;
	if (spTemplate->getSpecialPowerType() == SPECIAL_TANKHUNTER_TNT_ATTACK) isPlaceExplosive = true;


	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me->getPosition(), data->m_scanRange,
		FROM_CENTER_2D, filters, ITER_SORTED_NEAR_TO_FAR );
	MemoryPoolObjectHolder hold(iter);

	Object *bestTarget = nullptr;
	Int			effectivePriority=0;
	Int			actualPriority=0;
	const AttackPriorityInfo *info=nullptr;
	if (me->getAI()) {
		info = me->getAI()->getAttackInfo();
	}

	SpecialPowerModuleInterface *mod = me->getSpecialPowerModule( spTemplate );
	if( mod )
	{
		for( Object *other = iter->first(); other; other = iter->next() )
		{
			if (other->isDisabled() && isBlackLotusVehicleHack) {
				// The hack disables the vehicle, so we don't want to do it again.
				continue;
			}
			if (isCaptureBuilding) {
				if (me->getControllingPlayer() == other->getControllingPlayer()) {
					continue; // no need to capture our own buildings.
				}
				if (me->getRelationship(other) == ALLIES) {
					continue; // It's not polite to capture allies buildings.
				}
			}
			if( TheActionManager->canDoSpecialPowerAtObject( me, other, CMD_FROM_AI, spTemplate, 0 ) )
			{
				if (isPlaceExplosive) {
					Real range = spTemplate->getViewObjectRange();
					// Don't target things near explosives... It's just not a good idea.
					PartitionFilterSamePlayer filterPlayer( me->getControllingPlayer() );	// Look for our own mines.
					PartitionFilterAcceptByKindOf filterKind(MAKE_KINDOF_MASK(KINDOF_MINE), KINDOFMASK_NONE);
					PartitionFilter *filters[] = { &filterKind, &filterPlayer, nullptr };
					Object *mine = ThePartitionManager->getClosestObject( other, range, FROM_BOUNDINGSPHERE_2D, filters );// could be null. this is ok.
					if (mine) {
						continue;
					}
				}
				Real distSqr = ThePartitionManager->getDistanceSquared(me, other, FROM_BOUNDINGSPHERE_2D);
				Real dist = WWMath::SqrtOrigin(distSqr);
				Int curPriority = data->m_scanRange - dist;
				if (info) curPriority = info->getPriority(other->getTemplate());
				if (curPriority == 0)
					continue; // don't attack 0 priority targets.
				Int modifier = dist/TheAI->getAiData()->m_attackPriorityDistanceModifier;
				Int modPriority = curPriority-modifier;
				if (modPriority < 1)
					modPriority = 1;
				if (modPriority > effectivePriority)
				{
					effectivePriority = modPriority;
					actualPriority = curPriority;
					bestTarget = other;
				}
				if (modPriority == effectivePriority && curPriority > actualPriority)
				{
					effectivePriority = modPriority;
					actualPriority = curPriority;
					bestTarget = other;
				}

			}
		}
	}

	return bestTarget;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void CommandButtonHuntUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void CommandButtonHuntUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// command button name
	xfer->xferAsciiString( &m_commandButtonName );

	// command button pointer (on loading only)
	if( xfer->getXferMode() == XFER_LOAD )
	{

		// initialize to no command button
		m_commandButton = nullptr;

		// find command button pointer if name is present
		if( m_commandButtonName.isEmpty() == FALSE )
		{
			Object *us = getObject();
			const CommandSet *commandSet = TheControlBar->findCommandSet( us->getCommandSetString() );

			if( commandSet )
			{
				const CommandButton *button;

				for( Int i = 0; i < MAX_COMMANDS_PER_SET; i++ )
				{

					button = commandSet->getCommandButton(i);
					if( button && button->getName() == m_commandButtonName )
					{

						m_commandButton = button;
						break;  // exit for, i

					}

				}

			}

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void CommandButtonHuntUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
