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

// FILE: Team.h ///////////////////////////////////////////////////////////////
// Team interface definition
// Author: Michael S. Booth, Steven Johnson, March 2001

#pragma once

#include "Common/GameType.h"
#include "Common/Snapshot.h"
#include "Common/Thing.h"
#include "GameLogic/Object.h"

// ------------------------------------------------------------------------------------------------
typedef UnsignedInt TeamID;
#define TEAM_ID_INVALID 0

// ------------------------------------------------------------------------------------------------
typedef UnsignedInt TeamPrototypeID;
#define TEAM_PROTOTYPE_ID_INVALID 0

// ------------------------------------------------------------------------------------------------
typedef std::hash_map< TeamID, Relationship, std::hash<TeamID>, std::equal_to<TeamID>/**/> TeamRelationMapType;
class TeamRelationMap : public MemoryPoolObject,
												public Snapshot
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TeamRelationMap, "TeamRelationMapPool" )

public:

	TeamRelationMap();
	// virtual destructor provided by memory pool object

	/** @todo I'm just wrapping this up in a nice snapshot object, we really should isolate
		* m_map from public access and make access methods for our operations */
	TeamRelationMapType m_map;

protected:

	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

};

// ------------------------------------------------------------------------------------------------
typedef void (*ObjectIterateFunc)( Object *obj, void *userData );		///< callback type for iterating objects

// ------------------------------------------------------------------------
/**
	How are teams represented in mapfiles?

	-- All Players always have at least one top-level team, with the name "team<playername>"
		(WB will enforce this in mapfiles.)
	-- We'll have a TeamDict list that is parallel to the PlayerDict list
	-- All Teams (toplevel and subteams) are in the list
	-- Each TeamDict contains:
		-- Team Name (an internal ascii str)
		-- Owning Team (or Player), by Name
		-- Misc Flags
		-- Teams we are allies with, by name (only for toplevel teams)
		-- Teams we are enemies with, by name (only for toplevel teams)

	-- Note that Players and Teams share the same namespace, so you cannot have a team
		with the same name as a player (or vice versa).

	-- MapObject Dicts will be revised to have an "owning team" rather than "owning player" entry

	-- BuildLists must also have a way to assign units to Teams, as appropriate

*/

// ------------------------------------------------------------------------
class AIGroup;
class Dict;
class Player;
class PolygonTrigger;
class Script;
class SidesList;
class TeamFactory;
class TeamPrototype;
class Team;
class ThingTemplate;
class Waypoint;
class PlayerRelationMap;

enum AttitudeType CPP_11(: Int);

typedef struct {
	Int	minUnits;
	Int	maxUnits;
	AsciiString unitThingName;
} TCreateUnitsInfo;

enum { MAX_GENERIC_SCRIPTS = 16 };

// ------------------------------------------------------------------------
/// This is the info for creating reinforcement and AI teams.
class TeamTemplateInfo : public Snapshot
{

public:

	TeamTemplateInfo(Dict *d);

	enum {MAX_UNIT_TYPES = 7};
	typedef enum {NORMAL=0, IGNORE_DISTRACTIONS=1, DEAL_AGGRESSIVELY=2} TBehavior;

	TCreateUnitsInfo	m_unitsInfo[MAX_UNIT_TYPES];	///< Quantity of units to create or build.
	Int								m_numUnitsInfo;								///< Number of entries in m_unitsInfo
	Coord3D						m_homeLocation;								///< Spawn location for team.
	Bool							m_hasHomeLocation;						///< True is m_homeLocation is valid.
	AsciiString				m_scriptOnCreate;							///< Script executed when team is created.
	AsciiString				m_scriptOnIdle;								///< Script executed when team is idle.
	Int								m_initialIdleFrames;					///< Number of frames to continue recruiting after the minimum team size is achieved.
	AsciiString				m_scriptOnEnemySighted;				///< Script executed when enemy is sighted.
	AsciiString				m_scriptOnAllClear;						///< Script executed when enemy is sighted.
	AsciiString				m_scriptOnUnitDestroyed;			///< Script executed each time a unit on this team dies.
	AsciiString				m_scriptOnDestroyed;					///< Script executed m_destroyedThreshold of member units are destroyed.
	Real							m_destroyedThreshold;					///< OnDestroyed threshold - 1.0 = 100% = all destroyed, .5 = 50% = half of the units destroyed, 0 = useless.
	Bool							m_isAIRecruitable;						///< True if other ai teams can recruit.
	Bool							m_isBaseDefense;							///< True if is base defense team.
	Bool							m_isPerimeterDefense;					///< True if is a perimeter base defense team.
	Bool							m_automaticallyReinforce;			///< True is team automatically tries to reinforce.
	Bool							m_transportsReturn;						///< True if transports return to base after unloading.
	Bool							m_avoidThreats;								///< True if the team avoids threats.
	Bool							m_attackCommonTarget;					///< True if the team attacks the same target unit.
	Int								m_maxInstances;								///< Maximum number of instances of a team that is not singleton.
	mutable Int				m_productionPriority;					///< Production priority.
	Int								m_productionPrioritySuccessIncrease; ///< Production priority increase on success.
	Int								m_productionPriorityFailureDecrease; ///< Production priority decrease on failure.
	AttitudeType			m_initialTeamAttitude;				///< The initial team attitude

