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

///////////////////////////////////////////////////////////////////////////////////////
// FILE: LanLobbyMenu.cpp
// Author: Chris Huybregts, October 2001
// Description: Lan Lobby Menu
///////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Lib/BaseType.h"
#include "Common/crc.h"
#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/MultiplayerSettings.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/QuotedPrintable.h"
#include "Common/OptionPreferences.h"
#include "GameClient/AnimateWindowManager.h"
#include "GameClient/ClientInstance.h"
#include "GameClient/GameText.h"
#include "GameClient/MapUtil.h"
#include "GameClient/Mouse.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Gadget.h"
#include "GameClient/Shell.h"
#include "GameClient/ShellHooks.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/GameInfoWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/MessageBox.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/IPEnumeration.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "GameNetwork/LANGameInfo.h"

Bool LANisShuttingDown = false;
Bool LANbuttonPushed = false;
Bool LANSocketErrorDetected = FALSE;
char *LANnextScreen = nullptr;

static Int	initialGadgetDelay = 2;
static Bool justEntered = FALSE;



LANPreferences::LANPreferences()
{
	loadFromIniFile();
}

LANPreferences::~LANPreferences()
{
}

Bool LANPreferences::loadFromIniFile()
{
	if (rts::ClientInstance::getInstanceId() > 1u)
	{
		AsciiString fname;
		fname.format("Network_Instance%.2u.ini", rts::ClientInstance::getInstanceId());
		return load(fname);
	}

	return load("Network.ini");
}

UnicodeString LANPreferences::getUserName()
{
	UnicodeString ret;

	LANPreferences::const_iterator it = find("UserName");
	if (it != end())
	{
		// Found an user name. Use it if valid.
		ret = QuotedPrintableToUnicodeString(it->second);
		ret.trim();
		if (!ret.isEmpty())
		{
			return ret;
		}
	}

	if (rts::ClientInstance::getInstanceId() > 1u)
	{
		// TheSuperHackers @feature Use the instance id as default user name
		// to avoid duplicate names for multiple client instances.
		ret.format(L"Instance%.2d", rts::ClientInstance::getInstanceId());
		return ret;
	}

	// Use machine name as default user name.
	IPEnumeration IPs;
	ret.translate(IPs.getMachineName());
	return ret;
}

Int LANPreferences::getPreferredColor()
{
	Int ret;
	LANPreferences::const_iterator it = find("Color");
	if (it == end())
	{
		return -1;
	}

	ret = atoi(it->second.str());
	if (ret < -1 || ret >= TheMultiplayerSettings->getNumColors())
		ret = -1;

	return ret;
}

Int LANPreferences::getPreferredFaction()
{
	Int ret;
	LANPreferences::const_iterator it = find("PlayerTemplate");
	if (it == end())
	{
		return PLAYERTEMPLATE_RANDOM;
	}

	ret = atoi(it->second.str());
	if (ret == PLAYERTEMPLATE_OBSERVER || ret < PLAYERTEMPLATE_MIN || ret >= ThePlayerTemplateStore->getPlayerTemplateCount())
		ret = PLAYERTEMPLATE_RANDOM;

	if (ret >= 0)
	{
		const PlayerTemplate *fac = ThePlayerTemplateStore->getNthPlayerTemplate(ret);
		if (!fac)
			ret = PLAYERTEMPLATE_RANDOM;
		else if (fac->getStartingBuilding().isEmpty())
			ret = PLAYERTEMPLATE_RANDOM;
	}

	return ret;
}

