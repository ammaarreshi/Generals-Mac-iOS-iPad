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

// FILE: ControlBar.h /////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Context sensitive command interface
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "Common/AudioEventRTS.h"
#include "Common/GameType.h"
#include "Common/Overridable.h"
#include "Common/Science.h"
#include "GameClient/Color.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Drawable;
class GameFont;
class GameWindow;
class Image;
class Object;
class ThingTemplate;
class WeaponTemplate;
class SpecialPowerTemplate;
class WindowVideoManager;
class WindowVideoManager;
class AnimateWindowManager;
class GameWindow;
class WindowLayout;
class Player;
class PlayerTemplate;
class AudioEventRTS;
class ControlBarSchemeManager;
class UpgradeTemplate;
class ControlBarResizer;
class GameWindowTransitionsHandler;
class DisplayString;

enum ProductionID CPP_11(: Int);

enum CommandSourceType CPP_11(: Int);
enum ProductionType CPP_11(: Int);
enum GadgetGameMessage CPP_11(: Int);
enum ScienceType CPP_11(: Int);
enum TimeOfDay CPP_11(: Int);
enum RadiusCursorType CPP_11(: Int);

//-------------------------------------------------------------------------------------------------
/** Command options */
//-------------------------------------------------------------------------------------------------
enum CommandOption CPP_11(: Int)
{
	COMMAND_OPTION_NONE					= 0x00000000,
	NEED_TARGET_ENEMY_OBJECT		= 0x00000001, // command now needs user to select enemy target
	NEED_TARGET_NEUTRAL_OBJECT	= 0x00000002, // command now needs user to select neutral target
	NEED_TARGET_ALLY_OBJECT			= 0x00000004, // command now needs user to select ally target
#ifdef ALLOW_SURRENDER
	NEED_TARGET_PRISONER				= 0x00000008, // needs user to now select prisoner object
#endif
	ALLOW_SHRUBBERY_TARGET			= 0x00000010, // allow neutral shrubbery as a target
	NEED_TARGET_POS							= 0x00000020, // command now needs user to select target position
	NEED_UPGRADE								= 0x00000040, // command requires upgrade to be enabled
	NEED_SPECIAL_POWER_SCIENCE	= 0x00000080, // command requires a science in the special power specified
	OK_FOR_MULTI_SELECT					= 0x00000100, // command is ok to show when multiple objects selected
	CONTEXTMODE_COMMAND					= 0x00000200, // a context sensitive command mode that requires code to determine whether cursor is valid or not.
	CHECK_LIKE									= 0x00000400, // dynamically change the UI element push button to be "check like"
	ALLOW_MINE_TARGET						= 0x00000800, // allow (land)mines as a target
	ATTACK_OBJECTS_POSITION			=	0x00001000, // for weapons that need an object target but attack the position indirectly (like burning trees)
	OPTION_ONE									= 0x00002000, // User data -- option 1
	OPTION_TWO									= 0x00004000,	// User data -- option 2
	OPTION_THREE								= 0x00008000,	// User data -- option 3
	NOT_QUEUEABLE								= 0x00010000,	// Option not build queueable meaning you can only build it when queue is empty!
	SINGLE_USE_COMMAND					= 0x00020000, // Once used, it can never be used again!
	COMMAND_FIRED_BY_SCRIPT			= 0x00040000, // Used only by code to tell special powers that they have been fired by a script.
	SCRIPT_ONLY									= 0x00080000, // Only a script can use this command (not by users)
	IGNORES_UNDERPOWERED				= 0x00100000, // this button isn't disabled if its object is merely underpowered
	USES_MINE_CLEARING_WEAPONSET= 0x00200000,	// uses the special mine-clearing weaponset, even if not current
	CAN_USE_WAYPOINTS						= 0x00400000, // button has option to use a waypoint path
	MUST_BE_STOPPED							= 0x00800000, // Unit must be stopped in order to be able to use button.
};

#ifdef DEFINE_COMMAND_OPTION_NAMES
static const char *const TheCommandOptionNames[] =
{
	"NEED_TARGET_ENEMY_OBJECT",
	"NEED_TARGET_NEUTRAL_OBJECT",
	"NEED_TARGET_ALLY_OBJECT",
#ifdef ALLOW_SURRENDER
	"NEED_TARGET_PRISONER",
#else
	"unused-reserved",
#endif
	"ALLOW_SHRUBBERY_TARGET",
	"NEED_TARGET_POS",
	"NEED_UPGRADE",
	"NEED_SPECIAL_POWER_SCIENCE",
	"OK_FOR_MULTI_SELECT",
	"CONTEXTMODE_COMMAND",
	"CHECK_LIKE",
	"ALLOW_MINE_TARGET",
	"ATTACK_OBJECTS_POSITION",
	"OPTION_ONE",
	"OPTION_TWO",
	"OPTION_THREE",
	"NOT_QUEUEABLE",
	"SINGLE_USE_COMMAND",
	"---DO-NOT-USE---", //COMMAND_FIRED_BY_SCRIPT
	"SCRIPT_ONLY",
	"IGNORES_UNDERPOWERED",
	"USES_MINE_CLEARING_WEAPONSET",
	"CAN_USE_WAYPOINTS",
	"MUST_BE_STOPPED",

	nullptr
};
#endif  // end DEFINE_COMMAND_OPTION_NAMES

// convenient bit masks to group some command options together
const UnsignedInt COMMAND_OPTION_NEED_TARGET =
					NEED_TARGET_ENEMY_OBJECT |
					NEED_TARGET_NEUTRAL_OBJECT |
					NEED_TARGET_ALLY_OBJECT |
					NEED_TARGET_POS |
					CONTEXTMODE_COMMAND;

