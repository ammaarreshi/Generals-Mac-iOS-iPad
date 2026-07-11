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

// InGameUI.cpp ///////////////////////////////////////////////////////////////////////////////////
// Implementation of in-game user interface singleton
// Author: Michael S. Booth, March 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <stdio.h>

#define DEFINE_SHADOW_NAMES

#include "Common/ActionManager.h"
#include "Common/FramePacer.h"
#include "Common/GameAudio.h"
#include "Common/GameType.h"
#include "Common/GameUtility.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/PerfTimer.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Radar.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/BuildAssistant.h"
#include "Common/Recorder.h"
#include "Common/SpecialPower.h"

#include "GameClient/Anim2D.h"
#include "GameClient/ControlBar.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Diplomacy.h"
#include "GameClient/Eva.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameWindowGlobal.h"
#include "GameClient/GameWindowID.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/InGameUI.h"
#include "GameClient/VideoPlayer.h"
#include "GameClient/Mouse.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/View.h"
#include "GameClient/TerrainVisual.h"
#include "GameClient/Display.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/LookAtXlat.h"
#include "GameClient/SelectionXlat.h"
#include "GameClient/Shadow.h"
#include "GameClient/GlobalLanguage.h"

#include "GameLogic/AIGuard.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Object.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/Module/MobMemberSlavedUpdate.h"//ML

#include "GameNetwork/GameInfo.h"
#include "GameNetwork/NetworkInterface.h"

#include "Common/UnitTimings.h" //Contains the DO_UNIT_TIMINGS define jba.



// ------------------------------------------------------------------------------------------------
static const RGBColor IllegalBuildColor = { 1.0, 0.0, 0.0 };

// ------------------------------------------------------------------------------------------------
static UnicodeString formatMoneyValue(UnsignedInt amount)
{
	UnicodeString result;
	if (amount >= 100000)
	{
		result.format(L"%uk", amount / 1000);
	}
	else
	{
		result.format(L"%u", amount);
	}
	return result;
}

static UnicodeString formatIncomeValue(UnsignedInt cashPerMin)
{
	UnicodeString result;
	if (cashPerMin >= 10000)
	{
		result.format(L"%uk", cashPerMin / 1000);
	}
	else if (cashPerMin >= 1000)
	{
		result.format(L"%u", (cashPerMin / 100) * 100);
	}
	else
	{
		result.format(L"%u", (cashPerMin / 10) * 10);
	}
	return result;
}

//-------------------------------------------------------------------------------------------------
/// The InGameUI singleton instance.
InGameUI *TheInGameUI = nullptr;

GameWindow *m_replayWindow = nullptr;

// ------------------------------------------------------------------------------------------------
struct KindOfSelectionData
{
	KindOfMaskType m_mustbeSet;
	KindOfMaskType m_mustbeClear;