Bool LANPreferences::usesSystemMapDir()
{
	OptionPreferences::const_iterator it = find("UseSystemMapDir");
	if (it == end())
		return TRUE;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

AsciiString LANPreferences::getPreferredMap()
{
	AsciiString ret;
	LANPreferences::const_iterator it = find("Map");
	if (it == end())
	{
		ret = getDefaultMap(TRUE);
		return ret;
	}

	ret = QuotedPrintableToAsciiString(it->second);
	ret.trim();
	if (ret.isEmpty() || !isValidMap(ret, TRUE))
	{
		ret = getDefaultMap(TRUE);
		return ret;
	}

	return ret;
}

Int LANPreferences::getNumRemoteIPs()
{
	Int ret;
	LANPreferences::const_iterator it = find("NumRemoteIPs");
	if (it == end())
	{
		ret = 0;
		return ret;
	}

	ret = atoi(it->second.str());
	return ret;
}

UnicodeString LANPreferences::getRemoteIPEntry(Int i)
{
	UnicodeString ret;
	AsciiString key;
	key.format("RemoteIP%d", i);

	AsciiString ipstr;
	AsciiString asciientry;

	LANPreferences::const_iterator it = find(key.str());
	if (it == end())
	{
		asciientry = "";
		return ret;
	}

	asciientry = it->second;

	asciientry.nextToken(&ipstr, ":");
	asciientry.set(asciientry.str() + 1); // skip the ':'

	ret.translate(ipstr);
	if (!asciientry.isEmpty())
	{
		ret.concat(L"(");
		ret.concat(QuotedPrintableToUnicodeString(asciientry));
		ret.concat(L")");
	}

	return ret;
}

static const char superweaponRestrictionKey[] = "SuperweaponRestrict";

Bool LANPreferences::getSuperweaponRestricted() const
{
  LANPreferences::const_iterator it = find(superweaponRestrictionKey);
  if (it == end())
  {
    return false;
  }

  return ( it->second.compareNoCase( "yes" ) == 0 );
}

void LANPreferences::setSuperweaponRestricted( Bool superweaponRestricted )
{
  (*this)[superweaponRestrictionKey] = superweaponRestricted ? "Yes" : "No";
}

static const char startingCashKey[] = "StartingCash";
Money LANPreferences::getStartingCash() const
{
  LANPreferences::const_iterator it = find(startingCashKey);
  if (it == end())
  {
    return TheMultiplayerSettings->getDefaultStartingMoney();
  }

  Money money;
  money.deposit( strtoul( it->second.str(), nullptr, 10 ), FALSE, FALSE );

  return money;
}

void LANPreferences::setStartingCash( const Money & startingCash )
{
  AsciiString option;

  option.format( "%d", startingCash.countMoney() );

  (*this)[startingCashKey] = option;
}

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////


// window ids ------------------------------------------------------------------------------
static NameKeyType parentLanLobbyID = NAMEKEY_INVALID;
static NameKeyType buttonBackID = NAMEKEY_INVALID;
static NameKeyType buttonClearID = NAMEKEY_INVALID;
static NameKeyType buttonHostID = NAMEKEY_INVALID;
static NameKeyType buttonJoinID = NAMEKEY_INVALID;
static NameKeyType buttonDirectConnectID = NAMEKEY_INVALID;
static NameKeyType buttonEmoteID = NAMEKEY_INVALID;
static NameKeyType staticToolTipID = NAMEKEY_INVALID;
static NameKeyType textEntryPlayerNameID = NAMEKEY_INVALID;
static NameKeyType textEntryChatID = NAMEKEY_INVALID;
static NameKeyType listboxPlayersID = NAMEKEY_INVALID;
static NameKeyType staticTextGameInfoID = NAMEKEY_INVALID;


// Window Pointers ------------------------------------------------------------------------
static GameWindow *parentLanLobby = nullptr;
static GameWindow *buttonBack = nullptr;
static GameWindow *buttonClear = nullptr;
static GameWindow *buttonHost = nullptr;
static GameWindow *buttonJoin = nullptr;
static GameWindow *buttonDirectConnect = nullptr;
static GameWindow *buttonEmote = nullptr;
static GameWindow *staticToolTip = nullptr;
static GameWindow *textEntryPlayerName = nullptr;
static GameWindow *textEntryChat = nullptr;
static GameWindow *staticTextGameInfo = nullptr;

//external declarations of the Gadgets the callbacks can use
NameKeyType listboxChatWindowID = NAMEKEY_INVALID;
GameWindow *listboxChatWindow = nullptr;
GameWindow *listboxPlayers = nullptr;
NameKeyType listboxGamesID = NAMEKEY_INVALID;
GameWindow *listboxGames = nullptr;

// hack to disable framerate limiter in LAN games
//static Bool shellmapOn;
static Bool useFpsLimit;
static UnicodeString defaultName;

static void playerTooltip(GameWindow *window,
													WinInstanceData *instData,
													UnsignedInt mouse)
{
	Int x, y, row, col;
	x = LOLONGTOSHORT(mouse);
	y = HILONGTOSHORT(mouse);

	GadgetListBoxGetEntryBasedOnXY(window, x, y, row, col);

	if (row == -1 || col == -1)
	{
		//TheMouse->setCursorTooltip( TheGameText->fetch("TOOLTIP:LobbyPlayers") );
		return;
	}

	// GeneralsX @bugfix BenderAI 12/02/2026 - Cast via uintptr_t for 64-bit compatibility
	UnsignedInt playerIP = static_cast<UnsignedInt>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData( window, row, col )));
	LANPlayer *player = TheLAN->LookupPlayer(playerIP);
	if (!player)
	{
		DEBUG_CRASH(("No player info in listbox!"));
		//TheMouse->setCursorTooltip( TheGameText->fetch("TOOLTIP:LobbyPlayers") );
		return;
	}

	setLANPlayerTooltip(player);
}