const UnsignedInt COMMAND_OPTION_NEED_OBJECT_TARGET =
					NEED_TARGET_ENEMY_OBJECT |
					NEED_TARGET_NEUTRAL_OBJECT |
					NEED_TARGET_ALLY_OBJECT;

//-------------------------------------------------------------------------------------------------
/** These are the list of commands that can be assigned to buttons that will appear
	* in the context sensitive GUI for a selected unit.  Not all commands are available
	* on all units, in fact, many commands are for a particular single command.  It will
	* be up to the command GUI to translate the command assigned to a button into the
	* appropriate game command and get it across the network to the game logic to perform the
	* actual command logic
	*
	* IMPORTANT: Make sure the GUICommandType enum and the TheGuiCommandNames[] have the same
	*						 entries in the same order */
//-------------------------------------------------------------------------------------------------
enum GUICommandType CPP_11(: Int)
{
	GUI_COMMAND_NONE = 0,									///< invalid command
	GUI_COMMAND_DOZER_CONSTRUCT,					///< dozer construct
	GUI_COMMAND_DOZER_CONSTRUCT_CANCEL,		///< cancel a dozer construction process
	GUI_COMMAND_UNIT_BUILD,								///< build a unit
	GUI_COMMAND_CANCEL_UNIT_BUILD,				///< cancel a unit build
	GUI_COMMAND_PLAYER_UPGRADE,						///< put an upgrade that applies to the player in the queue
	GUI_COMMAND_OBJECT_UPGRADE,						///< put an object upgrade in the queue
	GUI_COMMAND_CANCEL_UPGRADE,						///< cancel an upgrade
	GUI_COMMAND_ATTACK_MOVE,							///< attack move command
	GUI_COMMAND_GUARD,										///< guard command
	GUI_COMMAND_GUARD_WITHOUT_PURSUIT,		///< guard command, no pursuit out of guard area
	GUI_COMMAND_GUARD_FLYING_UNITS_ONLY,	///< guard command, ignore nonflyers
	GUI_COMMAND_STOP,											///< stop moving
	GUI_COMMAND_WAYPOINTS,								///< create a set of waypoints for this unit
	GUI_COMMAND_EXIT_CONTAINER,						///< an inventory box for a container like a structure or transport
	GUI_COMMAND_EVACUATE,									///< dump all our contents
	GUI_COMMAND_EXECUTE_RAILED_TRANSPORT,	///< execute railed transport sequence
	GUI_COMMAND_BEACON_DELETE,						///< delete a beacon
	GUI_COMMAND_SET_RALLY_POINT,					///< set rally point for a structure
	GUI_COMMAND_SELL,											///< sell a structure
	GUI_COMMAND_FIRE_WEAPON,							///< fire a weapon
	GUI_COMMAND_SPECIAL_POWER,						///< do a special power
	GUI_COMMAND_PURCHASE_SCIENCE,					///< purchase science
	GUI_COMMAND_HACK_INTERNET,						///< gain income from the ether (by hacking the internet)
	GUI_COMMAND_TOGGLE_OVERCHARGE,				///< Overcharge command for power plants
#ifdef ALLOW_SURRENDER
	GUI_COMMAND_POW_RETURN_TO_PRISON,			///< POW Truck, return to prison
#endif
	GUI_COMMAND_COMBATDROP,								///< rappel contents to ground or bldg
	GUI_COMMAND_SWITCH_WEAPON,						///< switch weapon use

	//Context sensitive command modes
	GUICOMMANDMODE_HIJACK_VEHICLE,
	GUICOMMANDMODE_CONVERT_TO_CARBOMB,
	GUICOMMANDMODE_SABOTAGE_BUILDING,
#ifdef ALLOW_SURRENDER
	GUICOMMANDMODE_PICK_UP_PRISONER,			///< POW Truck assigned to pick up a specific prisoner
#endif

	// context-insensitive command mode(s)
	GUICOMMANDMODE_PLACE_BEACON,

	GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT,			///< do a special power from localPlayer's command center, regardless of selection
#if RTS_GENERALS
	GUI_COMMAND_SPECIAL_POWER_FROM_COMMAND_CENTER = GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT, ///< Legacy name
#endif
	GUI_COMMAND_SPECIAL_POWER_CONSTRUCT,					///< do a special power using the construct building interface
	GUI_COMMAND_SPECIAL_POWER_CONSTRUCT_FROM_SHORTCUT, ///< do a shortcut special power using the construct building interface

	GUI_COMMAND_SELECT_ALL_UNITS_OF_TYPE,

	// add more commands here, don't forget to update the string command list below too ...

	GUI_COMMAND_NUM_COMMANDS
};

#ifdef DEFINE_GUI_COMMAND_NAMES
static const char *const TheGuiCommandNames[] =
{
	"NONE",
	"DOZER_CONSTRUCT",
	"DOZER_CONSTRUCT_CANCEL",
	"UNIT_BUILD",
	"CANCEL_UNIT_BUILD",
	"PLAYER_UPGRADE",
	"OBJECT_UPGRADE",
	"CANCEL_UPGRADE",
	"ATTACK_MOVE",
	"GUARD",
	"GUARD_WITHOUT_PURSUIT",
	"GUARD_FLYING_UNITS_ONLY",
	"STOP",
	"WAYPOINTS",
	"EXIT_CONTAINER",
	"EVACUATE",
	"EXECUTE_RAILED_TRANSPORT",
	"BEACON_DELETE",
	"SET_RALLY_POINT",
	"SELL",
	"FIRE_WEAPON",
	"SPECIAL_POWER",
	"PURCHASE_SCIENCE",
	"HACK_INTERNET",
	"TOGGLE_OVERCHARGE",
#ifdef ALLOW_SURRENDER
	"POW_RETURN_TO_PRISON",
#endif
	"COMBATDROP",
	"SWITCH_WEAPON",
	"HIJACK_VEHICLE",
	"CONVERT_TO_CARBOMB",
	"SABOTAGE_BUILDING",
#ifdef ALLOW_SURRENDER
	"PICK_UP_PRISONER",
#endif
	"PLACE_BEACON",
#if RTS_GENERALS
	"SPECIAL_POWER_FROM_COMMAND_CENTER", ///< Legacy name
#else
	"SPECIAL_POWER_FROM_SHORTCUT",
#endif
	"SPECIAL_POWER_CONSTRUCT",
	"SPECIAL_POWER_CONSTRUCT_FROM_SHORTCUT",
	"SELECT_ALL_UNITS_OF_TYPE",

	nullptr
};
static_assert(ARRAY_SIZE(TheGuiCommandNames) == GUI_COMMAND_NUM_COMMANDS + 1, "Incorrect array size");
#endif  // end DEFINE_GUI_COMMAND_NAMES

