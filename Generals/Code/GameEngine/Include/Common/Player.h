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

// FILE: Player.h ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    RTS3
//
// File name:  Player.h
//
// Created:    Steven Johnson, October 2001
//
// Desc:			 @todo
//
//-----------------------------------------------------------------------------

#pragma once

#include "Common/AcademyStats.h"
#include "Common/Debug.h"
#include "Common/Energy.h"
#include "Common/GameType.h"
#include "Common/Handicap.h"
#include "Common/KindOf.h"
#include "Common/MissionStats.h"
#include "Common/Money.h"
#include "Common/Science.h"
#include "Common/UnicodeString.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Thing.h"
#include "Common/STLTypedefs.h"
#include "Common/ScoreKeeper.h"
#include "Common/Team.h"
#include "Common/Upgrade.h"

// ----------------------------------------------------------------------------------------------

class BuildListInfo;
class PolygonTrigger;
class ThingTemplate;
class AIPlayer;
class AIGroup;
class GameMessage;
class ResourceGatheringManager;
class PlayerTemplate;
class Squad;
class Team;
class TeamPrototype;
class TeamRelationMap;
class TunnelTracker;
class Upgrade;
class UpgradeTemplate;
class SpecialPowerModule;

struct BattlePlanBonusesData;
class BattlePlanBonuses;

enum BattlePlanStatus CPP_11(: Int);
enum UpgradeStatusType CPP_11(: Int);
enum CommandSourceType CPP_11(: Int);

enum ScienceAvailabilityType CPP_11(: Int)
{
	SCIENCE_AVAILABILITY_INVALID = -1,

	SCIENCE_AVAILABLE,
	SCIENCE_DISABLED,
	SCIENCE_HIDDEN,

	SCIENCE_AVAILABILITY_COUNT,
};

#ifdef DEFINE_SCIENCE_AVAILABILITY_NAMES
static const char *const ScienceAvailabilityNames[] =
{
	"Available",
	"Disabled",
	"Hidden",
	nullptr
};
static_assert(ARRAY_SIZE(ScienceAvailabilityNames) == SCIENCE_AVAILABILITY_COUNT + 1, "Incorrect array size");
#endif	// end DEFINE_SCIENCE_AVAILABILITY_NAMES

static const Int NUM_HOTKEY_SQUADS = 10;

enum { NO_HOTKEY_SQUAD = -1 };

// ------------------------------------------------------------------------------------------------
typedef Int PlayerIndex;
#define PLAYER_INDEX_INVALID -1

// ------------------------------------------------------------------------------------------------
class KindOfPercentProductionChange : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(KindOfPercentProductionChange, "KindOfPercentProductionChange")
public:
	KindOfMaskType		m_kindOf;
	Real							m_percent;
	UnsignedInt				m_ref;
};
EMPTY_DTOR(KindOfPercentProductionChange)

// ------------------------------------------------------------------------------------------------
struct SpecialPowerReadyTimerType
{
	SpecialPowerReadyTimerType()
	{
		clear();
	}
	void clear()
	{
		m_readyFrame = 0xffffffff;
		m_templateID = INVALID_ID;
	};

	UnsignedInt m_templateID;   ///< The ID of the specialpower template associated with this timer... no dupes allowed
	UnsignedInt m_readyFrame;   ///< on what game frame will this special power be ready?
};



// ------------------------------------------------------------------------------------------------
typedef std::hash_map< PlayerIndex, Relationship, std::hash<PlayerIndex>, std::equal_to<PlayerIndex>/**/> PlayerRelationMapType;
class PlayerRelationMap : public MemoryPoolObject,
													public Snapshot
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PlayerRelationMap, "PlayerRelationMapPool" )

public:

	PlayerRelationMap();
	// virtual destructor provided by memory pool object

	/** @todo I'm just wrapping this up in a nice snapshot object, we really should isolate
		* m_map from public access and make access methods for our operations */
	PlayerRelationMapType m_map;

protected:

	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

};