//-------------------------------------------------------------------------------------------------
/** Initialize the Lan Lobby Menu */
//-------------------------------------------------------------------------------------------------
void LanLobbyMenuInit( WindowLayout *layout, void *userData )
{
	LANnextScreen = nullptr;
	LANbuttonPushed = false;
	LANisShuttingDown = false;

	// get the ids for our controls
	parentLanLobbyID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:LanLobbyMenuParent" );
	buttonBackID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ButtonBack" );
	buttonClearID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ButtonClear" );
	buttonHostID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ButtonHost" );
	buttonJoinID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ButtonJoin" );
	buttonDirectConnectID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ButtonDirectConnect" );
	buttonEmoteID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ButtonEmote" );
	staticToolTipID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:StaticToolTip" );
	textEntryPlayerNameID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:TextEntryPlayerName" );
	textEntryChatID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:TextEntryChat" );
	listboxPlayersID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ListboxPlayers" );
	listboxChatWindowID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ListboxChatWindowLanLobby" );
	listboxGamesID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:ListboxGames" );
	staticTextGameInfoID = TheNameKeyGenerator->nameToKey( "LanLobbyMenu.wnd:StaticTextGameInfo" );


	// Get pointers to the window buttons
	parentLanLobby = TheWindowManager->winGetWindowFromId( nullptr, parentLanLobbyID );
	buttonBack = TheWindowManager->winGetWindowFromId( nullptr,  buttonBackID);
	buttonClear = TheWindowManager->winGetWindowFromId( nullptr,  buttonClearID);
	buttonHost = TheWindowManager->winGetWindowFromId( nullptr, buttonHostID );
	buttonJoin = TheWindowManager->winGetWindowFromId( nullptr, buttonJoinID );
	buttonDirectConnect = TheWindowManager->winGetWindowFromId( nullptr, buttonDirectConnectID );
	buttonEmote = TheWindowManager->winGetWindowFromId( nullptr,buttonEmoteID  );
	staticToolTip = TheWindowManager->winGetWindowFromId( nullptr, staticToolTipID );
	textEntryPlayerName = TheWindowManager->winGetWindowFromId( nullptr, textEntryPlayerNameID );
	textEntryChat = TheWindowManager->winGetWindowFromId( nullptr, textEntryChatID );
	listboxPlayers = TheWindowManager->winGetWindowFromId( nullptr, listboxPlayersID );
	listboxChatWindow = TheWindowManager->winGetWindowFromId( nullptr, listboxChatWindowID );
	listboxGames = TheWindowManager->winGetWindowFromId( nullptr, listboxGamesID );
	staticTextGameInfo = TheWindowManager->winGetWindowFromId( nullptr, staticTextGameInfoID );
	listboxPlayers->winSetTooltipFunc(playerTooltip);

	// Show Menu
	layout->hide( FALSE );

	// Init LAN API Singleton
	if (!TheLAN)
	{
		TheLAN = NEW LANAPI();	/// @todo clh delete TheLAN and
		useFpsLimit = TheGlobalData->m_useFpsLimit;
	}
	else
	{
		TheWritableGlobalData->m_useFpsLimit = useFpsLimit;
		TheLAN->reset();
	}

	// Choose an IP address, then initialize the LAN singleton
	OptionPreferences optionPrefs;
	UnsignedInt preferredLANIP = optionPrefs.getLANIPAddress();
	UnsignedInt IP = preferredLANIP ? preferredLANIP : TheGlobalData->m_defaultIP;
	IPEnumeration IPs;
	EnumeratedIP *IPlist = IPs.getAddresses();
	const WideChar* IPSource;
	Bool foundPreferredIP = FALSE;

	for (EnumeratedIP *candidate = IPlist; candidate != nullptr; candidate = candidate->getNext())
	{
		if (candidate->getIP() == IP)
		{
			foundPreferredIP = TRUE;
			break;
		}
	}

	if (IP != 0 && foundPreferredIP)
	{
		IPSource = (preferredLANIP == IP) ? L"Options LAN IP" : L"Global default LAN IP";
		// GeneralsX @build GitHubCopilot 12/04/2026 Make LAN lobby explicitly honor configured LAN IP preference when it is still valid.
		DEBUG_LOG(("LanLobbyMenuInit - using preferred LAN IP %d.%d.%d.%d from %ls",
			PRINTF_IP_AS_4_INTS(IP), IPSource));
	}
	else
	{
		/*
		while (IPlist && IPlist->getNext())
		{
			IPlist = IPlist->getNext();
		}
		*/
		DEBUG_ASSERTCRASH(IPlist, ("No IP addresses found!"));
		if (!IPlist)
		{
			/// @todo: display error and exit lan lobby if no IPs are found
		}

		IPSource = L"Enumerated LAN IP fallback";
		IP = IPlist->getIP();
		// GeneralsX @build GitHubCopilot 11/04/2026 Log auto-selected LAN IP to diagnose cross-platform discovery failures.
		DEBUG_LOG(("LanLobbyMenuInit - auto-selected LAN IP %d.%d.%d.%d from enumeration",
			PRINTF_IP_AS_4_INTS(IP)));
	}

#if defined(RTS_DEBUG)
	UnicodeString str;
	str.format(L"%s: %d.%d.%d.%d", IPSource, PRINTF_IP_AS_4_INTS(IP));
	GadgetListBoxAddEntryText(listboxChatWindow, str, chatSystemColor, -1, 0);
#endif

	// TheLAN->init() sets us to be in a LAN menu screen automatically.
	TheLAN->init();
	// GeneralsX @build GitHubCopilot 11/04/2026 Log LAN bind attempt from lobby startup path.
	DEBUG_LOG(("LanLobbyMenuInit - calling SetLocalIP(%d.%d.%d.%d)", PRINTF_IP_AS_4_INTS(IP)));
	if (TheLAN->SetLocalIP(IP) == FALSE) {
		LANSocketErrorDetected = TRUE;
		// GeneralsX @build GitHubCopilot 11/04/2026 Explicit failure breadcrumb for LAN socket initialization.
		DEBUG_LOG(("LanLobbyMenuInit - SetLocalIP failed for %d.%d.%d.%d", PRINTF_IP_AS_4_INTS(IP)));
	}
	else
	{
		// GeneralsX @build GitHubCopilot 11/04/2026 Explicit success breadcrumb for LAN socket initialization.
		DEBUG_LOG(("LanLobbyMenuInit - SetLocalIP succeeded for %d.%d.%d.%d", PRINTF_IP_AS_4_INTS(IP)));
	}

	//Initialize the gadgets on the window
	//UnicodeString	txtInput;
	//txtInput.translate(IPs.getMachineName());
	LANPreferences prefs;
	defaultName = prefs.getUserName();
	defaultName.truncateTo(g_lanPlayerNameLength);

	GadgetTextEntrySetText( textEntryPlayerName, defaultName);
	// Clear the text entry line
	GadgetTextEntrySetText(textEntryChat, UnicodeString::TheEmptyString);

	GadgetListBoxReset(listboxPlayers);
	GadgetListBoxReset(listboxGames);

	defaultName.truncateTo(g_lanPlayerNameLength);
	TheLAN->RequestSetName(defaultName);
	// GeneralsX @build GitHubCopilot 11/04/2026 Trace initial LAN discovery request from menu bootstrap.
	DEBUG_LOG(("LanLobbyMenuInit - issuing initial RequestLocations() from LAN lobby"));
	TheLAN->RequestLocations();

	/*
	UnicodeString unicodeChat;

	unicodeChat = L"Local IP list:";
	GadgetListBoxAddEntryText(listboxChatWindow, unicodeChat, chatSystemColor, -1, 0);

	IPlist = IPs.getAddresses();
	while (IPlist)
	{
		unicodeChat.translate(IPlist->getIPstring());
		GadgetListBoxAddEntryText(listboxChatWindow, unicodeChat, chatSystemColor, -1, 0);
		IPlist = IPlist->getNext();
	}
	*/

	// Set Keyboard to Main Parent
	//TheWindowManager->winSetFocus( parentLanLobby );
	TheWindowManager->winSetFocus( textEntryChat );
	CreateLANGameInfoWindow(staticTextGameInfo);

	//TheShell->showShellMap(FALSE);
	//shellmapOn = FALSE;
	// coming out of a game, re-load the shell map
	TheShell->showShellMap(TRUE);

	// check for MOTD
	TheLAN->checkMOTD();
	layout->hide(FALSE);
	layout->bringForward();

	justEntered = TRUE;
	initialGadgetDelay = 2;
	GameWindow *win = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("LanLobbyMenu.wnd:GadgetParent"));
	if(win)
		win->winHide(TRUE);


	// animate controls
	//TheShell->registerWithAnimateManager(parentLanLobby, WIN_ANIMATION_SLIDE_TOP, TRUE);