enum CommandButtonMappedBorderType CPP_11(: Int)
{
	COMMAND_BUTTON_BORDER_NONE = 0,
	COMMAND_BUTTON_BORDER_BUILD,
	COMMAND_BUTTON_BORDER_UPGRADE,
	COMMAND_BUTTON_BORDER_ACTION,
	COMMAND_BUTTON_BORDER_SYSTEM,

	COMMAND_BUTTON_BORDER_COUNT
};

static const LookupListRec CommandButtonMappedBorderTypeNames[] =
{
	{ "NONE",					COMMAND_BUTTON_BORDER_NONE },
	{ "BUILD",				COMMAND_BUTTON_BORDER_BUILD },
	{ "UPGRADE",			COMMAND_BUTTON_BORDER_UPGRADE },
	{ "ACTION",				COMMAND_BUTTON_BORDER_ACTION },
	{ "SYSTEM",				COMMAND_BUTTON_BORDER_SYSTEM },

	{ nullptr, 0	}
};
static_assert(ARRAY_SIZE(CommandButtonMappedBorderTypeNames) == COMMAND_BUTTON_BORDER_COUNT + 1, "Incorrect array size");
//-------------------------------------------------------------------------------------------------
/** Command buttons are used to load the buttons we place on throughout the command bar
	* interface in different context sensitive windows depending on the situation and
	* type of the object selected */
//-------------------------------------------------------------------------------------------------
class CommandButton : public Overridable
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CommandButton, "CommandButton" );

public:

	CommandButton();
	// virtual destructor prototype provided by MemoryPoolObject

	/// INI parsing
	const FieldParse *getFieldParse() const { return s_commandButtonFieldParseTable; }
	static const FieldParse s_commandButtonFieldParseTable[];		///< the parse table
	static void parseCommand( INI* ini, void *instance, void *store, const void *userData );

	Bool isContextCommand() const;									///< determines if this is a context sensitive command.
	Bool isValidRelationshipTarget(Relationship r) const;
	Bool isValidObjectTarget(const Player* sourcePlayer, const Object* targetObj) const;
	Bool isValidObjectTarget(const Object* sourceObj, const Object* targetObj) const;
	Bool isValidObjectTarget(const Drawable* source, const Drawable* target) const;

	// Note: It is perfectly valid for either (or both!) of targetObj and targetLocation to be nullptr.
	// This is a convenience function to make several calls to other functions.
	Bool isValidToUseOn(const Object *sourceObj, const Object *targetObj, const Coord3D *targetLocation, CommandSourceType commandSource) const;
	Bool isReady(const Object *sourceObj) const;

	const AsciiString& getName() const { return m_name; }
	const AsciiString& getCursorName() const { return m_cursorName; }
	const AsciiString& getInvalidCursorName() const { return m_invalidCursorName; }
	const AsciiString& getTextLabel() const { return m_textLabel; }
	const AsciiString& getDescriptionLabel() const { return m_descriptionLabel; }
	const AsciiString& getPurchasedLabel() const { return m_purchasedLabel; }
	const AsciiString& getConflictingLabel() const { return m_conflictingLabel; }
	const AudioEventRTS* getUnitSpecificSound() const { return &m_unitSpecificSound; }

	GUICommandType getCommandType() const { return m_command; }
	UnsignedInt getOptions() const { return m_options; }
	OVERRIDE<ThingTemplate> getThingTemplate() const { return m_thingTemplate; }
	const UpgradeTemplate* getUpgradeTemplate() const { return m_upgradeTemplate; }
	const SpecialPowerTemplate* getSpecialPowerTemplate() const { return m_specialPower; }
	RadiusCursorType getRadiusCursorType() const { return m_radiusCursor; }
	WeaponSlotType getWeaponSlot() const { return m_weaponSlot; }
	Int getMaxShotsToFire() const { return m_maxShotsToFire; }
	const ScienceVec& getScienceVec() const { return m_science; }
	CommandButtonMappedBorderType getCommandButtonMappedBorderType() const { return m_commandButtonBorder; }
	const Image* getButtonImage() const { return m_buttonImage;	}
	void cacheButtonImage();

	GameWindow* getWindow() const { return m_window;	}
	Int getFlashCount() const { return m_flashCount; }

	const CommandButton* getNext() const { return m_next; }

	void setName(const AsciiString& n) { m_name = n; }

	void setButtonImage( const Image *image ) { m_buttonImage = image; }

	// bleah. shouldn't be const, but is. sue me. (srj)
	void copyImagesFrom( const CommandButton *button, Bool markUIDirtyIfChanged ) const;

	// bleah. shouldn't be const, but is. sue me. (Kris) -snork!
	void copyButtonTextFrom( const CommandButton *button, Bool shortcutButton, Bool markUIDirtyIfChanged ) const;

	// bleah. shouldn't be const, but is. sue me. (srj)
	void setFlashCount(Int c) const { m_flashCount = c; }

	// only for ControlBar!
	void friend_addToList(CommandButton** list) {	m_next = *list;	*list = this; }
	CommandButton* friend_getNext() { return m_next; }

