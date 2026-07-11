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

// FILE: ControlBar.cpp ///////////////////////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Context sensitive command interface
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#define DEFINE_GUI_COMMAND_NAMES
#define DEFINE_COMMAND_OPTION_NAMES
#define DEFINE_WEAPONSLOTTYPE_NAMES
#define DEFINE_RADIUSCURSOR_NAMES

#include "Common/ActionManager.h"
#include "Common/FramePacer.h"
#include "Common/GameType.h"
#include "Common/MultiplayerSettings.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Override.h"
#include "Common/PlayerTemplate.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ProductionPrerequisite.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Upgrade.h"
#include "Common/Recorder.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/OCLUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Module/RebuildHoleBehavior.h"
#include "GameLogic/ScriptEngine.h"

#include "GameClient/AnimateWindowManager.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarScheme.h"
#include "GameClient/Drawable.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameText.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetProgressBar.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/InGameUI.h"
#include "GameClient/WindowVideoManager.h"
#include "GameClient/ControlBarResizer.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/HotKey.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/GUICallbacks.h"

#include "GameNetwork/GameInfo.h"


// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////
ControlBar *TheControlBar = nullptr;

const Image* ControlBar::m_rankVeteranIcon	= nullptr;
const Image* ControlBar::m_rankEliteIcon		= nullptr;
const Image* ControlBar::m_rankHeroicIcon		= nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////////
// CommandButton //////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const FieldParse CommandButton::s_commandButtonFieldParseTable[] =
{

	{ "Command",							CommandButton::parseCommand, nullptr, offsetof( CommandButton, m_command ) },
	{ "Options",							INI::parseBitString32,		   TheCommandOptionNames, offsetof( CommandButton, m_options ) },
	{ "Object",								INI::parseThingTemplate,		 nullptr, offsetof( CommandButton, m_thingTemplate ) },
	{ "Upgrade",							INI::parseUpgradeTemplate,	 nullptr, offsetof( CommandButton, m_upgradeTemplate ) },
	{ "WeaponSlot",						INI::parseLookupList,				 TheWeaponSlotTypeNamesLookupList, offsetof( CommandButton, m_weaponSlot ) },
	{ "MaxShotsToFire",				INI::parseInt,							 nullptr, offsetof( CommandButton, m_maxShotsToFire ) },
	{ "Science",							INI::parseScienceVector,					 nullptr, offsetof( CommandButton, m_science ) },
	{ "SpecialPower",					INI::parseSpecialPowerTemplate,			 nullptr, offsetof( CommandButton, m_specialPower ) },
	{ "TextLabel",						INI::parseAsciiString,			 nullptr, offsetof( CommandButton, m_textLabel ) },
	{ "DescriptLabel",				INI::parseAsciiString,			 nullptr, offsetof( CommandButton, m_descriptionLabel ) },
	{ "PurchasedLabel",				INI::parseAsciiString,			 nullptr, offsetof( CommandButton, m_purchasedLabel ) },
	{ "ConflictingLabel",			INI::parseAsciiString,			 nullptr, offsetof( CommandButton, m_conflictingLabel ) },
	{ "ButtonImage",					INI::parseAsciiString,			 nullptr, offsetof( CommandButton, m_buttonImageName ) },
	{ "CursorName",						INI::parseAsciiString,			 nullptr, offsetof( CommandButton, m_cursorName ) },
	{ "InvalidCursorName",		INI::parseAsciiString,       nullptr, offsetof( CommandButton, m_invalidCursorName ) },
	{ "ButtonBorderType",			INI::parseLookupList,				 CommandButtonMappedBorderTypeNames, offsetof( CommandButton, m_commandButtonBorder ) },
	{ "RadiusCursorType",			INI::parseIndexList,				 TheRadiusCursorNames, offsetof( CommandButton, m_radiusCursor ) },
	{ "UnitSpecificSound",		INI::parseAudioEventRTS,		 nullptr, offsetof( CommandButton, m_unitSpecificSound ) },

	{ nullptr,						nullptr,												 nullptr, 0 }

};
static void commandButtonTooltip(GameWindow *window,
													WinInstanceData *instData,
													UnsignedInt mouse)
{
	TheControlBar->showBuildTooltipLayout(window);
}

/// mark the UI as dirty so the context of everything is re-evaluated
void ControlBar::markUIDirty()
{
  m_UIDirty = TRUE;

#if defined(RTS_DEBUG)
	UnsignedInt now = TheGameLogic->getFrame();
	if( now == m_lastFrameMarkedDirty )
	{
		//Do nothing.
	}
	else if( now == m_lastFrameMarkedDirty + 1 )
	{
		m_consecutiveDirtyFrames++;
	}
	else
	{
		m_consecutiveDirtyFrames = 1;
	}
	m_lastFrameMarkedDirty = now;

	if( m_consecutiveDirtyFrames > 20 )
	{
		DEBUG_CRASH( ("Serious flaw in interface system! Either new code or INI has caused the interface to be marked dirty every frame. This problem actually causes the interface to completely lockup not allowing you to click normal game buttons.") );
	}

#endif
}

Player* ControlBar::getCurrentlyViewedPlayer()
{
	if (isObserverControlBarOn())
		return getObserverLookAtPlayer();

	return ThePlayerList->getLocalPlayer();
}

Relationship ControlBar::getCurrentlyViewedPlayerRelationship(const Team* team)
{
	if (Player* player = getCurrentlyViewedPlayer())
		return player->getRelationship(team);

	return NEUTRAL;
}

void ControlBar::populatePurchaseScience( Player* player )
{
//	TheInGameUI->deselectAllDrawables();

	const CommandSet *commandSet1;
	const CommandSet *commandSet3;
	const CommandSet *commandSet8;
	Int i;
	if(TheScriptEngine->isGameEnding())
		return;
	// get command set
	if(!player ||!player->getPlayerTemplate() || player->getPlayerTemplate()->getPurchaseScienceCommandSetRank1().isEmpty() ||
			player->getPlayerTemplate()->getPurchaseScienceCommandSetRank3().isEmpty() ||
			player->getPlayerTemplate()->getPurchaseScienceCommandSetRank8().isEmpty())
		return;
	commandSet1 = findCommandSet(player->getPlayerTemplate()->getPurchaseScienceCommandSetRank1()); // TEMP WILL CHANGE TO PROPER WAY ONCE WORKING
	commandSet3 = findCommandSet(player->getPlayerTemplate()->getPurchaseScienceCommandSetRank3()); // TEMP WILL CHANGE TO PROPER WAY ONCE WORKING
	commandSet8 = findCommandSet(player->getPlayerTemplate()->getPurchaseScienceCommandSetRank8()); // TEMP WILL CHANGE TO PROPER WAY ONCE WORKING

	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_1; i++ )
		if (m_sciencePurchaseWindowsRank1[i] != nullptr)
			m_sciencePurchaseWindowsRank1[i]->winHide(TRUE);

	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_3; i++ )
		if (m_sciencePurchaseWindowsRank3[i] != nullptr)
			m_sciencePurchaseWindowsRank3[i]->winHide(TRUE);

	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_8; i++ )
		if (m_sciencePurchaseWindowsRank8[i] != nullptr)
			m_sciencePurchaseWindowsRank8[i]->winHide(TRUE);

	// if no command set match is found hide all the buttons
	if( commandSet1 == nullptr ||
			commandSet3 == nullptr ||
			commandSet8 == nullptr )
		return;

	// populate the button with commands defined
	const CommandButton *commandButton;
	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_1; i++ )
	{
		if (m_sciencePurchaseWindowsRank1[i] == nullptr)
			continue;

		// get command button
		commandButton = commandSet1->getCommandButton(i);

		// if button is not present, just hide the window
		if( commandButton == nullptr || BitIsSet( commandButton->getOptions(), SCRIPT_ONLY ) )
		{
			// hide window on interface
			m_sciencePurchaseWindowsRank1[ i ]->winHide( TRUE );
		}
		else
		{
			// make sure the window is not hidden
			m_sciencePurchaseWindowsRank1[ i ]->winHide( FALSE );

			// Disable by default
			m_sciencePurchaseWindowsRank1[ i ]->winEnable( FALSE );

			// populate the visible button with data from the command button

			setControlCommand( m_sciencePurchaseWindowsRank1[ i ], commandButton );
			if (!commandButton->getScienceVec().empty())
			{
				ScienceType	st = commandButton->getScienceVec()[ 0 ];

				if( player->isScienceDisabled( st ) )
				{
					//A script has deemed this science disabled.
					m_sciencePurchaseWindowsRank1[ i ]->winEnable( FALSE );
				}
				else if( player->isScienceHidden( st ) )
				{
					//A script has deemed this science unavailable, thus hidden
					m_sciencePurchaseWindowsRank1[ i ]->winHide( TRUE );
				}
				else
				{
					//Handle normal game logic cases!
					if(!player->hasScience(st) && TheScienceStore->playerHasPrereqsForScience(player, st) && TheScienceStore->getSciencePurchaseCost(st) <= player->getSciencePurchasePoints())
					{
						m_sciencePurchaseWindowsRank1[ i ]->winEnable( TRUE );
					}
					if(player->hasScience(st))
					{
						m_sciencePurchaseWindowsRank1[ i ]->winSetStatus(WIN_STATUS_ALWAYS_COLOR);
					}
					else
					{
						m_sciencePurchaseWindowsRank1[ i ]->winClearStatus(WIN_STATUS_ALWAYS_COLOR);
					}
					if(!TheScienceStore->playerHasRootPrereqsForScience(player, st))
						m_sciencePurchaseWindowsRank1[ i ]->winHide(TRUE);
				}
			}
		}

	}

	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_3; i++ )
	{
		if (m_sciencePurchaseWindowsRank3[i] == nullptr)
			continue;

		// get command button
		commandButton = commandSet3->getCommandButton(i);

		// if button is not present, just hide the window
		if( commandButton == nullptr || BitIsSet( commandButton->getOptions(), SCRIPT_ONLY ) )
		{
			// hide window on interface
			m_sciencePurchaseWindowsRank3[ i ]->winHide( TRUE );
		}
		else
		{
			// make sure the window is not hidden
			m_sciencePurchaseWindowsRank3[ i ]->winHide( FALSE );

			// Disable by default
			m_sciencePurchaseWindowsRank3[ i ]->winEnable( FALSE );

			// populate the visible button with data from the command button

			setControlCommand( m_sciencePurchaseWindowsRank3[ i ], commandButton );
			ScienceType	st = SCIENCE_INVALID;
			ScienceVec sv = commandButton->getScienceVec();
			if (! sv.empty())
			{
				st = sv[ 0 ];
			}

			if( player->isScienceDisabled( st ) )
			{
				//A script has deemed this science disabled.
				m_sciencePurchaseWindowsRank3[ i ]->winEnable( FALSE );
			}
			else if( player->isScienceHidden( st ) )
			{
				//A script has deemed this science unavailable, thus hidden
				m_sciencePurchaseWindowsRank3[ i ]->winHide( TRUE );
			}
			else
			{
				//Handle normal game logic cases!
				if(!player->hasScience(st) && TheScienceStore->playerHasPrereqsForScience(player, st) && TheScienceStore->getSciencePurchaseCost(st) <= player->getSciencePurchasePoints())
				{
					m_sciencePurchaseWindowsRank3[ i ]->winEnable( TRUE );
				}
				if(player->hasScience(st))
				{
					m_sciencePurchaseWindowsRank3[ i ]->winSetStatus(WIN_STATUS_ALWAYS_COLOR);
				}
				else
				{
					m_sciencePurchaseWindowsRank3[ i ]->winClearStatus(WIN_STATUS_ALWAYS_COLOR);
				}
				if(!TheScienceStore->playerHasRootPrereqsForScience(player, st))
					m_sciencePurchaseWindowsRank3[ i ]->winHide(TRUE);
			}

		}

	}

	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_8; i++ )
	{
		if (m_sciencePurchaseWindowsRank8[i] == nullptr)
			continue;

		// get command button
		commandButton = commandSet8->getCommandButton(i);

		// if button is not present, just hide the window
		if( commandButton == nullptr || BitIsSet( commandButton->getOptions(), SCRIPT_ONLY ) )
		{
			// hide window on interface
			m_sciencePurchaseWindowsRank8[ i ]->winHide( TRUE );
		}
		else
		{
			// make sure the window is not hidden
			m_sciencePurchaseWindowsRank8[ i ]->winHide( FALSE );

			// Disable by default
			m_sciencePurchaseWindowsRank8[ i ]->winEnable( FALSE );

			// populate the visible button with data from the command button

			setControlCommand( m_sciencePurchaseWindowsRank8[ i ], commandButton );
			ScienceType	st = SCIENCE_INVALID;
			st = commandButton->getScienceVec()[ 0 ];
			if( player->isScienceDisabled( st ) )
			{
				//A script has deemed this science disabled.
				m_sciencePurchaseWindowsRank8[ i ]->winEnable( FALSE );
			}
			else if( player->isScienceHidden( st ) )
			{
				//A script has deemed this science unavailable, thus hidden
				m_sciencePurchaseWindowsRank8[ i ]->winHide( TRUE );
			}
			else
			{
				//Handle normal game logic cases!
				if(!player->hasScience(st) && TheScienceStore->playerHasPrereqsForScience(player, st) && TheScienceStore->getSciencePurchaseCost(st) <= player->getSciencePurchasePoints())
				{
					m_sciencePurchaseWindowsRank8[ i ]->winEnable( TRUE );
				}
				if(player->hasScience(st))
				{
					m_sciencePurchaseWindowsRank8[ i ]->winSetStatus(WIN_STATUS_ALWAYS_COLOR);
				}
				else
				{
					m_sciencePurchaseWindowsRank8[ i ]->winClearStatus(WIN_STATUS_ALWAYS_COLOR);
				}
				if(!TheScienceStore->playerHasRootPrereqsForScience(player, st))
					m_sciencePurchaseWindowsRank8[ i ]->winHide(TRUE);
			}

		}

	}


	GameWindow *win = nullptr;
	UnicodeString tempUS;
	win = TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], TheNameKeyGenerator->nameToKey( "GeneralsExpPoints.wnd:StaticTextRankPointsAvailable" ) );
	if(win)
	{
		tempUS.format(L"%d", player->getSciencePurchasePoints());
		GadgetStaticTextSetText(win, tempUS);
	}

#if RTS_GENERALS
	win = TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], TheNameKeyGenerator->nameToKey( "GeneralsExpPoints.wnd:StaticTextLevel" ) );
	if(win)
	{
		tempUS.format(TheGameText->fetch("SCIENCE:Rank"), player->getRankLevel());
		GadgetStaticTextSetText(win, tempUS);
	}
#else
	// redundant to StaticTextTitle in the Zero Hour context
#endif

	win = TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], TheNameKeyGenerator->nameToKey( "GeneralsExpPoints.wnd:ProgressBarExperience" ) );
	if(win)
	{
		Int progress;
		progress = ((player->getSkillPoints() - player->getSkillPointsLevelDown()) * 100) /(player->getSkillPointsLevelUp() - player->getSkillPointsLevelDown());
		GadgetProgressBarSetProgress(win, progress);
	}

	win = TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], TheNameKeyGenerator->nameToKey( "GeneralsExpPoints.wnd:StaticTextTitle" ) );
	if(win)
	{
		AsciiString tempAs;

		tempAs.format("SCIENCE:Rank%d", player->getRankLevel());
		GadgetStaticTextSetText(win, TheGameText->fetch(tempAs));
	}


	//
	// to avoid a one frame delay where windows may become enabled/disabled, run the update
	// at once to get it all in the correct state immediately
	//
	updateContextPurchaseScience();
/*
	// get the side select buttons
	GameWindow* win = m_contextParent[ CP_PURCHASE_SCIENCE ];

	Color color = GameMakeColor(255, 255, 255, 255);

	/// @todo srj -- evil hack testing code. do not imitate.
	ScienceVec purchasable, potential;
	TheScienceStore->getPurchasableSciences(player, purchasable, potential);
	GadgetListBoxReset(win);
	for (ScienceVec::const_iterator it = purchasable.begin(); it != purchasable.end(); ++it)
	{
		ScienceType st = *it;
		UnicodeString u;
		u.translate(TheScienceStore->getInternalNameForScience(st));
		GadgetListBoxAddEntryText(win, u, color, -1, -1);
	}
	for (ScienceVec::const_iterator it2 = potential.begin(); it2 != potential.end(); ++it2)
	{
		ScienceType st = *it2;
		AsciiString foo = "(Not Yet)";
		foo.concat(TheScienceStore->getInternalNameForScience(st));
		UnicodeString u;
		u.translate(foo);
		GadgetListBoxAddEntryText(win, u, color, -1, -1);
	}
	GadgetListBoxAddEntryText(win, L"Cancel", color, -1, -1);*/

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::updateContextPurchaseScience()
{
	GameWindow *win =nullptr;
	Player *player = ThePlayerList->getLocalPlayer();
	win = TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], TheNameKeyGenerator->nameToKey( "GeneralsExpPoints.wnd:ProgressBarExperience" ) );
	if(win)
	{
		Int progress;
		progress = ((player->getSkillPoints() - player->getSkillPointsLevelDown()) * 100) /(player->getSkillPointsLevelUp() - player->getSkillPointsLevelDown());
		GadgetProgressBarSetProgress(win, progress);
	}