	AsciiString				m_transportUnitType;					///< Unit used to transport the team.
	AsciiString				m_startReinforceWaypoint;			///< Waypoint where the reinforcement team starts.
	Bool							m_teamStartsFull;							///< If true, team loads into member transports.
	Bool							m_transportsExit;							///< True if the transports leave after deploying team.
	VeterancyLevel		m_veterancy;								///< Veterancy level;

	// Production scripts stuff
	AsciiString				m_productionCondition;				///< Script that contains the production conditions.
	Bool							m_executeActions;							///< If this is true, then when the production condition becomes true, we also execute the actions.

	AsciiString				m_teamGenericScripts[MAX_GENERIC_SCRIPTS];
protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

};

// ------------------------------------------------------------------------
class Team : public MemoryPoolObject,
						 public Snapshot
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(Team, "TeamPool" )

private:

	TeamPrototype	*m_proto;							///< the prototype used to create this Team
	TeamID m_id;												///< unique team id

	// lists we own
	/// @todo srj -- convert to non-DLINK list, after it is once again possible to test the change
	MAKE_DLINK_HEAD(Object, TeamMemberList)		///< the members of this team

	// lists we are members of
	/// @todo srj -- convert to non-DLINK list, after it is once again possible to test the change
	MAKE_DLINK(Team, TeamInstanceList)				///< the instances of our prototype

	AsciiString		m_state;						///< Name of the current AI state.

	Bool					m_enteredOrExited;  ///< True if a team member entered or exited a trigger area this frame.
	Bool					m_active;						///< True if a team is complete.  False while members are being added.
	Bool					m_created;					///< True when first activated.

	// Enemy sighted & All Clear:
	Bool					m_checkEnemySighted;///< True if we have an on enemy sighted or all clear script.
	Bool					m_seeEnemy;					///< True if we see an enemy.
	Bool					m_prevSeeEnemy;			///< Last value.

	// Idle flag.
	Bool					m_wasIdle;					///< True if idle last frame.

	// On %Destroyed
	Int					m_destroyThreshold;
	Int					m_curUnits;

	// Following waypoint paths as a team.
	const Waypoint *m_currentWaypoint;

	// Should check/Execute generic script
	Bool					m_shouldAttemptGenericScript[MAX_GENERIC_SCRIPTS];

	// Recruitablity.
	Bool				m_isRecruitablitySet;	///< If false, recruitability is team proto value.  If true, m_isRecruitable.
	Bool				m_isRecruitable;

	// Attack target.
	ObjectID		m_commonAttackTarget;

	TeamRelationMap				*m_teamRelations;									///< override allies & enemies
	PlayerRelationMap			*m_playerRelations;								///< override allies & enemies

	std::list< ObjectID > m_xferMemberIDList;			///< list for post processing and restoring object pointers after a load

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

