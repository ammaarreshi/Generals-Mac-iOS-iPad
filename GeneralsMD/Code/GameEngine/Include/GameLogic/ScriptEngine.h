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

// FILE: ScriptEngine.h ///////////////////////////////////////////////////////////////////////////
// Script evaluation engine.
// Author: John Ahlquist, Nov. 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameType.h"
#include "Common/GameMemory.h"
#include "Common/STLTypedefs.h"
#include "Common/Science.h"
#include "Common/Snapshot.h"
#include "Common/SubsystemInterface.h"
#include "GameLogic/Scripts.h"

class DataChunkInput;
struct DataChunkInfo;
class DataChunkOutput;
class Team;
class Object;
class ThingTemplate;
class Player;
class PolygonTrigger;
class ObjectTypes;

#ifdef RTS_PROFILE_LEGACY
#define SPECIAL_SCRIPT_PROFILING
#endif

// Slightly odd place to put breeze info, but the breeze info is
// set by script, so it's as good a place as any.  john a.
struct BreezeInfo
{
	Real		m_direction;				///< Direction of the breeze in radians. 0 == +x direction.
	Coord2D	m_directionVec;			///< sin/cos of direction, for efficiency.
	Real		m_intensity;				///< How far to sway back & forth in radians.  0 = none.
	Real		m_lean;							///< How far to lean with the wind in radians.  0 = none.
	Real		m_randomness;				///< Randomness 0=perfectly uniform, 1 = +- up to 50% randomly.
	Short		m_breezePeriod;			///< How many frames it takes to sway forward & back.
	Short		m_breezeVersion;		///< Incremented each time the settings are updated.
};

// This could belong elsewhere, but for now...
struct NamedReveal
{
	AsciiString m_revealName;
	AsciiString m_waypointName;
	Real				m_radiusToReveal;
	AsciiString m_playerName;
};

struct TScriptListReadInfo
{
	Int numLists;
	ScriptList *readLists[MAX_PLAYER_COUNT];
};

struct TCounter
{
	Int value;
	AsciiString name;
	Bool isCountdownTimer;
};

struct TFlag
{
	Bool value;
	AsciiString name;
};

typedef std::list<AsciiString> ListAsciiString;
typedef std::list<AsciiString>::iterator ListAsciiStringIt;

typedef std::pair<AsciiString, UnsignedInt> PairAsciiStringUINT;
typedef std::list<PairAsciiStringUINT> ListAsciiStringUINT;
typedef ListAsciiStringUINT::iterator ListAsciiStringUINTIt;

typedef std::map< const ThingTemplate *, Int, std::less<const ThingTemplate *>/**/> AttackPriorityMap;
typedef std::pair<AsciiString, ObjectID> AsciiStringObjectIDPair;
typedef std::list<AsciiStringObjectIDPair> ListAsciiStringObjectID;
typedef std::list<AsciiStringObjectIDPair>::iterator ListAsciiStringObjectIDIt;

typedef std::pair<AsciiString, Coord3D> AsciiStringCoord3DPair;
typedef std::list<AsciiStringCoord3DPair> ListAsciiStringCoord3D;
typedef ListAsciiStringCoord3D::iterator ListAsciiStringCoord3DIt;

typedef std::map< AsciiString, Int > ObjectTypeCount;

typedef std::vector<Player *> VectorPlayerPtr;
typedef VectorPlayerPtr::iterator VectorPlayerPtrIt;

typedef std::vector<ObjectTypes*> AllObjectTypes;
typedef AllObjectTypes::iterator AllObjectTypesIt;

typedef std::vector<NamedReveal> VecNamedReveal;
typedef VecNamedReveal::iterator VecNamedRevealIt;

// TheSuperHackers @build xezon 17/03/2025 Fixes destructor visibility by removing MemoryPoolObject base class.
// MemoryPoolObject looks to be unnecessary because it is never dynamically allocated.
class AttackPriorityInfo : public Snapshot
{
public:

	AttackPriorityInfo();
	~AttackPriorityInfo();

public:

	void setPriority(const ThingTemplate *tThing, Int priority);
	Int getPriority(const ThingTemplate *tThing) const;
	AsciiString getName() const {return m_name;}
#ifdef RTS_DEBUG
	void dumpPriorityInfo();
#endif