// ----------------------------------------------------------------------------------------------
/**
	A "Player" as an entity that contains the persistent info of the Player, as well as containing
	transient mission data.

	Some of the Player's attributes persist between missions, whereas others are "transient" and only
	have meaning in a mission, wherein they change a lot (current tech tree state, current buildings
	built, units trained, money, etc). (For clarity of nomenclature, we'll refer to all "persistent"
	vs "non-persistent" attributes, since "transient" has an unclear duration.)

	A "Player" consists of an entity controlling a single set of units in a mission.
	A Player may be human or computer controlled.

	All Players have a (transient) "Player Index" associated which allows us to do some shorthand for
	representing Players in some cases (mainly in bitfields). This is always a 0-based
	number (-1 is used as an illegal index). Note that a logical conclusion of this
	is that it makes it desirable to limit the maximum number of simultaneous Players
	to 32, but that seems quite reasonable.

	All persistent data associated with the Player will be saved to local storage and/or
	synchronized with one of our servers for online play.

	Note, simple accessor methods are omitted for brevity... for now, you may infer
	them from the data structures.

	Note also that the class (and the relevant subclasses) are almost certainly incomplete;
	a short perusal of the RA2 equivalent class(es) have waaay more tweaky data fields,
	some of which will probably be necessary (in some form, at some point), some of
	which won't ever be necessary. Rather than trying to guess at all of them here,
	we're limiting the initial setup to be the almost-certainly-necessary, filling in
	the extra things as needed. (Note that most of these will be non-persistent!)
*/
class Player : public Snapshot
{
public:

	Player( Int playerIndex );
	virtual ~Player();

	void update();			///< player update opportunity

	void newMap();			///< player after map loaded opportunity

	void init(const PlayerTemplate* pt);
	void initFromDict(const Dict* d);
	void setDefaultTeam();

	void deletePlayerAI();

	UnicodeString getPlayerDisplayName() { return m_playerDisplayName; }
	// GeneralsX @bugfix felipebraz 11/05/2026 Expose the raw ASCII player name so lookup fallbacks can match qualified skirmish owners.
	AsciiString getPlayerName() const { return m_playerName; }
	NameKeyType getPlayerNameKey() const { return m_playerNameKey; }

	AsciiString getSide() const { return m_side; }

	const PlayerTemplate* getPlayerTemplate() const { return m_playerTemplate;	}
	/// return the Player's Handicap sub-object
	const Handicap *getHandicap() const { return &m_handicap; }
	Handicap *getHandicap() { return &m_handicap; }

	/// return the Player's Money sub-object
	Money *getMoney() { return &m_money; }
	const Money *getMoney() const { return &m_money; }

	UnsignedInt getSupplyBoxValue();///< Many things can affect the alue of a crate, but at heart it is a GlobalData ratio.

	Energy *getEnergy() { return &m_energy; }
	const Energy *getEnergy() const { return &m_energy; }

	// adds a power bonus to this player because of energy upgrade at his power plants
	void addPowerBonus(Object *obj) { m_energy.addPowerBonus(obj); }
	void removePowerBonus(Object *obj) { m_energy.removePowerBonus(obj); }

	ResourceGatheringManager *getResourceGatheringManager(){ return m_resourceGatheringManager; }
	TunnelTracker* getTunnelSystem(){ return m_tunnelSystem; }

	Color getPlayerColor() const { return m_color; }
	Color getPlayerNightColor() const { return m_nightColor;}
	/// return the type of controller
	PlayerType getPlayerType() const { return m_playerType; }
	void setPlayerType(PlayerType t, Bool skirmish);

	PlayerIndex getPlayerIndex() const { return m_playerIndex; }

	/// return a bitmask that is unique to this player.
	PlayerMaskType getPlayerMask() const { return 1 << m_playerIndex; }

	/// a convenience function to test the ThingTemplate against the players canBuild flags
	/// called by canBuild
	Bool allowedToBuild(const ThingTemplate *tmplate) const;

	/// true if we have the prereqs for this item
	Bool canBuild(const ThingTemplate *tmplate) const;

	// Can we afford to build?
	Bool canAffordBuild( const ThingTemplate *whatToBuild ) const;

	/// Difficulty level for this player.
	GameDifficulty getPlayerDifficulty() const;

	/** return the player's command center. (must be one of his "normal" ones,
			not a captured one.)
			if he has none, return null.
			if he has multiple, return one arbitrarily. */
	Object* findNaturalCommandCenter();

	Object* findAnyExistingObjectWithThingTemplate( const ThingTemplate *thing );

	// Finds a short-cut firing special power of specified type returning the first ready power or
	// the most ready if none ready.
	Object* findMostReadyShortcutSpecialPowerOfType( SpecialPowerType spType );