public:

	Team( TeamPrototype *proto, TeamID id );
	// ~Team();

	/// return the prototype used to create this team
	const TeamPrototype *getPrototype() { return m_proto; }

	void setID( TeamID id ) { m_id = id; }
	TeamID getID() const { return m_id; }

	/**
		 Set the attack priority name for a team.
	*/
	void setAttackPriorityName(AsciiString name);

	/**
		return the player currently controlling this team (backtracking up if necessary)
	*/
	Player *getControllingPlayer() const;

	/**
		set the team's owner. (nullptr is not allowed)
	*/
	void setControllingPlayer(Player *newController);

	/**
		Get the team's state
	*/
	const AsciiString& getState() const {return m_state;}

	/**
		fill pAIGroup (which must be NONNULL) with the members of this team as a Group.
	*/
	void getTeamAsAIGroup(AIGroup *pAIGroup);

	/**
		Get the team's state
	*/
	inline const AsciiString& getName() const;

	/**
		Get the count of live things that are either alive or are buildings.
	*/
	Int getTargetableCount() const;

	/**
		Set the team's AI state.
	*/
	void setState(const AsciiString& state) {m_state = state;}

	/**
		Set the team's AI recruitablity.
	*/
	void setRecruitable(Bool recruitable) {m_isRecruitablitySet = true; m_isRecruitable = recruitable;}

	/**
		Set the team's target object.
	*/
	void setTeamTargetObject(const Object *target) ;

	/**
		Set the team's target object.
	*/
	Object *getTeamTargetObject();

	/**
		Set the team as active.  A team is considered created when set active.
	*/
	void setActive() {if (!m_active) { m_created = true;m_active = true;}}

	/**
		Is this team active?
	*/
	Bool isActive() {return m_active;}

	/**
		Is this team just createc?  (stays true one logic frame.)
	*/
	Bool isCreated() {return m_created;}

	/**
		Note that a team member entered or exited a trigger area.
	*/
	void setEnteredExited() {m_enteredOrExited = true;}

	/**
		Did a team member enter or exit a trigger area.
	*/
	Bool didEnterOrExit() {return m_enteredOrExited;}

	/**
		Clear the flag that a team member entered or exited a trigger area.
		Also checks and executes any onCreate scripts, and clears the created flag.
	*/
	void updateState();

	void notifyTeamOfObjectDeath();

	Bool didAllEnter(PolygonTrigger *pTrigger, UnsignedInt whichToConsider) const;					///< All members entered the area
	Bool didPartialEnter(PolygonTrigger *pTrigger, UnsignedInt whichToConsider) const;			///< One member entered the area
	Bool didAllExit(PolygonTrigger *pTrigger, UnsignedInt whichToConsider) const;					///< All members exited the area
	Bool didPartialExit(PolygonTrigger *pTrigger, UnsignedInt whichToConsider) const;			///< One member exited the area
	Bool allInside(PolygonTrigger *pTrigger, UnsignedInt whichToConsider) const;						///< All members are inside the area
	Bool someInsideSomeOutside(PolygonTrigger *pTrigger, UnsignedInt whichToConsider) const;///< One or more in, one or more out.
	Bool noneInside(PolygonTrigger *pTrigger, UnsignedInt whichToConsider) const;					///< No members are in the area.

	Object * tryToRecruit(const ThingTemplate *, const Coord3D *teamHome, Real maxDist); ///< Try to recruit the closest unit of this thing type.  Return true if successful.

	/**
		return our relationship with the other team.
		this is done as follows:

		-- if there's a TeamRelationship between this team and that team, return it.
		-- otherwise, we use the relationship between our players.

	*/
	Relationship getRelationship(const Team *that) const;

	/**
		set a special relationship between this team and that team, that overrides
		the one between our Players. (note that this doesn't imply anything about the
		relation of that to this.)
	*/
	void setOverrideTeamRelationship( TeamID teamID, Relationship r );

	/**
		remove the special relation (if any) between this and that.
		remove all special relations for 'this' if that==null.
		return true if anything removed.
	*/
	Bool removeOverrideTeamRelationship( TeamID teamID );

	/**
		set a special relationship between this team and that Player, that overrides
		the one between our Players. (note that this doesn't imply anything about the
		relation of that to this.) Note that override-team relationships take precedence
		over override-player relationships.
	*/
	void setOverridePlayerRelationship( Int playerIndex, Relationship r );

	/**
		remove the special relation (if any) between this and that.
		remove all special relations for 'this' if that==null.
		return true if anything removed.
	*/
	Bool removeOverridePlayerRelationship( Int playerIndex );

	/**
		a convenience routine to count the number of owned objects that match a set of ThingTemplates.
		You input the count and an array of ThingTemplate*, and provide an array of Int of the same
		size. It fills in the array to the correct counts. This is handy because we must traverse
		the team's list-of-objects only once.
	*/
	void countObjectsByThingTemplate(Int numTmplates, const ThingTemplate* const* things, Bool ignoreDead, Int *counts, Bool ignoreUnderConstruction = TRUE ) const;


	/**
		returns the number of buildings on this team, by checking each things' template kindof against KINDOF_STRUCTURE
	*/
	Int countBuildings();

	/**
		simply returns the number of objects on this team with a specific KindOfMaskType
	*/
	Int countObjects(KindOfMaskType setMask, KindOfMaskType clearMask);

	/**
		This Team will heal all its members
	*/
	void healAllObjects();

	/**
		* Iterate all members of the team
		*/
	void iterateObjects( ObjectIterateFunc func, void *userData );

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
		a convenience routine to quickly check if all the units are idle.
	*/
	Bool isIdle() const;

	/**
		a convenience routine to quickly check if any objects are in a trigger area.
	*/
	Bool unitsEntered(PolygonTrigger *pTrigger) const;

	/**
		a convenience routine to quickly check if any buildfacilities are owned.
	*/
	Bool hasAnyBuildFacility() const;

	/**
		Move team to destination.
	*/
	void moveTeamTo(Coord3D destination);

	/**
		a convenience routine to quickly destroy a team.
	*/
	Bool damageTeamMembers(Real amount);

	/**
		a convenience routine to destroy this team. The team goes through all shutdown stuff.
		Note that this doesn't actually call deleteInstance, the team will actually be
		deleted on the next update, if it is a deletable team.
		IgnoreDead lets you not delete people who are dying anyway.  Needed for scripts.
	*/
	void deleteTeam(Bool ignoreDead = FALSE);

	/**
		a convenience routine used to estimate the team's position by just returning the position
		of the first member of the team
		*/
	const Coord3D* getEstimateTeamPosition();

	/**
		a convenience routine to move a team's units to another team.
	*/
	void transferUnitsTo(Team *newTeam);

	/**
		a function to kill all members of a team
	*/
	void killTeam();

	/**
		a function to make all containers on a team to dump contents
	*/
	void evacuateTeam();

	/**
		the current waypoint for a team following a waypoint path.
	*/
	const Waypoint *getCurrentWaypoint() {return m_currentWaypoint;}
	void setCurrentWaypoint(const Waypoint *way) {m_currentWaypoint = way;}

	/**
		Update the generic scripts, allow them to see if they should run, etc.
	*/

	void updateGenericScripts();

};