	void friend_setName(const AsciiString& n) { m_name = n; }
	void friend_setDefaultPriority(Int n) { m_defaultPriority = n; }

	void reset();

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

	AsciiString m_name;
	Int	m_defaultPriority;
	AttackPriorityMap *m_priorityMap;

};

class SequentialScript : public MemoryPoolObject, public Snapshot
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SequentialScript, "SequentialScript")

public:

	enum { START_INSTRUCTION = -1 };
	Team *m_teamToExecOn;
	ObjectID m_objectID;
	Script *m_scriptToExecuteSequentially;
	Int m_currentInstruction;								// Which action within m_scriptToExecuteSequentially am I currently executing?
	Int m_timesToLoop;											// 0 = do once,  >0, loop till 0, <0, loop infinitely
	Int m_framesToWait;											// 0 = transition to next instruction, >0 = countdown to 0, <0 = advance on idle only
	Bool m_dontAdvanceInstruction;					// Must be set every frame by the instruction requesting the wait.
																					// so this parm tells us how many we've been idle so far.
	SequentialScript *m_nextScriptInSequence;

	SequentialScript();

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

};
EMPTY_DTOR(SequentialScript)

#ifdef NOT_IN_USE
class SequentialScriptStatus : public MemoryPoolObject, public Snapshot
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SequentialScriptStatus, "SequentialScriptStatus")

public:

	ObjectID m_objectID;
	AsciiString m_sequentialScriptCompleted;
	Bool m_isExecutingSequentially;

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer );
	virtual void xfer( Xfer *xfer );
	virtual void loadPostProcess();

};
#endif