private:
	AsciiString										m_name;												///< template name
	GUICommandType								m_command;										///< type of command this button
	CommandButton*								m_next;
	UnsignedInt										m_options;										///< command options (see CommandOption enum)
	const ThingTemplate*					m_thingTemplate;							///< for commands that use thing templates in command data
	const UpgradeTemplate*				m_upgradeTemplate;						///< for commands that use upgrade templates in command data
	const SpecialPowerTemplate*		m_specialPower;								///< actual special power template
	RadiusCursorType							m_radiusCursor;								///< radius cursor, if any
	AsciiString										m_cursorName;									///< cursor name for placement (NEED_TARGET_POS) or valid version (CONTEXTMODE_COMMAND)
	AsciiString										m_invalidCursorName;					///< cursor name for invalid version

	// bleah. shouldn't be mutable, but is. sue me. (Kris) -snork!
	mutable AsciiString										m_textLabel;									///< string manager text label
	mutable AsciiString										m_descriptionLabel;						///< The description of the current command, read in from the ini

	AsciiString										m_purchasedLabel;							///< Description for the current command if it has already been purchased.
	AsciiString										m_conflictingLabel;						///< Description for the current command if it can't be selected due to multually-exclusive choice.
	WeaponSlotType								m_weaponSlot;									///< for commands that refer to a weapon slot
	Int														m_maxShotsToFire;							///< for commands that fire weapons
	ScienceVec										m_science;										///< actual science
	CommandButtonMappedBorderType	m_commandButtonBorder;
	AsciiString										m_buttonImageName;
	GameWindow*										m_window;											///< used during the run-time assignment of a button to a gadget button window
	AudioEventRTS									m_unitSpecificSound;					///< Unit sound played whenever button is clicked.

	// bleah. shouldn't be mutable, but is. sue me. (srj)
	mutable const Image*					m_buttonImage;								///< button image
	// bleah. shouldn't be mutable, but is. sue me. (srj)
	mutable Int										m_flashCount;                 ///< the number of times a cameo is supposed to flash

};

//-------------------------------------------------------------------------------------------------
/** Command sets are collections of configurable command buttons.  They are used in the
	* command context sensitive window in the battle user interface */
//-------------------------------------------------------------------------------------------------
enum { MAX_COMMANDS_PER_SET = 18 };  // user interface max is 14 (but internally it's 18 for script only buttons!)
enum { MAX_RIGHT_HUD_UPGRADE_CAMEOS = 5};
enum {
			 MAX_PURCHASE_SCIENCE_RANK_1 = 4,
			 MAX_PURCHASE_SCIENCE_RANK_3 = 15,
			 MAX_PURCHASE_SCIENCE_RANK_8 = 4,
			};
enum { MAX_STRUCTURE_INVENTORY_BUTTONS = 10 }; // there are this many physical buttons in "inventory" windows for structures
enum { MAX_BUILD_QUEUE_BUTTONS = 9 };// physical button count for the build queue
enum { MAX_SPECIAL_POWER_SHORTCUTS = 11};
class CommandSet : public Overridable
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CommandSet, "CommandSet" )

public:

	CommandSet( const AsciiString& name );
	// virtual destructor prototype provided by MemoryPoolObject

	const AsciiString& getName() const { return m_name; }
	const CommandButton* getCommandButton(Int i) const;

	// only for the control bar.
	CommandSet* friend_getNext() { return m_next; }
	const FieldParse* friend_getFieldParse() const { return m_commandSetFieldParseTable; }
	void friend_addToList(CommandSet** listHead);

private:

	static const FieldParse m_commandSetFieldParseTable[];		///< the parse table
	static void parseCommandButton( INI* ini, void *instance, void *store, const void *userData );

	AsciiString m_name;															  ///< name of this command set
	const CommandButton *m_command[ MAX_COMMANDS_PER_SET ]; ///< the set of command buttons that make this set

	CommandSet *m_next;

};

//-------------------------------------------------------------------------------------------------
/** The Side selece window data is used to animate on the proper generals select */
//-------------------------------------------------------------------------------------------------
class SideSelectWindowData
{
public:
	SideSelectWindowData()
	{
		generalSpeak = nullptr;
		m_currColor = 0;
		m_gereralsNameWin = nullptr;
		m_lastTime = 0;
		m_pTemplate = nullptr;
		m_sideNameWin = nullptr;
		m_startTime = 0;
		m_state = 0;
		m_upgradeImage1 = nullptr;
		m_upgradeImage1Win = nullptr;
		m_upgradeImage2 = nullptr;
		m_upgradeImage2Win = nullptr;
		m_upgradeImage3 = nullptr;
		m_upgradeImage3Win = nullptr;
		m_upgradeImage4 = nullptr;
		m_upgradeImage4Win = nullptr;
		m_upgradeImageSize.x = m_upgradeImageSize.y = 0;

		m_upgradeLabel1Win = nullptr;
		m_upgradeLabel2Win = nullptr;
		m_upgradeLabel3Win = nullptr;
		m_upgradeLabel4Win = nullptr;
		sideWindow = nullptr;
	}
	~SideSelectWindowData();

	void init( ScienceType science, GameWindow *control );
	void reset();
	void update();
	void draw();

	GameWindow *sideWindow;
	GameWindow *m_animWindowWin;
	AudioEventRTS *generalSpeak;
private:
	enum
	{
		STATE_NONE = 0,
		STATE_1,
		STATE_2,
		STATE_3,
		STATE_4,
		STATE_5,
		STATE_6
	};

	const PlayerTemplate *m_pTemplate;