//	TheShell->registerWithAnimateManager(buttonHost, WIN_ANIMATION_SLIDE_LEFT, TRUE, 600);
//	TheShell->registerWithAnimateManager(buttonJoin, WIN_ANIMATION_SLIDE_LEFT, TRUE, 400);
//	TheShell->registerWithAnimateManager(buttonDirectConnect, WIN_ANIMATION_SLIDE_LEFT, TRUE, 200);
//	//TheShell->registerWithAnimateManager(buttonOptions, WIN_ANIMATION_SLIDE_LEFT, TRUE, 1);
//	TheShell->registerWithAnimateManager(buttonBack, WIN_ANIMATION_SLIDE_RIGHT, TRUE, 1);

}

//-------------------------------------------------------------------------------------------------
/** This is called when a shutdown is complete for this menu */
//-------------------------------------------------------------------------------------------------
static void shutdownComplete( WindowLayout *layout )
{

	LANisShuttingDown = false;

	// hide the layout
	layout->hide( TRUE );

	// our shutdown is complete
	TheShell->shutdownComplete( layout, (LANnextScreen != nullptr) );

	if (LANnextScreen != nullptr)
	{
		TheShell->push(LANnextScreen);
	}

	LANnextScreen = nullptr;

}

//-------------------------------------------------------------------------------------------------
/** Lan Lobby menu shutdown method */
//-------------------------------------------------------------------------------------------------
void LanLobbyMenuShutdown( WindowLayout *layout, void *userData )
{
	LANPreferences prefs;
	prefs["UserName"] = UnicodeStringToQuotedPrintable(GadgetTextEntryGetText( textEntryPlayerName ));
	prefs.write();

	DestroyGameInfoWindow();
	// hide menu
	//layout->hide( TRUE );

	TheLAN->RequestLobbyLeave( true );

	// Reset the LAN singleton
	//TheLAN->reset();

	// our shutdown is complete
	//TheShell->shutdownComplete( layout );
	TheWritableGlobalData->m_useFpsLimit = useFpsLimit;

	LANisShuttingDown = true;

	// if we are shutting down for an immediate pop, skip the animations
	Bool popImmediate = *(Bool *)userData;

	LANSocketErrorDetected = FALSE;

	if( popImmediate )
	{

		shutdownComplete( layout );
		return;

	}

	TheShell->reverseAnimatewindow();
	TheTransitionHandler->reverse("LanLobbyFade");
	//if(	shellmapOn)
//		TheShell->showShellMap(TRUE);
}