//-----------------------------------------------------------------------------
// ScriptEngine
//-----------------------------------------------------------------------------
/** Implementation for the Script Engine singleton */
//-----------------------------------------------------------------------------
class ScriptEngine : public SubsystemInterface,
										 public Snapshot
{

public:
	enum {MAX_COUNTERS=256, MAX_FLAGS=256, MAX_ATTACK_PRIORITIES=256};
	enum TFade {FADE_NONE, FADE_SUBTRACT, FADE_ADD, FADE_SATURATE, FADE_MULTIPLY};
	ScriptEngine();
	virtual ~ScriptEngine() override;

	virtual void init() override;		///< Init
	virtual void reset() override;		///< Reset
	virtual void update() override;	///< Update

	void appendSequentialScript(const SequentialScript *scriptToSequence);
	void removeSequentialScript(SequentialScript *scriptToRemove);
	void notifyOfTeamDestruction(Team *teamDestroyed);
	void notifyOfObjectCreationOrDestruction();
	UnsignedInt getFrameObjectCountChanged() {return m_frameObjectCountChanged;}
	void setSequentialTimer(Object *obj, Int frameCount);
	void setSequentialTimer(Team *team, Int frameCount);

	void removeAllSequentialScripts(Object *obj);
	void removeAllSequentialScripts(Team *team);

	AsciiString getStats(Real *curTime, Real *script1Time, Real *script2Time);

	virtual void newMap(  );	///< reset script engine for new map
	virtual const ActionTemplate *getActionTemplate( Int ndx); ///< Get the template for a script action.
	virtual const ConditionTemplate *getConditionTemplate( Int ndx); ///< Get the template for a script Condition.
	virtual void startEndGameTimer(); ///< Starts the end game timer after a mission is won or lost.
	Bool isGameEnding() { return m_endGameTimer >= 0;	}
	virtual void startQuickEndGameTimer(); ///< Starts the quick end game timer after a campaign is won or lost.
	virtual void startCloseWindowTimer(); ///< Starts the timer to close windows after a mission is won or lost.
	virtual void runScript(const AsciiString& scriptName, Team *pThisTeam=nullptr); ///<  Runs a script.
	virtual void runObjectScript(const AsciiString& scriptName, Object *pThisObject=nullptr); ///<  Runs a script attached to this object.
	virtual Team *getTeamNamed(const AsciiString& teamName); ///<  Gets the named team.  May be null.
	virtual Player *getSkirmishEnemyPlayer(); ///< Gets the ai's enemy Human player. May be null.
	virtual Player *getCurrentPlayer(); ///<  Gets the player that owns the current script.  May be null.
	virtual Player *getPlayerFromAsciiString(const AsciiString& skirmishPlayerString);

	void setObjectsShouldReceiveDifficultyBonus(Bool receive) { m_objectsShouldReceiveDifficultyBonus = receive; }
	Bool getObjectsShouldReceiveDifficultyBonus() const { return m_objectsShouldReceiveDifficultyBonus; }

	void setChooseVictimAlwaysUsesNormal(Bool receive) { m_ChooseVictimAlwaysUsesNormal = receive; }
	Bool getChooseVictimAlwaysUsesNormal() const { return m_ChooseVictimAlwaysUsesNormal; }

	Bool hasShownMPLocalDefeatWindow();
	void markMPLocalDefeatWindowShown();

	// NOTE NOTE NOTE: do not store of the return value of this call (getObjectTypeList) beyond the life of the
	// function it will be used in, as it can be deleted from under you if maintenance is performed on the object.
	virtual ObjectTypes *getObjectTypes(const AsciiString& objectTypeList);
	virtual void doObjectTypeListMaintenance(const AsciiString& objectTypeList, const AsciiString& objectType, Bool addObject);

	/// Return the trigger area with the given name
	virtual PolygonTrigger *getQualifiedTriggerAreaByName( AsciiString name );

	// For other systems to evaluate Conditions, execute Actions, etc.

	///< if pThisTeam is specified, then scripts in here can use <This Team> to mean the team this script is attached to.
	virtual Bool evaluateConditions( Script *pScript, Team *pThisTeam = nullptr, Player *pPlayer=nullptr );
	virtual void friend_executeAction( ScriptAction *pActionHead, Team *pThisTeam = nullptr);	///< Use this at yer peril.

	virtual Object *getUnitNamed(const AsciiString& unitName); ///< Gets the named unit. May be null.
	virtual Bool didUnitExist(const AsciiString& unitName);
	virtual void addObjectToCache( Object* pNewObject );
	virtual void removeObjectFromCache( Object* pDeadObject );
	virtual void transferObjectName( const AsciiString& unitName, Object *pNewObject );
	virtual void notifyOfObjectDestruction( Object *pDeadObject );
	virtual void notifyOfCompletedVideo( const AsciiString& completedVideo );	///< Notify the script engine that a video has completed
	virtual void notifyOfTriggeredSpecialPower( Int playerIndex, const AsciiString& completedPower, ObjectID sourceObj );
	virtual void notifyOfMidwaySpecialPower		( Int playerIndex, const AsciiString& completedPower, ObjectID sourceObj );
	virtual void notifyOfCompletedSpecialPower( Int playerIndex, const AsciiString& completedPower, ObjectID sourceObj );
	virtual void notifyOfCompletedUpgrade			( Int playerIndex, const AsciiString& upgrade,				 ObjectID sourceObj );
	virtual void notifyOfAcquiredScience				( Int playerIndex, ScienceType science );

	virtual void signalUIInteract(const AsciiString& hookName);	///< Notify that a UI button was pressed and some flag should go true, for one frame only.

	virtual Bool isVideoComplete( const AsciiString& completedVideo, Bool removeFromList );	///< Determine whether a video has completed
	virtual Bool isSpeechComplete( const AsciiString& completedSpeech, Bool removeFromList );	///< Determine whether a speech has completed
	virtual Bool isAudioComplete( const AsciiString& completedAudio, Bool removeFromList );	///< Determine whether a sound has completed
	virtual Bool isSpecialPowerTriggered( Int playerIndex, const AsciiString& completedPower, Bool removeFromList, ObjectID sourceObj );
	virtual Bool isSpecialPowerMidway		( Int playerIndex, const AsciiString& completedPower, Bool removeFromList, ObjectID sourceObj );
	virtual Bool isSpecialPowerComplete	( Int playerIndex, const AsciiString& completedPower, Bool removeFromList, ObjectID sourceObj );
	virtual Bool isUpgradeComplete			( Int playerIndex, const AsciiString& upgrade,				 Bool removeFromList, ObjectID sourceObj );
	virtual Bool isScienceAcquired			( Int playerIndex, ScienceType science, Bool removeFromList );

	void setToppleDirection( const AsciiString& objectName, const Coord3D *direction );
	virtual void adjustToppleDirection( Object *object, Coord2D *direction);
	virtual void adjustToppleDirection( Object *object, Coord3D *direction);
	virtual const Script *findScriptByName(const AsciiString& scriptName) {return findScript(scriptName);} ///<  Finds a script.

	const BreezeInfo& getBreezeInfo() const {return m_breezeInfo;}
	void turnBreezeOff() {m_breezeInfo.m_intensity = 0.0f;}

	Bool isTimeFrozenScript();		///< Ask whether a script has frozen time or not
	void doFreezeTime();
	void doUnfreezeTime();

	/// The following functions are used to update and query the debug window
	Bool isTimeFrozenDebug();		///< Ask whether the debug window has requested a pause.
	Bool isTimeFast();		///< Ask whether the debug window has requested a fast forward.
	void forceUnfreezeTime();	///< Force that time becomes unfrozen temporarily.
	void AppendDebugMessage(const AsciiString& strToAdd, Bool forcePause);
	void AdjustDebugVariableData(const AsciiString& variableName, Int value, Bool forcePause);

	void clearTeamFlags(); ///< Hack for dustin.
	void clearFlag(const AsciiString &name); ///< Hack for dustin.

	TFade getFade() {return m_fade;}
	Real	getFadeValue() {return m_curFadeValue;}

	AsciiString getCurrentTrackName() const { return m_currentTrackName; }
	void setCurrentTrackName(AsciiString a) { m_currentTrackName = a; }

	GameDifficulty getGlobalDifficulty() const { return m_gameDifficulty; }
	void setGlobalDifficulty( GameDifficulty difficulty );

	/// Attack priority stuff.
	const AttackPriorityInfo *getDefaultAttackInfo();
	const AttackPriorityInfo *getAttackInfo(const AsciiString& name);

	const TCounter *getCounter(const AsciiString& counterName);

	void createNamedMapReveal(const AsciiString& revealName, const AsciiString& waypointName, Real radiusToReveal, const AsciiString& playerName);
	void doNamedMapReveal(const AsciiString& revealName);
	void undoNamedMapReveal(const AsciiString& revealName);
	void removeNamedMapReveal(const AsciiString& revealName);

	Int getObjectCount(Int playerIndex, const AsciiString& objectTypeName) const;
	void setObjectCount(Int playerIndex, const AsciiString& objectTypeName, Int newCount);

	//Kris: Moved to public... so that I can refresh it when building abilities in script dialogs.
	void createNamedCache();

	///Begin VTUNE
	void setEnableVTune(Bool value);
	Bool getEnableVTune() const;
	///End VTUNE
//#if defined(RTS_DEBUG)
	void debugVictory();
//#endif


	static void parseScriptAction( INI* ini );
	static void parseScriptCondition( INI* ini );

protected:

	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

	void addActionTemplateInfo(Template *actionTemplate);
	void addConditionTemplateInfo(Template *conditionTemplate);

	Int allocateCounter( const AsciiString& name);
	Int allocateFlag( const AsciiString& name);
	void executeScripts( Script *pScriptHead );
	void executeScript( Script *pScript );
	Script *findScript(const AsciiString& name);
	ScriptGroup *findGroup(const AsciiString& name);
	void setSway( ScriptAction *pAction );
	void setCounter( ScriptAction *pAction );
	void addCounter( ScriptAction *pAction );
	void subCounter( ScriptAction *pAction );
	void setFade( ScriptAction *pAction );
	void setFlag( ScriptAction *pAction );
	void pauseTimer( ScriptAction *pAction );
	void restartTimer( ScriptAction *pAction );
	void setTimer( ScriptAction *pAction, Bool millisecondTimer, Bool random);
	void adjustTimer( ScriptAction *pAction, Bool millisecondTimer, Bool add);
	void enableScript( ScriptAction *pAction );
	void disableScript( ScriptAction *pAction );
	void callSubroutine( ScriptAction *pAction );
	void checkConditionsForTeamNames(Script *pScript);
	Bool evaluateCounter( Condition *pCondition );
	Bool evaluateFlag( Condition *pCondition );
	Bool evaluateTimer( Condition *pCondition );
	Bool evaluateCondition( Condition *pCondition );
	void executeActions( ScriptAction *pActionHead );

	void setPriorityThing( ScriptAction *pAction );
	void setPriorityKind( ScriptAction *pAction );
	void setPriorityDefault( ScriptAction *pAction );

	// For Object types maintenance.
	void removeObjectTypes(ObjectTypes *typesToRemove);

	void particleEditorUpdate();
	void updateFades();

	AttackPriorityInfo *findAttackInfo(const AsciiString& name, Bool addIfNotFound);

protected:
	/// Stuff to execute scripts sequentially
	typedef std::vector<SequentialScript*> VecSequentialScriptPtr;
	typedef VecSequentialScriptPtr::iterator VecSequentialScriptPtrIt;

	VecSequentialScriptPtr m_sequentialScripts;

	void evaluateAndProgressAllSequentialScripts();
	VecSequentialScriptPtrIt cleanupSequentialScript(VecSequentialScriptPtrIt it, Bool cleanDanglers);

	Bool hasUnitCompletedSequentialScript( Object *object, const AsciiString& sequentialScriptName );
	Bool hasTeamCompletedSequentialScript( Team *team, const AsciiString& sequentialScriptName );




protected:
	ActionTemplate		m_actionTemplates[ScriptAction::NUM_ITEMS];
	ConditionTemplate	m_conditionTemplates[Condition::NUM_ITEMS];
	TCounter					m_counters[MAX_COUNTERS];
	Int								m_numCounters;
	TFlag							m_flags[MAX_FLAGS];
	Int								m_numFlags;
	AttackPriorityInfo m_attackPriorityInfo[MAX_ATTACK_PRIORITIES];
	Int								m_numAttackInfo;
	Int								m_endGameTimer;
	Int								m_closeWindowTimer;
	Team							*m_callingTeam;					///< Team that is calling script, used for THIS_TEAM
	Object						*m_callingObject;					///< Object that is calling script, used for THIS_OBJECT
	Team							*m_conditionTeam;				///< Team that is being used to evaluate conditions, used for THIS_TEAM
	Object						*m_conditionObject;				///< Unit that is being used to evaluate conditions, used for THIS_OBJECT
	VecNamedRequests	m_namedObjects;
	Bool							m_firstUpdate;
	Player						*m_currentPlayer;
	Player						*m_skirmishHumanPlayer;
	AsciiString				m_currentTrackName;

	TFade							m_fade;
	Real							m_minFade;
	Real							m_maxFade;
	Real							m_curFadeValue;
	Int								m_curFadeFrame;
	Int								m_fadeFramesIncrease;
	Int								m_fadeFramesHold;
	Int								m_fadeFramesDecrease;

	UnsignedInt				m_frameObjectCountChanged;

	ObjectTypeCount		m_objectCounts[MAX_PLAYER_COUNT];

	/// These are three separate lists rather than one to increase speed efficiency
	ListAsciiString				m_completedVideo;
	ListAsciiStringUINT		m_testingSpeech;
	ListAsciiStringUINT		m_testingAudio;

	ListAsciiString				m_uiInteractions;

	ListAsciiStringObjectID	m_triggeredSpecialPowers[MAX_PLAYER_COUNT];
	ListAsciiStringObjectID	m_midwaySpecialPowers		[MAX_PLAYER_COUNT];
	ListAsciiStringObjectID	m_finishedSpecialPowers	[MAX_PLAYER_COUNT];
	ListAsciiStringObjectID	m_completedUpgrades			[MAX_PLAYER_COUNT];
	ScienceVec							m_acquiredSciences			[MAX_PLAYER_COUNT];

	ListAsciiStringCoord3D m_toppleDirections;

	VecNamedReveal		m_namedReveals;

	BreezeInfo				m_breezeInfo;
	GameDifficulty		m_gameDifficulty;

	Bool							m_freezeByScript;
	AllObjectTypes		m_allObjectTypeLists;
	Bool							m_objectsShouldReceiveDifficultyBonus;
	Bool							m_ChooseVictimAlwaysUsesNormal;

	Bool							m_shownMPLocalDefeatWindow;

#ifdef SPECIAL_SCRIPT_PROFILING
#ifdef DEBUG_LOGGING
	double						m_numFrames;
	double						m_totalUpdateTime;
	double						m_maxUpdateTime;
	double						m_curUpdateTime;
#endif
#endif

};

extern ScriptEngine *TheScriptEngine;   ///< singleton definition