	GameWindow *m_gereralsNameWin;
	GameWindow *m_sideNameWin;


	GameWindow *m_upgradeLabel1Win;
	GameWindow *m_upgradeLabel2Win;
	GameWindow *m_upgradeLabel3Win;
	GameWindow *m_upgradeLabel4Win;

	GameWindow *m_upgradeImage1Win;
	GameWindow *m_upgradeImage2Win;
	GameWindow *m_upgradeImage3Win;
	GameWindow *m_upgradeImage4Win;

	Image *m_upgradeImage1;
	Image *m_upgradeImage2;
	Image *m_upgradeImage3;
	Image *m_upgradeImage4;

	IRegion2D m_leftLineFromButton;
	IRegion2D m_rightLineFromButton;

	IRegion2D m_upgradeLine1a;
	IRegion2D m_upgradeLine2a;
	IRegion2D m_upgradeLine3a;
	IRegion2D m_upgradeLine4a;

	IRegion2D m_upgradeLine1;
	IRegion2D m_upgradeLine2;
	IRegion2D m_upgradeLine3;
	IRegion2D m_upgradeLine4;

	IRegion2D m_upgradeLine1MidReg;
	IRegion2D m_upgradeLine2MidReg;
	IRegion2D m_upgradeLine3MidReg;
	IRegion2D m_upgradeLine4MidReg;

	IRegion2D m_upgrade1Clip;
	IRegion2D m_upgrade2Clip;
	IRegion2D m_upgrade3Clip;
	IRegion2D m_upgrade4Clip;

	Color m_currColor;
	ICoord2D m_line1End;
	ICoord2D m_line2End;


	ICoord2D m_upgradeLine1Mid;
	ICoord2D m_upgradeLine2Mid;
	ICoord2D m_upgradeLine3Mid;
	ICoord2D m_upgradeLine4Mid;

	ICoord2D m_upgradeLine1End;
	ICoord2D m_upgradeLine2End;
	ICoord2D m_upgradeLine3End;
	ICoord2D m_upgradeLine4End;

	ICoord2D m_upgradeImagePos1;
	ICoord2D m_upgradeImagePos2;
	ICoord2D m_upgradeImagePos3;
	ICoord2D m_upgradeImagePos4;

	ICoord2D m_upgradeImageSize;

	Int m_state;
	UnsignedInt m_lastTime;
	UnsignedInt m_startTime;
};

//-------------------------------------------------------------------------------------------------
/** A command bar context is a window or set of windows that make up a context sensitive
	* display of commands and information to the user based on what objects are selected
	* and their capabilities */
//-------------------------------------------------------------------------------------------------
enum ControlBarContext CPP_11(: Int)
{
	CB_CONTEXT_NONE,									///< default view for center bar and portrait window
//	CB_CONTEXT_PURCHASE_SCIENCE,
	CB_CONTEXT_COMMAND,								///< set of commands (attack-move,stop,deploy etc)
	CB_CONTEXT_STRUCTURE_INVENTORY,		///< garrisonable building inventory interface
	CB_CONTEXT_BEACON,								///< beacon interface
	CB_CONTEXT_UNDER_CONSTRUCTION,		///< building under construction
	CB_CONTEXT_MULTI_SELECT,					///< for when we have multiple objects selected
	CB_CONTEXT_OBSERVER_INFO,					///< for when we want to populate the player info
	CB_CONTEXT_OBSERVER_LIST,					///< for when we want to update the observer list
	CB_CONTEXT_OCL_TIMER,							///< Countdown for OCL spewers
};

//-------------------------------------------------------------------------------------------------
/** Context parents, are parent windows in the control bar interface that are the key
	* parts we care about to make the whole interface a context sensitive one.  We will
	* hide and un-hide these windows and their interface controls in order to make
	* the control bar context sensitive to the object that is selected */
//-------------------------------------------------------------------------------------------------
enum ContextParent CPP_11(: Int)
{
	CP_MASTER,									///< *The* control bar window as a whole
	CP_PURCHASE_SCIENCE,
	CP_COMMAND,									///< configurable command buttons parent
	CP_BUILD_QUEUE,							///< build queue parent
	CP_BEACON,									///< beacon parent
	CP_UNDER_CONSTRUCTION,			///< building under construction parent
	CP_OBSERVER_INFO,						///< Observer Info window parent
	CP_OBSERVER_LIST,						///< Observer player list parent
	CP_OCL_TIMER,								///< Countdown for OCL spewers

	NUM_CONTEXT_PARENTS
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
enum CBCommandStatus CPP_11(: Int)
{
	CBC_COMMAND_NOT_USED = 0,		///< gui control message was *not* used
	CBC_COMMAND_USED						///< gui control message was used
};

// ------------------------------------------------------------------------------------------------
/** Command availability is used during the context update so that we can set the
	* GUI button to be enabled/disabled/checked/unchecked to represent the current
	* state of that command availability */
// ------------------------------------------------------------------------------------------------
enum CommandAvailability CPP_11(: Int)
{
	COMMAND_RESTRICTED,
	COMMAND_AVAILABLE,
	COMMAND_ACTIVE,
  COMMAND_HIDDEN,
	COMMAND_NOT_READY,
	COMMAND_CANT_AFFORD,
};

enum ControlBarStages CPP_11(: Int)
{
	CONTROL_BAR_STAGE_DEFAULT = 0,		///< full view for the world to see
	CONTROL_BAR_STAGE_SQUISHED,				///< squished just for expeirenced players
	CONTROL_BAR_STAGE_LOW,						///< control bar a la minimalist
	CONTROL_BAR_STAGE_HIDDEN,					///< yo, where be da control bar at?