//	win = TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], TheNameKeyGenerator->nameToKey( "ControlBar.wnd:TextEntryGeneralName" ) );
//	if(win)
//	{
//		UnicodeString temp = GadgetTextEntryGetText(win);
//		if(temp.compare(player->getGeneralName()) != 0)
//			player->setGeneralName(temp);
//	}
/*
	/// @todo srj -- evil hack testing code. do not imitate.
	Object *obj = m_currentSelectedDrawable->getObject();

	if( obj == nullptr )
		return;

	// sanity
	if( obj->isKindOf( KINDOF_COMMANDCENTER ) == FALSE )
		switchToContext( CB_CONTEXT_NONE, nullptr );

	GameWindow* win = m_contextParent[ CP_PURCHASE_SCIENCE ];

	Int selected;
	GadgetListBoxGetSelected( win, &selected );
	if( selected != -1 )
	{
		UnicodeString usci = GadgetListBoxGetText( win, selected, 0 );
		AsciiString sci;
		sci.translate(usci);
		ScienceType st = usci.getCharAt(0) == '(' ? SCIENCE_INVALID : TheScienceStore->getScienceFromInternalName(sci);

		if (st != SCIENCE_INVALID)
		{
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_PURCHASE_SCIENCE );
			msg->appendIntegerArgument( st );
		}

		switchToContext( CB_CONTEXT_NONE, nullptr );
	}
*/

}

//-------------------------------------------------------------------------------------------------
/** parse command definition */
//-------------------------------------------------------------------------------------------------
void CommandButton::parseCommand( INI* ini, void *instance, void *store, const void *userData )
{
	const char *token = ini->getNextToken();
	Int i;

	for( i = 0; TheGuiCommandNames[ i ]; i++ )
	{

		if( stricmp( TheGuiCommandNames[ i ], token ) == 0 )
		{

			GUICommandType *command = (GUICommandType *)store;
			*command = (GUICommandType)i;
			return;

		}

	}

	// if we're here the command was not found
	throw INI_INVALID_DATA;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CommandButton::CommandButton()
{

	m_command = GUI_COMMAND_NONE;
	m_thingTemplate = nullptr;
	m_upgradeTemplate = nullptr;
	m_weaponSlot = PRIMARY_WEAPON;
	m_maxShotsToFire = 0x7fffffff;	// huge number
	m_science.clear();
	m_specialPower = nullptr;
	m_buttonImage = nullptr;

	//Code renderer handles these states now.
	//m_disabledImage = nullptr;
	//m_hiliteImage = nullptr;
	//m_pushedImage = nullptr;

	m_flashCount = 0;
	m_conflictingLabel.clear();
	m_cursorName.clear();
	m_descriptionLabel.clear();
	m_invalidCursorName.clear();
	m_name.clear();
	m_options = 0;
	m_purchasedLabel.clear();
	m_textLabel.clear();
	m_window = nullptr;
	m_commandButtonBorder = COMMAND_BUTTON_BORDER_NONE;
	//m_prev = nullptr;
	m_next = nullptr;
	m_radiusCursor = RADIUSCURSOR_NONE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CommandButton::~CommandButton()
{

}

//-------------------------------------------------------------------------------------------------
Bool CommandButton::isValidRelationshipTarget(Relationship r) const
{
	UnsignedInt mask = 0;
	if (r == ENEMIES) mask |= NEED_TARGET_ENEMY_OBJECT;
	else if (r == ALLIES) mask |= NEED_TARGET_ALLY_OBJECT;
	else if (r == NEUTRAL) mask |= NEED_TARGET_NEUTRAL_OBJECT;

	return (m_options & mask) != 0;
}

//-------------------------------------------------------------------------------------------------
Bool CommandButton::isValidObjectTarget(const Player* sourcePlayer, const Object* targetObj) const
{
	if (!sourcePlayer || !targetObj)
		return false;

	Relationship r = sourcePlayer->getRelationship(targetObj->getTeam());

	return isValidRelationshipTarget(r);
}

//-------------------------------------------------------------------------------------------------
Bool CommandButton::isValidObjectTarget(const Object* sourceObj, const Object* targetObj) const
{
	if (!sourceObj || !targetObj)
		return false;

	Relationship r = sourceObj->getRelationship(targetObj);

	return isValidRelationshipTarget(r);
}

//-------------------------------------------------------------------------------------------------
Bool CommandButton::isValidToUseOn(const Object *sourceObj, const Object *targetObj, const Coord3D *targetLocation, CommandSourceType commandSource) const
{
	if (m_upgradeTemplate) {
		// @todo: Make a const version of pui. We're not altering the production queue, so this const-cast
		// is okay.
		ProductionUpdateInterface *pui = const_cast<Object*>(sourceObj)->getProductionUpdateInterface();
		if (pui) {
			const ProductionEntry *pe = pui->firstProduction();
			while (pe) {
				if (pe->getProductionUpgrade() != nullptr)
					return false;
				pe = pui->nextProduction(pe);
			}
			return sourceObj->affectedByUpgrade(m_upgradeTemplate) && !sourceObj->hasUpgrade(m_upgradeTemplate);
		}
		// No ProductionUpdateInterface means we can't do this.
		return false;
	}

	if( BitIsSet( m_options, COMMAND_OPTION_NEED_OBJECT_TARGET ) && !targetObj )
	{
		return false;
	}

	Coord3D pos;
	if( targetLocation )
	{
		pos.set( *targetLocation );
	}

	if( BitIsSet( m_options, NEED_TARGET_POS ) && !targetLocation )
	{
		if( targetObj )
		{
			pos.set( *targetObj->getPosition() );
		}
		else
		{
			return false;
		}
	}

	if( BitIsSet( m_options, COMMAND_OPTION_NEED_OBJECT_TARGET ) )
	{
		return TheActionManager->canDoSpecialPowerAtObject( sourceObj, targetObj, commandSource, m_specialPower, m_options, false );
	}

	if( BitIsSet( m_options, NEED_TARGET_POS ) )
	{
		return TheActionManager->canDoSpecialPowerAtLocation( sourceObj, &pos, commandSource, m_specialPower, nullptr, m_options, false );
	}

	return TheActionManager->canDoSpecialPower( sourceObj, m_specialPower, commandSource, m_options, false );
}

//-------------------------------------------------------------------------------------------------
Bool CommandButton::isReady(const Object *sourceObj) const
{
	SpecialPowerModuleInterface *mod = sourceObj->getSpecialPowerModule( m_specialPower );
	if( mod && mod->getPercentReady() == 1.0f )
		return true;

	if (m_upgradeTemplate && sourceObj->affectedByUpgrade(m_upgradeTemplate) && !sourceObj->hasUpgrade(m_upgradeTemplate))
		return true;

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool CommandButton::isValidObjectTarget(const Drawable* source, const Drawable* target) const
{
	return isValidObjectTarget(source ? source->getObject() : nullptr, target ? target->getObject() : nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CommandSet /////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** These are the fields you can define in a command set, they correspond to physical
	* buttons in the GUI */
//-------------------------------------------------------------------------------------------------
const FieldParse CommandSet::m_commandSetFieldParseTable[] =
{

	{ "1",			CommandSet::parseCommandButton, (void *)nullptr,		offsetof( CommandSet, m_command ) },
	{ "2",			CommandSet::parseCommandButton, (void *)1,		offsetof( CommandSet, m_command ) },
	{ "3",			CommandSet::parseCommandButton, (void *)2,		offsetof( CommandSet, m_command ) },
	{ "4",			CommandSet::parseCommandButton, (void *)3,		offsetof( CommandSet, m_command ) },
	{ "5",			CommandSet::parseCommandButton, (void *)4,		offsetof( CommandSet, m_command ) },
	{ "6",			CommandSet::parseCommandButton, (void *)5,		offsetof( CommandSet, m_command ) },
	{ "7",			CommandSet::parseCommandButton, (void *)6,		offsetof( CommandSet, m_command ) },
	{ "8",			CommandSet::parseCommandButton, (void *)7,		offsetof( CommandSet, m_command ) },
	{ "9",			CommandSet::parseCommandButton, (void *)8,		offsetof( CommandSet, m_command ) },
	{ "10",			CommandSet::parseCommandButton, (void *)9,		offsetof( CommandSet, m_command ) },
	{ "11",			CommandSet::parseCommandButton, (void *)10,		offsetof( CommandSet, m_command ) },
	{ "12",			CommandSet::parseCommandButton, (void *)11,		offsetof( CommandSet, m_command ) },
	{ "13",			CommandSet::parseCommandButton, (void *)12,		offsetof( CommandSet, m_command ) },
	{ "14",			CommandSet::parseCommandButton, (void *)13,		offsetof( CommandSet, m_command ) },
	{ "15",			CommandSet::parseCommandButton, (void *)14,		offsetof( CommandSet, m_command ) },
	{ "16",			CommandSet::parseCommandButton, (void *)15,		offsetof( CommandSet, m_command ) },
	{ "17",			CommandSet::parseCommandButton, (void *)16,		offsetof( CommandSet, m_command ) },
	{ "18",			CommandSet::parseCommandButton, (void *)17,		offsetof( CommandSet, m_command ) },
	{ nullptr,			nullptr,														 nullptr,				0	}

};

//-------------------------------------------------------------------------------------------------
Bool CommandButton::isContextCommand() const
{
	return BitIsSet( m_options, CONTEXTMODE_COMMAND );
}

//-------------------------------------------------------------------------------------------------
// bleah. shouldn't be const, but is. sue me. (srj)
void CommandButton::copyImagesFrom( const CommandButton *button, Bool markUIDirtyIfChanged ) const
{
	if( m_buttonImage != button->getButtonImage() )
	{
		m_buttonImage = button->getButtonImage();

		//Code renderer handles these states now.
		//m_disabledImage = button->getDisabledImage();
		//m_hiliteImage = button->getHiliteImage();
		//m_pushedImage = button->getPushedImage();

		if( markUIDirtyIfChanged )
		{
			TheControlBar->markUIDirty();
		}
	}
}

//-------------------------------------------------------------------------------------------------
// bleah. shouldn't be const, but is. sue me. (Kris) -snork!
void CommandButton::copyButtonTextFrom( const CommandButton *button, Bool shortcutButton, Bool markUIDirtyIfChanged ) const
{
	//This function was added to change the strings when you upgrade from a DaisyCutter to a MOAB. All other special
	//powers are the same.
	Bool change = FALSE;
	if( shortcutButton )
	{
		//Not the best code, but conflicting label means shortcut label (most won't have any string specified).
		if( button->getConflictingLabel().isNotEmpty() && m_textLabel.compare( button->getConflictingLabel() ) )
		{
			m_textLabel = button->getConflictingLabel();
			change = TRUE;
		}
	}
	else
	{
		//Copy the text from the purchase science button if it exists (most won't).
		if( button->getTextLabel().isNotEmpty() && m_textLabel.compare( button->getTextLabel() ) )
		{
			m_textLabel = button->getTextLabel();
			change = TRUE;
		}
	}
	if( button->getDescriptionLabel().isNotEmpty() && m_descriptionLabel.compare( button->getDescriptionLabel() ) )
	{
		m_descriptionLabel = button->getDescriptionLabel();
		change = TRUE;
	}
	if( markUIDirtyIfChanged && change )
	{
		TheControlBar->markUIDirty();
	}
}

//-------------------------------------------------------------------------------------------------
/** Parse a single command button definition */
//-------------------------------------------------------------------------------------------------
void CommandSet::parseCommandButton( INI* ini, void *instance, void *store, const void *userData )
{
	const char *token = ini->getNextToken();

	// get find the command button from this name
	const CommandButton *commandButton = TheControlBar->findCommandButton( AsciiString( token ) );
	if( commandButton == nullptr )
	{

		DEBUG_CRASH(( "[LINE: %d - FILE: '%s'] Unknown command '%s' found in command set",
								  ini->getLineNum(), ini->getFilename().str(), token ));
		throw INI_INVALID_DATA;

	}

	// get the index to store the command at, and the command array itself
	const CommandButton **buttonArray = (const CommandButton **)store;
	// GeneralsX @bugfix BenderAI 12/02/2026 - Cast via intptr_t for 64-bit compatibility
	// userData is actually an integer index stored as a pointer (common pattern in callbacks).
	// On 64-bit Linux, void* is 8 bytes but Int is 4 bytes. Cast through intptr_t first.
	Int buttonIndex = static_cast<Int>(reinterpret_cast<intptr_t>(userData));

	// sanity
	DEBUG_ASSERTCRASH( buttonIndex < MAX_COMMANDS_PER_SET, ("parseCommandButton: button index '%d' out of range",
										 buttonIndex) );

	// save it
	buttonArray[ buttonIndex ] = commandButton;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CommandSet::CommandSet(const AsciiString& name) :
	m_name(name),
	m_next(nullptr)
{
	for( Int i = 0; i < MAX_COMMANDS_PER_SET; i++ )
		m_command[ i ] = nullptr;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const CommandButton* CommandSet::getCommandButton(Int i) const
{
	const CommandButton* button;
  // Check for TheGameLogic == null, cause it is in Worldbuilder, and wb gets command bar info. jba.
	if (TheGameLogic && TheGameLogic->findControlBarOverride(m_name, i, button))
		return button;

	return m_command[i];
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void CommandSet::friend_addToList(CommandSet** listHead)
{
	m_next = *listHead;
	*listHead = this;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CommandSet::~CommandSet()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// ControlBar /////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ControlBar::ControlBar()
{
	Int i;
	m_commandButtons = nullptr;
	m_commandSets = nullptr;
	m_controlBarSchemeManager = nullptr;
	m_isObserverCommandBar = FALSE;
	m_observerLookAtPlayer = nullptr;
	m_observedPlayer = nullptr;
	m_buildToolTipLayout = nullptr;
	m_showBuildToolTipLayout = FALSE;

	m_animateDownWin1Pos.x = m_animateDownWin1Pos.y = 0;
	m_animateDownWin1Size.x = m_animateDownWin1Size.y = 0;
	m_animateDownWin2Pos.x = m_animateDownWin2Pos.y = 0;
	m_animateDownWin2Size.x = m_animateDownWin2Size.y = 0;

	m_animateDownWindow = nullptr;
	m_animTime = 0;

	for( i = 0; i < MAX_COMMANDS_PER_SET; i++)
	{
		m_commonCommands[i] = nullptr;
	}

	m_currContext = CB_CONTEXT_NONE;
	m_defaultControlBarPosition.x = m_defaultControlBarPosition.y = 0;
	m_genStarFlash = FALSE;
	m_genStarFlashTimeAccumulator = 0.0f;
	m_genStarOff = nullptr;
	m_genStarOn  = nullptr;
	m_UIDirty    = FALSE;
	//	m_controlBarResizer = nullptr;
	m_buildUpClockColor = GameMakeColor(0,0,0,100);
	m_commandBarBorderColor = GameMakeColor(0,0,0,100);
	for( i = 0; i < NUM_CONTEXT_PARENTS; i++ )
		m_contextParent[ i ] = nullptr;
	for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
	{
		m_commandWindows[ i ] = nullptr;
	// removed from multiplayer branch
		//m_commandMarkers[ i ] = nullptr;
	}

	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_1; i++ )
		m_sciencePurchaseWindowsRank1[i] = nullptr;
	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_3; i++ )
		m_sciencePurchaseWindowsRank3[i] = nullptr;
	for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_8; i++ )
		m_sciencePurchaseWindowsRank8[i] = nullptr;

	for( i = 0; i < MAX_SPECIAL_POWER_SHORTCUTS; i++ )
	{
		m_specialPowerShortcutButtons[i] = nullptr;
		m_specialPowerShortcutButtonParents[i] = nullptr;
	}

	m_specialPowerShortcutParent = nullptr;
	m_specialPowerLayout = nullptr;
	m_scienceLayout = nullptr;
	m_rightHUDWindow = nullptr;
	m_rightHUDCameoWindow = nullptr;
	for( i = 0; i < MAX_RIGHT_HUD_UPGRADE_CAMEOS; i++ )
		m_rightHUDUpgradeCameos[i];
	m_rightHUDUnitSelectParent = nullptr;
	m_communicatorButton = nullptr;
	m_currentSelectedDrawable = nullptr;
	m_currContext = CB_CONTEXT_NONE;
	m_rallyPointDrawableID = INVALID_DRAWABLE_ID;
	m_displayedConstructPercent = -1.0f;
	m_displayedOCLTimerSeconds = 0;
	m_displayedQueueCount = 0;
	resetBuildQueueData();
	resetContainData();
	m_lastRecordedInventoryCount = 0;

	m_videoManager = nullptr;
	m_animateWindowManager = nullptr;
	m_generalsScreenAnimate = nullptr;
	m_animateWindowManagerForGenShortcuts = nullptr;
	m_flash = FALSE;
	m_toggleButtonUpIn = nullptr;
	m_toggleButtonUpOn = nullptr;
	m_toggleButtonUpPushed = nullptr;
	m_toggleButtonDownIn = nullptr;
	m_toggleButtonDownOn = nullptr;
	m_toggleButtonDownPushed = nullptr;

	m_generalButtonEnable = nullptr;
	m_generalButtonHighlight = nullptr;
	m_genArrow = nullptr;
	m_sideSelectAnimateDown = FALSE;
	updateCommandBarBorderColors(GAME_COLOR_UNDEFINED,GAME_COLOR_UNDEFINED,GAME_COLOR_UNDEFINED,GAME_COLOR_UNDEFINED);

	m_radarAttackGlowOn = FALSE;
	m_remainingRadarAttackGlowFrames = 0;
	m_radarAttackGlowWindow = nullptr;

#if defined(RTS_DEBUG)
	m_lastFrameMarkedDirty = 0;
	m_consecutiveDirtyFrames = 0;
#endif

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ControlBar::~ControlBar()
{

	if(m_scienceLayout)
	{
		m_scienceLayout->destroyWindows();
		deleteInstance(m_scienceLayout);
		m_scienceLayout = nullptr;
	}
	m_genArrow = nullptr;

	delete m_videoManager;
	m_videoManager = nullptr;

	delete m_animateWindowManagerForGenShortcuts;
	m_animateWindowManagerForGenShortcuts = nullptr;

	delete m_animateWindowManager;
	m_animateWindowManager = nullptr;

	delete m_generalsScreenAnimate;
	m_generalsScreenAnimate = nullptr;

	delete m_controlBarSchemeManager;
	m_controlBarSchemeManager = nullptr;

//	delete m_controlBarResizer;
//	m_controlBarResizer = nullptr;

	// destroy all the command set definitions
	CommandSet *set;
	while( m_commandSets )
	{
		set = m_commandSets->friend_getNext();
		deleteInstance(m_commandSets);
		m_commandSets = set;

	}

	// destroy all our command button definitions
	CommandButton *button;
	while( m_commandButtons )
	{
		button = m_commandButtons->friend_getNext();
		deleteInstance(m_commandButtons);
		m_commandButtons = button;

	}
	if(m_buildToolTipLayout)
	{
		m_buildToolTipLayout->destroyWindows();
		deleteInstance(m_buildToolTipLayout);
		m_buildToolTipLayout = nullptr;
	}

	if(m_specialPowerLayout)
	{
		m_specialPowerLayout->destroyWindows();
		deleteInstance(m_specialPowerLayout);
		m_specialPowerLayout = nullptr;
	}

	m_radarAttackGlowWindow = nullptr;

	if (m_rightHUDCameoWindow && m_rightHUDCameoWindow->winGetUserData())
	{
		delete m_rightHUDCameoWindow->winGetUserData();
		m_rightHUDCameoWindow->winSetUserData(nullptr);
	}

}
void ControlBarPopupDescriptionUpdateFunc( WindowLayout *layout, void *param );

//-------------------------------------------------------------------------------------------------
/** Initialize the control bar, this is our interface to the context sensitive GUI */
//-------------------------------------------------------------------------------------------------
void ControlBar::init()
{
	INI ini;
	m_sideSelectAnimateDown = FALSE;
	// load the command buttons
	ini.loadFileDirectory( "Data\\INI\\Default\\CommandButton", INI_LOAD_OVERWRITE, nullptr );
	ini.loadFileDirectory( "Data\\INI\\CommandButton", INI_LOAD_OVERWRITE, nullptr );

	// load the command sets
	ini.loadFileDirectory( "Data\\INI\\CommandSet", INI_LOAD_OVERWRITE, nullptr );

	// post process step after loading the command buttons and command sets
	postProcessCommands();

	// Init the scheme manager, this will call its own INI init function.
	m_controlBarSchemeManager = NEW ControlBarSchemeManager;
	m_controlBarSchemeManager->init();

	//Added this check because the builder uses the ControlBar, but doesn't care about
	//the GUI.
	if( TheWindowManager )
	{
		//
		// the control bar has several windows that make up our context sensitive interface, we
		// want those parent windows so that we can easily hide and show them to make the
		// interface context sensitive
		//
		NameKeyType id;
		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:ControlBarParent" );
		m_contextParent[ CP_MASTER ] = TheWindowManager->winGetWindowFromId( nullptr, id );
	m_contextParent[ CP_MASTER ]->winGetPosition(&m_defaultControlBarPosition.x, &m_defaultControlBarPosition.y);

		m_scienceLayout = TheWindowManager->winCreateLayout("GeneralsExpPoints.wnd");
		m_scienceLayout->hide(TRUE);
		id = TheNameKeyGenerator->nameToKey( "GeneralsExpPoints.wnd:GenExpParent" );

		m_contextParent[ CP_PURCHASE_SCIENCE ] = TheWindowManager->winGetWindowFromId( nullptr, id );//m_scienceLayout->getFirstWindow();

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:UnderConstructionWindow" );
		m_contextParent[ CP_UNDER_CONSTRUCTION ] = TheWindowManager->winGetWindowFromId( nullptr, id );

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:OCLTimerWindow" );
		m_contextParent[ CP_OCL_TIMER ] = TheWindowManager->winGetWindowFromId( nullptr, id );

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:BeaconWindow" );
		m_contextParent[ CP_BEACON ] = TheWindowManager->winGetWindowFromId( nullptr, id );

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:CommandWindow" );
		m_contextParent[ CP_COMMAND ] = TheWindowManager->winGetWindowFromId( nullptr, id );

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:ProductionQueueWindow" );
		m_contextParent[ CP_BUILD_QUEUE ] = TheWindowManager->winGetWindowFromId( nullptr, id );

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:ObserverPlayerListWindow" );
		m_contextParent[ CP_OBSERVER_LIST ] = TheWindowManager->winGetWindowFromId( nullptr, id );

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:ObserverPlayerInfoWindow" );
		m_contextParent[ CP_OBSERVER_INFO ] = TheWindowManager->winGetWindowFromId( nullptr, id );


		// get the command windows and save for easy access later
		Int i;
		ICoord2D commandSize, commandPos;
		AsciiString windowName;
		for( i = 0; i < MAX_COMMANDS_PER_SET; i++ )
		{

			windowName.format( "ControlBar.wnd:ButtonCommand%02d", i + 1 );
			id = TheNameKeyGenerator->nameToKey( windowName.str() );
			m_commandWindows[ i ] =
				TheWindowManager->winGetWindowFromId( m_contextParent[ CP_COMMAND ], id );
			if (m_commandWindows[ i ])
			{
				m_commandWindows[ i ]->winGetPosition(&commandPos.x, &commandPos.y);
				m_commandWindows[ i ]->winGetSize(&commandSize.x, &commandSize.y);
				m_commandWindows[ i ]->winSetStatus( WIN_STATUS_USE_OVERLAY_STATES );
			}

	// removed from multiplayer branch
//			windowName.format( "ControlBar.wnd:CommandMarker%02d", i + 1 );
//			id = TheNameKeyGenerator->nameToKey( windowName.str() );
//			m_commandMarkers[ i ] =
//				TheWindowManager->winGetWindowFromId( m_contextParent[ CP_COMMAND ], id );
//			// set the size and position to make sure their in the same place as the buttons.
//			m_commandMarkers[i]->winSetPosition(commandPos.x -2, commandPos.y - 2);
//			m_commandMarkers[i]->winSetSize(commandSize.x + 2, commandSize.y + 2);



		}


		for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_1; i++ )
		{
			windowName.format( "GeneralsExpPoints.wnd:ButtonRank1Number%d", i );
			id = TheNameKeyGenerator->nameToKey( windowName.str() );
			m_sciencePurchaseWindowsRank1[ i ] =
				TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], id );
			if (m_sciencePurchaseWindowsRank1[ i ] != nullptr)
				m_sciencePurchaseWindowsRank1[ i ]->winSetStatus( WIN_STATUS_USE_OVERLAY_STATES );
		}
		for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_3; i++ )
		{
			windowName.format( "GeneralsExpPoints.wnd:ButtonRank3Number%d", i );
			id = TheNameKeyGenerator->nameToKey( windowName.str() );
			m_sciencePurchaseWindowsRank3[ i ] =
				TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], id );
			if (m_sciencePurchaseWindowsRank3[ i ] != nullptr)
				m_sciencePurchaseWindowsRank3[ i ]->winSetStatus( WIN_STATUS_USE_OVERLAY_STATES );
		}

		for( i = 0; i < MAX_PURCHASE_SCIENCE_RANK_8; i++ )
		{
			windowName.format( "GeneralsExpPoints.wnd:ButtonRank8Number%d", i );
			id = TheNameKeyGenerator->nameToKey( windowName.str() );
			m_sciencePurchaseWindowsRank8[ i ] =
				TheWindowManager->winGetWindowFromId( m_contextParent[ CP_PURCHASE_SCIENCE ], id );
			if (m_sciencePurchaseWindowsRank8[ i ] != nullptr)
				m_sciencePurchaseWindowsRank8[ i ]->winSetStatus( WIN_STATUS_USE_OVERLAY_STATES );
		}

		// keep a pointer to the window making up the right HUD display
		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:RightHUD" );
		m_rightHUDWindow = TheWindowManager->winGetWindowFromId( nullptr, id );

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:WinUnitSelected" );
		m_rightHUDUnitSelectParent = TheWindowManager->winGetWindowFromId( nullptr, id );

		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:CameoWindow" );
		m_rightHUDCameoWindow = TheWindowManager->winGetWindowFromId( nullptr, id );
		for( i = 0; i < MAX_RIGHT_HUD_UPGRADE_CAMEOS; i++ )
		{
			windowName.format( "ControlBar.wnd:UnitUpgrade%d", i+1 );
			id = TheNameKeyGenerator->nameToKey( windowName.str() );
			m_rightHUDUpgradeCameos[ i ] =
				TheWindowManager->winGetWindowFromId( m_rightHUDWindow, id );
			m_rightHUDUpgradeCameos[ i ]->winSetStatus( WIN_STATUS_USE_OVERLAY_STATES );
		}

//		m_transitionHandler = NEW GameWindowTransitionsHandler;
//		m_transitionHandler->load();
//		m_transitionHandler->init();

		// don't forget about the communicator button CCB
		id = TheNameKeyGenerator->nameToKey( "ControlBar.wnd:PopupCommunicator" );
		m_communicatorButton = TheWindowManager->winGetWindowFromId( nullptr, id );
		setControlCommand(m_communicatorButton, findCommandButton("NonCommand_Communicator") );
		m_communicatorButton->winSetTooltipFunc(commandButtonTooltip);

		GameWindow *win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey("ControlBar.wnd:ButtonOptions"));
		if(win)
		{
			setControlCommand(win, findCommandButton("NonCommand_Options") );
			win->winSetTooltipFunc(commandButtonTooltip);
		}
		win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey("ControlBar.wnd:ButtonIdleWorker"));
		if(win)
		{
			setControlCommand(win, findCommandButton("NonCommand_IdleWorker") );
			win->winSetTooltipFunc(commandButtonTooltip);
		}
		win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey("ControlBar.wnd:ButtonPlaceBeacon"));
		if(win)
		{
			setControlCommand(win, findCommandButton("NonCommand_Beacon") );
			win->winSetTooltipFunc(commandButtonTooltip);
		}
		win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey("ControlBar.wnd:ButtonGeneral"));
		if(win)
		{
			setControlCommand(win, findCommandButton("NonCommand_GeneralsExperience") );
			win->winSetTooltipFunc(commandButtonTooltip);
		}
		win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey("ControlBar.wnd:ButtonLarge"));
		if(win)
		{
			setControlCommand(win, findCommandButton("NonCommand_UpDown") );
			win->winSetTooltipFunc(commandButtonTooltip);
		}

		win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey("ControlBar.wnd:PowerWindow"));
		if(win)
		{
			win->winSetTooltipFunc(commandButtonTooltip);
		}
		win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey("ControlBar.wnd:MoneyDisplay"));
		if(win)
		{
			win->winSetTooltipFunc(commandButtonTooltip);
		}
		win = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("ControlBar.wnd:GeneralsExp"));
		if(win)
		{
			win->winSetTooltipFunc(commandButtonTooltip);
		}

		m_radarAttackGlowWindow = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("ControlBar.wnd:WinUAttack"));


		win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey( "ControlBar.wnd:BackgroundMarker" ));
		win->winGetScreenPosition(&m_controlBarForegroundMarkerPos.x, &m_controlBarForegroundMarkerPos.y);
		win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey( "ControlBar.wnd:BackgroundMarker" ));
		win->winGetScreenPosition(&m_controlBarBackgroundMarkerPos.x,&m_controlBarBackgroundMarkerPos.y);

		if(!m_videoManager)
			m_videoManager = NEW WindowVideoManager;
		if(!m_animateWindowManager)
			m_animateWindowManager = NEW AnimateWindowManager;
		if(!m_generalsScreenAnimate)
			m_generalsScreenAnimate = NEW AnimateWindowManager;
		if(!m_animateWindowManagerForGenShortcuts)
			m_animateWindowManagerForGenShortcuts = NEW AnimateWindowManager;
		m_buildToolTipLayout = TheWindowManager->winCreateLayout( "ControlBarPopupDescription.wnd" );
		if(m_buildToolTipLayout)
		{
			m_buildToolTipLayout->hide(TRUE);
			m_buildToolTipLayout->setUpdate(ControlBarPopupDescriptionUpdateFunc);
		}

		m_genStarOn = TheMappedImageCollection ? (Image *)TheMappedImageCollection->findImageByName("BarButtonGenStarON") : nullptr;
		m_genStarOff = TheMappedImageCollection ? (Image *)TheMappedImageCollection->findImageByName("BarButtonGenStarOFF") : nullptr;
		m_genStarFlash = TRUE;
		m_lastFlashedAtPointValue = -1;

		m_rankVeteranIcon = TheMappedImageCollection ? TheMappedImageCollection->findImageByName( "SSChevron1L" ) : nullptr;
		m_rankEliteIcon		= TheMappedImageCollection ? TheMappedImageCollection->findImageByName( "SSChevron2L" ) : nullptr;
		m_rankHeroicIcon	= TheMappedImageCollection ? TheMappedImageCollection->findImageByName( "SSChevron3L" ) : nullptr;


