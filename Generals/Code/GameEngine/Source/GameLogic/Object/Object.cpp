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

// FILE Object.cpp ////////////////////////////////////////////////////////////////////////////////
// Simple base object
// Author: Michael S. Booth, October 2000
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include <cmath>	// For isnan() on Linux
#define DEFINE_WEAPONCONDITIONMAP
#include "Common/BitFlagsIO.h"
#include "Common/BuildAssistant.h"
#include "Common/Dict.h"
#include "Common/GameEngine.h"
#include "Common/GameState.h"
#include "Common/GameUtility.h"
#include "Common/ModuleFactory.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Radar.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/WellKnownKeys.h"
#include "Common/Xfer.h"
#include "Common/XferCRC.h"
#include "Common/PerfTimer.h"

#include "GameClient/Anim2D.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/Eva.h"
#include "GameClient/InGameUI.h"

#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/FiringTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AutoHealBehavior.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CollideModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/DamageModule.h"
#include "GameLogic/Module/DeletionUpdate.h"
#include "GameLogic/Module/DestroyModule.h"
#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/ObjectDefectionHelper.h"
#include "GameLogic/Module/ObjectRepulsorHelper.h"
#include "GameLogic/Module/ObjectSMCHelper.h"
#include "GameLogic/Module/ObjectWeaponStatusHelper.h"
#include "GameLogic/Module/OverchargeBehavior.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/PowerPlantUpgrade.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/RadarUpgrade.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/ToppleUpdate.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/Module/RadarUpdate.h"
#include "GameLogic/Module/PowerPlantUpdate.h"

#include "Common/CRCDebug.h"
#include "Common/MiscAudio.h"


#ifdef DEBUG_OBJECT_ID_EXISTS
ObjectID TheObjectIDToDebug = INVALID_ID;
#endif

// ------------------------------------------------------------------------------------------------
static const ModelConditionFlags s_allWeaponFireFlags[WEAPONSLOT_COUNT] =
{
	MAKE_MODELCONDITION_MASK5(
		MODELCONDITION_FIRING_A,
		MODELCONDITION_BETWEEN_FIRING_SHOTS_A,
		MODELCONDITION_RELOADING_A,
		MODELCONDITION_PREATTACK_A,
		MODELCONDITION_USING_WEAPON_A
	),
	MAKE_MODELCONDITION_MASK5(
		MODELCONDITION_FIRING_B,
		MODELCONDITION_BETWEEN_FIRING_SHOTS_B,
		MODELCONDITION_RELOADING_B,
		MODELCONDITION_PREATTACK_B,
		MODELCONDITION_USING_WEAPON_B
	),
	MAKE_MODELCONDITION_MASK5(
		MODELCONDITION_FIRING_C,
		MODELCONDITION_BETWEEN_FIRING_SHOTS_C,
		MODELCONDITION_RELOADING_C,
		MODELCONDITION_PREATTACK_C,
		MODELCONDITION_USING_WEAPON_C
	)
};

//-------------------------------------------------------------------------------------------------
extern void addIcon(const Coord3D *pos, Real width, Int numFramesDuration, RGBColor color);

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AsciiString DebugDescribeObject(const Object *obj)
{
	if (!obj)
		return "<No Object>";

	AsciiString ret;

	if (obj->getName().isNotEmpty())
	{
		ret.format("Object %d (%s) [%s, owned by player %d (%ls)]",
			obj->getID(), obj->getName().str(), obj->getTemplate()->getName().str(),
			obj->getControllingPlayer()->getPlayerIndex(),
			obj->getControllingPlayer()->getPlayerDisplayName().str());
	}
	else
	{
		ret.format("Object %d [%s, owned by player %d (%ls)]",
			obj->getID(), obj->getTemplate()->getName().str(),
			obj->getControllingPlayer()->getPlayerIndex(),
			obj->getControllingPlayer()->getPlayerDisplayName().str());
	}

	return ret;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Object::Object( const ThingTemplate *tt, const ObjectStatusMaskType &objectStatusMask, Team *team ) :
	Thing(tt),
	m_indicatorColor(0),
	m_ai(nullptr),
	m_physics(nullptr),
	m_geometryInfo(tt->getTemplateGeometryInfo()),
	m_containedBy(nullptr),
	m_containedByID(INVALID_ID),
	m_containedByFrame(0),
	m_behaviors(nullptr),
	m_body(nullptr),
	m_contain(nullptr),
	m_stealth(nullptr),
	m_partitionData(nullptr),
	m_radarData(nullptr),
	m_drawable(nullptr),
	m_next(nullptr),
	m_prev(nullptr),
	m_team(nullptr),
	m_experienceTracker(nullptr),
	m_firingTracker(nullptr),
	m_repulsorHelper(nullptr),
	m_smcHelper(nullptr),
	m_wsHelper(nullptr),
	m_defectionHelper(nullptr),
	m_partitionLastLook(nullptr),
	m_partitionLastShroud(nullptr),
	m_partitionLastThreat(nullptr),
	m_partitionLastValue(nullptr),
	m_smcUntil(NEVER),
	m_privateStatus(0),
	m_formationID(NO_FORMATION_ID),
	m_isReceivingDifficultyBonus(FALSE)
{
#if defined(RTS_DEBUG)
	m_hasDiedAlready = false;
#endif
	//Modules have not been created yet!
	m_modulesReady = false;

	// Force the thing template to use the most overridden version of itself - jkmcd
	// Note that after this, the object will be using m_template, which forces the usage of the
	// most overridden version of tt, so this is okay.
	tt = (const ThingTemplate *) tt->getFinalOverride();

	Int i, modIdx;
	AsciiString modName;

	m_formationOffset.x = m_formationOffset.y = 0.0f;
	m_iPos.zero();
	for( i = 0; i < DISABLED_COUNT; i++ )
	{
		m_disabledTillFrame[ i ] = NEVER;
	}

	// sanity
	if( TheGameLogic == nullptr || tt == nullptr )
	{

		assert( 0 );
		return;

	}

	// Object's set of these persist for the life of the object.
	m_partitionLastLook = newInstance(SightingInfo);
	m_partitionLastLook->reset();
	m_partitionLastShroud = newInstance(SightingInfo);
	m_partitionLastShroud->reset();
	m_partitionLastThreat = newInstance(SightingInfo);
	m_partitionLastThreat->reset();
	m_partitionLastValue = newInstance(SightingInfo);
	m_partitionLastValue->reset();

	// must set ID to zero, since some of these set methods
	// will cause network messages to be sent
	// which use this ID.
	m_id = INVALID_ID;
	m_producerID = INVALID_ID;
	m_builderID = INVALID_ID;

	m_status = objectStatusMask;
	m_layer = LAYER_GROUND;

	m_group = nullptr;

	m_constructionPercent = CONSTRUCTION_COMPLETE;  // complete by default

	m_visionRange = tt->friend_getVisionRange();
	m_shroudClearingRange = tt->friend_getShroudClearingRange();
	if( m_shroudClearingRange == -1.0f )
		m_shroudClearingRange = m_visionRange;// Backwards compatible, and perfectly logical default to assign
	m_shroudRange = 0.0f;

	m_singleUseCommandUsed = false;

	// assign unique object id
	setID( TheGameLogic->allocateObjectID() );

	//
	// allocate any modules we need to, we should keep
	// this at or near the end of the drawable construction so that we have
	// all the valid data about the thing when we create the module
	//
	Int totalModules = tt->getBehaviorModuleInfo().getCount() + NUM_SLEEP_HELPERS;  // need to take into account all the helper modules

	// allocate the publicModule arrays
// pool[]ify
	m_behaviors = MSGNEW("ModulePtrs") BehaviorModule*[totalModules + 1];
	BehaviorModule** curB = m_behaviors;

	// set m_team to null before the first call, to avoid naughtiness...
	// If no team is specified in the constructor, then assign the object
	// to the neutral team.
	setTeam(team ? team : ThePlayerList->getNeutralPlayer()->getDefaultTeam());

	// the helpers are done first -- even before Behaviors! -- in case a module needs
	// to call something that uses them.
	static const NameKeyType smcHelperModuleDataTagNameKey = NAMEKEY( "ModuleTag_SMCHelper" );
	static ObjectSMCHelperModuleData smcModuleData;
	smcModuleData.setModuleTagNameKey( smcHelperModuleDataTagNameKey );
	m_smcHelper = newInstance(ObjectSMCHelper)(this, &smcModuleData);
	*curB++ = m_smcHelper;

	if (TheAI != nullptr
			&& TheAI->getAiData()->m_enableRepulsors
			&& isKindOf(KINDOF_CAN_BE_REPULSED))
	{
		// if we can ever be a temporary-repulsor, make a repulsor helper. (srj)
		static const NameKeyType repulsorHelperModuleDataTagNameKey = NAMEKEY( "ModuleTag_RepulsorHelper" );
		static ObjectRepulsorHelperModuleData repulsorModuleData;
		repulsorModuleData.setModuleTagNameKey( repulsorHelperModuleDataTagNameKey );
		m_repulsorHelper = newInstance(ObjectRepulsorHelper)(this, &repulsorModuleData);
		*curB++ = m_repulsorHelper;
	}

	/** @todo srj -- figure out how to create this only on demand.
		currently we don't have a good way to add/remove update modules from
		an object on-the-fly, so we fake it here, and just skip the creation
		if it is impossible for this object to ever defect... */

	// shrubbery cannot defect. no, really.
	if (!tt->isKindOf(KINDOF_SHRUBBERY))
	{
		static const NameKeyType defectionModuleDataTagNameKey = NAMEKEY( "ModuleTag_DefectionHelper" );
		static ObjectDefectionHelperModuleData defectionModuleData;
		defectionModuleData.setModuleTagNameKey( defectionModuleDataTagNameKey );
		m_defectionHelper = newInstance(ObjectDefectionHelper)(this, &defectionModuleData);
		*curB++ = m_defectionHelper;
	}

	if (tt->canPossiblyHaveAnyWeapon())
	{
		// we only need a firingtracker and wshelper if we can possibly have a weapon.
		static const NameKeyType weaponStatusModuleDataTagNameKey = NAMEKEY( "ModuleTag_WeaponStatusHelper" );
		static ObjectWeaponStatusHelperModuleData weaponStatusModuleData;
		weaponStatusModuleData.setModuleTagNameKey( weaponStatusModuleDataTagNameKey );
		m_wsHelper = newInstance(ObjectWeaponStatusHelper)(this, &weaponStatusModuleData);
		*curB++ = m_wsHelper;

		static const NameKeyType firingTrackerModuleDataTagNameKey = NAMEKEY( "ModuleTag_FiringTrackerHelper" );
		static FiringTrackerModuleData firingTrackerModuleData;
		firingTrackerModuleData.setModuleTagNameKey( firingTrackerModuleDataTagNameKey );
		m_firingTracker = newInstance(FiringTracker)(this, &firingTrackerModuleData);
		*curB++ = m_firingTracker;
	}

	// behaviors are always done first, so they get into the publicModule arrays
	// before anything else.
	const ModuleInfo& mi = tt->getBehaviorModuleInfo();
	for (modIdx = 0; modIdx < mi.getCount(); ++modIdx)
	{
		modName = mi.getNthName(modIdx);
		if (modName.isEmpty())
			continue;

		BehaviorModule* newMod = (BehaviorModule*)TheModuleFactory->newModule(this, modName, mi.getNthData(modIdx), MODULETYPE_BEHAVIOR);
		*curB++ = newMod;

		BodyModuleInterface* body = newMod->getBody();
		if (body)
		{
			DEBUG_ASSERTCRASH(m_body == nullptr, ("Duplicate bodies"));
			m_body = body;
		}

		ContainModuleInterface* contain = newMod->getContain();
		if (contain)
		{
			DEBUG_ASSERTCRASH(m_contain == nullptr, ("Duplicate containers"));
			m_contain = contain;
		}

		StealthUpdate* stealth = newMod->getStealth();
		if ( stealth )
		{
			DEBUG_ASSERTCRASH( m_stealth == nullptr, ("DuplicateStealthUpdates!") );
			m_stealth = stealth;
		}

		AIUpdateInterface* ai = newMod->getAIUpdateInterface();
		if (ai)
		{
			DEBUG_ASSERTCRASH(m_ai == nullptr, ("You should never have more than one AI module (srj)"));
			m_ai = ai;
		}

		static NameKeyType key_PhysicsUpdate = NAMEKEY("PhysicsBehavior");
		if (newMod->getModuleNameKey() == key_PhysicsUpdate)
		{
			DEBUG_ASSERTCRASH(m_physics == nullptr, ("You should never have more than one Physics module (%s)",getTemplate()->getName().str()));
			m_physics = (PhysicsBehavior*)newMod;
		}
	}

	*curB = nullptr;

	AIUpdateInterface *ai = getAIUpdateInterface();
	if (ai) {
		ai->setAttitude(getTeam()->getPrototype()->getTemplateInfo()->m_initialTeamAttitude);
		if (m_team && m_team->getPrototype() && m_team->getPrototype()->getAttackPriorityName().isNotEmpty()) {
			AsciiString name = m_team->getPrototype()->getAttackPriorityName();
			const AttackPriorityInfo *info = TheScriptEngine->getAttackInfo(name);
			if (info && info->getName().isNotEmpty()) {
				ai->setAttackInfo(info);
			}
		}
	}

	// allocate experience tracker
	m_experienceTracker = newInstance(ExperienceTracker)(this);

	// If a valid team has been assigned me, then I have a Player I can ask about my starting level
	const Player* controller = getControllingPlayer();
	m_experienceTracker->setVeterancyLevel( controller->getProductionVeterancyLevel( getTemplate()->getName() ) );

	/// allow for inter-Module resolution
	for (BehaviorModule** b = m_behaviors; *b; ++b)
	{
		(*b)->onObjectCreated();
	}

	m_numTriggerAreasActive = 0;
	m_enteredOrExitedFrame = 0;
	m_isSelectable = tt->isKindOf(KINDOF_SELECTABLE);

	m_healthBoxOffset.zero();// this is used for units that are amorphous, like angry mob

	//Modules have now been completely created!
	m_modulesReady = true;

	TheRadar->addObject( this );

	// register the object with the GameLogic
	TheGameLogic->registerObject( this );

	//disable occlusion for some time after object is created to allow them to exit the factory/building.
	m_safeOcclusionFrame = TheGameLogic->getFrame()+tt->getOcclusionDelay();


	m_soleHealingBenefactorID = INVALID_ID; ///< who is the only other object that can give me this non-stacking heal benefit?
	m_soleHealingBenefactorExpirationFrame = 0; ///< on what frame can I accept healing (thus to switch) from a new benefactor

	// TheSuperHackers @bugfix Mauller/xezon 02/08/2025 sendObjectCreated needs calling before CreateModule's are initialized to prevent drawable related crashes
	// This predominantly occurs with the veterancy create module when the chemical suits upgrade is unlocked as it tries to set the terrain decal.

	// emit message announcing object's creation
	TheGameLogic->sendObjectCreated( this );

}

//-------------------------------------------------------------------------------------------------
/** Emit message announcing object's creation
 * Note: Have to do this in virtual init() method because virtual methods
 * don't become virtual until AFTER the constructor has completed, and we
 * need to send our type in this message via virtual getType(). */
//-------------------------------------------------------------------------------------------------
void Object::initObject()
{
	// Weapons & Damage -------------------------------------------------------------------------------------------------
	// Force the initial weapon set to be instantiated & reloaded.
	m_curWeaponSetFlags.clear();
	m_weaponSet.updateWeaponSet(this);

	m_weaponBonusCondition = 0;

	for (int i = 0; i < WEAPONSLOT_COUNT; ++i)
		m_lastWeaponCondition[i] = WSF_INVALID;

	// If I have a valid team assigned, I can run through my Upgrade modules with his flags
	updateUpgradeModules();

	//If the player has battle plans (America Strategy Center), then apply those bonuses
	//to this object if applicable. Internally it validates certain kinds of objects.
	const Player* controller = getControllingPlayer();
	if (controller)
	{
		if (!getReceivingDifficultyBonus() && TheScriptEngine->getObjectsShouldReceiveDifficultyBonus())
		{
			setReceivingDifficultyBonus(TRUE);
		}

		if (controller->getNumBattlePlansActive() > 0)
		{
			controller->applyBattlePlanBonusesForObject( this );
		}
	}


	//For each special power module that we have, add it's type to the specialpower bits. This is
	//for optimal access later.
	for (BehaviorModule** m = m_behaviors; *m; ++m)
	{
		SpecialPowerModuleInterface* sp = (*m)->getSpecialPower();
		if (!sp)
			continue;

		const SpecialPowerTemplate *spTemplate = sp->getSpecialPowerTemplate();
		if( spTemplate )
		{
			SET_SPECIALPOWERMASK( m_specialPowerBits, spTemplate->getSpecialPowerType() );
		}
	}

	// Kris -- All missiles must be projectiles! This is the perfect place to assert them!
	// srj: yes, but only in debug...
#if defined(RTS_DEBUG)
	if( !isKindOf( KINDOF_PROJECTILE ) )
	{
		if( isKindOf( KINDOF_SMALL_MISSILE ) || isKindOf( KINDOF_BALLISTIC_MISSILE ) )
		{
			//Warning only...
			DEBUG_CRASH( ("Missile %s must also be a KindOf = PROJECTILE in addition to being either a SMALL_MISSILE or PROJECTILE_MISSILE -- call Kris (36844) for questions!", getTemplate()->getName().str() ) );
		}
	}
#endif
	if (!isKindOf(KINDOF_PROJECTILE) && !isKindOf(KINDOF_INERT)) {
		// Notify script conditions to update conditions that consider unit counts.
		// We ignore projectiles cause they are frequently created & destroyed, and are not
		// of general interest.  Normal unit count tests consider tanks or infantry or planes, etc. jba.
		TheScriptEngine->notifyOfObjectCreationOrDestruction();
		TheGameLogic->updateObjectsChangedTriggerAreas();
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Object::~Object()
{

	// tell the AI the building is gone
	/// @todo Generalize the notion of objects entering and leaving the world, so we don't have to special case this
	TheAI->pathfinder()->removeObjectFromPathfindMap( this );

	if (!isKindOf(KINDOF_PROJECTILE) && !isKindOf(KINDOF_INERT)) {
		// Notify script conditions to update conditions that consider unit counts.
		// We ignore projectiles cause they are frequently created & destroyed, and are not
		// of general interest.  Normal unit count tests consider tanks or infantry or planes, etc. jba.
		TheGameLogic->updateObjectsChangedTriggerAreas();
		TheScriptEngine->notifyOfObjectCreationOrDestruction();
	}

	//
	// remove from radar before we null out the team ... the order of ops are critical here
	// because the radar code will sometimes look at the team info and it is assumed through
	// the team and player code that the team is valid
	//
	if( m_radarData )
		TheRadar->removeObject( this );

	// emit message announcing object's destruction. Again, order is important; we must do this
	// before wiping out the team.
	TheGameLogic->sendObjectDestroyed( this );

	// empty the team
	setTeam( nullptr );

	// Object's set of these persist for the life of the object.
	deleteInstance(m_partitionLastLook);
	m_partitionLastLook = nullptr;
	deleteInstance(m_partitionLastShroud);
	m_partitionLastShroud = nullptr;
	deleteInstance(m_partitionLastThreat);
	m_partitionLastThreat = nullptr;
	deleteInstance(m_partitionLastValue);
	m_partitionLastValue = nullptr;

	// remove the object from the partition system if present
	if( m_partitionData )
		ThePartitionManager->unRegisterObject( this );

	// if we are in a group, remove us
	leaveGroup();

	// note, do NOT free these, there are just a shadow copy!
	m_ai = nullptr;
	m_physics = nullptr;

	// delete any modules present
	for (BehaviorModule** b = m_behaviors; *b; ++b)
	{
		deleteInstance(*b);
		*b = nullptr;	// in case other modules call findModule from their dtor!
	}

	delete [] m_behaviors;
	m_behaviors = nullptr;

	deleteInstance(m_experienceTracker);
	m_experienceTracker = nullptr;

	// we don't need to delete these, there were deleted on the m_behaviors list
	m_firingTracker = nullptr;
	m_repulsorHelper = nullptr;
	m_smcHelper = nullptr;
	m_wsHelper = nullptr;
	m_defectionHelper = nullptr;

	// reset id to zero so we never mistaken grab "dead" objects
	m_id = INVALID_ID;

	// Instead of removing it from the named cache, notify the script engine that it has died.
	// The script engine will remove it from the cache if necessary. The script engine needs to take
	// a crack at this in case it is the current "This Object" pointer.
	TheScriptEngine->notifyOfObjectDestruction(this);
}

//-------------------------------------------------------------------------------------------------
// TheSuperHackers @performance bobtista 13/11/2025 Use cached hero count for O(1) lookup instead of O(n) iteration.
Bool Object::isHero() const
{
	ContainModuleInterface *contain = getContain();
	if( contain )
	{
		if( contain->getHeroUnitsContained() > 0 )
		{
			return TRUE;
		}
	}
	return isKindOf( KINDOF_HERO );
}


//-------------------------------------------------------------------------------------------------
/// this object now contained in "containedBy"
//-------------------------------------------------------------------------------------------------
void Object::onContainedBy( Object *containedBy )
{
	setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNSELECTABLE ) );
	if (containedBy && containedBy->getContain()->isEnclosingContainerFor(this))
		setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_MASKED ) );
	else
		clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_MASKED ) );
	m_containedBy = containedBy;
	m_containedByFrame = TheGameLogic->getFrame();

#if RETAIL_COMPATIBLE_CRC
	// TheSuperHackers @info Set INVALID_ID if the container object was destroyed
	// to indicate that the pointer will become a dangling pointer in the next frame.
	if (containedBy && !containedBy->isDestroyed())
	{
		m_containedByID = containedBy->getID();
	}
	else
	{
		m_containedByID = INVALID_ID;
	}
#else
	DEBUG_ASSERTCRASH(containedBy == nullptr || !containedBy->isDestroyed(),
		("Object::onContainedBy - Adding into a destroyed container"));
#endif
}

//-------------------------------------------------------------------------------------------------
/// this object no longer contained in "containedBy"
//-------------------------------------------------------------------------------------------------
void Object::onRemovedFrom( Object *removedFrom )
{
	clearStatus( MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_MASKED, OBJECT_STATUS_UNSELECTABLE ) );
	m_containedBy = nullptr;
	m_containedByFrame = 0;

#if RETAIL_COMPATIBLE_CRC
	m_containedByID = INVALID_ID;