	//Find specified thing template's most ready weapon.
	Object* findMostReadyShortcutWeaponForThing( const ThingTemplate *thing, UnsignedInt &mostReadyPercentage );
	Object* findMostReadyShortcutSpecialPowerForThing( const ThingTemplate *thing, UnsignedInt &mostReadyPercentage );

	// Finds a short-cut firing special power of any type arbitrarily.
	Bool hasAnyShortcutSpecialPower();

	// Counts available shortcut special power of specified type that can fire now.
	Int countReadyShortcutSpecialPowersOfType( SpecialPowerType spType );

	/// return t if the player has the given science, either intrinsically, via specialization, or via capture.
	Bool hasScience(ScienceType t) const;
	Bool isScienceDisabled( ScienceType t ) const;	///< Can't purchase this science because of script reasons.
	Bool isScienceHidden( ScienceType t ) const;		///< This science is effectively hidden due to  script reasons.

	//Allows scripts to make specific sciences available, hidden, or disabled.
	void setScienceAvailability( ScienceType science, ScienceAvailabilityType type );

	ScienceAvailabilityType getScienceAvailabilityTypeFromString( const AsciiString& name );

	/// return t iff the player has all sciences that are prereqs for knowing the given science
	Bool hasPrereqsForScience(ScienceType t) const;

	Bool hasUpgradeComplete( const UpgradeTemplate *upgradeTemplate ) const;		///< does player have totally done and produced upgrade
	Bool hasUpgradeComplete( const UpgradeMaskType& testMask ) const;		///< does player have totally done and produced upgrade
	const UpgradeMaskType& getCompletedUpgradeMask() const { return m_upgradesCompleted; }	///< get list of upgrades that are completed
	Bool hasUpgradeInProduction( const UpgradeTemplate *upgradeTemplate );		///< does player have this upgrade in progress right now
	Upgrade *addUpgrade( const UpgradeTemplate *upgradeTemplate,
											 UpgradeStatusType status );		///< add upgrade, or update existing upgrade status
	void removeUpgrade( const UpgradeTemplate *upgradeTemplate );	///< remove this upgrade from us

	/** find upgrade, NOTE, the upgrade may *NOT* be "complete" and therefore "active", it could be in production
	  This function is for actually retrieving the Upgrade.  To test existence, use the fast bit testers hasUpgradeX()
	*/
	Upgrade *findUpgrade( const UpgradeTemplate *upgradeTemplate );

	void onUpgradeCompleted( const UpgradeTemplate *upgradeTemplate );				///< An upgrade just finished, do things like tell all objects to recheck UpgradeModules
	void onUpgradeRemoved(){}					///< An upgrade just got removed, this doesn't do anything now.

#if defined(RTS_DEBUG)
	/// Prereq disabling cheat key
	void toggleIgnorePrereqs(){ m_DEMO_ignorePrereqs = !m_DEMO_ignorePrereqs; }
	void enableIgnorePrereqs(Bool enable) { m_DEMO_ignorePrereqs = enable; }
	Bool ignoresPrereqs() const { return m_DEMO_ignorePrereqs; }

	/// No cost building cheat key
	void toggleFreeBuild(){ m_DEMO_freeBuild = !m_DEMO_freeBuild; }
	void enableFreeBuild(Bool enable) { m_DEMO_freeBuild = enable; }
	Bool buildsForFree() const { return m_DEMO_freeBuild; }

	/// No time building cheat key
	void toggleInstantBuild(){ m_DEMO_instantBuild = !m_DEMO_instantBuild; }
	void enableInstantBuild(Bool enable) { m_DEMO_instantBuild = enable; }
	Bool buildsInstantly() const { return m_DEMO_instantBuild; }
#endif

	///< Power just changed at all.  Didn't make two functions so you can't forget to undo something you didin one of them.
	///< @todo Can't do edge trigger until after demo; make things check for power on creation
	void onPowerBrownOutChange( Bool brownOut );


	///< one of my command centers just fired a special power, let us reset timers for all command centers.
	void resetOrStartSpecialPowerReadyFrame( const SpecialPowerTemplate *temp );
	///< my new command center wants to init his timers to the status quo
	UnsignedInt getOrStartSpecialPowerReadyFrame( const SpecialPowerTemplate *temp);
	void expressSpecialPowerReadyFrame( const SpecialPowerTemplate *temp, UnsignedInt frame );
	void addNewSharedSpecialPowerTimer( const SpecialPowerTemplate *temp, UnsignedInt frame );