//		if(!m_controlBarResizer)
//			m_controlBarResizer = NEW ControlBarResizer;
//		m_controlBarResizer->init();



		// Initialize the Observer controls
		initObserverControls();

		// by default switch to the none context
		switchToContext( CB_CONTEXT_NONE, nullptr );
	}

}

//-------------------------------------------------------------------------------------------------
/** Reset the context sensitive control bar GUI */
//-------------------------------------------------------------------------------------------------
void ControlBar::reset()
{
	hideSpecialPowerShortcut();
	// do not destroy the rally drawable, it will get destroyed with everything else during a reset
	m_rallyPointDrawableID = INVALID_DRAWABLE_ID;
	if(m_radarAttackGlowWindow)
		m_radarAttackGlowWindow->winEnable(TRUE);
	m_radarAttackGlowOn = FALSE;
	m_remainingRadarAttackGlowFrames = 0;

	m_displayedConstructPercent = -1.0f;
	m_displayedOCLTimerSeconds = 0;

	m_isObserverCommandBar = FALSE; // reset us to use a normal command bar
	m_observerLookAtPlayer = nullptr;
	m_observedPlayer = nullptr;

	if(m_buildToolTipLayout)
		m_buildToolTipLayout->hide(TRUE);
	m_showBuildToolTipLayout = FALSE;

	if(m_animateWindowManager)
		m_animateWindowManager->reset();

	if(m_animateWindowManagerForGenShortcuts)
		m_animateWindowManagerForGenShortcuts->reset();

	if(m_generalsScreenAnimate)
		m_generalsScreenAnimate->reset();


	if(m_videoManager)
		m_videoManager->reset();

	// go back to default context
	switchToContext( CB_CONTEXT_NONE, nullptr );
	m_sideSelectAnimateDown = FALSE;
	if(m_animateDownWindow)
	{
		TheWindowManager->winDestroy( m_animateDownWindow );
		m_animateDownWindow = nullptr;
	}

	// Remove any overridden sets.
	CommandSet *set, *nextSet;
	set = m_commandSets;
	while (set) {
		Bool possibleAdjustment = FALSE;
		nextSet = set->friend_getNext();
		if (set == m_commandSets) {
			possibleAdjustment = TRUE;
		}

		Overridable *stillValid = set->deleteOverrides();
		if (stillValid == nullptr && possibleAdjustment) {
			m_commandSets = nextSet;
		}

		set = nextSet;
	}

	// Remove any overridden command buttons.
	CommandButton *button, *nextButton;
	button = m_commandButtons;
	while (button) {
		Bool possibleAdjustment = FALSE;
		nextButton = button->friend_getNext();
		if (button == m_commandButtons) {
			possibleAdjustment = TRUE;
		}

		Overridable *stillValid = button->deleteOverrides();
		if (stillValid == nullptr && possibleAdjustment) {
			m_commandButtons = nextButton;
		}

		button = nextButton;
	}
	if(TheTransitionHandler)
		TheTransitionHandler->remove("ControlBarArrow");
	m_genArrow = nullptr;

	m_lastFlashedAtPointValue = -1;
	m_genStarFlash = TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Update phase, we can track if our selected object is destroyed, update button
	* percentages, status, enabled status etc */
//-------------------------------------------------------------------------------------------------
void ControlBar::update()
{
	if (TheGlobalData->m_headless)
		return;
	getStarImage();
	updateRadarAttackGlow();
	if(m_controlBarSchemeManager)
		m_controlBarSchemeManager->update();

	// Update our video manager
	if( m_videoManager )
		m_videoManager->update();

	if (m_animateWindowManager)
		m_animateWindowManager->update();

	if (m_animateWindowManager)
		{
			if (m_animateWindowManager->isFinished() && m_animateWindowManager->isReversed())
			{
				Int id = (Int)TheNameKeyGenerator->nameToKey("ControlBar.wnd:ControlBarParent");
				GameWindow *window = TheWindowManager->winGetWindowFromId(nullptr, id);
				if (window && !window->winIsHidden())
					window->winHide(TRUE);
			}
		}

	if(m_animateWindowManagerForGenShortcuts)
		m_animateWindowManagerForGenShortcuts->update();
	if (m_animateWindowManagerForGenShortcuts && m_specialPowerShortcutParent)
		{
			if (m_animateWindowManagerForGenShortcuts->isFinished() && m_animateWindowManagerForGenShortcuts->isReversed())
			{
				if (m_specialPowerShortcutParent && !m_specialPowerShortcutParent->winIsHidden())
					m_specialPowerShortcutParent->winHide(TRUE);
			}
		}



	if( !m_buildToolTipLayout->isHidden())
	{
		m_buildToolTipLayout->runUpdate();
		m_showBuildToolTipLayout = FALSE;
	}
/*
	else if( m_buildToolTipLayout )
	{
		hideBuildTooltipLayout();
	}*/

	updateSpecialPowerShortcut();
	// if we're an observer, don't do the complete update
	if( m_isObserverCommandBar)
	{
		if((TheGameLogic->getFrame() % (LOGICFRAMES_PER_SECOND/2)) == 0)
			populateObserverInfoWindow();

		Drawable *drawToEvaluateFor = nullptr;
		if( TheInGameUI->getSelectCount() > 1 )
		{
			// Attempt to isolate a Drawable here to evaluate
			// The need arises when selected is an AngryMob,
			// whose selection actually consists of varied units
			// but is represented in the UI as a single unit,
			// so we must isolate and evaluate only the Nexus
			drawToEvaluateFor = TheGameClient->findDrawableByID( TheInGameUI->getSoloNexusSelectedDrawableID() ) ;
		}
		else // get the first and only drawble in the selection list
			// TheSuperHackers @fix Mauller 07/04/2025 The first access to this can return an empty list
			if (!TheInGameUI->getAllSelectedDrawables()->empty()) {
				drawToEvaluateFor = TheInGameUI->getAllSelectedDrawables()->front();
			}

		Object* obj = drawToEvaluateFor ? drawToEvaluateFor->getObject() : nullptr;
		setPortraitByObject(obj);

		const Coord3D* exitPosition = nullptr;
		if (obj && obj->getControllingPlayer() == getCurrentlyViewedPlayer() && obj->getObjectExitInterface())
			exitPosition = obj->getObjectExitInterface()->getRallyPoint();

		showRallyPoint(exitPosition);
		return;
	}


	// check flashing
	if( m_flash )
	{
		// go through all the command buttons to see which one needs to flash
		for( Int i = 0; i < MAX_COMMANDS_PER_SET; ++i )
		{
			GameWindow *button = m_commandWindows[ i ];
			if( button != nullptr)
			{
				const CommandButton *commandButton = (const CommandButton *)GadgetButtonGetData(button);
				if( commandButton != nullptr )
				{
					if( commandButton->getFlashCount() > 0 && TheGameClient->getFrame() % 10 == 0 )
					{
						if( commandButton->getFlashCount() % 2 == 0 )
						{
							commandButton->setFlashCount(commandButton->getFlashCount() - 1);
							button->winSetStatus( WIN_STATUS_FLASHING );
						}
						else
						{
							commandButton->setFlashCount(commandButton->getFlashCount() - 1);
							button->winClearStatus( WIN_STATUS_FLASHING );
							if( commandButton->getFlashCount() == 0 )
							{
								setFlash( FALSE );
							}
						}
					}
				}
			}
		}
	}

	if(!m_contextParent[ CP_PURCHASE_SCIENCE ]->winIsHidden())
		updateContextPurchaseScience();

	//
	// first, if the UI is dirty repopulate the UI with what the user should see for all the
	// selected drawables
	//
	if( m_UIDirty )
	{
		evaluateContextUI();
		populateSpecialPowerShortcut(ThePlayerList->getLocalPlayer());
		// if we have a build tooltip layout, update it with the new data.
		repopulateBuildTooltipLayout();
	}

	// enable/disable the beacon button depending on if the max has been reached
	if (ThePlayerList && ThePlayerList->getLocalPlayer() && ThePlayerList->getLocalPlayer()->getPlayerTemplate())
	{
		Int count;
		const ThingTemplate *thing = TheThingFactory->findTemplate( ThePlayerList->getLocalPlayer()->getPlayerTemplate()->getBeaconTemplate() );
		ThePlayerList->getLocalPlayer()->countObjectsByThingTemplate( 1, &thing, false, &count );
		static NameKeyType beaconPlacementButtonID = NAMEKEY("ControlBar.wnd:ButtonPlaceBeacon");
		GameWindow *win = TheWindowManager->winGetWindowFromId(nullptr, beaconPlacementButtonID);
		if (win)
		{
			if (count < TheMultiplayerSettings->getMaxBeaconsPerPlayer())
			{
				win->winEnable(TRUE);
			}
			else
			{
				win->winEnable(FALSE);
			}
		}
	}

	//
	// most control bar contexts have one selected thing that we switch on and update
	// based on if that thing changed in some way ... the exception is when multi selected
	//
	if( m_currContext == CB_CONTEXT_MULTI_SELECT )
	{

		updateContextMultiSelect();
		return;

	}

	// if nothing is selected get out of here except if we're in the Purchase science context... that requires
	// us to not have anything selected
	if( m_currentSelectedDrawable == nullptr )
	{

		// we better be in the default none context
		DEBUG_ASSERTCRASH( m_currContext == CB_CONTEXT_NONE, ("ControlBar::update no selection, but not we're not showing the default NONE context") );
		return;

	}



	// if our selected drawable has no object get out of here
	Object *obj = nullptr;
	if(m_currentSelectedDrawable)
		obj = m_currentSelectedDrawable->getObject();
	if( obj == nullptr )
	{

		switchToContext( CB_CONTEXT_NONE, nullptr );
		return;

	}

	switch( m_currContext )
	{

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_NONE:
			break;

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_COMMAND:
			updateContextCommand();
			break;

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_STRUCTURE_INVENTORY:
			updateContextStructureInventory();
			break;

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_BEACON:
			updateContextBeacon();
			break;

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_UNDER_CONSTRUCTION:
			updateContextUnderConstruction();
			break;

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_OCL_TIMER:
			updateContextOCLTimer();
			break;

	}



}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::onDrawableSelected( Drawable *draw )
{

	// set a dirty flag so next time we update we can reconstruct the UI
	markUIDirty();

	// cancel any pending GUI commands
	TheInGameUI->setGUICommand( nullptr );


}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::onDrawableDeselected( Drawable *draw )
{

	// set a dirty flag so next time we update we can reconstruct the UI
	markUIDirty();

	if (TheInGameUI->getSelectCount() == 0)
	{
		// we just deselected everything - cancel any pending GUI commands
		TheInGameUI->setGUICommand( nullptr );
	}

	//
	// always when becoming unselected should we remove any build placement icons because if
	// we have some and are in the middle of a build process, it must obviously be over now
	// because we are no longer selecting the dozer or worker
	//
	TheInGameUI->placeBuildAvailable( nullptr, nullptr );

}

//-------------------------------------------------------------------------------------------------

const Image *ControlBar::getStarImage()
{
	if(m_lastFlashedAtPointValue > ThePlayerList->getLocalPlayer()->getSciencePurchasePoints() || ThePlayerList->getLocalPlayer()->getSciencePurchasePoints() <= 0)
		m_genStarFlash = FALSE;
	else
		m_lastFlashedAtPointValue = ThePlayerList->getLocalPlayer()->getSciencePurchasePoints();

	GameWindow *win= TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( "ControlBar.wnd:ButtonGeneral" ) );
	if(!win)
		return nullptr;
	if(!m_genStarFlash)
	{
		GadgetButtonSetEnabledImage(win, m_generalButtonEnable);
		return nullptr;
	}

	// TheSuperHackers @tweak bobtista 27/06/2026 Blink on a wall-clock cycle so the rate is independent of render frame rate and logic time scale.
	m_genStarFlashTimeAccumulator += TheFramePacer->getUpdateTime();
	const Real blinkPeriodSeconds = 1.0f;
	while( m_genStarFlashTimeAccumulator >= blinkPeriodSeconds )
	{
		m_genStarFlashTimeAccumulator -= blinkPeriodSeconds;
	}
	if( m_genStarFlashTimeAccumulator >= blinkPeriodSeconds / 2 )
	{
		GadgetButtonSetEnabledImage(win, m_generalButtonHighlight);
		return nullptr;
	}

	GadgetButtonSetEnabledImage(win, m_generalButtonEnable);

	return nullptr;

}