// ------------------------------------------------------------------------
/**
	Note that TeamPrototype is used to hold information that is invariant between
	multiple instances of a given Team (e.g., alliance info).

	However, a TeamPrototype doesn't contain any build-list style info; that is handled
	by the BuildList stuff.
*/
class TeamPrototype : public MemoryPoolObject,
											public Snapshot
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TeamPrototype, "TeamPrototypePool" )

public:

	TeamPrototype( TeamFactory *tf,
								 const AsciiString& name,
								 Player *ownerPlayer,
								 Bool isSingleton,
								 Dict *d,
								 TeamPrototypeID id );
	// virtual destructor prototype provided by memory pool object

	TeamPrototypeID getID() const { return m_id; }
	const AsciiString& getName() const { return m_name; }
	Bool getIsSingleton() const { return (m_flags & TEAM_SINGLETON) != 0; }
	const TeamTemplateInfo *getTemplateInfo() const {return &m_teamTemplate;}
	/**
		return the team's owner (backtracking up if necessary)
	*/
	Player *getControllingPlayer() const;

	/**
		* Return a team with matching team ID if present
	*/
	Team *findTeamByID( TeamID teamID );

	/**
		set the team's owner. (nullptr is not allowed)
	*/
	void setControllingPlayer(Player *newController);

	/**
		Evaluate team's production condition.
	*/
	Bool evaluateProductionCondition();

	/**
		count for all the team instances belonging to this prototype.
	*/
	void countObjectsByThingTemplate(Int numTmplates, const ThingTemplate* const* things, Bool ignoreDead, Int *counts, Bool ignoreUnderConstruction = TRUE ) const;

	/**
		count the buildings owned by this Team template
	*/
	Int countBuildings();

	/**
		simply returns the number of objects on this team with a specific KindOfMaskType
	*/
	Int countObjects(KindOfMaskType setMask, KindOfMaskType clearMask);

	/**
		This TeamProtoType will heal all objects in all its instances
	*/
	void healAllObjects();

	/**
		* Iterate all members of the team
		*/
	void iterateObjects( ObjectIterateFunc func, void *userData );

	/// count the number of teams that have been instanced by this prototype
	Int countTeamInstances();

	/**
		Checks & clears the flags that a team member entered or exited a trigger area, or was created.
	*/
	void updateState();

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
		a convenience routine to quickly destroy a team.
	*/
	void damageTeamMembers(Real amount);

	/**
		Move team to destination.
	*/
	void moveTeamTo(Coord3D destination);

	// this is intended for use ONLY by class Player.
	void friend_setOwningPlayer(Player* p) { m_owningPlayer = p; }

	void teamAboutToBeDeleted(Team* team);

	Script *getGenericScript(Int scriptToRetrieve);

	// Make a team more likely to be selected by the ai for building due to success.
	void increaseAIPriorityForSuccess() const;
	// Make a team less likely to be selected by the ai for building due to failure.
	void decreaseAIPriorityForFailure() const;

	void setAttackPriorityName(const AsciiString &name) { m_attackPriorityName = name;}
	AsciiString getAttackPriorityName() const { return m_attackPriorityName;}

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