	void addRadar( Bool disableProof );///< One more producer of Radar exists
	void removeRadar( Bool disableProof );///< One less thing produces radar
	void disableRadar();	///< No matter how many radar producers I have, I do not have radar
	void enableRadar();	///< remove the restriction imposed by disableRadar
	Bool hasRadar() const;///< A positive number of radar producers, plus my radar is not disabled
	Bool okToPlayRadarEdgeSound();///< just like it "sounds"
	//Battle plans effect the players abilities... so may as well add it here. Also
	//it's possible for multiple strategy centers to have the same plan, so we need
	//to keep track of that like radar. Keep in mind multiple strategy centers with
	//same plan do not stack, but different strategy centers with different plans do.
	void changeBattlePlan( BattlePlanStatus plan, Int delta, const BattlePlanBonusesData *bonus );
	Int getNumBattlePlansActive() const { return m_bombardBattlePlans + m_holdTheLineBattlePlans + m_searchAndDestroyBattlePlans; }
	Int getBattlePlansActiveSpecific( BattlePlanStatus plan ) const;
	void applyBattlePlanBonusesForObject( Object *obj ) const;	//New object or converted object gaining our current battle plan bonuses.
	void removeBattlePlanBonusesForObject( Object *obj ) const; //Object left team
	void applyBattlePlanBonusesForPlayerObjects( const BattlePlanBonusesData *bonus ); //Battle plan bonuses changing, so apply to all of our objects!
	Bool doesObjectQualifyForBattlePlan( Object *obj ) const;

	// If apply is false, then we are repealing already granted bonuses.
	// Note: Should only be called by Object.
	void friend_applyDifficultyBonusesForObject(Object* obj, Bool apply) const;

	/// Decrement the ref counter on the typeof production list node
	void removeKindOfProductionCostChange(KindOfMaskType kindOf, Real percent);
	/// add type of production cost change (Used for upgrades)
	void addKindOfProductionCostChange( KindOfMaskType kindOf, Real percent);
	/// Returns production cost change based on typeof (Used for upgrades)
	Real getProductionCostChangeBasedOnKindOf( KindOfMaskType kindOf ) const;

	/** Return bonus or penalty for construction of this thing.
	*/
	Real getProductionCostChangePercent( AsciiString buildTemplateName ) const;

	/** Return bonus or penalty for construction of this thing.
	*/
	Real getProductionTimeChangePercent( AsciiString buildTemplateName ) const;

	/** Return starting veterancy level of a newly built thing of this type
	*/
	VeterancyLevel getProductionVeterancyLevel( AsciiString buildTemplateName ) const;

	// Friend function for the script engine's usage.
	void friend_setSkillset(Int skillSet);

	/**
		the given object has just become (or just ceased to be) a member of one of our teams (or subteams)
	*/
	void becomingTeamMember(Object *obj, Bool yes);

	/**
		this is called when the player becomes the local player (yes==true)
		or ceases to be the local player (yes==false). you can't stop this
		from happening; you can only react to it.
	*/
	void becomingLocalPlayer(Bool yes);

	/// Is this player the local player?
	Bool isLocalPlayer() const;

	/// this player should be listed in the score screen.
	void setListInScoreScreen(Bool listInScoreScreen);

	/// return TRUE if this player should be listed in the score screen.
	Bool getListInScoreScreen();

	/// A unit was just created and is ready to control
	void onUnitCreated( Object *factory, Object *unit );

	/// A team is about to be destroyed.
	void preTeamDestroy( const Team *team );

	/// Is the nearest supply source safe?
	Bool isSupplySourceSafe( Int minSupplies );

	/// Is a supply source attacked?
	Bool isSupplySourceAttacked();

	/// Set delay between team production.
	void setTeamDelaySeconds(Int delay);

	/// Have the team guard a supply center.
	void guardSupplyCenter( Team *team, Int minSupplies );

	virtual void computeSuperweaponTarget(const SpecialPowerTemplate *power, Coord3D *pos, Int playerNdx, Real weaponRadius); ///< Calculates best pos for weapon given radius.

	/// Get the enemy an ai player is currently focused on.  NOTE - Can be nullptr.
	Player  *getCurrentEnemy();

	/// Is this player a skirmish ai player?
	Bool isSkirmishAIPlayer();

	/// Have the ai check for bridges.
	virtual Bool checkBridges(Object *unit, Waypoint *way);