//-------------------------------------------------------------------------------------------------
/** Lan Lobby menu update method */
//-------------------------------------------------------------------------------------------------
void LanLobbyMenuUpdate( WindowLayout * layout, void *userData)
{
	if (TheGameLogic->isInShellGame() && TheGameLogic->getFrame() == 1)
	{
		SignalUIInteraction(SHELL_SCRIPT_HOOK_LAN_ENTERED_FROM_GAME);
	}

	if(justEntered)
	{
		if(initialGadgetDelay == 1)
		{
			TheTransitionHandler->setGroup("LanLobbyFade");
			initialGadgetDelay = 2;
			justEntered = FALSE;
		}
		else
			initialGadgetDelay--;
	}

	if(LANisShuttingDown && TheShell->isAnimFinished() && TheTransitionHandler->isFinished())
		shutdownComplete(layout);

	if (TheShell->isAnimFinished() && !LANbuttonPushed && TheLAN)
		TheLAN->update();

	if (LANSocketErrorDetected == TRUE) {
		LANSocketErrorDetected = FALSE;
		DEBUG_LOG(("SOCKET ERROR!  BAILING!"));
		MessageBoxOk(TheGameText->fetch("GUI:NetworkError"), TheGameText->fetch("GUI:SocketError"), nullptr);

		// we have a socket problem, back out to the main menu.
		TheWindowManager->winSendSystemMsg(buttonBack->winGetParent(), GBM_SELECTED,
																			 (WindowMsgData)buttonBack, buttonBackID);
	}


}