	DrawableList newlySelectedDrawables;
};
// ------------------------------------------------------------------------------------------------
static Bool kindOfUnitSelection( Drawable *test, void *userData )
{
	KindOfSelectionData *data = (KindOfSelectionData *) userData;

	if( test )
	{
		const Object *object = test->getObject();
		// Only things with objects can be selected, and the code below isn't
		// safe unless you've verified that there is a valid object.
		if (!object)
			return FALSE;

		Bool isKindOfMatch = object->isKindOfMulti(data->m_mustbeSet, data->m_mustbeClear);

		// only select objects if not already selected
		if( object && isKindOfMatch
					&& object->isLocallyControlled()
					&& !object->isContained()
					&& !object->getDrawable()->isSelected()
					&& !object->isEffectivelyDead()
					&& object->isMassSelectable()
					&& !object->isOffMap()
				)
		{
			// enforce optional unit cap
			if (TheInGameUI->getMaxSelectCount() > 0 && TheInGameUI->getSelectCount() >= TheInGameUI->getMaxSelectCount())
			{
				if ( !TheInGameUI->getDisplayedMaxWarning() )
				{
					TheInGameUI->setDisplayedMaxWarning( TRUE );
					UnicodeString msg;
					msg.format(TheGameText->fetch("GUI:MaxSelectionSize").str(), TheInGameUI->getMaxSelectCount());
					TheInGameUI->message(msg);
				}
			}
			else
			{
				TheInGameUI->selectDrawable( test );
				TheInGameUI->setDisplayedMaxWarning( FALSE );
				data->newlySelectedDrawables.push_back(test);
				return TRUE;
			}
		}
	}
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
struct MatchingUnitSelectionData
{
	const ThingTemplate *templateToSelect;
	DrawableList newlySelectedDrawables;
	Bool isCarBomb;
};
// ------------------------------------------------------------------------------------------------
static Bool similarUnitSelection( Drawable *test, void *userData )
{
	MatchingUnitSelectionData *data = (MatchingUnitSelectionData *) userData;
	const ThingTemplate *selectedType = data->templateToSelect;

	if( test )
	{
		const Object *object = test->getObject();
		// Only things with objects can be selected, and the code below isn't
		// safe unless you've verified that there is a valid object.
		if (!object)
			return FALSE;

		Bool isEquivalent = object->getTemplate()->isEquivalentTo( selectedType );
		if( data->isCarBomb && !isEquivalent && object->testStatus( OBJECT_STATUS_IS_CARBOMB ) )
		{
			isEquivalent = TRUE;
		}

		// only select objects if not already selected
		if( object && isEquivalent
			  && object->isLocallyControlled()
				&& !object->isContained()
				&& !( object->getDrawable()->isSelected() )
				&& object->isMassSelectable() // And only if they can be multiply selected. (otherwise the drawable will be, but the object will not be)
				&& !object->isOffMap()
				)
		{
			// enforce optional unit cap
			if (TheInGameUI->getMaxSelectCount() > 0 && TheInGameUI->getSelectCount() >= TheInGameUI->getMaxSelectCount())
			{
				if ( !TheInGameUI->getDisplayedMaxWarning() )
				{
					TheInGameUI->setDisplayedMaxWarning( TRUE );
					UnicodeString msg;
					msg.format(TheGameText->fetch("GUI:MaxSelectionSize").str(), TheInGameUI->getMaxSelectCount());
					TheInGameUI->message(msg);
				}
			}
			else
			{
				TheInGameUI->selectDrawable( test );
				TheInGameUI->setDisplayedMaxWarning( FALSE );
				data->newlySelectedDrawables.push_back(test);
				return TRUE;
			}
		}
	}
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void showReplayControls()
{
	if (m_replayWindow)
	{
		Bool show = TheGameLogic->isInReplayGame();
		m_replayWindow->winHide(!show);
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void hideReplayControls()
{
	if (m_replayWindow)
	{
		m_replayWindow->winHide(TRUE);
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void toggleReplayControls()
{
	if (m_replayWindow)
	{
		Bool show = TheGameLogic->isInReplayGame() && m_replayWindow->winIsHidden();
		m_replayWindow->winHide(!show);
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
SuperweaponInfo::SuperweaponInfo(
	ObjectID id,
	UnsignedInt timestamp,
	Bool hiddenByScript,
	Bool hiddenByScience,
	Bool ready,
  Bool evaReadyPlayed,
	const AsciiString& superweaponNormalFont,
	Int superweaponNormalPointSize,
	Bool superweaponNormalBold,
	Color c,
	const SpecialPowerTemplate* spt
) :
	m_id(id),
	m_timestamp(timestamp),
	m_hiddenByScript(hiddenByScript),
	m_hiddenByScience(hiddenByScience),
	m_ready(ready),
  m_evaReadyPlayed( evaReadyPlayed ),
	m_forceUpdateText(false),
	m_nameDisplayString(nullptr),
	m_timeDisplayString(nullptr),
	m_color(c),
	m_powerTemplate(spt)
{
	m_nameDisplayString = TheDisplayStringManager->newDisplayString();
	m_nameDisplayString->reset();
	m_nameDisplayString->setText( UnicodeString::TheEmptyString );

	m_timeDisplayString = TheDisplayStringManager->newDisplayString();
	m_timeDisplayString->reset();
	m_timeDisplayString->setText( UnicodeString::TheEmptyString );

	setFont( superweaponNormalFont, superweaponNormalPointSize, superweaponNormalBold );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
SuperweaponInfo::~SuperweaponInfo()
{
	if (m_nameDisplayString)
		TheDisplayStringManager->freeDisplayString( m_nameDisplayString );
	m_nameDisplayString = nullptr;

	if (m_timeDisplayString)
		TheDisplayStringManager->freeDisplayString( m_timeDisplayString );
	m_timeDisplayString = nullptr;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void SuperweaponInfo::setFont(const AsciiString& superweaponNormalFont, Int superweaponNormalPointSize, Bool superweaponNormalBold)
{
	m_nameDisplayString->setFont( TheFontLibrary->getFont( superweaponNormalFont,
		TheGlobalLanguageData->adjustFontSize(superweaponNormalPointSize), superweaponNormalBold ) );
	m_timeDisplayString->setFont( TheFontLibrary->getFont( superweaponNormalFont,
		TheGlobalLanguageData->adjustFontSize(superweaponNormalPointSize), superweaponNormalBold ) );
}

// ------------------------------------------------------------------------------------------------
void SuperweaponInfo::setText(const UnicodeString& name, const UnicodeString& time)
{
	m_nameDisplayString->setText(name);
	m_timeDisplayString->setText(time);
}

// ------------------------------------------------------------------------------------------------
void SuperweaponInfo::drawName(Int x, Int y, Color color, Color dropColor)
{
	if (color == 0)
		color = m_color;
 	m_nameDisplayString->draw(x - m_nameDisplayString->getWidth(), y, color, dropColor);
}

// ------------------------------------------------------------------------------------------------
void SuperweaponInfo::drawTime(Int x, Int y, Color color, Color dropColor)
{
	if (color == 0)
		color = m_color;
 	m_timeDisplayString->draw(x, y, color, dropColor);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Real SuperweaponInfo::getHeight() const
{
	return m_nameDisplayString->getFont()->height;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void InGameUI::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: Save NamedTimers, but not specifically their Info structs.  We'll recreate them.
  * 3: Added m_evaReadyPlayed boolean to transfer
*/
// ------------------------------------------------------------------------------------------------
void InGameUI::xfer( Xfer *xfer )
{
	// version
	const XferVersion currentVersion = 3;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	if( version >= 2 )
	{
		// Saving the named timer infos and their friends so we get script timers back after we load
		xfer->xferInt(&m_namedTimerLastFlashFrame);
		xfer->xferBool(&m_namedTimerUsedFlashColor);
		xfer->xferBool(&m_showNamedTimers);

		// For the timers themselves, all I need to save is the things that are used in the call to addNamedTimer.
		// It is okay to do this, because SuperweaponInfos pushes things on to a map; addNamedTimer is just a more
		// organized way to push things on the namedTimer Map.
		// addNamedTimer needs (const AsciiString& timerName, const UnicodeString& text, Bool isCountdown)
		if (xfer->getXferMode() == XFER_SAVE)
		{
			Int timerCount = m_namedTimers.size();
			xfer->xferInt( &timerCount );
			for( NamedTimerMapIt timerIter = m_namedTimers.begin(); timerIter != m_namedTimers.end(); ++timerIter )
			{
				xfer->xferAsciiString( &(timerIter->second->m_timerName) );
				xfer->xferUnicodeString( &(timerIter->second->timerText) );
				xfer->xferBool( &(timerIter->second->isCountdown) );
			}
		}
		else // iz a Load
		{
			Int timerCount;
			xfer->xferInt( &timerCount );
			for( Int timerIndex = 0; timerIndex < timerCount; ++timerIndex )
			{
				AsciiString timerName;
				UnicodeString timerText;
				Bool isCountdown;
				xfer->xferAsciiString( &timerName );
				xfer->xferUnicodeString( &timerText );
				xfer->xferBool( &isCountdown );

				addNamedTimer( timerName, timerText, isCountdown );
			}
		}
	}

	xfer->xferBool(&m_superweaponHiddenByScript);
	//xfer->xferBool(&m_inputEnabled);	// no, don't save this yet. somewhat problematic.

	if (xfer->getXferMode() == XFER_SAVE)
	{
		for (Int playerIndex = 0; playerIndex < MAX_PLAYER_COUNT; ++playerIndex)
		{
			for (SuperweaponMap::iterator mapIt = m_superweapons[playerIndex].begin(); mapIt != m_superweapons[playerIndex].end(); ++mapIt)
			{
				AsciiString powerName = mapIt->first;
				SuperweaponList& swList = mapIt->second;
				for (SuperweaponList::iterator listIt = swList.begin(); listIt != swList.end(); ++listIt)
				{
					SuperweaponInfo* swInfo = *listIt;

					// since this list tends to be somewhat sparse, we write stuff out pretty explicitly.
					xfer->xferInt(&playerIndex);

					AsciiString templateName = swInfo->getSpecialPowerTemplate()->getName();

					xfer->xferAsciiString(&templateName);
					xfer->xferAsciiString(&powerName);
					xfer->xferObjectID(&swInfo->m_id);
					xfer->xferUnsignedInt(&swInfo->m_timestamp);
					xfer->xferBool(&swInfo->m_hiddenByScript);
					xfer->xferBool(&swInfo->m_hiddenByScience);
					xfer->xferBool(&swInfo->m_ready);
          if ( currentVersion >= 3 )
          {
            xfer->xferBool( &swInfo->m_evaReadyPlayed );
          }
				}
			}
		}
		Int noMorePlayers = -1;		// our "done" sentinel
		xfer->xferInt(&noMorePlayers);
	}
	else if (xfer->getXferMode() == XFER_LOAD)
	{
		for (;;)
		{
			Int playerIndex;
			xfer->xferInt(&playerIndex);

			if (playerIndex == -1)
			{
				break;	// our "done" sentinel
			}
			else if (playerIndex < 0 || playerIndex >= MAX_PLAYER_COUNT)
			{
				DEBUG_CRASH(("SWInfo bad plyrindex"));
				throw INI_INVALID_DATA;
			}

			AsciiString templateName;
			xfer->xferAsciiString(&templateName);
			const SpecialPowerTemplate* powerTemplate = TheSpecialPowerStore->findSpecialPowerTemplate(templateName);
			if (powerTemplate == nullptr)
			{
				DEBUG_CRASH(("power %s not found",templateName.str()));
				throw INI_INVALID_DATA;
			}

			AsciiString powerName;
			ObjectID id;
			UnsignedInt timestamp;
			Bool hiddenByScript, hiddenByScience, ready, evaReadyPlayed;

			xfer->xferAsciiString(&powerName);
			xfer->xferObjectID(&id);
			xfer->xferUnsignedInt(&timestamp);
			xfer->xferBool(&hiddenByScript);
			xfer->xferBool(&hiddenByScience);
			xfer->xferBool(&ready);
      if ( currentVersion >= 3 )
      {
        xfer->xferBool( &evaReadyPlayed );
      }
      else
      {
        evaReadyPlayed = ready;
      }

			// srj sez: due to order-of-operation stuff, sometimes these will already exist,
			// sometimes not. not sure why. so handle both cases.
			SuperweaponInfo* swInfo = findSWInfo(playerIndex, powerName, id, powerTemplate);
			if (swInfo == nullptr)
			{
				const Player* player = ThePlayerList->getNthPlayer(playerIndex);
				swInfo = newInstance(SuperweaponInfo)(
					id,
					timestamp,
					hiddenByScript,
					hiddenByScience,
					ready,
          evaReadyPlayed,
					m_superweaponNormalFont,
					m_superweaponNormalPointSize,
					m_superweaponNormalBold,
					player->getPlayerColor(),
					powerTemplate);
				m_superweapons[playerIndex][powerName].push_back(swInfo);
			}
			else
			{
				// swInfo->m_id = id;	// redundant, already matches
				swInfo->m_timestamp = timestamp;
				swInfo->m_hiddenByScript = hiddenByScript;
				swInfo->m_hiddenByScience = hiddenByScience;
				swInfo->m_ready = ready;
        swInfo->m_evaReadyPlayed = evaReadyPlayed;
			}
			swInfo->m_forceUpdateText = true;

		}
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void InGameUI::loadPostProcess()
{

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::setMouseCursor(Mouse::MouseCursor c)
{
	if (!TheMouse)
		return;

	TheMouse->setCursor(c);

	if (m_mouseMode == MOUSEMODE_GUI_COMMAND && c != Mouse::ARROW && c != Mouse::SCROLL)
		m_mouseModeCursor = c;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
SuperweaponInfo* InGameUI::findSWInfo(Int playerIndex, const AsciiString& powerName, ObjectID id, const SpecialPowerTemplate *powerTemplate)
{
	SuperweaponMap::iterator mapIt = m_superweapons[playerIndex].find(powerName);
	if (mapIt != m_superweapons[playerIndex].end())
	{
		for (SuperweaponList::iterator listIt = mapIt->second.begin(); listIt != mapIt->second.end(); ++listIt)
		{
			if ((*listIt)->m_id == id)
			{
				return *listIt;
			}
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::addSuperweapon(Int playerIndex, const AsciiString& powerName, ObjectID id, const SpecialPowerTemplate *powerTemplate)
{
	if (powerTemplate == nullptr)
		return;

	// srj sez: don't allow adding the same superweapon more than once. it can happen. not sure how. (srj)
	SuperweaponInfo* swInfo = findSWInfo(playerIndex, powerName, id, powerTemplate);
	if (swInfo != nullptr)
		return;

	const Player* player = ThePlayerList->getNthPlayer(playerIndex);
	Bool hiddenByScience = (powerTemplate->getRequiredScience() != SCIENCE_INVALID) && (player->hasScience(powerTemplate->getRequiredScience()) == false);

#ifndef DO_UNIT_TIMINGS
  DEBUG_LOG(("Adding superweapon UI timer"));
#endif
	SuperweaponInfo *info = newInstance(SuperweaponInfo)(
					id,
					-1,			// timestamp
					FALSE,	// hiddenByScript
					hiddenByScience,//Aaayeeee! This is meaningless and just clogs up the works, sez srj, nuke or repair or SHIP WITH(tm), ASAP
													// THe trouble is: There is no mechanism to clear this bit when the science is granted, thus,
													// the timer never, ever, ever get drawn.... unless the owning object is post-science constructed.
					FALSE,	// ready
          FALSE,  // evaReadyPlayed
					m_superweaponNormalFont,
					m_superweaponNormalPointSize,
					m_superweaponNormalBold,
					player->getPlayerColor(),
					powerTemplate);

	m_superweapons[playerIndex][powerName].push_back(info);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool InGameUI::removeSuperweapon(Int playerIndex, const AsciiString& powerName, ObjectID id, const SpecialPowerTemplate *powerTemplate)
{
	DEBUG_LOG(("Removing superweapon UI timer"));
	SuperweaponMap::iterator mapIt = m_superweapons[playerIndex].find(powerName);
	if (mapIt != m_superweapons[playerIndex].end())
	{
		SuperweaponList& swList = mapIt->second;
		for (SuperweaponList::iterator listIt = swList.begin(); listIt != swList.end(); ++listIt)
		{
			if ((*listIt)->m_id == id)
			{
				SuperweaponInfo *info = *listIt;
				swList.erase(listIt);
				deleteInstance(info);
				if (swList.empty())
				{
					m_superweapons[playerIndex].erase(mapIt);
				}
				return TRUE;
			}
		}
	}

	return FALSE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::objectChangedTeam(const Object *obj, Int oldPlayerIndex, Int newPlayerIndex)
{
	// if we already had it listed, remove and re-add it
	if (obj && oldPlayerIndex >= 0 && newPlayerIndex >= 0)
	{
		ObjectID id = obj->getID();
		AsciiString powerName;
		for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
		{
			SpecialPowerModuleInterface* sp = (*m)->getSpecialPower();
			if (!sp)
				continue;

			const SpecialPowerTemplate *powerTemplate = sp->getSpecialPowerTemplate();
			powerName = powerTemplate->getName();

			SuperweaponMap::iterator mapIt = m_superweapons[oldPlayerIndex].find(powerName);
			Bool found = false;
			if (mapIt != m_superweapons[oldPlayerIndex].end())
			{
				for (SuperweaponList::iterator listIt = mapIt->second.begin(); listIt != mapIt->second.end(); ++listIt)
				{
					if ((*listIt)->m_id == id)
					{
						removeSuperweapon(oldPlayerIndex, powerName, id, powerTemplate);
						addSuperweapon(newPlayerIndex, powerName, id, powerTemplate);
						found = true;
						break;
					}
				}
			}
			if (!found)
			{
				if( TheGameLogic->getFrame() == 0 && !obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) &&
					obj->isKindOf( KINDOF_COMMANDCENTER ) == FALSE )
					addSuperweapon(newPlayerIndex, powerName, id, powerTemplate);
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::hideObjectSuperweaponDisplayByScript(const Object *obj)
{
	ObjectID objID = obj->getID();
	for (Int playerIndex = 0; playerIndex < MAX_PLAYER_COUNT; ++playerIndex)
	{
		for (SuperweaponMap::iterator mapIt = m_superweapons[playerIndex].begin(); mapIt != m_superweapons[playerIndex].end(); ++mapIt)
		{
			for (SuperweaponList::iterator listIt = mapIt->second.begin(); listIt != mapIt->second.end(); ++listIt)
			{
				if ((*listIt)->m_id == objID)
				{
					(*listIt)->m_hiddenByScript = TRUE;
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::showObjectSuperweaponDisplayByScript(const Object *obj)
{
	ObjectID objID = obj->getID();
	for (Int playerIndex = 0; playerIndex < MAX_PLAYER_COUNT; ++playerIndex)
	{
		for (SuperweaponMap::iterator mapIt = m_superweapons[playerIndex].begin(); mapIt != m_superweapons[playerIndex].end(); ++mapIt)
		{
			for (SuperweaponList::iterator listIt = mapIt->second.begin(); listIt != mapIt->second.end(); ++listIt)
			{
				if ((*listIt)->m_id == objID)
				{
					(*listIt)->m_hiddenByScript = FALSE;
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::setSuperweaponDisplayEnabledByScript(Bool enable)
{
	m_superweaponHiddenByScript = !enable;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool InGameUI::getSuperweaponDisplayEnabledByScript() const
{
	return !m_superweaponHiddenByScript;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::addNamedTimer( const AsciiString& timerName, const UnicodeString& text, Bool isCountdown )
{
	NamedTimerInfo *info = newInstance( NamedTimerInfo );
	info->m_timerName = timerName;
	info->color = m_namedTimerNormalColor;
	info->timerText = text;
	info->displayString = TheDisplayStringManager->newDisplayString();
	info->displayString->reset();
	info->displayString->setFont( TheFontLibrary->getFont( m_namedTimerNormalFont,
		TheGlobalLanguageData->adjustFontSize(m_namedTimerNormalPointSize), m_namedTimerNormalBold ) );
	info->displayString->setText( UnicodeString::TheEmptyString );
	info->timestamp = -1;
	info->isCountdown = isCountdown;

//	GameFont *font = info->displayString->getFont();

	removeNamedTimer(timerName);
	m_namedTimers[timerName] = info;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::removeNamedTimer( const AsciiString& timerName )
{
	NamedTimerMapIt mapIt = m_namedTimers.find(timerName);
	if (mapIt != m_namedTimers.end())
	{
		TheDisplayStringManager->freeDisplayString( mapIt->second->displayString );
		deleteInstance(mapIt->second);
		m_namedTimers.erase(mapIt);
		return;
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::showNamedTimerDisplay( Bool show )
{
	m_showNamedTimers = show;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const FieldParse InGameUI::s_fieldParseTable[] =
{
	{ "MaxSelectionSize",								INI::parseInt,					nullptr,		offsetof( InGameUI, m_maxSelectCount ) },

	{ "MessageColor1",									INI::parseColorInt,			nullptr,		offsetof( InGameUI, m_messageColor1 ) },
	{ "MessageColor2",									INI::parseColorInt,			nullptr,		offsetof( InGameUI, m_messageColor2 ) },
	{ "MessagePosition",								INI::parseICoord2D,			nullptr,		offsetof( InGameUI, m_messagePosition ) },
	{ "MessageFont",										INI::parseAsciiString,	nullptr,		offsetof( InGameUI, m_messageFont ) },
	{ "MessagePointSize",								INI::parseInt,					nullptr,		offsetof( InGameUI, m_messagePointSize ) },
	{ "MessageBold",										INI::parseBool,					nullptr,		offsetof( InGameUI, m_messageBold ) },
	{ "MessageDelayMS",									INI::parseInt,					nullptr,		offsetof( InGameUI, m_messageDelayMS ) },

	{ "MilitaryCaptionColor",						INI::parseRGBAColorInt,	nullptr,		offsetof( InGameUI, m_militaryCaptionColor ) },
	{ "MilitaryCaptionPosition",				INI::parseICoord2D,			nullptr,		offsetof( InGameUI, m_militaryCaptionPosition ) },

	{ "MilitaryCaptionTitleFont",				INI::parseAsciiString,	nullptr,		offsetof( InGameUI, m_militaryCaptionTitleFont ) },
	{ "MilitaryCaptionTitlePointSize",	INI::parseInt,					nullptr,		offsetof( InGameUI, m_militaryCaptionTitlePointSize ) },
	{ "MilitaryCaptionTitleBold",				INI::parseBool,					nullptr,		offsetof( InGameUI, m_militaryCaptionTitleBold ) },

	{ "MilitaryCaptionFont",						INI::parseAsciiString,	nullptr,		offsetof( InGameUI, m_militaryCaptionFont ) },
	{ "MilitaryCaptionPointSize",				INI::parseInt,					nullptr,		offsetof( InGameUI, m_militaryCaptionPointSize ) },
	{ "MilitaryCaptionBold",						INI::parseBool,					nullptr,		offsetof( InGameUI, m_militaryCaptionBold ) },

	{ "MilitaryCaptionRandomizeTyping",	INI::parseBool,					nullptr,		offsetof( InGameUI, m_militaryCaptionRandomizeTyping ) },
	{ "MilitaryCaptionSpeed",						INI::parseInt,					nullptr,		offsetof( InGameUI, m_militaryCaptionSpeed ) },

	{ "MilitaryCaptionPosition",				INI::parseICoord2D,			nullptr,		offsetof( InGameUI, m_militaryCaptionPosition ) },

	{ "SuperweaponCountdownPosition",					INI::parseCoord2D,			nullptr,		offsetof( InGameUI, m_superweaponPosition ) },
	{ "SuperweaponCountdownFlashDuration",		INI::parseDurationReal,	nullptr,		offsetof( InGameUI, m_superweaponFlashDuration ) },
	{ "SuperweaponCountdownFlashColor",				INI::parseColorInt,			nullptr,		offsetof( InGameUI, m_superweaponFlashColor ) },

	{ "SuperweaponCountdownNormalFont",				INI::parseAsciiString,	nullptr,		offsetof( InGameUI, m_superweaponNormalFont ) },
	{ "SuperweaponCountdownNormalPointSize",	INI::parseInt,					nullptr,		offsetof( InGameUI, m_superweaponNormalPointSize ) },
	{ "SuperweaponCountdownNormalBold",				INI::parseBool,					nullptr,		offsetof( InGameUI, m_superweaponNormalBold ) },

	{ "SuperweaponCountdownReadyFont",				INI::parseAsciiString,	nullptr,		offsetof( InGameUI, m_superweaponReadyFont ) },
	{ "SuperweaponCountdownReadyPointSize",		INI::parseInt,					nullptr,		offsetof( InGameUI, m_superweaponReadyPointSize ) },
	{ "SuperweaponCountdownReadyBold",				INI::parseBool,					nullptr,		offsetof( InGameUI, m_superweaponReadyBold ) },

	{ "NamedTimerCountdownPosition",					INI::parseCoord2D,			nullptr,		offsetof( InGameUI, m_namedTimerPosition ) },
	{ "NamedTimerCountdownFlashDuration",			INI::parseDurationReal,	nullptr,		offsetof( InGameUI, m_namedTimerFlashDuration ) },
	{ "NamedTimerCountdownFlashColor",				INI::parseColorInt,			nullptr,		offsetof( InGameUI, m_namedTimerFlashColor ) },

	{ "NamedTimerCountdownNormalFont",				INI::parseAsciiString,	nullptr,		offsetof( InGameUI, m_namedTimerNormalFont ) },
	{ "NamedTimerCountdownNormalPointSize",		INI::parseInt,					nullptr,		offsetof( InGameUI, m_namedTimerNormalPointSize ) },
	{ "NamedTimerCountdownNormalBold",				INI::parseBool,					nullptr,		offsetof( InGameUI, m_namedTimerNormalBold ) },
	{ "NamedTimerCountdownNormalColor",				INI::parseColorInt,			nullptr,		offsetof( InGameUI, m_namedTimerNormalColor ) },

	{ "NamedTimerCountdownReadyFont",					INI::parseAsciiString,	nullptr,		offsetof( InGameUI, m_namedTimerReadyFont ) },
	{ "NamedTimerCountdownReadyPointSize",		INI::parseInt,					nullptr,		offsetof( InGameUI, m_namedTimerReadyPointSize ) },
	{ "NamedTimerCountdownReadyBold",					INI::parseBool,					nullptr,		offsetof( InGameUI, m_namedTimerReadyBold ) },
	{ "NamedTimerCountdownReadyColor",				INI::parseColorInt,			nullptr,		offsetof( InGameUI, m_namedTimerReadyColor ) },

	{ "FloatingTextTimeOut",									INI::parseDurationUnsignedInt,		nullptr,		offsetof( InGameUI, m_floatingTextTimeOut ) },
	{ "FloatingTextMoveUpSpeed",							INI::parseVelocityReal,	nullptr,		offsetof( InGameUI, m_floatingTextMoveUpSpeed ) },
	{ "FloatingTextVanishRate",								INI::parseVelocityReal,	nullptr,		offsetof( InGameUI, m_floatingTextMoveVanishRate ) },

	{ "PopupMessageColor",								INI::parseColorInt,					nullptr,		offsetof( InGameUI, m_popupMessageColor ) },

	{ "DrawableCaptionFont",									INI::parseAsciiString,	nullptr,		offsetof( InGameUI, m_drawableCaptionFont ) },
	{ "DrawableCaptionPointSize",							INI::parseInt,					nullptr,		offsetof( InGameUI, m_drawableCaptionPointSize ) },
	{ "DrawableCaptionBold",									INI::parseBool,					nullptr,		offsetof( InGameUI, m_drawableCaptionBold ) },
	{ "DrawableCaptionColor",									INI::parseColorInt,			nullptr,		offsetof( InGameUI, m_drawableCaptionColor ) },

	{ "DrawRMBScrollAnchor",									INI::parseBool,					nullptr,		offsetof( InGameUI, m_drawRMBScrollAnchor ) },
	{ "MoveRMBScrollAnchor",									INI::parseBool,					nullptr,		offsetof( InGameUI, m_moveRMBScrollAnchor ) },

	{ "AttackDamageAreaRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[RADIUSCURSOR_ATTACK_DAMAGE_AREA] ) },
	{ "AttackScatterAreaRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[RADIUSCURSOR_ATTACK_SCATTER_AREA] ) },
	{ "AttackContinueAreaRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[RADIUSCURSOR_ATTACK_CONTINUE_AREA] ) },
	{ "FriendlySpecialPowerRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[RADIUSCURSOR_FRIENDLY_SPECIALPOWER] ) },
	{ "OffensiveSpecialPowerRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[RADIUSCURSOR_OFFENSIVE_SPECIALPOWER] ) },
	{ "SuperweaponScatterAreaRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[RADIUSCURSOR_SUPERWEAPON_SCATTER_AREA] ) },

	{ "GuardAreaRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[RADIUSCURSOR_GUARD_AREA] ) },
	{ "EmergencyRepairRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[RADIUSCURSOR_EMERGENCY_REPAIR] ) },

	{ "ParticleCannonRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_PARTICLECANNON] ) },
	{ "A10StrikeRadiusCursor",			RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_A10STRIKE] ) },
	{ "CarpetBombRadiusCursor",			RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_CARPETBOMB] ) },
	{ "DaisyCutterRadiusCursor",		RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_DAISYCUTTER] ) },
	{ "ParadropRadiusCursor",				RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_PARADROP] ) },
	{ "SpySatelliteRadiusCursor",		RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_SPYSATELLITE] ) },
	{ "SpectreGunshipRadiusCursor",	RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_SPECTREGUNSHIP] ) },
	{ "HelixNapalmBombRadiusCursor",RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_HELIX_NAPALM_BOMB] ) },

	{ "NuclearMissileRadiusCursor", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_NUCLEARMISSILE] ) },
	{ "EMPPulseRadiusCursor",		  	RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_EMPPULSE] ) },
	{ "ArtilleryRadiusCursor",		  RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_ARTILLERYBARRAGE] ) },
	{ "FrenzyRadiusCursor",				  RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_FRENZY] ) },
	{ "NapalmStrikeRadiusCursor",		RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_NAPALMSTRIKE] ) },
	{ "ClusterMinesRadiusCursor",		RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_CLUSTERMINES] ) },

	{ "ScudStormRadiusCursor",			RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_SCUDSTORM] ) },
	{ "AnthraxBombRadiusCursor",		RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_ANTHRAXBOMB] ) },
	{ "AmbushRadiusCursor",					RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_AMBUSH] ) },
	{ "RadarRadiusCursor",					RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[	RADIUSCURSOR_RADAR] ) },
	{ "SpyDroneRadiusCursor",				RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[ RADIUSCURSOR_SPYDRONE] ) },

	{ "ClearMinesRadiusCursor",			RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[ RADIUSCURSOR_CLEARMINES] ) },
	{ "AmbulanceRadiusCursor",			RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( InGameUI, m_radiusCursors[ RADIUSCURSOR_AMBULANCE] ) },

	// TheSuperHackers @info ui enhancement configuration
	{ "NetworkLatencyFont",      INI::parseAsciiString,  nullptr, offsetof( InGameUI, m_networkLatencyFont ) },
	{ "NetworkLatencyBold",      INI::parseBool,         nullptr, offsetof( InGameUI, m_networkLatencyBold ) },
	{ "NetworkLatencyPosition",  INI::parseCoord2D,      nullptr, offsetof( InGameUI, m_networkLatencyPosition ) },
	{ "NetworkLatencyColor",     INI::parseColorInt,     nullptr, offsetof( InGameUI, m_networkLatencyColor ) },
	{ "NetworkLatencyDropColor", INI::parseColorInt,     nullptr, offsetof( InGameUI, m_networkLatencyDropColor ) },

	{ "RenderFpsFont",          INI::parseAsciiString,  nullptr, offsetof( InGameUI, m_renderFpsFont ) },
	{ "RenderFpsBold",          INI::parseBool,         nullptr, offsetof( InGameUI, m_renderFpsBold ) },
	{ "RenderFpsPosition",      INI::parseCoord2D,      nullptr, offsetof( InGameUI, m_renderFpsPosition ) },
	{ "RenderFpsColor",         INI::parseColorInt,     nullptr, offsetof( InGameUI, m_renderFpsColor ) },
	{ "RenderFpsLimitColor",    INI::parseColorInt,     nullptr, offsetof( InGameUI, m_renderFpsLimitColor ) },
	{ "RenderFpsDropColor",     INI::parseColorInt,     nullptr, offsetof( InGameUI, m_renderFpsDropColor ) },
	{ "RenderFpsRefreshMs",     INI::parseUnsignedInt,  nullptr, offsetof( InGameUI, m_renderFpsRefreshMs ) },

	{ "SystemTimeFont",         INI::parseAsciiString,  nullptr, offsetof( InGameUI, m_systemTimeFont ) },
	{ "SystemTimeBold",         INI::parseBool,         nullptr, offsetof( InGameUI, m_systemTimeBold ) },
	{ "SystemTimePosition",     INI::parseCoord2D,      nullptr, offsetof( InGameUI, m_systemTimePosition ) },
	{ "SystemTimeColor",        INI::parseColorInt,     nullptr, offsetof( InGameUI, m_systemTimeColor ) },
	{ "SystemTimeDropColor",    INI::parseColorInt,     nullptr, offsetof( InGameUI, m_systemTimeDropColor ) },

	{ "GameTimeFont",           INI::parseAsciiString,  nullptr, offsetof( InGameUI, m_gameTimeFont ) },
	{ "GameTimeBold",           INI::parseBool,         nullptr, offsetof( InGameUI, m_gameTimeBold ) },
	{ "GameTimePosition",       INI::parseCoord2D,      nullptr, offsetof( InGameUI, m_gameTimePosition ) },
	{ "GameTimeColor",          INI::parseColorInt,     nullptr, offsetof( InGameUI, m_gameTimeColor ) },
	{ "GameTimeDropColor",      INI::parseColorInt,     nullptr, offsetof( InGameUI, m_gameTimeDropColor ) },

	{ "PlayerInfoListFont",               INI::parseAsciiString,   nullptr, offsetof( InGameUI, m_playerInfoListFont ) },
	{ "PlayerInfoListBold",               INI::parseBool,          nullptr, offsetof( InGameUI, m_playerInfoListBold ) },
	{ "PlayerInfoListPosition",           INI::parseCoord2D,       nullptr, offsetof( InGameUI, m_playerInfoListPosition ) },
	{ "PlayerInfoListLabelColor",         INI::parseColorInt,      nullptr, offsetof( InGameUI, m_playerInfoListLabelColor ) },
	{ "PlayerInfoListValueColor",         INI::parseColorInt,      nullptr, offsetof( InGameUI, m_playerInfoListValueColor ) },
	{ "PlayerInfoListDropColor",          INI::parseColorInt,      nullptr, offsetof( InGameUI, m_playerInfoListDropColor ) },
	{ "PlayerInfoListBackgroundAlpha",    INI::parseUnsignedInt  , nullptr, offsetof( InGameUI, m_playerInfoListBackgroundAlpha ) },

	{ nullptr,													nullptr,										nullptr,		0 }
};

//-------------------------------------------------------------------------------------------------
/** Parse MouseCursor entry */
//-------------------------------------------------------------------------------------------------
void INI::parseInGameUIDefinition( INI* ini )
{
	if( TheInGameUI )
	{
		// parse the ini weapon definition
		ini->initFromINI( TheInGameUI, TheInGameUI->getFieldParse() );
	}
}

//-------------------------------------------------------------------------------------------------
namespace
{
	// helpers for inline counters
	constexpr const Int kHudAnchorX = 3;
	constexpr const Int kHudAnchorY = -1;
	constexpr const Int kHudGapPx = 6;
	inline Bool isAtHudAnchorPos(const Coord2D &p) { return p.x == kHudAnchorX && p.y == kHudAnchorY; }
}

//-------------------------------------------------------------------------------------------------
InGameUI::PlayerInfoList::PlayerInfoList()
{
	std::fill(labels, labels + ARRAY_SIZE(labels), static_cast<DisplayString*>(nullptr));
	for (Int column = 0; column < ARRAY_SIZE(values); ++column)
	{
		std::fill(values[column], values[column] + ARRAY_SIZE(values[column]), static_cast<DisplayString*>(nullptr));
	}
}

//-------------------------------------------------------------------------------------------------
void InGameUI::PlayerInfoList::init(const AsciiString &fontName, Int pointSize, Bool bold)
{
	Int i;
	GameFont *listFont = TheWindowManager->winFindFont(fontName, pointSize, bold);

	for (i = 0; i < ARRAY_SIZE(labels); ++i)
	{
		if (!labels[i])
		{
			labels[i] = TheDisplayStringManager->newDisplayString();
		}
		labels[i]->setFont(listFont);
	}

	for (i = 0; i < ARRAY_SIZE(values); ++i)
	{
		for (Int j = 0; j < MAX_PLAYER_COUNT; ++j)
		{
			if (!values[i][j])
			{
				values[i][j] = TheDisplayStringManager->newDisplayString();
			}
			values[i][j]->setFont(listFont);
		}
	}

	lastValues = LastValues();

	labels[LabelType_Team]->setText(TheGameText->FETCH_OR_SUBSTITUTE_FORMAT("GUI:PlayerInfoListLabelTeam", L"T"));
	labels[LabelType_Money]->setText(TheGameText->FETCH_OR_SUBSTITUTE_FORMAT("GUI:PlayerInfoListLabelMoney", L"$"));
	labels[LabelType_Rank]->setText(TheGameText->FETCH_OR_SUBSTITUTE_FORMAT("GUI:PlayerInfoListLabelRank", L"*"));
	labels[LabelType_Xp]->setText(TheGameText->FETCH_OR_SUBSTITUTE_FORMAT("GUI:PlayerInfoListLabelXp", L"XP"));
}

//-------------------------------------------------------------------------------------------------
void InGameUI::PlayerInfoList::clear()
{
	Int i;

	for (i = 0; i < ARRAY_SIZE(labels); ++i)
	{
		TheDisplayStringManager->freeDisplayString(labels[i]);
		labels[i] = nullptr;
	}

	for (i = 0; i < ARRAY_SIZE(values); ++i)
	{
		for (Int j = 0; j < MAX_PLAYER_COUNT; ++j)
		{
			TheDisplayStringManager->freeDisplayString(values[i][j]);
			values[i][j] = nullptr;
		}
	}

	lastValues = LastValues();
}

//-------------------------------------------------------------------------------------------------
InGameUI::PlayerInfoList::LastValues::LastValues()
{
	for (Int column = 0; column < ARRAY_SIZE(values); ++column)
	{
		std::fill(values[column], values[column] + ARRAY_SIZE(values[column]), ~0u);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
InGameUI::InGameUI()
{
	Int i;


  m_inputEnabled = true;
	m_isDragSelecting = false;
	m_nextMoveHint = 0;
	m_selectCount = 0;
	m_frameSelectionChanged = 0;
  m_duringDoubleClickAttackMoveGuardHintTimer = 0;
  m_duringDoubleClickAttackMoveGuardHintStashedPosition.zero();
	m_maxSelectCount = -1;
	m_isScrolling = FALSE;
	m_isSelecting = FALSE;
	m_mouseMode = MOUSEMODE_DEFAULT;
	m_mouseModeCursor = Mouse::ARROW;
	m_mousedOverDrawableID = INVALID_DRAWABLE_ID;

	m_currentlyPlayingMovie.clear();
	m_militarySubtitle = nullptr;
	m_popupMessageData = nullptr;
	m_waypointMode = FALSE;
	m_clientQuiet = FALSE;

	m_messageColor1 = GameMakeColor( 255, 255, 255, 255 );
	m_messageColor2 = GameMakeColor( 180, 180, 180, 255 );
	m_messagePosition.x = 10;
	m_messagePosition.y = 10;
	m_messageFont = "Arial";
	m_messagePointSize = 10;
	m_messageBold = FALSE;
	m_messageDelayMS = 5000;

	m_militaryCaptionColor.red   = 200;
	m_militaryCaptionColor.green = 200;
	m_militaryCaptionColor.blue  = 30;
	m_militaryCaptionColor.alpha = 255;
	m_militaryCaptionPosition.x = 10;
	m_militaryCaptionPosition.y = 380;

	m_militaryCaptionTitleFont = "Courier";
	m_militaryCaptionTitlePointSize = 12;
	m_militaryCaptionTitleBold = TRUE;

	m_militaryCaptionFont = "Courier";
	m_militaryCaptionPointSize = 12;
	m_militaryCaptionBold = FALSE;

	m_militaryCaptionRandomizeTyping = FALSE;
	m_militaryCaptionSpeed = 1;
	m_popupMessageColor = GameMakeColor(255,255,255,255);

	m_tooltipsDisabledUntil = 0;

	// init hint lists
	for( i = 0; i < MAX_MOVE_HINTS; i++ )
	{

		m_moveHint[ i ].pos.zero();
		m_moveHint[ i ].sourceID = 0;
		m_moveHint[ i ].frame = 0;

	}

	for( i = 0; i < MAX_BUILD_PROGRESS; i++ )
	{

		m_buildProgress[ i ].m_thingTemplate = nullptr;
		m_buildProgress[ i ].m_percentComplete = 0.0f;
		m_buildProgress[ i ].m_control = nullptr;

	}

	m_pendingGUICommand = nullptr;

	// allocate an array for the placement icons
	m_placeIcon = NEW Drawable* [ TheGlobalData->m_maxLineBuildObjects ];
	for( i = 0; i < TheGlobalData->m_maxLineBuildObjects; i++ )
		m_placeIcon[ i ] = nullptr;
	m_pendingPlaceType = nullptr;
	m_pendingPlaceSourceObjectID = INVALID_ID;
	m_preventLeftClickDeselectionInAlternateMouseModeForOneClick = FALSE;
	m_placeAnchorStart.x = m_placeAnchorStart.y = 0;
	m_placeAnchorEnd.x = m_placeAnchorEnd.y = 0;
	m_placeAnchorInProgress = FALSE;

	m_videoStream = nullptr;
	m_videoBuffer = nullptr;
	m_cameoVideoStream = nullptr;
	m_cameoVideoBuffer = nullptr;

	// message info
	for( i = 0; i < MAX_UI_MESSAGES; i++ )
	{

		m_uiMessages[ i ].fullText.clear();
		m_uiMessages[ i ].displayString = nullptr;
		m_uiMessages[ i ].timestamp = 0;
		m_uiMessages[ i ].color = 0;

	}

	m_replayWindow = nullptr;
	m_messagesOn = TRUE;

	// TheSuperHackers @info the default font, size and positions of the various counters were chosen based on GenTools implementation
	m_networkLatencyString = nullptr;
	m_networkLatencyFont = "Tahoma";
	m_networkLatencyPointSize = TheGlobalData->m_networkLatencyFontSize;
	m_networkLatencyBold = TRUE;
	m_networkLatencyPosition.x = kHudAnchorX;
	m_networkLatencyPosition.y = kHudAnchorY;
	m_networkLatencyColor = GameMakeColor( 173, 216, 255, 255 );
	m_networkLatencyDropColor = GameMakeColor( 0, 0, 0, 255 );
	m_lastNetworkLatencyFrames = ~0u;

	m_renderFpsString = nullptr;
	m_renderFpsLimitString = nullptr;
	m_renderFpsFont = "Tahoma";
	m_renderFpsPointSize = TheGlobalData->m_renderFpsFontSize;
	m_renderFpsBold = TRUE;
	m_renderFpsPosition.x = kHudAnchorX;
	m_renderFpsPosition.y = kHudAnchorY;
	m_renderFpsColor = GameMakeColor( 255, 255, 0, 255 );
	m_renderFpsLimitColor = GameMakeColor(119, 119, 119, 255);
	m_renderFpsDropColor = GameMakeColor( 0, 0, 0, 255 );
	m_renderFpsRefreshMs = 1000;
	m_lastRenderFps = ~0u;
	m_lastRenderFpsLimit = ~0u;
	m_lastRenderFpsUpdateMs = 0u;

	m_systemTimeString = nullptr;
	m_systemTimeFont = "Tahoma";
	m_systemTimePointSize = TheGlobalData->m_systemTimeFontSize;
	m_systemTimeBold = TRUE;
	m_systemTimePosition.x = kHudAnchorX; // TheSuperHackers @info relative to the left of the screen
	m_systemTimePosition.y = kHudAnchorY;
	m_systemTimeColor = GameMakeColor( 255, 255, 255, 255 );
	m_systemTimeDropColor = GameMakeColor( 0, 0, 0, 255 );

	m_gameTimeString = nullptr;
	m_gameTimeFrameString = nullptr;
	m_gameTimeFont = "Tahoma";
	m_gameTimePointSize = TheGlobalData->m_gameTimeFontSize;
	m_gameTimeBold = TRUE;
	m_gameTimePosition.x = kHudAnchorX; // TheSuperHackers @info relative to the right of the screen
	m_gameTimePosition.y = kHudAnchorY;
	m_gameTimeColor = GameMakeColor( 255, 255, 255, 255 );
	m_gameTimeDropColor = GameMakeColor( 0, 0, 0, 255 );

	m_playerInfoListFont = "Tahoma";
	m_playerInfoListPointSize = TheGlobalData->m_playerInfoListFontSize;
	m_playerInfoListBold = TRUE;
	m_playerInfoListPosition.x = 0.0f;
	m_playerInfoListPosition.y = 0.5f;
	m_playerInfoListLabelColor = GameMakeColor(125, 124, 122, 255);
	m_playerInfoListValueColor = GameMakeColor(253, 251, 251, 255);
	m_playerInfoListDropColor = GameMakeColor(0, 0, 0, 255);
	m_playerInfoListBackgroundAlpha = 170;

	m_superweaponPosition.x = 0.7f;
	m_superweaponPosition.y = 0.7f;
	m_superweaponFlashDuration = 1.0f;
	m_superweaponNormalFont = "Arial";
	m_superweaponNormalPointSize = 10;
	m_superweaponNormalBold = FALSE;
	m_superweaponReadyFont = "Arial";
	m_superweaponReadyPointSize = 10;
	m_superweaponReadyBold = FALSE;

	m_superweaponFlashColor = GameMakeColor(255, 255, 255, 255);
	m_superweaponLastFlashFrame = 0;
	m_superweaponUsedFlashColor = TRUE; // so next one is false
	m_superweaponHiddenByScript = FALSE;

	m_namedTimerPosition.x = 0.05f;
	m_namedTimerPosition.y = 0.7f;
	m_namedTimerFlashDuration = 1.0f;
	m_namedTimerNormalFont = "Arial";
	m_namedTimerNormalPointSize = 10;
	m_namedTimerNormalBold = FALSE;
	m_namedTimerReadyFont = "Arial";
	m_namedTimerReadyPointSize = 10;
	m_namedTimerReadyBold = FALSE;


	m_namedTimerNormalColor	= GameMakeColor(255, 255,   0, 255);
	m_namedTimerReadyColor	= GameMakeColor(255,   0, 255, 255);
	m_namedTimerFlashColor	= GameMakeColor(  0, 255, 255, 255);
	m_namedTimerLastFlashFrame = 0;
	m_namedTimerUsedFlashColor = TRUE; // so next one is false
	m_showNamedTimers = TRUE;

	m_floatingTextTimeOut = DEFAULT_FLOATING_TEXT_TIMEOUT;
	m_floatingTextMoveUpSpeed = 1.0f;
	m_floatingTextMoveVanishRate = 0.1f;

	m_drawableCaptionFont = "Arial";
	m_drawableCaptionPointSize = 10;
	m_drawableCaptionBold = FALSE;
	m_drawableCaptionColor = GameMakeColor(255, 255, 255, 255);

	m_drawRMBScrollAnchor = FALSE;
	m_moveRMBScrollAnchor = FALSE;
	m_displayedMaxWarning = FALSE;

	m_idleWorkerWin = nullptr;
	m_currentIdleWorkerDisplay = -1;

	m_waypointMode			= false;
	m_forceAttackMode		= false;
	m_forceMoveToMode		= false;
	m_attackMoveToMode	= false;
	m_preferSelection		= false;

	m_curRcType = RADIUSCURSOR_NONE;

	m_soloNexusSelectedDrawableID = INVALID_DRAWABLE_ID;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
InGameUI::~InGameUI()
{
	delete TheControlBar;
	TheControlBar = nullptr;

	// free all the display strings if we're
	removeMilitarySubtitle();

	stopMovie();
	stopCameoMovie();

	// remove any build available status
	placeBuildAvailable( nullptr, nullptr );
	setRadiusCursorNone();

	// delete the message resources
	freeMessageResources();

	// free custom ui strings
	freeCustomUiResources();

	// delete the array for the drawables
	delete [] m_placeIcon;
	m_placeIcon = nullptr;

	// clear floating text
	clearFloatingText();

	// clear world animations
	clearWorldAnimations();
	resetIdleWorker();
}

//-------------------------------------------------------------------------------------------------
/** Initialize the in game user interface */
//-------------------------------------------------------------------------------------------------
void InGameUI::init()
{
	INI ini;
	ini.loadFileDirectory( "Data\\INI\\InGameUI", INI_LOAD_OVERWRITE, nullptr );

	//override INI values with language localized values:
	if (TheGlobalLanguageData)
	{
		if (TheGlobalLanguageData->m_drawableCaptionFont.name.isNotEmpty())
		{
			m_drawableCaptionFont = TheGlobalLanguageData->m_drawableCaptionFont.name;
			m_drawableCaptionPointSize = TheGlobalLanguageData->m_drawableCaptionFont.size;
			m_drawableCaptionBold = TheGlobalLanguageData->m_drawableCaptionFont.bold;
		}

		if (TheGlobalLanguageData->m_messageFont.name.isNotEmpty())
		{
			m_messageFont = TheGlobalLanguageData->m_messageFont.name;
			m_messagePointSize = TheGlobalLanguageData->m_messageFont.size;
			m_messageBold = TheGlobalLanguageData->m_messageFont.bold;
		}

		if (TheGlobalLanguageData->m_militaryCaptionTitleFont.name.isNotEmpty())
		{
			m_militaryCaptionTitleFont = TheGlobalLanguageData->m_militaryCaptionTitleFont.name;
			m_militaryCaptionTitlePointSize = TheGlobalLanguageData->m_militaryCaptionTitleFont.size;
			m_militaryCaptionTitleBold = TheGlobalLanguageData->m_militaryCaptionTitleFont.bold;
		}

		if (TheGlobalLanguageData->m_militaryCaptionFont.name.isNotEmpty())
		{
			m_militaryCaptionFont = TheGlobalLanguageData->m_militaryCaptionFont.name;
			m_militaryCaptionPointSize = TheGlobalLanguageData->m_militaryCaptionFont.size;
			m_militaryCaptionBold = TheGlobalLanguageData->m_militaryCaptionFont.bold;
		}

		if (TheGlobalLanguageData->m_superweaponCountdownNormalFont.name.isNotEmpty())
		{
			m_superweaponNormalFont = TheGlobalLanguageData->m_superweaponCountdownNormalFont.name;
			m_superweaponNormalPointSize = TheGlobalLanguageData->m_superweaponCountdownNormalFont.size;
			m_superweaponNormalBold = TheGlobalLanguageData->m_superweaponCountdownNormalFont.bold;
		}

		if (TheGlobalLanguageData->m_superweaponCountdownReadyFont.name.isNotEmpty())
		{
			m_superweaponReadyFont = TheGlobalLanguageData->m_superweaponCountdownReadyFont.name;
			m_superweaponReadyPointSize = TheGlobalLanguageData->m_superweaponCountdownReadyFont.size;
			m_superweaponReadyBold = TheGlobalLanguageData->m_superweaponCountdownReadyFont.bold;
		}

		if (TheGlobalLanguageData->m_namedTimerCountdownNormalFont.name.isNotEmpty())
		{
			m_namedTimerNormalFont = TheGlobalLanguageData->m_namedTimerCountdownNormalFont.name;
			m_namedTimerNormalPointSize = TheGlobalLanguageData->m_namedTimerCountdownNormalFont.size;
			m_namedTimerNormalBold = TheGlobalLanguageData->m_namedTimerCountdownNormalFont.bold;
		}

		if (TheGlobalLanguageData->m_namedTimerCountdownReadyFont.name.isNotEmpty())
		{
			m_namedTimerReadyFont = TheGlobalLanguageData->m_namedTimerCountdownReadyFont.name;
			m_namedTimerReadyPointSize = TheGlobalLanguageData->m_namedTimerCountdownReadyFont.size;
			m_namedTimerReadyBold = TheGlobalLanguageData->m_namedTimerCountdownReadyFont.bold;
		}
	}

	/**@ todo we used to put in the hint spy translator, but it's difficult
	to order the translators when the code is not centralized so it has
	been moved to where all the other translators are attached in game client */

	// create the tactical view
	TheTacticalView = createView(TheGlobalData->m_headless);
	if (TheTacticalView && TheDisplay)
	{
		TheTacticalView->init();
		TheDisplay->attachView( TheTacticalView );

		// make the tactical display the full screen width and height
		TheTacticalView->setWidth( TheDisplay->getWidth() );
		TheTacticalView->setHeight( TheDisplay->getHeight() );
		// GeneralsX @tweak Copilot 24/03/2026 Preserve cross-platform tactical camera height limits.
		TheTacticalView->setCameraHeightAboveGroundLimitsToDefault();
	}

	/** @todo this may be the wrong place to create the sidebar, but for now
	this is where it lives */
	createControlBar();

	/** @todo This may be the wrong place to create the replay menu, but for now
	this is where it lives */
	createReplayControl();

	// create the command bar
	TheControlBar = NEW ControlBar;
	TheControlBar->init();

	m_windowLayouts.clear();

	m_soloNexusSelectedDrawableID = INVALID_DRAWABLE_ID;

	setDrawRMBScrollAnchor(TheGlobalData->m_drawScrollAnchor);
	setMoveRMBScrollAnchor(TheGlobalData->m_moveScrollAnchor);

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void InGameUI::setRadiusCursor(RadiusCursorType cursorType, const SpecialPowerTemplate* specPowTempl, WeaponSlotType weaponSlot)
{
	if (cursorType == m_curRcType)
		return;

	m_curRadiusCursor.clear();
	m_curRcType = RADIUSCURSOR_NONE;

	if (cursorType == RADIUSCURSOR_NONE)
		return;

	Object* obj = nullptr;
	if( m_pendingGUICommand && m_pendingGUICommand->getCommandType() == GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT )
	{
		if( ThePlayerList && ThePlayerList->getLocalPlayer() && specPowTempl != nullptr )
		{
			obj = ThePlayerList->getLocalPlayer()->findMostReadyShortcutSpecialPowerOfType( specPowTempl->getSpecialPowerType() );
		}
	}
	else
	{
		if (getSelectCount() == 0)
			return;

		Drawable *draw = getFirstSelectedDrawable();
		if (draw == nullptr)
			return;

		obj = draw->getObject();
	}

	if (obj == nullptr)
		return;

	Player* controller = obj->getControllingPlayer();
	if (controller == nullptr)
		return;

	Real radius = 0.0f;
	const Weapon* w = nullptr;
	switch (cursorType)
	{
		// already handled
		//case RADIUSCURSOR_NONE:
		//	return;
		case RADIUSCURSOR_ATTACK_DAMAGE_AREA:
			w = obj->getWeaponInWeaponSlot(weaponSlot);
			radius = w ? w->getPrimaryDamageRadius(obj) : 0.0f;
			break;
		case RADIUSCURSOR_ATTACK_SCATTER_AREA:
			w = obj->getWeaponInWeaponSlot(weaponSlot);
			radius = w ? (w->getScatterRadius() + w->getScatterTargetScalar()) : 0.0f;
			break;
		case RADIUSCURSOR_ATTACK_CONTINUE_AREA:
		case RADIUSCURSOR_CLEARMINES:
			w = obj->getWeaponInWeaponSlot(weaponSlot);
			radius = w ? w->getContinueAttackRange() : 0.0f;
			break;
		case RADIUSCURSOR_GUARD_AREA:
			radius = AIGuardMachine::getStdGuardRange(obj);
			break;
		case RADIUSCURSOR_FRIENDLY_SPECIALPOWER:
		case RADIUSCURSOR_OFFENSIVE_SPECIALPOWER:
		case RADIUSCURSOR_SUPERWEAPON_SCATTER_AREA:
		case RADIUSCURSOR_EMERGENCY_REPAIR:
		case RADIUSCURSOR_PARTICLECANNON:
		case RADIUSCURSOR_A10STRIKE:
		case RADIUSCURSOR_SPECTREGUNSHIP:
    case RADIUSCURSOR_HELIX_NAPALM_BOMB:
		case RADIUSCURSOR_DAISYCUTTER:
		case RADIUSCURSOR_CARPETBOMB:
		case RADIUSCURSOR_PARADROP:
		case RADIUSCURSOR_SPYSATELLITE:
		case RADIUSCURSOR_NUCLEARMISSILE:
		case RADIUSCURSOR_EMPPULSE:
		case RADIUSCURSOR_ARTILLERYBARRAGE:
		case RADIUSCURSOR_FRENZY:
		case RADIUSCURSOR_NAPALMSTRIKE:
		case RADIUSCURSOR_CLUSTERMINES:
		case RADIUSCURSOR_SCUDSTORM:
		case RADIUSCURSOR_ANTHRAXBOMB:
		case RADIUSCURSOR_AMBUSH:
		case RADIUSCURSOR_RADAR:
		case RADIUSCURSOR_SPYDRONE:
		case RADIUSCURSOR_AMBULANCE:
			radius = specPowTempl ? specPowTempl->getRadiusCursorRadius() : 0.0f;
			break;

	}

	if (radius <= 0.0f)
		return;

	Coord3D pos = { 0, 0, 0 };	// will be updated right away
	m_radiusCursors[cursorType].createRadiusDecal(pos, radius, controller, m_curRadiusCursor);
	m_curRcType = cursorType;

	handleRadiusCursor();
}

//-------------------------------------------------------------------------------------------------
/** handle updating of "radius cursors" that follow the mouse pos */
//-------------------------------------------------------------------------------------------------
void InGameUI::handleRadiusCursor()
{
	if (!m_curRadiusCursor.isEmpty())
	{
    if ( TheGlobalData->m_doubleClickAttackMove && m_duringDoubleClickAttackMoveGuardHintTimer > 0 )
    {
      m_curRadiusCursor.setOpacity( m_duringDoubleClickAttackMoveGuardHintTimer * 0.1f );
  		m_curRadiusCursor.setPosition( m_duringDoubleClickAttackMoveGuardHintStashedPosition );	//world space position of center of decal

    }
    else
    {
			const MouseIO* mouseIO = TheMouse->getMouseStatus();
			Coord3D pos;
			Bool hasPos = false;

			//
			// if the mouse is in the radar window, the position in the world is that which is
			// represented by the radar, otherwise we use the mouse position itself transformed
			// from screen to world, but only if the radar is on.
			//
			if( rts::localPlayerHasRadar() )
			{
				hasPos = TheRadar->screenPixelToWorld( &mouseIO->pos, &pos );
			}

			if( !hasPos )
			{
				// if radar off, or point not on radar
				hasPos = TheTacticalView->screenToTerrain( &mouseIO->pos, &pos );
			}

			if( hasPos )
			{
				m_curRadiusCursor.setPosition(pos);	//world space position of center of decal
				m_curRadiusCursor.update();
			}
    }

  }
}


void InGameUI::triggerDoubleClickAttackMoveGuardHint()
{
	const MouseIO* mouseIO = TheMouse->getMouseStatus();
	if( TheTacticalView->screenToTerrain( &mouseIO->pos, &m_duringDoubleClickAttackMoveGuardHintStashedPosition ) )
	{
		m_duringDoubleClickAttackMoveGuardHintTimer = 11;
	}
}


//-------------------------------------------------------------------------------------------------
/** Handle the placement "icons" that appear at the cursor when we're putting down a
	* structure to build.  Note that this has additional logic to also show a line
	* of objects because when we build "walls" we want to draw a line of repeating
	* wall pieces on the map where we want to put all of them */
//-------------------------------------------------------------------------------------------------


void InGameUI::evaluateSoloNexus( Drawable *newlyAddedDrawable )
{

	m_soloNexusSelectedDrawableID = INVALID_DRAWABLE_ID;//failsafe...

	// short test: If the thing just added is a nonmobster, bail with nullptr
	if ( newlyAddedDrawable )
	{
		const Object *newObj = newlyAddedDrawable->getObject();
		if ( newObj && ! ( newObj->isKindOf(KINDOF_MOB_NEXUS) || newObj->isKindOf(KINDOF_IGNORED_IN_GUI) ) )
			return;
	}

	//LoopAllSelectedDrawables
	UnsignedShort nexaeFound = 0;
	for( DrawableListCIt it = m_selectedDrawables.begin(); it != m_selectedDrawables.end(); ++it )
	{

		Drawable *draw = (*it);
		const Object *obj = draw->getObject();


		if ( ! obj )
			continue;

		if ( obj->isKindOf( KINDOF_MOB_NEXUS ) )
		{
			++nexaeFound;
			if ( nexaeFound == 1 )
			{
				m_soloNexusSelectedDrawableID = draw->getID();
			}
			else // darn! more than one!
			{
				m_soloNexusSelectedDrawableID = INVALID_DRAWABLE_ID;
				return;
			}
		}
		else if ( ! obj->isKindOf( KINDOF_IGNORED_IN_GUI ) )// darn! a non-angrymobster!
		{
			m_soloNexusSelectedDrawableID = INVALID_DRAWABLE_ID;
			return;
		}

	}


}


void InGameUI::handleBuildPlacements()
{

	//
	// if we're in the process of placing something we need up update one or more drawables
	// based on the position of the mouse
	//
	if( m_pendingPlaceType )
	{
		ICoord2D loc;
		Coord3D world;
		Real angle = m_placeIcon[ 0 ]->getOrientation();

		// update the angle of the icon to match any placement angle and pick the
		// location the icon will be at (anchored is the start, otherwise it's the mouse)
		if( isPlacementAnchored() )
		{
			ICoord2D start, end;

			// get the placement arrow points
			getPlacementPoints( &start, &end );

			// set icon to anchor point
			loc = start;

			// only adjust angle if we've actually moved the mouse
			if( start.x != end.x || start.y != end.y )
			{
				Coord3D worldStart, worldEnd;

				// project the start and the end points of the line anchor into the 3D world
				if( TheTacticalView->screenToTerrain( &start, &worldStart ) &&
					TheTacticalView->screenToTerrain( &end, &worldEnd ) )
				{
					Coord2D v;
					v.x = worldEnd.x - worldStart.x;
					v.y = worldEnd.y - worldStart.y;
					angle = v.toAngle();

					// TheSuperHackers @tweak Stubbjax 04/08/2025 Snap angle to nearest 45 degrees
					// while using force attack mode for convenience.
					if (isInForceAttackMode())
					{
						const Real snapRadians = DEG_TO_RADF(45);
						angle = WWMath::Round(angle / snapRadians) * snapRadians;
					}
				}
			}

		}
		else
		{
			const MouseIO *mouseIO = TheMouse->getMouseStatus();

			// location is the mouse position
			loc = mouseIO->pos;

		}

		// set the location and angle of the place icon
		/**@todo this whole orientation vector thing is LAME! Must replace, all I want to
		to do is set a simple angle and have it automatically change, ug! */
		if( TheTacticalView->screenToTerrain( &loc, &world ) )
		{
			m_placeIcon[ 0 ]->setPosition( &world );
			m_placeIcon[ 0 ]->setOrientation( angle );

			//
			// check to see if this is a legal location to build something at and tint or "un-tint"
			// the cursor icons as appropriate.  This involves a pathfind which could be
			// expensive so we don't want to do it on every frame (although that would be ideal)
			// If we discover there are cases that this is just too slow we should increase the
			// delay time between checks or we need to come up with a way of recording what is
			// valid and what isn't or "fudge" the results to feel "ok"
			//
			if( TheGameClient->getFrame() & 0x1 )
			{
				TheTerrainVisual->removeAllBibs();

				Object *builderObject = TheGameLogic->findObjectByID( getPendingPlaceSourceObjectID() );

				LegalBuildCode lbc;
				lbc = TheBuildAssistant->isLocationLegalToBuild( &world,
																												 m_pendingPlaceType,
																												 angle,
																												 BuildAssistant::USE_QUICK_PATHFIND |
																												 BuildAssistant::TERRAIN_RESTRICTIONS |
																												 BuildAssistant::CLEAR_PATH |
																												 BuildAssistant::NO_OBJECT_OVERLAP |
																												 BuildAssistant::SHROUD_REVEALED |
																												 BuildAssistant::IGNORE_STEALTHED,
																												 builderObject,
																												 nullptr );

				if( lbc != LBC_OK )
					m_placeIcon[ 0 ]->colorTint( &IllegalBuildColor );
				else
					m_placeIcon[ 0 ]->colorTint( nullptr );

				// Add the bibs around the structure.
				if (lbc != LBC_OK)
				{
					TheTerrainVisual->addFactionBibDrawable(m_placeIcon[0], lbc != LBC_OK);
				} else {
					TheTerrainVisual->removeFactionBibDrawable(m_placeIcon[0]);
				}
			}
		}

		//
		// we have additional place icons when we're placing down a line of walls or other
		// similarly placed object ... for those we will have them be oriented the same way
		// as the first one, but we'll set their positions so that they "tile" end to end
		//
		if( isPlacementAnchored() && TheBuildAssistant->isLineBuildTemplate( m_pendingPlaceType ) )
		{
			// get our line placement points
			ICoord2D screenStart, screenEnd;
			getPlacementPoints( &screenStart, &screenEnd );

			// project the start and the end points of the line anchor into the 3D world
			Coord3D worldStart, worldEnd;
			if( TheTacticalView->screenToTerrain( &screenStart, &worldStart ) &&
				TheTacticalView->screenToTerrain( &screenEnd, &worldEnd ) )
			{
				// how big are each of our objects
				Real objectSize = m_pendingPlaceType->getTemplateGeometryInfo().getMajorRadius() * 2.0f;

				// what is our max tiling length we can make
				Int maxObjects = TheGlobalData->m_maxLineBuildObjects;

				// get the builder object that will be constructing things
				Object *builderObject = TheGameLogic->findObjectByID( getPendingPlaceSourceObjectID() );

				//
				// given the start/end points in the world and the the angle of the wall, fill
				// out an array of positions that "tile" this wall across the landscape
				//
				BuildAssistant::TileBuildInfo *tileBuildInfo;
				tileBuildInfo = TheBuildAssistant->buildTiledLocations( m_pendingPlaceType, angle,
																																&worldStart, &worldEnd,
																																objectSize, maxObjects,
																																builderObject );

				// create any necessary drawables we need to "fill out" the line
				Int i;
				for( i = 0; i < tileBuildInfo->tilesUsed; i++ )
				{

					if( m_placeIcon[ i ] == nullptr )
					{
						UnsignedInt drawableStatus = DRAWABLE_STATUS_NO_STATE_PARTICLES;
						drawableStatus |= TheGlobalData->m_objectPlacementShadows ? DRAWABLE_STATUS_SHADOWS : 0;
						m_placeIcon[ i ] = TheThingFactory->newDrawable( m_pendingPlaceType, drawableStatus );
					}

				}

				//
				// destroy any drawables that we're not using anymore because a previous
				// line length was longer
				//
				for( i = tileBuildInfo->tilesUsed; i < maxObjects; i++ )
				{

					if( m_placeIcon[ i ] != nullptr )
						TheGameClient->destroyDrawable( m_placeIcon[ i ] );
					m_placeIcon[ i ] = nullptr;

				}

				//
				// march down each drawable and set the position based on its position in the
				// line and set their angles all the same
				//
				for( i = 0; i < tileBuildInfo->tilesUsed; i++ )
				{

					// set the drawable position
					m_placeIcon[ i ]->setPosition( &tileBuildInfo->positions[ i ] );

					// set opacity for the drawable
					m_placeIcon[ i ]->setDrawableOpacity( TheGlobalData->m_objectPlacementOpacity );

					// set the drawable angle
					m_placeIcon[ i ]->setOrientation( angle );

				}

			}

		}

	}

}

//-------------------------------------------------------------------------------------------------
/** Pre-draw phase of the in game ui */
//-------------------------------------------------------------------------------------------------
void InGameUI::preDraw()
{

	// handle any "icons" for the act of building things and placing them in the world
	handleBuildPlacements();

	// handle radius-cursors, if any
	handleRadiusCursor();

	// draw the floating text first;
	drawFloatingText();

	// draw world animations
	updateAndDrawWorldAnimations();

}

//-------------------------------------------------------------------------------------------------
/** Update the in game user interface */
//-------------------------------------------------------------------------------------------------
//DECLARE_PERF_TIMER(InGameUI_update)
void InGameUI::update()
{
	//USE_PERF_TIMER(InGameUI_update)
	Int i;

	/// @todo make sure this code gets called even when the UI is not being drawn
	if ( m_videoStream && m_videoBuffer )
	{
		if ( m_videoStream->isFrameReady())
		{
			m_videoStream->frameDecompress();
			m_videoStream->frameRender( m_videoBuffer );
			m_videoStream->frameNext();
			if ( m_videoStream->frameIndex() == 0 )
			{
				stopMovie();
			}
		}
	}

	if ( m_cameoVideoStream && m_cameoVideoBuffer )
	{
		if ( m_cameoVideoStream->isFrameReady())
		{
			m_cameoVideoStream->frameDecompress();
			m_cameoVideoStream->frameRender( m_cameoVideoBuffer );
			m_cameoVideoStream->frameNext();
//			if ( m_cameoVideoStream->frameIndex() == 0 )
//			{
//				stopMovie();
//			}
		}
	}

	//
	// remove any message strings that have expired, note that the oldest strings are
	// always at the end of the array (higher index numbers) so we can just remove things
	// from the rear and never have to worry about shifting entries cause we check every
	// frame
	//
	UnsignedInt currLogicFrame = TheGameLogic->getFrame();
	const int messageTimeout = m_messageDelayMS / static_cast<float>(LOGICFRAMES_PER_SECOND) / 1000;
	UnsignedByte r, g, b, a;
	Int amount;
	for( i = MAX_UI_MESSAGES - 1; i >= 0; i-- )
	{

		if( currLogicFrame - m_uiMessages[ i ].timestamp > messageTimeout )
		{

			// get the current color of this text
			GameGetColorComponents( m_uiMessages[ i ].color, &r, &g, &b, &a );

			// start fading the alpha on this color down
			amount = REAL_TO_INT( ((currLogicFrame - m_uiMessages[ i ].timestamp) * 0.01f) );
			if( a - amount < 0 )
				a = 0;
			else
				a -= amount;

			// set the new color
			m_uiMessages[ i ].color = GameMakeColor( r, g, b, a );

			// when alpha is completely zero we remove this string
			if( a == 0 )
				removeMessageAtIndex( i );

		}

	}

	//
	// Update the Military Subtitle display
	//
	if( m_militarySubtitle )		// if we have a subtitle, work on it
	{
		// if the timeis frozen by a script, then we still want the text to display
		if(TheScriptEngine->isTimeFrozenScript())
		{
			m_militarySubtitle->lifetime--;
			m_militarySubtitle->blockBeginFrame--;
			m_militarySubtitle->incrementOnFrame--;
		}
		// if it's time to remove the subtitle, Then remove it
		if((Int)m_militarySubtitle->lifetime < (Int)currLogicFrame)
		{
			//steal colins fade from above :)
			GameGetColorComponents( m_militarySubtitle->color, &r, &g, &b, &a );
			// start fading the alpha on this color down
			amount = REAL_TO_INT( ((currLogicFrame - m_militarySubtitle->lifetime ) * 0.1f) );
			if( a - amount < 0 )
			{
				removeMilitarySubtitle();
			}
			else
			{
				a -= amount;
				m_militarySubtitle->color = GameMakeColor(r, g, b, a);
			}
		}
		else
		{
			// trigger whether or not we should draw the block
			if( m_militarySubtitle->blockBeginFrame + 9 < currLogicFrame )
			{
				m_militarySubtitle->blockBeginFrame = currLogicFrame;
				m_militarySubtitle->blockDrawn = !m_militarySubtitle->blockDrawn;
			}

			// If it's time to add another letter to the display string, lets do that.
			if( m_militarySubtitle->incrementOnFrame < currLogicFrame )
			{
				// first grab the letter we want to add
				WideChar tempWChar = m_militarySubtitle->subtitle.getCharAt(m_militarySubtitle->index);
				// if that letter is a return, add a new line
				if(tempWChar == L'\n')
				{
					// increment the Block position's Y value to draw it on the next line
					Int height;
					m_militarySubtitle->displayStrings[m_militarySubtitle->currentDisplayString]->getSize(nullptr, &height);
					m_militarySubtitle->blockPos.y = m_militarySubtitle->blockPos.y + height;

					// Now add a new display string
					m_militarySubtitle->currentDisplayString++;
					if(!(m_militarySubtitle->currentDisplayString >= MAX_SUBTITLE_LINES) )
					{
						m_militarySubtitle->blockPos.x = m_militarySubtitle->position.x;
						m_militarySubtitle->displayStrings[m_militarySubtitle->currentDisplayString] = TheDisplayStringManager->newDisplayString();
						m_militarySubtitle->displayStrings[m_militarySubtitle->currentDisplayString]->reset();
						m_militarySubtitle->displayStrings[m_militarySubtitle->currentDisplayString]->setFont(	TheFontLibrary->getFont( m_militaryCaptionFont, TheGlobalLanguageData->adjustFontSize(m_militaryCaptionPointSize), m_militaryCaptionBold ) )	;

						m_militarySubtitle->blockDrawn = TRUE;
						m_militarySubtitle->incrementOnFrame = currLogicFrame + (Int)(((Real)LOGICFRAMES_PER_SECOND * TheGlobalLanguageData->m_militaryCaptionDelayMS)/1000.0f);
					}
					else
					{
						// if we've exceeded the allocated number of display strings, this will force us to essentially truncate the remaining text
						m_militarySubtitle->index = m_militarySubtitle->subtitle.getLength();
						DEBUG_CRASH(("You're Only Allowed to use %d lines of subtitle text",MAX_SUBTITLE_LINES));
					}
				}
				else
				{
					// okay, we're not a \n, lets append this character to the display string
					m_militarySubtitle->displayStrings[m_militarySubtitle->currentDisplayString]->appendChar(tempWChar);
					// increment the draw position of the block
					Int width;
					m_militarySubtitle->displayStrings[m_militarySubtitle->currentDisplayString]->getSize(&width,nullptr);
					m_militarySubtitle->blockPos.x = m_militarySubtitle->position.x + width;

					// lets make a sound
					static AudioEventRTS click("MilitarySubtitlesTyping");
					TheAudio->addAudioEvent(&click);
					if(TheGlobalLanguageData)
						m_militarySubtitle->incrementOnFrame = currLogicFrame + TheGlobalLanguageData->m_militaryCaptionSpeed;
					else
						m_militarySubtitle->incrementOnFrame = currLogicFrame + m_militaryCaptionSpeed;

				}
				// increment the index
				m_militarySubtitle->index++;
				if(m_militarySubtitle->index >= m_militarySubtitle->subtitle.getLength())
				{
					// We're at the end of the subtitle, set everything to persist till the subtitle has expired
					m_militarySubtitle->incrementOnFrame = m_militarySubtitle->lifetime + 1;
				}
/*
							else
								{
									// randomize the space between printing of characters
									if(GameClientRandomValueReal(0,1) < 0.95f)
									{
										m_militarySubtitle->incrementOnFrame = GameClientRandomValue(2, 5) + currLogicFrame;
									}
									else
									{
										m_militarySubtitle->incrementOnFrame = GameClientRandomValue(10, 13) + currLogicFrame;
									}
								}*/

			}
		}
	}

	// update the player money window if the money amount has changed
	// this seems like as good a place as any to do the power hide/show
	static UnsignedInt lastMoney = ~0u;
	static UnsignedInt lastIncome = ~0u;
	static NameKeyType moneyWindowKey = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:MoneyDisplay" );
	static NameKeyType powerWindowKey = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:PowerWindow" );

	GameWindow *moneyWin = TheWindowManager->winGetWindowFromId( nullptr, moneyWindowKey );
	GameWindow *powerWin = TheWindowManager->winGetWindowFromId( nullptr, powerWindowKey );
//	if( moneyWin == nullptr )
//	{
//		NameKeyType moneyWindowKey = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:MoneyDisplay" );
//
//		moneyWin = TheWindowManager->winGetWindowFromId( nullptr, moneyWindowKey );
//
//	}  // end if
	Player* moneyPlayer = TheControlBar->getCurrentlyViewedPlayer();
	if( moneyPlayer)
	{
		Money *money = moneyPlayer->getMoney();
		Bool wantShowIncome = TheGlobalData->m_showMoneyPerMinute;
		Bool canShowIncome = TheGlobalData->m_allowMoneyPerMinuteForPlayer || TheControlBar->isObserverControlBarOn();
		Bool doShowIncome = wantShowIncome && canShowIncome;
		if (!doShowIncome)
		{
			UnsignedInt currentMoney = money->countMoney();
			if( lastMoney != currentMoney )
			{
				UnicodeString buffer;

				buffer.format(TheGameText->fetch( "GUI:ControlBarMoneyDisplay" ), currentMoney );
				GadgetStaticTextSetText( moneyWin, buffer );
				lastMoney = currentMoney;

			}
		}
		else
		{
			// TheSuperHackers @feature L3-M 21/08/2025 player money per minute
			UnsignedInt currentMoney = money->countMoney();
			UnsignedInt cashPerMin = money->getCashPerMinute();
			if ( lastMoney != currentMoney || lastIncome != cashPerMin )
			{
				UnicodeString buffer;
				UnicodeString moneyStr = formatMoneyValue(currentMoney);
				UnicodeString incomeStr = formatIncomeValue(cashPerMin);

				buffer.format(TheGameText->FETCH_OR_SUBSTITUTE_FORMAT("GUI:ControlBarMoneyDisplayIncome", L"$ %ls +%ls/min", moneyStr.str(), incomeStr.str()));
				GadgetStaticTextSetText(moneyWin, buffer);
				lastMoney = currentMoney;
				lastIncome = cashPerMin;
			}
		}
		moneyWin->winHide(FALSE);
		powerWin->winHide(FALSE);
	}
	else
	{
		moneyWin->winHide(TRUE);
		powerWin->winHide(TRUE);
	}

	// Update the floating Text;
	updateFloatingText();

	// update the control bar
	TheControlBar->update();

	updateIdleWorker();

	// update any random window layout that so requests
	for (std::list<WindowLayout *>::iterator it = m_windowLayouts.begin(); it != m_windowLayouts.end(); ++it)
	{
		WindowLayout *layout = *it;
		layout->runUpdate();
	}

	if (m_cameraRotatingLeft || m_cameraRotatingRight || m_cameraZoomingIn || m_cameraZoomingOut)
	{
		// TheSuperHackers @tweak The camera rotation and zoom are now decoupled from the render update.
		const Real fpsRatio = TheFramePacer->getBaseOverUpdateFpsRatio();
		const Real rotateAngle = TheGlobalData->m_keyboardCameraRotateSpeed * fpsRatio;
		const Real zoomHeight = (Real)View::ZoomHeightPerSecond * fpsRatio;

		if( m_cameraRotatingLeft && !m_cameraRotatingRight )
		{
			TheTacticalView->userSetAngle( TheTacticalView->getAngle() - rotateAngle );
		}
		else if( m_cameraRotatingRight && !m_cameraRotatingLeft )
		{
			TheTacticalView->userSetAngle( TheTacticalView->getAngle() + rotateAngle );
		}

		if( m_cameraZoomingIn && !m_cameraZoomingOut )
		{
			TheTacticalView->userZoom( -zoomHeight );
		}
		else if( m_cameraZoomingOut && !m_cameraZoomingIn )
		{
			TheTacticalView->userZoom( +zoomHeight );
		}
	}


}

//-------------------------------------------------------------------------------------------------
void InGameUI::registerWindowLayout( WindowLayout *layout )
{
	unregisterWindowLayout(layout); // sanity
	m_windowLayouts.push_back(layout);
}

//-------------------------------------------------------------------------------------------------
void InGameUI::unregisterWindowLayout( WindowLayout *layout )
{
	for (std::list<WindowLayout *>::iterator it = m_windowLayouts.begin(); it != m_windowLayouts.end(); ++it)
	{
		if (*it == layout)
		{
			m_windowLayouts.erase(it);
			return;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Reset the in game user interface */
//-------------------------------------------------------------------------------------------------
void InGameUI::reset()
{
	m_isQuitMenuVisible = FALSE;
	m_inputEnabled = true;
	// reset the command bar
	TheControlBar->reset();

	TheTacticalView->setCameraHeightAboveGroundLimitsToDefault();

	ResetInGameChat();

	// stop any movie currently playing
	stopMovie();

	// remove any pending GUI command
	setGUICommand( nullptr );

	// remove any build available status
	placeBuildAvailable( nullptr, nullptr );

	// free any message resources allocated
	freeMessageResources();

	// refresh custom ui strings - this will create the strings if required and update the fonts
	refreshCustomUiResources();

	Int i;
	for (i=0; i<MAX_PLAYER_COUNT; ++i)
	{
		for (SuperweaponMap::iterator mapIt = m_superweapons[i].begin(); mapIt != m_superweapons[i].end(); ++mapIt)
		{
			for (SuperweaponList::iterator listIt = mapIt->second.begin(); listIt != mapIt->second.end(); ++listIt)
			{
				SuperweaponInfo *info = *listIt;
				deleteInstance(info);
			}
			mapIt->second.clear();
		}
		m_superweapons[i].clear();
	}

	for (NamedTimerMapIt timerIt = m_namedTimers.begin(); timerIt != m_namedTimers.end(); ++timerIt)
	{
		NamedTimerInfo *info = timerIt->second;
		TheDisplayStringManager->freeDisplayString(info->displayString);
		deleteInstance(info);
	}
	m_namedTimers.clear();
	m_namedTimerLastFlashFrame = 0;
	m_namedTimerUsedFlashColor = TRUE; // so next one is false
	showNamedTimerDisplay(true);

	removeMilitarySubtitle();
	clearPopupMessageData();
	m_superweaponLastFlashFrame = 0;
	m_superweaponUsedFlashColor = TRUE; // so next one is false
	setSuperweaponDisplayEnabledByScript(true);

	clearFloatingText();
	clearWorldAnimations();
	resetIdleWorker();
	// clear hint lists
	for( i = 0; i < MAX_MOVE_HINTS; i++ )
	{

		m_moveHint[ i ].pos.zero();
		m_moveHint[ i ].sourceID = 0;
		m_moveHint[ i ].frame = 0;

	}

	setClientQuiet(false);
	setWaypointMode(false);
	setForceMoveMode(false);
	setForceAttackMode(false);
	setPreferSelectionMode(false);
	clearAttackMoveToMode();

	// TheSuperHackers @bugfix Disable all camera interactions to prevent them getting stuck after game end.
	setScrolling(false);
	setSelecting(false);
	setCameraRotateLeft(false);
	setCameraRotateRight(false);
	setCameraZoomIn(false);
	setCameraZoomOut(false);
	setCameraTrackingDrawable(false);

	m_windowLayouts.clear();

	m_tooltipsDisabledUntil = 0;

	UpdateDiplomacyBriefingText(AsciiString::TheEmptyString, TRUE);
}

//-------------------------------------------------------------------------------------------------
/** Free any resources we used for our messages */
//-------------------------------------------------------------------------------------------------
void InGameUI::freeMessageResources()
{
	Int i;

	// release display strings and set text to empty
	for( i = 0; i < MAX_UI_MESSAGES; i++ )
	{

		// empty text
		m_uiMessages[ i ].fullText.clear();

		// free display string
		if( m_uiMessages[ i ].displayString )
			TheDisplayStringManager->freeDisplayString( m_uiMessages[ i ].displayString );
		m_uiMessages[ i ].displayString = nullptr;

		// set timestamp to zero
		m_uiMessages[ i ].timestamp = 0;

	}

}

void InGameUI::freeCustomUiResources()
{
	TheDisplayStringManager->freeDisplayString(m_networkLatencyString);
	m_networkLatencyString = nullptr;
	TheDisplayStringManager->freeDisplayString(m_renderFpsString);
	m_renderFpsString = nullptr;
	TheDisplayStringManager->freeDisplayString(m_renderFpsLimitString);
	m_renderFpsLimitString = nullptr;
	TheDisplayStringManager->freeDisplayString(m_systemTimeString);
	m_systemTimeString = nullptr;
	TheDisplayStringManager->freeDisplayString(m_gameTimeString);
	m_gameTimeString = nullptr;
	TheDisplayStringManager->freeDisplayString(m_gameTimeFrameString);
	m_gameTimeFrameString = nullptr;

	m_playerInfoList.clear();
}

//-------------------------------------------------------------------------------------------------
/** Same as the unicode message method, but this takes an ascii string which is assumed
	* to me a string manager label */
//-------------------------------------------------------------------------------------------------
// srj sez: passing as const-ref screws up varargs for some reason. dunno why. just pass by value.
void InGameUI::message( AsciiString stringManagerLabel, ... )
{
	UnicodeString stringManagerString;
	UnicodeString formattedMessage;

	// fetch the string from the string manger
	stringManagerString = TheGameText->fetch( stringManagerLabel.str() );

	// GeneralsX @bugfix copilot 19/04/2026 Route UI text formatting through UnicodeString for cross-platform wide printf compatibility.
	va_list args;
	va_start( args, stringManagerLabel );
	formattedMessage.format_va(stringManagerString, args);
	va_end(args);

	// add the text to the ui
	addMessageText( formattedMessage );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void InGameUI::messageNoFormat( const UnicodeString& message )
{
	addMessageText( message, nullptr );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void InGameUI::messageNoFormat( const RGBColor *rgbColor, const UnicodeString& message )
{
	addMessageText( message, rgbColor );
}

//-------------------------------------------------------------------------------------------------
/** Interface for display text messages to the user */
//-------------------------------------------------------------------------------------------------
// srj sez: passing as const-ref screws up varargs for some reason. dunno why. just pass by value.
void InGameUI::message( UnicodeString format, ... )
{
	UnicodeString formattedMessage;

	// GeneralsX @bugfix copilot 19/04/2026 Route UI text formatting through UnicodeString for cross-platform wide printf compatibility.
	va_list args;
	va_start( args, format );
	formattedMessage.format_va(format, args);
	va_end(args);

	// add the text to the ui
	addMessageText( formattedMessage );
}

//-------------------------------------------------------------------------------------------------
/** Interface for display text messages to the user */
//-------------------------------------------------------------------------------------------------
// srj sez: passing as const-ref screws up varargs for some reason. dunno why. just pass by value.
void InGameUI::messageColor( const RGBColor *rgbColor, UnicodeString format, ... )
{
	UnicodeString formattedMessage;

	// GeneralsX @bugfix copilot 19/04/2026 Route UI text formatting through UnicodeString for cross-platform wide printf compatibility.
	va_list args;
	va_start( args, format );
	formattedMessage.format_va(format, args);
	va_end(args);

	// add the text to the ui
	addMessageText( formattedMessage, rgbColor );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void InGameUI::addMessageText( const UnicodeString& formattedMessage, const RGBColor *rgbColor )
{
	Int i;
	Color color1 = m_messageColor1;
	Color color2 = m_messageColor2;

	if (rgbColor)
	{
		color1 = rgbColor->getAsInt() | GameMakeColor( 0, 0, 0, 255 );
		color2 = rgbColor->getAsInt() | GameMakeColor( 0, 0, 0, 255 );
	}

	// delete the message stuff at the last index
	m_uiMessages[ MAX_UI_MESSAGES - 1 ].fullText.clear();
	if( m_uiMessages[ MAX_UI_MESSAGES - 1 ].displayString )
		TheDisplayStringManager->freeDisplayString( m_uiMessages[ MAX_UI_MESSAGES - 1 ].displayString );
	m_uiMessages[ MAX_UI_MESSAGES - 1 ].displayString = nullptr;
	m_uiMessages[ MAX_UI_MESSAGES - 1 ].timestamp = 0;

	// shift all the messages down one index and remove the last one
	for( i = MAX_UI_MESSAGES - 1; i >= 1; i-- )
		m_uiMessages[ i ] = m_uiMessages[ i - 1 ];

	//
	// set the new message in index 0, note that we need to allocate a display string, but
	// we do not need to free the one that is already there because it has been moved
	// "up" an index
	//
	m_uiMessages[ 0 ].fullText = formattedMessage;
	m_uiMessages[ 0 ].timestamp = TheGameLogic->getFrame();
	m_uiMessages[ 0 ].displayString = TheDisplayStringManager->newDisplayString();
	m_uiMessages[ 0 ].displayString->setFont( TheFontLibrary->getFont( m_messageFont,
																						TheGlobalLanguageData->adjustFontSize(m_messagePointSize), m_messageBold ) );
	m_uiMessages[ 0 ].displayString->setText( m_uiMessages[ 0 ].fullText );

	//
	// assign a color for this string instance that will stay with it no matter what
	// line it is rendered on
	//
	if( m_uiMessages[ 1 ].displayString == nullptr || m_uiMessages[ 1 ].color == color2 )
		m_uiMessages[ 0 ].color = color1;
	else
		m_uiMessages[ 0 ].color = color2;

}

//-------------------------------------------------------------------------------------------------
/** Remove the message on screen at index i */
//-------------------------------------------------------------------------------------------------
void InGameUI::removeMessageAtIndex( Int i )
{

	m_uiMessages[ i ].fullText.clear();
	if( m_uiMessages[ i ].displayString )
		TheDisplayStringManager->freeDisplayString( m_uiMessages[ i ].displayString );
	m_uiMessages[ i ].displayString = nullptr;
	m_uiMessages[ i ].timestamp = 0;

}

//-------------------------------------------------------------------------------------------------
/** An area selection is occurring, start graphical "hint". */
//-------------------------------------------------------------------------------------------------
void InGameUI::beginAreaSelectHint( const GameMessage *msg )
{
	m_isDragSelecting = true;
	m_dragSelectRegion = msg->getArgument( 0 )->pixelRegion;
}

//-------------------------------------------------------------------------------------------------
/** An area selection has occurred, finish graphical "hint". */
//-------------------------------------------------------------------------------------------------
void InGameUI::endAreaSelectHint( const GameMessage *msg )
{
	m_isDragSelecting = false;
}

//-------------------------------------------------------------------------------------------------
/** A move command has occurred, start graphical "hint". */
//-------------------------------------------------------------------------------------------------
void InGameUI::createMoveHint( const GameMessage *msg )
{
	Int i;

	// first, remove any existing move hint for this source if present
	for( i = 0; i < MAX_MOVE_HINTS; i++ )
		if( m_moveHint[ i ].sourceID == msg->getArgument( 0 )->objectID &&
				m_moveHint[ i ].frame != 0 )
			expireHint( MOVE_HINT, i );


	if( getSelectCount() == 1 )
	{
		Drawable *draw = getFirstSelectedDrawable();
		Object *obj = draw ? draw->getObject() : nullptr;
		if( obj && obj->isKindOf( KINDOF_IMMOBILE ) )
		{
			//Don't allow move hints to be created if our selected object can't move!
			return;
		}
	}

	m_moveHint[ m_nextMoveHint ].frame = TheGameClient->getFrame();
	m_moveHint[ m_nextMoveHint ].pos = msg->getArgument( 0 )->location;

	m_nextMoveHint++;

	// wrap around
	if (m_nextMoveHint == InGameUI::MAX_MOVE_HINTS)
		m_nextMoveHint = 0;
}

//-------------------------------------------------------------------------------------------------
/** An attack command has occurred, start graphical "hint". */
//-------------------------------------------------------------------------------------------------
void InGameUI::createAttackHint( const GameMessage *msg )
{

}

//-------------------------------------------------------------------------------------------------
/** A force attack command has occurred, start graphical "hint". */
//-------------------------------------------------------------------------------------------------
void InGameUI::createForceAttackHint( const GameMessage *msg )
{

}

//-------------------------------------------------------------------------------------------------
/** An garrison command has occurred, start graphical "hint". */
//-------------------------------------------------------------------------------------------------
void InGameUI::createGarrisonHint( const GameMessage *msg )
{
	Drawable *draw = TheGameClient->findDrawableByID( msg->getArgument(0)->drawableID );
	if( draw )
	{
		draw->onSelected();
	}
}

#if defined(RTS_DEBUG)
#define AI_DEBUG_TOOLTIPS		1

#ifdef AI_DEBUG_TOOLTIPS
#include "Common/StateMachine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/AIPathfind.h"
#endif // AI_DEBUG_TOOLTIPS

#endif // defined(RTS_DEBUG)

//-------------------------------------------------------------------------------------------------
/** Details of what is mouse hovered over right now are in this message.  Terrain might result
	* in just a tooltip.  An object might get a tooltip and show its hit points.
 */
//-------------------------------------------------------------------------------------------------
void InGameUI::createMouseoverHint( const GameMessage *msg )
{
	if (m_isScrolling || m_isSelecting)
		return; // no mouseover for you

	GameWindow *window = nullptr;
	const MouseIO *io = TheMouse->getMouseStatus();
	Bool underWindow = false;
	if (io && TheWindowManager)
		window = TheWindowManager->getWindowUnderCursor(io->pos.x, io->pos.y);

	while (window)
	{
		if (window->winGetInputFunc() == LeftHUDInput) {
			underWindow = false;
			break;
		}

		// check to see if it or any of its parents are opaque.  If so, we can't select anything.
		if (!BitIsSet( window->winGetStatus(), WIN_STATUS_SEE_THRU ))
		{
			underWindow = true;
			break;
		}

		window = window->winGetParent();
	}
	if (underWindow)
	{
		setMouseCursor(Mouse::ARROW); // regardless of m_mouseMode
		return;
	}





	DrawableID oldID = m_mousedOverDrawableID;

	if (msg->getType() == GameMessage::MSG_MOUSEOVER_DRAWABLE_HINT)
	{
		TheMouse->setCursorTooltip(UnicodeString::TheEmptyString );
		m_mousedOverDrawableID = INVALID_DRAWABLE_ID;
		const Drawable *draw = TheGameClient->findDrawableByID(msg->getArgument(0)->drawableID);
		const Object *obj = draw ? draw->getObject() : nullptr;
		if( obj )
		{

 			//Ahh, here is a weird exception: if the moused-over drawable is a mob-member
			//(e.g. AngryMob), Lets fool the UI into creating the hint for the NEXUS instead...
 			if (obj->isKindOf( KINDOF_IGNORED_IN_GUI ))
 			{
 				static NameKeyType key_MobMemberSlavedUpdate = NAMEKEY( "MobMemberSlavedUpdate" );
 				MobMemberSlavedUpdate *MMSUpdate = (MobMemberSlavedUpdate*)obj->findUpdateModule( key_MobMemberSlavedUpdate );
 				if( MMSUpdate )
 				{
 					Object *slaver = TheGameLogic->findObjectByID(MMSUpdate->getSlaverID());
 					if ( slaver )
 					{
 						Drawable *slaverDraw = slaver->getDrawable();
 						if ( slaverDraw )
 							m_mousedOverDrawableID = slaverDraw->getID();
 							// if this fails, not to worry... it has already defaulted to INVALID_DRAWABLE_ID, above
 					}
 				}
 			}
 			else
 				m_mousedOverDrawableID = draw->getID();

#if defined(RTS_DEBUG) //Extra hacky, sorry, but I need to use this in constantdebug report
			if ( TheGlobalData->m_constantDebugUpdate == TRUE )
				m_mousedOverDrawableID = draw->getID();
#endif


			const Player* player = nullptr;
			const ThingTemplate *thingTemplate = obj->getTemplate();

			ContainModuleInterface* contain = obj->getContain();
			if( contain )
				player = contain->getApparentControllingPlayer(ThePlayerList->getLocalPlayer());

			if (player == nullptr)
				player = obj->getControllingPlayer();

			Bool disguised = false;
			if( obj->isKindOf( KINDOF_DISGUISER ) )
			{
				//Because we have support for disguised units pretending to be units from another
				//team, we need to intercept it here and make sure it's rendered appropriately
				//based on which client is rendering it.
        StealthUpdate *update = obj->getStealth();
				if( update )
				{
					if( update->isDisguised() )
					{
						Player *clientPlayer = ThePlayerList->getLocalPlayer();
						Player *disguisedPlayer = ThePlayerList->getNthPlayer( update->getDisguisedPlayerIndex() );
						if( player->getRelationship( clientPlayer->getDefaultTeam() ) != ALLIES && clientPlayer->isPlayerActive() )
						{
							//Neutrals and enemies will see this disguised unit as the team it's disguised as.
							player = disguisedPlayer;
							const ThingTemplate *disguisedTemplate = update->getDisguisedTemplate();
							if( disguisedTemplate )
							{
								thingTemplate = disguisedTemplate;
								disguised = true;
							}
						}
						//Otherwise, the color will show up as the team it really belongs to (already set above).
					}
				}
			}


			UnicodeString str = thingTemplate->getDisplayName();
			UnicodeString displayName = thingTemplate->getDisplayName();
			if( str.isEmpty() )
			{
				AsciiString txtTemp;
				txtTemp.format("ThingTemplate:%s", obj->getTemplate()->getName().str());
				str = TheGameText->fetch(txtTemp);
				//str.format(L"ThingTemplate:'%hs'", obj->getTemplate()->getName().str());
			}

#ifdef AI_DEBUG_TOOLTIPS
			if (TheGlobalData->m_debugAI) {
				const Team *team = obj->getTeam();
				AsciiString objName = obj->getName();
				AsciiString teamName;
				AsciiString stateName;

				AIUpdateInterface *ai = (AIUpdateInterface*)obj->getAI();
				if (ai) {
					if (ai->getPath()) {
						TheAI->pathfinder()->setDebugPath(ai->getPath());
					}
#ifdef STATE_MACHINE_DEBUG
					stateName = ai->getCurrentStateName();
					if (ai->getAttackInfo()) {
						stateName.concat(" AttackPriority=");
						stateName.concat(ai->getAttackInfo()->getName());
					}
#endif
				}
				if( team )
				{
					teamName = team->getName();
				}
				if (!objName.isEmpty())
				{
					if (!teamName.isEmpty())
					{
						str.format(L"%hs(%hs): %s", teamName.str(), objName.str(), str.str());
					}
					else
					{
						str.format(L"%hs: %s", objName.str(), str.str());
					}
				}
				else
				{
					if (!teamName.isEmpty())
					{
						str.format(L"%hs: %s", teamName.str(), str.str());
					}
				}
				str.format(L"%s - %hs", str.str(), stateName.str());

			}
#endif
			UnicodeString warehouseFeedback;
			// Add on dollar amount of warehouse contents so people don't freak out until the art is hooked up
			static const NameKeyType warehouseModuleKey = TheNameKeyGenerator->nameToKey( "SupplyWarehouseDockUpdate" );
			SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate *)obj->findUpdateModule( warehouseModuleKey );
			if( warehouseModule != nullptr )
			{
				Int boxes = warehouseModule->getBoxesStored();
				Int value = boxes * TheGlobalData->m_baseValuePerSupplyBox;
				warehouseFeedback.format(TheGameText->fetch("TOOLTIP:SupplyWarehouse"), value);
				str.concat(warehouseFeedback);
			}

      if (player)
			{
				UnicodeString tooltip;
				//if (TheRecorder->isMultiplayer() && player->getPlayerType() == PLAYER_HUMAN)
				if (TheRecorder->isMultiplayer() && player->isPlayableSide())
					tooltip.format(L"%s\n%s", str.str(), ((Player *)player)->getPlayerDisplayName().str());
				else
					tooltip = str;

				const Int localPlayerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();

				Int x, y;
				ThePartitionManager->worldToCell(obj->getPosition()->x, obj->getPosition()->y, &x, &y);
				if( ThePartitionManager->getShroudStatusForPlayer(localPlayerIndex, x, y) == CELLSHROUD_CLEAR )
				{
					RGBColor rgb;
					if( disguised )
					{
						rgb.setFromInt( player->getPlayerColor() );
					}
					else
					{
						rgb.setFromInt(draw->getObject()->getIndicatorColor());

						// Unless this is a stealth garrisoned building,
						// Let's not use the contained's housecolor
						const Object *obj = draw->getObject();
						if ( obj )
						{
							ContainModuleInterface *contain = obj->getContain();
							if ( contain && contain->isGarrisonable() )
							{
								const Player *play = contain->getApparentControllingPlayer( ThePlayerList->getLocalPlayer() );
								if ( play )
									rgb.setFromInt( play->getPlayerColor() );
							}
						}

					}

					//Object:Prop is a blank string... but we don't want to show
					//any popup box at all if that is the case!
					if( displayName.compare( TheGameText->fetch( "OBJECT:Prop" ) ) )
					{
	  				TheMouse->setCursorTooltip(tooltip, -1, &rgb );
					}
				}
			}
		}

	}
	else
	{
		m_mousedOverDrawableID = INVALID_DRAWABLE_ID;
	}

	if (oldID != m_mousedOverDrawableID)
	{
		//DEBUG_LOG(("Resetting tooltip delay"));
		TheMouse->resetTooltipDelay();
	}

	if (m_mouseMode == MOUSEMODE_DEFAULT && !m_isScrolling && !m_isSelecting && !getSelectCount() && (TheRecorder->getMode() != RECORDERMODETYPE_PLAYBACK || TheLookAtTranslator->hasMouseMovedRecently()))
	{
		if( m_mousedOverDrawableID != INVALID_DRAWABLE_ID )
		{
			Drawable *draw = TheGameClient->findDrawableByID(m_mousedOverDrawableID);

			//Add basic logic to determine if we can select a unit (or hint)
			const Object *obj = draw ? draw->getObject() : nullptr;
			Bool drawSelectable = CanSelectDrawable(draw, FALSE);
			if( !obj )
			{
				drawSelectable = false;
			}

			if( drawSelectable && obj->isLocallyControlled() )
			{
				setMouseCursor(Mouse::SELECTING);
			}
			else
			{
				setMouseCursor(Mouse::ARROW);
			}
		}
		else
		{
			setMouseCursor(Mouse::ARROW);
		}
	}
	else if (m_mouseMode != MOUSEMODE_DEFAULT && m_mouseMode != MOUSEMODE_BUILD_PLACE )
	{
		setMouseCursor((Mouse::MouseCursor)m_mouseModeCursor);
	}
}

//-------------------------------------------------------------------------------------------------
/** A command would be given if a click were to happen, so give a preview hint of what it would be.
	* Changing the mouse cursor is an example
	*/
void InGameUI::createCommandHint( const GameMessage *msg )
{
	if (m_isScrolling || m_isSelecting || TheRecorder->getMode() == RECORDERMODETYPE_PLAYBACK)
		return;

	const Drawable *draw = TheGameClient->findDrawableByID(m_mousedOverDrawableID);
	GameMessage::Type t = msg->getType();
//#ifdef DO_SHROUD_PROJECTION
	if( draw && (t == GameMessage::MSG_DO_ATTACK_OBJECT_HINT || t == GameMessage::MSG_DO_ATTACK_OBJECT_AFTER_MOVING_HINT) )
	{
		const Object* obj = draw->getObject();
		const Int localPlayerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();
#if ENABLE_CONFIGURABLE_SHROUD
		ObjectShroudStatus ss = (!obj || !TheGlobalData->m_shroudOn) ? OBJECTSHROUD_CLEAR : obj->getShroudedStatus(localPlayerIndex);
#else
		ObjectShroudStatus ss = (!obj) ? OBJECTSHROUD_CLEAR : obj->getShroudedStatus(localPlayerIndex);
#endif
		if (ss == OBJECTSHROUD_SHROUDED)
		{
			t = GameMessage::MSG_DO_MOVETO_HINT;	// if the object is hidden, switch to something innocuous
		}
	}
//#endif


	setRadiusCursorNone();
  if ( TheGlobalData->m_doubleClickAttackMove )
  {
    if ( --m_duringDoubleClickAttackMoveGuardHintTimer > 0 )
    {
      setMouseCursor(Mouse::FORCE_ATTACK_GROUND);
		  setRadiusCursor(RADIUSCURSOR_GUARD_AREA,
										  nullptr,
										  PRIMARY_WEAPON);
      return;
    }
  }





	// set cursor to normal if there is a window under the cursor
	GameWindow *window = nullptr;
	const MouseIO *io = TheMouse->getMouseStatus();
	Bool underWindow = false;
	if (io && TheWindowManager)
		window = TheWindowManager->getWindowUnderCursor(io->pos.x, io->pos.y);


	while (window)
	{
		if (window->winGetInputFunc() == LeftHUDInput) {
			underWindow = false;
			break;
		}

		// check to see if it or any of its parents are opaque.  If so, we can't select anything.
		if (!BitIsSet( window->winGetStatus(), WIN_STATUS_SEE_THRU ))
		{
			underWindow = true;
			break;
		}

		window = window->winGetParent();
	}

	//Add basic logic to determine if we can select a unit (or hint)
	const Object *obj = draw ? draw->getObject() : nullptr;
	Bool drawSelectable = CanSelectDrawable(draw, FALSE);
	if( !obj )
	{
		drawSelectable = false;
	}

	// Note: These are only non-null if there is exactly one thing selected.
	const Drawable *srcDraw = nullptr;
	const Object *srcObj = nullptr;
	if (getSelectCount() == 1) {
		srcDraw = getAllSelectedDrawables()->front();
		srcObj = (srcDraw ? srcDraw->getObject() : nullptr);
	}

	switch (m_mouseMode)
	{
		case MOUSEMODE_DEFAULT:
			{
				// This section of code only gets called when there is no specific cursor mode happening.
				if (underWindow || (srcObj && !srcObj->isLocallyControlled()))
				{
					setMouseCursor(Mouse::ARROW);
					return;
				}
				switch (t)
				{
					case GameMessage::MSG_DO_MOVETO_HINT:
					{
						if( !drawSelectable && srcObj && srcObj->isLocallyControlled() && srcObj->isKindOf(KINDOF_STRUCTURE))
							setMouseCursor( Mouse::GENERIC_INVALID );
						else if( drawSelectable && obj->isLocallyControlled() && !obj->isKindOf(KINDOF_MINE))
							setMouseCursor( Mouse::SELECTING );
						else if( TheRadar->isRadarWindow( window ) && !rts::localPlayerHasRadar() )
							setMouseCursor( Mouse::ARROW );
						else
							setMouseCursor( Mouse::MOVETO );
						break;
					}
					case GameMessage::MSG_DO_ATTACKMOVETO_HINT:
						if( drawSelectable && obj->isLocallyControlled()  )
							setMouseCursor( Mouse::SELECTING );
						else
							setMouseCursor( Mouse::ATTACKMOVETO );
						break;
					case GameMessage::MSG_ADD_WAYPOINT_HINT:
						setMouseCursor( Mouse::WAYPOINT );
						break;
					case GameMessage::MSG_DO_ATTACK_OBJECT_HINT:
						setMouseCursor( Mouse::ATTACK_OBJECT );
						break;
					case GameMessage::MSG_DO_ATTACK_OBJECT_AFTER_MOVING_HINT:
						setMouseCursor( Mouse::OUTRANGE );
						break;
					case GameMessage::MSG_DO_FORCE_ATTACK_OBJECT_HINT:
						setMouseCursor( Mouse::FORCE_ATTACK_OBJECT );
						break;
					case GameMessage::MSG_DO_FORCE_ATTACK_GROUND_HINT:
						setMouseCursor( Mouse::FORCE_ATTACK_GROUND );
						break;
					case GameMessage::MSG_GET_REPAIRED_HINT:
						setMouseCursor( Mouse::GET_REPAIRED );
						break;
					case GameMessage::MSG_DOCK_HINT:
						setMouseCursor( Mouse::DOCK );
						break;
					case GameMessage::MSG_GET_HEALED_HINT:
						setMouseCursor( Mouse::GET_HEALED );
						break;
					case GameMessage::MSG_DO_REPAIR_HINT:
						setMouseCursor( Mouse::DO_REPAIR );
						break;
					case GameMessage::MSG_RESUME_CONSTRUCTION_HINT:
						setMouseCursor( Mouse::RESUME_CONSTRUCTION );
						break;
					case GameMessage::MSG_ENTER_HINT:
						setMouseCursor( Mouse::ENTER_FRIENDLY );
						break;
					case GameMessage::MSG_CONVERT_TO_CARBOMB_HINT:
					case GameMessage::MSG_HIJACK_HINT:
					case GameMessage::MSG_SABOTAGE_HINT:
						setMouseCursor( Mouse::ENTER_AGGRESSIVELY );
						break;
					case GameMessage::MSG_DEFECTOR_HINT:
						setMouseCursor( Mouse::DEFECTOR );
						break;
#ifdef ALLOW_SURRENDER
					case GameMessage::MSG_PICK_UP_PRISONER_HINT:
						setMouseCursor( Mouse::PICK_UP_PRISONER );
						break;
#endif
					case GameMessage::MSG_CAPTUREBUILDING_HINT:
						setMouseCursor( Mouse::CAPTUREBUILDING );
						break;
					case GameMessage::MSG_HACK_HINT:
						setMouseCursor( Mouse::HACK );
						break;
					case GameMessage::MSG_IMPOSSIBLE_ATTACK_HINT:
						setMouseCursor( Mouse::GENERIC_INVALID );
						break;
					case GameMessage::MSG_SET_RALLY_POINT_HINT:
						if ( !drawSelectable )
							setMouseCursor( Mouse::SET_RALLY_POINT );
						else
							setMouseCursor( Mouse::SELECTING );
						break;
					case GameMessage::MSG_DO_SPECIAL_POWER_OVERRIDE_DESTINATION_HINT:
						setMouseCursor( Mouse::PARTICLE_UPLINK_CANNON );
						break;
					case GameMessage::MSG_DO_SALVAGE_HINT:
						setMouseCursor( Mouse::MOVETO );
						break;
					case GameMessage::MSG_DO_INVALID_HINT:
						setMouseCursor( Mouse::GENERIC_INVALID );
						break;
				}
			}
			break;
		case MOUSEMODE_BUILD_PLACE:
			{
				if (underWindow)
				{
					setMouseCursor(Mouse::ARROW);
					return;
				}
				switch (t)
				{
					case GameMessage::MSG_DO_MOVETO_HINT:
					case GameMessage::MSG_DO_ATTACKMOVETO_HINT:
					case GameMessage::MSG_ADD_WAYPOINT:
						setMouseCursor(Mouse::BUILD_PLACEMENT);
						break;
					case GameMessage::MSG_DO_ATTACK_OBJECT_HINT:
					case GameMessage::MSG_DO_ATTACK_OBJECT_AFTER_MOVING_HINT:
						setMouseCursor(Mouse::INVALID_BUILD_PLACEMENT);
						break;
				}
			}
			break;
		case MOUSEMODE_GUI_COMMAND:
			{
				if (underWindow)
				{
					setMouseCursor(Mouse::ARROW);
					return;
				}
				// set the mouse cursor for commands that need a targeting or to normal with no command
				if( m_pendingGUICommand )
				{
					if( m_pendingGUICommand->isContextCommand() ||
							m_pendingGUICommand->getCommandType() == GUI_COMMAND_SPECIAL_POWER ||
							m_pendingGUICommand->getCommandType() == GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT )
					{
						//Here is the hook for when we are in a context sensitive command mode. We can
						//either do the specified command mode command or nothing! Whether or not the
						//command is valid or not was determined in evaluateContextCommand which is
						//called first, and posts the appropriate message.
						AsciiString cursorName;	// empty by default
						switch( t )
						{
							case GameMessage::MSG_VALID_GUICOMMAND_HINT:
								cursorName = m_pendingGUICommand->getCursorName();
								break;
							case GameMessage::MSG_INVALID_GUICOMMAND_HINT:
							default:
								cursorName = m_pendingGUICommand->getInvalidCursorName();
								break;
						}

						Int index = TheMouse->getCursorIndex(cursorName);
						if( index != Mouse::INVALID_MOUSE_CURSOR )
						{
							setMouseCursor( (Mouse::MouseCursor)index );
						}
						else
						{
							setMouseCursor( Mouse::CROSS );
						}
						setRadiusCursor(m_pendingGUICommand->getRadiusCursorType(), //*****************************************************************
														m_pendingGUICommand->getSpecialPowerTemplate(),
														m_pendingGUICommand->getWeaponSlot());
					}
					else if( BitIsSet( m_pendingGUICommand->getOptions(), COMMAND_OPTION_NEED_TARGET ) )
					{
						Int index = TheMouse->getCursorIndex(m_pendingGUICommand->getCursorName());
						if (index != Mouse::INVALID_MOUSE_CURSOR)
							setMouseCursor( (Mouse::MouseCursor)index );
						else
							setMouseCursor( Mouse::CROSS );
						setRadiusCursor(m_pendingGUICommand->getRadiusCursorType(), //*****************************************************************
														m_pendingGUICommand->getSpecialPowerTemplate(),
														m_pendingGUICommand->getWeaponSlot());
					}
					else
					{
						setRadiusCursorNone();
					}
				}
			}
			break;
	}
}

//-------------------------------------------------------------------------------------------------
/// Get drawable ID under cursor
//-------------------------------------------------------------------------------------------------
DrawableID InGameUI::getMousedOverDrawableID() const
{

	return m_mousedOverDrawableID;

}

//-------------------------------------------------------------------------------------------------
/// set right-click scroll mode
//-------------------------------------------------------------------------------------------------
void InGameUI::setScrolling( Bool isScrolling )
{
	if (m_isScrolling == isScrolling)
	{
		return;
	}

	if (isScrolling)
	{
		setMouseCursor( Mouse::SCROLL );

		// break any camera locks
		TheTacticalView->userSetCameraLock( INVALID_ID );
		TheTacticalView->userSetCameraLockDrawable( nullptr );
	}
	else
	{
		setMouseCursor( Mouse::ARROW );
	}

	m_isScrolling = isScrolling;

}

//-------------------------------------------------------------------------------------------------
/// are we scrolling?
//-------------------------------------------------------------------------------------------------
Bool InGameUI::isScrolling()
{
	return m_isScrolling;
}

//-------------------------------------------------------------------------------------------------
/// set drag select mode
//-------------------------------------------------------------------------------------------------
void InGameUI::setSelecting( Bool isSelecting )
{
	if (m_isSelecting == isSelecting)
	{
		return;
	}

	//setMouseCursor( Mouse::SELECTING );
	m_isSelecting = isSelecting;
}

//-------------------------------------------------------------------------------------------------
/// are we selecting?
//-------------------------------------------------------------------------------------------------
Bool InGameUI::isSelecting()
{
	return m_isSelecting;
}

//-------------------------------------------------------------------------------------------------
/// get scroll amount
//-------------------------------------------------------------------------------------------------
void InGameUI::setScrollAmount( Coord2D amt )
{
	m_scrollAmt = amt;
}

//-------------------------------------------------------------------------------------------------
/// get scroll amount
//-------------------------------------------------------------------------------------------------
Coord2D InGameUI::getScrollAmount()
{
	return m_scrollAmt;
}

//-------------------------------------------------------------------------------------------------
/** Like the building "placement" mode, clicking on some buttons in the UI require us to
	* provide additional data by clicking on a target object/location in the world.  This
	* is where we enable that "mode" so that we can get the additional data needed for a
	* command from the user */
//-------------------------------------------------------------------------------------------------
void InGameUI::setGUICommand( const CommandButton *command )
{
	if (TheRecorder->getMode() == RECORDERMODETYPE_PLAYBACK)
		return;

	// sanity
	if( command )
	{

		if( BitIsSet( command->getOptions(), COMMAND_OPTION_NEED_TARGET ) == FALSE )
		{

			DEBUG_CRASH( ("setGUICommand: Command '%s' does not need additional user interaction",
														command->getName().str()) );
			m_pendingGUICommand = nullptr;
			m_mouseMode = MOUSEMODE_DEFAULT;
			return;

		}

		m_mouseMode = MOUSEMODE_GUI_COMMAND;

	}
	else
	{
		m_mouseMode = MOUSEMODE_DEFAULT;
	}

	// set the command
	m_pendingGUICommand = command;

	// set the mouse cursor for commands that need a targeting or to normal with no command
	if( command && BitIsSet( command->getOptions(), COMMAND_OPTION_NEED_TARGET ) && !command->isContextCommand() )
	{
		setMouseCursor( Mouse::ARROW );// This occurs on the mouse-up of a panel button, so make an arrow
		// the mouseoverhint code will take care of the cursor context, once the mouse leaves the panel
		// but we will set the radius cursor here, so you can see it bleeding out from beneath the panel

		setRadiusCursor(command->getRadiusCursorType(), //*****************************************************************
										command->getSpecialPowerTemplate(),
										command->getWeaponSlot());
	}
	else
	{
		if (TheMouse)
		{
			setMouseCursor( Mouse::ARROW );
		}
		setRadiusCursorNone();
	}

	m_mouseModeCursor = TheMouse->getMouseCursor();

}

//-------------------------------------------------------------------------------------------------
/** Get the pending gui command */
//-------------------------------------------------------------------------------------------------
const CommandButton *InGameUI::getGUICommand() const
{

	return m_pendingGUICommand;

}

//-------------------------------------------------------------------------------------------------
/** Destroy any drawables we have in our placement icon array and set to null */
//-------------------------------------------------------------------------------------------------
void InGameUI::destroyPlacementIcons()
{
	Int i;

	for( i = 0; i < TheGlobalData->m_maxLineBuildObjects; ++i )
	{

		if( m_placeIcon[ i ] )
		{
			TheTerrainVisual->removeFactionBibDrawable(m_placeIcon[ i ]);
			TheGameClient->destroyDrawable( m_placeIcon[ i ] );
		}
		m_placeIcon[ i ] = nullptr;

	}
	TheTerrainVisual->removeAllBibs();

}

//-------------------------------------------------------------------------------------------------
/** User has clicked on a built item that requires placement in the world.  We will
	* record what that thing is so that the we can catch the next click in the world
	* and try to place the object there */
//-------------------------------------------------------------------------------------------------
void InGameUI::placeBuildAvailable( const ThingTemplate *build, Drawable *buildDrawable )
{

	if (build != nullptr)
	{
		// if building something, no radius cursor, thankew
		setRadiusCursorNone();
	}

	//
	// if we're setting another place available, but we're somehow already in the placement
	// mode, get out of it before we start a new one
	//
	if( m_pendingPlaceType != nullptr && build != nullptr )
		placeBuildAvailable( nullptr, nullptr );

	//
	// keep a record of what we are trying to place, if we are already trying to
	// place something, it is overwritten
	//
	m_pendingPlaceType = build;

	//Keep the prev pending place for left click deselection prevention in alternate mouse mode.
	//We want to keep our dozer selected after initiating construction.
	setPreventLeftClickDeselectionInAlternateMouseModeForOneClick( m_pendingPlaceSourceObjectID != INVALID_ID );
	m_pendingPlaceSourceObjectID = INVALID_ID;

	Object *sourceObject = nullptr;
	if( buildDrawable )
		sourceObject = buildDrawable->getObject();
	if( sourceObject )
		m_pendingPlaceSourceObjectID = sourceObject->getID();

	//
	// hack, change our cursor to at least something different ... also note that it's
	// possible to not have the mouse yet, as some UI systems as part of initialization
	// make sure that there isn't anything valid for to "place build"
	//
	if( TheMouse )
	{

		if( build )
		{
			m_mouseMode = MOUSEMODE_BUILD_PLACE;
			m_mouseModeCursor = Mouse::CROSS;

			Drawable *draw;

			// hack for changing cursor
			setMouseCursor( Mouse::CROSS );

			// deselect all drawables, otherwise they move to the place we click
			///@ todo when message stream order more formalized eliminate this
//			TheInGameUI->deselectAllDrawables();

			{
				// create a drawable of what we are building to be "attached" at the cursor
				UnsignedInt drawableStatus = DRAWABLE_STATUS_NO_STATE_PARTICLES;
				drawableStatus |= TheGlobalData->m_objectPlacementShadows ? DRAWABLE_STATUS_SHADOWS : 0;
				draw = TheThingFactory->newDrawable( build, drawableStatus );
			}
			if (sourceObject)
			{
				if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT)
					draw->setIndicatorColor(sourceObject->getControllingPlayer()->getPlayerNightColor());
				else
					draw->setIndicatorColor(sourceObject->getControllingPlayer()->getPlayerColor());
			}
			DEBUG_ASSERTCRASH( draw, ("Unable to create icon at cursor for placement '%s'",
												 build->getName().str()) );

			//
			// set the initial angle of the free floating building to the property from INI
			// we have this so we can have the "cool" face the user until they click and
			// pick an actual direction for placement
			//
			Real angle = build->getPlacementViewAngle();

			// set the angle in the icon we just created
			draw->setOrientation( angle );

			// set the build icon attached to the cursor to be "see-thru"
			draw->setDrawableOpacity( TheGlobalData->m_objectPlacementOpacity );

			// set the "icon" in the icon array at the first index
			DEBUG_ASSERTCRASH( m_placeIcon[ 0 ] == nullptr, ("placeBuildAvailable, build icon array is not empty!") );
			m_placeIcon[ 0 ] = draw;

		}
		else
		{
			if (m_mouseMode == MOUSEMODE_BUILD_PLACE)
			{
				m_mouseMode = MOUSEMODE_DEFAULT;
				m_mouseModeCursor = Mouse::ARROW;
			}

			setMouseCursor( Mouse::ARROW );
			setPlacementStart( nullptr );

			// if we have a place icons destroy them
			destroyPlacementIcons();

			if( sourceObject )
			{
				ProductionUpdateInterface *puInterface = sourceObject->getProductionUpdateInterface();
				if( puInterface )
				{
					//Clear the special power mode for construction if we set it. Actually call it everytime
					//rather than checking if it's set before clearing (cheaper).
					puInterface->setSpecialPowerConstructionCommandButton( nullptr );
				}
			}

		}

	}

}

//-------------------------------------------------------------------------------------------------
/** Return the thing we're attempting to place */
//-------------------------------------------------------------------------------------------------
const ThingTemplate *InGameUI::getPendingPlaceType()
{
	return m_pendingPlaceType;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectID InGameUI::getPendingPlaceSourceObjectID()
{

	return m_pendingPlaceSourceObjectID;

}

//-------------------------------------------------------------------------------------------------
/** Start the angle selection interface for selecting building angles when placing them */
//-------------------------------------------------------------------------------------------------
void InGameUI::setPlacementStart( const ICoord2D *start )
{

	// if we have a start point we turn "on" the interface, otherwise we turn it "off"
	if( start )
	{

		m_placeAnchorStart = *start;
		m_placeAnchorEnd = *start;
		m_placeAnchorInProgress = TRUE;

	}
	else
		m_placeAnchorInProgress = FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Set the end anchor for the angle build interface */
//-------------------------------------------------------------------------------------------------
void InGameUI::setPlacementEnd( const ICoord2D *end )
{

	if( end )
		m_placeAnchorEnd = *end;

}

//-------------------------------------------------------------------------------------------------
/** Is the angle selection interface for placing building at angles up? */
//-------------------------------------------------------------------------------------------------
Bool InGameUI::isPlacementAnchored()
{

	return m_placeAnchorInProgress;

}

//-------------------------------------------------------------------------------------------------
/** Get the start and end anchor points for the building angle selection interface */
//-------------------------------------------------------------------------------------------------
void InGameUI::getPlacementPoints( ICoord2D *start, ICoord2D *end )
{

	if( start )
		*start = m_placeAnchorStart;
	if( end )
		*end = m_placeAnchorEnd;

}

//-------------------------------------------------------------------------------------------------
/** Return the angle of the drawable at the cursor if any */
//-------------------------------------------------------------------------------------------------
Real InGameUI::getPlacementAngle()
{

	if( m_placeIcon[ 0 ] )
		return m_placeIcon[ 0 ]->getOrientation();

	return 0.0f;

}

//-------------------------------------------------------------------------------------------------
/** Mark given Drawable as "selected". */
//-------------------------------------------------------------------------------------------------
void InGameUI::selectDrawable( Drawable *draw )
{

	if( draw->isSelected() == FALSE )
	{

		m_frameSelectionChanged = TheGameLogic->getFrame();
		// set the selection in the drawable
		draw->friend_setSelected();

		// add to our selected list
		m_selectedDrawables.push_front( draw );

		// we now have one more selected drawable
		incrementSelectCount();


		// evaluate whether our selection consists of exactly one angry mob
		evaluateSoloNexus( draw );

		// the control needs to update its context sensitive display now
		TheControlBar->onDrawableSelected( draw );

	}

}

//-------------------------------------------------------------------------------------------------
/** Clear "selected" status of Drawable. */
//-------------------------------------------------------------------------------------------------
void InGameUI::deselectDrawable( Drawable *draw )
{

	if( draw->isSelected() )
	{

		m_frameSelectionChanged = TheGameLogic->getFrame();
		// clear the selected bit out of the drawable
		draw->friend_clearSelected();

		// find the drawable entry in our list
		DrawableListIt findIt = std::find( m_selectedDrawables.begin(),
																			 m_selectedDrawables.end(),
																			 draw );

		// sanity
		DEBUG_ASSERTCRASH( findIt != m_selectedDrawables.end(),
											 ("deselectDrawable: Drawable not found in the selected drawable list '%s'",
											 draw->getTemplate()->getName().str()) );

		// remove it from the selected drawable list
		m_selectedDrawables.erase( findIt );

		// keep out own internal count happy
		decrementSelectCount();

		// evaluate whether our selection consists of exactly one angry mob
		evaluateSoloNexus();

		// the control needs to update its context sensitive display now
		TheControlBar->onDrawableDeselected( draw );

	}

}

//-------------------------------------------------------------------------------------------------
/** Clear all drawables' "select" status */
//-------------------------------------------------------------------------------------------------
void InGameUI::deselectAllDrawables( Bool postMsg )
{
	const DrawableList *selected = getAllSelectedDrawables();

	// loop through all the selected drawables
	for ( DrawableListCIt it = selected->begin(); it != selected->end(); )
	{

		// get drawable and increment iterator, we will invalidate it as we deselect
		Drawable* draw = *it++;

		// do the deselection
		deselectDrawable( draw );

	}

	// keep our list all tidy
	m_selectedDrawables.clear();


	// our selection can no longer consist of exactly one angry mob
	m_soloNexusSelectedDrawableID = INVALID_DRAWABLE_ID;


	///@todo don't we want to not emit this message if there wasn't a group at all? (CBD)
	/** @todo also, we probably are sending this message too much, we should come up with
	some kind of "selections are dirty" status that we can check once per frame and send
	the correct group info over the network ... could be tricky tho (or impossible) given
	the order of operations of things happening in the code (CBD) */
	if( postMsg )
	{
		// TheSuperHackers @tweak Originally this message had one boolean argument, but it wasn't used for anything.
		TheMessageStream->appendMessage( GameMessage::MSG_DESTROY_SELECTED_GROUP );
	}
}



//-------------------------------------------------------------------------------------------------
/** Return the list of all the currently selected Drawable pointers. */
//-------------------------------------------------------------------------------------------------
const DrawableList *InGameUI::getAllSelectedDrawables() const
{
	return &m_selectedDrawables;
}

//-------------------------------------------------------------------------------------------------
/** Return the list of all the currently selected Drawable pointers. */
//-------------------------------------------------------------------------------------------------
const DrawableList *InGameUI::getAllSelectedLocalDrawables()
{
	m_selectedLocalDrawables.clear();
	for (DrawableList::const_iterator it = m_selectedDrawables.begin(); it != m_selectedDrawables.end(); ++it)
	{
		Drawable *draw = (*it);
		if (draw && draw->getObject() && draw->getObject()->isLocallyControlled())
			m_selectedLocalDrawables.push_back( draw );
	}
	return &m_selectedLocalDrawables;
}

//-------------------------------------------------------------------------------------------------
/** Return pointer to the first selected drawable, if any */
//-------------------------------------------------------------------------------------------------
Drawable *InGameUI::getFirstSelectedDrawable()
{

	// sanity
	if( m_selectedDrawables.empty() )
		return nullptr;  // this is valid, nothing is selected

	return m_selectedDrawables.front();

}

//-------------------------------------------------------------------------------------------------
/** Return true if the selected ID is in the drawable list */
//-------------------------------------------------------------------------------------------------
Bool InGameUI::isDrawableSelected( DrawableID idToCheck ) const
{

	for( DrawableListCIt it = m_selectedDrawables.begin(); it != m_selectedDrawables.end(); ++it )
	{

		if( (*it)->getID() == idToCheck )
			return TRUE;

	}

	return FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Return true if all of the given objects are selected */
//-------------------------------------------------------------------------------------------------
Bool InGameUI::areAllObjectsSelected(const std::vector<Object*>& objectsToCheck) const
{
	for (std::vector<Object*>::const_iterator it = objectsToCheck.begin(); it != objectsToCheck.end(); ++it)
	{
		if (!(*it)->getDrawable()->isSelected())
			return FALSE;
	}

	return TRUE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool InGameUI::isAnySelectedKindOf( KindOfType kindOf ) const
{
	Drawable *draw;

	for( DrawableListCIt it = m_selectedDrawables.begin();
			 it != m_selectedDrawables.end();
			 ++it )
	{

		/** @todo, it seems like we might want to keep a list of drawable pointers so we
		don't have to do this lookup ... it seems "tightly coupled" to me (CBD) */
		// get the drawable from the ID
		draw = *it;
		if( draw && draw->isKindOf( kindOf ) )
			return TRUE;

	}

	return FALSE;  // no selected objects are of the kind of type

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool InGameUI::isAllSelectedKindOf( KindOfType kindOf ) const
{
	Drawable *draw;

	for( DrawableListCIt it = m_selectedDrawables.begin();
			 it != m_selectedDrawables.end();
			 ++it )
	{

		/** @todo, it seems like we might want to keep a list of drawable pointers so we
		don't have to do this lookup ... it seems "tightly coupled" to me (CBD) */
		// get the drawable from the ID
		draw = *it;
		if( draw && draw->isKindOf( kindOf ) == FALSE )
			return FALSE;  // not all objects are of the kind of type

	}

	return TRUE;  // all objects have this kindof bit set in them

}

//-------------------------------------------------------------------------------------------------
/** Set the input enabled/disabled */
//-------------------------------------------------------------------------------------------------
void InGameUI::setInputEnabled( Bool enable )
{
	if(!enable)
		setSelecting( FALSE );

	Bool wasEnabled = m_inputEnabled;

	m_inputEnabled = enable;

	if (wasEnabled && !enable)
	{
		/*
			when input is disabled, clear out all the special "modes" we can be in, since we can miss
			the "exit mode" message during the cinematic. e.g., hold down the ctrl key when a cinematic
			begins, then release it during the cinematic... since input is disabled, we never see the keyup
			and thus think we're still in forceattack when its done, until you jiggle that key again.
			(admittedly, this code will actually do the wrong thing if you were to hold down the ctrl
			key thru the whole cinematic, but that's even more unlikely...)
		*/
		setForceAttackMode( false );			// CTRL
		setForceMoveMode( false );				// apparently unmapped in current CommandMap.ini
		setWaypointMode( false );					// ALT
		setPreferSelectionMode( false );	// SHIFT
		setCameraRotateLeft( false );			// KP4
		setCameraRotateRight( false );		// KP6
		setCameraZoomIn( false );					// KP8
		setCameraZoomOut( false );				// KP2
	}
}

//-------------------------------------------------------------------------------------------------
/** Drawable is being destroyed, clean up any UI elements associated with it. */
//-------------------------------------------------------------------------------------------------
void InGameUI::disregardDrawable( Drawable *draw )
{

	// make sure drawable is no longer selected
	deselectDrawable( draw );

}

//-------------------------------------------------------------------------------------------------
/** This is called after the WindowManager has drawn the menus. */
//-------------------------------------------------------------------------------------------------
void InGameUI::postWindowDraw()
{
	Int hudOffsetX = 0;
	Int hudOffsetY = 0;

	if (m_networkLatencyPointSize > 0 && TheGameLogic->isInMultiplayerGame())
	{
		drawNetworkLatency(hudOffsetX, hudOffsetY);
	}

	if (m_renderFpsPointSize > 0)
	{
		drawRenderFps(hudOffsetX, hudOffsetY);
	}

	if (m_systemTimePointSize > 0)
	{
		drawSystemTime(hudOffsetX, hudOffsetY);
	}

	if ( (m_gameTimePointSize > 0) && !TheGameLogic->isInShellGame() && TheGameLogic->isInGame() )
	{
		drawGameTime();
	}

	if (m_playerInfoListPointSize > 0 && TheGameLogic->isInGame() && TheControlBar->isObserverControlBarOn())
	{
		drawPlayerInfoList();
	}
}

//-------------------------------------------------------------------------------------------------
/** This is called after the UI has been drawn. */
//-------------------------------------------------------------------------------------------------
void InGameUI::postDraw()
{

	// render our display strings for the messages if on
	if( m_messagesOn )
	{
		Int i, x, y;
		Color dropColor;
		UnsignedByte r, g, b, a;

		x = m_messagePosition.x;
		y = m_messagePosition.y;
		for( i = MAX_UI_MESSAGES - 1; i >= 0; i-- )
		{

			if( m_uiMessages[ i ].displayString )
			{

				// make drop color black, but use the alpha setting of the fill color specified (for fading)
				GameGetColorComponents( m_uiMessages[ i ].color, &r, &g, &b, &a );
				dropColor = GameMakeColor( 0, 0, 0, a );

				// draw the text
				m_uiMessages[ i ].displayString->draw( x, y, m_uiMessages[ i ].color, dropColor );

				// increment text spot to next location
				if (GameFont *font = m_uiMessages[ i ].displayString->getFont())
				{
					y += font->height;
				}

			}

		}

	}

	if( m_militarySubtitle )
	{
		ICoord2D pos;
		pos.x = m_militarySubtitle->position.x;
		pos.y = m_militarySubtitle->position.y;
		Color dropColor;
		UnsignedByte r, g, b, a;
		GameGetColorComponents( m_militarySubtitle->color, &r, &g, &b, &a );
		dropColor = GameMakeColor( 0, 0, 0, a );
		for(UnsignedInt i = 0; i <= m_militarySubtitle->currentDisplayString; i++)
		{
			m_militarySubtitle->displayStrings[i]->draw(pos.x,pos.y, m_militarySubtitle->color,dropColor );
			Int height;
			m_militarySubtitle->displayStrings[i]->getSize(nullptr, &height);
			pos.y += height;
		}
		if( m_militarySubtitle->blockDrawn )
		{
			ICoord2D size;
			size.y = m_militarySubtitle->displayStrings[m_militarySubtitle->currentDisplayString]->getFont()->height;
			size.x = size.y * 0.8f;
			TheDisplay->drawFillRect(m_militarySubtitle->blockPos.x, m_militarySubtitle->blockPos.y, size.x, size.y, m_militarySubtitle->color);
		}

	}

	// draw superweapon timers
  // Also responsible for Eva saying "Superweapon is ready for launch"
  //  IMPORTANT: Don't bail out of this block early just because you don't
  //  want to display the timers -- Eva still needs to be checked
	if (TheGameLogic->getFrame() > 0 )
	{
//	Int superweaponCount = 0;
		Int startX = (Int)(m_superweaponPosition.x * TheDisplay->getWidth());
		Int startY = (Int)(m_superweaponPosition.y * TheDisplay->getHeight());

		Int bottomMargin = (Int)( (Real)TheTacticalView->getHeight() * 0.82f );



		Bool marginExceeded = FALSE;

		for (Int i=0; i<MAX_PLAYER_COUNT; ++i)
		{
			Color bgColor = GameMakeColor( 0, 0, 0, 255 );
			for (SuperweaponMap::iterator mapIt = m_superweapons[i].begin(); mapIt != m_superweapons[i].end(); ++mapIt)
			{
				AsciiString templateName = mapIt->first;
				for (SuperweaponList::iterator listIt = mapIt->second.begin(); listIt != mapIt->second.end(); ++listIt)
				{
					SuperweaponInfo *info = *listIt;
					DEBUG_ASSERTCRASH(info, ("No superweapon info!"));
					if (info && !info->m_hiddenByScript && !info->m_hiddenByScience)
					{
						//enforce bottom margin of tactical view
						if ( startY >= bottomMargin)
						{
							UnicodeString ellipsis;
							ellipsis.format(L"...");
							info->setText( ellipsis, ellipsis );
							info->setFont( m_superweaponReadyFont, m_superweaponNormalPointSize, m_superweaponNormalBold );
							info->drawTime( startX,	startY, m_superweaponFlashColor, bgColor );

							marginExceeded = TRUE;
						}

						Object * owningObject = TheGameLogic->findObjectByID(info->m_id);
						if (owningObject)
						{

							// We don't draw our timers until we are finished with construction.
							// It is important that let the SpecialPowerUpdate is add its timer in its constructor,,
							// since the science for it could be added before construction is finished,
							// And thus the timer set to READY before the timer is first drawn, here
							if ( owningObject->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ))
								continue;

							SpecialPowerModuleInterface *module = owningObject->getSpecialPowerModule(info->getSpecialPowerTemplate());
							if (module)
							{
								// found one - draw it
 								Bool isReady = module->isReady();
 								Int readySecs;

								// IsReady includes disabledness, so if you have a 0 timer disabled super, you don't want
 								// the UnsignedInt to wrap around to hundreds of millions of seconds.
 								if( module->getReadyFrame() < TheGameLogic->getFrame() )
									readySecs = 0;
 								else
 									readySecs = (module->getReadyFrame() - TheGameLogic->getFrame()) / static_cast<float>(LOGICFRAMES_PER_SECOND);
								// Yes, integer math.  We can't have float imprecision display 4:01 on a disabled superweapon.

                // Only if we actually changed the ready status do we want to play an Eva event.
                if ( isReady && !info->m_evaReadyPlayed )
                {
                  if ( TheGameLogic->getFrame() > 0 )
                  {
                    SpecialPowerType type = module->getSpecialPowerTemplate()->getSpecialPowerType();

                    Player *localPlayer = ThePlayerList->getLocalPlayer();

                    if( type == SPECIAL_PARTICLE_UPLINK_CANNON || type == SUPW_SPECIAL_PARTICLE_UPLINK_CANNON || type == LAZR_SPECIAL_PARTICLE_UPLINK_CANNON )
                    {
                      if ( localPlayer == owningObject->getControllingPlayer() )
                      {
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Own_ParticleCannon);
                      }
                      else if ( localPlayer->getRelationship(owningObject->getTeam()) != ENEMIES )
                      {
                        // Note: counting relationship NEUTRAL as ally. Not sure if this makes a difference???
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Ally_ParticleCannon);
                      }
                      else
                      {
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Enemy_ParticleCannon);
                      }
                    }
                    else if( type == SPECIAL_NEUTRON_MISSILE || type == NUKE_SPECIAL_NEUTRON_MISSILE || type == SUPW_SPECIAL_NEUTRON_MISSILE )
                    {
                      if ( localPlayer == owningObject->getControllingPlayer() )
                      {
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Own_Nuke);
                      }
                      else if ( localPlayer->getRelationship(owningObject->getTeam()) != ENEMIES )
                      {
                        // Note: counting relationship NEUTRAL as ally. Not sure if this makes a difference???
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Ally_Nuke);
                      }
                      else
                      {
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Enemy_Nuke);
                      }
                    }
                    else if (type == SPECIAL_SCUD_STORM)
                    {
                      if ( localPlayer == owningObject->getControllingPlayer() )
                      {
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Own_ScudStorm);
                      }
                      else if ( localPlayer->getRelationship(owningObject->getTeam()) != ENEMIES )
                      {
                        // Note: counting relationship NEUTRAL as ally. Not sure if this makes a difference???
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Ally_ScudStorm);
                      }
                      else
                      {
                        TheEva->setShouldPlay(EVA_SuperweaponReady_Enemy_ScudStorm);
                      }
                    }
                  }
                  info->m_evaReadyPlayed = true;
                }
                else
                {
                  if ( !isReady )
                    info->m_evaReadyPlayed = false; // Reset Eva for next time
                }

                // draw the text
                if ( !m_superweaponHiddenByScript && !marginExceeded )
                {
                  // Similarly, only checking timers is not truly indicative of readiness.
 								  Bool changeBolding = (readySecs != info->m_timestamp) || (isReady != info->m_ready) || info->m_forceUpdateText;
 								  if (changeBolding)
 								  {
 									  if (isReady)
									  {
										  // go bold - we're good to go
										  info->setFont( m_superweaponReadyFont, m_superweaponReadyPointSize, m_superweaponReadyBold );
									  }
									  else
									  {
										  // if we were at 0, we've just fired - kill the bold
										  if (info->m_timestamp == 0)
										  {
											  info->setFont( m_superweaponNormalFont, m_superweaponNormalPointSize, m_superweaponNormalBold );
										  }
									  }


									  info->m_forceUpdateText = false;
 									  info->m_ready = isReady;
									  info->m_timestamp = readySecs;
                    Int min = readySecs/60;
                    Int sec = readySecs - min*60;
                    AsciiString strIndex;
                    strIndex.format("GUI:%s", templateName.str());
                    UnicodeString name, time;
                    name.format(L"%ls: ", TheGameText->fetch(strIndex.str()).str());
                    time.format(L"%d:%2.2d", min, sec);
                    info->setText(name, time);
                  }

                  if (isReady)
								  {
									  if ( m_superweaponFlashDuration != 0.0f )
									  {
										  if ( TheGameLogic->getFrame() >= m_superweaponLastFlashFrame + (Int)(m_superweaponFlashDuration) )
										  {
											  m_superweaponUsedFlashColor = !m_superweaponUsedFlashColor;
											  m_superweaponLastFlashFrame = TheGameLogic->getFrame();
										  }
										  info->drawName( startX,
											  startY, (m_superweaponUsedFlashColor)?0:m_superweaponFlashColor, bgColor );
										  info->drawTime( startX,
											  startY, (m_superweaponUsedFlashColor)?0:m_superweaponFlashColor, bgColor );
									  }
									  else
									  {
										  info->drawName( startX, startY, 0, bgColor );
										  info->drawTime( startX, startY, 0, bgColor );
									  }
								  }
								  else
								  {
									  info->drawName( startX,	startY, 0, bgColor );
									  info->drawTime( startX, startY, 0, bgColor );
								  }

								  // increment text spot to next location
								  startY += info->getHeight();

                }
                if (info->getSpecialPowerTemplate()->isSharedNSync())
                  break; // Wow, it is almost too easy!
                // This prevents redundant timers for shared powers/superweapons
                // No matter how many specialpowermodules register their timers with me,
                // I will only draw the timer of the first valid one in my list,
                // since they all have the same template, ans they all
                // use the Player::getReadyFrame() functions to stay in sync.
              }
						}
					}
				}
			}
		}
	}

	// draw named timers
	if (TheGameLogic->getFrame() > 0 && m_showNamedTimers)
	{
//		Int namedTimerCount = 0;
		Bool reverseXDir = (m_namedTimerPosition.x >= 0.5f);
		Int startX = (Int)(m_namedTimerPosition.x * TheDisplay->getWidth());
		Int startY = (Int)(m_namedTimerPosition.y * TheDisplay->getHeight());
		Color bgColor = GameMakeColor( 0, 0, 0, 255 );
		for (NamedTimerMapIt mapIt = m_namedTimers.begin(); mapIt != m_namedTimers.end(); ++mapIt)
		{
			AsciiString timerName = mapIt->first;
			NamedTimerInfo *info = mapIt->second;
			DEBUG_ASSERTCRASH(info, ("No namedTimer info!"));
			if (info)
			{
				// found one - draw it
				UnicodeString line;
				Int framesLeft = TheScriptEngine->getCounter(timerName)->value;
				UnsignedInt readyFrame = TheGameLogic->getFrame();
				if (framesLeft > 0)
					readyFrame += framesLeft;
				Int readySecs = (Int)(SECONDS_PER_LOGICFRAME_REAL * (readyFrame - TheGameLogic->getFrame()));
				if ( (info->isCountdown && readySecs != info->timestamp) || (!info->isCountdown && framesLeft != info->timestamp) )
				{
					if (!readySecs && info->isCountdown)
					{
						// go bold - we're good to go
						info->displayString->setFont( TheFontLibrary->getFont( m_namedTimerReadyFont,
							TheGlobalLanguageData->adjustFontSize(m_namedTimerReadyPointSize), m_namedTimerReadyBold ) );
					}
					else
					{
						// if we were at 0, we've just fired - kill the bold
						if (info->timestamp == 0 || info->isCountdown)
						{
							info->displayString->setFont( TheFontLibrary->getFont( m_namedTimerNormalFont,
								TheGlobalLanguageData->adjustFontSize(m_namedTimerNormalPointSize), m_namedTimerNormalBold ) );
						}
					}

					info->timestamp = readySecs;
					Int min = readySecs/60;
					Int sec = readySecs - min*60;

					if (!info->isCountdown)
						line.format(L"%s %d", info->timerText.str(), framesLeft);
					else
					{
						if (sec >= 10)
							line.format(L"%s %d:%d", info->timerText.str(), min, sec);
						else
							line.format(L"%s %d:0%d", info->timerText.str(), min, sec);
					}
					info->displayString->setText(line);
				}

				// draw the text
				Int drawX = startX;
				if (reverseXDir)
					drawX -= info->displayString->getWidth();
				if (!readySecs && info->isCountdown)
				{
					if ( m_namedTimerFlashDuration != 0.0f )
					{
						if ( TheGameLogic->getFrame() >= m_namedTimerLastFlashFrame + (Int)(m_namedTimerFlashDuration) )
						{
							m_namedTimerUsedFlashColor = !m_namedTimerUsedFlashColor;
							m_namedTimerLastFlashFrame = TheGameLogic->getFrame();
						}
						info->displayString->draw( drawX, startY, (m_namedTimerUsedFlashColor)?info->color:m_namedTimerFlashColor, bgColor );
					}
					else
					{
						info->displayString->draw( drawX, startY, info->color, bgColor );
					}
				}
				else
				{
					info->displayString->draw( drawX, startY, info->color, bgColor );
				}

				// increment text spot to next location
				startY -= info->displayString->getFont()->height;
			}
		}
	}

	// draw RMB scroll anchor
	if (TheLookAtTranslator && m_drawRMBScrollAnchor)
	{
		const ICoord2D* anchor = TheLookAtTranslator->getRMBScrollAnchor();
		if (anchor)
		{
			static const Int w = 2;
			static const Int h = 2;
			static const Int r = 4; // ratio
			static const Color mainColor = GameMakeColor(0, 255, 0, 255);
			static const Color dropColor = GameMakeColor(0, 0, 0, 255);
			TheDisplay->drawFillRect( anchor->x-w*r-1, anchor->y-h-1, w*2*r+3, h*2+3, dropColor );
			TheDisplay->drawFillRect( anchor->x-w-1, anchor->y-h*r-1, w*2+3, h*2*r+3, dropColor );
			TheDisplay->drawFillRect( anchor->x-w*r, anchor->y-h, w*2*r+1, h*2+1, mainColor );
			TheDisplay->drawFillRect( anchor->x-w, anchor->y-h*r, w*2+1, h*2*r+1, mainColor );
		}
	}

	//draw superweapon ready multipliers
	TheControlBar->drawSpecialPowerShortcutMultiplierText();

}

//-------------------------------------------------------------------------------------------------
/** Expire a hint of the specified type with the corresponding hint index */
//-------------------------------------------------------------------------------------------------
void InGameUI::expireHint( HintType type, UnsignedInt hintIndex )
{

	if( type == MOVE_HINT )
	{

		// sanity
		if( hintIndex < 0 || hintIndex >= MAX_MOVE_HINTS )
			return;

		m_moveHint[ hintIndex ].sourceID = 0;
		m_moveHint[ hintIndex ].frame = 0;

	}
	else
	{

		// undefined hint type
		DEBUG_CRASH(("undefined hint type"));
		return;

	}

}

//-------------------------------------------------------------------------------------------------
/** Create the control user interface GUI */
//-------------------------------------------------------------------------------------------------
void InGameUI::createControlBar()
{

	TheWindowManager->winCreateFromScript( "ControlBar.wnd" );
	HideControlBar();
/*
	// hide all windows created from this layout
	GameWindow *window = TheWindowManager->winGetWindowList();
	for( ; window; window = window->winGetPrev() )
		window->winHide( TRUE );
*/

}

//-------------------------------------------------------------------------------------------------
/** Create the replay control GUI */
//-------------------------------------------------------------------------------------------------
void InGameUI::createReplayControl()
{

	m_replayWindow = TheWindowManager->winCreateFromScript( "ReplayControl.wnd" );

/*
	// hide all windows created from this layout
	GameWindow *window = TheWindowManager->winGetWindowList();
	for( ; window; window = window->winGetPrev() )
		window->winHide( TRUE );
*/

}

// ------------------------------------------------------------------------------------------------
// InGameUI::playMovie
// ------------------------------------------------------------------------------------------------
void InGameUI::playMovie( const AsciiString& movieName )
{

	stopMovie();

	m_videoStream = TheVideoPlayer->open( movieName );

	if ( m_videoStream == nullptr )
	{
		return;
	}

	m_currentlyPlayingMovie = movieName;
	m_videoBuffer = TheDisplay->createVideoBuffer();

	if (	m_videoBuffer == nullptr ||
				!m_videoBuffer->allocate(	m_videoStream->width(),
													m_videoStream->height())
		)
	{
		stopMovie();
		return;
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::stopMovie()
{
	delete m_videoBuffer;
	m_videoBuffer = nullptr;

	if ( m_videoStream )
	{
		m_videoStream->close();
		m_videoStream = nullptr;
	}

	if (!m_currentlyPlayingMovie.isEmpty()) {
		//TheScriptEngine->notifyOfCompletedVideo(m_currentlyPlayingMovie); // removing sync error source -MDC
		m_currentlyPlayingMovie = AsciiString::TheEmptyString;
	}
}

// ------------------------------------------------------------------------------------------------
// InGameUI::videoBuffer
// ------------------------------------------------------------------------------------------------
VideoBuffer* InGameUI::videoBuffer()
{
	return m_videoBuffer;
}

// ------------------------------------------------------------------------------------------------
// InGameUI::playCameoMovie
// ------------------------------------------------------------------------------------------------
void InGameUI::playCameoMovie( const AsciiString& movieName )
{

	stopCameoMovie();

	m_cameoVideoStream = TheVideoPlayer->open( movieName );

	if ( m_cameoVideoStream == nullptr )
	{
		return;
	}

	m_cameoVideoBuffer = TheDisplay->createVideoBuffer();

	if (	m_cameoVideoBuffer == nullptr ||
				!m_cameoVideoBuffer->allocate(	m_cameoVideoStream->width(),
													m_cameoVideoStream->height())
		)
	{
		stopCameoMovie();
		return;
	}
	GameWindow *window = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey( "ControlBar.wnd:RightHUD" ));
	WinInstanceData *winData = window->winGetInstanceData();
	winData->setVideoBuffer(m_cameoVideoBuffer);
//	window->winHide(FALSE);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void InGameUI::stopCameoMovie()
{
//RightHUD
	//GameWindow *window = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey( "ControlBar.wnd:CameoMovieWindow" ));
	GameWindow *window = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey( "ControlBar.wnd:RightHUD" ));
//	window->winHide(FALSE);
	WinInstanceData *winData = window->winGetInstanceData();
	winData->setVideoBuffer(nullptr);

	delete m_cameoVideoBuffer;
	m_cameoVideoBuffer = nullptr;

	if ( m_cameoVideoStream )
	{
		m_cameoVideoStream->close();
		m_cameoVideoStream = nullptr;
	}

}

// ------------------------------------------------------------------------------------------------
// InGameUI::videoBuffer
// ------------------------------------------------------------------------------------------------
VideoBuffer* InGameUI::cameoVideoBuffer()
{
	return m_cameoVideoBuffer;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void InGameUI::displayCantBuildMessage( LegalBuildCode lbc )
{

	switch( lbc )
	{

		//---------------------------------------------------------------------------------------------
		case LBC_RESTRICTED_TERRAIN:
			message( "GUI:CantBuildRestrictedTerrain" );
			break;

		//---------------------------------------------------------------------------------------------
		case LBC_NOT_FLAT_ENOUGH:
			message( "GUI:CantBuildNotFlatEnough" );
			break;

		//---------------------------------------------------------------------------------------------
		case LBC_OBJECTS_IN_THE_WAY:
			message( "GUI:CantBuildObjectsInTheWay" );
			break;

		//---------------------------------------------------------------------------------------------
		case LBC_TOO_CLOSE_TO_SUPPLIES:
			message( "GUI:CantBuildTooCloseToSupplies" );
			break;

		//---------------------------------------------------------------------------------------------
		case LBC_NO_CLEAR_PATH:
		  message( "GUI:CantBuildNoClearPath" );
			break;

		//---------------------------------------------------------------------------------------------
		case LBC_SHROUD:
			message( "GUI:CantBuildShroud" );
			break;

		//---------------------------------------------------------------------------------------------
		case LBC_GENERIC_FAILURE:
		default:

			message( "GUI:CantBuildThere" );
			break;

	}

}

// ------------------------------------------------------------------------------------------------
// InGameUI::militarySubtitle
// ------------------------------------------------------------------------------------------------
void InGameUI::militarySubtitle( const AsciiString& label, Int duration )
{
	// make sure we don't already have a subtitle up there
	removeMilitarySubtitle();

	// update our history
	UpdateDiplomacyBriefingText(label, FALSE);

	UnicodeString title = TheGameText->fetch(label);

	// make sure we actually will be displaying something
	if( title.isEmpty() || duration <= 0)
	{
		DEBUG_CRASH(("Trying to create a military subtitle but either title is empty (%ls) or duration is <= 0 (%d)",title.str(), duration));
		return;
	}

	// we need some frame info to set our timings
	UnsignedInt currLogicFrame = TheGameLogic->getFrame();
	const int messageTimeout = currLogicFrame + (Int)(((Real)LOGICFRAMES_PER_SECOND * duration)/1000.0f);

	// disable tooltips until this frame, cause we don't want to collide with the military subtitles.
	disableTooltipsUntil(messageTimeout);

	// calculate where this screen position should be since the position being passed in is based off 8x6
	Coord2D multiplier;
	multiplier.x = (Real)TheDisplay->getWidth() / (Real)DEFAULT_DISPLAY_WIDTH;
	multiplier.y = (Real)TheDisplay->getHeight() / (Real)DEFAULT_DISPLAY_HEIGHT;

	// lets bring out the data structure!
	m_militarySubtitle = NEW MilitarySubtitleData;

	m_militarySubtitle->subtitle.set(title);
	m_militarySubtitle->blockDrawn = TRUE;
	m_militarySubtitle->blockBeginFrame = currLogicFrame;
	m_militarySubtitle->lifetime = messageTimeout;
	m_militarySubtitle->blockPos.x =  m_militarySubtitle->position.x = m_militaryCaptionPosition.x * multiplier.x;
	m_militarySubtitle->blockPos.y =  m_militarySubtitle->position.y = m_militaryCaptionPosition.y * multiplier.y;
	m_militarySubtitle->incrementOnFrame = currLogicFrame + (Int)(((Real)LOGICFRAMES_PER_SECOND * TheGlobalLanguageData->m_militaryCaptionDelayMS)/1000.0f);
	m_militarySubtitle->index = 0;
	for (int i = 1; i < MAX_SUBTITLE_LINES; i ++)
		m_militarySubtitle->displayStrings[i] = nullptr;

	m_militarySubtitle->currentDisplayString = 0;
	m_militarySubtitle->displayStrings[0] = TheDisplayStringManager->newDisplayString();
	m_militarySubtitle->displayStrings[0]->reset();
	m_militarySubtitle->displayStrings[0]->setFont(	TheFontLibrary->getFont( m_militaryCaptionTitleFont,
		TheGlobalLanguageData->adjustFontSize(m_militaryCaptionTitlePointSize), m_militaryCaptionTitleBold ) );
	m_militarySubtitle->color = GameMakeColor(m_militaryCaptionColor.red, m_militaryCaptionColor.green, m_militaryCaptionColor.blue, m_militaryCaptionColor.alpha);
}

// ------------------------------------------------------------------------------------------------
// InGameUI::removeMilitarySubtitle
// ------------------------------------------------------------------------------------------------
void InGameUI::removeMilitarySubtitle()
{
	// sanity (is there really such a thing in this world?)
	if(!m_militarySubtitle)
		return;

	clearTooltipsDisabled();

	// loop through and free up the display strings
	for(UnsignedInt i = 0; i <= m_militarySubtitle->currentDisplayString; i ++)
	{
		TheDisplayStringManager->freeDisplayString(m_militarySubtitle->displayStrings[i]);
		m_militarySubtitle->displayStrings[i] = nullptr;
	}

	//delete it man!
	delete m_militarySubtitle;
	m_militarySubtitle= nullptr;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool InGameUI::areSelectedObjectsControllable() const
{
	const DrawableList *selected = getAllSelectedDrawables();

	// loop through all the selected drawables
	const Drawable *draw;
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		// get this drawable
		draw = *it;

		// All selected objects will have the same local controller, so
		// simply return the first one.
		return draw->getObject()->isLocallyControlled();
	}

	// Nothing selected...
	return FALSE;
}

//------------------------------------------------------------------------------
//Resets the camera to default zoom and orientation.
//------------------------------------------------------------------------------
void InGameUI::resetCamera()
{
	TheTacticalView->userResetPivotToGround();
	TheTacticalView->userSetAngleToDefault();
	TheTacticalView->userSetPitchToDefault();
	TheTacticalView->userSetZoomToDefault();
}

//------------------------------------------------------------------------------
//Checks to see if an object can interact with an object in a non-hostile manner. This is currently used by the selection
//translator to determine whether to do something to an object or select it instead based on the context of what is currently
//selected.
//------------------------------------------------------------------------------
Bool InGameUI::canSelectedObjectsNonAttackInteractWithObject( const Object *objectToInteractWith, SelectionRules rule ) const
{
	for( int i = 1; i < NUM_ACTIONTYPES; i++ )
	{
		if( i != ACTIONTYPE_ATTACK_OBJECT )
		{
			if( canSelectedObjectsDoAction( (ActionType)i, objectToInteractWith, rule ) )
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

CanAttackResult InGameUI::getCanSelectedObjectsAttack( ActionType action, const Object *objectToInteractWith, SelectionRules rule, Bool additionalChecking ) const
{
	//Kris: Aug 16, 2003
	//John McDonald added this code back in Oct 09, 2002.
	//Replaced it with palatable code.
	//if( (objectToInteractWith == nullptr) != (action == ACTIONTYPE_SET_RALLY_POINT)) <---BAD CODE
	if( (!objectToInteractWith && action != ACTIONTYPE_SET_RALLY_POINT) || //No object to interact with (and not rally point mode)
			 (objectToInteractWith && action == ACTIONTYPE_SET_RALLY_POINT) )  //Object to interact with (and rally point mode)
	{
		//Sanity check OR can't set a rally point over an object.
		return ATTACKRESULT_NOT_POSSIBLE;
	}

	// get selected list of drawables
	const DrawableList *selected = getAllSelectedDrawables();

	// set up counters for rule checking
	Int count = 0;
	CanAttackResult bestResult = ATTACKRESULT_NOT_POSSIBLE;
	CanAttackResult worstResult = ATTACKRESULT_POSSIBLE;

	// loop through all the selected drawables
	Drawable *other;
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{

		// get this drawable
		other = *it;
		count++;

		switch( action )
		{
			case ACTIONTYPE_ATTACK_OBJECT:
			{
				//additionalChecking is TRUE only if force attack mode is on.
				CanAttackResult result = 	TheActionManager->getCanAttackObject( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER,
									additionalChecking ? ATTACK_NEW_TARGET_FORCED : ATTACK_NEW_TARGET );

				if( result > bestResult )
				{
					//Best result is used for the rule: SELECTION_ANY
					bestResult = result;
				}
				if( result < worstResult )
				{
					//Worst result is used for the rule: SELECTION_ALL
					worstResult = result;
				}
				break;
			}

			case ACTIONTYPE_NONE:
			case ACTIONTYPE_GET_REPAIRED_AT:
			case ACTIONTYPE_DOCK_AT:
			case ACTIONTYPE_GET_HEALED_AT:
			case ACTIONTYPE_REPAIR_OBJECT:
			case ACTIONTYPE_RESUME_CONSTRUCTION:
			case ACTIONTYPE_COMBATDROP_INTO:
			case ACTIONTYPE_ENTER_OBJECT:
			case ACTIONTYPE_HIJACK_VEHICLE:
			case ACTIONTYPE_SABOTAGE_BUILDING:
			case ACTIONTYPE_CONVERT_OBJECT_TO_CARBOMB:
			case ACTIONTYPE_CAPTURE_BUILDING:
			case ACTIONTYPE_DISABLE_VEHICLE_VIA_HACKING:
			case ACTIONTYPE_MAKE_DEFECTOR:
			case ACTIONTYPE_SET_RALLY_POINT:
			default:
				DEBUG_CRASH( ("Called InGameUI::getCanSelectedObjectsAttack() with actiontype %d. Only accepts attack types! Should you be calling InGameUI::canSelectedObjectsDoAction() instead?", action) );
				return ATTACKRESULT_INVALID_SHOT;

		}

	}

	if( count > 0 )
	{
		if( rule == SELECTION_ANY )
		{
			return bestResult;
		}
		return worstResult;
	}

	// no can do!
	return ATTACKRESULT_NOT_POSSIBLE;
}

//------------------------------------------------------------------------------
//Wrapper function that checks a specific action.
//------------------------------------------------------------------------------
Bool InGameUI::canSelectedObjectsDoAction( ActionType action, const Object *objectToInteractWith, SelectionRules rule, Bool additionalChecking ) const
{

	//Kris: Aug 16, 2003
	//John McDonald added this code back in Oct 09, 2002. This code is SO wrong that it should
	//be a firing offense. Strangely enough, this code has gone unnoticed for nearly a year
	//and nearly two projects. I'm fixing this now by moving it to the rally point code...
	//because it would be nice if a saboteur could actually sabotage a building via a
	//commandbutton.
	//if( (objectToInteractWith == nullptr) != (action == ACTIONTYPE_SET_RALLY_POINT))
	if( (!objectToInteractWith && action != ACTIONTYPE_SET_RALLY_POINT) || //No object to interact with (and not rally point mode)
			 (objectToInteractWith && action == ACTIONTYPE_SET_RALLY_POINT) )  //Object to interact with (and rally point mode)
	{
		//Sanity check OR can't set a rally point over an object.
		return FALSE;
	}

	// get selected list of drawables
	const DrawableList *selected = getAllSelectedDrawables();

	// set up counters for rule checking
	Int count = 0;
	Int qualify = 0;

	// loop through all the selected drawables
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{

		// get this drawable
		Drawable *other = *it;
		count++;
		Bool success = FALSE;

		switch( action )
		{
			case ACTIONTYPE_NONE:
				//However strange this might be, it is always possible to do "nothing"
				//although I can't think of why this would be needed...
				return TRUE;
			case ACTIONTYPE_GET_REPAIRED_AT:
				success = TheActionManager->canGetRepairedAt( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_DOCK_AT:
				success = TheActionManager->canDockAt( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_GET_HEALED_AT:
				success = TheActionManager->canGetHealedAt( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				if( success )
				{
					ContainModuleInterface *contain = objectToInteractWith->getContain();
					if( contain && contain->isHealContain() )
					{
						//This container is only used for the purposes of healing and we cannot
						//enter it normally -- this is NOT a transport!
						success = false;
					}
				}
				break;
			case ACTIONTYPE_REPAIR_OBJECT:
			{
				ObjectID currentRepairer = objectToInteractWith->getSoleHealingBenefactor();
				success = ( TheActionManager->canRepairObject( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER )
										&& ( currentRepairer == INVALID_ID || currentRepairer == other->getObject()->getID() ) );
											// unless someone else is already healing it...
											// please note that this add'l test is left out of canRepairObject() since canRepairObject
											// gets called from within the Dozer/WorkerAIUpdates' stateMachines as they continue the repair process.
											// This remains true.
				break;
			}
			case ACTIONTYPE_RESUME_CONSTRUCTION:
				success = TheActionManager->canResumeConstructionOf( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_COMBATDROP_INTO:
				success = TheActionManager->canEnterObject( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER, COMBATDROP_INTO );
				break;
			case ACTIONTYPE_ENTER_OBJECT:
				//additionalChecking is TRUE only if we want to check if transport is full first.
				success = TheActionManager->canEnterObject( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER, additionalChecking ? CHECK_CAPACITY : DONT_CHECK_CAPACITY );
				break;
			case ACTIONTYPE_ATTACK_OBJECT:
				DEBUG_CRASH( ("Called InGameUI::canSelectedObjectsDoAction() with ACTIONTYPE_ATTACK_OBJECT. You must use InGameUI::getCanSelectedObjectsAttack() instead.") );
				return FALSE;
			case ACTIONTYPE_HIJACK_VEHICLE:
				success = TheActionManager->canHijackVehicle( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_SABOTAGE_BUILDING:
				success = TheActionManager->canSabotageBuilding( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_CONVERT_OBJECT_TO_CARBOMB:
				success = TheActionManager->canConvertObjectToCarBomb( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_CAPTURE_BUILDING:
				success = TheActionManager->canCaptureBuilding( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_DISABLE_VEHICLE_VIA_HACKING:
				success = TheActionManager->canDisableVehicleViaHacking( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
#ifdef ALLOW_SURRENDER
			case ACTIONTYPE_PICK_UP_PRISONER:
				success = TheActionManager->canPickUpPrisoner( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
#endif
			case ACTIONTYPE_STEAL_CASH_VIA_HACKING:
				success = TheActionManager->canStealCashViaHacking( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_DISABLE_BUILDING_VIA_HACKING:
				success = TheActionManager->canDisableBuildingViaHacking( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_MAKE_DEFECTOR:
				success = TheActionManager->canMakeObjectDefector( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER );
				break;
			case ACTIONTYPE_SET_RALLY_POINT:
			{
				Object *obj = other->getObject();
				if (!obj) {
					success = false;
					break;
				}
				success = (obj->isKindOf(KINDOF_AUTO_RALLYPOINT) && obj->isLocallyControlled());
				break;
			}
		}

		if( success )
		{
			if( rule == SELECTION_ANY )
			{
				return TRUE;
			}

			++qualify;
		}
	}

	//If the rule is all must qualify, do the check now and return success
	//only if all the selected units qualified.
	if( rule == SELECTION_ALL && count > 0 && qualify == count )
	{
		return TRUE;
	}

	// no can do!
	return FALSE;
}

//------------------------------------------------------------------------------
Bool InGameUI::canSelectedObjectsDoSpecialPower( const CommandButton *command, const Object *objectToInteractWith, const Coord3D *position, SelectionRules rule, UnsignedInt commandOptions, Object* ignoreSelObj ) const
{
	//Get the special power template.
	const SpecialPowerTemplate *spTemplate = command->getSpecialPowerTemplate();

	//Order of precedence:
	//1) NO TARGET OR POS
	//2) COMMAND_OPTION_NEED_OBJECT_TARGET
	//3) NEED_TARGET_POS
	Bool doAtPosition = BitIsSet( command->getOptions(), NEED_TARGET_POS );
	Bool doAtObject = BitIsSet( command->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET );

	//Sanity checks
	if( doAtObject && !objectToInteractWith )
	{
		return false;
	}
	if( doAtPosition && !position )
	{
		return false;
	}

	// get selected list of drawables
	Drawable* ignoreSelDraw = ignoreSelObj ? ignoreSelObj->getDrawable() : nullptr;

	DrawableList tmpList;
	if (ignoreSelDraw)
		tmpList.push_back(ignoreSelDraw);

	const DrawableList* selected = (!tmpList.empty()) ? &tmpList : getAllSelectedDrawables();

	// set up counters for rule checking
	Int count = 0;
	Int qualify = 0;

	// loop through all the selected drawables
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{

		// get this drawable
		Drawable* other = *it;
		count++;

		if( !doAtObject && !doAtPosition )
		{
			if( TheActionManager->canDoSpecialPower( other->getObject(), spTemplate, CMD_FROM_PLAYER, commandOptions ) )
			{
				//This is the no target version
				if( rule == SELECTION_ANY )
				{
					return true;
				}
				qualify++;
			}
		}
		else if( doAtObject )
		{
			if( TheActionManager->canDoSpecialPowerAtObject( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER, spTemplate, commandOptions ) )
			{
				//This requires a object target
				if( rule == SELECTION_ANY )
				{
					return true;
				}
				qualify++;
			}
		}
		else if( doAtPosition )
		{
			if( TheActionManager->canDoSpecialPowerAtLocation( other->getObject(), position, CMD_FROM_PLAYER, spTemplate, objectToInteractWith, commandOptions ) )
			{
				//This requires a valid location.
				if( rule == SELECTION_ANY )
				{
					return true;
				}
				qualify++;
			}
		}
	}
	if( rule == SELECTION_ALL && count > 0 && qualify == count )
	{
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
Bool InGameUI::canSelectedObjectsOverrideSpecialPowerDestination( const Coord3D *loc, SelectionRules rule, SpecialPowerType spType ) const
{
	// set up counters for rule checking
	Int count = 0;
	Int qualify = 0;

	// get selected list of drawables
	const DrawableList *selected = getAllSelectedDrawables();

	// loop through all the selected drawables
	Drawable *other;
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{

		// get this drawable
		other = *it;
		count++;

		if( TheActionManager->canOverrideSpecialPowerDestination( other->getObject(), loc, spType, CMD_FROM_PLAYER ) )
		{
			if( rule == SELECTION_ANY )
			{
				return true;
			}
			qualify++;
		}
	}
	if( rule == SELECTION_ALL && count > 0 && qualify == count )
	{
		return true;
	}
	return false;
}


//------------------------------------------------------------------------------
Bool InGameUI::canSelectedObjectsEffectivelyUseWeapon( const CommandButton *command, const Object *objectToInteractWith, const Coord3D *position, SelectionRules rule ) const
{
	//Get the special power template.
	WeaponSlotType slot = command->getWeaponSlot();

	//Order of precedence:
	//1) NO TARGET OR POS
	//2) COMMAND_OPTION_NEED_OBJECT_TARGET
	//3) NEED_TARGET_POS
	Bool doAtPosition = BitIsSet( command->getOptions(), NEED_TARGET_POS );
	Bool doAtObject = BitIsSet( command->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET );

	//Sanity checks
	if( doAtObject && !objectToInteractWith )
	{
		return false;
	}
	if( doAtPosition && !position )
	{
		return false;
	}

	// get selected list of drawables
	const DrawableList *selected = getAllSelectedDrawables();

	// set up counters for rule checking
	Int count = 0;
	Int qualify = 0;

	// loop through all the selected drawables
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{

		// get this drawable
		Drawable *other = *it;
		count++;

		if( !doAtObject && !doAtPosition )
		{
			if( TheActionManager->canFireWeapon( other->getObject(), slot, CMD_FROM_PLAYER ) )
			{
				//This is the no target version
				if( rule == SELECTION_ANY )
				{
					return true;
				}
				qualify++;
			}
		}
		else if( doAtObject )
		{
			if( TheActionManager->canFireWeaponAtObject( other->getObject(), objectToInteractWith, CMD_FROM_PLAYER, slot ) )
			{
				//This requires a object target
				if( rule == SELECTION_ANY )
				{
					return true;
				}
				qualify++;
			}
		}
		else if( doAtPosition )
		{
			if( TheActionManager->canFireWeaponAtLocation( other->getObject(), position, CMD_FROM_PLAYER, slot, objectToInteractWith ) )
			{
				//This requires a valid location.
				if( rule == SELECTION_ANY )
				{
					return true;
				}
				qualify++;
			}
		}
	}
	if( rule == SELECTION_ALL && count > 0 && qualify == count )
	{
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
Int InGameUI::selectAllUnitsByTypeAcrossRegion( IRegion2D *region, KindOfMaskType mustBeSet, KindOfMaskType mustBeClear )
{
	KindOfSelectionData data;
	Int newSelectionCount = 0;
	Int oldSelectionCount = getAllSelectedDrawables()->size();

	data.m_mustbeSet = mustBeSet;
	data.m_mustbeClear = mustBeClear;

	if (region)
	{
		TheTacticalView->iterateDrawablesInRegion(region, kindOfUnitSelection, (void *)&data);
		newSelectionCount += data.newlySelectedDrawables.size();
	}
	else
	{
		// loop over the map
		Drawable *temp = TheGameClient->firstDrawable();
		while( temp )
		{
			if( kindOfUnitSelection( temp, (void *)&data) )
			{
				newSelectionCount ++;
			}

			temp = temp->getNextDrawable();
		}
	}
	setDisplayedMaxWarning( FALSE );

	if (newSelectionCount > 0)
	{
		// create selected message
		GameMessage *teamMsg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP );

		teamMsg->appendBooleanArgument( (oldSelectionCount == 0) ? TRUE : FALSE );

		const Drawable *draw;

		//Loop through each drawable add append it's objectID to the event.
		for( DrawableListCIt it = data.newlySelectedDrawables.begin(); it != data.newlySelectedDrawables.end(); ++it )
		{
			draw = *it;
			if( draw && draw->getObject() )
			{
				teamMsg->appendObjectIDArgument( draw->getObject()->getID() );
			}
		}
	}

	return newSelectionCount;
}

// ------------------------------------------------------------------------------------------------
/** Selects matching units on the screen */
// ------------------------------------------------------------------------------------------------
Int InGameUI::selectMatchingAcrossRegion( IRegion2D *region )
{
	const DrawableList *selected = getAllSelectedDrawables();

	/* loop through all the selected drawables and create a set of all the objects,
	   so that you only iterate once through each type of object
	*/

	const Drawable *draw;

	//std::set<AsciiString> drawableList;
	std::set<const ThingTemplate*> drawableList;
	Bool carBomb = FALSE;

	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		// get this drawable
		draw = *it;
		if( draw && draw->getObject() && draw->getObject()->isLocallyControlled() )
		{
			// Use the Object's thing template, doing so will prevent weirdness for disguised vehicles.
			drawableList.insert( draw->getObject()->getTemplate() );
			if( draw->getObject()->testStatus( OBJECT_STATUS_IS_CARBOMB ) )
			{
				carBomb = TRUE;
			}
		}
	}

	if (drawableList.empty())
		return -1; // nothing useful selected to begin with - don't bother iterating

	std::set<const ThingTemplate*>::iterator iter;
	const ThingTemplate *templateName;

	// now use the list to select across screen
	MatchingUnitSelectionData data;
	Int newSelectionCount = 0;

	for( iter = drawableList.begin(); iter != drawableList.end(); ++iter )
	{
		// get this drawable
		templateName = *iter;

		data.templateToSelect = templateName;
		data.isCarBomb        = carBomb;
		if (region)
			newSelectionCount +=TheTacticalView->iterateDrawablesInRegion(region, similarUnitSelection, (void *)&data);
		else
		{
			// loop over the map
			Drawable *temp = TheGameClient->firstDrawable();
			while( temp )
			{
				newSelectionCount += similarUnitSelection( temp, (void *)&data);
				temp = temp->getNextDrawable();
			}
		}
		setDisplayedMaxWarning( FALSE );
	}

	if (newSelectionCount > 0)
	{
		// create selected message
		GameMessage *teamMsg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND );
		// not creating a new team so pass in false
		teamMsg->appendBooleanArgument( FALSE );

		//Loop through each drawable add append it's objectID to the event.
		for( DrawableListCIt it = data.newlySelectedDrawables.begin(); it != data.newlySelectedDrawables.end(); ++it )
		{
			draw = *it;
			if( draw && draw->getObject() )
			{
				teamMsg->appendObjectIDArgument( draw->getObject()->getID() );
			}
		}
	}

	return newSelectionCount;

}

// ------------------------------------------------------------------------------------------------
Int InGameUI::selectAllUnitsByTypeAcrossScreen(KindOfMaskType mustBeSet, KindOfMaskType mustBeClear)
{
	/// When implementing this, obey TheInGameUI->getMaxSelectCount() if it is > 0

	IRegion2D region;
	ICoord2D origin;
	ICoord2D size;

	TheTacticalView->getOrigin( &origin.x, &origin.y );
	size.x = TheTacticalView->getWidth();
	size.y = TheTacticalView->getHeight();

	buildRegion( &origin, &size, &region );

	Int numSelected = selectAllUnitsByTypeAcrossRegion(&region, mustBeSet, mustBeClear);
	if (numSelected == -1)
	{
		UnicodeString msgStr = TheGameText->fetch( "GUI:NothingSelected" );
		message( msgStr );
	}
	else if (numSelected == 0)
	{
	}
	else
	{
		UnicodeString msgStr = TheGameText->fetch( "GUI:SelectedAcrossScreen" );
		message( msgStr );
	}
	return numSelected;
}

// ------------------------------------------------------------------------------------------------
/** Selects matching units on the screen */
// ------------------------------------------------------------------------------------------------
Int InGameUI::selectMatchingAcrossScreen()
{
	/// When implementing this, obey TheInGameUI->getMaxSelectCount() if it is > 0

	IRegion2D region;
	ICoord2D origin;
	ICoord2D size;

	TheTacticalView->getOrigin( &origin.x, &origin.y );
	size.x = TheTacticalView->getWidth();
	size.y = TheTacticalView->getHeight();

	buildRegion( &origin, &size, &region );

	Int numSelected = selectMatchingAcrossRegion(&region);
	if (numSelected == -1)
	{
		UnicodeString msgStr = TheGameText->fetch( "GUI:NothingSelected" );
		message( msgStr );
	}
	else if (numSelected == 0)
	{
	}
	else
	{
		UnicodeString msgStr = TheGameText->fetch( "GUI:SelectedAcrossScreen" );
		message( msgStr );
	}
	return numSelected;
}

//-------------------------------------------------------------------------------------------------
Int InGameUI::selectAllUnitsByTypeAcrossMap(KindOfMaskType mustBeSet, KindOfMaskType mustBeClear)
{
	/// When implementing this, obey TheInGameUI->getMaxSelectCount() if it is > 0
	Int numSelected = selectAllUnitsByTypeAcrossRegion(nullptr, mustBeSet, mustBeClear);
	if (numSelected == -1)
	{
		UnicodeString msgStr = TheGameText->fetch( "GUI:NothingSelected" );
		message( msgStr );
	}
	else if (numSelected == 0)
	{
		Drawable *draw = getFirstSelectedDrawable();
		if( !draw || !draw->getObject() || !draw->getObject()->isKindOf( KINDOF_STRUCTURE ) )
		{
			UnicodeString msgStr = TheGameText->fetch( "GUI:SelectedAcrossMap" );
			message( msgStr );
		}
	}
	else
	{
		UnicodeString msgStr = TheGameText->fetch( "GUI:SelectedAcrossMap" );
		message( msgStr );
	}
	return numSelected;
}

//-------------------------------------------------------------------------------------------------
/** Selects matching units across map */
//-------------------------------------------------------------------------------------------------
Int InGameUI::selectMatchingAcrossMap()
{
	/// When implementing this, obey TheInGameUI->getMaxSelectCount() if it is > 0
	Int numSelected = selectMatchingAcrossRegion(nullptr);
	if (numSelected == -1)
	{
		UnicodeString msgStr = TheGameText->fetch( "GUI:NothingSelected" );
		message( msgStr );
	}
	else if (numSelected == 0)
	{
		Drawable *draw = getFirstSelectedDrawable();
		if( !draw || !draw->getObject() || !draw->getObject()->isKindOf( KINDOF_STRUCTURE ) )
		{
			UnicodeString msgStr = TheGameText->fetch( "GUI:SelectedAcrossMap" );
			message( msgStr );
		}
	}
	else
	{
		UnicodeString msgStr = TheGameText->fetch( "GUI:SelectedAcrossMap" );
		message( msgStr );
	}
	return numSelected;
}

//-------------------------------------------------------------------------------------------------
Int InGameUI::selectAllUnitsByType(KindOfMaskType mustBeSet, KindOfMaskType mustBeClear)
{
	/// When implementing this, obey TheInGameUI->getMaxSelectCount() if it is > 0
	Int numSelected = selectAllUnitsByTypeAcrossScreen(mustBeSet, mustBeClear);
	if (numSelected == -1)
	{
		return numSelected;
	}

	if (numSelected == 0)
	{
		Int numSelectedAcrossMap = selectAllUnitsByTypeAcrossMap(mustBeSet, mustBeClear);
		return numSelectedAcrossMap;
	}
	return numSelected;
}

//-------------------------------------------------------------------------------------------------
/** Selects matching units, either on screen or across map.  When called by pressing 'T',
    their is not a way to tell if the game is supposed to select across the screen, or
    across the map.  For mouse clicks, i.e. Alt + click or double click, we can directly call
    selectMatchingAcrossScreen or selectMatchingAcrossMap */
//-------------------------------------------------------------------------------------------------
Int InGameUI::selectUnitsMatchingCurrentSelection()
{
	/// When implementing this, obey TheInGameUI->getMaxSelectCount() if it is > 0
	Int numSelected = selectMatchingAcrossScreen();
	if (numSelected == -1)
		return numSelected;
	if (numSelected == 0)
	{
		Int numSelectedAcrossMap = selectMatchingAcrossMap();
		//if (numSelectedAcrossMap < 1)
		//{
			//UnicodeString message = TheGameText->fetch( "GUI:NothingSelected" );
			//TheInGameUI->message( message );
		//}
		return numSelectedAcrossMap;
	}
	return numSelected;

}

//-----------------------------------------------------------------------------
/**
 * Given an "anchor" point and the current mouse position (dest),
 * construct a valid 2D bounding region.
 */
//-----------------------------------------------------------------------------------
void InGameUI::buildRegion( const ICoord2D *anchor, const ICoord2D *dest, IRegion2D *region )
{
	// build rectangular region defined by the drag selection
	if (anchor->x < dest->x)
	{
		region->lo.x = anchor->x;
		region->hi.x = dest->x;
	}
	else
	{
		region->lo.x = dest->x;
		region->hi.x = anchor->x;
	}

	if (anchor->y < dest->y)
	{
		region->lo.y = anchor->y;
		region->hi.y = dest->y;
	}
	else
	{
		region->lo.y = dest->y;
		region->hi.y = anchor->y;
	}
}

//-------------------------------------------------------------------------------------------------
/** Add a new floating text to our list */
//-------------------------------------------------------------------------------------------------
void InGameUI::addFloatingText(const UnicodeString& text,const Coord3D *pos, Color color)
{
	if( TheGameLogic->getDrawIconUI() )
	{
		FloatingTextData *newFTD = newInstance( FloatingTextData );
		newFTD->m_frameCount = 0;
		newFTD->m_color = color;
		newFTD->m_pos3D.x = pos->x;
		newFTD->m_pos3D.z = pos->z;
		newFTD->m_pos3D.y = pos->y;
		newFTD->m_text = text;
		newFTD->m_dString->setText(text);


		if(m_floatingTextTimeOut <= 0)
			newFTD->m_frameTimeOut = TheGameLogic->getFrame() +  DEFAULT_FLOATING_TEXT_TIMEOUT;
		else
			newFTD->m_frameTimeOut = TheGameLogic->getFrame() +  m_floatingTextTimeOut;

		m_floatingTextList.push_front( newFTD ); // add to the list
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
#if defined(RTS_DEBUG)
inline Bool isClose(Real a, Real b) { return fabs(a-b) <= 1.0f; }
inline Bool isClose(const Coord3D& a, const Coord3D& b)
{
		return	isClose(a.x, b.x) &&
			isClose(a.y, b.y) &&
			isClose(a.z, b.z);
}
void InGameUI::DEBUG_addFloatingText(const AsciiString& text, const Coord3D * pos, Color color)
{
	const Int POINTSIZE = 8;
	const Int LEADING = 0;

	Coord3D posToUse = *pos;

try_again:
	for (FloatingTextListIt it = m_floatingTextList.begin(); it != m_floatingTextList.end(); ++it)
	{
		if (isClose((*it)->m_pos3D, posToUse))
		{
			posToUse.z -= (POINTSIZE + LEADING);
			goto try_again;
		}
	}

	FloatingTextData *newFTD = newInstance( FloatingTextData );
	newFTD->m_color = color;
	newFTD->m_pos3D.x = posToUse.x;
	newFTD->m_pos3D.y = posToUse.y;
	newFTD->m_pos3D.z = posToUse.z;
	UnicodeString translate;
	translate.translate(text);
	newFTD->m_text = translate;
	newFTD->m_dString->setText(translate);
	newFTD->m_dString->setFont(TheWindowManager->winFindFont( "Arial", POINTSIZE, FALSE ));

	if(m_floatingTextTimeOut <= 0)
		newFTD->m_frameTimeOut = TheGameLogic->getFrame() +  DEFAULT_FLOATING_TEXT_TIMEOUT;
	else
		newFTD->m_frameTimeOut = TheGameLogic->getFrame() +  m_floatingTextTimeOut;

	m_floatingTextList.push_front( newFTD ); // add to the list

	//DEBUG_LOG(("%s",text.str()));
}
#endif

//-------------------------------------------------------------------------------------------------
/** modify the position of our floating text */
//-------------------------------------------------------------------------------------------------
void InGameUI::updateFloatingText()
{
	FloatingTextData *ftd;		// pointer to our floating point data
	UnsignedInt currLogicFrame = TheGameLogic->getFrame();			// the current logic frame
	UnsignedByte r, g, b, a;	// we'll need to break apart our color so we can modify the alpha
	Int amount;								// The amount we'll change the alpha
	static UnsignedInt lastLogicFrameUpdate = currLogicFrame;		// We need to make sure our current frame is different then our last frame we updated.

	// only update the position if we're incrementing frames
	if(lastLogicFrameUpdate == currLogicFrame)
		return;

	lastLogicFrameUpdate = currLogicFrame;

	// Loop through our floating text list
	for(FloatingTextListIt it = m_floatingTextList.begin(); it != m_floatingTextList.end();)
	{
		ftd = *it;

		// move it up
		++ftd->m_frameCount;

		// fade the text
		if( currLogicFrame > ftd->m_frameTimeOut)
		{
			// modify the color
			GameGetColorComponents(ftd->m_color, &r, &g, &b, &a);
			amount = REAL_TO_INT( (currLogicFrame - ftd->m_frameTimeOut) * m_floatingTextMoveVanishRate);
			if(a - amount < 0)
				a = 0;
			else
				a -= amount;
			ftd->m_color = GameMakeColor(r, g, b, a);
			// if we have 0 alpha delete it
			if( a <= 0)
			{
				it = m_floatingTextList.erase(it);
				deleteInstance(ftd);
				continue; // don't do the ++it below
			}

		}
		// increase our iterator
		++it;

	}

}

//-------------------------------------------------------------------------------------------------
/** Iterates through and draws each floating text */
//-------------------------------------------------------------------------------------------------
void InGameUI::drawFloatingText()
{
	FloatingTextData *ftd;
	// loop through and draw all the texts
	for(FloatingTextListIt it = m_floatingTextList.begin(); it != m_floatingTextList.end(); ++it)
	{
		ftd = *it;
		ICoord2D pos;
		const Int playerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();

		// which PartitionManager cells are we looking at?
		Int pCX, pCY;
		ThePartitionManager->worldToCell(ftd->m_pos3D.x, ftd->m_pos3D.y, &pCX, &pCY);

		// translate it's 3d pos into a 2d screen pos
		if( TheTacticalView->worldToScreen(&ftd->m_pos3D, &pos)
			&& ftd->m_dString
			&& ThePartitionManager->getShroudStatusForPlayer(playerIndex, pCX, pCY) == CELLSHROUD_CLEAR )
		{
			pos.y -= ftd->m_frameCount * m_floatingTextMoveUpSpeed;
			Color dropColor;
			UnsignedByte r, g, b, a;
			Int width;

			// make drop color black, but use the alpha setting of the fill color specified (for fading)
			GameGetColorComponents( ftd->m_color, &r, &g, &b, &a );
			dropColor = GameMakeColor( 0, 0, 0, a );
			ftd->m_dString->getSize(&width, nullptr);
			// draw it!
			ftd->m_dString->draw(pos.x - (width / 2), pos.y, ftd->m_color,dropColor);
		}

	}
}

//-------------------------------------------------------------------------------------------------
/** ittereate through and clear out the list of floating text */
//-------------------------------------------------------------------------------------------------
void InGameUI::clearFloatingText()
{
	FloatingTextData *ftd;
	// loop through and draw all the texts
	for(FloatingTextListIt it = m_floatingTextList.begin(); it != m_floatingTextList.end();)
	{
		ftd = *it;
		it = m_floatingTextList.erase(it);
		deleteInstance(ftd);
	}

}

//-------------------------------------------------------------------------------------------------
/** If we want to use the default text color, then we call this function */
//-------------------------------------------------------------------------------------------------
void InGameUI::popupMessage( const AsciiString& message, Int x, Int y, Int width, Bool pause, Bool pauseMusic)
{
	popupMessage( message, x, y, width, m_popupMessageColor, pause, pauseMusic);
}

//-------------------------------------------------------------------------------------------------
/** initialize, and popup a message box to the user */
//-------------------------------------------------------------------------------------------------
void InGameUI::popupMessage( const AsciiString& identifier, Int x, Int y, Int width, Color textColor, Bool pause, Bool pauseMusic)
{
	if(m_popupMessageData)
		clearPopupMessageData();

	UpdateDiplomacyBriefingText(identifier, FALSE);

	UnicodeString message = TheGameText->fetch(identifier);

	m_popupMessageData = newInstance( PopupMessageData );
	m_popupMessageData->message = message;
	// x and why are passed in as a percentage of the screen, convert to screen coords
	if( x > 100 )
		x = 100;
	if( x < 0 )
		x = 0;

	if( y > 100 )
		y = 100;
	if( y < 0 )
		y = 0;

	m_popupMessageData->x = TheDisplay->getWidth() * (INT_TO_REAL(x) / 100);
	m_popupMessageData->y = TheDisplay->getHeight() * (INT_TO_REAL(y) / 100);
	// cap the lower limit of the width
	if(width < 50)
		width = 50;
	m_popupMessageData->width = width;
	m_popupMessageData->textColor = textColor;
	m_popupMessageData->pause = pause;
	m_popupMessageData->pauseMusic = pauseMusic;

	if( pause )
		TheGameLogic->setGamePaused(TRUE, pauseMusic);

	m_popupMessageData->layout = TheWindowManager->winCreateLayout("InGamePopupMessage.wnd");
	m_popupMessageData->layout->runInit();
}

//-------------------------------------------------------------------------------------------------
/** take care of the logic of clearing the popupMessageData */
//-------------------------------------------------------------------------------------------------
void InGameUI::clearPopupMessageData()
{
	if(!m_popupMessageData)
		return;
	if(m_popupMessageData->layout)
	{
		m_popupMessageData->layout->destroyWindows();
		deleteInstance(m_popupMessageData->layout);
		m_popupMessageData->layout = nullptr;
	}
	if( m_popupMessageData->pause )
		TheGameLogic->setGamePaused(FALSE, m_popupMessageData->pauseMusic);
	deleteInstance(m_popupMessageData);
	m_popupMessageData = nullptr;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
/** Floating Text Constructor */
//-------------------------------------------------------------------------------------------------
FloatingTextData::FloatingTextData()
{
	m_color = 0;
	m_frameCount = 0;
	m_frameTimeOut = 0;
	m_pos3D.zero();
	m_text.clear();
	m_dString = TheDisplayStringManager->newDisplayString();
}

//-------------------------------------------------------------------------------------------------
/** Floating Text Destructor */
//-------------------------------------------------------------------------------------------------
FloatingTextData::~FloatingTextData()
{
	if(m_dString)
		TheDisplayStringManager->freeDisplayString( m_dString );
	m_dString = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// WORLD ANIMATION DATA ///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WorldAnimationData::WorldAnimationData()
{

	m_anim = nullptr;
	m_worldPos.zero();
	m_expireFrame = 0;
	m_options = WORLD_ANIM_NO_OPTIONS;
	m_zRisePerSecond = 0.0f;

}

// ------------------------------------------------------------------------------------------------
/** Add a 2D animation at a spot in the world */
// ------------------------------------------------------------------------------------------------
void InGameUI::addWorldAnimation( Anim2DTemplate *animTemplate,
																	const Coord3D *pos,
																	WorldAnimationOptions options,
																	Real durationInSeconds,
																	Real zRisePerSecond )
{

	// sanity
	if( animTemplate == nullptr || pos == nullptr || durationInSeconds <= 0.0f )
		return;

	// allocate a new world animation data struct
	// (huh huh, he said "wad")
	WorldAnimationData *wad = NEW WorldAnimationData;
	if( wad == nullptr )
		return;

	// allocate a new animation instance
	Anim2D *anim = newInstance(Anim2D)( animTemplate, TheAnim2DCollection );

	// assign all data
	wad->m_anim = anim;
	wad->m_expireFrame = TheGameLogic->getFrame() + (durationInSeconds * static_cast<float>(LOGICFRAMES_PER_SECOND));
	wad->m_options = options;
	wad->m_worldPos = *pos;
	wad->m_zRisePerSecond = zRisePerSecond;

	// add to list
	m_worldAnimationList.push_front( wad );

}

// ------------------------------------------------------------------------------------------------
/** Delete all world animations */
// ------------------------------------------------------------------------------------------------
void InGameUI::clearWorldAnimations()
{
	// iterate through all entries and delete the animation data
	for( WorldAnimationListIterator it = m_worldAnimationList.begin();
			 it != m_worldAnimationList.end(); /*empty*/ )
	{

		WorldAnimationData *wad = *it;

		// delete the animation instance
		deleteInstance(wad->m_anim);

		// delete the world animation data
		delete wad;

		it = m_worldAnimationList.erase( it );

	}

}

static const UnsignedInt FRAMES_BEFORE_EXPIRE_TO_FADE = static_cast<float>(LOGICFRAMES_PER_SECOND) * 1;
// ------------------------------------------------------------------------------------------------
/** Update all world animations and draw the visible ones */
// ------------------------------------------------------------------------------------------------
void InGameUI::updateAndDrawWorldAnimations()
{
	// TheSuperHackers @tweak bobtista World animation Z-rise is now decoupled from the render update.
	const Real zRiseTimeScale = TheFramePacer->getActualLogicTimeScaleOverFpsRatio();

	// go through all animations
	for( WorldAnimationListIterator it = m_worldAnimationList.begin();
			 it != m_worldAnimationList.end(); /*empty*/ )
	{

		// get data
		WorldAnimationData *wad = *it;

		//
		// see if it's time to expire this animation based on animation type and options or
		// the expire frame
		//
		if( TheGameLogic->getFrame() >= wad->m_expireFrame ||
				(BitIsSet( wad->m_options, WORLD_ANIM_PLAY_ONCE_AND_DESTROY ) &&
				 BitIsSet( wad->m_anim->getStatus(), ANIM_2D_STATUS_COMPLETE )) )
		{

			// delete this element and continue
			deleteInstance(wad->m_anim);
			delete wad;
			it = m_worldAnimationList.erase( it );
			continue;

		}

		// update the Z value
		if( wad->m_zRisePerSecond )
		{
			wad->m_worldPos.z += wad->m_zRisePerSecond / LOGICFRAMES_PER_SECOND * zRiseTimeScale;
		}

		//
		// don't bother going forward with the draw process if this location is shrouded for
		// the local player
		//
		const Int playerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();

		if( ThePartitionManager->getShroudStatusForPlayer( playerIndex, &wad->m_worldPos ) != CELLSHROUD_CLEAR )
		{

			++it;
			continue;

		}

		// update translucency value
		if( BitIsSet( wad->m_options, WORLD_ANIM_FADE_ON_EXPIRE ) )
		{

			// see if we should be setting the translucency value
			UnsignedInt framesTillExpire = wad->m_expireFrame - TheGameLogic->getFrame();
			if( framesTillExpire < FRAMES_BEFORE_EXPIRE_TO_FADE )
			{

				// compute alpha level so that we're totally gone by the expire frame
				Real alpha = INT_TO_REAL( framesTillExpire ) / INT_TO_REAL( FRAMES_BEFORE_EXPIRE_TO_FADE );
				wad->m_anim->setAlpha( alpha );

			}

		}

		// project the point to screen space
		ICoord2D screen;
		if( TheTacticalView->worldToScreen( &wad->m_worldPos, &screen ) == TRUE )
		{
			UnsignedInt width = wad->m_anim->getCurrentFrameWidth();
			UnsignedInt height = wad->m_anim->getCurrentFrameHeight();

			// scale the width and height given the camera zoom level
			// TheSuperHackers @todo Rework this with sane values. scaler=1.3 originally came from TheTacticalView::getMaxZoom()
			constexpr Real scaler = 1.3f;
			Real zoomScale = scaler / TheTacticalView->getZoom();
			width *= zoomScale;
			height *= zoomScale;

			// adjust the screen position to draw so the image is centered at the location
			screen.x -= width / 2;
			screen.y -= height / 2;

			// draw the animation
			wad->m_anim->draw( screen.x, screen.y, width, height );

		}

		// go to the next element in the list
		++it;

	}

}


Object *InGameUI::findIdleWorker( Object *obj)
{
	if(!obj)
		return nullptr;

	Int index = obj->getControllingPlayer()->getPlayerIndex();
	if(m_idleWorkers[index].empty())
		return nullptr;

	ObjectListIt it = m_idleWorkers[index].begin();
	while(it != m_idleWorkers[index].end())
	{
		Object *itObj = *it;
		if(itObj == obj)
		{
			return itObj;
			break;
		}
		++it;
	}
	return nullptr;
}

void InGameUI::addIdleWorker( Object *obj )
{
	if(!obj)
		return;

	if(findIdleWorker(obj))
		return;

	Int index = obj->getControllingPlayer()->getPlayerIndex();
	m_idleWorkers[index].push_back(obj);
}

void InGameUI::removeIdleWorker( Object *obj, Int playerNumber )
{
	if(!obj)
		return;
	if(playerNumber < 0 || playerNumber >= MAX_PLAYER_COUNT)  // we're leaving the game, so this is all screwed
		return;

	if(m_idleWorkers[playerNumber].empty())
		return;


	ObjectListIt it = m_idleWorkers[playerNumber].begin();
	while(it != m_idleWorkers[playerNumber].end())
	{
		Object *itObj = *it;
		if(itObj == obj)
		{
			m_idleWorkers[playerNumber].erase(it);
			return;
		}
		++it;
	}
}

void InGameUI::selectNextIdleWorker()
{
	Player* player = rts::getObservedOrLocalPlayer();
	Int index = player->getPlayerIndex();

	if(m_idleWorkers[index].empty())
	{
		DEBUG_CRASH(("InGameUI::selectNextIdleWorker We're trying to select a worker when our list is empty for player %ls", player->getPlayerDisplayName().str()));
		return;
	}
	Object *selectThisObject = nullptr;

	if(getSelectCount() == 0 || getSelectCount() > 1)
	{
		selectThisObject = *m_idleWorkers[index].begin();
		// If our idle worker is contained by anything, we need to select the container instead.
		while (selectThisObject->getContainedBy())
			selectThisObject = selectThisObject->getContainedBy();
	}
	else
	{
		Drawable *selectedDrawable = getFirstSelectedDrawable();
		// TheSuperHackers @tweak Stubbjax 22/07/2025 Idle worker iteration now correctly identifies and
		// iterates contained idle workers. Previous iteration logic would not go past contained workers,
		// and was not guaranteed to select top-level containers.
		ObjectPtrVector uniqueIdleWorkers = getUniqueIdleWorkers(m_idleWorkers[index]);

		ObjectPtrVector::iterator it = uniqueIdleWorkers.begin();
		while(it != uniqueIdleWorkers.end())
		{
			Object *itObj = *it;
			if(itObj == selectedDrawable->getObject())
			{
				++it;
				if(it != uniqueIdleWorkers.end())
					selectThisObject = *it;
				else
					selectThisObject = *uniqueIdleWorkers.begin();
				break;
			}
			++it;
		}
		// if we had something selected that wasn't a worker, we'll get here
		if (!selectThisObject)
			selectThisObject = uniqueIdleWorkers.front();
	}
	DEBUG_ASSERTCRASH(selectThisObject, ("InGameUI::selectNextIdleWorker Could not select the next IDLE worker"));
	if(selectThisObject)
	{
		DEBUG_ASSERTCRASH(selectThisObject->getContainedBy() == nullptr, ("InGameUI::selectNextIdleWorker Selected idle object should not be contained"));
		deselectAllDrawables();
		GameMessage *teamMsg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP );


		//New group or add to group? Passed in value is true if we are creating a new group.
		teamMsg->appendBooleanArgument( TRUE );

		teamMsg->appendObjectIDArgument( selectThisObject->getID() );

		selectDrawable( selectThisObject->getDrawable() );

		/*// removed because we're already playing a select sound... left in, just in case i"m wrong.
		// play the units sound
				const AudioEventRTS *soundEvent = selectThisObject->getTemplate()->getVoiceSelect();
				if (soundEvent)
				{
					TheAudio->addAudioEvent( soundEvent );
				}*/

		// center on the unit
		TheTacticalView->userLookAt(selectThisObject->getPosition());
	}
}

// Finds unique selectables to avoid selecting the same or a previous container if multiple idle workers are contained.
ObjectPtrVector InGameUI::getUniqueIdleWorkers(const ObjectList& idleWorkers)
{
	ObjectPtrVector uniqueIdleWorkers;
	uniqueIdleWorkers.reserve(idleWorkers.size());

	for (ObjectList::const_iterator it = idleWorkers.begin(); it != idleWorkers.end(); ++it)
	{
		Object* itObj = *it;
		while (itObj->getContainedBy())
			itObj = itObj->getContainedBy();

		stl::push_back_unique(uniqueIdleWorkers, itObj);
	}

	return uniqueIdleWorkers;
}

Int InGameUI::getIdleWorkerCount()
{
	Player* player = rts::getObservedOrLocalPlayer();
	Int index = player->getPlayerIndex();
	return m_idleWorkers[index].size();
}

void InGameUI::showIdleWorkerLayout()
{
	if (!m_idleWorkerWin)
	{
		m_idleWorkerWin = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("ControlBar.wnd:ButtonIdleWorker"));
		DEBUG_ASSERTCRASH(m_idleWorkerWin, ("InGameUI::showIdleWorkerLayout could not find IdleWorker.wnd to load"));
		return;
	}

	m_idleWorkerWin->winEnable(TRUE);

	m_currentIdleWorkerDisplay = getIdleWorkerCount();

//	if(m_currentIdleWorkerDisplay < 1)
//		GadgetButtonSetText(m_idleWorkerWin, UnicodeString::TheEmptyString);
//	else
//	{
//		UnicodeString number;
//		number.format(L"%d",m_currentIdleWorkerDisplay);
//		GadgetButtonSetText(m_idleWorkerWin, number);
//	}
}
void InGameUI::hideIdleWorkerLayout()
{
	if(!m_idleWorkerWin)
		return;
	GadgetButtonSetText(m_idleWorkerWin, UnicodeString::TheEmptyString);
	m_idleWorkerWin->winEnable(FALSE);
	m_currentIdleWorkerDisplay = -1;
}

void InGameUI::updateIdleWorker()
{
	Int idleCount = getIdleWorkerCount();

	if(idleCount > 0 && m_currentIdleWorkerDisplay != idleCount)
		showIdleWorkerLayout();

	if(idleCount <= 0 && m_idleWorkerWin)
		hideIdleWorkerLayout();
}

void InGameUI::resetIdleWorker()
{
	if(m_idleWorkerWin)
	{
		GadgetButtonSetText(m_idleWorkerWin, UnicodeString::TheEmptyString);
	}
	m_currentIdleWorkerDisplay = -1;
	for(Int i = 0; i < MAX_PLAYER_COUNT; ++i)
	{
		m_idleWorkers[i].clear();
	}

}

void InGameUI::recreateControlBar()
{
	GameWindow *win = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("ControlBar.wnd"));
	deleteInstance(win);

	m_idleWorkerWin = nullptr;

	createControlBar();

	delete TheControlBar;
	TheControlBar = NEW ControlBar;
	TheControlBar->init();
}

void InGameUI::refreshCustomUiResources()
{
	refreshNetworkLatencyResources();
	refreshRenderFpsResources();
	refreshSystemTimeResources();
	refreshGameTimeResources();
	refreshPlayerInfoListResources();
}

void InGameUI::refreshNetworkLatencyResources()
{
	if (!m_networkLatencyString)
	{
		m_networkLatencyString = TheDisplayStringManager->newDisplayString();
		m_lastNetworkLatencyFrames = ~0u;
	}

	m_networkLatencyPointSize = TheGlobalData->m_networkLatencyFontSize;
	Int adjustedNetworkLatencyFontSize = TheGlobalLanguageData->adjustFontSize(m_networkLatencyPointSize);
	GameFont* latencyFont = TheWindowManager->winFindFont(m_networkLatencyFont, adjustedNetworkLatencyFontSize, m_networkLatencyBold);
	m_networkLatencyString->setFont(latencyFont);
}

void InGameUI::refreshRenderFpsResources()
{
	if (!m_renderFpsString)
	{
		m_renderFpsString = TheDisplayStringManager->newDisplayString();
		m_lastRenderFps = ~0u;
		m_lastRenderFpsUpdateMs = 0u;
	}

	if (!m_renderFpsLimitString)
	{
		m_renderFpsLimitString = TheDisplayStringManager->newDisplayString();
		m_lastRenderFpsLimit = ~0u;
	}

	m_renderFpsPointSize = TheGlobalData->m_renderFpsFontSize;
	Int adjustedRenderFpsFontSize = TheGlobalLanguageData->adjustFontSize(m_renderFpsPointSize);
	GameFont *fpsFont = TheWindowManager->winFindFont(m_renderFpsFont, adjustedRenderFpsFontSize, m_renderFpsBold);
	m_renderFpsString->setFont(fpsFont);
	m_renderFpsLimitString->setFont(fpsFont);

	if (m_renderFpsPointSize > 0)
	{
		updateRenderFpsString();
	}
}

void InGameUI::refreshSystemTimeResources()
{
	if (!m_systemTimeString)
	{
		m_systemTimeString = TheDisplayStringManager->newDisplayString();
	}

	m_systemTimePointSize = TheGlobalData->m_systemTimeFontSize;
	Int adjustedSystemTimeFontSize = TheGlobalLanguageData->adjustFontSize(m_systemTimePointSize);
	GameFont* systemTimeFont = TheWindowManager->winFindFont(m_systemTimeFont, adjustedSystemTimeFontSize, m_systemTimeBold);
	m_systemTimeString->setFont(systemTimeFont);
}

void InGameUI::refreshGameTimeResources()
{
	if (!m_gameTimeString)
	{
		m_gameTimeString = TheDisplayStringManager->newDisplayString();
	}

	if (!m_gameTimeFrameString)
	{
		m_gameTimeFrameString = TheDisplayStringManager->newDisplayString();
	}

	m_gameTimePointSize = TheGlobalData->m_gameTimeFontSize;
	Int adjustedGameTimeFontSize = TheGlobalLanguageData->adjustFontSize(m_gameTimePointSize);
	GameFont* gameTimeFont = TheWindowManager->winFindFont(m_gameTimeFont, adjustedGameTimeFontSize, m_gameTimeBold);
	m_gameTimeString->setFont(gameTimeFont);
	m_gameTimeFrameString->setFont(gameTimeFont);
}

void InGameUI::refreshPlayerInfoListResources()
{
	m_playerInfoListPointSize = TheGlobalData->m_playerInfoListFontSize;
	Int adjustedPlayerInfoListPointSize = TheGlobalLanguageData->adjustFontSize(m_playerInfoListPointSize);
	m_playerInfoList.init(m_playerInfoListFont, adjustedPlayerInfoListPointSize, m_playerInfoListBold);
}

void InGameUI::disableTooltipsUntil(UnsignedInt frameNum)
{
	if (frameNum > m_tooltipsDisabledUntil)
		m_tooltipsDisabledUntil = frameNum;
}

void InGameUI::clearTooltipsDisabled()
{
	m_tooltipsDisabledUntil = 0;
}

Bool InGameUI::areTooltipsDisabled() const
{
	return (TheGameLogic->getFrame() < m_tooltipsDisabledUntil);
}


WindowMsgHandledType IdleWorkerSystem( GameWindow *window, UnsignedInt msg,
																				WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{
		//---------------------------------------------------------------------------------------------
		case GWM_INPUT_FOCUS:
		{
			// if we're givin the opportunity to take the keyboard focus we must say we don't want it
			if( mData1 == TRUE )
				*(Bool *)mData2 = FALSE;
			break;

		}
		//---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			static NameKeyType buttonSelectID = NAMEKEY( "IdleWorker.wnd:ButtonSelectNextIdleWorker" );
			if (control && control->winGetWindowId() == buttonSelectID)
			{
				TheInGameUI->selectNextIdleWorker();
			}
			break;

		}

		//---------------------------------------------------------------------------------------------
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;

}


void InGameUI::updateRenderFpsString()
{
	const UnsignedInt renderFps = (UnsignedInt)(TheDisplay->getAverageFPS() + 0.5f);
	if (renderFps != m_lastRenderFps)
	{
		UnicodeString fpsStr;
		fpsStr.format(L"%u", renderFps);
		m_renderFpsString->setText(fpsStr);
		m_lastRenderFps = renderFps;
	}
}

void InGameUI::drawNetworkLatency(Int &x, Int &y)
{
	const UnsignedInt networkLatencyFrames = TheNetwork->getRunAhead();

	if (networkLatencyFrames != m_lastNetworkLatencyFrames)
	{
		UnicodeString latencyStr;
		latencyStr.format(L"%u", networkLatencyFrames);
		m_networkLatencyString->setText(latencyStr);
		m_lastNetworkLatencyFrames = networkLatencyFrames;
	}

	// TheSuperHackers @info at the HUD anchor this draws inline and advances x otherwise uses configured position
	if (isAtHudAnchorPos(m_networkLatencyPosition))
	{
		m_networkLatencyString->draw(kHudAnchorX + x, kHudAnchorY + y, m_networkLatencyColor, m_networkLatencyDropColor);
		x += m_networkLatencyString->getWidth() + kHudGapPx;
	}
	else
	{
		m_networkLatencyString->draw(m_networkLatencyPosition.x, m_networkLatencyPosition.y, m_networkLatencyColor, m_networkLatencyDropColor);
	}
}

void InGameUI::drawRenderFps(Int &x, Int &y)
{
	if (m_renderFpsRefreshMs > 0u)
	{
		const UnsignedInt nowMs = timeGetTime();
		const UnsignedInt deltaMs = nowMs - m_lastRenderFpsUpdateMs;
		if (deltaMs >= m_renderFpsRefreshMs)
		{
			m_lastRenderFpsUpdateMs = nowMs;
			updateRenderFpsString();
		}
	}
	else
	{
		updateRenderFpsString();
	}

	UnsignedInt renderFpsLimit = 0u;
	if (TheGlobalData->m_useFpsLimit)
	{
		renderFpsLimit = (UnsignedInt)TheFramePacer->getFramesPerSecondLimit();
		if (renderFpsLimit == RenderFpsPreset::UncappedFpsValue)
		{
			renderFpsLimit = 0u;
		}
	}
	if (renderFpsLimit != m_lastRenderFpsLimit)
	{
		UnicodeString fpsLimitStr;
		fpsLimitStr.format(L"[%u]", renderFpsLimit);
		m_renderFpsLimitString->setText(fpsLimitStr);
		m_lastRenderFpsLimit = renderFpsLimit;
	}

	// TheSuperHackers @info at the HUD anchor this draws inline and advances x otherwise uses configured position
	if (isAtHudAnchorPos(m_renderFpsPosition))
	{
		const Int drawY = kHudAnchorY + y;

		m_renderFpsString->draw(kHudAnchorX + x, drawY, m_renderFpsColor, m_renderFpsDropColor);
		x += m_renderFpsString->getWidth();
		m_renderFpsLimitString->draw(kHudAnchorX + x, drawY, m_renderFpsLimitColor, m_renderFpsDropColor);
		x += m_renderFpsLimitString->getWidth() + kHudGapPx;
	}
	else
	{
		m_renderFpsString->draw(m_renderFpsPosition.x, m_renderFpsPosition.y, m_renderFpsColor, m_renderFpsDropColor);
		m_renderFpsLimitString->draw(m_renderFpsPosition.x + m_renderFpsString->getWidth(), m_renderFpsPosition.y, m_renderFpsLimitColor, m_renderFpsDropColor);
	}
}

void InGameUI::drawSystemTime(Int &x, Int &y)
{
	// current system time
	SYSTEMTIME systemTime;
	GetLocalTime( &systemTime );

	UnicodeString TimeString;
	TimeString.format(L"%2.2d:%2.2d:%2.2d", systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
	m_systemTimeString->setText(TimeString);

	// TheSuperHackers @info at the HUD anchor this draws inline and advances x otherwise uses configured position
	if (isAtHudAnchorPos(m_systemTimePosition))
	{
		m_systemTimeString->draw(kHudAnchorX + x, kHudAnchorY + y, m_systemTimeColor, m_systemTimeDropColor);
		x += m_systemTimeString->getWidth() + kHudGapPx;
	}
	else
	{
		m_systemTimeString->draw(m_systemTimePosition.x, m_systemTimePosition.y, m_systemTimeColor, m_systemTimeDropColor);
	}
}

void InGameUI::drawGameTime()
{
	Int currentFrame = TheGameLogic->getFrame();
	Int gameSeconds = (Int) (SECONDS_PER_LOGICFRAME_REAL * currentFrame );
	Int hours = gameSeconds / 60 / 60;
	Int minutes = (gameSeconds / 60) % 60;
	Int seconds = gameSeconds % 60;
	Int frame = currentFrame % 30;

    UnicodeString gameTimeString;
    gameTimeString.format(L"%2.2d:%2.2d:%2.2d", hours, minutes, seconds);
    m_gameTimeString->setText(gameTimeString);

	UnicodeString gameTimeFrameString;
    gameTimeFrameString.format(L".%2.2d", frame);
    m_gameTimeFrameString->setText(gameTimeFrameString);

	// TheSuperHackers @info this implicitly offsets the game timer from the right instead of left of the screen
	int horizontalTimerOffset = TheDisplay->getWidth() - (Int)m_gameTimePosition.x - m_gameTimeString->getWidth() - m_gameTimeFrameString->getWidth();
	int horizontalFrameOffset = TheDisplay->getWidth() - (Int)m_gameTimePosition.x - m_gameTimeFrameString->getWidth();

	m_gameTimeString->draw(horizontalTimerOffset, m_gameTimePosition.y, m_gameTimeColor, m_gameTimeDropColor);
	m_gameTimeFrameString->draw(horizontalFrameOffset, m_gameTimePosition.y, GameMakeColor(180,180,180,255), m_gameTimeDropColor);
}

void InGameUI::drawPlayerInfoList()
{
	const Int baseX = (Int)(m_playerInfoListPosition.x * TheDisplay->getWidth());
	const Int baseY = (Int)(m_playerInfoListPosition.y * TheDisplay->getHeight());
	const Int lineH = m_playerInfoList.labels[PlayerInfoList::LabelType_Team]->getFont()->height;
	const Int columnGap = static_cast<Int>(lineH * (6.0f / 12.0f) + 0.5f);

	AsciiString name;
	UnicodeString playerInfoListValue;
	Int rowCount = 0;
	Int maxValueWidths[PlayerInfoList::LabelType_Count] = {0};
	Color rowColors[MAX_PLAYER_COUNT] = {0};
	Int nameValueWidth[MAX_PLAYER_COUNT] = {0};
	Int column;

	for (Int slotIndex = 0; slotIndex < MAX_SLOTS && rowCount < MAX_PLAYER_COUNT; ++slotIndex)
	{
		name.format("player%d", slotIndex);
		const NameKeyType key = TheNameKeyGenerator->nameToKey(name);
		Player *player = ThePlayerList->findPlayerWithNameKey(key);
		if (!player || player->isPlayerObserver())
			continue;

		const GameSlot *slot = TheGameInfo->getConstSlot(slotIndex);

		const Int row = rowCount++;
		const UnsignedInt teamValue = (slot && slot->getTeamNumber() >= 0) ? static_cast<UnsignedInt>(slot->getTeamNumber() + 1) : 0;
		const UnsignedInt moneyValue = player->getMoney()->countMoney();
		const UnsignedInt rankValue = static_cast<UnsignedInt>(player->getRankLevel());
		const UnsignedInt xpValue = static_cast<UnsignedInt>(player->getSkillPoints());
		const UnicodeString nameValue = player->getPlayerDisplayName();

		const UnsignedInt currentValues[] = {teamValue, moneyValue, rankValue, xpValue};
		for (column = 0; column < ARRAY_SIZE(currentValues); ++column)
		{
			UnsignedInt &lastValue = m_playerInfoList.lastValues.values[column][row];
			if (lastValue != currentValues[column])
			{
				playerInfoListValue.format(L"%u", currentValues[column]);
				m_playerInfoList.values[column][row]->setText(playerInfoListValue);
				lastValue = currentValues[column];
			}
		}
		if (m_playerInfoList.lastValues.name[row].isEmpty())
		{
			m_playerInfoList.values[PlayerInfoList::ValueType_Name][row]->setText(nameValue);
			m_playerInfoList.lastValues.name[row] = nameValue;
		}

		for (column = 0; column < PlayerInfoList::LabelType_Count; ++column)
		{
			const Int valueWidth = m_playerInfoList.values[column][row]->getWidth();
			if (maxValueWidths[column] < valueWidth)
				maxValueWidths[column] = valueWidth;
		}

		rowColors[row] = player->getPlayerColor();
		nameValueWidth[row] = m_playerInfoList.values[PlayerInfoList::ValueType_Name][row]->getWidth();
	}

	Int labelWidths[PlayerInfoList::LabelType_Count];
	Int columnLabelX[PlayerInfoList::LabelType_Count];
	Int labelX = baseX;
	for (column = 0; column < PlayerInfoList::LabelType_Count; ++column)
	{
		labelWidths[column] = m_playerInfoList.labels[column]->getWidth();
		columnLabelX[column] = labelX;
		labelX += labelWidths[column] + maxValueWidths[column] + columnGap;
	}

	Int drawY = baseY - ((rowCount * lineH) / 2);
	for (Int row = 0; row < rowCount; ++row)
	{
		TheDisplay->drawFillRect(baseX, drawY, labelX - baseX + nameValueWidth[row], lineH, GameMakeColor(0, 0, 0, m_playerInfoListBackgroundAlpha));

		for (column = 0; column < PlayerInfoList::LabelType_Count; ++column)
		{
			m_playerInfoList.labels[column]->draw(columnLabelX[column], drawY, m_playerInfoListLabelColor, m_playerInfoListDropColor);
			m_playerInfoList.values[column][row]->draw(columnLabelX[column] + labelWidths[column], drawY, m_playerInfoListValueColor, m_playerInfoListDropColor);
		}

		m_playerInfoList.values[PlayerInfoList::ValueType_Name][row]->draw(labelX, drawY, rowColors[row], m_playerInfoListDropColor);

		drawY += lineH;
	}
}