	/// Get the center of the ai's base.
	virtual Bool getAiBaseCenter(Coord3D *pos);

	/// Have the ai check for bridges.
	virtual void repairStructure(ObjectID structureID);

	/// a structure was just created, but is under construction
	void onStructureCreated( Object *builder, Object *structure );

	/// a structure that was under construction has become completed
	void onStructureConstructionComplete( Object *builder, Object *structure, Bool isRebuild );

	/// a structure that was created has been pre-emptively removed and shouldn't be counted in the score.
	void onStructureUndone(Object *structure);

	/**
		a convenience routine to count the number of owned objects that match a set of ThingTemplates.
		You input the count and an array of ThingTemplate*, and provide an array of Int of the same
		size. It fills in the array to the correct counts. This is handy because we must traverse
		the player's list-of-objects only once.
	*/
	void countObjectsByThingTemplate(Int numTmplates, const ThingTemplate* const * things, Bool ignoreDead, Int *counts, Bool ignoreUnderConstruction = TRUE ) const;

	/**
		simply returns the number of buildings owned by this player
	*/
	Int countBuildings();

	/**
		simply returns the number of objects owned by this player with a specific KindOfMaskType
	*/
	Int countObjects(KindOfMaskType setMask, KindOfMaskType clearMask);

	/// Returns the closest of a given type to the given object
	Object *findClosestByKindOf( Object *queryObject, KindOfMaskType setMask, KindOfMaskType clearMask );

	/**
		a convenience routine to quickly check if any buildings are owned.
	*/
	Bool hasAnyBuildings() const;

	/**
		a convenience routine to quickly check if any buildings with a specific KindOfType flag are owned.
	*/
	Bool hasAnyBuildings(KindOfMaskType kindOf) const;

	/**
		a convenience routine to quickly check if any units are owned.
	*/
	Bool hasAnyUnits() const;

	/**
		a convenience routine to quickly check if any objects are owned.
	*/
	Bool hasAnyObjects() const;

	/**
		a convenience routine to quickly check if any buildfacilities are owned.
	*/
	Bool hasAnyBuildFacility() const;

	/**
		a convenience routine to quickly update the state flags on all teams.
	*/
	void updateTeamStates();

	/**
		This player will heal everything owned by it
	*/
	void healAllObjects();

	/**
		* Iterate all objects that this player has
		*/
	void iterateObjects( ObjectIterateFunc func, void *userData ) const;

	/**
		return this player's "default" team.
	*/
	Team *getDefaultTeam() { DEBUG_ASSERTCRASH(m_defaultTeam!=nullptr,("default team is null")); return m_defaultTeam; }
	const Team *getDefaultTeam() const { DEBUG_ASSERTCRASH(m_defaultTeam!=nullptr,("default team is null")); return m_defaultTeam; }

	void setBuildList(BuildListInfo *pBuildList);			///< sets the build list.
	BuildListInfo *getBuildList() { return m_pBuildList; }		///< returns the build list. (build list might be modified by the solo AI)
	void addToBuildList(Object *obj);			///< Adds this to the build list.	 Used for factories placed instead of in build list.
	void addToPriorityBuildList(AsciiString templateName, Coord3D *pos, Real angle);			///< Adds this to the build list.	 Used for factories placed instead of in build list.

	/// get the relationship between this->that.
	Relationship getRelationship(const Team *that) const;

	/// set the relationship between this->that. (note that this doesn't affect the that->this relationship.)
	void setPlayerRelationship(const Player *that, Relationship r);
	Bool removePlayerRelationship(const Player *that);
	void setTeamRelationship(const Team *that, Relationship r);
	Bool removeTeamRelationship(const Team *that);

	void addTeamToList(TeamPrototype* team);
	void removeTeamFromList(TeamPrototype* team);

	typedef std::list<TeamPrototype*> PlayerTeamList;
	const PlayerTeamList* getPlayerTeams() const { return &m_playerTeamPrototypes; }

	Int getMpStartIndex() {return m_mpStartIndex;}

	/// Set that all units should begin hunting.
	void setUnitsShouldHunt(Bool unitsShouldHunt, CommandSourceType source);
	Bool getUnitsShouldHunt() const { return m_unitsShouldHunt; }

	/// All of our units are new spied upon; they sight for the given enemy
	void setUnitsVisionSpied( Bool setting, PlayerIndex byWhom );

	/// This will be asked by units when they look to get extra people to sight for
	PlayerMaskType getVisionSpiedMask() const;