//-------------------------------------------------------------------------------------------------
/** Lan Lobby menu input callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType LanLobbyMenuInput( GameWindow *window, UnsignedInt msg,
																			 WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
			UnsignedByte state = mData2;
			if (LANbuttonPushed)
				break;

			switch( key )
			{

				// ----------------------------------------------------------------------------------------
				case KEY_ESC:
				{

					//
					// send a simulated selected event to the parent window of the
					// back/exit button
					//
					if( BitIsSet( state, KEY_STATE_UP ) )
					{
						TheWindowManager->winSendSystemMsg( window, GBM_SELECTED,
																							(WindowMsgData)buttonBack, buttonBackID );

					}

					// don't let key fall through anywhere else
					return MSG_HANDLED;

				}

			}

		}

	}

	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
/** Lan Lobby menu window system callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType LanLobbyMenuSystem( GameWindow *window, UnsignedInt msg,
														 WindowMsgData mData1, WindowMsgData mData2 )
{
	UnicodeString txtInput;

	switch( msg )
	{


		case GWM_CREATE:
			{
				SignalUIInteraction(SHELL_SCRIPT_HOOK_LAN_OPENED);
				break;
			}

		case GWM_DESTROY:
			{
				SignalUIInteraction(SHELL_SCRIPT_HOOK_LAN_CLOSED);
				break;
			}

		case GWM_INPUT_FOCUS:
			{
				// if we're givin the opportunity to take the keyboard focus we must say we want it
				if( mData1 == TRUE )
					*(Bool *)mData2 = TRUE;

				return MSG_HANDLED;
			}
		case GLM_DOUBLE_CLICKED:
			{
				if (LANbuttonPushed)
					break;
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();
				if( controlID == listboxGamesID )
				{
					int rowSelected = mData2;

					if (rowSelected >= 0)
					{
						LANGameInfo * theGame = TheLAN->LookupGameByListOffset(rowSelected);
						if (theGame)
						{
							TheLAN->RequestGameJoin(theGame);
						}
					}
				}
				break;
			}
		case GLM_SELECTED:
			{
				if (LANbuttonPushed)
					break;
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();
				if( controlID == listboxGamesID )
				{
					int rowSelected = mData2;
					if( rowSelected < 0 )
					{
						HideGameInfoWindow(TRUE);
						break;
					}
					LANGameInfo * theGame = TheLAN->LookupGameByListOffset(rowSelected);
					if (theGame)
						RefreshGameInfoWindow(theGame, theGame->getName());
					else
						HideGameInfoWindow(TRUE);

				}
				break;
			}
		case GBM_SELECTED:
			{
				if (LANbuttonPushed)
					break;
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();

				if ( controlID == buttonBackID )
				{
					//shellmapOn = TRUE;
					LANbuttonPushed = true;
					DEBUG_LOG(("Back was hit - popping to main menu"));
					TheShell->pop();
					delete TheLAN;
					TheLAN = nullptr;
					//TheTransitionHandler->reverse("LanLobbyFade");

				}
				else if ( controlID == buttonHostID )
				{
					TheLAN->RequestGameCreate( L"", FALSE);

				}
				else if ( controlID == buttonClearID )
				{
					GadgetTextEntrySetText(textEntryPlayerName, UnicodeString::TheEmptyString);
					TheWindowManager->winSendSystemMsg( window,
																						GEM_UPDATE_TEXT,
																						(WindowMsgData)textEntryPlayerName,
																						0 );

				}
				else if ( controlID == buttonJoinID )
				{

					//TheShell->push( "Menus/LanGameOptionsMenu.wnd" );

					int rowSelected = -1;
					GadgetListBoxGetSelected( listboxGames, &rowSelected );

					if (rowSelected >= 0)
					{
						LANGameInfo * theGame = TheLAN->LookupGameByListOffset(rowSelected);
						if (theGame)
						{
							TheLAN->RequestGameJoin(theGame);
						}
					}
					else
					{
						GadgetListBoxAddEntryText(listboxChatWindow, TheGameText->fetch("LAN:ErrorNoGameSelected") , chatSystemColor, -1, 0);
					}

				}
				else if ( controlID == buttonEmoteID )
				{
					// read the user's input
					txtInput.set(GadgetTextEntryGetText( textEntryChat ));
					// Clear the text entry line
					GadgetTextEntrySetText(textEntryChat, UnicodeString::TheEmptyString);
					// Clean up the text (remove leading/trailing chars, etc)
					txtInput.trim();
					// Echo the user's input to the chat window
					if (!txtInput.isEmpty()) {
//						TheLAN->RequestChat(txtInput, LANAPIInterface::LANCHAT_EMOTE);
						TheLAN->RequestChat(txtInput, LANAPIInterface::LANCHAT_NORMAL);
					}
				}
				else if (controlID == buttonDirectConnectID)
				{
					TheLAN->RequestLobbyLeave( false );
					TheShell->push("Menus/NetworkDirectConnect.wnd");
				}

				break;
			}

		case GEM_UPDATE_TEXT:
			{
				if (LANbuttonPushed)
					break;
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();

				if ( controlID == textEntryPlayerNameID )
				{
					// grab the user's name
					txtInput.set(GadgetTextEntryGetText( textEntryPlayerName ));

					// Clean up the text (remove leading/trailing chars, etc)
					const WideChar *c = txtInput.str();
					while (c && (iswspace(*c)))
						c++;

					if (c)
						txtInput = UnicodeString(c);
					else
						txtInput = UnicodeString::TheEmptyString;

					txtInput.truncateTo(g_lanPlayerNameLength);

					if (!txtInput.isEmpty() && txtInput.getCharAt(txtInput.getLength()-1) == L',')
						txtInput.removeLastChar(); // we use , for strtok's so we can't allow them in names.  :(

					if (!txtInput.isEmpty() && txtInput.getCharAt(txtInput.getLength()-1) == L':')
						txtInput.removeLastChar(); // we use : for strtok's so we can't allow them in names.  :(

					if (!txtInput.isEmpty() && txtInput.getCharAt(txtInput.getLength()-1) == L';')
						txtInput.removeLastChar(); // we use ; for strtok's so we can't allow them in names.  :(

					// send it over the network
					if (!txtInput.isEmpty())
						TheLAN->RequestSetName(txtInput);
					else
						{
							TheLAN->RequestSetName(defaultName);
						}

					// Put the whitespace-free version in the box
					GadgetTextEntrySetText( textEntryPlayerName, txtInput );

				}
				break;
			}
		case GEM_EDIT_DONE:
			{
				if (LANbuttonPushed)
					break;
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();

				// Take the user's input and echo it into the chat window as well as
				// send it to the other clients on the lan
				if ( controlID == textEntryChatID )
				{

					// read the user's input
					txtInput.set(GadgetTextEntryGetText( textEntryChat ));
					// Clear the text entry line
					GadgetTextEntrySetText(textEntryChat, UnicodeString::TheEmptyString);
					// Clean up the text (remove leading/trailing chars, etc)
					while (!txtInput.isEmpty() && iswspace(txtInput.getCharAt(0)))
						txtInput = UnicodeString(txtInput.str()+1);

					// Echo the user's input to the chat window
					if (!txtInput.isEmpty())
						TheLAN->RequestChat(txtInput, LANAPIInterface::LANCHAT_NORMAL);

				}
				/*
				else if ( controlID == textEntryPlayerNameID )
				{
					// grab the user's name
					txtInput.set(GadgetTextEntryGetText( textEntryPlayerName ));

					// Clean up the text (remove leading/trailing chars, etc)
					txtInput.trim();

					// send it over the network
					if (!txtInput.isEmpty())
						TheLAN->RequestSetName(txtInput);

					// Put the whitespace-free version in the box
					GadgetTextEntrySetText( textEntryPlayerName, txtInput );

				}
				*/
				break;
			}
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;
}