#endif
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Int Object::getTransportSlotCount() const
{
	Int count = getTemplate()->getRawTransportSlotCount();
	ContainModuleInterface* contain = getContain();
	if ( contain && contain->isSpecialZeroSlotContainer() )
	{
		count = 0;
		const ContainedItemsList* items = contain->getContainedItemsList();
		if (items)
		{
			for (ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it)
			{
				count += (*it)->getTransportSlotCount();
			}
		}
	}
	return count;
}

const Object* Object::getEnclosingContainedBy() const
{
	for (const Object* child = this, *container = getContainedBy(); container; child = container, container = container->getContainedBy())
	{
		ContainModuleInterface* containModule = container->getContain();
		if (containModule && containModule->isEnclosingContainerFor(child))
			return container;
	}

	return nullptr;
}

const Object* Object::getOuterObject() const
{
	if (const Object* enclosing = getEnclosingContainedBy())
		return enclosing;

	return this;
}

//-------------------------------------------------------------------------------------------------
/** Run from GameLogic::destroyObject */
//-------------------------------------------------------------------------------------------------
void Object::onDestroy()
{

	// This is the old cleanUpContain safeguard.  Say goodbye so they don't try to look us up.
	if (m_containedBy)
	{
#if RETAIL_COMPATIBLE_CRC
		if (m_containedByID == INVALID_ID)
		{
			// TheSuperHackers @bugfix Caball009 25/05/2026 Due to a potential use-after-free bug that cannot be fixed
			// with retail compatibility, the 'contained by' pointer of this object may point to an already destroyed object.
			// Avoid removing this object from the contain list, because it could crash the game,
			// as the begin / end iterator for STLPort and MSVC std::list implementations depends on dynamically allocated memory.
			DEBUG_CRASH(("container object must be valid; this looks like use-after-free"));
		}
		else
		{
			DEBUG_ASSERTCRASH(TheGameLogic->findObjectByID(m_containedByID) == m_containedBy,
				("contained by pointer is out of sync with contained by ID"));

			if (ContainModuleInterface* contain = m_containedBy->getContain())
			{
				contain->removeFromContain(this);
			}
		}
#else
		if (ContainModuleInterface* contain = m_containedBy->getContain())
		{
			contain->removeFromContain(this);
		}
#endif
	}

	//
	// run the onDelete on all modules present so they each have an opportunity to cleanup
	// anything they need to ... including talking to any other modules
	//
	for (BehaviorModule** b = m_behaviors; *b; ++b)
	{
		(*b)->onDelete();
	}

	//Have to remove ourself from looking as well.  RebuildHoleWorkers definitely hit here.
	handlePartitionCellMaintenance();
}

//=============================================================================
//=============================================================================
void Object::setGeometryInfo(const GeometryInfo& geom)
{
	m_geometryInfo = geom;
	if( m_partitionData )
	{
		// if our geometry changes, we unregister and re-register with the partitionmgr
		// so that our size gets updated appropriately. this shouldn't be a problem
		// unless setGeometryInfo gets called frequently. (srj)
		ThePartitionManager->unRegisterObject( this );
		ThePartitionManager->registerObject( this );
	}

	if (m_drawable)
		m_drawable->reactToGeometryChange();
}

//=============================================================================
//=============================================================================
void Object::setGeometryInfoZ( Real newZ )
{
	// A Z change only does not need to un/register with the PartitionManager
	m_geometryInfo.setMaxHeightAbovePosition( newZ );

	if (m_drawable)
		m_drawable->reactToGeometryChange();
}

//=============================================================================
void Object::friend_setUndetectedDefector( Bool status )
{
	if (status)
		m_privateStatus |= UNDETECTED_DEFECTOR;
	else
		m_privateStatus &= ~UNDETECTED_DEFECTOR;
}

//=============================================================================
void Object::restoreOriginalTeam()
{
	if( m_team == nullptr || m_originalTeamName.isEmpty() )
		return;

	Team* origTeam = TheTeamFactory->findTeam(m_originalTeamName);
	if (origTeam == nullptr)
	{
		DEBUG_CRASH(("Object original team (%s) could not be found or created! (srj)",m_originalTeamName.str()));
		return;
	}

	if (m_team == origTeam)
	{
		DEBUG_CRASH(("Object appears to still be on its original team, so why are we attempting to restore it? (srj)"));
		return;
	}

	setTeam(origTeam);
}

//=============================================================================
//=============================================================================
void Object::setTeam( Team *team )
{
	// In order to prevent spawning useful units for a player after he dies, we
	// just assign objects to the neutral player if we try to misbehave.
	if (team && !team->getControllingPlayer()->isPlayerActive())
		team = ThePlayerList->getNeutralPlayer()->getDefaultTeam();

	setTemporaryTeam(team);
	m_originalTeamName = m_team ? m_team->getName() : AsciiString::TheEmptyString;
}

//=============================================================================
//=============================================================================
void Object::setTemporaryTeam( Team *team )
{
	const Bool restoring = false;
	setOrRestoreTeam(team, restoring);
}

//=============================================================================
//=============================================================================
void Object::setOrRestoreTeam( Team* team, Bool restoring )
{
	// don't do anything if the team hasn't changed
	if( m_team == team )
		return;

	Team* oldTeam = m_team;

	// Before Switch //////////////////////////
	if (m_team)
	{
		if (m_team->isInList_TeamMemberList(this))
		{
			m_team->removeFrom_TeamMemberList(this);
			m_team->getControllingPlayer()->becomingTeamMember(this, false);
		}
	}

	// Switch //////////////////////////
	m_team = team;

	// After Switch //////////////////////////
	if (m_team)
	{
		if (!m_team->isInList_TeamMemberList(this))
		{
			m_team->prependTo_TeamMemberList(this);
			m_team->getControllingPlayer()->becomingTeamMember(this, true);
		}

		// now, adjust the attitude of the unit to its new team.
		const TeamPrototype* proto = m_team->getPrototype();
		if (proto && proto->getTemplateInfo())
		{
			AIUpdateInterface *ai = getAIUpdateInterface();
			if (ai)
			{
				ai->setAttitude(proto->getTemplateInfo()->m_initialTeamAttitude);
				if (proto->getAttackPriorityName().isNotEmpty()) {
					AsciiString name = proto->getAttackPriorityName();
					const AttackPriorityInfo *info = TheScriptEngine->getAttackInfo(name);
					if (info && info->getName().isNotEmpty()) {
						ai->setAttackInfo(info);
					}
				}
			}
		}
		// emit message announcing object's new alliance
		Drawable *draw = getDrawable();
		if (draw)
			draw->changedTeam();
	}

	// This can't just go in ::defect, because some things just do setTeam.  The act of
	// setting a new team needs to tell the modules and do other important stuff.
	// And it needs to happen after the switch.
	if( oldTeam && team && !restoring )
		onCapture( oldTeam->getControllingPlayer(), team->getControllingPlayer() );

	//
	// the team changed we have a change in priorities on the radar if we are
	// a candidate for the radar as it is
	//
	if( m_radarData )
	{

		// removing it and adding it will cause a resort to happen
		TheRadar->removeObject( this );
		TheRadar->addObject( this );
	}

	// Tell TheInGameUI that the object has changed hands
	Int oldPlayerIndex = (oldTeam)?(oldTeam->getControllingPlayer()->getPlayerIndex()):-1;
	Int newPlayerIndex = (m_team)?(m_team->getControllingPlayer()->getPlayerIndex()):-1;
	if (oldPlayerIndex != newPlayerIndex)
		TheInGameUI->objectChangedTeam(this, oldPlayerIndex, newPlayerIndex);
}