	/// Destroy all of the teams for this player, causing him to DIE.
	void killPlayer();

	/// Enabled/Disable all objects of type templateTypeToAffect
	void setObjectsEnabled(AsciiString templateTypeToAffect, Bool enable);

	/// Build an instance of a specific team.  Gets passed to aiPlayer.
	void buildSpecificTeam(TeamPrototype *teamProto);

	/// Build an instance of a specific building.  Gets passed to aiPlayer.
	void buildSpecificBuilding(const AsciiString &thingName);

	/// Build a building near a supply dump with at least cash.  Gets passed to aiPlayer.
	void buildBySupplies(Int minimumCash, const AsciiString &thingName);

	/// Build an upgrade.  Gets passed to aiPlayer.
	void buildUpgrade(const AsciiString &upgrade);

	/// Build base defense on front or flank of base.  Gets passed to aiPlayer.
	void buildBaseDefense(Bool flank);

	/// Build structure type on front or flank of base.  Gets passed to aiPlayer.
	void buildBaseDefenseStructure(const AsciiString &thingName, Bool flank);

	/// Recruits an instance of a specific team.  Gets passed to aiPlayer.
	void recruitSpecificTeam(TeamPrototype *teamProto, Real recruitRadius);

	/// Enable/Disable the construction of units
	Bool getCanBuildUnits() { return m_canBuildUnits; }
	void setCanBuildUnits(Bool canProduce) { m_canBuildUnits = canProduce; }

	/// Enable/Disable the construction of base buildings.
	Bool getCanBuildBase() { return m_canBuildBase; }
	void setCanBuildBase(Bool canProduce) { m_canBuildBase = canProduce; }

	/// Transfer all assets from player that to this
	void transferAssetsFromThat(Player* that);

	/// Sell everything this player owns.
	void sellEverythingUnderTheSun();

	void garrisonAllUnits(CommandSourceType source);
	void ungarrisonAllUnits(CommandSourceType source);

	void setUnitsShouldIdleOrResume(Bool idle);

	Bool isPlayableSide() const;

	Bool isPlayerObserver() const; // Favor !isPlayerActive() - this is used for Observer GUI mostly, not in-game stuff
	Bool isPlayerDead() const; // Favor !isPlayerActive() - this is used so OCLs don't give us stuff after death.
	Bool isPlayerActive() const; // Player is alive and not observer. !isPlayerActive() is synonymous with observing.

	Bool didPlayerPreorder() const { return m_isPreorder; }

	/// Grab the scorekeeper so we can score up in here!
	ScoreKeeper* getScoreKeeper() { return &m_scoreKeeper; }

	/// time to create a hotkey team based on this GameMessage
	void processCreateTeamGameMessage(Int hotkeyNum, const GameMessage *msg);

	/// time to select a hotkey team based on this GameMessage
	void processSelectTeamGameMessage(Int hotkeyNum);

	// add to the player's current selection this hotkey team.
	void processAddTeamGameMessage(Int hotkeyNum);

	// fills an AIGroup object that is the currently selected group.
	void getCurrentSelectionAsAIGroup(AIGroup *group);

	// sets the currently selected group to be the given AIGroup
	void setCurrentlySelectedAIGroup(AIGroup *group);

	// adds the given AIGroup to the current selection of this player.
	void addAIGroupToCurrentSelection(AIGroup *group);

	// return the requested hotkey squad
	Squad *getHotkeySquad(Int squadNumber);

	// return the hotkey squad that a unit is in, or NO_HOTKEY_SQUAD if it isn't in one.
	Int getSquadNumberForObject(const Object *objToFind) const;

	// remove an object from any hotkey squads that its in.
	void removeObjectFromHotkeySquad(Object *objToRemove);

	void setAttackedBy( Int playerNdx );
	Bool getAttackedBy( Int playerNdx ) const;
	UnsignedInt getAttackedFrame() {return m_attackedFrame;}  // Return last frame attacked.

	Real getCashBounty() const { return m_cashBountyPercent; }
	void setCashBounty(Real percentage) { m_cashBountyPercent = percentage; }
	void doBountyForKill(const Object* killer, const Object* victim);

	AcademyStats* getAcademyStats() { return &m_academyStats; }
	const AcademyStats* getAcademyStats() const { return &m_academyStats; }