private:

	enum TeamPrototypeFlags
	{
		/**
			if set, this prototype should only produce one team. if there's already a team
			by this name, newly produced team members are added to it, rather than
			put into a new team of the same name.
		*/
		TEAM_SINGLETON = 0x01
	};

	TeamFactory						*m_factory;						///< the factory that created us
	Player								*m_owningPlayer;			///< the Player that currently controls the team-proto (null if NOT a top-level team)

	TeamPrototypeID				m_id;							///< unique prototype ID
	AsciiString						m_name;						///< name of the team(s) produced
	Int										m_flags;					///< misc team flags

	Bool									m_productionConditionAlwaysFalse; ///< Flag set to true if we don't have a production condition.
	Script								*m_productionConditionScript; ///< Script to evaluate for production condition.

	Bool									m_retrievedGenericScripts;
	Script								*m_genericScriptsToRun[MAX_GENERIC_SCRIPTS];

	TeamTemplateInfo			m_teamTemplate;						///< Team template info.

	AsciiString						m_attackPriorityName;
	// lists we own
	/// @todo srj -- convert to non-DLINK list, after it is once again possible to test the change
	MAKE_DLINK_HEAD(Team, TeamInstanceList)							///< the instances of this prototype

};

// ------------------------------------------------------------------------
class TeamFactory : public SubsystemInterface,
										public Snapshot
{
public:

	TeamFactory();
	virtual ~TeamFactory() override;

	// subsystem methods
	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;

	void clear();
	void initFromSides(SidesList *sides);

	/// create a new TeamPrototype and if singleton, a team.
	void initTeam(const AsciiString& name, const AsciiString& owner, Bool isSingleton, Dict *d);

	/// return the TeamPrototype with the given name. if none exists, return null.
	TeamPrototype *findTeamPrototype(const AsciiString& name);

	/// return TeamPrototype with matching ID.  if none exists nullptr is returned
	TeamPrototype *findTeamPrototypeByID( TeamPrototypeID id );

	/// search all prototypes for the team with the matching id, if none found nullptr is returned
	Team *findTeamByID( TeamID teamID );

	// note that there is no way to directly destroy a specific TeamPrototype (or a Team); the only
	// way to do this is to call TeamFactory::reset(), which destroys all of them.

	// ------ routines for dealing with Teams

	/// create a team. there must be a TeamPrototype with the given name, or an exception is thrown.
	Team *createTeam(const AsciiString& name);

	/// create a team given an explicitly team prototype rather than a prototype name
	Team *createTeamOnPrototype( TeamPrototype *prototype );

	/// create a team. there must be a TeamPrototype with the given name, or an exception is thrown.
	Team *createInactiveTeam(const AsciiString& name);

	/// return the team with the given name, or null if none exists. if multiple instances exist, return one arbitrarily.
	Team *findTeam(const AsciiString& name);

	void addTeamPrototypeToList(TeamPrototype* team);
	void removeTeamPrototypeFromList(TeamPrototype* team);

	void teamAboutToBeDeleted(Team* team);

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

private:

	typedef std::map< NameKeyType, TeamPrototype*, std::less<NameKeyType>/**/> TeamPrototypeMap;

	TeamPrototypeMap m_prototypes;
	TeamPrototypeID m_uniqueTeamPrototypeID;		///< used to assign unique ids to each team prototype
	TeamID m_uniqueTeamID;											///< used to assign unique team ids to each team instance

};

extern TeamFactory *TheTeamFactory;


// inline function ------------------------------------------------------------------------
const AsciiString& Team::getName() const
{
	return m_proto->getName();
}



// ------------------------------------------------------------------------