	MAX_CONTROL_BAR_STAGES
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class ControlBar : public SubsystemInterface
{

public:

	ControlBar();
	virtual ~ControlBar() override;

	virtual void init() override;					///< from subsystem interface
	virtual void reset() override;					///< from subsystem interface
	virtual void update() override;				///< from subsystem interface

	/// mark the UI as dirty so the context of everything is re-evaluated
	void markUIDirty();

	/// a drawable has just become selected
	void onDrawableSelected( Drawable *draw );

	/// a drawable has just become de-selected
	void onDrawableDeselected( Drawable *draw );

	void onPlayerRankChanged(const Player *p);
	void onPlayerSciencePurchasePointsChanged(const Player *p);

	/** if this button is part of the context sensitive command system, process a button click
	the gadgetMessage is either a GBM_SELECTED or GBM_SELECTED_RIGHT */
	CBCommandStatus processContextSensitiveButtonClick( GameWindow *button,
																											GadgetGameMessage gadgetMessage );

	/** if this button is part of the context sensitive command system, process the Transition
	gadgetMessage is either a GBM_MOUSE_LEAVING or GBM_MOUSE_ENTERING */
	CBCommandStatus processContextSensitiveButtonTransition( GameWindow *button,
																											GadgetGameMessage gadgetMessage );


	/// is the drawable the currently selected drawable for the context sensitive UI?
	Bool isDrivingContextUI( Drawable *draw ) const { return draw == m_currentSelectedDrawable; }

	//-----------------------------------------------------------------------------------------------
	// the remaining methods are used to construct the command buttons and command sets for
	// the command bar
	//-----------------------------------------------------------------------------------------------

	/// find existing command button if present
	const CommandButton *findCommandButton( const AsciiString& name );

	/// find existing command set
	const CommandSet *findCommandSet( const AsciiString& name );

	void showPurchaseScience();
	void hidePurchaseScience();
	void togglePurchaseScience();

	Bool hasAnyShortcutSelection() const;
	Bool canShowSpecialPowerShortcut() const;
	void showSpecialPowerShortcut();
	void hideSpecialPowerShortcut();
	void animateSpecialPowerShortcut( Bool isOn );

	void setFullViewportHeight();
	void setScaledViewportHeight();

	/// set the control bar to the proper scheme based off a player template that's passed in
	ControlBarSchemeManager *getControlBarSchemeManager() { return m_controlBarSchemeManager; }
	void setControlBarSchemeByPlayer(Player *p);
	void setControlBarSchemeByName(const AsciiString& name);
	void setControlBarSchemeByPlayerTemplate(const PlayerTemplate *pt);

	/// We need to sometime change what the images look like depending on what scheme we're using
	void updateBuildQueueDisabledImages( const Image *image );

	/// We need to sometime change what the images look like depending on what scheme we're using
	void updateRightHUDImage( const Image *image );

	/// We need to be able to update the command marker image based on which scheme we're using.
	void updateCommandMarkerImage( const Image *image );
	void updateSlotExitImage( const Image *image);

	void updateUpDownImages( const Image *toggleButtonUpIn, const Image *toggleButtonUpOn, const Image *toggleButtonUpPushed, const Image *toggleButtonDownIn, const Image *toggleButtonDownOn, const Image *toggleButtonDownPushed,const Image *generalButtonEnable, const Image *generalButtonHighlight  );

	void preloadAssets( TimeOfDay timeOfDay );		///< preload the assets

	/// We want to be able to have the control bar scheme set the color of the build up clock
	void updateBuildUpClockColor( Color color);

	WindowVideoManager *m_videoManager;						///< Video manager to take care of all animations on screen.
	AnimateWindowManager *m_animateWindowManager; ///< The animate window manager
	AnimateWindowManager *m_animateWindowManagerForGenShortcuts; ///< The animate window manager
	void updatePurchaseScience();
	AnimateWindowManager *m_generalsScreenAnimate; ///< The animate window manager

	// Initialize the Observer controls Must be called after we've already loaded the window
	void initObserverControls();
	void populateObserverInfoWindow ();
	void populateObserverList();
	Bool isObserverControlBarOn() { return m_isObserverCommandBar;}

	void setObserverLookAtPlayer (Player *player); ///< Sets the looked at player. Used to present information about the player.
	Player *getObserverLookAtPlayer () const { return m_observerLookAtPlayer; } ///< Returns the looked at player. Can return null.

	void setObservedPlayer(Player *player); ///< Sets the observed player. Used to present the game world as if that player was the local player.
	Player *getObservedPlayer() const { return m_observedPlayer; } ///< Return the observed player. Can return null.

	/// Returns the currently viewed player. May return nullptr if no player is selected while observing.
	Player* getCurrentlyViewedPlayer();
	/// Returns the relationship with the currently viewed player. May return NEUTRAL if no player is selected while observing.
	Relationship getCurrentlyViewedPlayerRelationship(const Team* team);

//	ControlBarResizer *getControlBarResizer() {return m_controlBarResizer;}

	// Functions for repositioning/resizing the control bar
	void switchControlBarStage( ControlBarStages stage );
	void toggleControlBarStage();

	const Image *getStarImage();

	Color getBorderColor(){return m_commandBarBorderColor;}
	void updateBorderColor( Color color) {m_commandBarBorderColor = color;	}

	/// set the command data into the button
	void setControlCommand( GameWindow *button, const CommandButton *commandButton );

	void getForegroundMarkerPos(Int *x, Int *y);
	void getBackgroundMarkerPos(Int *x, Int *y);


	static void parseCommandSetDefinition( INI *ini );
	static void parseCommandButtonDefinition( INI *ini );

	void drawTransitionHandler();
	const Image *getArrowImage(){ return m_genArrow;	}
	void setArrowImage( const Image *arrowImage ){ m_genArrow = arrowImage;	}

	void initSpecialPowershortcutBar( Player *player);

	void triggerRadarAttackGlow();

	void drawSpecialPowerShortcutMultiplierText();

protected:
	void updateRadarAttackGlow ();

	void setDefaultControlBarConfig();
	void setSquishedControlBarConfig();
	void setLowControlBarConfig();
	void setHiddenControlBar();

	/// find existing command button if present
	CommandButton* findNonConstCommandButton( const AsciiString& name );

	/// allocate a new command button, link to list, initialize to default, and return
	CommandButton *newCommandButton( const AsciiString& name );
	CommandButton *newCommandButtonOverride( CommandButton *buttonToOverride );

	/// allocate a new command set, link to list, initialize to default, and return it
	CommandSet *newCommandSet( const AsciiString& name );
	CommandSet *newCommandSetOverride( CommandSet *setToOverride );


	/// evaluate what the user should see based on what selected drawables we have in our UI
	void evaluateContextUI();

	/// add the common commands of this drawable to the common command set
	void addCommonCommands( Drawable *draw, Bool firstDrawable );

	/// switch the interface context to the new mode and populate as needed
	void switchToContext( ControlBarContext context, Drawable *draw );

	/// set the command data into the button
	void setControlCommand( const AsciiString& buttonWindowName, GameWindow *parent,
											 const CommandButton *commandButton );

	/// show/hide the portrait window image using the image pointer to set
	void setPortraitByImage( const Image *image );

	/// show/hide the portrait window image using the image from the object
	void setPortraitByObject( Object *obj );

	/// show rally point at world location, a nullptr location will hide any visible rally point marker
	void showRallyPoint( const Coord3D *loc );

	/// post process step, after all commands and command sets are loaded
	void postProcessCommands();

	// the following methods are for resetting data for various contexts
	void resetCommonCommandData();	/// reset shared command data
	void resetContainData();			/// reset container data we use to tie controls to objects IDs for containment
	void resetBuildQueueData();			/// reset the build queue data we use to die queue entires to control

	// the following methods are for populating the context GUI controls for a particular context
	static void populateButtonProc( Object *obj, void *userData );
	void populatePurchaseScience(Player* player);
	void populateCommand( Object *obj );
	void populateMultiSelect();
	void populateBuildQueue( Object *producer );
	void populateStructureInventory( Object *building );
	void populateBeacon( Object *beacon );
	void populateUnderConstruction( Object *objectUnderConstruction );
	void populateOCLTimer( Object *creatorObject );
	void doTransportInventoryUI( Object *transport, const CommandSet *commandSet );
	static void populateInvDataCallback( Object *obj, void *userData );

	// the following methods are for updating the currently showing context
	CommandAvailability getCommandAvailability( const CommandButton *command, Object *obj, GameWindow *win, GameWindow *applyToWin = nullptr, Bool forceDisabledEvaluation = FALSE ) const;
	void updateContextMultiSelect();
	void updateContextPurchaseScience();
	void updateContextCommand();
	void updateContextStructureInventory();
	void updateContextBeacon();
	void updateContextUnderConstruction();
	void updateContextOCLTimer();

	// the following methods are for the special power shortcut window

	void populateSpecialPowerShortcut( Player *player);
	void updateSpecialPowerShortcut();

	static const Image* calculateVeterancyOverlayForThing( const ThingTemplate *thingTemplate );
	static const Image* calculateVeterancyOverlayForObject( const Object *obj );

	// the following methods do command processing for GUI selections
	CBCommandStatus processCommandUI( GameWindow *control, GadgetGameMessage gadgetMessage );
	CBCommandStatus processCommandTransitionUI( GameWindow *control, GadgetGameMessage gadgetMessage );

	// methods to help out with each context
	void updateConstructionTextDisplay( Object *obj );
	void updateOCLTimerTextDisplay( UnsignedInt totalSeconds, Real percent );

	void setUpDownImages();
		// methods for flashing cameos
public:
	void setFlash( Bool b ) { m_flash = b; }

	// get method for list of commandbuttons
	const CommandButton *getCommandButtons() { return m_commandButtons; }

protected:

	ICoord2D m_defaultControlBarPosition;				///< Stored the original position of the control bar on the screen
	ControlBarStages m_currentControlBarStage;

	Bool m_UIDirty;																///< the context UI must be re-evaluated

	CommandButton *m_commandButtons;							///< list of possible commands to have
	CommandSet *m_commandSets;										///< list of all command sets defined
	ControlBarSchemeManager *m_controlBarSchemeManager;		///< The Scheme singleton

	GameWindow *m_contextParent[ NUM_CONTEXT_PARENTS ];		///< "parent" window for buttons that are part of the context sensitive interface

	Drawable *m_currentSelectedDrawable;					///< currently selected drawable for the context sensitive interface
	ControlBarContext m_currContext;							///< our current displayed context

	DrawableID m_rallyPointDrawableID;						///< rally point drawable for visual rally point

	Real m_displayedConstructPercent;							///< construct percent last displayed to user
	UnsignedInt m_displayedOCLTimerSeconds;				///< OCL Timer seconds remaining last displayed to user
	UnsignedInt m_displayedQueueCount;						///< queue count last displayed to user
	UnsignedInt m_lastRecordedInventoryCount;			///< last known UI state of an inventory count

	GameWindow *m_rightHUDWindow;									///< window of the right HUD display
	GameWindow *m_rightHUDCameoWindow;									///< window of the right HUD display
	GameWindow *m_rightHUDUpgradeCameos[MAX_RIGHT_HUD_UPGRADE_CAMEOS];
	GameWindow *m_rightHUDUnitSelectParent;

	GameWindow *m_communicatorButton;             ///< button for the communicator

	WindowLayout *m_scienceLayout;								///< the Science window layout
	GameWindow *m_sciencePurchaseWindowsRank1[ MAX_PURCHASE_SCIENCE_RANK_1 ];			///< command window controls for easy access
	GameWindow *m_sciencePurchaseWindowsRank3[ MAX_PURCHASE_SCIENCE_RANK_3 ];			///< command window controls for easy access
	GameWindow *m_sciencePurchaseWindowsRank8[ MAX_PURCHASE_SCIENCE_RANK_8 ];			///< command window controls for easy access
	GameWindow *m_specialPowerShortcutButtons[ MAX_SPECIAL_POWER_SHORTCUTS ];
	GameWindow *m_specialPowerShortcutButtonParents[ MAX_SPECIAL_POWER_SHORTCUTS ];
	DisplayString *m_shortcutDisplayStrings[ MAX_SPECIAL_POWER_SHORTCUTS ];
	Int m_currentlyUsedSpecialPowersButtons; ///< Value will be <= MAX_SPECIAL_POWER_SHORTCUTS;


	WindowLayout *m_specialPowerLayout;
	GameWindow *m_specialPowerShortcutParent;

	GameWindow *m_commandWindows[ MAX_COMMANDS_PER_SET ];			///< command window controls for easy access
	const CommandButton *m_commonCommands[ MAX_COMMANDS_PER_SET ];	///< shared commands we will use for multi-selection

		// removed from multiplayer branch
	//GameWindow *m_commandMarkers[ MAX_COMMANDS_PER_SET ];			///< When we don't have a command, they want to show an image
// removed from multiplayer branch
	//void showCommandMarkers();													///< function that compare's what's being shown in m_commandWindows and shows the ones that are hidden.


public:
	// method for hiding communicator window CCB
	void hideCommunicator( Bool b );

protected:

	struct ContainEntry
	{
	  GameWindow *control;
		ObjectID objectID;
	};
	static ContainEntry m_containData[ MAX_COMMANDS_PER_SET ];  ///< inventory buttons integrated into the regular command set for buildings/transports

	struct QueueEntry
	{
		GameWindow *control;											///< window that the GUI control is tied to
		ProductionType type;											///< type of queue data
		union
		{
			ProductionID productionID;										///< production id for unit productions
			const UpgradeTemplate *upgradeToResearch;			///< upgrade template for upgrade productions
		};

	};
	QueueEntry m_queueData[ MAX_BUILD_QUEUE_BUTTONS ];	///< what the build queue represents

	//cameo flash
	Bool m_flash;                                       ///< tells update whether or not to check for flash

	Bool m_sideSelectAnimateDown;
	ICoord2D m_animateDownWin1Size;
	ICoord2D m_animateDownWin2Size;
	ICoord2D m_animateDownWin1Pos;
	ICoord2D m_animateDownWin2Pos;
	GameWindow *m_animateDownWindow;
	UnsignedInt m_animTime;

	Color m_buildUpClockColor;

	Bool m_isObserverCommandBar;												///< If this is true, the command bar behaves greatly different
	Player *m_observerLookAtPlayer;											///< The current player we're looking at, Null if we're not looking at anyone.
	Player *m_observedPlayer;														///< The current player we're observing, Null if we're not observing anyone.

	WindowLayout *m_buildToolTipLayout;										///< The window that will slide on/display tooltips
	Bool m_showBuildToolTipLayout;											///< every frame we test to see if we are going to continue showing this or not.
public:
	void showBuildTooltipLayout( GameWindow *cmdButton );
	void hideBuildTooltipLayout();
	void deleteBuildTooltipLayout();
	Bool getShowBuildTooltipLayout(){return m_showBuildToolTipLayout;	}
	void populateBuildTooltipLayout( const CommandButton *commandButton, GameWindow *tooltipWin = nullptr );
	void repopulateBuildTooltipLayout();
	GameFont *overrideTooltipGadgetFont( GameWindow *win );
private:


	// Command Bar button border bars stuff
	Color m_commandButtonBorderBuildColor;
	Color m_commandButtonBorderActionColor;
	Color m_commandButtonBorderUpgradeColor;
	Color m_commandButtonBorderSystemColor;

	Color m_commandBarBorderColor;

	void setCommandBarBorder( GameWindow *button, CommandButtonMappedBorderType type);
public:
	void updateCommandBarBorderColors(Color build, Color action, Color upgrade, Color system );

private:

	/// find existing command set
	CommandSet *findNonConstCommandSet( const AsciiString& name );

	const Image *m_genStarOn;
	const Image *m_genStarOff;

	const Image *m_toggleButtonUpIn;
	const Image *m_toggleButtonUpOn;
	const Image *m_toggleButtonUpPushed;
	const Image *m_toggleButtonDownIn;
	const Image *m_toggleButtonDownOn;
	const Image *m_toggleButtonDownPushed;

	GameWindowTransitionsHandler *m_transitionHandler;
	const Image *m_genArrow;

	static const Image *m_rankVeteranIcon;
	static const Image *m_rankEliteIcon;
	static const Image *m_rankHeroicIcon;

	const Image *m_generalButtonEnable;
	const Image *m_generalButtonHighlight;


	Bool m_genStarFlash;
	Real m_genStarFlashTimeAccumulator; ///< Frame time accumulated within the current star blink cycle, in seconds
	Int m_lastFlashedAtPointValue;

	ICoord2D m_controlBarForegroundMarkerPos;
	ICoord2D m_controlBarBackgroundMarkerPos;

	Bool m_radarAttackGlowOn;
	Int m_remainingRadarAttackGlowFrames;
	GameWindow *m_radarAttackGlowWindow;

#if defined(RTS_DEBUG)
	UnsignedInt m_lastFrameMarkedDirty;
	UnsignedInt m_consecutiveDirtyFrames;
#endif
//	ControlBarResizer *m_controlBarResizer;

};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern ControlBar *TheControlBar;