	//Set via logical message. Options menu sets the client value in global data. Player::update()
	//detects a change, and posts a message. When the message gets processed, this value gets set.
	Bool isLogicalRetaliationModeEnabled() const { return m_logicalRetaliationModeEnabled; }
	void setLogicalRetaliationModeEnabled( Bool set ) { m_logicalRetaliationModeEnabled = set; }

private:

	/** give the science. doesn't check prereqs, nor charge to purchase points...
	 just grants it! so be careful! NOTE that this is private to Player,
	 and for a good reason; if public, this could be used to grant sciences
	 that shouldn't be directly granted (e.g., SCIENCE_RankX, which should
	 be managed via setRankLevel instead). If you think you need to use
	 this call externally, you probably don't... you should probably be
	 using grantScience() instead.
	*/
	Bool addScience(ScienceType science);

public:
	Int getSkillPoints() const						{ return m_skillPoints; }
	Int getSciencePurchasePoints() const	{ return m_sciencePurchasePoints; }
	Int getRankLevel() const							{ return m_rankLevel; }
	Int getSkillPointsLevelUp() const			{ return m_levelUp;	}
	Int getSkillPointsLevelDown() const			{ return m_levelDown;	}
	UnicodeString getGeneralName() const	{ return m_generalName; }
	void setGeneralName( UnicodeString name ){ m_generalName = name;	}
	/// returns TRUE if rank level really changed.
	Bool setRankLevel(Int level);

	/// returns TRUE if the player gained/lost levels as a result.
	Bool addSkillPoints(Int delta);

	/// returns TRUE if the player gained/lost levels as a result.
	Bool addSkillPointsForKill(const Object* killer, const Object* victim);

	void addSciencePurchasePoints(Int delta);

	void setSkillPointsModifier(Real expMod) { m_skillPointsModifier = expMod; }
	Real getSkillPointsModifier() const { return m_skillPointsModifier; }

	/// reset the sciences to just the intrinsic ones from the playertemplate, if any.
	void resetSciences();

	/// reset rank to 1.
	void resetRank();

	/**
		attempt to purchase the science, but use prereqs, and charge points.
		return true if successful.
	*/
	Bool attemptToPurchaseScience(ScienceType science);

	/**
		grant the science, ignore prereqs & charge no points,
		but still restrict you to purchasable sciences (ie, intrinsic and rank sciences
		are not allowed)
		return true if successful.
	*/
	Bool grantScience(ScienceType science);

	/** return true attemptToPurchaseScience() would succeed for this science. */
	Bool isCapableOfPurchasingScience(ScienceType science) const;

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

	void deleteUpgradeList();															///< delete all our upgrades

private:

	const PlayerTemplate*				m_playerTemplate;			///< Pointer back to the Player Template

	UnicodeString								m_playerDisplayName;					///< This player's persistent name.
	Handicap										m_handicap;										///< adjustment to varied capabilities (@todo: is this persistent or recalced each time?)
	AsciiString									m_playerName;									///< player's internal name (for matching map objects)
	NameKeyType									m_playerNameKey;							///< This player's internal name (for matching map objects)
	PlayerIndex									m_playerIndex;								///< player unique index.
	AsciiString									m_side;												///< the "side" this player is on
	PlayerType									m_playerType;									///< human/computer control
	Money												m_money;											///< Player's current wealth
	Upgrade*										m_upgradeList;								///< list of all upgrades this player has
	Int													m_radarCount;									///< # of facilities that have a radar under the players control
	Int													m_disableProofRadarCount;			///< # of disable proof radars.  A disable proof one will be in both refcounts
	Bool												m_radarDisabled;							///< The radar is disabled regardless of the number of radar objects
	Int													m_bombardBattlePlans;					///< Number of strategy centers with active bombardment plan
	Int													m_holdTheLineBattlePlans;			///< Number of strategy centers with active hold the line plan
	Int													m_searchAndDestroyBattlePlans;///< Number of strategy centers with active search and destroy plan
	BattlePlanBonuses*					m_battlePlanBonuses;
	UpgradeMaskType							m_upgradesInProgress;					///< Bit field of in Production status upgrades
	UpgradeMaskType							m_upgradesCompleted;					///< Bit field of upgrades completed.  Bits are assigned by UpgradeCenter
	Energy											m_energy;											///< current energy production & consumption
	MissionStats								m_stats;											///< stats about the current mission (units destroyed, etc)
	BuildListInfo*							m_pBuildList;									///< linked list of buildings for PLAYER_COMPUTER.
	Color												m_color;											///< color for our units
	Color												m_nightColor;	///<tweaked version of regular color to make it easier to see on night maps.
	ProductionChangeMap					m_productionCostChanges;			///< Map to keep track of Faction specific discounts or penalties on prices of units
	ProductionChangeMap					m_productionTimeChanges;			///< Map to keep track of Faction specific discounts or penalties on build times of units
	ProductionVeterancyMap			m_productionVeterancyLevels;	///< Map to keep track of starting level of produced units
	AIPlayer*										m_ai;													///< if PLAYER_COMPUTER, the entity that does the thinking
	Int													m_mpStartIndex;								///< The player's starting index for multiplayer.
	ResourceGatheringManager*		m_resourceGatheringManager;		///< Keeps track of all Supply Centers and Warehouses
	TunnelTracker*							m_tunnelSystem;								///< All TunnelContain buildings use this part of me for actual containment
	Team*												m_defaultTeam;								///< our "default" team.