//=============================================================================
void Object::setStatus( ObjectStatusMaskType objectStatus, Bool set )
{
	ObjectStatusMaskType oldStatus = m_status;

	if (set)
		m_status.set( objectStatus );
	else
		m_status.clear( objectStatus );

	if (m_status != oldStatus)
	{
		if( set && objectStatus.test( OBJECT_STATUS_REPULSOR ) && m_repulsorHelper != nullptr )
		{
			// Damaged repulsable civilians scare (repulse) other civs, but only
			// for a short amount of time... use the repulsor helper to turn off repulsion shortly.
			m_repulsorHelper->sleepUntil(TheGameLogic->getFrame() + 2*LOGICFRAMES_PER_SECOND);
		}

		// when an object's construction status changes, it needs to have its partition data updated,
		// in order to maintain the shroud correctly.
		if( m_status.test( OBJECT_STATUS_UNDER_CONSTRUCTION ) != oldStatus.test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		{

			// CHECK FOR MINES, AND DETONATE THEM NOW
			ObjectIterator *iter =
					ThePartitionManager->iteratePotentialCollisions( getPosition(), getGeometryInfo(), getOrientation() );
			MemoryPoolObjectHolder hold( iter );
			Object *them;
			for( them = iter->first(); them; them = iter->next() )
			{
				if (them->isKindOf( KINDOF_MINE ))
				{
					//DETONATE ANY ENEMY MINES, OR DELETE FRIENDLY ONES
					Relationship r = getRelationship(them);
					if (r == ENEMIES)
					{
						them->kill(); // detonate mine
					}
					else
					{
						TheGameLogic->destroyObject(them);
					}
				}
			}

			if (m_partitionData)
				m_partitionData->makeDirty(true);
		}

	}

}

//=============================================================================
void Object::setScriptStatus( ObjectScriptStatusBit bit, Bool set )
{
	UnsignedInt oldScriptStatus = m_scriptStatus;

	if( set )
	{
		m_scriptStatus |= bit;
	}
	else
	{
		m_scriptStatus &= ~bit;
	}

	if( m_scriptStatus != oldScriptStatus )
	{
		if( (m_scriptStatus & OBJECT_STATUS_SCRIPT_DISABLED) != (oldScriptStatus & OBJECT_STATUS_SCRIPT_DISABLED) )
		{
			if( m_partitionData )
			{
				// if an object becomes disabled or unpowered, then you have to update its partition data because it will
				// change how far it can see.
				m_partitionData->makeDirty(true);
			}
			if( m_scriptStatus & OBJECT_STATUS_SCRIPT_DISABLED )
			{
				//I am now disabled, so tell the main game engine!
				setDisabled( DISABLED_SCRIPT_DISABLED );
			}
			else
			{
				//I am no longer disabled, so tell the main game engine!
				clearDisabled( DISABLED_SCRIPT_DISABLED );
			}
		}
		if( (m_scriptStatus & OBJECT_STATUS_SCRIPT_UNPOWERED) != (oldScriptStatus & OBJECT_STATUS_SCRIPT_UNPOWERED) )
		{
			if( m_partitionData )
			{
				// if an object becomes disabled or unpowered, then you have to update its partition data because it will
				// change how far it can see.
				m_partitionData->makeDirty(true);
			}
			if( m_scriptStatus & OBJECT_STATUS_SCRIPT_UNPOWERED )
			{
				//I am now underpowered, so tell the main game engine!
				setDisabled( DISABLED_SCRIPT_UNDERPOWERED );
			}
			else
			{
				//I am no longer undperpowered, so tell the main game engine!
				clearDisabled( DISABLED_SCRIPT_UNDERPOWERED );
			}
		}
	}
}

//=============================================================================
Bool Object::canCrushOrSquish(Object *otherObj, CrushSquishTestType testType ) const
{
	DEBUG_ASSERTCRASH(this, ("null this in canCrushOrSquish"));

	if( !otherObj )
	{
		//Can't crush anything.
		return false;
	}

	if( isDisabledByType( DISABLED_UNMANNED ) )
	{
		//Unmanned vehicles cannot crush troops. This was happening when Jarmen Kell sniped
		//the vehicle and booted the guys out while still moving, as the vehicle is now
		//on a different team.
		return false;
	}

	UnsignedByte crusherLevel = getCrusherLevel();

	// order matters: we want to know if I consider it to be an ally, not vice versa
	if( getRelationship( otherObj ) == ALLIES )
	{
		//Friends don't let friends crush friends.
		return false;
	}

	if( !crusherLevel )
	{
		//Can't crush anything!
		return false;
	}

	//Test this case for generic infantry getting squished by vehicles!
	if( testType == TEST_SQUISH_ONLY || testType == TEST_CRUSH_OR_SQUISH )
	{

		//****************************************************************************************
		//NOTE: This section of code is used by the pathfinder to determine if the object should
		//      move to the target. I don't think it's the right place to check for this because
		//      the semantics check to see if we can squish something -- not approach it. However
		//      I'm not moving it for fear of some major breakage! -- KM
		//Bool squisher = crusherLevel > 0;
		//if( !squisher )
		//{
		//	Weapon *weapon = getCurrentWeapon();
		//	if( weapon && weapon->isContactWeapon() )
		//	{
		//		squisher = true;
		//	}
		//}
		//if( squisher )
		//NOTE2: *** IF YOU REENABLE THIS CODE -- Move the "if( !crusherLevel ) return false" below
		//                                        this squish section.
		//****************************************************************************************
		{
			// See if other is squishable
			static NameKeyType key_squish = NAMEKEY( "SquishCollide" );
			if( otherObj->findModule( key_squish ) )
			{
				return true; // squishable.
			}
		}
	}


	UnsignedByte crushableLevel = otherObj->getCrushableLevel();

	if( testType == TEST_CRUSH_ONLY || testType == TEST_CRUSH_OR_SQUISH )
	{
		if( crusherLevel > crushableLevel )
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
UnsignedByte Object::getCrusherLevel() const
{
	return getTemplate()->getCrusherLevel();
}

//-------------------------------------------------------------------------------------------------
UnsignedByte Object::getCrushableLevel() const
{
	return getTemplate()->getCrushableLevel();
}


// ------------------------------------------------------------------------------------------------
/** Topple an object, if possible */
// ------------------------------------------------------------------------------------------------
void Object::topple( const Coord3D *toppleDirection, Real toppleSpeed, UnsignedInt options )
{
	static NameKeyType key_ToppleUpdate = NAMEKEY("ToppleUpdate");

	ToppleUpdate* toppleUpdate = (ToppleUpdate*)findModule(key_ToppleUpdate);
	if( toppleUpdate && toppleUpdate->isAbleToBeToppled() )
	{

		// apply the topple force
		toppleUpdate->applyTopplingForce( toppleDirection, toppleSpeed, options );

	}

}

//=============================================================================
void Object::reloadAllAmmo(Bool now)
{
	m_weaponSet.reloadAllAmmo(this, now);
}

//=============================================================================
Bool Object::isOutOfAmmo() const
{
	return m_weaponSet.isOutOfAmmo();
}

//=============================================================================
Bool Object::hasAnyWeapon() const
{
	return m_weaponSet.hasAnyWeapon();
}

//=============================================================================
Bool Object::hasAnyDamageWeapon() const
{
	//First check to see if we have any weapons -- if not return false.
	if( !m_weaponSet.hasAnyDamageWeapon() )
	{
		return FALSE;
	}
	return TRUE;
}

//=============================================================================
UnsignedInt Object::getMostPercentReadyToFireAnyWeapon() const
{
	return m_weaponSet.getMostPercentReadyToFireAnyWeapon();
}

//=============================================================================
Bool Object::hasWeaponToDealDamageType(DamageType typeToDeal) const
{
	return m_weaponSet.hasWeaponToDealDamageType(typeToDeal);
}

//=============================================================================
Real Object::getLargestWeaponRange() const
{
	Real retVal = -1;
	for (Int i = PRIMARY_WEAPON; i < WEAPONSLOT_COUNT; ++i) {
		Weapon* weapon = m_weaponSet.getWeaponInWeaponSlot((WeaponSlotType)i);
		if (!weapon) {
			continue;
		}

		Real tmpVal = weapon->getAttackRange(this);
		if (tmpVal > retVal) {
			retVal = tmpVal;
		}
	}
	return retVal;
}

//=============================================================================
void Object::setFiringConditionForCurrentWeapon() const
{
	if (m_drawable)
	{
		WeaponSlotType wslot = m_weaponSet.getCurWeaponSlot();
		ModelConditionFlags c = m_weaponSet.getModelConditionForWeaponSlot(wslot, WSF_FIRING);
		m_drawable->clearAndSetModelConditionFlags(s_allWeaponFireFlags[wslot], c);
	}
}

//=============================================================================
void Object::setModelConditionState( ModelConditionFlagType a )
{
	if (m_drawable)
	{
		m_drawable->setModelConditionState(a);
	}
}

//=============================================================================
void Object::clearModelConditionState( ModelConditionFlagType a )
{
	if (m_drawable)
	{
		m_drawable->clearModelConditionState(a);
	}
}

//=============================================================================
void Object::clearAndSetModelConditionState( ModelConditionFlagType clr, ModelConditionFlagType set )
{
	if (m_drawable)
	{
		m_drawable->clearAndSetModelConditionState(clr, set);
	}
}

//=============================================================================
void Object::clearModelConditionFlags( const ModelConditionFlags& clr )
{
	if (m_drawable)
	{
		m_drawable->clearModelConditionFlags(clr);
	}
}

//=============================================================================
void Object::setModelConditionFlags( const ModelConditionFlags& set )
{
	if (m_drawable)
	{
		m_drawable->setModelConditionFlags(set);
	}
}

//=============================================================================
void Object::clearAndSetModelConditionFlags( const ModelConditionFlags& clr, const ModelConditionFlags& set )
{
	if (m_drawable)
	{
		m_drawable->clearAndSetModelConditionFlags(clr, set);
	}
}

//=============================================================================
// Special model states are states that are turned on for a period of time, and
// turned off automatically -- used for cheer, and scripted special moment
// animations. Setting a special state will automatically clear any other
// special states that may be turned on so you can only have one at a time.
//=============================================================================
void Object::setSpecialModelConditionState( ModelConditionFlagType set, UnsignedInt frames )
{
	clearSpecialModelConditionStates();

	setModelConditionState( set );

	if( frames == 0 )
	{
		frames = 1;
	}

	m_smcUntil = TheGameLogic->getFrame() + frames;
	m_smcHelper->sleepUntil(m_smcUntil);
}

//=============================================================================
void Object::clearSpecialModelConditionStates()
{
	clearModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_SPECIAL_CHEERING ) );
	m_smcUntil = NEVER;
}

// Lorenzen has some interest in this, ask before deleting
//=============================================================================
//const ModelConditionFlags& Object::getModelConditionFlags() const
//{
//	if (m_drawable)
//	{
//		return m_drawable->getModelConditionFlags();
//	}
//	else
//	{
//		DEBUG_CRASH(("null Drawable at this point, you can't get modelconditionflags now."));
//		static ModelConditionFlags noFlags;
//		return noFlags;
//	}
//}

//=============================================================================
Weapon* Object::getCurrentWeapon(WeaponSlotType* wslot)
{
	if (!m_weaponSet.hasAnyWeapon())
		return nullptr;

	if (wslot)
		*wslot = m_weaponSet.getCurWeaponSlot();
	return m_weaponSet.getCurWeapon();
}

//=============================================================================
const Weapon* Object::getCurrentWeapon(WeaponSlotType* wslot) const
{
	if (!m_weaponSet.hasAnyWeapon())
		return nullptr;

	if (wslot)
		*wslot = m_weaponSet.getCurWeaponSlot();
	return m_weaponSet.getCurWeapon();
}

//=============================================================================
Weapon* Object::findWaypointFollowingCapableWeapon()
{
	return m_weaponSet.findWaypointFollowingCapableWeapon();
}

//=============================================================================
Bool Object::getAmmoPipShowingInfo(Int& numTotal, Int& numFull) const
{
/// @todo srj -- may need to cache this inside weaponset.
	const Weapon* w = m_weaponSet.findAmmoPipShowingWeapon();
	if (w)
	{
		numTotal = w->getClipSize();
		numFull = w->getRemainingAmmo();
		return true;
	}
	else
	{
		return false;
	}
}

//=============================================================================
/*
	NOTE: getAbleToAttackSpecificObject NO LONGER internally calls isAbleToAttack(),
	since that isn't an incredibly fast call, and this is called repeatedly in some inner loops
	where we already know that isAbleToAttack() == true. so you should always
	call isAbleToAttack prior to calling this! (srj)
*/
CanAttackResult Object::getAbleToAttackSpecificObject( AbleToAttackType t, const Object* target, CommandSourceType commandSource ) const
{
	// NO! BAD! WRONG!
	// If we can't attack at all, then we cannot attack this
	//if (!isAbleToAttack())
	//	return FALSE;

	// Otherwise leave it up to our weapons.
	return m_weaponSet.getAbleToAttackSpecificObject( t, this, target, commandSource );
}

//=============================================================================
//Used for base defenses and otherwise stationary units to see if you can attack a position potentially out of range.
CanAttackResult Object::getAbleToUseWeaponAgainstTarget( AbleToAttackType attackType, const Object *victim, const Coord3D *pos, CommandSourceType commandSource ) const
{
	return m_weaponSet.getAbleToUseWeaponAgainstTarget( attackType, this, victim, pos, commandSource );
}


//=============================================================================
Bool Object::chooseBestWeaponForTarget(const Object* target, WeaponChoiceCriteria criteria, CommandSourceType cmdSource )
{
	return m_weaponSet.chooseBestWeaponForTarget(this, target, criteria, cmdSource );
}

//DECLARE_PERF_TIMER(fireCurrentWeapon)
//=============================================================================
void Object::fireCurrentWeapon(Object *target)
{
	//USE_PERF_TIMER(fireCurrentWeapon)

	// victim may have already been destroyed
	if (target == nullptr)
		return;

	Weapon* weapon = m_weaponSet.getCurWeapon();
	if (weapon && (weapon->getStatus() == READY_TO_FIRE))
	{
		Bool reloaded = weapon->fireWeapon(this, target);
		DEBUG_ASSERTCRASH(m_firingTracker, ("hey, we are firing but have no firing tracker. this is wrong."));
		if (m_firingTracker)
			m_firingTracker->shotFired(weapon, target->getID());
		if (reloaded)
			releaseWeaponLock(LOCKED_TEMPORARILY);	// release any temporary locks.

		friend_setUndetectedDefector( FALSE );// My secret is out
	}
}

//=============================================================================
void Object::fireCurrentWeapon(const Coord3D* pos)
{
	//USE_PERF_TIMER(fireCurrentWeapon)

	if (pos == nullptr)
		return;

	Weapon* weapon = m_weaponSet.getCurWeapon();
	if (weapon && (weapon->getStatus() == READY_TO_FIRE))
	{
		Bool reloaded = weapon->fireWeapon(this, pos);
		DEBUG_ASSERTCRASH(m_firingTracker, ("hey, we are firing but have no firing tracker. this is wrong."));
		if (m_firingTracker)
			m_firingTracker->shotFired(weapon, INVALID_ID);
		if (reloaded)
			releaseWeaponLock(LOCKED_TEMPORARILY);	// release any temporary locks.

		friend_setUndetectedDefector( FALSE );// My secret is out
	}
}

//=============================================================================
void Object::preFireCurrentWeapon( const Object *victim )
{
	Weapon* weapon = m_weaponSet.getCurWeapon();

	//If we are going to be capable of firing our weapon NEXT frame, set the pre-attack
	//up now. This gets called by AIAttackFireWeaponState::onEnter().. but the update happens
	//next frame.
	if (weapon && TheGameLogic->getFrame() + 1 >= weapon->getPossibleNextShotFrame() )
	{
		weapon->preFireWeapon( this, victim );
		friend_setUndetectedDefector( FALSE );// My secret is out
	}
}

// ============================================================================
/** Using the firing tracker, return the frame a shot was last fired on */
// ============================================================================
UnsignedInt Object::getLastShotFiredFrame() const
{
	UnsignedInt recent = 0;
	for (int i = 0; i < WEAPONSLOT_COUNT; ++i)
	{
		const Weapon* w = m_weaponSet.getWeaponInWeaponSlot((WeaponSlotType)i);
		if (w)
		{
			UnsignedInt when = w->getLastShotFrame();
			if (when > recent)
				recent = when;
		}
	}
	return recent;
}

// ============================================================================
/** Get the victim ID we last shot at */
// ============================================================================
ObjectID Object::getLastVictimID() const
{
	return m_firingTracker ? m_firingTracker->getLastShotVictim() : INVALID_ID;
}

//=============================================================================
// Object::getRelationship
//=============================================================================
Relationship Object::getRelationship(const Object *that) const
{
	const Team *myTeam = getTeam();

	if (myTeam && that)
	{
		if (getIsUndetectedDefector())
		{
			return NEUTRAL; // so my AI does not give away my position by auto acquire
		}
		else if (that->getIsUndetectedDefector())
		{
			return ALLIES; // so I treat undetecteddefectors like they were my very own
		}
		else
		{
			return myTeam->getRelationship( that->getTeam() );
		}
	}

	return NEUTRAL;

}

//=============================================================================
// Object::getControllingPlayer
//=============================================================================
Player * Object::getControllingPlayer() const
{
	const Team* myTeam = this->getTeam();
	if (myTeam)
		return myTeam->getControllingPlayer();

	return nullptr;
}

//=============================================================================
void Object::setProducer(const Object* obj)
{
	m_producerID = obj ? obj->getID() : INVALID_ID;
// seems like a good idea, but is not. (srj)
//	if (obj)
//		m_indicatorColor = obj->m_indicatorColor;
}

//=============================================================================
void Object::setBuilder( const Object *obj )
{

  m_builderID = obj ? obj->getID() : INVALID_ID;

}

//=============================================================================
void Object::setCustomIndicatorColor(Color c)
{
	if (m_indicatorColor != c)
	{
		m_indicatorColor = c;
		if (m_drawable)
			m_drawable->changedTeam();
	}
}

//=============================================================================
void Object::removeCustomIndicatorColor()
{
	setCustomIndicatorColor(0);
}

//=============================================================================
// Object::getIndicatorColor
//=============================================================================
Color Object::getIndicatorColor() const
{
	if (m_indicatorColor == 0)
	{
		const Team *myTeam = getTeam();
		if (myTeam)
		{
			const Player* p = myTeam->getControllingPlayer();
			if (p)
			{
				return p->getPlayerColor();
			}
		}
		return GameMakeColor(0, 0, 0, 255);
	}
	else
	{
		return m_indicatorColor;
	}
}

//=============================================================================
// Object::getNightIndicatorColor - used to make blue/purple easier to see on night models.
//=============================================================================
Color Object::getNightIndicatorColor() const
{
	if (m_indicatorColor == 0)
	{
		const Team *myTeam = getTeam();
		if (myTeam)
		{
			const Player* p = myTeam->getControllingPlayer();
			if (p)
			{
				return p->getPlayerNightColor();
			}
		}
		return GameMakeColor(0, 0, 0, 255);
	}
	else
	{
		return m_indicatorColor;
	}
}

//=============================================================================
// Object::isLogicallyVisible
//=============================================================================
Bool Object::isLogicallyVisible() const
{
	const Object* obj = getOuterObject();

	// Disguisers are always visible to all players, irrespective of any stealth
	// status. We thus need to check the type rather than the status as the
	// disguise status is absent during the disguise transition phase.
	if (obj->isKindOf(KINDOF_DISGUISER))
		return true;

	if (obj->testStatus(OBJECT_STATUS_STEALTHED) && !obj->testStatus(OBJECT_STATUS_DETECTED))
	{
		const Player* player = rts::getObservedOrLocalPlayer();
		const Relationship relationship = player->getRelationship(getTeam());

		if (player->isPlayerActive() && relationship != ALLIES)
			return false;
	}

	return true;
}

//=============================================================================
// Object::isLocallyControlled
//=============================================================================
Bool Object::isLocallyControlled() const
{
	return getControllingPlayer() == ThePlayerList->getLocalPlayer();
}

//=============================================================================
// Object::isLocallyViewed
//=============================================================================
Bool Object::isLocallyViewed() const
{
	return getControllingPlayer() == rts::getObservedOrLocalPlayer();
}

//=============================================================================
// Object::isLocallyControlled
//=============================================================================
Bool Object::isNeutralControlled() const
{
	return getControllingPlayer() == ThePlayerList->getNeutralPlayer();
}

//-------------------------------------------------------------------------------------------------
inline Bool isPosDifferent(const Coord3D* a, const Coord3D* b)
{
	// this is necessary because PhysicsBehavior may generate tiny changes even when
	// "standing still", due to roundoff errors. It's important that we only invalidate
	// the PartitionManager stuff when the pos/orientation really changes (for efficiency purposes)
	// so we must put in some cleverness...
	const Real THRESH = 0.01f;

	if (fabs(a->x - b->x) > THRESH)
		return true;

	if (fabs(a->y - b->y) > THRESH)
		return true;

	if (fabs(a->z - b->z) > THRESH)
		return true;

	return false;
}

//-------------------------------------------------------------------------------------------------
inline Bool isAngleDifferent(Real a, Real b)
{
	// this is necessary because PhysicsBehavior may generate tiny changes even when
	// "standing still", due to roundoff errors. It's important that we only invalidate
	// the PartitionManager stuff when the pos/orientation really changes (for efficiency purposes)
	// so we must put in some cleverness...

	const Real THRESH = 0.01f;	// in radians, this is approx 1/2 degree.

	if (fabs(a - b) > THRESH)
		return true;

	return false;
}

//-------------------------------------------------------------------------------------------------
void Object::reactToTurretChange( WhichTurretType turret, Real oldRotation, Real oldPitch )
{
	Real currentRotation = 0.0f;
	Real currentPitch = 0.0f;
	if( getAI() )
	{
		getAI()->getTurretRotAndPitch( turret, &currentRotation, &currentPitch );
	}
	Bool rotationChange = (currentRotation != oldRotation);
//	Bool pitchChange = (currentPitch != oldPitch);

	if( rotationChange )
	{
		if (getContain())
			getContain()->containReactToTransformChange();
	}
}

//-------------------------------------------------------------------------------------------------
//DECLARE_PERF_TIMER(Object_reactToTransformChange)
void Object::reactToTransformChange(const Matrix3D* oldMtx, const Coord3D* oldPos, Real oldAngle)
{
	//USE_PERF_TIMER(Object_reactToTransformChange)
	if(_isnan(getPosition()->x) || _isnan(getPosition()->y) || _isnan(getPosition()->z)) {
		DEBUG_CRASH(("Object pos is nan."));
		TheGameLogic->destroyObject(this);
	}
	if (m_drawable)
	{
  	m_drawable->setTransformMatrix( this->getTransformMatrix() );
	}

	Bool posDiff = isPosDifferent(oldPos, getPosition());
	Bool angDiff = isAngleDifferent(oldAngle, getOrientation());

	if (posDiff || angDiff)
	{
		if (m_partitionData)
			m_partitionData->makeDirty(true);

		if (getContain())
			getContain()->containReactToTransformChange();
	}

	if (posDiff)
	{
		setTriggerAreaFlagsForChangeInPosition(); // Update for entered/exited

		Region3D mapExtent;
		TheTerrainLogic->getExtent(&mapExtent);
		if (mapExtent.isInRegionNoZ(*getPosition()))
			m_privateStatus &= ~OFF_MAP;
		else
			m_privateStatus |= OFF_MAP;
	}
}

//-------------------------------------------------------------------------------------------------
ObjectShroudStatus Object::getShroudedStatus(Int playerIndex) const
{
	if (getTemplate()->isKindOf( KINDOF_ALWAYS_VISIBLE ))
		return OBJECTSHROUD_CLEAR;

	if (m_partitionData)
		return m_partitionData->getShroudedStatus(playerIndex);

	// This can happen for objects removed from the partition system (e.g.,
	// for soldiers that are garrisoned inside a building).
	return OBJECTSHROUD_CLEAR;
}

//-------------------------------------------------------------------------------------------------
/** Something is attempting to damage this object */
//-------------------------------------------------------------------------------------------------
void Object::attemptDamage( DamageInfo *damageInfo )
{
	BodyModuleInterface* body = getBodyModule();
	if (body)
		body->attemptDamage( damageInfo );

	/// @todo track damage dealt/attempted

	//
	// if actual damage occurred, and this is an object owned by the local player we
	// might do a radar event for under attack. Note that we do not even try
	// to do radar events for DAMAGE_PENALTY as that damage type is a type of damage
	// that occurs with explicit player knowledge
	//
	if( damageInfo->out.m_actualDamageDealt > 0.0f &&
			damageInfo->in.m_damageType != DAMAGE_PENALTY &&
			damageInfo->in.m_damageType != DAMAGE_HEALING &&
			!BitIsSet(damageInfo->in.m_sourcePlayerMask, getControllingPlayer()->getPlayerMask()) &&
			m_radarData != nullptr &&
			isLocallyControlled() )
		TheRadar->tryUnderAttackEvent( this );

}

//-------------------------------------------------------------------------------------------------
void Object::attemptHealing(Real amount, const Object* source)
{
	BodyModuleInterface* body = getBodyModule();
	if (body)
	{
		DamageInfo damageInfo;
		damageInfo.in.m_damageType = DAMAGE_HEALING;
		damageInfo.in.m_deathType = DEATH_NONE;
		damageInfo.in.m_sourceID = source ? source->getID() : INVALID_ID;
		damageInfo.in.m_amount = amount;
		body->attemptHealing( &damageInfo );
	}
}

ObjectID Object::getSoleHealingBenefactor() const
{
	UnsignedInt now = TheGameLogic->getFrame();
	if( now > m_soleHealingBenefactorExpirationFrame )
		return INVALID_ID;

	return	m_soleHealingBenefactorID;

}

Bool Object::attemptHealingFromSoleBenefactor ( Real amount, const Object* source, UnsignedInt duration )
{///< for the non-stacking healers like ambulance and propaganda

	if( ! source ) // sanity
		return FALSE;

	UnsignedInt now = TheGameLogic->getFrame();
	ObjectID id = source->getID();

// Either it is ok to accept healing from any who offer or this is my guy, calling again
	if( now > m_soleHealingBenefactorExpirationFrame || m_soleHealingBenefactorID == id )
	{
		m_soleHealingBenefactorID = id;
		m_soleHealingBenefactorExpirationFrame = now + duration;

		BodyModuleInterface* body = getBodyModule();
		if (body)
		{
			DamageInfo damageInfo;
			damageInfo.in.m_damageType = DAMAGE_HEALING;
			damageInfo.in.m_deathType = DEATH_NONE;
			damageInfo.in.m_sourceID = source ? source->getID() : INVALID_ID;
			damageInfo.in.m_amount = amount;
			body->attemptHealing( &damageInfo );
		}

		return TRUE;
	}

	return FALSE;

}


//-------------------------------------------------------------------------------------------------
Real Object::estimateDamage( DamageInfoInput& damageInfo ) const
{
	BodyModuleInterface* body = getBodyModule();
	if (body)
		return body->estimateDamage( damageInfo );

	return 0.0f;
}

//-------------------------------------------------------------------------------------------------
/** Do so much damage to an object that it will certainly die */
//-------------------------------------------------------------------------------------------------
void Object::kill()
{
	DamageInfo damageInfo;

	// Do unmodifiable damage equal to their max health to kill.
	damageInfo.in.m_damageType = DAMAGE_UNRESISTABLE;
	damageInfo.in.m_deathType = DEATH_NORMAL;
	damageInfo.in.m_sourceID = INVALID_ID;
	damageInfo.in.m_amount = getBodyModule()->getMaxHealth();
	attemptDamage( &damageInfo );

	DEBUG_ASSERTCRASH(!damageInfo.out.m_noEffect, ("Attempting to kill an unKillable object (InactiveBody?)"));

}

//-------------------------------------------------------------------------------------------------
/** Restore max health to this Object */
//-------------------------------------------------------------------------------------------------
void Object::healCompletely()
{
	attemptHealing(HUGE_DAMAGE_AMOUNT, nullptr);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::setEffectivelyDead(Bool dead)
{
	if (dead)
		BitSet(m_privateStatus, EFFECTIVELY_DEAD);
	else
		BitClear(m_privateStatus, EFFECTIVELY_DEAD);

	if (dead)
	{
		if( m_radarData )
			TheRadar->removeObject( this );
	}
}

//-------------------------------------------------------------------------------------------------
void Object::setCaptured(Bool isCaptured)
{
	if (isCaptured)
		BitSet(m_privateStatus, CAPTURED);
	else
	{
		DEBUG_LOG(("Clearing Captured Status. This should never happen. jkmcd"));
		BitClear(m_privateStatus, CAPTURED);
	}

	// No need to see if we should skip updates, this flag has no effect on skipping updates.
}



//-------------------------------------------------------------------------------------------------
Bool Object::isStructure() const
{
	return isKindOf(KINDOF_STRUCTURE);
}

//-------------------------------------------------------------------------------------------------
Bool Object::isFactionStructure() const
{
	KindOfMaskType bits;
	bits.set(KINDOF_FS_POWER);
	bits.set(KINDOF_FS_FACTORY);
	bits.set(KINDOF_FS_BASE_DEFENSE);
	bits.set(KINDOF_FS_TECHNOLOGY);

	return isAnyKindOf(bits);
}

//-------------------------------------------------------------------------------------------------
Bool Object::isNonFactionStructure() const
{
	return isStructure() && !isFactionStructure();
}

//-------------------------------------------------------------------------------------------------
void Object::setReceivingDifficultyBonus(Bool receive)
{
	if (receive == m_isReceivingDifficultyBonus) {
		return;
	}

	m_isReceivingDifficultyBonus = receive;
	getControllingPlayer()->friend_applyDifficultyBonusesForObject(this, m_isReceivingDifficultyBonus);
}

//-------------------------------------------------------------------------------------------------
//- DISABLEDNESS STUFF ----------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::setDisabled( DisabledType type )
{
	setDisabledUntil(type, FOREVER);
}

//-------------------------------------------------------------------------------------------------
void Object::setDisabledUntil( DisabledType type, UnsignedInt frame )
{
	Bool edgeCase = !isDisabled();

	if( type < 0 || type >= DISABLED_COUNT )
	{
		DEBUG_CRASH( ("Invalid disabled type value %d specified -- doesn't not exist!", type ) );
		return;
	}

	//Handle audio events!
 	AudioEventRTS sound;
	if( type == DISABLED_UNMANNED && !isKindOf( KINDOF_DRONE ) )
	{
		//We've been sniped! Play a splatter sound for the pilot losing his face.
		sound = TheAudio->getMiscAudio()->m_splatterVehiclePilotsBrain;
		sound.setPosition( getPosition() );
		TheAudio->addAudioEvent( &sound );
	}
	else if( type == DISABLED_UNDERPOWERED || type == DISABLED_EMP || type == DISABLED_HACKED )
	{
		//We've lost power -- make sure we aren't already out of power as the sounds shouldn't happen
		//if you were already disabled.
		if( !isDisabledByType( DISABLED_UNDERPOWERED ) &&
				!isDisabledByType( DISABLED_EMP ) &&
				!isDisabledByType( DISABLED_HACKED ) )
		{
			if( isKindOf( KINDOF_STRUCTURE ) )
			{
				sound = TheAudio->getMiscAudio()->m_buildingDisabled;
				sound.setPosition( getPosition() );
				TheAudio->addAudioEvent( &sound );
			}
			else if( isKindOf( KINDOF_VEHICLE ) )
			{
				sound = TheAudio->getMiscAudio()->m_vehicleDisabled;
				sound.setPosition( getPosition() );
				TheAudio->addAudioEvent( &sound );
			}
		}
	}

	if( m_disabledTillFrame[ type ] != frame )
	{
		// an edge-test for disabledness, for type. This INCREMENTS m_pauseCount
		// srj sez: HELD never disables special powers.
		if ( type != DISABLED_HELD && !isDisabledByType( type ) )
			pauseAllSpecialPowers( TRUE );

		m_disabledTillFrame[ type ] = frame;
		m_disabledMask.set( type, frame > TheGameLogic->getFrame() );

		if( m_drawable )
		{
			if( isDisabled() )
			{
				// Held does not tint anybody.  If we are multiply disabled, the other setting will hit the tint,
				// and in clear, only-held and not-disabled are both causes to untint.
				// Doh. Also shouldn't be tinting when disabled by scripting.
				// Doh^2. Also shouldn't be CLEARING tinting if we're disabling by held or script disabledness
				// Doh^3. Unmanned is no tint too
				if( type != DISABLED_HELD && type != DISABLED_SCRIPT_DISABLED && type != DISABLED_UNMANNED )
				{
					m_drawable->setTintStatus( TINT_STATUS_DISABLED );
				}
			}
		}

		ContainModuleInterface *contain = getContain();
		if ( contain )
		{
			Object *rider = (Object*)contain->friend_getRider();
			if ( rider )
			{
				rider->setDisabledUntil(type, frame);
			}
		}
	}

	if( type == DISABLED_UNMANNED && !isKindOf( KINDOF_DRONE ) )
	{
		//strange but true: If I am a carbomb,
		//my driver actually has a dead-man's
		//trigger for my dynamite...
		//If he gets sniped, I blow up! Wheeee!

		WeaponSetFlags flags;
		flags.set( WEAPONSET_CARBOMB );
		const WeaponTemplateSet* set = getTemplate()->findWeaponTemplateSet( flags );
		if( set && set->testWeaponSetFlag( WEAPONSET_CARBOMB ) )
		{
			Object* sniper = TheGameLogic->findObjectByID( getBodyModule()->getLastDamageInfo()->in.m_sourceID );
			if ( sniper )
				sniper->scoreTheKill( this );

			kill();
		}
		else
		{
			//This vehicle's pilot has been sniped, so we want to clear the veterancy rating (if any)
			ExperienceTracker *xpTracker = getExperienceTracker();
			if( xpTracker )
			{
				xpTracker->setExperienceAndLevel( 0 );
			}
			//Not only that, but it also loses any healing bonuses it may have earned in its prior life
			{
				static const NameKeyType key_AutoHealBehavior = NAMEKEY("AutoHealBehavior");
				AutoHealBehavior* autoHeal = (AutoHealBehavior*)(findUpdateModule( key_AutoHealBehavior ));
				if (autoHeal)
					autoHeal->undoUpgrade();


			}
		}

	}

	// This will only be called if we were NOT disabled before coming into this function.
	if (edgeCase) {
		onDisabledEdge(true);
	}
}

//-------------------------------------------------------------------------------------------------
Bool Object::clearDisabled( DisabledType type )
{
	if( type < 0 || type >= DISABLED_COUNT )
	{
		DEBUG_CRASH( ("Invalid disabled type value %d specified -- doesn't not exist!", type ) );
		return FALSE;
	}

	if (!isDisabledByType(type)) {
		return FALSE;
	}

	if( type == DISABLED_UNDERPOWERED || type == DISABLED_EMP || type == DISABLED_HACKED )
	{
		//We've regained power-- make sure we aren't still disabled by another type.
	 	AudioEventRTS sound;
		if( (!isDisabledByType( DISABLED_UNDERPOWERED ) || type == DISABLED_UNDERPOWERED ) &&
				(!isDisabledByType( DISABLED_EMP ) || type == DISABLED_EMP ) &&
				(!isDisabledByType( DISABLED_HACKED ) || type == DISABLED_HACKED ) )
		{
			if( isKindOf( KINDOF_STRUCTURE ) )
			{
				sound = TheAudio->getMiscAudio()->m_buildingReenabled;
				sound.setPosition( getPosition() );
				TheAudio->addAudioEvent( &sound );
			}
			else if( isKindOf( KINDOF_VEHICLE ) )
			{
				sound = TheAudio->getMiscAudio()->m_vehicleReenabled;
				sound.setPosition( getPosition() );
				TheAudio->addAudioEvent( &sound );
			}
		}
	}


	// an edge-test for disabledness, for type. This DECREMENTS m_pauseCount
	// srj sez: HELD never disables special powers.
	if ( type != DISABLED_HELD && isDisabledByType( type ) )
		pauseAllSpecialPowers( FALSE );

	ContainModuleInterface *contain = getContain();
	if ( contain )
	{
		// We explicitly pass stuff in up in the set, so we need to turn it off if it is a forever type
		Object *rider = (Object*)contain->friend_getRider();
		if( rider  &&  (m_disabledTillFrame[ type ] == FOREVER) )
		{
			rider->clearDisabled(type);
		}
	}

	m_disabledTillFrame[ type ] = NEVER;
	m_disabledMask.set( type, 0 );

	DisabledMaskType exceptions;
	exceptions.set(DISABLED_HELD);
	exceptions.set(DISABLED_SCRIPT_DISABLED);
	exceptions.set(DISABLED_UNMANNED);

	// to clarify, if I am NOT disabled by anything other than DISABLED_HELD, or DISABLED_SCRIPT_DISABLED
	if( !isDisabled() || getDisabledFlags().countInverseIntersection( exceptions ) == 0 )
	{
		if (m_drawable)
			m_drawable->clearTintStatus( TINT_STATUS_DISABLED );
	}

	checkDisabledStatus();// in case we just edged

	// if we're no longer disabled by anything, then call the edge function.
	if (!isDisabled()) {
		onDisabledEdge(false);
	}
	return TRUE;
}


//-------------------------------------------------------------------------------------------------
//Checks any timers and clears disabled statii that have expired.
//-------------------------------------------------------------------------------------------------
void Object::checkDisabledStatus()
{
	UnsignedInt now = TheGameLogic->getFrame();
	for( int i = 0; i < DISABLED_COUNT; i++ )
	{
		DisabledType type = (DisabledType)i;
		if( isDisabledByType( type ) )
		{
			if ( now >= m_disabledTillFrame[ i ] )
			{
				clearDisabled( type ); // This will also DECREMENT m_pauseCount in all specialpowers
				m_disabledMask.set( type, 0 );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void Object::pauseAllSpecialPowers( const Bool disabling ) const
{
	for (BehaviorModule** m = m_behaviors; *m; ++m)
	{
		SpecialPowerModuleInterface* sp = (*m)->getSpecialPower();
		if (!sp)
			continue;

		sp->pauseCountdown( disabling );// So it will pause if we are disabling.
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/** Clear the previous entered/exited flags. */
//-------------------------------------------------------------------------------------------------
void Object::updateTriggerAreaFlags()
{
	Int j = 0;
	// Update the flags, and remove any trigger areas that this object isn't inside.
	for (Int i=0; i<m_numTriggerAreasActive; i++)
	{
		if (!m_triggerInfo[j].isInside)
			continue;
		m_triggerInfo[j].entered = false;
		m_triggerInfo[j].exited = false;
		m_triggerInfo[j].isInside = m_triggerInfo[i].isInside;
		m_triggerInfo[j].pTrigger = m_triggerInfo[i].pTrigger;
		j++;
	}
	m_numTriggerAreasActive = j;
}

//-------------------------------------------------------------------------------------------------
void Object::onCollide( Object *other, const Coord3D *loc, const Coord3D *normal )
{
	for (BehaviorModule** m = m_behaviors; *m; ++m)
	{
		CollideModuleInterface* collide = (*m)->getCollide();
		if (!collide)
			continue;

		// check each time thru the loop, in case a collide module sets it
		if( getStatusBits().test( OBJECT_STATUS_NO_COLLISIONS ) )
		{
#ifdef DEBUG_CRC
			//DEBUG_LOG(("Object::onCollide() - OBJECT_STATUS_NO_COLLISIONS set"));
#endif
			break;
		}
#ifdef DEBUG_CRC
		//DEBUG_LOG(("Object::onCollide() - calling collide module"));
#endif
		collide->onCollide(other, loc, normal);
	}
}

//-------------------------------------------------------------------------------------------------
Bool Object::isSalvageCrate() const
{
	for( BehaviorModule** m = m_behaviors; *m; ++m )
	{
		CollideModuleInterface* collide = (*m)->getCollide();
		if( collide && collide->isSalvageCrateCollide() )
		{
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/**
	Our owning player is telling us to recheck our UpgradeModules, as an upgrade has completed
 */
void Object::updateUpgradeModules()
{
	const UpgradeMaskType& playerMask = getControllingPlayer()->getCompletedUpgradeMask();
	const UpgradeMaskType& objectMask = getObjectCompletedUpgradeMask();
	UpgradeMaskType maskToCheck = playerMask;
	maskToCheck.set( objectMask );
	// We need to add in all of the already owned upgrades to handle "AND" requiring upgrades.
	// We combine all the masks in case someone has a Object AND Player combination

	for (BehaviorModule** module = m_behaviors; *module; ++module)
	{
		UpgradeModuleInterface* upgrade = (*module)->getUpgrade();
		if (!upgrade)
			continue;

		if( !upgrade->isAlreadyUpgraded() )
		{
			upgrade->attemptUpgrade( maskToCheck );
		}
	}
}

//-------------------------------------------------------------------------------------------------
//This function sucks.
//It was added for objects that can disguise as other objects and contain upgraded subobject overrides.
//A concrete example is the bomb truck. Different payloads are displayed based on which upgrades have been
//made. When the bomb truck disguises as something else, these subobjects are lost because the vector is
//stored in W3DDrawModule. When we revert back to the original bomb truck, we call this function to
//recalculate those upgraded subobjects.
//-------------------------------------------------------------------------------------------------
void Object::forceRefreshSubObjectUpgradeStatus()
{
	for (BehaviorModule** module = m_behaviors; *module; ++module)
	{
		UpgradeModuleInterface* upgrade = (*module)->getUpgrade();
		if (!upgrade)
			continue;

		if( upgrade->isSubObjectsUpgrade() )
		{
			upgrade->forceRefreshUpgrade();
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Returns whether an object entered or exited an area. */
//-------------------------------------------------------------------------------------------------
Bool Object::didEnterOrExit() const
{
	if (isKindOf(KINDOF_INERT)) {
		return FALSE;
	}
	// note that this needs to return true if we
	// entered or exited on the current frame OR
	// the previous frame... since the current execution
	// order is ScriptEngine, then ObjectUpdates,
	// enter/exits detected in ObjectUpdate on frame N
	// won't be noticed by the ScriptEngine till frame N+1.
	UnsignedInt now = TheGameLogic->getFrame();
	return m_enteredOrExitedFrame == now || m_enteredOrExitedFrame == now - 1;
}

//-------------------------------------------------------------------------------------------------
/** Returns whether an object entered an area. */
//-------------------------------------------------------------------------------------------------
Bool Object::didEnter(const PolygonTrigger *pTrigger) const
{
	if (!didEnterOrExit())
		return false;

	DEBUG_ASSERTCRASH(!isKindOf(KINDOF_INERT), ("Asking whether an inert object entered or exited. This is invalid."));

	for (Int i=0; i<m_numTriggerAreasActive; i++)
	{
		if (m_triggerInfo[i].entered && m_triggerInfo[i].pTrigger == pTrigger)
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** Returns whether an object entered an area. */
//-------------------------------------------------------------------------------------------------
Bool Object::didExit(const PolygonTrigger *pTrigger) const
{
	if (!didEnterOrExit())
		return false;

	DEBUG_ASSERTCRASH(!isKindOf(KINDOF_INERT), ("Asking whether an inert object entered or exited. This is invalid."));
	for (Int i=0; i<m_numTriggerAreasActive; i++)
	{
		if (m_triggerInfo[i].exited && m_triggerInfo[i].pTrigger == pTrigger)
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** Returns whether an object is inside an area. */
//-------------------------------------------------------------------------------------------------
Bool Object::isInside(const PolygonTrigger *pTrigger) const
{
	DEBUG_ASSERTCRASH(!isKindOf(KINDOF_INERT), ("Asking whether an inert is inside a trigger area. This is invalid."));

	for (Int i=0; i<m_numTriggerAreasActive; i++)
	{
		if (m_triggerInfo[i].isInside && m_triggerInfo[i].pTrigger == pTrigger)
			return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
/** Get production exit interface in object is present */
// ------------------------------------------------------------------------------------------------
ExitInterface *Object::getObjectExitInterface() const
{
	ExitInterface *exitInterface = nullptr;

	for( BehaviorModule **umod = m_behaviors; *umod; ++umod )
	{
		if( (exitInterface = (*umod)->getUpdateExitInterface()) != nullptr )
			break;
	}

	// If you don't have a fancy one, you may have one from your contain module,
	// since if you can contain something, they will need to get out.
	if( exitInterface == nullptr )
	{
		ContainModuleInterface *cmod = getContain();
		if( cmod )
		{
			exitInterface = cmod->getContainExitInterface();
		}
	}

	return exitInterface;

}

//-------------------------------------------------------------------------------------------------
/** Checks the object against trigger areas when the position changes. */
//-------------------------------------------------------------------------------------------------
void Object::setTriggerAreaFlagsForChangeInPosition()
{
	// projectiles cannot trigger areas. (jkmcd)
	// neither can inert objects, like the radar ping, etc. (jkmcd)
	if (isKindOf(KINDOF_PROJECTILE) || isKindOf(KINDOF_INERT))
		return;

	ICoord3D iPos;
	Coord3D pos = *getPosition();
	iPos.x = REAL_TO_INT(pos.x);
	iPos.y = REAL_TO_INT(pos.y);
	iPos.z = 0; // Trigger areas compare on xy only.
	if (m_iPos.x == iPos.x && m_iPos.y == iPos.y)
	{
		return; // didn't move enough to change integer position.
	}

	if (getAIUpdateInterface())
	{
		TheAI->pathfinder()->updatePos(this, getPosition());
	}

	UnsignedInt now = TheGameLogic->getFrame();
	if (m_enteredOrExitedFrame != 0 && m_enteredOrExitedFrame != now)
		updateTriggerAreaFlags();

	// Check for exited.
	Int i;
	for (i=0; i<m_numTriggerAreasActive; i++)
	{
		if (!m_triggerInfo[i].pTrigger->pointInTrigger(m_iPos))
		{
			m_triggerInfo[i].isInside = false;
			m_triggerInfo[i].exited = true;
			m_enteredOrExitedFrame = now;
			if (m_team)
				m_team->setEnteredExited();
			TheGameLogic->updateObjectsChangedTriggerAreas();
#ifdef RTS_DEBUG
			//TheScriptEngine->AppendDebugMessage("Object exited.", false);
#endif
		}
	}

	m_iPos = iPos;

	for (const PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext())
	{
		Bool skip = false;
		for (i = 0; i < m_numTriggerAreasActive; i++)
		{
			if (m_triggerInfo[i].pTrigger == pTrig)
			{
				// Already handled this one in the check for exited above.
				skip = true;
				break;
			}
		}
		if (skip)
			continue;
		if (pTrig->pointInTrigger(m_iPos))
		{
			if (m_numTriggerAreasActive < MAX_TRIGGER_AREA_INFOS)
			{
				m_triggerInfo[m_numTriggerAreasActive].isInside = true;
				m_triggerInfo[m_numTriggerAreasActive].entered = true;
				m_triggerInfo[m_numTriggerAreasActive].exited = false;
				m_triggerInfo[m_numTriggerAreasActive].pTrigger = pTrig;
				m_enteredOrExitedFrame = now;
				if (m_team)
					m_team->setEnteredExited();
				TheGameLogic->updateObjectsChangedTriggerAreas();
				++m_numTriggerAreasActive;
#ifdef RTS_DEBUG
				//TheScriptEngine->AppendDebugMessage("Object entered.", false);
#endif
			}
			else
			{
				// Shouldn't happen.
				static Bool didWarn = false;
				if (!didWarn)
				{
					didWarn = true;
					TheScriptEngine->AppendDebugMessage("***WARNING - Too many nested trigger areas. ***", true);
				}
			}
		}

	}

}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool Object::isInList(Object **pListHead) const
{
	Bool result = m_prev || m_next || *pListHead == this;
#ifdef INTENSE_DEBUG
	Bool found = false;
	for (Object* o = *pListHead; o; o = o->m_next)
	{
		if (o == this)
		{
			found = true;
			break;
		}
	}
	DEBUG_ASSERTCRASH(found==result,("inconsistent links in Object::isInList"));
#endif
	return result;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::prependToList(Object **pListHead)
{
	DEBUG_ASSERTCRASH(!isInList(pListHead), ("obj is already in a list"));

	m_prev = nullptr;
	m_next = *pListHead;
	if (*pListHead)
		(*pListHead)->m_prev = this;
	*pListHead = this;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::setLayer(PathfindLayerEnum layer)
{
	if (layer!=m_layer) {
#define no_SET_LAYER_INTENSE_DEBUG
#ifdef SET_LAYER_INTENSE_DEBUG
		DEBUG_LOG(("Changing layer from %d to %d", m_layer, layer));
		if (m_layer != LAYER_GROUND) {
			if (TheTerrainLogic->objectInteractsWithBridgeLayer(this, m_layer)) {
				DEBUG_CRASH(("Probably shouldn't be changing layer. jba."));
			}
		}
#endif
		TheAI->pathfinder()->removePos(this);
		m_layer = layer;
		TheAI->pathfinder()->updatePos(this, getPosition());
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::setDestinationLayer(PathfindLayerEnum layer)
{
	if (layer!=m_destinationLayer) {
		m_destinationLayer = layer;
	}
}

// ------------------------------------------------------------------------------------------------
/** Set unique ID */
// ------------------------------------------------------------------------------------------------
void Object::setID( ObjectID id )
{

	// sanity
	DEBUG_ASSERTCRASH( id != INVALID_ID, ("Object::setID - Invalid id") );

	// if id hasn't changed do nothing
	if( m_id == id )
		return;

	// remove this objects previous id from the lookup table
	TheGameLogic->removeObjectFromLookupTable( this );

	// assign new id
	m_id = id;

	// add new id to lookup table
	TheGameLogic->addObjectToLookupTable( this );

}

// ------------------------------------------------------------------------------------------------
Real Object::calculateHeightAboveTerrain() const
{
	const Coord3D* pos = getPosition();
	Real terrainZ = TheTerrainLogic->getLayerHeight( pos->x, pos->y, m_layer );
	Real myZ = pos->z;
	return myZ - terrainZ;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::removeFromList(Object **pListHead)
{
	if (m_next)
		m_next->m_prev = m_prev;

	if (m_prev)
		m_prev->m_next = m_next;
	else
		*pListHead = m_next;

	m_prev = nullptr;
	m_next = nullptr;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::friend_prepareForMapBoundaryAdjust()
{
	// NOTE - DO NOT remove from pathfind map. jba.
	// NO NO. jba. TheAI->pathfinder()->removeObjectFromPathfindMap( this );

	// remove from the radar, remove from the partition manager
	TheRadar->removeObject(this);
	ThePartitionManager->unRegisterObject(this);

	// The whole PartitionManager and all of the Looker data is about to be blown away,
	// so forget what I think I have done
	m_partitionLastLook->reset();
	m_partitionLastShroud->reset();

	m_partitionLastThreat->reset();
	m_partitionLastValue->reset();

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::friend_notifyOfNewMapBoundary()
{
	ThePartitionManager->registerObject(this);
	TheRadar->addObject(this);
	TheAI->pathfinder()->addObjectToPathfindMap( this );

	// Now that the PartitionManager has finished its reset, we need to relook
	handlePartitionCellMaintenance();

	Region3D mapExtent;
	TheTerrainLogic->getExtent(&mapExtent);
	if (mapExtent.isInRegionNoZ(*getPosition()))
		m_privateStatus &= ~OFF_MAP;
	else
		m_privateStatus |= OFF_MAP;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Object::calcNaturalRallyPoint(Coord2D *pt)
{
	const Matrix3D *transform = getTransformMatrix();
	Vector3 v;

	//
	// get the natural rally point from the template, this coord is in model space relative
	// to the model (0,0,0)
	//
/*
	const Coord3D *naturalRallyPoint;
  naturalRallyPoint = m_template->getNaturalRallyPoint();
	v.X = naturalRallyPoint->x;
	v.Y = naturalRallyPoint->y;
	v.Z = naturalRallyPoint->z;
*/
	v.Set( 0, 0, 0 );

	// transform the point into world space
	transform->Transform_Vector( *transform, v, &v );

	// we're only concerned with the 2D elements for now
	pt->x = v.X;
	pt->y = v.Y;

}

//-------------------------------------------------------------------------------------------------
Module* Object::findModule(NameKeyType key) const
{
	Module* m = nullptr;

	for (BehaviorModule** b = m_behaviors; *b; ++b)
	{
		if ((*b)->getModuleNameKey() == key)
		{
#ifdef INTENSE_DEBUG
			if (m == nullptr)
			{
				m = *b;
			}
			else
			{
				DEBUG_CRASH(("Duplicate modules found for name %s!",TheNameKeyGenerator->keyToName(key).str()));
			}
#else
			m = *b;
			break;
#endif
		}
	}

	return m;
}

//-------------------------------------------------------------------------------------------------
/**
 * Returns true if object is currently able to move.
 */
Bool Object::isMobile() const
{
	if (isKindOf(KINDOF_IMMOBILE))
		return false;

	if( isDisabled() )
		return false;

	return true;
}

//-------------------------------------------------------------------------------------------------
void Object::scoreTheKill( const Object *victim )
{
	// Do stuff that has nothing to do with experience points here, like tell our Player we killed something
	/// @todo Multiplayer score hook location?

	Player* victimController = victim->getControllingPlayer();
	// if the other player is not a playable side (i.e. they are civilian, observer, whatever)
	// we shouldn't count the kill.
	if (victimController->isPlayableSide() == FALSE)
	{
		return;
	}


	if ( victim->isKindOf( KINDOF_IGNORED_IN_GUI ) )
		return;


	Player* controller = getControllingPlayer();

	if (victimController)
	{
		victimController->getScoreKeeper()->addObjectLost(victim);
	}

	Relationship r = getRelationship(victim);
	if (r != ENEMIES)
		return;

	// Don't count kills that I do on my own buildings or units, cause thats just silly.
	if (controller == victimController)
	{
		return;
	}

	if (controller)
	{
		controller->getScoreKeeper()->addObjectDestroyed(victim);
		controller->addSkillPointsForKill(this, victim);
		controller->doBountyForKill(this, victim);
	}

	// Now handle experience, if we can gain any
	if (m_experienceTracker && m_experienceTracker->isAcceptingExperiencePoints())
	{
		// srj sez: per dustin, no experience (et al) for killing things under construction.
		if (!victim->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		{
			Int experienceValue = victim->getExperienceTracker()->getExperienceValue( this );
			getExperienceTracker()->addExperiencePoints( experienceValue );
		}
	}
}

//-------------------------------------------------------------------------------------------------
VeterancyLevel Object::getVeterancyLevel() const
{
	return m_experienceTracker ? m_experienceTracker->getVeterancyLevel() : LEVEL_REGULAR;
}

//-------------------------------------------------------------------------------------------------
void Object::friend_bindToDrawable( Drawable *draw )
{
	m_drawable = draw;
	if (m_drawable)
	{
		ModelConditionFlags set;
		ModelConditionFlags clr;
		for (int i = 0; i < WEAPONSET_COUNT; ++i)
		{
			ModelConditionFlagType mcs = TheWeaponSetTypeToModelConditionTypeMap[i];
			if (m_curWeaponSetFlags.test(i))
				set.set(mcs);
			else
				clr.set(mcs);
		}
		if (TheGlobalData)
		{
			if (TheGlobalData->m_forceModelsToFollowTimeOfDay)
			{
				set.set(MODELCONDITION_NIGHT, (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT) ? 1 : 0);
			}

			if (TheGlobalData->m_forceModelsToFollowWeather)
			{
				set.set(MODELCONDITION_SNOW, (TheGlobalData->m_weather == WEATHER_SNOWY) ? 1 : 0);
			}
		}
		m_drawable->clearAndSetModelConditionFlags(clr, set);
	}

	for (BehaviorModule** b = m_behaviors; *b; ++b)
	{
		(*b)->onDrawableBoundToObject();
	}
}

//-------------------------------------------------------------------------------------------------
void Object::setSelectable(Bool selectable)
{
	m_isSelectable = selectable;
	if (m_drawable)
	{
		m_drawable->setSelectable(selectable);
	}
}

//-------------------------------------------------------------------------------------------------
Bool Object::isSelectable() const
{
	return getTemplate()->isKindOf(KINDOF_ALWAYS_SELECTABLE)
				|| (m_isSelectable
						&& !testStatus(OBJECT_STATUS_UNSELECTABLE)
						&& !isEffectivelyDead()
						&& !getTemplate()->isKindOf(KINDOF_DRONE)//Most drones are unselectable from being slaved, but the SpyDrone needs help
						);
}

//-------------------------------------------------------------------------------------------------
Bool Object::isMassSelectable() const
{
	return isSelectable() && !isKindOf(KINDOF_STRUCTURE);
}

//-------------------------------------------------------------------------------------------------
void Object::setWeaponSetFlag(WeaponSetType wst)
{
	m_curWeaponSetFlags.set(wst);
	m_weaponSet.updateWeaponSet(this);
	if (m_drawable)
	{
		m_drawable->setModelConditionState(TheWeaponSetTypeToModelConditionTypeMap[wst]);
	}
}

//-------------------------------------------------------------------------------------------------
void Object::clearWeaponSetFlag(WeaponSetType wst)
{
	m_curWeaponSetFlags.set(wst, 0);
	m_weaponSet.updateWeaponSet(this);
	if (m_drawable)
	{
		m_drawable->clearModelConditionState(TheWeaponSetTypeToModelConditionTypeMap[wst]);
	}
}

//-------------------------------------------------------------------------------------------------
Bool Object::hasSpecialPower( SpecialPowerType type ) const
{
	return TEST_SPECIALPOWERMASK( m_specialPowerBits, type );
}

//-------------------------------------------------------------------------------------------------
Bool Object::hasAnySpecialPower() const
{
  return SPECIALPOWERMASK_ANY_SET( m_specialPowerBits );
}

//-------------------------------------------------------------------------------------------------
void Object::onVeterancyLevelChanged( VeterancyLevel oldLevel, VeterancyLevel newLevel, Bool provideFeedback )
{
#if !RETAIL_COMPATIBLE_CRC
	// TheSuperHackers @bugfix Stubbjax 10/12/2025 Do not apply veterancy bonuses and animations for dead units.
	if (isEffectivelyDead())
		return;
#endif

	updateUpgradeModules();

	const UpgradeTemplate* up = TheUpgradeCenter->findVeterancyUpgrade(newLevel);
	if (up)
		giveUpgrade(up);

	BodyModuleInterface* body = getBodyModule();
	if (body)
		body->onVeterancyLevelChanged(oldLevel, newLevel, provideFeedback);

	switch (newLevel)
	{
		case LEVEL_REGULAR:
			clearWeaponSetFlag(WEAPONSET_VETERAN);
			clearWeaponSetFlag(WEAPONSET_ELITE);
			clearWeaponSetFlag(WEAPONSET_HERO);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_VETERAN);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_ELITE);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_HERO);
			break;
		case LEVEL_VETERAN:
			setWeaponSetFlag(WEAPONSET_VETERAN);
			clearWeaponSetFlag(WEAPONSET_ELITE);
			clearWeaponSetFlag(WEAPONSET_HERO);
			setWeaponBonusCondition(WEAPONBONUSCONDITION_VETERAN);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_ELITE);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_HERO);
			break;
		case LEVEL_ELITE:
			clearWeaponSetFlag(WEAPONSET_VETERAN);
			setWeaponSetFlag(WEAPONSET_ELITE);
			clearWeaponSetFlag(WEAPONSET_HERO);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_VETERAN);
			setWeaponBonusCondition(WEAPONBONUSCONDITION_ELITE);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_HERO);
			break;
		case LEVEL_HEROIC:
			clearWeaponSetFlag(WEAPONSET_VETERAN);
			clearWeaponSetFlag(WEAPONSET_ELITE);
			setWeaponSetFlag(WEAPONSET_HERO);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_VETERAN);
			clearWeaponBonusCondition(WEAPONBONUSCONDITION_ELITE);
			setWeaponBonusCondition(WEAPONBONUSCONDITION_HERO);
			break;
	}

	Bool doAnimation = provideFeedback
		&& newLevel > oldLevel
		&& !isKindOf(KINDOF_IGNORED_IN_GUI)
		&& isLogicallyVisible();

	if (doAnimation)
		createVeterancyLevelFX(oldLevel, newLevel);
}

void Object::createVeterancyLevelFX(VeterancyLevel oldLevel, VeterancyLevel newLevel)
{
#if RETAIL_COMPATIBLE_CRC
	if (isEffectivelyDead())
		return;
#endif

	if (TheGameLogic->getDrawIconUI())
	{
		if( TheAnim2DCollection && TheGlobalData->m_levelGainAnimationName.isEmpty() == FALSE )
		{
			Anim2DTemplate *animTemplate = TheAnim2DCollection->findTemplate( TheGlobalData->m_levelGainAnimationName );

			Coord3D pos = *getPosition();
			pos.add(m_healthBoxOffset);

			TheInGameUI->addWorldAnimation( animTemplate,
																			&pos,
																			WORLD_ANIM_FADE_ON_EXPIRE,
																			TheGlobalData->m_levelGainAnimationDisplayTimeInSeconds,
																			TheGlobalData->m_levelGainAnimationZRisePerSecond);
		}

		AudioEventRTS soundToPlay = TheAudio->getMiscAudio()->m_unitPromoted;
		soundToPlay.setObjectID( getID() );
		TheAudio->addAudioEvent( &soundToPlay );
	}
}

//-------------------------------------------------------------------------------------------------
/**
 * Returns true if object currently has some kind of attack capability
 */
Bool Object::isAbleToAttack() const
{

	//******************************************************
	//********* AUTOMATICALLY FALSE CONDITIONS *************
	//******************************************************

	// For things that may or may not be able to normally attack, but are under a status condition
	if( getStatusBits().test( OBJECT_STATUS_NO_ATTACK ) )
		return false;

	// if we're contained within a transport we cannot attack unless it specifically allows us
	const Object *containedBy = getContainedBy();
	DEBUG_ASSERTCRASH( (containedBy == nullptr) || (containedBy->getContain() != nullptr), ("A %s thinks they are contained by something with no contain module!", getTemplate()->getName().str() ) );
	if( containedBy && containedBy->getContain() && !containedBy->getContain()->isPassengerAllowedToFire() )
		return false;


	// We can't fire if under construction
	if( testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) )
		return false;

	// or being sold
	if( testStatus(OBJECT_STATUS_SOLD) )
		return false;

	//We can't fire if we, as a portable structure, are aptly disabled
	if ( isKindOf( KINDOF_PORTABLE_STRUCTURE ) || isKindOf( KINDOF_SPAWNS_ARE_THE_WEAPONS ))
	{
		if( isDisabledByType( DISABLED_HACKED ) || isDisabledByType( DISABLED_EMP ) )
			return false;
	}

	//We can't fire if all our weapons are disabled!
	//Currently, only turreted weapons can be disabled.
	//ONLY DO THIS CHECK IF OUR UNIT DOESN'T HAVE THE
	//KINDOF_CAN_ATTACK flag... nuke cannons have disabled
	//turrets when not deployed, and need to be able to attack to deploy!
	//Strategy centers can't attack when bombardment isn't active!
	Bool anyEnabled = FALSE;
	Bool anyWeapon = FALSE;
	const AIUpdateInterface *ai = getAI();
	if( ai && !isKindOf( KINDOF_CAN_ATTACK ) )
	{
		for( Int i = 0; i < WEAPONSLOT_COUNT;	i++ )
		{
			//Find the weapon in this slot.
			Weapon* weapon = getWeaponInWeaponSlot( (WeaponSlotType)i );
			if( !weapon )
				continue;

			anyWeapon = TRUE;

			//We found a weapon, is it a turret?
			Real dummy;
			WhichTurretType tur = ai->getWhichTurretForWeaponSlot( (WeaponSlotType)i, &dummy );
			if( tur == TURRET_INVALID )
			{
				//Currently impossible to disable a non-turreted weapon, so we
				//have a non turreted weapon that is enabled. Quit.
				anyEnabled = TRUE;
				break;
			}

			if( ai->isTurretEnabled( tur ) )
			{
				//The turret is enable, meaning we have an enabled weapon. Quit.
				anyEnabled = TRUE;
				break;
			}
		}
		if( anyWeapon && !anyEnabled )
		{
			//We failed to find any active weapons.
			return FALSE;
		}
	}


	//***************************************
	//********* TRUE CONDITIONS *************
	//***************************************

	// for certain buildings
	if (isKindOf(KINDOF_CAN_ATTACK))
		return true;

	// for garrisonned buildings that can attack sometimes
	if( getStatusBits().test( OBJECT_STATUS_CAN_ATTACK ) )
		return true;

	// for weaponless transports.  This will make me think I can, but I will check if I literally can by looking
	// at passenger weapons in CanAttack.
	const ContainModuleInterface* contain = getContain();
	if( contain && contain->isPassengerAllowedToFire() && contain->getContainCount() > 0 )
		return true;

	// if we have AI and a weapon, assume we know how to use it
	if (getAIUpdateInterface() != nullptr && m_weaponSet.hasAnyWeapon())
	{

// actually, we don't want to do this; we want the troop crawler to be considered "able to attack"
// even if empty, so sayeth Dustin. (srj)
//		// special case: if the only damage we do is DEPLOY, we must have some guys contained.
//		if (m_weaponSet.hasSingleDamageType(DAMAGE_DEPLOY))
//		{
//			return contain->getContainCount() > 0;
//		}
//		else
		{
			return true;
		}
	}

	SpawnBehaviorInterface *spawnInterface = getSpawnBehaviorInterface();
	if( spawnInterface )
	{
		if( spawnInterface->canAnySlavesAttack() )
		{
			return TRUE;
		}
	}


//Default is no
	return false;
}

//-------------------------------------------------------------------------------------------------
/**
	* Mask/Un-Mask an object
	*/
void Object::maskObject( Bool mask )
{

	// set or clear the mask bit
	setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_MASKED ), mask );

	//
	// when masking objects they become unselected ... we do this in any situation for
	// any player cause you aren't allowed to select masked objects, if the object is not
	// selected (ie, belongs to another player) it's no big deal cause it won't be selected
	// anyway
	//

	if (mask)
		TheGameLogic->deselectObject(this, ~getControllingPlayer()->getPlayerMask(), TRUE);

}

//-------------------------------------------------------------------------------------------------
/*
 * returns true if the current locomotor is an airborne one
 */
Bool Object::isUsingAirborneLocomotor() const
{
	return ( m_ai && m_ai->getCurLocomotor() && ((m_ai->getCurLocomotor()->getLegalSurfaces() & LOCOMOTORSURFACE_AIR) != 0) );
}

//-------------------------------------------------------------------------------------------------
//THIS FUNCTION BELONGS AT THE OBJECT LEVEL BECAUSE THERE IS AT LEAST ONE SPECIAL UNIT
//(ANGRY MOB) WHICH NEEDS LOGIC-SIDE POSITION CALC'S...
//IT WOULD PROBABLY BE WISE TO MOVE ALL THE HARD-CODED DEFAULTS BELOW
//INTO A NEW Drawable::getHealthBox..() WHICH USES GEOM0INFO, MODEL DATA, INI DATA, ETC.
void Object::getHealthBoxPosition(Coord3D& pos) const
{
	pos = *getPosition();
	pos.z += getGeometryInfo().getMaxHeightAbovePosition() + 10;
	pos.add(m_healthBoxOffset);

	// this needs to get moved to the mobspawnerupdate
	if (isKindOf(KINDOF_MOB_NEXUS)) // quicker idiot test
	{
		pos.z += 20;// dear God, I confess my kluge, and repent.
	}
}

//-------------------------------------------------------------------------------------------------
//THIS FUNCTION BELONGS AT THE OBJECT LEVEL BECAUSE THERE IS AT LEAST ONE SPECIAL UNIT
//(ANGRY MOB) WHICH NEEDS LOGIC-SIDE POSITION CALC'S...
//IT WOULD PROBABLY BE WISE TO MOVE ALL THE HARD-CODED DEFAULTS BELOW
//INTO A NEW Drawable::getHealthBox..() WHICH USES GEOM0INFO, MODEL DATA, INI DATA, ETC.
Bool Object::getHealthBoxDimensions(Real &healthBoxHeight, Real &healthBoxWidth) const
{

#ifdef CALC_HEALTHBAR_FROM_HITPOINTS
	Real maxHP = getBodyModule()->getMaxHealth();

	if( isKindOf( KINDOF_STRUCTURE ) )
	{
		//enforce healthBoxHeightMinimum/Maximum
		healthBoxHeight = min(3.0f, max(5.0f, maxHP/50));
		//enforce healthBoxWidthMinimum/Maximum
		healthBoxWidth = min(150.0f, max(100.0f, maxHP/10));
		return true;
	}
	else if ( isKindOf(KINDOF_MOB_NEXUS) )
	{
		//enforce healthBoxHeightMinimum/Maximum
		healthBoxHeight = min(3.0f, max(5.0f, maxHP/50));
		//enforce healthBoxWidthMinimum/Maximum
		healthBoxWidth = min(100.0f, max(66.0f, maxHP/10));
		return true;
	}
	else if ( isKindOf( KINDOF_IGNORED_IN_GUI ) )
	{
		healthBoxHeight = 0;
		healthBoxWidth = 0;
		return false;
	}
	else
	{
		//enforce healthBoxHeightMinimum/Maximum
		healthBoxHeight = min(3.0f, max(5.0f, maxHP/50));
		//enforce healthBoxWidthMinimum/Maximum
		healthBoxWidth = min(150.0f, max(35.0f, maxHP/10));
		return true;
	}
#else

	if ( isKindOf( KINDOF_IGNORED_IN_GUI ) )
	{
		healthBoxHeight = 0;
		healthBoxWidth = 0;
		return false;
	}

	//just add the major and minor axes
	Real size = MAX(20.0f, MIN(150.0f, (getGeometryInfo().getMajorRadius() + getGeometryInfo().getMinorRadius())) );
	healthBoxHeight = 3.0f;
	healthBoxWidth = MAX(20.0f, size * 2.0f);
	return TRUE;

#endif

}


//-------------------------------------------------------------------------------------------------
/**
 * Update this object instance with properties from the map object
 *
 */
void Object::updateObjValuesFromMapProperties(Dict* properties)
{
	Bool exists;

	AsciiString valStr;
	Bool valBool = false;
	Int valInt = 0;
	Real valReal = 0.0f;

	valStr = properties->getAsciiString(TheKey_objectName, &exists);
	if (exists) {
		setName(valStr);
	}

	valInt = properties->getInt(TheKey_objectMaxHPs, &exists);
	if (exists && valInt >= 0) {
		BodyModuleInterface* body = getBodyModule();
		if (body)	{
			body->setMaxHealth(valInt);
		}
	}

	valInt = properties->getInt(TheKey_objectInitialHealth, &exists);
	if (exists) {
		BodyModuleInterface* body = getBodyModule();
		if (body)	{
			body->setInitialHealth(valInt);
		}
	}

	valInt = properties->getInt(TheKey_objectTime, &exists);
	if (exists)
	{
		switch (valInt)
		{
			case 1:
				getDrawable()->clearModelConditionState(MODELCONDITION_NIGHT);
				break;
			case 2:
				getDrawable()->setModelConditionState(MODELCONDITION_NIGHT);
				break;
			default:
				break;
		}
	}

	valInt = properties->getInt(TheKey_objectWeather, &exists);
	if (exists)
	{
		switch (valInt)
		{
			case 1:
				getDrawable()->clearModelConditionState(MODELCONDITION_SNOW);
				break;
			case 2:
				getDrawable()->setModelConditionState(MODELCONDITION_SNOW);
				break;
			default:
				break;
		}
	}

	// set the veterancy level
	valInt = properties->getInt(TheKey_objectVeterancy, &exists);
	if (exists) {
		if (m_experienceTracker && m_experienceTracker->isTrainable())
		{
			m_experienceTracker->setVeterancyLevel((VeterancyLevel)valInt);
		}
	}

	// set the aggressiveness/mood
	valInt = properties->getInt(TheKey_objectAggressiveness, &exists);
	if (exists) {
		AIUpdateInterface *ai = getAIUpdateInterface();
		if (ai)
		{
			ai->setAttitude((AttitudeType)valInt);
		}
	}

	// set recruitable
	valBool = properties->getBool(TheKey_objectRecruitableAI, &exists);
	if (exists) {
		if (getAIUpdateInterface())
		{
			getAIUpdateInterface()->setIsRecruitable(valBool);
		}
	}

	// set selectable
	valBool = properties->getBool(TheKey_objectSelectable, &exists);
	if (exists) {
		if (valBool != isSelectable()) {
			setSelectable(valBool);
		}
	}

	// set the stopping distance
	valReal = properties->getReal(TheKey_objectStoppingDistance, &exists);
	if (exists && valReal >= 0.5f)
	{
		if (getAIUpdateInterface() && getAIUpdateInterface()->getCurLocomotor())
		{
			Locomotor *loco = getAIUpdateInterface()->getCurLocomotor();
			loco->setCloseEnoughDist(valReal);
		}
	}

	// set the disabledness of this object
	valBool = properties->getBool(TheKey_objectEnabled, &exists);
	if (exists) {
		setScriptStatus(OBJECT_STATUS_SCRIPT_DISABLED, !valBool);
	}

	// set the disabledness of this object
	valBool = properties->getBool(TheKey_objectPowered, &exists);
	if (exists) {
		setScriptStatus(OBJECT_STATUS_SCRIPT_UNPOWERED, !valBool);
	}

	// set the invulnerability of the object
	valBool = properties->getBool(TheKey_objectIndestructible, &exists);
	if (exists) {
		BodyModuleInterface* body = getBodyModule();
		if (body)	{
			body->setIndestructible(valBool);
		}
	}

	// set the sellability of the object
	valBool = properties->getBool(TheKey_objectUnsellable, &exists);
	if (exists) {
		setScriptStatus(OBJECT_STATUS_SCRIPT_UNSELLABLE, valBool);
	}

	//Set the player targetable setting of the object
	valBool = properties->getBool( TheKey_objectTargetable, &exists );
	if( exists )
	{
		setScriptStatus(OBJECT_STATUS_SCRIPT_TARGETABLE, valBool);
	}

	// adjust the vision distance of this object, overriding its default vision distance
	valInt = properties->getInt(TheKey_objectVisualRange, &exists);
	if (exists)
	{
		if (valInt < 0)
			valInt = 0;
		m_visionRange = INT_TO_REAL(valInt);
	}

	// adjust the shroud clearing distance of this object, overriding its default distance
	valInt = properties->getInt(TheKey_objectShroudClearingDistance, &exists);
	if (exists)
	{
		if (valInt < 0)
			valInt = 0.0f;
		m_shroudClearingRange = INT_TO_REAL(valInt);
	}


	Int upgradeNum = 0;
	do
	{
		AsciiString keyName;
		keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_objectGrantUpgrade).str(), upgradeNum);
		valStr = properties->getAsciiString(NAMEKEY(keyName), &exists);

		if (exists)
		{
			const UpgradeTemplate *ut = TheUpgradeCenter->findUpgrade(valStr);
			if (ut)
				giveUpgrade(ut);
		}
		else
		{
			valStr.clear();
		}

		++upgradeNum;
	} while (!valStr.isEmpty());


}

//-------------------------------------------------------------------------------------------------
void Object::friend_adjustPowerForPlayer( Bool incoming )
{
	if (isDisabled() && getTemplate()->getEnergyProduction() > 0)
	{
		// Disabledness only affects Producers, not Consumers.
		return;
	}

	if (incoming) {
		getControllingPlayer()->getEnergy()->objectEnteringInfluence(this);
	} else {
		getControllingPlayer()->getEnergy()->objectLeavingInfluence(this);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
void Object::onDisabledEdge(Bool becomingDisabled)
{
	Player* controller = getControllingPlayer();
	// can be called during game teardown, thus controller can be null
	if (controller)
	{
		//@todo jkmcd - Colin suggested we rewrite this to use the interface stuff. I agree, but need
		// to get some more bugs fixed today.
		static NameKeyType radar = NAMEKEY("RadarUpgrade");
		Module *mod = mod = findModule(radar);
		if (mod) {
			RadarUpgrade *radarMod = (RadarUpgrade*) mod;
			if (radarMod->isAlreadyUpgraded()) {
				// Need to decrement the count here, because we own a radar upgrade
				if (becomingDisabled) {
					controller->removeRadar(radarMod->getIsDisableProof());
				} else {
					controller->addRadar(radarMod->getIsDisableProof());
				}
			}
		}
	}

	// We will need to adjust power ... somehow ...
	Int powerToAdjust = getTemplate()->getEnergyProduction();

	// TheSuperHackers @bugfix Caball009 18/07/2025 Don't adjust the power for power plants that are still under construction.
#if !RETAIL_COMPATIBLE_CRC
	if ( powerToAdjust > 0 && !testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) )
#else
	if ( powerToAdjust > 0 )
#endif
	{
		// We can't affect something that consumes, or else we go low power which removes the consumption
		// which makes us not low power so we add the consumption so we go low power...
		// This check also guaards the IsDisabled in friend_adjustPower above
		static NameKeyType powerPlant = NAMEKEY("PowerPlantUpgrade");
		static NameKeyType overCharge = NAMEKEY("OverchargeBehavior");

		Module* mod = findModule(powerPlant);
		if (mod) {
			PowerPlantUpgrade *powerPlantMod = (PowerPlantUpgrade*) mod;
			if (powerPlantMod->isAlreadyUpgraded()) {
				powerToAdjust += getTemplate()->getEnergyBonus();
			}
		}

		mod = findModule(overCharge);
		if (mod) {
			OverchargeBehavior *overChargeMod = (OverchargeBehavior*) mod;
			if (overChargeMod->isOverchargeActive()) {
				powerToAdjust += getTemplate()->getEnergyBonus();
			}
		}

		// Now, adjust the power for the player.
		if (controller)
			controller->getEnergy()->adjustPower(powerToAdjust, !becomingDisabled);
	}
}

//-------------------------------------------------------------------------------------------------
/** Object CRC implementation */
//-------------------------------------------------------------------------------------------------
void Object::crc( Xfer *xfer )
{
#ifdef DEBUG_CRC
//	g_logObjectCRCs = TRUE;
//	Bool g_logAllObjects = TRUE;
	AsciiString logString;
	AsciiString tmp;
	Bool doLogging = g_logObjectCRCs /* && getControllingPlayer()->getPlayerType() == PLAYER_HUMAN */;
	if (doLogging)
	{
		tmp.format("CRC of Object %d (%s), owned by player %d, team: %d, ", m_id, getTemplate()->getName().str(), getControllingPlayer()->getPlayerIndex(), this->getTeam() ? this->getTeam()->getID() : TEAM_ID_INVALID);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	xfer->xferUnsignedByte(&m_privateStatus);
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_privateStatus: %X, ", (UnsignedInt)m_privateStatus);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	// This is evil - we cast the const Matrix3D * to a Matrix3D * because the XferCRC class must use
	// the same interface as the XferLoad class for save game restore.  This only works because
	// XferCRC does not modify its data.
	xfer->xferUser((Matrix3D *)getTransformMatrix(),	sizeof(Matrix3D));
#ifdef DEBUG_CRC
	if (doLogging)
	{
		XferCRC tmpXfer;
		tmpXfer.open("tmp");
		tmpXfer.xferUser((Matrix3D *)getTransformMatrix(),	sizeof(Matrix3D));
		tmp.format("getTransformMatrix(): %8.8X, ", tmpXfer.getCRC());
		tmpXfer.close();
		logString.concat(tmp);
	}
#endif // DEBUG_CRC


	xfer->xferUser(&m_id,															sizeof(m_id));
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_id: %d, ", m_id);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC
#if RETAIL_COMPATIBLE_CRC
	xfer->xferUser(&m_objectUpgradesCompleted,				sizeof(Int64));
#else
	xfer->xferUser(&m_objectUpgradesCompleted, sizeof(m_objectUpgradesCompleted));
#endif
#ifdef DEBUG_CRC
	if (doLogging)
	{
#if RETAIL_COMPATIBLE_CRC
		tmp.format("m_objectUpgradesCompleted: %I64X, ", m_objectUpgradesCompleted);
#else
		tmp.format("m_objectUpgradesCompleted: %s, ", m_objectUpgradesCompleted.toHexString().str());
#endif
		logString.concat(tmp);
	}
#endif // DEBUG_CRC
	if (m_experienceTracker)
		xfer->xferSnapshot( m_experienceTracker );
#ifdef DEBUG_CRC
	if (doLogging)
	{
		XferCRC tmpXfer;
		tmpXfer.open("tmp");
		tmpXfer.xferSnapshot(m_experienceTracker);
		tmp.format("m_experienceTracker: %8.8X, ", tmpXfer.getCRC());
		tmpXfer.close();
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	Real health = getBodyModule()->getHealth();
	xfer->xferUser(&health,														sizeof(health));
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("health: %g/%8.8X, ", health, AS_INT(health));
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	xfer->xferUnsignedInt(&m_weaponBonusCondition);
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("m_weaponBonusCondition: %8.8X, ", m_weaponBonusCondition);
		logString.concat(tmp);
	}
#endif // DEBUG_CRC

	Real scalar = getBodyModule()->getDamageScalar();
	xfer->xferUser(&scalar,														sizeof(scalar));
#ifdef DEBUG_CRC
	if (doLogging)
	{
		tmp.format("damage scalar: %g/%8.8X", scalar, AS_INT(scalar));
		logString.concat(tmp);

		CRCDEBUG_LOG(("%s", logString.str()));
	}
#endif // DEBUG_CRC

	for (Int i=0; i<WEAPONSLOT_COUNT; ++i)
	{
		Weapon *thisWeapon = getWeaponInWeaponSlot((WeaponSlotType)i);
		if (thisWeapon)
		{
			xfer->xferSnapshot( thisWeapon );
		}
	}

}

//-------------------------------------------------------------------------------------------------
/** Object xfer implementation
	* Version Info:
	* 1: Initial version
	* 2: Xfers m_singleUseCommandUsed... determines if the single use command button has been used or not.
	* 3: Xfers the solehealingbenefactor ID and expiration frame
	* 4: misc stuff that got missed somehow
	* 5: m_isReceivingDifficultyBonus
	* 6: We do indeed need to save m_containedBy.  The comment misrepresents what the contain module will do.
	* 7: save full mtx, not pos+orient.
	* 8: Kris: Conversion of object status bits from UnsignedInt to BitFlags<>
	*/
//-------------------------------------------------------------------------------------------------
void Object::xfer( Xfer *xfer )
{

	// version
	const XferVersion currentVersion = 8;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// object ID
	ObjectID id = getID();
	xfer->xferObjectID( &id );
	setID( id );

	DEBUG_LOG(("Xfer Object %s id=%d",getTemplate()->getName().str(),id));

	if (version >= 7)
	{
		Matrix3D mtx = *getTransformMatrix();
		xfer->xferMatrix3D(&mtx);
		setTransformMatrix(&mtx);
	}
	else
	{
		// object position
		Coord3D pos = *getPosition();
		xfer->xferCoord3D( &pos );
		setPosition( &pos );

		// orientation
		Real orientation = getOrientation();
		xfer->xferReal( &orientation );
		setOrientation( orientation );
	}

	// team
	TeamID teamID = m_team ? m_team->getID() : TEAM_ID_INVALID;
	xfer->xferUser( &teamID, sizeof( TeamID ) );
	// DON'T set the team yet; must wait till we read our status bits,
	// since setTeam can affect the player's power usage, but that could
	// be done incorrectly if our status bits aren't accurate yet... (srj)

	// producer id
	xfer->xferObjectID( &m_producerID );

	// builder id
	xfer->xferObjectID( &m_builderID );

	// drawable id
	Drawable *draw = getDrawable();
	DrawableID drawableID = draw ? draw->getID() : INVALID_DRAWABLE_ID;
	xfer->xferDrawableID( &drawableID );
	if( xfer->getXferMode() == XFER_LOAD )
	{

		// change the ID of the drawable attached to be the same ID as it was when it was saved
		draw->setID( drawableID );

	}

	// internal name
	xfer->xferAsciiString( &m_name );

	// status
	if( version >= 8 )
	{
		m_status.xfer( xfer );
	}
	else
	{
		//We are loading an old version, so we must convert it from a 32-bit int to a bitflag
		UnsignedInt oldStatus;
		xfer->xferUnsignedInt( &oldStatus );

		//Clear our status
		m_status.clear();

		for( int i = 0; i < 32; i++ )
		{
			UnsignedInt bit = 1<<i;
			if( oldStatus & bit )
			{
				ObjectStatusTypes status = (ObjectStatusTypes)(i+1);
				m_status.set( MAKE_OBJECT_STATUS_MASK( status ) );
			}
		}
	}

	// script status
	xfer->xferUnsignedByte( &m_scriptStatus );

	// private status
	xfer->xferUnsignedByte( &m_privateStatus );

	// geometry info
	xfer->xferSnapshot( &m_geometryInfo );

	// sighting info, last look - must be saved cause we save PartitionCell::m_shroudLevel
	xfer->xferSnapshot( m_partitionLastLook );

	// sighting info, last shroud - must be saved cause we save PartitionCell::m_shroudLevel
	xfer->xferSnapshot( m_partitionLastShroud );

	// sighting info, last threat
	// John M says we don't need to save this (CBD)
//	xfer->xferSnapshot( &m_partitionLastThreat );

	// sighting info, last value
	// John M says we don't need to save this (CBD)
//	xfer->xferSnapshot( &m_partitionLastValue );

	// vision range
	xfer->xferReal( &m_visionRange );

	// shroud clearing range
	xfer->xferReal( &m_shroudClearingRange );

	// shroud range
	xfer->xferReal( &m_shroudRange );

	// disabled mask
	m_disabledMask.xfer( xfer );

	//New var added for version 2. Determines if the single use command button has been used or not.
	if( xfer->getXferMode() == XFER_SAVE || version >= 2 )
	{
		xfer->xferBool( &m_singleUseCommandUsed );
	}
	else
	{
		m_singleUseCommandUsed = false;
	}

	// disabled till frame
	xfer->xferUser( m_disabledTillFrame, sizeof( UnsignedInt ) * DISABLED_COUNT );

	// OK, now that we have xferred our status bits and disabled data, it's safe to set the team...
	// TheSuperHackers @todo Refactor so that this code can be moved to loadPostProcess.
	if( xfer->getXferMode() == XFER_LOAD )
	{
		Team *team = TheTeamFactory->findTeamByID( teamID );
		if( team == nullptr )
		{
			DEBUG_CRASH(( "Object::xfer - Unable to load team" ));
			throw SC_INVALID_DATA;
		}
		const Bool restoring = true;
		setOrRestoreTeam( team, restoring );
	}

	// special model condition until
	xfer->xferUnsignedInt( &m_smcUntil );

	//
	// radar data ... when loading, we will remove all objects from the radar and let
	// the radar system load itself as a separate chunk of data from the save file
	//
	if( xfer->getXferMode() == XFER_LOAD && m_radarData )
		TheRadar->removeObject( this );

	// experience tracker
	xfer->xferSnapshot( m_experienceTracker );

	//
	// we do not need to do anything with our m_containedBy pointer, the post process
	// of that objects contain module will actually re-do the contain process again
	//
	// m_containedBy <-- do nothing with this right now
	if( version >= 6 )
	{
		// No, the contain module is just going to friend_ reach in and set this for us.
		// Containers more complicated than Open (like Tunnel) can't do that.  Our variable,
		// our responsibility.
#if !RETAIL_COMPATIBLE_CRC
		// TheSuperHackers @tweak Contained by ID is already set with retail compatibility; don't overwrite it.
		if( xfer->getXferMode() == XFER_SAVE )
		{
			if( m_containedBy != nullptr )
				m_containedByID = m_containedBy->getID();
			else
				m_containedByID = INVALID_ID;
		}
#endif

		xfer->xferObjectID( &m_containedByID );
	}

	// contained by frame
	xfer->xferUnsignedInt( &m_containedByFrame );

	// construction percent
	xfer->xferReal( &m_constructionPercent );

	// upgrades completed
	xfer->xferUpgradeMask( &m_objectUpgradesCompleted );

	// original team name
	xfer->xferAsciiString( &m_originalTeamName );

	// indicator color
	xfer->xferColor( &m_indicatorColor );

	// health box offset
	xfer->xferCoord3D( &m_healthBoxOffset );

	// Entered & exited housekeeping.
	Int i;
	xfer->xferByte(&m_numTriggerAreasActive);
	xfer->xferUnsignedInt(&m_enteredOrExitedFrame);
	xfer->xferICoord3D(&m_iPos);
	if (m_numTriggerAreasActive<0 || m_numTriggerAreasActive>MAX_TRIGGER_AREA_INFOS) {
		DEBUG_CRASH(("Invalid m_numTriggerAreasActive = %d, max is %d", m_numTriggerAreasActive,
			MAX_TRIGGER_AREA_INFOS));
		throw SC_INVALID_DATA;
	}
	for (i=0; i<m_numTriggerAreasActive; i++) {
		AsciiString triggerName;
		if (m_triggerInfo[i].pTrigger) {
			triggerName = m_triggerInfo[i].pTrigger->getTriggerName();
		}
		xfer->xferAsciiString(&triggerName);
		if (xfer->getXferMode() == XFER_LOAD)
		{
			//
			// CBD (11-13-2002) I'm disabling this because it appears there might be some areas with
			// empty names, see John A. for more info
			//
			//if (triggerName.isNotEmpty())
			  m_triggerInfo[i].pTrigger = TheTerrainLogic->getTriggerAreaByName(triggerName);
		}
		xfer->xferByte(&m_triggerInfo[i].entered);
		xfer->xferByte(&m_triggerInfo[i].exited);
		xfer->xferByte(&m_triggerInfo[i].isInside);
	}
	// Layer object is pathing on.
	xfer->xferUser(&m_layer, sizeof(m_layer));

	// Layer of current path goal.
	xfer->xferUser(&m_destinationLayer, sizeof(m_destinationLayer));

	// Object selectability.
	xfer->xferBool(&m_isSelectable);

	xfer->xferUnsignedInt(&m_safeOcclusionFrame);

	// User formations.
	xfer->xferUser(&m_formationID, sizeof(m_formationID));
	if (m_formationID!=NO_FORMATION_ID) {
		xfer->xferCoord2D(&m_formationOffset);
	}

	// module count
	UnsignedShort moduleCount = 0;
	for (BehaviorModule** b = m_behaviors; *b; ++b)
		++moduleCount;

	xfer->xferUnsignedShort( &moduleCount );
	AsciiString moduleIdentifier;
	BehaviorModule *module;
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// go through all modules
		for (BehaviorModule** b = m_behaviors; *b; ++b)
		{

			// get module
			module = *b;

			// write module identifier
			moduleIdentifier = TheNameKeyGenerator->keyToName( module->getModuleTagNameKey() );
			DEBUG_ASSERTCRASH( moduleIdentifier != AsciiString::TheEmptyString,
												 ("Object::xfer - Module tag key does not translate to a string!\n") );
			xfer->xferAsciiString( &moduleIdentifier );

			// begin a data block
			xfer->beginBlock();

			// xfer data
			xfer->xferSnapshot( module );

			// end data block
			xfer->endBlock();

		}

	}
	else
	{
		AsciiString otherModuleIdentifier;

		// read all module data
		for( UnsignedShort i = 0; i < moduleCount; ++i )
		{

			// read module name
			xfer->xferAsciiString( &moduleIdentifier );
			NameKeyType moduleIdentifierKey = TheNameKeyGenerator->nameToKey(moduleIdentifier);

			// find the module with this identifier in the module list
			module = nullptr;
			for (BehaviorModule** b = m_behaviors; b && *b; ++b)
			{

				if (moduleIdentifierKey == (*b)->getModuleTagNameKey())
				{
					module = *b;
					break;
				}

			}

			// start of a new block
			Int dataSize = xfer->beginBlock();

			//
			// if we didn't find the module, it's quite possible that we have removed
			// it from the object definition in a future patch, if that is so, we need to
			// skip the module data in the file
			//
			if( module == nullptr )
			{

				// for testing purposes, this module better be found
				DEBUG_CRASH(( "Object::xfer - Module '%s' was indicated in file, but not found on object '%s'(%d)",
											moduleIdentifier.str(), getTemplate()->getName().str(), getID() ));

				// skip this data in the file
				xfer->skip( dataSize );

			}
			else
			{

				// xfer the data into this module
				xfer->xferSnapshot( module );

			}

			// end block
			xfer->endBlock();

		}

	}


	if ( version >= 3 )
	{
		xfer->xferObjectID( &m_soleHealingBenefactorID );
		xfer->xferUnsignedInt( &m_soleHealingBenefactorExpirationFrame );
	}
	else if ( xfer->getXferMode() == XFER_LOAD )
	{
		m_soleHealingBenefactorID = INVALID_ID;
		m_soleHealingBenefactorExpirationFrame = 0;
	}

	// Doesn't need to be saved.  These are created as needed.  jba.
	//m_group;

	// don't need to save m_partitionData.
	DEBUG_ASSERTCRASH(!(xfer->getXferMode() == XFER_LOAD && m_partitionData == nullptr), ("should not be in partitionmgr yet"));

	// don't need to be saved or loaded; are inited & cached for runtime only by our ctor (srj)
	//m_repulsorHelper;
	//m_smcHelper;
	//m_wsHelper;
	//m_defectionHelper;
	//m_firingTracker;
	//m_contain;
	//m_body;
	//m_ai;
	//m_physics;
#if defined(RTS_DEBUG)
	//m_hasDiedAlready;
#endif

	if (version >= 4)
	{
		// xfer the weaponSetFlags FIRST, since we need 'em to restore the weaponSet properly. (srj)
		m_curWeaponSetFlags.xfer( xfer );
		xfer->xferUnsignedInt(&m_weaponBonusCondition);
		xfer->xferUser(&m_lastWeaponCondition, sizeof(m_lastWeaponCondition));

		// do the weaponSet itself after all the weapon-related stuff, just in case
		xfer->xferSnapshot(&m_weaponSet);

		m_specialPowerBits.xfer( xfer );

		xfer->xferAsciiString(&m_commandSetStringOverride);

		xfer->xferBool(&m_modulesReady);
	}

	if (version >= 5)
	{
		xfer->xferBool(&m_isReceivingDifficultyBonus);
	}
	else
		m_isReceivingDifficultyBonus = FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Object load game post process phase */
//-------------------------------------------------------------------------------------------------
void Object::loadPostProcess()
{
	if( m_containedByID != INVALID_ID )
		m_containedBy = TheGameLogic->findObjectByID(m_containedByID);
	else
		m_containedBy = nullptr;

}

//-------------------------------------------------------------------------------------------------
/** Does this object have this upgrade */
//-------------------------------------------------------------------------------------------------
Bool Object::hasUpgrade( const UpgradeTemplate *upgradeT ) const
{
	if( m_objectUpgradesCompleted.testForAll( upgradeT->getUpgradeMask() ) )
	{
		return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Is this object capable of having this upgrade */
//-------------------------------------------------------------------------------------------------
Bool Object::affectedByUpgrade( const UpgradeTemplate *upgradeT ) const
{
	const UpgradeMaskType& objectMask = getObjectCompletedUpgradeMask();
	const UpgradeMaskType& playerMask = getControllingPlayer()->getCompletedUpgradeMask();
	UpgradeMaskType maskToCheck = playerMask;
	maskToCheck.set( objectMask );
	maskToCheck.set( upgradeT->getUpgradeMask() );

	// We need to add in all of the already owned upgrades to handle "AND" requiring upgrades.
	// We combine all the masks in case someone has a Object AND Player combination

	for (BehaviorModule** module = m_behaviors; *module; ++module)
	{
		UpgradeModuleInterface* upgrade = (*module)->getUpgrade();
		if (!upgrade)
			continue;

		if( upgrade->wouldUpgrade( maskToCheck ) )
		{
			// if any of my many upgrade modules would execute in response to this flag, say yes.
			return TRUE;
		}
	}
	return FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Give this upgrade to this object */
//-------------------------------------------------------------------------------------------------
void Object::giveUpgrade( const UpgradeTemplate *upgradeT )
{
	if (upgradeT)
	{
		m_objectUpgradesCompleted.set( upgradeT->getUpgradeMask() );

		//
		// iterate through all the upgrade modules of this object and call the method to
		// grant a new upgrade
		//
		updateUpgradeModules();
	}
}

//-------------------------------------------------------------------------------------------------
/** Remove this upgrade from this object */
//-------------------------------------------------------------------------------------------------
void Object::removeUpgrade( const UpgradeTemplate *upgradeT )
{
	m_objectUpgradesCompleted.clear( upgradeT->getUpgradeMask() );
	for (BehaviorModule** module = m_behaviors; *module; ++module)
	{
		UpgradeModuleInterface* upgrade = (*module)->getUpgrade();
		if (!upgrade)
			continue;

		// Whoa, please note that while the function is called Object::RemoveUpgrade, it is not removing anything
		// in the sense of undoing the effects.  It is just resetting the upgrade so it may be run again.
		upgrade->resetUpgrade( upgradeT->getUpgradeMask() );
	}
}

//-------------------------------------------------------------------------------------------------
/** Central point for onCapture logic */
//-------------------------------------------------------------------------------------------------
void Object::onCapture( Player *oldOwner, Player *newOwner )
{
	// Everybody chills when they captured so they don't keep doing something the new player might not want him to be doing
	// TheSuperHackers @tweak Stubbjax 19/11/2025 Except when the new owner is an ally, so that Hackers keep on hacking, etc.
	if( getAIUpdateInterface()  &&  (oldOwner != newOwner) )
	{
#if RETAIL_COMPATIBLE_CRC
		getAIUpdateInterface()->aiIdle(CMD_FROM_AI);
#else
		if (oldOwner->getRelationship(newOwner->getDefaultTeam()) != ALLIES)
		{
			getAIUpdateInterface()->aiIdle(CMD_FROM_AI);

			DozerAIInterface* dozerAI = getAIUpdateInterface()->getDozerAIInterface();
			if (dozerAI)
			{
				dozerAI->cancelAllTasks();
			}
		}
#endif
	}

	// this gets the new owner some points
	newOwner->getScoreKeeper()->addObjectCaptured(this);

	// rip through the behavior modules and call the onCapture for any modules that care
	for( BehaviorModule **module = m_behaviors; *module; ++module )
		(*module)->onCapture( oldOwner, newOwner );

	//
	// We have to undo our look for the old team and redo it for the new.
	// onCapture is used now, so it better be called after ownership changes and not before.
	//
	handlePartitionCellMaintenance();

	// Design needs the player to be able to sell buildings he steals from the AI's build list, and this is the
	// easiest fix.  The only snafu would be a key building build listed by the AI that the player can capture
	// and the AI tries to capture back but needs to not sell.  In that case, a Cinematic Unsellable version
	// of the building needs to be made.  This fix has been okayed as the most non-lethal in November.
	clearScriptStatus(OBJECT_STATUS_SCRIPT_UNSELLABLE);

	// mark the command bar to redraw
	TheControlBar->markUIDirty();

	if (oldOwner!=newOwner && newOwner->isSkirmishAIPlayer()) {
		// The skirmish ai doesn't know what to do with captured faction buildings except sell them.
		if (isFactionStructure()) {
			TheBuildAssistant->sellObject( this );
		}
	}

}

//-------------------------------------------------------------------------------------------------
/// Object level events that need to happen upon game death
void Object::onDie( DamageInfo *damageInfo )
{

#if defined(RTS_DEBUG)
	DEBUG_ASSERTCRASH(m_hasDiedAlready == false, ("Object::onDie has been called multiple times. This is invalid. jkmcd"));
	m_hasDiedAlready = true;
#endif

	Bool selfInflicted = (damageInfo->in.m_sourceID == getID());

	// FIRST, call our die modules.
	for (BehaviorModule** d = m_behaviors; *d; ++d)
	{
		DieModuleInterface* die = (*d)->getDie();
		if (die)
			die->onDie(damageInfo);
	}

	// When objects die we remove from the radar as they're really not interesting anymore
	if( m_radarData )
		TheRadar->removeObject( this );

	// Just in case I have been sporting one of these fancy Terrain Decals,
	//I naturally lose it now, because I'm dead.
	Drawable *draw = getDrawable();
	if (draw) draw->setTerrainDecalFadeTarget(0.0f, -0.03f);//fade...
	//if (draw) draw->setTerrainDecal(TERRAIN_DECAL_NONE);//pop!


	// objects that were spawned from something, need to tell their spawner that they have died
	Object* spawner = TheGameLogic->findObjectByID( getProducerID() );
	if( spawner )
	{

		// get the spawn behavior interface of the spawner
		SpawnBehaviorInterface *spawnerBehavior = spawner->getSpawnBehaviorInterface();
		if( spawnerBehavior )
			spawnerBehavior->onSpawnDeath( getID(), damageInfo );

	}

	handlePartitionCellMaintenance();
	if(m_team)
		m_team->notifyTeamOfObjectDeath();
	// Play death sound here.

	AudioEventRTS deathSound = *getTemplate()->getSoundDie();
	// If we were killed by fire, or by poison, we should play those die sounds instead of the usual
	// sound
	if (damageInfo->in.m_deathType == DEATH_BURNED)
		deathSound = *getTemplate()->getSoundDieFire();
	else if (damageInfo->in.m_deathType == DEATH_POISONED || damageInfo->in.m_deathType == DEATH_POISONED_BETA)
		deathSound = *getTemplate()->getSoundDieToxin();

	// If we didn't actually have a specialized die sound (for the case of fire or poison, we
	// should use the generic death sound)
	if (!TheAudio->isValidAudioEvent(&deathSound))
		deathSound = *getTemplate()->getSoundDie();

	// Use the position. Next frame, when this unit is gone, this sound will be clipped because we
	// can no longer automatically find its position. - jkmcd
	deathSound.setPosition(getPosition());
	PlayerIndex index = getControllingPlayer() ? getControllingPlayer()->getPlayerIndex() : 0;
	deathSound.setPlayerIndex( index );
	TheAudio->addAudioEvent(&deathSound);

	if (isLocallyViewed() && !selfInflicted) // wasLocallyViewed? :-)
	{
		if (isKindOf(KINDOF_STRUCTURE) && isKindOf(KINDOF_MP_COUNT_FOR_VICTORY))
		{
			TheEva->setShouldPlay(EVA_BuldingLost);
		}
		else if (isKindOf(KINDOF_INFANTRY) || isKindOf(KINDOF_VEHICLE))
		{
			TheEva->setShouldPlay(EVA_UnitLost);
			//Create a fake radar event so the user can use the spacebar to quickly jump to this!
			TheRadar->tryEvent( RADAR_EVENT_FAKE, getPosition() );
		}
	}

	// This call won't do anything if we aren't actually in the list.
	TheInGameUI->removeIdleWorker(this, this->getControllingPlayer()->getPlayerIndex());
}

//-------------------------------------------------------------------------------------------------
/**
	A weapon cannot be in charge of maintaining condition flags as it is all event driven.
	I will maintain my ModelCondition myself if it should change.  Firing is set by firing logic,
	so I don't include it here.  It is only the states that expire on timers that noone watches
	that I am concerned with.
*/
//-------------------------------------------------------------------------------------------------
void Object::adjustModelConditionForWeaponStatus()
{
	UnsignedInt now = TheGameLogic->getFrame();

	for (int i = 0; i < WEAPONSLOT_COUNT; ++i)
	{
		const Weapon* w = m_weaponSet.getWeaponInWeaponSlot((WeaponSlotType)i);
		if (!w)
		{
			m_lastWeaponCondition[i] = WSF_NONE;
			continue;
		}

		WeaponSetConditionType conditionToSet = WSF_INVALID;
		if (i != m_weaponSet.getCurWeaponSlot())
		{
			// if this isn't the current weapon, then we never set ANYTHING for it.
			conditionToSet = WSF_NONE;
		}
		else if (w->getLastShotFrame() == now)
		{
			// yep, this overrides any weapon-status condition!
			conditionToSet = WSF_FIRING;
		}
		else if (!testStatus( OBJECT_STATUS_IS_ATTACKING ))
		{
			// srj sez: not 100% sure about this one, but the problem is: say we were attacking,
			// then issue a move command. if we didn't do this here, we might still have a 'firing'
			// pose, because his weapon might be in 'reloading' mode. since we're not attacking, however,
			// we really don't care, so we just force the issue here. (This might still need tweaking for the pursue state.)
			conditionToSet = WSF_NONE;
		}
		else
		{
			WeaponStatus newStatus = w->getStatus();

			const static WeaponSetConditionType s_wsfLookup[WEAPON_STATUS_COUNT] =
			{
				WSF_NONE,				// READY_TO_FIRE,
				WSF_NONE,				// OUT_OF_AMMO,
				WSF_BETWEEN,		// BETWEEN_FIRING_SHOTS,
				WSF_RELOADING,	// RELOADING_CLIP,
				WSF_PREATTACK		// PRE_ATTACK,
			};
			conditionToSet = s_wsfLookup[newStatus];

			// special case this: say we are firing in bursts: pow-pow-pow-pause, etc.
			// then we might have a frame where we have reloaded and are ready-to-fire,
			// but haven't fired yet this frame. in that case, use 'between' so we still have
			// a firing pose, 'cuz if we use 'none' we will 'pop' back to idle for a frame. (srj)
			// additional note: only do if aiming or firing, since we could also be in this state if
			// we are approaching or pursuing a target! (srj)
			if (newStatus == READY_TO_FIRE && conditionToSet == WSF_NONE && testStatus( OBJECT_STATUS_IS_ATTACKING ) &&
					(testStatus( OBJECT_STATUS_IS_AIMING_WEAPON ) || testStatus( OBJECT_STATUS_IS_FIRING_WEAPON )))
			{
				conditionToSet = WSF_BETWEEN;
			}

		}

		if (m_drawable)
		{
			m_drawable->updateDrawableClipStatus( w->getRemainingAmmo(), w->getClipSize(), w->getWeaponSlot() );
			if (conditionToSet != WSF_INVALID && conditionToSet != m_lastWeaponCondition[i])
			{
				m_lastWeaponCondition[i] = conditionToSet;
				ModelConditionFlags c = m_weaponSet.getModelConditionForWeaponSlot((WeaponSlotType)i, conditionToSet);
				m_drawable->clearAndSetModelConditionFlags(s_allWeaponFireFlags[i], c);
				if (conditionToSet == WSF_PREATTACK)
				{
					// in the preattack state, adjust the speed of the preattack anim to match the actual time it will take
					UnsignedInt preAttackDone = w->getPreAttackFinishedFrame();
					if (preAttackDone > now)
						m_drawable->setAnimationLoopDuration(preAttackDone - now);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool Object::hasGhostObject() const
{
	if (m_partitionData == nullptr)
		return false;

	return m_partitionData->getGhostObject() != nullptr;
}

//-------------------------------------------------------------------------------------------------
/// We have moved a 'significant' amount, so do maintenance that can be considered 'cell-based'
void Object::onPartitionCellChange()
{
	handlePartitionCellMaintenance();
}

//-------------------------------------------------------------------------------------------------
void Object::handlePartitionCellMaintenance()
{
	handleShroud();
	handleValueMap();
	handleThreatMap();
}

//-------------------------------------------------------------------------------------------------
void Object::handleShroud()
{
	// Undo last looking
	unlook();
	// and shrouding
	unshroud();

	// redo shrouding
	shroud();
	// Redo looking
	look();
}

//-------------------------------------------------------------------------------------------------
void Object::handleValueMap()
{
	removeValue();
	addValue();
}

//-------------------------------------------------------------------------------------------------
void Object::handleThreatMap()
{
	removeThreat();
	addThreat();
}

//-------------------------------------------------------------------------------------------------
void Object::addValue()
{
	if( !m_partitionLastValue->isInvalid() )
	{
		DEBUG_CRASH( ("An Object is adding value, but hasn't removed his previous value.") );
		return;
	}

	if (!getControllingPlayer())
		return;

	if( getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) || isEffectivelyDead() || getShroudClearingRange() <= 0.0f )
		return;


	m_partitionLastValue->m_where = *getPosition();
	m_partitionLastValue->m_data = getTemplate()->friend_getBuildCost();

	m_partitionLastValue->m_forWhom = getControllingPlayer()->getPlayerMask();
	m_partitionLastValue->m_howFar = getVisionRange();	// we are valuable all the way to where we can target.

	ThePartitionManager->doValueAffect(m_partitionLastValue->m_where.x,
																		 m_partitionLastValue->m_where.y,
																		 m_partitionLastValue->m_howFar,
																		 m_partitionLastValue->m_data,
																		 m_partitionLastValue->m_forWhom
																		 );
}

//-------------------------------------------------------------------------------------------------
void Object::removeValue()
{
	if( m_partitionLastValue->isInvalid() )
	{
		// removing before adding is valid, cause we always remove before adding. (So the first remove
		// will occur before the first add)
		return;
	}

	ThePartitionManager->undoValueAffect(m_partitionLastValue->m_where.x,
																			 m_partitionLastValue->m_where.y,
																			 m_partitionLastValue->m_howFar,
																			 m_partitionLastValue->m_data,
																			 m_partitionLastValue->m_forWhom
																			);

	m_partitionLastValue->reset();
}

//-------------------------------------------------------------------------------------------------
void Object::addThreat()
{
	if( !m_partitionLastThreat->isInvalid() )
	{
		DEBUG_CRASH( ("An Object is adding threat, but hasn't removed his previous threat. (He hasn't finished the threat?)") );
		return;
	}

	if (!getControllingPlayer())
		return;

	if( getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) || isEffectivelyDead() || getShroudClearingRange() <= 0.0f )
		return;


	m_partitionLastThreat->m_where = *getPosition();
	m_partitionLastThreat->m_data = getTemplate()->getThreatValue();

	m_partitionLastThreat->m_forWhom = getControllingPlayer()->getPlayerMask();
	m_partitionLastThreat->m_howFar = getVisionRange();	// we are threatening all the way to where we can target.

	ThePartitionManager->doThreatAffect(m_partitionLastThreat->m_where.x,
																			m_partitionLastThreat->m_where.y,
																			m_partitionLastThreat->m_howFar,
																			m_partitionLastThreat->m_data,
																			m_partitionLastThreat->m_forWhom
																		 );
}

//-------------------------------------------------------------------------------------------------
void Object::removeThreat()
{
	if( m_partitionLastThreat->isInvalid() )
	{
		// removing before adding is valid, cause we always remove before adding. (So the first remove
		// will occur before the first add)
		return;
	}

	ThePartitionManager->undoThreatAffect(m_partitionLastThreat->m_where.x,
																			  m_partitionLastThreat->m_where.y,
																			  m_partitionLastThreat->m_howFar,
																				m_partitionLastThreat->m_data,
																			  m_partitionLastThreat->m_forWhom
																			 );

	m_partitionLastThreat->reset();
}



//-------------------------------------------------------------------------------------------------
void Object::look()
{
	if( ! m_partitionLastLook->isInvalid() )
	{
		DEBUG_CRASH( ("An Object is looking, but hasn't unlooked the last one.") );
		return;
	}

	Player* controller = getControllingPlayer();
	if ( controller )
	{
		// I removed the check for objects under construction by request of designers since
		// they want constructing objects to have a reduced sight range now. -MW
		// dead or blind things don't reveal shroud
		if( ( ! isDestroyed() )// Some things get Destroyed directly without hitting Death.
				&& ( ! isEffectivelyDead() )
				&& ( getShroudClearingRange() > 0.0f )
			)
		{
			PlayerMaskType lookingMask = 0;

			for( Int currentIndex = ThePlayerList->getPlayerCount() - 1; currentIndex >=0; currentIndex-- )
			{
				const Player *currentPlayer = ThePlayerList->getNthPlayer( currentIndex );

				// Build mask of of allies who can see me.
				// This is the Object-centric game level that cares
				if( getControllingPlayer()->getRelationship( currentPlayer->getDefaultTeam() ) == ALLIES )
				{
					lookingMask |= currentPlayer->getPlayerMask();
				}
			}

			// Other players can also be looking through our eyes.
			lookingMask |= controller->getVisionSpiedMask();

			if ( isKindOf(KINDOF_REVEAL_TO_ALL) )
				lookingMask = PLAYERMASK_ALL;

			Coord3D pos = *getPosition();
			ThePartitionManager->doShroudReveal(pos.x,
																					pos.y,
																					getShroudClearingRange(),
																					lookingMask
																					);

			m_partitionLastLook->m_where = pos;
			m_partitionLastLook->m_forWhom = lookingMask;
			m_partitionLastLook->m_howFar = getShroudClearingRange();

//			DEBUG_LOG(( "A %s looks at %f, %f for %x at range %f",
//									getTemplate()->getName().str(),
//									pos.x,
//									pos.y,
//									lookingMask,
//									getShroudClearingRange()
//									));
		}
	}
}

//-------------------------------------------------------------------------------------------------
void Object::unlook()
{
	if( m_partitionLastLook->isInvalid() )
	{
		// Your very first action will be an unlook, so of course you haven't looked yet.  This is not an error
		// This early return prevents an extra unlook if you never looked.  Like you have 0 vision.
		return;
	}

	ThePartitionManager->queueUndoShroudReveal(m_partitionLastLook->m_where.x,
																				m_partitionLastLook->m_where.y,
																				m_partitionLastLook->m_howFar,
																				m_partitionLastLook->m_forWhom
																				);

//			DEBUG_LOG(( "A %s queues an unlook at %f, %f for %x at range %f",
//									getTemplate()->getName().str(),
//									m_partitionLastLook.m_where.x,
//									m_partitionLastLook.m_where.y,
//									m_partitionLastLook.m_forWhom,
//									m_partitionLastLook.m_howFar
//									));

	m_partitionLastLook->reset();
}

//-------------------------------------------------------------------------------------------------
void Object::shroud()
{
	if( ! m_partitionLastShroud->isInvalid() )
	{
		DEBUG_CRASH( ("An Object is shrouding, but hasn't unshrouded the last one.") );
		return;
	}

	Player* controller = getControllingPlayer();
	if ( controller )
	{
		// things under construction don't  shroud. (srj), nor do dead or blind things
		if( !getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) && !isEffectivelyDead()	&& getShroudRange() > 0.0f )
		{
			PlayerMaskType shroudingMask = 0;
			for( Int currentIndex = ThePlayerList->getPlayerCount() - 1; currentIndex >=0; currentIndex-- )
			{
				const Player *currentPlayer = ThePlayerList->getNthPlayer( currentIndex );
				//Build mask of NON-allies.  This is the Object-centric game level that cares
				if( getControllingPlayer()->getRelationship( currentPlayer->getDefaultTeam() ) != ALLIES )
				{
					shroudingMask |= currentPlayer->getPlayerMask();
				}
			}

			Coord3D pos = *getPosition();
			ThePartitionManager->doShroudCover(pos.x, pos.y,
				getShroudRange(),
				shroudingMask);

			m_partitionLastShroud->m_where = pos;
			m_partitionLastShroud->m_forWhom = shroudingMask;
			m_partitionLastShroud->m_howFar = getShroudRange();
		}
	}
}

//-------------------------------------------------------------------------------------------------
void Object::unshroud()
{
	if( m_partitionLastShroud->isInvalid() )
	{
		// Your very first action will be an unlook, so of course you haven't looked yet.  This is not an error
		// This early return prevents an extra unlook if you never looked.  Like you have 0 shroud generation.
		return;
	}

	ThePartitionManager->undoShroudCover(m_partitionLastShroud->m_where.x,
		m_partitionLastShroud->m_where.y,
		m_partitionLastShroud->m_howFar,
		m_partitionLastShroud->m_forWhom);

	m_partitionLastShroud->reset();
}

//-------------------------------------------------------------------------------------------------
Real Object::getVisionRange() const
{
#if defined(RTS_DEBUG)
	if (TheGlobalData->m_debugVisibility)
	{
		Vector3 pos(m_visionRange, 0, 0);
		for (int i = 0; i < TheGlobalData->m_debugVisibilityTileCount; ++i)
		{
			pos.Rotate_Z(1.0f * i / TheGlobalData->m_debugVisibilityTileCount * 2 * PI);
			Coord3D coord = { pos.X + getPosition()->x, pos.Y + getPosition()->y, pos.Z + getPosition()->z };

			addIcon(&coord, TheGlobalData->m_debugVisibilityTileWidth,
											TheGlobalData->m_debugVisibilityTileDuration,
											TheGlobalData->m_debugVisibilityTargettableColor);
		}
	}
#endif
	return m_visionRange;
}

//-------------------------------------------------------------------------------------------------
void Object::setVisionRange( Real newVisionRange )
{
	m_visionRange = newVisionRange;
}

//-------------------------------------------------------------------------------------------------
Real Object::getShroudClearingRange() const
{
	Real shroudClearingRange=m_shroudClearingRange;

	if( getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{
		//structures under construction have limited vision range.  For now, base it
		//on the geometry extents so the structure can only see itself.
		shroudClearingRange = getGeometryInfo().getBoundingCircleRadius();
	}

#if defined(RTS_DEBUG)
	if (TheGlobalData->m_debugVisibility)
	{
		Vector3 pos(shroudClearingRange, 0, 0);
		for (int i = 0; i < TheGlobalData->m_debugVisibilityTileCount; ++i)
		{
			pos.Rotate_Z(1.0f * i / TheGlobalData->m_debugVisibilityTileCount * 2 * PI);
			Coord3D coord = { pos.X + getPosition()->x, pos.Y + getPosition()->y, pos.Z + getPosition()->z };

			addIcon(&coord, TheGlobalData->m_debugVisibilityTileWidth,
											TheGlobalData->m_debugVisibilityTileDuration,
											TheGlobalData->m_debugVisibilityDeshroudColor);
		}
	}
#endif

	return shroudClearingRange;
}

//-------------------------------------------------------------------------------------------------
void Object::setShroudClearingRange( Real newShroudClearingRange )
{
 	if( newShroudClearingRange != m_shroudClearingRange )
 	{
 		// The partition cell refresh is a slow operation, so only do it if you really have to.
 		// Range change is a valid reason to relook.
 		m_shroudClearingRange = newShroudClearingRange;

		/*
			Complete and total monkey hack fix.

			The problem: newObject doesn't get an initial pos, so all objects start at 0,0,0.
			Most code paths instantly move 'em to a good pos, but in some cases, that is too late:
			If we have search-and-destroy battle plan, we will apply it at that point, and clear out
			a vision range based on our current (wrong) location. Doh!

			So, this just sez: if you are at 0,0,0, don't call handlePartitionCellMaintenance()... since
			you will either (1) be moved elsewhere immediately, thus forcing it to be called via
			another route anyway, or (2) not be moved, which means you are a very naughty and worthless
			object anyway and we should just ignore you.

			Proper fix for next version is to require initial pos to be passed in to newObject so that
			all objects can start at their proper initial position from the start of the ctor.

			(srj)
		*/
		const Coord3D* pos = getPosition();
		if (pos->x || pos->y || pos->z)
		{
	 		handlePartitionCellMaintenance();
		}
 	}
}

//-------------------------------------------------------------------------------------------------
Real Object::getShroudRange() const
{
#if defined(RTS_DEBUG)
	if (TheGlobalData->m_debugVisibility)
	{
		Vector3 pos(m_shroudRange, 0, 0);
		for (int i = 0; i < TheGlobalData->m_debugVisibilityTileCount; ++i)
		{
			pos.Rotate_Z(1.0f * i / TheGlobalData->m_debugVisibilityTileCount * 2 * PI);
			Coord3D coord = { pos.X + getPosition()->x, pos.Y + getPosition()->y, pos.Z + getPosition()->z };

			addIcon(&coord, TheGlobalData->m_debugVisibilityTileWidth,
											TheGlobalData->m_debugVisibilityTileDuration,
											TheGlobalData->m_debugVisibilityGapColor);
		}
	}
#endif

	return m_shroudRange;
}

//-------------------------------------------------------------------------------------------------
void Object::setShroudRange( Real newShroudRange )
{
	m_shroudRange = newShroudRange;
}

//-------------------------------------------------------------------------------------------------
/** Given a special power template, find the module in the object that can implement it.
	* There can be at most one */
//-------------------------------------------------------------------------------------------------
SpecialPowerModuleInterface *Object::getSpecialPowerModule( const SpecialPowerTemplate *specialPowerTemplate ) const
{

	// sanity
	if( specialPowerTemplate == nullptr )
		return nullptr;

	// search the modules for the one with the matching template
	for (BehaviorModule** m = m_behaviors; *m; ++m)
	{
		SpecialPowerModuleInterface* sp = (*m)->getSpecialPower();
		if (!sp)
			continue;

		if( sp->isModuleForPower( specialPowerTemplate ) )
			return sp;
	}

	return nullptr;

}

//-------------------------------------------------------------------------------------------------
/** Execute special power */
//-------------------------------------------------------------------------------------------------
void Object::doSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, UnsignedInt commandOptions, Bool forced )
{

	if (isDisabled())
		return;

	// sanity
	if( !forced && TheSpecialPowerStore->canUseSpecialPower( this, specialPowerTemplate ) == FALSE )
		return;

	// get the module and execute
	SpecialPowerModuleInterface *mod = getSpecialPowerModule( specialPowerTemplate );
	if( mod )
		mod->doSpecialPower( commandOptions );

}

//-------------------------------------------------------------------------------------------------
/** Execute special power */
//-------------------------------------------------------------------------------------------------
void Object::doSpecialPowerAtObject( const SpecialPowerTemplate *specialPowerTemplate, Object *obj, UnsignedInt commandOptions, Bool forced )
{

	if (isDisabled())
		return;

	// sanity
	if( !forced && TheSpecialPowerStore->canUseSpecialPower( this, specialPowerTemplate ) == FALSE )
		return;

	// get the module and execute
	SpecialPowerModuleInterface *mod = getSpecialPowerModule( specialPowerTemplate );
	if( mod )
		mod->doSpecialPowerAtObject( obj, commandOptions );
}

//-------------------------------------------------------------------------------------------------
/** Execute special power */
//-------------------------------------------------------------------------------------------------
void Object::doSpecialPowerAtLocation( const SpecialPowerTemplate *specialPowerTemplate,
																			 const Coord3D *loc, Real angle, UnsignedInt commandOptions, Bool forced )
{

	if (isDisabled())
		return;

	// sanity
	if( !forced && TheSpecialPowerStore->canUseSpecialPower( this, specialPowerTemplate ) == FALSE )
		return;

	// get the module and execute
	SpecialPowerModuleInterface *mod = getSpecialPowerModule( specialPowerTemplate );
	if( mod )
		mod->doSpecialPowerAtLocation( loc, angle, commandOptions );

}

//-------------------------------------------------------------------------------------------------
/** Execute special power */
//-------------------------------------------------------------------------------------------------
void Object::doSpecialPowerUsingWaypoints( const SpecialPowerTemplate *specialPowerTemplate, const Waypoint *way, UnsignedInt commandOptions, Bool forced )
{

	if (isDisabled())
		return;

	// sanity
	if( !forced && TheSpecialPowerStore->canUseSpecialPower( this, specialPowerTemplate ) == FALSE )
		return;

	// get the module and execute
	SpecialPowerModuleInterface *mod = getSpecialPowerModule( specialPowerTemplate );
	if( mod )
		mod->doSpecialPowerUsingWaypoints( way, commandOptions );

}

//-------------------------------------------------------------------------------------------------
/** Execute command button ability */
//-------------------------------------------------------------------------------------------------
void Object::doCommandButton( const CommandButton *commandButton, CommandSourceType cmdSource )
{
	if (isDisabled())
		return;

	AIUpdateInterface *ai = getAIUpdateInterface();
	if( commandButton )
	{
		switch( commandButton->getCommandType() )
		{
			case GUI_COMMAND_SPECIAL_POWER:
				if( commandButton->getSpecialPowerTemplate() )
				{
					CommandOption commandOptions = (CommandOption)(commandButton->getOptions() | COMMAND_FIRED_BY_SCRIPT);
					doSpecialPower( commandButton->getSpecialPowerTemplate(), commandOptions, cmdSource == CMD_FROM_SCRIPT );
				}
				break;
			case GUI_COMMAND_STOP:
				if( ai )
				{
					ai->aiIdle( cmdSource );
				}
				break;

			case GUI_COMMAND_SWITCH_WEAPON:
				{
					WeaponSlotType weaponSlot = commandButton->getWeaponSlot();
					// GUI_COMMAND_SWITCH_WEAPON switches until un-switched, or switched to something else.
					this->setWeaponLock( weaponSlot, LOCKED_PERMANENTLY );
					break;
				}

			case GUI_COMMAND_FIRE_WEAPON:
				{
					WeaponSlotType weaponSlot = commandButton->getWeaponSlot();
					// GUI_COMMAND_FIRE_WEAPON merely fires till the weapon is empty or the attack is "done"
					this->setWeaponLock( weaponSlot, LOCKED_TEMPORARILY );
					break;
				}

			case GUI_COMMAND_OBJECT_UPGRADE:
			case GUI_COMMAND_PLAYER_UPGRADE:
				{
					const UpgradeTemplate *upgradeT = commandButton->getUpgradeTemplate();
					DEBUG_ASSERTCRASH( upgradeT, ("Undefined upgrade '%s' in player upgrade command", "UNKNOWN") );
					// sanity
					if( upgradeT == nullptr )
						break;
					if( upgradeT->getUpgradeType() == UPGRADE_TYPE_OBJECT )
					{
						if( hasUpgrade( upgradeT )  || !affectedByUpgrade( upgradeT ) )
							break;
					}
					// producer must have a production update
					ProductionUpdateInterface *pu = getProductionUpdateInterface();
					if( pu == nullptr )
						break;
					// queue the upgrade "research"
					pu->queueUpgrade( upgradeT );
				}
				break;
			case GUI_COMMAND_UNIT_BUILD:
			case GUI_COMMAND_DOZER_CONSTRUCT: {
				const ThingTemplate *tt = commandButton->getThingTemplate();
				ProductionUpdateInterface *pu = this->getProductionUpdateInterface();
				if (pu && tt) {
					pu->queueCreateUnit( tt, pu->requestUniqueUnitID());
				}
				break;
			}
			case GUI_COMMAND_HACK_INTERNET:{
				if( ai )
				{
					ai->aiHackInternet( cmdSource );
				}
				break;
			}

			//Feel free to implement object based command buttons.
			case GUI_COMMAND_COMBATDROP:
			case GUI_COMMAND_DOZER_CONSTRUCT_CANCEL:
			case GUI_COMMAND_CANCEL_UNIT_BUILD:
			case GUI_COMMAND_CANCEL_UPGRADE:
			case GUI_COMMAND_ATTACK_MOVE:
			case GUI_COMMAND_GUARD:
			case GUI_COMMAND_GUARD_WITHOUT_PURSUIT:
			case GUI_COMMAND_GUARD_FLYING_UNITS_ONLY:
			case GUI_COMMAND_WAYPOINTS:
			case GUI_COMMAND_EXIT_CONTAINER:
			case GUI_COMMAND_EVACUATE:
			case GUI_COMMAND_EXECUTE_RAILED_TRANSPORT:
			case GUI_COMMAND_BEACON_DELETE:
			case GUI_COMMAND_SET_RALLY_POINT:
			case GUI_COMMAND_SELL:
			case GUI_COMMAND_TOGGLE_OVERCHARGE:
#ifdef ALLOW_SURRENDER
			case GUI_COMMAND_POW_RETURN_TO_PRISON:
#endif
			case GUICOMMANDMODE_HIJACK_VEHICLE:
			case GUICOMMANDMODE_CONVERT_TO_CARBOMB:
#ifdef ALLOW_SURRENDER
			case GUICOMMANDMODE_PICK_UP_PRISONER:
#endif
			default:
				break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Execute command button ability directed at an object target */
//-------------------------------------------------------------------------------------------------
void Object::doCommandButtonAtObject( const CommandButton *commandButton, Object *obj, CommandSourceType cmdSource )
{
	if (isDisabled())
		return;

	AIUpdateInterface *ai = getAIUpdateInterface();
	if( commandButton )
	{
		switch( commandButton->getCommandType() )
		{
			case GUI_COMMAND_COMBATDROP:
				if( ai )
				{
					ai->aiCombatDrop( obj, *(obj->getPosition()), cmdSource );
				}
				break;
			case GUI_COMMAND_SPECIAL_POWER:
			{
				if( commandButton->getSpecialPowerTemplate() )
				{
					CommandOption commandOptions = (CommandOption)(commandButton->getOptions() | COMMAND_FIRED_BY_SCRIPT);
					doSpecialPowerAtObject( commandButton->getSpecialPowerTemplate(), obj, commandOptions, cmdSource == CMD_FROM_SCRIPT );
				}
				break;
			}

			case GUI_COMMAND_STOP:
				if( ai )
				{
					ai->aiIdle( cmdSource );
				}
				break;

			case GUICOMMANDMODE_HIJACK_VEHICLE:
				if ( ai )
				{
					ai->aiEnter(obj, cmdSource);
				}
				break;

			//Feel free to implement object based command buttons.
			case GUI_COMMAND_DOZER_CONSTRUCT:
			case GUI_COMMAND_DOZER_CONSTRUCT_CANCEL:
			case GUI_COMMAND_UNIT_BUILD:
			case GUI_COMMAND_CANCEL_UNIT_BUILD:
			case GUI_COMMAND_PLAYER_UPGRADE:
			case GUI_COMMAND_OBJECT_UPGRADE:
			case GUI_COMMAND_CANCEL_UPGRADE:
			case GUI_COMMAND_ATTACK_MOVE:
			case GUI_COMMAND_GUARD:
			case GUI_COMMAND_GUARD_WITHOUT_PURSUIT:
			case GUI_COMMAND_GUARD_FLYING_UNITS_ONLY:
			case GUI_COMMAND_WAYPOINTS:
			case GUI_COMMAND_EXIT_CONTAINER:
			case GUI_COMMAND_EVACUATE:
			case GUI_COMMAND_EXECUTE_RAILED_TRANSPORT:
			case GUI_COMMAND_BEACON_DELETE:
			case GUI_COMMAND_SET_RALLY_POINT:
			case GUI_COMMAND_SELL:
			case GUI_COMMAND_FIRE_WEAPON:
			case GUI_COMMAND_HACK_INTERNET:
			case GUI_COMMAND_TOGGLE_OVERCHARGE:
#ifdef ALLOW_SURRENDER
			case GUI_COMMAND_POW_RETURN_TO_PRISON:
#endif
			case GUI_COMMAND_SWITCH_WEAPON:
			case GUICOMMANDMODE_CONVERT_TO_CARBOMB:
#ifdef ALLOW_SURRENDER
			case GUICOMMANDMODE_PICK_UP_PRISONER:
#endif
			default:
				break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Execute command button ability directed at a location */
//-------------------------------------------------------------------------------------------------
void Object::doCommandButtonAtPosition( const CommandButton *commandButton, const Coord3D *pos, CommandSourceType cmdSource )
{
	if (isDisabled())
		return;

	AIUpdateInterface *ai = getAIUpdateInterface();
	if( commandButton )
	{
		switch( commandButton->getCommandType() )
		{
			case GUI_COMMAND_SPECIAL_POWER:
			{
				if( commandButton->getSpecialPowerTemplate() )
				{
					CommandOption commandOptions = (CommandOption)(commandButton->getOptions() | COMMAND_FIRED_BY_SCRIPT);
					doSpecialPowerAtLocation( commandButton->getSpecialPowerTemplate(), pos, INVALID_ANGLE, commandOptions, cmdSource == CMD_FROM_SCRIPT );
				}
				break;
			}
			case GUI_COMMAND_ATTACK_MOVE:
				if( ai )
				{
					ai->aiAttackMoveToPosition( pos, commandButton->getMaxShotsToFire(), cmdSource );
				}
				break;
			case GUI_COMMAND_STOP:
				if( ai )
				{
					ai->aiIdle( cmdSource );
				}
				break;

			case GUI_COMMAND_DOZER_CONSTRUCT:
				TheBuildAssistant->buildObjectNow( this, commandButton->getThingTemplate(), pos, 0.0f, getControllingPlayer() );
				break;

			case GUI_COMMAND_DOZER_CONSTRUCT_CANCEL:
			case GUI_COMMAND_UNIT_BUILD:
			case GUI_COMMAND_CANCEL_UNIT_BUILD:
			case GUI_COMMAND_PLAYER_UPGRADE:
			case GUI_COMMAND_OBJECT_UPGRADE:
			case GUI_COMMAND_CANCEL_UPGRADE:
			case GUI_COMMAND_GUARD:
			case GUI_COMMAND_GUARD_WITHOUT_PURSUIT:
			case GUI_COMMAND_GUARD_FLYING_UNITS_ONLY:
			case GUI_COMMAND_WAYPOINTS:
			case GUI_COMMAND_EXIT_CONTAINER:
			case GUI_COMMAND_EVACUATE:
			case GUI_COMMAND_EXECUTE_RAILED_TRANSPORT:
			case GUI_COMMAND_BEACON_DELETE:
			case GUI_COMMAND_SET_RALLY_POINT:
			case GUI_COMMAND_SELL:
			case GUI_COMMAND_FIRE_WEAPON:
			case GUI_COMMAND_HACK_INTERNET:
			case GUI_COMMAND_TOGGLE_OVERCHARGE:
#ifdef ALLOW_SURRENDER
			case GUI_COMMAND_POW_RETURN_TO_PRISON:
#endif
			case GUI_COMMAND_COMBATDROP:
			case GUI_COMMAND_SWITCH_WEAPON:
			case GUICOMMANDMODE_HIJACK_VEHICLE:
			case GUICOMMANDMODE_CONVERT_TO_CARBOMB:
#ifdef ALLOW_SURRENDER
			case GUICOMMANDMODE_PICK_UP_PRISONER:
#endif
			default:
				break;
		}
	}
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Object::clearLeechRangeModeForAllWeapons()
{
	m_weaponSet.clearLeechRangeModeForAllWeapons();
}

// ------------------------------------------------------------------------------------------------
/** Search our update modules for a production update interface and return it if one is found */
// ------------------------------------------------------------------------------------------------
ProductionUpdateInterface* Object::getProductionUpdateInterface()
{
	ProductionUpdateInterface *pui;

	// tell our update modules that we intend to do this special power.
	for( BehaviorModule** u = m_behaviors; *u; ++u )
	{

		pui = (*u)->getProductionUpdateInterface();
		if( pui )
			return pui;

	}

	return nullptr;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
DockUpdateInterface *Object::getDockUpdateInterface()
{
	DockUpdateInterface *dock = nullptr;

	for( BehaviorModule **u = m_behaviors; *u; ++u )
	{
		if( (dock = (*u)->getDockUpdateInterface()) != nullptr )
			return dock;
	}

	return nullptr;

}

// ------------------------------------------------------------------------------------------------
// Search our special power modules for a specific one.
// ------------------------------------------------------------------------------------------------
SpecialPowerModuleInterface* Object::findSpecialPowerModuleInterface( SpecialPowerType type ) const
{
	for (BehaviorModule** m = m_behaviors; *m; ++m)
	{
		SpecialPowerModuleInterface* sp = (*m)->getSpecialPower();
		if (!sp)
			continue;

		const SpecialPowerTemplate *spTemplate = sp->getSpecialPowerTemplate();
		if (spTemplate && spTemplate->getSpecialPowerType() == type)
		{
			return sp;
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
// Search our special power modules for the first occurrence of a shortcut special.
// ------------------------------------------------------------------------------------------------
SpecialPowerModuleInterface* Object::findAnyShortcutSpecialPowerModuleInterface() const
{
	for( BehaviorModule** m = m_behaviors; *m; ++m )
	{
		SpecialPowerModuleInterface* sp = (*m)->getSpecialPower();
		if (!sp)
			continue;

		const SpecialPowerTemplate *spTemplate = sp->getSpecialPowerTemplate();
		if( spTemplate && spTemplate->isShortcutPower() )
		{
			return sp;
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
/** Get spawn behavior interface from object */
// ------------------------------------------------------------------------------------------------
SpawnBehaviorInterface* Object::getSpawnBehaviorInterface() const
{
	for (BehaviorModule** m = m_behaviors; *m; ++m)
	{
		SpawnBehaviorInterface *sbi = (*m)->getSpawnBehaviorInterface();
		if( sbi )
		{
			return sbi;
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
// Simply find the special power module that is currently allowing plotting of positions to target.
// ------------------------------------------------------------------------------------------------
SpecialPowerUpdateInterface* Object::findSpecialPowerWithOverridableDestinationActive( SpecialPowerType type ) const
{
	for( BehaviorModule** u = m_behaviors; *u; ++u )
	{
		SpecialPowerUpdateInterface *spInterface = (*u)->getSpecialPowerUpdateInterface();
		if( spInterface )
		{
			if( spInterface->doesSpecialPowerHaveOverridableDestinationActive() )
			{
				return spInterface;
			}
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
// Simply find the special power module that is potentially allowed to plot positions to target.
// ------------------------------------------------------------------------------------------------
SpecialPowerUpdateInterface* Object::findSpecialPowerWithOverridableDestination( SpecialPowerType type ) const
{
	for( BehaviorModule** u = m_behaviors; *u; ++u )
	{
		SpecialPowerUpdateInterface *spInterface = (*u)->getSpecialPowerUpdateInterface();
		if( spInterface )
		{
			if( spInterface->doesSpecialPowerHaveOverridableDestination() )
			{
				return spInterface;
			}
		}
	}
	return nullptr;
}


// ------------------------------------------------------------------------------------------------
// Search our special ability updates for a specific one.
// ------------------------------------------------------------------------------------------------
SpecialAbilityUpdate* Object::findSpecialAbilityUpdate( SpecialPowerType type ) const
{
	for( BehaviorModule** u = m_behaviors; *u; ++u )
	{
		SpecialPowerUpdateInterface *spInterface = (*u)->getSpecialPowerUpdateInterface();
		if( spInterface && spInterface->isSpecialAbility() )
		{
			SpecialAbilityUpdate *spUpdate = (SpecialAbilityUpdate*)spInterface;
			if( spUpdate->getSpecialPowerType() == type )
			{
				return spUpdate;
			}
		}
	}

	return nullptr;
}

// ------------------------------------------------------------------------------------------------
SpecialPowerCompletionDie* Object::findSpecialPowerCompletionDie() const
{
	static NameKeyType key_SpecialPowerCompletionDie = NAMEKEY("SpecialPowerCompletionDie");
	return (SpecialPowerCompletionDie*)findModule(key_SpecialPowerCompletionDie);
}

// ------------------------------------------------------------------------------------------------
Int Object::getNumConsecutiveShotsFiredAtTarget( const Object *victim ) const
{
	return m_firingTracker ? m_firingTracker->getNumConsecutiveShotsAtVictim( victim ) : 0;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool Object::getSingleLogicalBonePosition(const char* boneName, Coord3D* position, Matrix3D* transform) const
{
	if (m_drawable && m_drawable->getPristineBonePositions( boneName, 0, position, transform, 1 ) == 1 )
	{
		m_drawable->convertBonePosToWorldPos( position, transform, position, transform );
		return true;
	}
	else
	{
		if (position)
			*position = *getPosition();
		if (transform)
			*transform = *getTransformMatrix();
		return false;
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool Object::getSingleLogicalBonePositionOnTurret( WhichTurretType whichTurret, const char* boneName, Coord3D* position, Matrix3D* transform ) const
{
	Coord3D turretPosition;
	Coord3D bonePosition;
	if( getDrawable() == nullptr  || getAI() == nullptr )
		return FALSE;

	// We need to find the TurretBone's pristine position.
	getDrawable()->getProjectileLaunchOffset( PRIMARY_WEAPON, 1, nullptr, whichTurret, &turretPosition, nullptr );
	// And the required bone's pristine position
	if( getDrawable()->getPristineBonePositions(boneName, 0, &bonePosition, nullptr, 1) != 1 )
		return FALSE;
	//Then we mojo the Logic position of the required bone like Missile firing does.  Using the logic twist of the turret
	Real turretRotation;
	getAI()->getTurretRotAndPitch( whichTurret, &turretRotation, nullptr );

	Matrix3D boneOffset(TRUE);// This will be from the turret to the requested bone

//	Vector3 bonePositionVector(	bonePosition.x - turretPosition.x,
//															bonePosition.y - turretPosition.y,
//															bonePosition.z - turretPosition.z );
	Vector3 bonePositionVector(	bonePosition.x,
															bonePosition.y,
															bonePosition.z );
	boneOffset.Translate(bonePositionVector);

	Matrix3D turnAdjustment(TRUE);// this is the turret twist to be applied to the final answer

	turnAdjustment.Translate( turretPosition.x, turretPosition.y, turretPosition.z );
	turnAdjustment.In_Place_Pre_Rotate_Z(turretRotation);
	turnAdjustment.Translate( -turretPosition.x, -turretPosition.y, -turretPosition.z );

	Matrix3D boneLogicTransform;
	boneLogicTransform.mul( turnAdjustment, boneOffset );

	Matrix3D worldTransform;
	convertBonePosToWorldPos(nullptr, &boneLogicTransform, nullptr, &worldTransform);

	Vector3 tmp = worldTransform.Get_Translation();
	Coord3D worldPos;
	worldPos.x = tmp.X;
	worldPos.y = tmp.Y;
	worldPos.z = tmp.Z;

	if( position )
		*position = worldPos;
	if( transform )
		*transform = worldTransform;

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Int Object::getMultiLogicalBonePosition(const char* boneNamePrefix, Int maxBones,
																				Coord3D* positions, Matrix3D* transforms,
																				Bool convertToWorld ) const
{
	Int count;
	if (m_drawable && (count = m_drawable->getPristineBonePositions( boneNamePrefix, 1, positions, transforms, maxBones )) > 0 )
	{
		if( convertToWorld )
		{
			for (Int i = 0; i < count; ++i)
				m_drawable->convertBonePosToWorldPos( positions ? &positions[i] : nullptr, transforms ? &transforms[i] : nullptr, positions ? &positions[i] : nullptr, transforms ? &transforms[i] : nullptr );
		}
		return count;
	}
	else
	{
		return 0;
	}
}

//=============================================================================
Bool Object::canProduceUpgrade( const UpgradeTemplate *upgrade )
{
	// TheSuperHackers @logic-client-separation helmutbuhler 11/04/2025
	// TheControlBar belongs to the client, we shouldn't depend on that to check this.

	// We need to have the button to make the upgrade.  CommandSets are a weird Logic/Client hybrid.
	const CommandSet *set = TheControlBar->findCommandSet(getCommandSetString());

 	for( Int buttonIndex = 0; buttonIndex < MAX_COMMANDS_PER_SET; buttonIndex++ )
 	{
 		const CommandButton *button = set->getCommandButton(buttonIndex);
 		if( button
				&&  ( (button->getCommandType() == GUI_COMMAND_PLAYER_UPGRADE)  ||  (button->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE) ) // Or else a button that requires an upgrade will appear the same as a button that gives an upgrade
				&&  button->getUpgradeTemplate()
				&&  (button->getUpgradeTemplate() == upgrade)
				)
 			return TRUE; // getUpgradeTemplate only returns something if it is actually an upgrade
 	}

 	return FALSE;// Cheatin' punk.
}

 //=============================================================================
const AsciiString& Object::getCommandSetString() const
{
	if (m_commandSetStringOverride.isNotEmpty())
		return m_commandSetStringOverride;

	return getTemplate()->friend_getCommandSetString();
}

//=============================================================================
// Object::defect, and related methods                                        =
//=============================================================================
void Object::defect( Team* newTeam, UnsignedInt detectionTime )
{
	if ( isContained() ) //@todo (KRIS?) make contained units unselectable, until then... lorenzen
	{
		return;
	}

	Player *player = getControllingPlayer();
	if ( !player )
		return;

	Team* myTeam = player->getDefaultTeam();
	if ( myTeam == newTeam ) // can't defect from my own team, that would be silly
		return;

	// things that are under construction, or sold, cannot defect.
	if (testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ||
			testStatus(OBJECT_STATUS_SOLD))
	{
		return;
	}

	// Before switch ////////////////////////////////////////

	//Design says:
	ProductionUpdateInterface *production = getProductionUpdateInterface();
	if ( production )
	{
		production->cancelAndRefundAllProduction();
	}

	// pop it up on the radar, so as to warn those who care
	// do this first, since after setTeam() the infiltrator
	// becomes the controllingplayer, not me

	// But don't do this is if the new team is not a real team.  "'Enemy' infiltration" wouldn't make
	// sense, and we are probably just reverting a cave or something.
	if( friend_getRadarData() && newTeam->getControllingPlayer()->isPlayableSide() && myTeam->getControllingPlayer()->isPlayableSide())
	{
		TheRadar->tryInfiltrationEvent( this );
	}

	friend_setUndetectedDefector( detectionTime > 0 );

	if (m_defectionHelper)
		m_defectionHelper->startDefectionTimer(detectionTime);

	// Switch ////////////////////////////////////////
	setTeam( newTeam );

	// After switch ////////////////////////////////////////

	AIUpdateInterface *ai = getAI();

	handlePartitionCellMaintenance();// to clear the shoud for my new master

	if ( ai )
	{
		ai->aiIdle( CMD_FROM_AI );
	}

	// Play our sound indicating we've been defected. (weird verbiage, but true.)
	AudioEventRTS voiceDefect = *getTemplate()->getVoiceDefect();
	voiceDefect.setObjectID(getID());
	TheAudio->addAudioEvent(&voiceDefect);

	//make the new recruit the only selected thing, awaiting new command to move, attack, etc...
	Drawable *dr = getDrawable();
	if (dr)
	{
		dr->flashAsSelected(); //This is the first of several flashes which get cue'd by doDefectorUpdateStuff()
		AudioEventRTS defectorTimerSound = TheAudio->getMiscAudio()->m_defectorTimerTickSound;
		defectorTimerSound.setObjectID( getID() );
		TheAudio->addAudioEvent(&defectorTimerSound);
	}

	ContainModuleInterface *ct = getContain();
	if( ct  &&  ct->isKickOutOnCapture() )
	{
		// Caves really really don't want to do this.
		ct->removeAllContained( TRUE );
	}

	// if it has parking places, defect anything parked there.
	for (BehaviorModule** i = getBehaviorModules(); *i; ++i)
	{
		ParkingPlaceBehaviorInterface* pp = (*i)->getParkingPlaceBehaviorInterface();
		if (pp)
		{
			pp->defectAllParkedUnits(newTeam, detectionTime);
			break;
		}
	}

	// defect any mines that are owned by this structure, right now.
	// unfortunately, structures don't keep a list of mines they own, so we must do
	// this the hard way :-( [fortunately, this doesn't happen very often, so this
	// is probably an acceptable, if icky, solution.] (srj)
	for (Object* mine = TheGameLogic->getFirstObject(); mine; mine = mine->getNextObject())
	{
		if (mine->isKindOf(KINDOF_MINE))
		{
			if (mine->getProducerID() == this->getID())
			{
				mine->setTeam(newTeam);
			}
		}
	}

}

//=============================================================================
// Object::goInvulnerable
//=============================================================================
void Object::goInvulnerable( UnsignedInt time )
{
	const Bool WITHOUT_DEFECTOR_FX = FALSE;


	friend_setUndetectedDefector( time > 0 );

	if (m_defectionHelper)
		m_defectionHelper->startDefectionTimer(time, WITHOUT_DEFECTOR_FX);

}

// ------------------------------------------------------------------------------------------------
/** Return the radar priority for this object type */
// ------------------------------------------------------------------------------------------------
RadarPriorityType Object::getRadarPriority() const
{
	// first, get the priority at the thing template level
	RadarPriorityType priority = getTemplate()->getDefaultRadarPriority();

	//
	// there are some objects that we want to show up on the radar when they have
	// certain properties ... here we will check for those properties unless the INI
	// setting of "not on radar" has been manually entered which explicitly forbids an
	// object from being on the radar ... by default objects get an "invalid" priority
	// on the radar and this means that we are free to decide one here if we want
	//
	if( priority == RADAR_PRIORITY_INVALID )
	{

		// objects that are "garrisonable" show up on the radar
		ContainModuleInterface *cmi = getContain();
		if( cmi && cmi->isGarrisonable() )
			priority = RADAR_PRIORITY_STRUCTURE;

		// objects that are "capturable" show up on the radar
		if( isKindOf( KINDOF_CAPTURABLE ) )
			priority = RADAR_PRIORITY_STRUCTURE;


	}

	// Carbombs will show up as units regardless of their default priority
	if ( testStatus( OBJECT_STATUS_IS_CARBOMB ) )
		priority = RADAR_PRIORITY_UNIT;


	// return the priority we're going to use
	return priority;

}

// ------------------------------------------------------------------------------------------------
AIGroup *Object::getGroup()
{
#if RETAIL_COMPATIBLE_AIGROUP
	return m_group;
#else
	return m_group.Peek();
#endif
}

//-------------------------------------------------------------------------------------------------
void Object::enterGroup( AIGroup *group )
{
//	DEBUG_LOG(("***AIGROUP %x involved in enterGroup on %x", group, this));
	// if we are in another group, remove ourselves from it first
	leaveGroup();

#if RETAIL_COMPATIBLE_AIGROUP
	m_group = group;
#else
	m_group = AIGroupPtr::Create_AddRef(group);
#endif
}

//-------------------------------------------------------------------------------------------------
void Object::leaveGroup()
{
//	DEBUG_LOG(("***AIGROUP %x involved in leaveGroup on %x", m_group, this));
	// if we are in a group, remove ourselves from it
	if (m_group)
	{
		// to avoid recursion, set m_group to nullptr before removing
		AIGroupPtr group = m_group;
		m_group = nullptr;
		group->remove( this );
	}
}