//-------------------------------------------------------------------------------------------------
void ControlBar::onPlayerRankChanged(const Player *p)
{
	if (!p->isLocalPlayer())
		return;

	if(!(m_lastFlashedAtPointValue > ThePlayerList->getLocalPlayer()->getSciencePurchasePoints()))
	{
		if(TheTransitionHandler && TheInGameUI->getInputEnabled())
			TheTransitionHandler->setGroup("ControlBarArrow");
	}
//	populateSpecialPowerShortcut((Player *)p);
	m_genStarFlash = TRUE;
	/// @todo implement me
	markUIDirty();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::onPlayerSciencePurchasePointsChanged(const Player *p)
{
	if (!p->isLocalPlayer())
		return;
	if(!(m_lastFlashedAtPointValue > ThePlayerList->getLocalPlayer()->getSciencePurchasePoints()))
	{
		if(TheTransitionHandler && TheInGameUI->getInputEnabled())
			TheTransitionHandler->setGroup("ControlBarArrow");
	}
//	populateSpecialPowerShortcut((Player *)p);
	m_genStarFlash = TRUE;
	/// @todo implement me
	markUIDirty();
}

//-------------------------------------------------------------------------------------------------
/** Given the drawables that we have selected into our context sensitive UI, evaluate
	* and perform all UI manipulations to make the GUI show to the user what we want them
	* to see */
//-------------------------------------------------------------------------------------------------
void ControlBar::evaluateContextUI()
{

	//
	// the UI has been "evaluated" and is now displaying the most current and correct
	// information to the player
	//
	m_UIDirty = FALSE;

	// if our purchase science window is up, we will want to update it by repopulating it.
	if(!m_contextParent[ CP_PURCHASE_SCIENCE ]->winIsHidden())
		showPurchaseScience();

	// erase any current state of the GUI by switching out to the empty context
	switchToContext( CB_CONTEXT_NONE, nullptr );

	// sanity, nothing selected
	if( TheInGameUI->getSelectCount() == 0 )
		return;

	// get the list of drawable IDs from the in game UI
	const DrawableList *selectedDrawables = TheInGameUI->getAllSelectedDrawables();

	// sanity
	if( selectedDrawables->empty() == TRUE )
		return;

	//Make sure the selected objects are in fact, controllable! If not, then
	//we don't show any GUI commands for them!!!
	//This is used when we select enemy objects or objects on another team.
	//@todo we may want to show their portrait
	if( !TheInGameUI->areSelectedObjectsControllable() )
	{
		//Also make sure the unit isn't a garrisonable neutral civ team building!
		Drawable *draw = selectedDrawables->front();

		//sanity
		if( !draw )
		{
			return;
		}
		Object *obj = draw->getObject();
		if( !obj )
		{
			return;
		}

		if (obj->getControllingPlayer()
			&& obj->getControllingPlayer()->getPlayerTemplate()
			&& obj->getControllingPlayer()->getPlayerTemplate()->getBeaconTemplate().compare(obj->getTemplate()->getName()) == 0
			)
		{
			switchToContext( CB_CONTEXT_BEACON, draw );
		}
		else
		{
			switchToContext( CB_CONTEXT_NONE, draw );
		}

		//Check for a contain interface and a enemy relationship and reject that!
		ContainModuleInterface *contain = obj->getContain();
		if( contain && contain->getContainMax() > 0 )
		{

			const Player *otherPlayer = contain->getApparentControllingPlayer(ThePlayerList->getLocalPlayer());
			if (!otherPlayer)
				otherPlayer = obj->getControllingPlayer();
			Player *player = ThePlayerList->getLocalPlayer();

			if( !player || !otherPlayer )
			{
				//Sanity.
				return;
			}
			Relationship relation = player->getRelationship( otherPlayer->getDefaultTeam() );

			//Note: All following checks already account for the fact that this object
			//isn't ours.

			//The only case we can actually see a non-controlled controlbar is a neutral garrisonable structure.
			if( !contain->isGarrisonable() || relation != NEUTRAL )
			{
				//Can't peek inside enemy/allied containers period!
				return;
			}
		}
		else
		{
			return;
		}
	}

	//
	// when we have multiple things selected, we will only display the common commands
	// in the center command bar that can be displayed with multi-units selected
	//


	Drawable *drawToEvaluateFor = nullptr;
	Bool multiSelect = FALSE;


	if( TheInGameUI->getSelectCount() > 1 )
	{
		// Attempt to isolate a Drawable here to evaluate
		// The need arises when selected is an AngryMob,
		// whose selection actually consists of varied units
		// but is represented in the UI as a single unit,
		// so we must isolate and evaluate only the Nexus
		drawToEvaluateFor = TheGameClient->findDrawableByID( TheInGameUI->getSoloNexusSelectedDrawableID() ) ;
		multiSelect = ( drawToEvaluateFor == nullptr );

	}
	else // get the first and only drawble in the selection list
		drawToEvaluateFor = selectedDrawables->front();



	if( multiSelect )
	{
		switchToContext( CB_CONTEXT_MULTI_SELECT, nullptr );
	}
	else if ( drawToEvaluateFor )// either we have exactly one drawable, or we have isolated one to evaluate for...
	{

		// get the first and only drawable in the selection list
		//Drawable *draw = selectedDrawables->front();

		// sanity
		//if( draw == nullptr )
		//	return;

		// get object
		Object *obj = drawToEvaluateFor->getObject();
		if( obj == nullptr )
			return;

		// we show no interface for objects being sold
		if( obj->getStatusBits().test( OBJECT_STATUS_SOLD ) )
			return;

		static const NameKeyType key_OCLUpdate = NAMEKEY( "OCLUpdate" );
		OCLUpdate *update = (OCLUpdate*)obj->findUpdateModule( key_OCLUpdate );

		//
		// a command center is context sensitive itself, if a side has *NOT* been chosen we display
		// the side select interface for command centers only, but note how under construction is
		// more important than anything
		//
		Bool contextSelected = FALSE;
		if( obj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		{

			switchToContext( CB_CONTEXT_UNDER_CONSTRUCTION, drawToEvaluateFor );
			contextSelected = TRUE;

		}

		// check for a regular switch to the appropriate context
		if( contextSelected == FALSE )
		{
			ContainModuleInterface *cmi = obj->getContain();

			if( cmi && cmi->isGarrisonable() && obj->getCommandSetString().isEmpty() )
			{
				//Kris: This is a convenient section to graft an inventory commandset for
				//garrisoned troops. However, we only want to use this if we DON'T have
				//a commandset defined. If we do, then trust that the commandset will
				//handle it!

				Player *localPlayer = ThePlayerList->getLocalPlayer();
				Relationship relationship;

				// we cannot select objects that are controlled by our enemies
				relationship = localPlayer->getRelationship( obj->getTeam() );
				if( obj->isLocallyControlled() == TRUE || relationship == NEUTRAL )
					switchToContext( CB_CONTEXT_STRUCTURE_INVENTORY, drawToEvaluateFor );

			}
			else if( update )
			{
				switchToContext( CB_CONTEXT_OCL_TIMER, drawToEvaluateFor );
			}
			else if( obj->getCommandSetString().isEmpty() == FALSE )
			{

				switchToContext( CB_CONTEXT_COMMAND, drawToEvaluateFor );

			}
			else if (obj->getControllingPlayer()->getPlayerTemplate()->getBeaconTemplate().compare(obj->getTemplate()->getName()) == 0)
			{
				switchToContext( CB_CONTEXT_BEACON, drawToEvaluateFor );
			}
			else
				switchToContext( CB_CONTEXT_NONE, drawToEvaluateFor );
		}

	}

}

//-------------------------------------------------------------------------------------------------
/** Find a command button of the given name if present */
//-------------------------------------------------------------------------------------------------
CommandButton *ControlBar::findNonConstCommandButton( const AsciiString& name )
{

	for( const CommandButton *command = m_commandButtons; command; command = command->getNext() )
		if( command->getName() == name )
			return const_cast<CommandButton*>((const CommandButton*)command->getFinalOverride());

	return nullptr;  // not found

}

//-------------------------------------------------------------------------------------------------
/** Allocate a new command button, assign name, and tie to list */
//-------------------------------------------------------------------------------------------------
CommandButton *ControlBar::newCommandButton( const AsciiString& name )
{
	CommandButton *newButton;

	// allocate new button
	newButton = newInstance(CommandButton);

	// assign name
	newButton->setName(name);

	// link to list
	newButton->friend_addToList(&m_commandButtons);

	// return the new button
	return newButton;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CommandButton *ControlBar::newCommandButtonOverride( CommandButton *buttonToOverride )
{
	if (!buttonToOverride) {
		return nullptr;
	}

	CommandButton *newOverride;

	// allocate new button
	newOverride = newInstance(CommandButton);

	*newOverride = *buttonToOverride;

	newOverride->markAsOverride();
	buttonToOverride->setNextOverride(newOverride);

	return newOverride;
}

//-------------------------------------------------------------------------------------------------
/** Parse a command set */
//-------------------------------------------------------------------------------------------------
/*static*/ void ControlBar::parseCommandSetDefinition( INI *ini )
{
	AsciiString name;
	CommandSet *commandSet;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	// find existing item if present
	commandSet = TheControlBar->findNonConstCommandSet( name );
	if( commandSet == nullptr )
	{

		// allocate a new item
		commandSet = TheControlBar->newCommandSet( name );
		if (ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES) {
			commandSet->markAsOverride();
		}
	}
	else if( ini->getLoadType() != INI_LOAD_CREATE_OVERRIDES )
	{
		//Holy crap, this sucks to debug!!!
		//If you have two different command sets, the previous
		//code would simply allow you to define multiple command set
		//with the same name, and just nuke the old button with the new one.
		//So, I (KM) have added this assert to notify in case of two same-name
		//command set.
		DEBUG_CRASH(( "[LINE: %d in '%s'] Duplicate commandset %s found!", ini->getLineNum(), ini->getFilename().str(), name.str() ));
		throw INI_INVALID_DATA;

		//@todo SUPPORT OVERRIDES -- JM
	} else {
		commandSet = TheControlBar->newCommandSetOverride(commandSet);
	}

	// sanity
	DEBUG_ASSERTCRASH( commandSet, ("parseCommandSetDefinition: Unable to allocate set '%s'", name.str()) );

	// parse the ini definition
	ini->initFromINI( commandSet, commandSet->friend_getFieldParse() );

}

//-------------------------------------------------------------------------------------------------
/** Find existing command set by name */
//-------------------------------------------------------------------------------------------------
CommandSet* ControlBar::findNonConstCommandSet( const AsciiString& name )
{
	CommandSet* set;

	for( set = m_commandSets; set != nullptr; set = set->friend_getNext() )
		if( set->getName() == name )
			return const_cast<CommandSet*>((const CommandSet *) set);

	return nullptr;  // set not found

}
//-------------------------------------------------------------------------------------------------
/** find existing command button if present	*/
//-------------------------------------------------------------------------------------------------
const CommandButton *ControlBar::findCommandButton( const AsciiString& name )
{
	CommandButton *btn =  findNonConstCommandButton(name);
	if( btn )
	{
		btn = (CommandButton *)btn->friend_getFinalOverride();
	}
	return btn;
}

//-------------------------------------------------------------------------------------------------
/** Find existing command set by name */
//-------------------------------------------------------------------------------------------------
const CommandSet *ControlBar::findCommandSet( const AsciiString& name )
{
	CommandSet *set = findNonConstCommandSet(name);
	if (set)
		set = (CommandSet*)set->friend_getFinalOverride();
	return set;
}

//-------------------------------------------------------------------------------------------------
/** Allocate a new command set, link to list, initialize to default, and return it */
//-------------------------------------------------------------------------------------------------
CommandSet *ControlBar::newCommandSet( const AsciiString& name )
{
	// allocate a new set
	CommandSet* set = newInstance(CommandSet)(name);
	// add it to the list.
	set->friend_addToList(&m_commandSets);
	// return the newly created set
	return set;

}

//-------------------------------------------------------------------------------------------------
/** Create an overridden command set. */
//-------------------------------------------------------------------------------------------------
CommandSet *ControlBar::newCommandSetOverride( CommandSet *setToOverride )
{
	if (!setToOverride) {
		return nullptr;
	}

	// allocate a new set
	CommandSet* set = newInstance(CommandSet)(setToOverride->getName());

	// it's an override; DON'T add it to the main list.
	// !!! DO NOT DO THIS !!! -- > set->friend_addToList(&m_commandSets); <-- !!! DO NOT DO THIS !!!

	*set = *setToOverride;
	set->markAsOverride();

	setToOverride->setNextOverride(set);

	return set;
}

//-------------------------------------------------------------------------------------------------
/** Process a button click for the context sensitive GUI */
//-------------------------------------------------------------------------------------------------
CBCommandStatus ControlBar::processContextSensitiveButtonClick( GameWindow *button,
																																GadgetGameMessage gadgetMessage )
{

	// call command processing method
	return processCommandUI( button, gadgetMessage );

}

//-------------------------------------------------------------------------------------------------
/** Process a button click for the context sensitive GUI */
//-------------------------------------------------------------------------------------------------
CBCommandStatus ControlBar::processContextSensitiveButtonTransition( GameWindow *button,
																																GadgetGameMessage gadgetMessage )
{

	// call command processing method
	return processCommandTransitionUI( button, gadgetMessage );

}


//-------------------------------------------------------------------------------------------------
/** Switch the user interface to the new context specified and fill out any of the
	* art and/or buttons that we need to for the new context using data from the object
	* passed in */
//-------------------------------------------------------------------------------------------------
void ControlBar::switchToContext( ControlBarContext context, Drawable *draw )
{

	// restore the right hud to a plain window
	setPortraitByObject( nullptr );

	Object *obj = draw ? draw->getObject() : nullptr;
	setPortraitByObject( obj );

	// if we're switching context, we have to repopulate the hotkey manager
	if(TheHotKeyManager)
		TheHotKeyManager->reset();

	// ... and also remove any radius cursor that is active.
	TheInGameUI->setRadiusCursorNone();

	// save a pointer for the currently selected drawable
	m_currentSelectedDrawable = draw;

	if (IsInGameChatActive() == FALSE && TheGameLogic && !TheGameLogic->isInShellGame()) {
		TheWindowManager->winSetFocus( nullptr );
	}

	// hide/un-hide the appropriate windows for the context
	switch( context )
	{

		//-------------------------------------------------------------------------------------------------
		case CB_CONTEXT_NONE:
		{

			// show or hide the right window groups
			//m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
			m_contextParent[ CP_COMMAND ]->winHide( TRUE );
			m_contextParent[ CP_BUILD_QUEUE ]->winHide( TRUE );
			m_contextParent[ CP_BEACON ]->winHide( TRUE );
			m_contextParent[ CP_UNDER_CONSTRUCTION ]->winHide( TRUE );
			m_contextParent[ CP_OCL_TIMER ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_INFO ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_LIST ]->winHide( TRUE );

			//Clear any potentially flashing buttons!
			for( int i = 0; i < MAX_COMMANDS_PER_SET; i++ )
			{
				// the implementation won't necessarily use the max number of windows possible
				if (m_commandWindows[ i ])
				{
					m_commandWindows[ i ]->winClearStatus( WIN_STATUS_FLASHING );
				}
			}
			// if there is a current selected drawable then we wil display a selection portrait if present
			if( draw )
			{
				//Get the current thing template.
				const ThingTemplate *thing = draw->getTemplate();

				//Special case -- if we are a GLA hole, then get the rebuild building template
				Object *obj = draw->getObject();
				if( obj && obj->isKindOf( KINDOF_REBUILD_HOLE ) )
				{
					RebuildHoleBehaviorInterface *rhbi = RebuildHoleBehavior::getRebuildHoleBehaviorInterfaceFromObject( obj );
					if( rhbi )
					{
						thing = rhbi->getRebuildTemplate();
					}
				}

				//Set the correct portrait.
				setPortraitByObject( obj );
			}

			// do not show any rally point marker
			showRallyPoint( nullptr );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_COMMAND:
		{

			// show or hide the right window groups
			//m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
			m_contextParent[ CP_COMMAND ]->winHide( FALSE );
			m_contextParent[ CP_BUILD_QUEUE ]->winHide( TRUE );
			m_contextParent[ CP_BEACON ]->winHide( TRUE );
			m_contextParent[ CP_UNDER_CONSTRUCTION ]->winHide( TRUE );
			m_contextParent[ CP_OCL_TIMER ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_INFO ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_LIST ]->winHide( TRUE );

			// fill the specific UI info
			populateCommand( draw->getObject() );

			//
			// for objects that are able to create units, we show a build queue if we actually
			// actually have something in production, otherwise we show the selection portrait
			//
			if( obj )
			{
				ProductionUpdateInterface *pu = obj->getProductionUpdateInterface();

				if( pu && pu->firstProduction() != nullptr )
				{

					m_contextParent[ CP_BUILD_QUEUE ]->winHide( FALSE );
					populateBuildQueue( obj );
					setPortraitByObject( nullptr );
				}
				else
				{
					setPortraitByObject( obj );
				}

			}

			break;

		}

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_STRUCTURE_INVENTORY:
		{

			// show or hide the right window groups
			//m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
			m_contextParent[ CP_COMMAND ]->winHide( FALSE );
			m_contextParent[ CP_BUILD_QUEUE ]->winHide( TRUE );
			m_contextParent[ CP_BEACON ]->winHide( TRUE );
			m_contextParent[ CP_UNDER_CONSTRUCTION ]->winHide( TRUE );
			m_contextParent[ CP_OCL_TIMER ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_INFO ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_LIST ]->winHide( TRUE );

			// fill the specific UI info
			populateStructureInventory( draw->getObject() );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_BEACON:
		{

			// show or hide the right window groups
			//m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
			m_contextParent[ CP_COMMAND ]->winHide( TRUE );
			m_contextParent[ CP_BUILD_QUEUE ]->winHide( TRUE );
			m_contextParent[ CP_BEACON ]->winHide( FALSE );
			m_contextParent[ CP_UNDER_CONSTRUCTION ]->winHide( TRUE );
			m_contextParent[ CP_OCL_TIMER ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_INFO ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_LIST ]->winHide( TRUE );


			// fill the specific UI info
			populateBeacon( draw->getObject() );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_UNDER_CONSTRUCTION:
		{

			// show or hide the right window groups
			//m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
			m_contextParent[ CP_COMMAND ]->winHide( TRUE );
			m_contextParent[ CP_BUILD_QUEUE ]->winHide( TRUE );
			m_contextParent[ CP_BEACON ]->winHide( TRUE );
			m_contextParent[ CP_UNDER_CONSTRUCTION ]->winHide( FALSE );
			m_contextParent[ CP_OCL_TIMER ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_INFO ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_LIST ]->winHide( TRUE );

			// fill the specific UI info
			populateUnderConstruction( draw->getObject() );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_OCL_TIMER:
		{

			// show or hide the right window groups
//			m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
			m_contextParent[ CP_COMMAND ]->winHide( TRUE );
			m_contextParent[ CP_BUILD_QUEUE ]->winHide( TRUE );
			m_contextParent[ CP_BEACON ]->winHide( TRUE );
			m_contextParent[ CP_UNDER_CONSTRUCTION ]->winHide( TRUE );
			m_contextParent[ CP_OCL_TIMER ]->winHide( FALSE );
			m_contextParent[ CP_OBSERVER_INFO ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_LIST ]->winHide( TRUE );

			// fill the specific UI info
			populateOCLTimer( draw->getObject() );

			break;

		}

		//---------------------------------------------------------------------------------------------
		case CB_CONTEXT_MULTI_SELECT:
		{

			// show or hide the right window groups
//			m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
			m_contextParent[ CP_COMMAND ]->winHide( FALSE );		// multi select shows common commands
			m_contextParent[ CP_BUILD_QUEUE ]->winHide( TRUE );
			m_contextParent[ CP_BEACON ]->winHide( TRUE );
			m_contextParent[ CP_UNDER_CONSTRUCTION ]->winHide( TRUE );
			m_contextParent[ CP_OCL_TIMER ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_INFO ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_LIST ]->winHide( TRUE );


			// fill the specific UI info
			populateMultiSelect();

			break;

		}
		case CB_CONTEXT_OBSERVER_LIST:
		{

			// show or hide the right window groups
//			m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
			m_contextParent[ CP_COMMAND ]->winHide( TRUE );
			m_contextParent[ CP_BUILD_QUEUE ]->winHide( TRUE );
			m_contextParent[ CP_BEACON ]->winHide( TRUE );
			m_contextParent[ CP_UNDER_CONSTRUCTION ]->winHide( TRUE );
			m_contextParent[ CP_OCL_TIMER ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_INFO ]->winHide( TRUE );
			m_contextParent[ CP_OBSERVER_LIST ]->winHide( FALSE );


			// fill the specific UI info
			populateObserverList();
			break;

		}

		//---------------------------------------------------------------------------------------------
		default:
		{

			DEBUG_CRASH( ("ControlBar::switchToContext, unknown context '%d'", context) );
			break;

		}

	}

	// save our context
	m_currContext = context;

}

void ControlBar::setCommandBarBorder( GameWindow *button, CommandButtonMappedBorderType type)
{
	if(!button)
		return;

	switch( type )
	{
		case COMMAND_BUTTON_BORDER_BUILD:
		{
			GadgetButtonSetBorder(button, m_commandButtonBorderBuildColor);
			break;
		}
		//-------------------------------------------------------------------------------------------------
		case COMMAND_BUTTON_BORDER_UPGRADE:
		{
			GadgetButtonSetBorder(button, m_commandButtonBorderUpgradeColor );
			break;
		}
		//-------------------------------------------------------------------------------------------------
		case COMMAND_BUTTON_BORDER_ACTION:
		{
			GadgetButtonSetBorder(button, m_commandButtonBorderActionColor);
			break;
		}
		//-------------------------------------------------------------------------------------------------
		case COMMAND_BUTTON_BORDER_SYSTEM:
		{
			GadgetButtonSetBorder(button, m_commandButtonBorderSystemColor);
			break;
		}

		//-------------------------------------------------------------------------------------------------
		case COMMAND_BUTTON_BORDER_NONE:
		default:
			GadgetButtonSetBorder(button, GAME_COLOR_UNDEFINED, FALSE);
	}
}


//-------------------------------------------------------------------------------------------------
/** Set the command data into the control */
//-------------------------------------------------------------------------------------------------
void ControlBar::setControlCommand( GameWindow *button, const CommandButton *commandButton )
{

	// the window must be a gadget button
	if( button->winGetInputFunc() != GadgetPushButtonInput )
	{

		DEBUG_CRASH( ("setControlCommand: Window is not a button") );
		return;

	}

	// sanity
	if( commandButton == nullptr )
	{

		DEBUG_CRASH( ("setControlCommand: null commandButton passed in") );
		return;

	}

	//
	// set the button gadget control to be a normal button or a check like button if
	// the command says it needs one
	//
	if( BitIsSet( commandButton->getOptions(), CHECK_LIKE ))
		GadgetButtonEnableCheckLike( button, TRUE, FALSE );
	else
		GadgetButtonEnableCheckLike( button, FALSE, FALSE );

	//
	// set the imagry ... note that for 99% of the command buttons it's sufficient to specify
	// only the disabled, enabled, hilite, and hilite pushed images.  For push-like buttons
	// we actually utilize all the state available to a GameWindow.  We will replicate the
	// hilite pushed image to be the enabled pushed image ... and we will also replicate
	// the disabled image to be the disabled pushed image.  For complete control over all
	// the states of these buttons we would add additional lines to the INI for a command
	// button and store those additional images in the command button
	//
	if( commandButton->getButtonImage() )
		GadgetButtonSetEnabledImage( button, commandButton->getButtonImage() );

	//if( commandButton->getDisabledImage() )
	//{
	//	GadgetButtonSetDisabledImage( button, commandButton->getDisabledImage() );
	//	GadgetButtonSetDisabledSelectedImage( button, commandButton->getDisabledImage() );
	//}  //end if
	//if( commandButton->getHiliteImage() )
	//	GadgetButtonSetHiliteImage( button, commandButton->getHiliteImage() );
	//if( commandButton->getPushedImage() )
	//{
	//	GadgetButtonSetHiliteSelectedImage( button, commandButton->getPushedImage() );
	//	GadgetButtonSetEnabledSelectedImage( button, commandButton->getPushedImage() );
	//}  // end if

	// set the text
	if( commandButton->getTextLabel().isEmpty() == FALSE || !commandButton->getScienceVec().empty())
	{
		button->winSetTooltipFunc(commandButtonTooltip);
	}
	else
		GadgetButtonSetText( button, L"" );

	// save the command in the user data of the window
	GadgetButtonSetData(button, (void*)commandButton);
	//button->winSetUserData( commandButton );

	setCommandBarBorder(button, commandButton->getCommandButtonMappedBorderType());

	if (TheHotKeyManager)
	{
		AsciiString hotKey =	TheHotKeyManager->searchHotKey(commandButton->getTextLabel());
		if(hotKey.isNotEmpty())
			TheHotKeyManager->addHotKey(button, hotKey);
	}
	GadgetButtonSetAltSound(button, "GUICommandBarClick");

}

//-------------------------------------------------------------------------------------------------
void CommandButton::cacheButtonImage()
{
	if (!TheMappedImageCollection) {
		return;
	}
	if( m_buttonImageName.isNotEmpty() )
	{
		m_buttonImage = TheMappedImageCollection->findImageByName( m_buttonImageName );
		DEBUG_ASSERTCRASH( m_buttonImage, ("CommandButton: %s is looking for button image %s but can't find it. Skipping...", m_name.str(), m_buttonImageName.str() ) );
		m_buttonImageName.clear();	// we're done with this, so nuke it
	}
}

//-------------------------------------------------------------------------------------------------
/** post process step, after all commands and command sets are loaded */
//-------------------------------------------------------------------------------------------------
void ControlBar::postProcessCommands()
{
	for ( CommandButton *button = m_commandButtons; button; button = button->friend_getNext() )
	{
		button->cacheButtonImage();
	}
}

//-------------------------------------------------------------------------------------------------
/** set the command for the button identified by the window name
	* NOTE that parent may be nullptr, it only helps to speed up the search for a particular
	* window ID */
//-------------------------------------------------------------------------------------------------
void ControlBar::setControlCommand( const AsciiString& buttonWindowName, GameWindow *parent,
																		const CommandButton *commandButton )
{
	UnsignedInt winID = TheNameKeyGenerator->nameToKey( buttonWindowName );
	GameWindow *win = TheWindowManager->winGetWindowFromId( parent, winID );

	if( win == nullptr )
	{

		DEBUG_CRASH( ("setControlCommand: Unable to find window '%s'", buttonWindowName.str()) );
		return;

	}

	// call the workhorse
	setControlCommand( win, commandButton );

}

//-------------------------------------------------------------------------------------------------
/** show/hide the portrait window image */
//-------------------------------------------------------------------------------------------------
void ControlBar::setPortraitByImage( const Image *image )
{

	if( image )
	{
		m_rightHUDUnitSelectParent->winHide(FALSE);
		m_rightHUDCameoWindow->winSetEnabledImage( 0, image );
		//m_rightHUDWindow->winSetEnabledImage( 0, image );
		m_rightHUDWindow->winClearStatus( WIN_STATUS_IMAGE );
		m_rightHUDCameoWindow->winSetStatus( WIN_STATUS_IMAGE );
		for(Int i = 0; i < MAX_UPGRADE_CAMEO_UPGRADES; ++i)
			m_rightHUDUpgradeCameos[i]->winHide(TRUE);

	}
	else
	{
		m_rightHUDWindow->winSetStatus( WIN_STATUS_IMAGE );
		m_rightHUDCameoWindow->winClearStatus( WIN_STATUS_IMAGE );
		m_rightHUDUnitSelectParent->winHide(TRUE);
		for(Int i = 0; i < MAX_UPGRADE_CAMEO_UPGRADES; ++i)
			m_rightHUDUpgradeCameos[i]->winHide(TRUE);
		//m_rightHUDWindow->winSetEnabledImage( 0, image );
		//m_rightHUDWindow->winSetStatus( WIN_STATUS_IMAGE );

	}

}

//-------------------------------------------------------------------------------------------------
/** show/hide the portrait image by object.  We like to use this method as opposed to the
	* plain image one above so that we can build more intelligence into what portrait to
	* show for an object given its current state or object type */
//-------------------------------------------------------------------------------------------------
void ControlBar::setPortraitByObject( Object *obj )
{

	if( obj )
	{
		if( obj->isKindOf( KINDOF_SHOW_PORTRAIT_WHEN_CONTROLLED ) && !obj->isLocallyControlled() )
		{
			//Handles civ vehicles without terrorists in them
			setPortraitByObject( nullptr );
			return;
		}

		const ThingTemplate *thing = obj->getTemplate();
		Player *player = obj->getControllingPlayer();

		//If we have an enemy stealth disguised unit, swap portraits!
		Drawable *draw = obj->getDrawable();
		if( draw && draw->getStealthLook() == STEALTHLOOK_DISGUISED_ENEMY )
		{
			thing = draw->getTemplate();
			if( thing->isKindOf( KINDOF_SHOW_PORTRAIT_WHEN_CONTROLLED ) )
			{
				//If a bomb truck disguises as a civ vehicle, don't use it's portrait (or else you'll see the terrorist).
				setPortraitByObject( nullptr );
				return;
			}
			StealthUpdate *stealth = obj->getStealth();
			if( stealth && stealth->isDisguised() )
			{
				//Fake player upgrades too!
				player = ThePlayerList->getNthPlayer( stealth->getDisguisedPlayerIndex() );
			}
		}

		const Image* portrait = thing->getSelectedPortraitImage();

		m_rightHUDUnitSelectParent->winHide(FALSE);
		// enable the window window as an image window and set the image
		m_rightHUDCameoWindow->winSetEnabledImage( 0, portrait );

		//Display the veterancy rank of the object on the portrait.
		const Image *image = calculateVeterancyOverlayForObject( obj );
		GadgetButtonDrawOverlayImage( m_rightHUDCameoWindow, image );

		//m_rightHUDWindow->winSetEnabledImage( 0, portrait );
		m_rightHUDWindow->winClearStatus( WIN_STATUS_IMAGE );
		m_rightHUDCameoWindow->winSetStatus( WIN_STATUS_IMAGE );

		for(Int i = 0; i < MAX_UPGRADE_CAMEO_UPGRADES; ++i)
		{
			AsciiString upgradeName = thing->getUpgradeCameoName(i);
			if(upgradeName.isEmpty())
			{
				m_rightHUDUpgradeCameos[i]->winHide(TRUE);
				continue;
			}
			const UpgradeTemplate *ut =  TheUpgradeCenter->findUpgrade(upgradeName);
			if(!ut)
			{
				m_rightHUDUpgradeCameos[i]->winHide(TRUE);
				continue;
			}

			m_rightHUDUpgradeCameos[i]->winHide(FALSE);
			m_rightHUDUpgradeCameos[i]->winSetEnabledImage( 0, ut->getButtonImage() );
			if( obj->hasUpgrade(ut) )
			{
				//Object level upgrades
				m_rightHUDUpgradeCameos[i]->winEnable( TRUE );
			}
			else if( player && player->hasUpgradeComplete( ut ) )
			{
				//Player level upgrades
				m_rightHUDUpgradeCameos[i]->winEnable( TRUE );
			}
			else
			{
				//Failure
				m_rightHUDUpgradeCameos[i]->winEnable( FALSE );
			}
		}


	}
	else
	{
		m_rightHUDUnitSelectParent->winHide(TRUE);
		m_rightHUDWindow->winSetStatus( WIN_STATUS_IMAGE );
		m_rightHUDCameoWindow->winClearStatus( WIN_STATUS_IMAGE );
		for(Int i = 0; i < MAX_UPGRADE_CAMEO_UPGRADES; ++i)
			m_rightHUDUpgradeCameos[i]->winHide(TRUE);

		//Clear any overlay the portrait had on it.
		GadgetButtonDrawOverlayImage( m_rightHUDCameoWindow, nullptr );
	}

}

// ------------------------------------------------------------------------------------------------
/** Show a rally point marker at the world location specified.  If no location is specified
	* any marker that we might have visible is hidden */
// ------------------------------------------------------------------------------------------------
void ControlBar::showRallyPoint(const Coord3D* loc)
{
	// if loc is null, destroy any rally point drawable we have shown
	if (loc == nullptr)
	{
		// destroy rally point drawable if present
		if (m_rallyPointDrawableID != INVALID_DRAWABLE_ID)
			TheGameClient->destroyDrawable(TheGameClient->findDrawableByID(m_rallyPointDrawableID));

		m_rallyPointDrawableID = INVALID_DRAWABLE_ID;
		return;
	}

	Drawable* marker = nullptr;

	// create a rally point drawable if necessary
	if (m_rallyPointDrawableID == INVALID_DRAWABLE_ID)
	{
		const ThingTemplate* ttn = TheThingFactory->findTemplate("RallyPointMarker");
		marker = TheThingFactory->newDrawable(ttn);
		DEBUG_ASSERTCRASH(marker, ("showRallyPoint: Unable to create rally point drawable"));
		if (marker)
		{
			marker->setDrawableStatus(DRAWABLE_STATUS_NO_SAVE);
			m_rallyPointDrawableID = marker->getID();
		}
	}
	else
		marker = TheGameClient->findDrawableByID(m_rallyPointDrawableID);

	// sanity
	DEBUG_ASSERTCRASH(marker, ("showRallyPoint: No rally point marker found"));

	// set the position of the rally point drawable to the position passed in
	marker->setPosition(loc);
	marker->setOrientation(TheGlobalData->m_downwindAngle); // To blow down wind -- ML

	// set the marker colors to that of the local player
	Player* player = getCurrentlyViewedPlayer();
	if (player)
	{
		if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT)
			marker->setIndicatorColor(player->getPlayerNightColor());
		else
			marker->setIndicatorColor(player->getPlayerColor());
	}
}

// ------------------------------------------------------------------------------------------------
/** Show a rally point marker at the world location specified.  If no location is specified
	* any marker that we might have visible is hidden */
// ------------------------------------------------------------------------------------------------
void ControlBar::setControlBarSchemeByPlayer(Player *p)
{
	if(m_controlBarSchemeManager)
		m_controlBarSchemeManager->setControlBarSchemeByPlayer(p);

	static NameKeyType buttonPlaceBeaconID = NAMEKEY( "ControlBar.wnd:ButtonPlaceBeacon" );
	static NameKeyType buttonIdleWorkerID = NAMEKEY("ControlBar.wnd:ButtonIdleWorker");
	static NameKeyType buttonGeneralID = NAMEKEY("ControlBar.wnd:ButtonGeneral");
	GameWindow *buttonPlaceBeacon = TheWindowManager->winGetWindowFromId( nullptr, buttonPlaceBeaconID );
	GameWindow *buttonIdleWorker = TheWindowManager->winGetWindowFromId( nullptr, buttonIdleWorkerID );
	GameWindow *buttonGeneral = TheWindowManager->winGetWindowFromId( nullptr, buttonGeneralID );

	if( !p->isPlayerActive() )
	{
		m_isObserverCommandBar = TRUE;
		switchToContext( CB_CONTEXT_OBSERVER_LIST, nullptr );
		DEBUG_LOG(("We're loading the Observer Command Bar"));

		if (buttonPlaceBeacon)
			buttonPlaceBeacon->winHide(TRUE);
		if (buttonIdleWorker)
			buttonIdleWorker->winHide(TRUE);
		if (buttonGeneral)
			buttonGeneral->winEnable(FALSE);
	}
	else
	{
		switchToContext( CB_CONTEXT_NONE, nullptr );
		m_isObserverCommandBar = FALSE;

		if (buttonPlaceBeacon)
			buttonPlaceBeacon->winHide(
			(TheGameLogic->getGameMode() != GAME_LAN && TheGameLogic->getGameMode() != GAME_INTERNET) ||
			!TheGameInfo->isMultiPlayer());
		if (buttonIdleWorker)
			buttonIdleWorker->winHide(FALSE);
		if (buttonGeneral)
		{
			buttonGeneral->winHide(FALSE);
			buttonGeneral->winEnable(TRUE);
		}
	}
	switchControlBarStage(CONTROL_BAR_STAGE_DEFAULT);
}

void ControlBar::setControlBarSchemeByPlayerTemplate( const PlayerTemplate *pt)
{
	if(m_controlBarSchemeManager)
		m_controlBarSchemeManager->setControlBarSchemeByPlayerTemplate(pt);

	static NameKeyType buttonPlaceBeaconID = NAMEKEY( "ControlBar.wnd:ButtonPlaceBeacon" );
	static NameKeyType buttonIdleWorkerID = NAMEKEY("ControlBar.wnd:ButtonIdleWorker");
	static NameKeyType buttonGeneralID = NAMEKEY("ControlBar.wnd:ButtonGeneral");
	GameWindow *buttonPlaceBeacon = TheWindowManager->winGetWindowFromId( nullptr, buttonPlaceBeaconID );
	GameWindow *buttonIdleWorker = TheWindowManager->winGetWindowFromId( nullptr, buttonIdleWorkerID );
	GameWindow *buttonGeneral = TheWindowManager->winGetWindowFromId( nullptr, buttonGeneralID );

	if(pt == ThePlayerTemplateStore->findPlayerTemplate(TheNameKeyGenerator->nameToKey("FactionObserver")))
	{
		m_isObserverCommandBar = TRUE;
		switchToContext( CB_CONTEXT_OBSERVER_LIST, nullptr );
		DEBUG_LOG(("We're loading the Observer Command Bar"));

		if (buttonPlaceBeacon)
			buttonPlaceBeacon->winHide(TRUE);
		if (buttonIdleWorker)
			buttonIdleWorker->winHide(TRUE);
		if (buttonGeneral)
			buttonGeneral->winEnable(FALSE);
	}
	else
	{
		switchToContext( CB_CONTEXT_NONE, nullptr );
		m_isObserverCommandBar = FALSE;

		if (buttonPlaceBeacon)
			buttonPlaceBeacon->winHide(
			(TheGameLogic->getGameMode() != GAME_LAN && TheGameLogic->getGameMode() != GAME_INTERNET) ||
			!TheGameInfo->isMultiPlayer());
		if (buttonIdleWorker)
			buttonIdleWorker->winHide(FALSE);
		if (buttonGeneral)
		{
			buttonGeneral->winHide(FALSE);
			buttonGeneral->winEnable(TRUE);
		}
	}
	switchControlBarStage(CONTROL_BAR_STAGE_DEFAULT);

	hidePurchaseScience();
}

void ControlBar::setControlBarSchemeByName(const AsciiString& name)
{
	if(m_controlBarSchemeManager)
		m_controlBarSchemeManager->setControlBarScheme( name );
		switchControlBarStage(CONTROL_BAR_STAGE_DEFAULT);

}

void ControlBar::preloadAssets( TimeOfDay timeOfDay )
{
	if (m_controlBarSchemeManager)
		m_controlBarSchemeManager->preloadAssets( timeOfDay );
}

void ControlBar::updateBuildQueueDisabledImages( const Image *image )
{
	if(!image)
		return;
	// We have to do this because the build queue data might have been reset
	static NameKeyType buildQueueIDs[ MAX_BUILD_QUEUE_BUTTONS ];
	static Bool idsInitialized = FALSE;
	Int i;

	// get name key ids for the build queue buttons
	if( idsInitialized == FALSE )
	{
		AsciiString buttonName;

		for( i = 0; i < MAX_BUILD_QUEUE_BUTTONS; i++ )
		{

			buttonName.format( "ControlBar.wnd:ButtonQueue%02d", i + 1 );
			buildQueueIDs[ i ] = TheNameKeyGenerator->nameToKey( buttonName );

		}

		idsInitialized = TRUE;

	}

	// get window pointers to all the buttons for the build queue
	for( i = 0; i < MAX_BUILD_QUEUE_BUTTONS; i++ )
	{

		// get window commented out cause I believe we already set this.  We'll see in a few minutes
		m_queueData[ i ].control = TheWindowManager->winGetWindowFromId( m_contextParent[ CP_BUILD_QUEUE ],
																																		 buildQueueIDs[ i ] );

		GadgetButtonSetDisabledImage( m_queueData[ i ].control, image );

	}

}

void ControlBar::updateRightHUDImage( const Image *image )
{
	if(!m_rightHUDWindow || !image)
		return;
	m_rightHUDWindow->winSetEnabledImage(0, image);

}

void ControlBar::updateBuildUpClockColor( Color color)
{
	m_buildUpClockColor = color;
}



void ControlBar::updateCommandBarBorderColors(Color build, Color action, Color upgrade, Color system )
{
	m_commandButtonBorderBuildColor = build;
	m_commandButtonBorderActionColor = action;
	m_commandButtonBorderUpgradeColor = upgrade;
	m_commandButtonBorderSystemColor = system;
}

// ---------------------------------------------------------------------------------------
// hides the communicator button
void ControlBar::hideCommunicator( Bool b )
{
	//sanity
	if( m_communicatorButton != nullptr )
		m_communicatorButton->winHide( b );
}

// ---------------------------------------------------------------------------------------
// Outside hook so when the genera's head is pushed, we can switch to the purchase science
// context
void ControlBar::updatePurchaseScience()
{
//	if(m_generalsScreenAnimate && TheGlobalData->m_animateWindows)
//	{
//		Bool wasFinished = m_generalsScreenAnimate->isFinished();
//		m_generalsScreenAnimate->update();
//		if (m_generalsScreenAnimate->isFinished() && !wasFinished && m_generalsScreenAnimate->isReversed())
//			m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide(TRUE);
//	}
}

void ControlBar::showPurchaseScience()
{

	if(TheScriptEngine->isGameEnding())
		return;
	populatePurchaseScience(ThePlayerList->getLocalPlayer());
	m_genStarFlash = FALSE;
	if(!m_contextParent[ CP_PURCHASE_SCIENCE ]->winIsHidden())
		return;
	//switchToContext(CB_CONTEXT_PURCHASE_SCIENCE, nullptr);
	m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide(FALSE);
	if (TheGlobalData->m_animateWindows)
		TheTransitionHandler->setGroup("GenExpFade");
		//m_generalsScreenAnimate->registerGameWindow( m_contextParent[ CP_PURCHASE_SCIENCE ], WIN_ANIMATION_SLIDE_TOP, TRUE, 200 );

}

void ControlBar::hidePurchaseScience()
{
	if(m_contextParent[ CP_PURCHASE_SCIENCE ]->winIsHidden())
		return;

	if( m_contextParent[ CP_PURCHASE_SCIENCE ] )
	{
		m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
	}
//	if (!TheGlobalData->m_animateWindows)
//		{
//			if( m_contextParent[ CP_PURCHASE_SCIENCE ] )
//			{
//				m_contextParent[ CP_PURCHASE_SCIENCE ]->winHide( TRUE );
//			}
//		}
//		else
//		{
//			//if (m_generalsScreenAnimate->isFinished())
//			if(TheTransitionHandler->isFinished())
//				TheTransitionHandler->reverse("GenExpFade");
//				//m_generalsScreenAnimate->reverseAnimateWindow();
//		}
}

void ControlBar::togglePurchaseScience()
{
	if(m_contextParent[ CP_PURCHASE_SCIENCE ]->winIsHidden())
		showPurchaseScience();
	else
		hidePurchaseScience();
}

void ControlBar::toggleControlBarStage()
{
	if(m_currentControlBarStage == CONTROL_BAR_STAGE_DEFAULT )
		switchControlBarStage(CONTROL_BAR_STAGE_LOW);
	else
		switchControlBarStage(CONTROL_BAR_STAGE_DEFAULT);
}

// Functions for repositioning/resizing the control bar
void ControlBar::switchControlBarStage( ControlBarStages stage )
{
	if(stage < CONTROL_BAR_STAGE_DEFAULT || stage >= MAX_CONTROL_BAR_STAGES)
		return;
	switch (stage) {
	case CONTROL_BAR_STAGE_DEFAULT:
		setDefaultControlBarConfig();
		break;
//	case CONTROL_BAR_STAGE_SQUISHED:
//		setSquishedControlBarConfig();
//		break;
	case CONTROL_BAR_STAGE_LOW:
		setLowControlBarConfig();
		break;
	case CONTROL_BAR_STAGE_HIDDEN:
		setHiddenControlBar();
		break;
	default:
		DEBUG_CRASH(("ControlBar::switchControlBarStage we were passed in a stage that's not supported %d", stage));
	}

}
void ControlBar::setDefaultControlBarConfig()
{
//	if(m_currentControlBarStage == CONTROL_BAR_STAGE_SQUISHED)
//	{
//		m_controlBarResizer->sizeWindowsDefault();
//		m_controlBarSchemeManager->setControlBarSchemeByPlayerTemplate(ThePlayerList->getLocalPlayer()->getPlayerTemplate(), FALSE);
//	}
	m_currentControlBarStage = CONTROL_BAR_STAGE_DEFAULT;
	setScaledViewportHeight();
	m_contextParent[ CP_MASTER ]->winSetPosition(m_defaultControlBarPosition.x, m_defaultControlBarPosition.y);
	m_contextParent[ CP_MASTER ]->winHide(FALSE);
	repopulateBuildTooltipLayout();
	setUpDownImages();

}

void ControlBar::setSquishedControlBarConfig()
{
	if(m_currentControlBarStage == CONTROL_BAR_STAGE_SQUISHED)
		return;
	m_currentControlBarStage = CONTROL_BAR_STAGE_SQUISHED;
	m_contextParent[ CP_MASTER ]->winSetPosition(m_defaultControlBarPosition.x, m_defaultControlBarPosition.y);

//	m_controlBarResizer->sizeWindowsAlt();
	repopulateBuildTooltipLayout();
	setFullViewportHeight();
	m_controlBarSchemeManager->setControlBarSchemeByPlayerTemplate(ThePlayerList->getLocalPlayer()->getPlayerTemplate(), TRUE);
}

void ControlBar::setLowControlBarConfig()
{
//	if(m_currentControlBarStage == CONTROL_BAR_STAGE_SQUISHED)
//	{
//		m_controlBarResizer->sizeWindowsDefault();
//		m_controlBarSchemeManager->setControlBarSchemeByPlayerTemplate(ThePlayerList->getLocalPlayer()->getPlayerTemplate(), FALSE);
//	}

	m_currentControlBarStage = CONTROL_BAR_STAGE_LOW;
	ICoord2D pos;
	pos.x = m_defaultControlBarPosition.x;
	pos.y = TheDisplay->getHeight() - .1 * TheDisplay->getHeight();
	setFullViewportHeight();
	m_contextParent[ CP_MASTER ]->winSetPosition(pos.x, pos.y);
	m_contextParent[ CP_MASTER ]->winHide(FALSE);
	setUpDownImages();

}

void ControlBar::setHiddenControlBar()
{
	m_currentControlBarStage = CONTROL_BAR_STAGE_HIDDEN;
	m_contextParent[ CP_MASTER ]->winHide(TRUE);
}
// removed from multiplayer test
//void ControlBar::showCommandMarkers()
//{
//	for(Int i =0; i < MAX_COMMANDS_PER_SET; ++i)
//	{
//		if(m_commandWindows[i]->winIsHidden())
//			m_commandMarkers[i]->winHide(FALSE);
//		else
//			m_commandMarkers[i]->winHide(TRUE);
//	}
//}
//
void ControlBar::updateCommandMarkerImage( const Image *image )
{
	// removed from multiplayer branch

//	// we don't mind if the image is null, that way we can not draw anything
	//	for(Int i =0; i < MAX_COMMANDS_PER_SET; ++i)
	//	{
	//		m_commandMarkers[i]->winSetEnabledImage(0, image);
	//	}

}
void ControlBar::updateSlotExitImage( const Image *image )
{
	//Hardcoding values here Not a good thing but there's no other way right now.
	if(!image)
		return;

	//Kris:
	//Other than this being a completely ridiculously retarded idea, I'm not inclined
	//to recode this in a better way, yikes! Btw, I DID NOT CODE THIS! But this is
	//what this does: The button images are overridden by a faction specific icon.
	//The proper way to fix this would be to make a commandbutton option and loop
	//through all buttons on init to replace the icon. We need a system like this
	//for neutral buildings which can have a different empty inventory icon based
	//on the faction player.

	CommandButton *cmdButton = findNonConstCommandButton( "Command_StructureExit" );
	if(cmdButton)
		cmdButton->setButtonImage(image);

	cmdButton = findNonConstCommandButton( "Command_TransportExit" );
	if(cmdButton)
		cmdButton->setButtonImage(image);

	cmdButton = findNonConstCommandButton( "Command_BunkerExit" );
	if(cmdButton)
		cmdButton->setButtonImage(image);

	cmdButton = findNonConstCommandButton( "Command_FireBaseExit" );
	if(cmdButton)
		cmdButton->setButtonImage(image);

}

void ControlBar::updateUpDownImages( const Image *toggleButtonUpIn, const Image *toggleButtonUpOn, const Image *toggleButtonUpPushed,
																		 const Image *toggleButtonDownIn, const Image *toggleButtonDownOn, const Image *toggleButtonDownPushed,
																		 const Image *generalButtonEnable, const Image *generalButtonHighlight  )
{
	m_toggleButtonUpIn = toggleButtonUpIn;
	m_toggleButtonUpOn = toggleButtonUpOn;
	m_toggleButtonUpPushed = toggleButtonUpPushed;
	m_toggleButtonDownIn = toggleButtonDownIn;
	m_toggleButtonDownOn = toggleButtonDownOn;
	m_toggleButtonDownPushed = toggleButtonDownPushed;


	m_generalButtonEnable = generalButtonEnable;
	m_generalButtonHighlight = generalButtonHighlight;

	setUpDownImages();
}

void ControlBar::setUpDownImages()
{
	GameWindow *win= TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( "ControlBar.wnd:ButtonLarge" ) );
	if(!win)
		return;
	// we only care if it's in it's low state, else we put the default images up
	if(m_currentControlBarStage == CONTROL_BAR_STAGE_LOW)
	{
		GadgetButtonSetEnabledImage(win, m_toggleButtonUpOn);
		GadgetButtonSetHiliteImage(win, m_toggleButtonUpIn);
		GadgetButtonSetHiliteSelectedImage(win, m_toggleButtonUpPushed);
		return;
	}

	GadgetButtonSetEnabledImage(win, m_toggleButtonDownOn);
	GadgetButtonSetHiliteImage(win, m_toggleButtonDownIn);
	GadgetButtonSetHiliteSelectedImage(win, m_toggleButtonDownPushed);

}

void ControlBar::getForegroundMarkerPos(Int *x, Int *y)
{
	*x = m_controlBarForegroundMarkerPos.x;
	*y = m_controlBarForegroundMarkerPos.y;
}
void ControlBar::getBackgroundMarkerPos(Int *x, Int *y)
{
	*x = m_controlBarBackgroundMarkerPos.x;
	*y = m_controlBarBackgroundMarkerPos.y;
}

void ControlBar::drawTransitionHandler()
{
//	if(m_transitionHandler)
//		m_transitionHandler->draw();
}
enum{
	RADAR_ATTACK_GLOW_FRAMES = 150,
	RADAR_ATTACK_GLOW_NUM_TIMES = 15  ///< number of times we'll flash
};

void ControlBar::triggerRadarAttackGlow()
{
	if(!m_radarAttackGlowWindow)
		return;
	m_radarAttackGlowOn = TRUE;
	m_remainingRadarAttackGlowFrames = RADAR_ATTACK_GLOW_FRAMES;
	if(BitIsSet(m_radarAttackGlowWindow->winGetStatus(),WIN_STATUS_ENABLED) == TRUE)
		m_radarAttackGlowWindow->winEnable(FALSE);
}

void ControlBar::updateRadarAttackGlow ()
{
	if(!m_radarAttackGlowOn || !m_radarAttackGlowWindow)
		return;
	m_remainingRadarAttackGlowFrames--;
	if(m_remainingRadarAttackGlowFrames <= 0)
	{
		m_radarAttackGlowOn = FALSE;
		m_radarAttackGlowWindow->winEnable(TRUE);
		return;
	}

	if(m_remainingRadarAttackGlowFrames % RADAR_ATTACK_GLOW_NUM_TIMES == 0)
	{
		m_radarAttackGlowWindow->winEnable(!BitIsSet(m_radarAttackGlowWindow->winGetStatus(),WIN_STATUS_ENABLED));
	}


}
void ControlBar::initSpecialPowershortcutBar( Player *player)
{
	Int i = 0;
	for( ; i < MAX_SPECIAL_POWER_SHORTCUTS; ++i )
	{
		m_specialPowerShortcutButtonParents[i] = nullptr;
		m_specialPowerShortcutButtons[i] = nullptr;
	}

	if(m_specialPowerLayout)
	{
		m_specialPowerLayout->destroyWindows();
		deleteInstance(m_specialPowerLayout);
		m_specialPowerLayout = nullptr;
	}
	m_specialPowerShortcutParent = nullptr;
	m_currentlyUsedSpecialPowersButtons = 0;
	const PlayerTemplate *pt = player->getPlayerTemplate();

	if(!player || !pt|| !player->isLocalPlayer()
			|| pt->getSpecialPowerShortcutButtonCount() == 0
			|| pt->getSpecialPowerShortcutWinName().isEmpty()
			|| !player->isPlayerActive())
		return;
	m_currentlyUsedSpecialPowersButtons = pt->getSpecialPowerShortcutButtonCount();
	AsciiString layoutName, tempName, windowName, parentName;
	layoutName = pt->getSpecialPowerShortcutWinName();
	m_specialPowerLayout = TheWindowManager->winCreateLayout(layoutName);
	m_specialPowerLayout->hide(TRUE);

	tempName = layoutName;
	tempName.concat(":GenPowersShortcutBarParent");
	NameKeyType id = TheNameKeyGenerator->nameToKey( tempName );
	m_specialPowerShortcutParent = TheWindowManager->winGetWindowFromId( nullptr, id );//m_scienceLayout->getFirstWindow();

	tempName = layoutName;
	tempName.concat(":ButtonCommand%d");
	parentName = layoutName;
	parentName.concat(":ButtonParent%d");
	m_currentlyUsedSpecialPowersButtons = MIN(pt->getSpecialPowerShortcutButtonCount(), MAX_SPECIAL_POWER_SHORTCUTS);
	for( i = 0; i < MAX_SPECIAL_POWER_SHORTCUTS; i++ )
	{
		windowName.format( tempName, i+1 );
		id = TheNameKeyGenerator->nameToKey( windowName.str() );
		m_specialPowerShortcutButtons[ i ] =
			TheWindowManager->winGetWindowFromId( m_specialPowerShortcutParent, id );

		if (m_specialPowerShortcutButtons[ i ] != nullptr)
		{
			m_specialPowerShortcutButtons[ i ]->winSetStatus( WIN_STATUS_USE_OVERLAY_STATES );
			// Oh god... this is a total hack for shortcut buttons to handle rendering text top left corner...
			m_specialPowerShortcutButtons[ i ]->winSetStatus( WIN_STATUS_SHORTCUT_BUTTON );

			windowName.format( parentName, i+1 );
			id = TheNameKeyGenerator->nameToKey( windowName.str() );
			m_specialPowerShortcutButtonParents[ i ] =
				TheWindowManager->winGetWindowFromId( m_specialPowerShortcutParent, id );
		}
	}

}

void ControlBar::populateSpecialPowerShortcut( Player *player)
{
	const CommandSet *commandSet;
	Int i;
	if(!player || !player->getPlayerTemplate()
			|| !player->isLocalPlayer() || m_currentlyUsedSpecialPowersButtons == 0
			|| m_specialPowerShortcutButtons == nullptr || m_specialPowerShortcutButtonParents == nullptr)
		return;
	for( i = 0; i < MAX_SPECIAL_POWER_SHORTCUTS; ++i )
	{
		if (m_specialPowerShortcutButtons[i])
			m_specialPowerShortcutButtons[i]->winHide(TRUE);
		if (m_specialPowerShortcutButtonParents[i])
			m_specialPowerShortcutButtonParents[i]->winHide(TRUE);

	}

	// get command set
	if(player->getPlayerTemplate()->getSpecialPowerShortcutCommandSet().isEmpty() )
		return;
	commandSet = findCommandSet(player->getPlayerTemplate()->getSpecialPowerShortcutCommandSet()); // TEMP WILL CHANGE TO PROPER WAY ONCE WORKING
	if(!commandSet)
		return;
	// populate the button with commands defined
	Int currentButton = 0;
	const CommandButton *commandButton;
	for( i = 0; i < m_currentlyUsedSpecialPowersButtons; i++ )
	{

		// get command button
		commandButton = commandSet->getCommandButton(i);

		// if button is not present, just hide the window
		if( commandButton == nullptr )
		{
			continue;
			// hide window on interface
			//m_specialPowerShortcutButtons[ i ]->winHide( TRUE );

		}
		else
		{

			if( BitIsSet( commandButton->getOptions(), NEED_UPGRADE ) )
			{
				const UpgradeTemplate *upgrade = commandButton->getUpgradeTemplate();
				if( upgrade && !ThePlayerList->getLocalPlayer()->hasUpgradeComplete( upgrade->getUpgradeMask() ) )
				{
					//Kris: 8/13/03 - Don't show shortcut buttons that require upgrades we don't have. As far as
					//I know, only the radar van scan has this. The MOAB is handled differently (sciences).
					continue;
				}
			}

			//
			// commands that require sciences we don't have are hidden so they never show up
			// cause we can never pick "another" general technology throughout the game
			//
			if( BitIsSet( commandButton->getOptions(), NEED_SPECIAL_POWER_SCIENCE ) )
			{
				const SpecialPowerTemplate *power = commandButton->getSpecialPowerTemplate();

				if( !power )
				{
					//Should have the power.. button is probably missing the SpecialPower = xxx entry.
					DEBUG_CRASH( ("CommandButton %s needs a SpecialPower entry, but it's either incorrect or missing.", commandButton->getName().str()) );
					continue;
				}

				//We just need to find something that has the power.
				Object *obj = ThePlayerList->getLocalPlayer()->findMostReadyShortcutSpecialPowerOfType( commandButton->getSpecialPowerTemplate()->getSpecialPowerType() );
				if( !obj )
				{
					continue;
				}

				if( power->getRequiredScience() != SCIENCE_INVALID )
				{
					if( player->hasScience( power->getRequiredScience() ) == FALSE )
					{
						//Hide the power
						//m_specialPowerShortcutButtons[ i ]->winHide( TRUE );
						continue;
					}
					else
					{
						//The player does have the special power! Now determine if the images require
						//enhancement based on upgraded versions. This is determined by the command
						//button specifying a vector of sciences in the command button.
						Int bestIndex = -1;
						ScienceType science;
						for( size_t scienceIndex = 0; scienceIndex < commandButton->getScienceVec().size(); ++scienceIndex )
						{
							science = commandButton->getScienceVec()[ scienceIndex ];

							//Keep going until we reach the end or don't have the required science!
							if( player->hasScience( science ) )
							{
								bestIndex = scienceIndex;
							}
							else
							{
								break;
							}
						}

						if( bestIndex != -1 )
						{
							//Now get the best sciencetype.
							science = commandButton->getScienceVec()[ bestIndex ];

							const CommandSet *commandSet1;
							const CommandSet *commandSet3;
							const CommandSet *commandSet8;
							Int i;

							// get command set
							if( !player || !player->getPlayerTemplate()
									|| player->getPlayerTemplate()->getPurchaseScienceCommandSetRank1().isEmpty()
									|| player->getPlayerTemplate()->getPurchaseScienceCommandSetRank3().isEmpty()
									|| player->getPlayerTemplate()->getPurchaseScienceCommandSetRank8().isEmpty() )
							{
								continue;
							}
							commandSet1 = findCommandSet( player->getPlayerTemplate()->getPurchaseScienceCommandSetRank1() );
							commandSet3 = findCommandSet( player->getPlayerTemplate()->getPurchaseScienceCommandSetRank3() );
							commandSet8 = findCommandSet( player->getPlayerTemplate()->getPurchaseScienceCommandSetRank8() );

							if( !commandSet1 || !commandSet3 || !commandSet8 )
							{
								continue;
							}

							Bool found = FALSE;
							for( i = 0; !found && i < MAX_PURCHASE_SCIENCE_RANK_1; i++ )
							{
								const CommandButton *command = commandSet1->getCommandButton( i );
								if( command && command->getCommandType() == GUI_COMMAND_PURCHASE_SCIENCE )
								{
									//All purchase sciences specify a single science.
									if( command->getScienceVec().empty() )
									{
										DEBUG_CRASH( ("Commandbutton %s is a purchase science button without any science! Please add it.", command->getName().str() ) );
									}
									else if( command->getScienceVec()[0] == science )
									{
										commandButton->copyImagesFrom( command, TRUE );
										commandButton->copyButtonTextFrom( command, TRUE, TRUE );
										found = TRUE;
										break;
									}
								}
							}
							for( i = 0; !found && i < MAX_PURCHASE_SCIENCE_RANK_3; i++ )
							{
								const CommandButton *command = commandSet3->getCommandButton( i );
								if( command && command->getCommandType() == GUI_COMMAND_PURCHASE_SCIENCE )
								{
									//All purchase sciences specify a single science.
									if( command->getScienceVec().empty() )
									{
										DEBUG_CRASH( ("Commandbutton %s is a purchase science button without any science! Please add it.", command->getName().str() ) );
									}
									else if( command->getScienceVec()[0] == science )
									{
										commandButton->copyImagesFrom( command, TRUE );
										commandButton->copyButtonTextFrom( command, TRUE, TRUE );
										found = TRUE;
										break;
									}
								}
							}
							for( i = 0; !found && i < MAX_PURCHASE_SCIENCE_RANK_8; i++ )
							{
								const CommandButton *command = commandSet8->getCommandButton( i );
								if( command && command->getCommandType() == GUI_COMMAND_PURCHASE_SCIENCE )
								{
									//All purchase sciences specify a single science.
									if( command->getScienceVec().empty() )
									{
										DEBUG_CRASH( ("Commandbutton %s is a purchase science button without any science! Please add it.", command->getName().str() ) );
									}
									else if( command->getScienceVec()[0] == science )
									{
										commandButton->copyImagesFrom( command, TRUE );
										commandButton->copyButtonTextFrom( command, TRUE, TRUE );
										found = TRUE;
										break;
									}
								}
							}
						}
					}
				}
			}
			else if( commandButton->getCommandType() == GUI_COMMAND_SELECT_ALL_UNITS_OF_TYPE )
			{
				//Make sure we actually have an object of type that we want to be able to select.
				Object *obj = ThePlayerList->getLocalPlayer()->findAnyExistingObjectWithThingTemplate( commandButton->getThingTemplate() );
				if( !obj )
				{
					continue;
				}
			}

			DEBUG_ASSERTCRASH(m_specialPowerShortcutButtons[ currentButton ] != nullptr, ("m_specialPowerShortcutButtons[%d] is null", currentButton));
			DEBUG_ASSERTCRASH(m_specialPowerShortcutButtonParents[ currentButton ] != nullptr, ("m_specialPowerShortcutButtonParents[%d] is null", currentButton));

			// make sure the window is not hidden
			m_specialPowerShortcutButtons[ currentButton ]->winHide( FALSE );
			m_specialPowerShortcutButtonParents[ currentButton ]->winHide( FALSE );
			// enable by default
			m_specialPowerShortcutButtons[ currentButton ]->winEnable( TRUE );
			m_specialPowerShortcutButtonParents[ currentButton ]->winEnable( TRUE );

			// populate the visible button with data from the command button
			setControlCommand( m_specialPowerShortcutButtons[ currentButton ], commandButton );
			GadgetButtonSetAltSound(m_specialPowerShortcutButtons[ currentButton ], "GUIGenShortcutClick");
			currentButton++;

		}

	}
	if(m_contextParent[ CP_MASTER ] && !m_contextParent[ CP_MASTER ]->winIsHidden() && m_specialPowerShortcutParent->winIsHidden())
	{
		showSpecialPowerShortcut();
		animateSpecialPowerShortcut(TRUE);
	}
	updateSpecialPowerShortcut();
}

//-------------------------------------------------------------------------------------------------
Bool ControlBar::hasAnyShortcutSelection() const
{
	for( Int i = 0; i < m_currentlyUsedSpecialPowersButtons; i++ )
	{
		GameWindow *win;
		const CommandButton *command;

		win = m_specialPowerShortcutButtons[ i ];
		if( win->winIsHidden() == TRUE )
			continue;

		// get the command from the control
		command = (const CommandButton *)GadgetButtonGetData(win);
		if( !command )
			continue;

		if( command->getCommandType() == GUI_COMMAND_SELECT_ALL_UNITS_OF_TYPE )
		{
			//We found one, so we'll always show shortcuts!
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ControlBar::canShowSpecialPowerShortcut() const
{
#ifdef RTS_GENERALS
	// Special Powers in Generals do not have the ShortcutPower flag set and therefore this function
	// is satisfied with the presence of a Command Center, which is supposed to host Special Powers.
	if (ThePlayerList->getLocalPlayer()->findNaturalCommandCenter() != nullptr)
		return true;
#endif

	if (hasAnyShortcutSelection())
		return true;

	if (ThePlayerList->getLocalPlayer()->hasAnyShortcutSpecialPower())
		return true;

	return false;
}

//-------------------------------------------------------------------------------------------------
void ControlBar::updateSpecialPowerShortcut()
{
	if(!m_specialPowerShortcutParent || !m_specialPowerShortcutButtons
	   || !ThePlayerList || !ThePlayerList->getLocalPlayer())
		return;

	const Bool hasValidShortcutButton = canShowSpecialPowerShortcut();

	if( hasValidShortcutButton
		  && m_specialPowerShortcutParent->winIsHidden()
			&& m_contextParent[ CP_MASTER ]
			&& !m_contextParent[ CP_MASTER ]->winIsHidden() )
	{
		showSpecialPowerShortcut();
		animateSpecialPowerShortcut(TRUE);
	}
	else if( !hasValidShortcutButton
					 && !m_specialPowerShortcutParent->winIsHidden()
					 && m_animateWindowManagerForGenShortcuts->isFinished() )
	{
		animateSpecialPowerShortcut(FALSE);
	}

	if(m_specialPowerShortcutParent->winIsHidden())
		return;

	if(!ThePlayerList->getLocalPlayer()->isPlayerActive())
	{
		hideSpecialPowerShortcut();
		return;
	}
	if(m_contextParent[ CP_MASTER ] && !m_contextParent[ CP_MASTER ]->winIsHidden() && m_specialPowerShortcutParent->winIsHidden())
		showSpecialPowerShortcut();

	for( Int i = 0; i < m_currentlyUsedSpecialPowersButtons; i++ )
	{
		GameWindow *win;
		const CommandButton *command;
		// get the window
		win = m_specialPowerShortcutButtons[ i ];

		if( win->winIsHidden() == TRUE )
			continue;
		// get the command from the control
		command = (const CommandButton *)GadgetButtonGetData(win);
		//command = (const CommandButton *)win->winGetUserData();
		if( command == nullptr )
			continue;


		win->winClearStatus( WIN_STATUS_NOT_READY );
		win->winClearStatus( WIN_STATUS_ALWAYS_COLOR );


		// is the command available

		CommandAvailability availability = COMMAND_RESTRICTED;

		const SpecialPowerTemplate *spTemplate = command->getSpecialPowerTemplate();
		Object *obj = nullptr;
		if( spTemplate )
		{
			obj = ThePlayerList->getLocalPlayer()->findMostReadyShortcutSpecialPowerOfType( command->getSpecialPowerTemplate()->getSpecialPowerType() );
			availability = getCommandAvailability( command, obj, win );
		}
		else if( command->getCommandType() == GUI_COMMAND_SELECT_ALL_UNITS_OF_TYPE )
		{
			availability = COMMAND_HIDDEN;
			Object *obj = ThePlayerList->getLocalPlayer()->findAnyExistingObjectWithThingTemplate( command->getThingTemplate() );
			if( obj )
			{
				//Make command available if it isn't a special power template shortcut power.
				availability = COMMAND_AVAILABLE;

				UnsignedInt mostReadyPercentage;
				obj = ThePlayerList->getLocalPlayer()->findMostReadyShortcutSpecialPowerForThing( command->getThingTemplate(), mostReadyPercentage );
				if( obj )
				{
					//Ugh... hacky.
					//Look for a command button for a special power and if so, then get the command availability for it.
					const CommandSet *commandSet = findCommandSet( obj->getCommandSetString() );
					if( commandSet )
					{
						for( Int commandIndex = 0; commandIndex < MAX_COMMANDS_PER_SET; commandIndex++ )
						{
							const CommandButton *evalButton = commandSet->getCommandButton( commandIndex );
							GameWindow *evalButtonWin = m_commandWindows[ commandIndex ];
							if( evalButton && evalButton->getCommandType() == GUI_COMMAND_SPECIAL_POWER )
							{
								//We want to evaluate the special powerbutton... but apply the clock overlay to our button!
								availability = getCommandAvailability( evalButton, obj, evalButtonWin, win );
								break;
							}
						}
					}
				}
			}
		}

		// enable/disable the window control
		switch( availability )
		{
			case COMMAND_HIDDEN:
				win->winHide( TRUE );
				break;
			case COMMAND_RESTRICTED:
				win->winEnable( FALSE );
				break;
			case COMMAND_NOT_READY:
				win->winEnable( FALSE );
				win->winSetStatus( WIN_STATUS_NOT_READY );
				break;
			case COMMAND_CANT_AFFORD:
				win->winEnable( FALSE );
				win->winSetStatus( WIN_STATUS_ALWAYS_COLOR );
				break;
			default:
				win->winEnable( TRUE );
				break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ControlBar::drawSpecialPowerShortcutMultiplierText()
{
	for( Int i = 0; i < m_currentlyUsedSpecialPowersButtons; i++ )
	{
		GameWindow *win;
		const CommandButton *command;
		// get the window
		win = m_specialPowerShortcutButtons[ i ];

		if( win->winIsHidden() == TRUE )
			continue;
		// get the command from the control
		command = (const CommandButton *)GadgetButtonGetData(win);
		//command = (const CommandButton *)win->winGetUserData();
		if( command == nullptr )
			continue;

		//draw superweapon ready multipliers
		for( int i = 0; i < MAX_SPECIAL_POWER_SHORTCUTS; i++ )
		{
			if( !m_shortcutDisplayStrings[ i ] )
			{
				//m_shortcutDisplayStrings[ i ] = TheDisplayStringManager->newDisplayString();
				//m_shortcutDisplayStrings[ i ]->setFont( TheFontLibrary->getFont( "Arial", 16, false ) );
			}

			const SpecialPowerTemplate *spTemplate = command->getSpecialPowerTemplate();
			Int numReady = 0;
			if( spTemplate )
			{
				numReady = ThePlayerList->getLocalPlayer()->countReadyShortcutSpecialPowersOfType( spTemplate->getSpecialPowerType() );
			}
			if( numReady > 1 ) // Lorenzen changed... Displaying a "1" is superfluous
			{
				UnicodeString unibuffer;
				unibuffer.format( L"%d", numReady );

				GadgetButtonSetText( win, unibuffer );

				//m_shortcutDisplayStrings[ i ]->setText( unibuffer );
				//TheControlBar->m_shortcutDisplayStrings[ i ]->draw( 600, i * 40 + 40, GameMakeColor(255,255,255,255), GameMakeColor(0,0,0,0), 0, 0 );
			}
			else
			{
				UnicodeString unibuffer;
				GadgetButtonSetText( win, unibuffer );
				//TheDisplayStringManager->freeDisplayString( m_shortcutDisplayStrings[ i ] );
				//m_shortcutDisplayStrings[ i ] = nullptr;
			}
		}
	}
}

void ControlBar::animateSpecialPowerShortcut( Bool isOn )
{
	if(!m_specialPowerShortcutParent || !m_animateWindowManagerForGenShortcuts || !m_currentlyUsedSpecialPowersButtons)
		return;
	Bool dontAnimate = TRUE;
	for( Int i = 0; i < m_currentlyUsedSpecialPowersButtons; ++i )
	{
		if (m_specialPowerShortcutButtons[i]->winGetUserData())
		{
			dontAnimate = FALSE;
			break;
		}
	}
	if(dontAnimate)
		return;

	if(isOn)
	{
		m_animateWindowManagerForGenShortcuts->reset();
		m_animateWindowManagerForGenShortcuts->registerGameWindow(m_specialPowerShortcutParent,WIN_ANIMATION_SLIDE_RIGHT,TRUE,500,0);
	}
	else
	{
		m_animateWindowManagerForGenShortcuts->reverseAnimateWindow();
	}
}

void ControlBar::showSpecialPowerShortcut()
{
	if(TheScriptEngine->isGameEnding() || !m_specialPowerShortcutParent
		||!m_specialPowerShortcutButtons || !ThePlayerList || !ThePlayerList->getLocalPlayer())
		return;
	Bool dontAnimate = TRUE;
	for( Int i = 0; i < m_currentlyUsedSpecialPowersButtons; ++i )
	{
		if (m_specialPowerShortcutButtons[i]->winGetUserData())
		{
			dontAnimate = FALSE;
			break;
		}
	}
	if( dontAnimate || !canShowSpecialPowerShortcut() )
		return;
	m_specialPowerShortcutParent->winHide(FALSE);
	populateSpecialPowerShortcut(ThePlayerList->getLocalPlayer());

}

void ControlBar::hideSpecialPowerShortcut()
{
	if(!m_specialPowerShortcutParent)
		return;

	m_specialPowerShortcutParent->winHide(TRUE);

}

void ControlBar::setFullViewportHeight()
{
	TheTacticalView->setHeight(TheDisplay->getHeight());
}

void ControlBar::setScaledViewportHeight()
{
	TheTacticalView->setHeight(TheDisplay->getHeight() * TheGlobalData->m_viewportHeightScale);
}

// GeneralsX @bugfix w1semannn 07/06/2026 Fix tooltip height clipping with Unicode fonts (Issue #153)
GameFont *ControlBar::overrideTooltipGadgetFont( GameWindow *win )
{
	if( !win )
		return nullptr;

	// Use the unicode font name from language data, default to "Arial Unicode MS"
	AsciiString fontName = "Arial Unicode MS";
	if( TheGlobalLanguageData && TheGlobalLanguageData->m_unicodeFontName.isNotEmpty() )
		fontName = TheGlobalLanguageData->m_unicodeFontName;

	// Get the font from the font library (12pt, not bold)
	GameFont *newFont = TheFontLibrary->getFont( fontName, 12, FALSE );
	if( !newFont )
		return nullptr;

	// Set the font on the gadget (updates display strings for rendering)
	GadgetStaticTextSetFont( win, newFont );

	// Return the new font so callers can use it for measurement
	return newFont;
}