	ScienceVec					m_sciences;					///< (SAVE) sciences that we know (either intrinsically or via later purchases)
	ScienceVec					m_sciencesDisabled;	///< (SAVE) sciences that we are not permitted to purchase "yet". Controlled by mission scripts.
	ScienceVec					m_sciencesHidden;		///< (SAVE) sciences that aren't shown. Controlled by mission scripts.

	Int									m_rankLevel;			///< (SAVE) our RankLevel, 1...n
	Int									m_skillPoints;		///< (SAVE) our cumulative SkillPoint total
	Int									m_sciencePurchasePoints;		///< (SAVE) our unspent SciencePurchasePoint total
	Int									m_levelUp, m_levelDown;			///< (NO-SAVE) skill points to go up/down a level (runtime only)
	UnicodeString				m_generalName;		///< (SAVE) This is the name of the general the player is allowed to change.

	PlayerTeamList				m_playerTeamPrototypes;				///< ALL the teams we control, via prototype
	PlayerRelationMap			*m_playerRelations;						///< allies & enemies
	TeamRelationMap				*m_teamRelations;							///< allies & enemies

	AcademyStats					m_academyStats;				///< Keeps track of various statistics in order to provide advice to the player about how to improve playing.

	Bool									m_canBuildUnits;		///< whether the current player is allowed to build units
	Bool									m_canBuildBase;			///< whether the current player is allowed to build Base buildings
	Bool									m_observer;
	Bool									m_isPreorder;
	Real									m_skillPointsModifier;	///< Multiplied by skill points before they are applied

	Bool									m_listInScoreScreen;	///< should this player be listed in the score screen or not.
	Bool									m_unitsShouldHunt;

	Bool									m_attackedBy[MAX_PLAYER_COUNT];	///< For each player, have they attacked me?
	UnsignedInt						m_attackedFrame;	///< Last frame attacked.
	Int										m_visionSpiedBy[MAX_PLAYER_COUNT];  ///< Reference count of having units spied on by players.
	PlayerMaskType				m_visionSpiedMask;	///< For quick lookup and edge triggered maintenance

	Real									m_cashBountyPercent;

	/// @todo REMOVE (not disable) these cheat keys
#if defined(RTS_DEBUG)
	Bool									m_DEMO_ignorePrereqs;		///< Can I ignore prereq checks?
	Bool									m_DEMO_freeBuild;				///< Can I build everything for no money?
	Bool									m_DEMO_instantBuild;		///< Can I build anything in one frame?
#endif

	ScoreKeeper						m_scoreKeeper;					///< The local scorekeeper for this player

	typedef std::list<KindOfPercentProductionChange*> KindOfPercentProductionChangeList;
	typedef KindOfPercentProductionChangeList::iterator KindOfPercentProductionChangeListIt;
	mutable KindOfPercentProductionChangeList m_kindOfPercentProductionChangeList;


	typedef std::list<SpecialPowerReadyTimerType> SpecialPowerReadyTimerList;
	typedef SpecialPowerReadyTimerList::iterator SpecialPowerReadyTimerListIterator;
	SpecialPowerReadyTimerList m_specialPowerReadyTimerList;

	Squad									*m_squads[NUM_HOTKEY_SQUADS];	///< The hotkeyed squads
	Squad									*m_currentSelection;		///< This player's currently selected group

	Bool									m_isPlayerDead;
	Bool									m_logicalRetaliationModeEnabled;
};
