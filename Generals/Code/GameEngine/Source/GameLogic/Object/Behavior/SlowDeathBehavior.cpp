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

// FILE: SlowDeathBehavior.cpp ///////////////////////////////////////////////////////////////////////
// Author:
// Desc:
///////////////////////////////////////////////////////////////////////////////////////////////////


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#define DEFINE_SLOWDEATHPHASE_NAMES
#include "Common/GameLOD.h"
#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/SlowDeathBehavior.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"


const Real BEGIN_MIDPOINT_RATIO = 0.35f;
const Real END_MIDPOINT_RATIO = 0.65f;

//-------------------------------------------------------------------------------------------------
SlowDeathBehaviorModuleData::SlowDeathBehaviorModuleData()
{
	m_sinkRate = 0;
	m_probabilityModifier = 10;
	m_modifierBonusPerOverkillPercent = 0;
	m_sinkDelay = 0;
	m_sinkDelayVariance = 0;
	m_destructionDelay = 0;
	m_destructionDelayVariance = 0;
	m_destructionAltitude = -10;
	m_maskOfLoadedEffects = 0; //assume no ocl, fx, or weapons.
	m_flingForce = 0;
	m_flingForceVariance = 0;
	m_flingPitch = 0;
	m_flingPitchVariance = 0;
	// redundant.
	//m_fx.clear();
	//m_ocls.clear();
	//m_weapons.clear();
}

//-------------------------------------------------------------------------------------------------
static void parseFX( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	SlowDeathBehaviorModuleData* self = (SlowDeathBehaviorModuleData*)instance;
	SlowDeathPhaseType sdphase = (SlowDeathPhaseType)INI::scanIndexList(ini->getNextToken(), TheSlowDeathPhaseNames);
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		const FXList *fxl = TheFXListStore->findFXList((token));	// could be null! this is OK!
		self->m_fx[sdphase].push_back(fxl);
		if (fxl)
			self->m_maskOfLoadedEffects |= SlowDeathBehaviorModuleData::HAS_FX;
	}
}

//-------------------------------------------------------------------------------------------------
static void parseOCL( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	SlowDeathBehaviorModuleData* self = (SlowDeathBehaviorModuleData*)instance;
	SlowDeathPhaseType sdphase = (SlowDeathPhaseType)INI::scanIndexList(ini->getNextToken(), TheSlowDeathPhaseNames);
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		const ObjectCreationList *ocl = TheObjectCreationListStore->findObjectCreationList(token);	// could be null! this is OK!
		self->m_ocls[sdphase].push_back(ocl);
		if (ocl)
			self->m_maskOfLoadedEffects |= SlowDeathBehaviorModuleData::HAS_OCL;
	}
}

//-------------------------------------------------------------------------------------------------
static void parseWeapon( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	SlowDeathBehaviorModuleData* self = (SlowDeathBehaviorModuleData*)instance;
	SlowDeathPhaseType sdphase = (SlowDeathPhaseType)INI::scanIndexList(ini->getNextToken(), TheSlowDeathPhaseNames);
	for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		const WeaponTemplate *wt = TheWeaponStore->findWeaponTemplate(token);	// could be null! this is OK!
		self->m_weapons[sdphase].push_back(wt);
		if (wt)
			self->m_maskOfLoadedEffects |= SlowDeathBehaviorModuleData::HAS_WEAPON;
	}
}

//-------------------------------------------------------------------------------------------------
/*static*/ void SlowDeathBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "SinkRate",													INI::parseVelocityReal,						nullptr, offsetof( SlowDeathBehaviorModuleData, m_sinkRate ) },
		{ "ProbabilityModifier",							INI::parseInt,										nullptr, offsetof( SlowDeathBehaviorModuleData, m_probabilityModifier ) },
		{ "ModifierBonusPerOverkillPercent",	INI::parsePercentToReal,					nullptr, offsetof( SlowDeathBehaviorModuleData, m_modifierBonusPerOverkillPercent ) },
		{ "SinkDelay",												INI::parseDurationUnsignedInt,		nullptr, offsetof( SlowDeathBehaviorModuleData, m_sinkDelay ) },
		{ "SinkDelayVariance",								INI::parseDurationUnsignedInt,		nullptr, offsetof( SlowDeathBehaviorModuleData, m_sinkDelayVariance ) },
		{ "DestructionDelay",									INI::parseDurationUnsignedInt,		nullptr, offsetof( SlowDeathBehaviorModuleData, m_destructionDelay ) },
		{ "DestructionDelayVariance",					INI::parseDurationUnsignedInt,		nullptr, offsetof( SlowDeathBehaviorModuleData, m_destructionDelayVariance ) },
		{ "DestructionAltitude",							INI::parseReal,										nullptr, offsetof( SlowDeathBehaviorModuleData, m_destructionAltitude ) },
		{ "FX",																parseFX,													nullptr, 0 },
		{ "OCL",															parseOCL,													nullptr, 0 },
		{ "Weapon",														parseWeapon,											nullptr, 0 },
		{ "FlingForce",												INI::parseReal,										nullptr, offsetof( SlowDeathBehaviorModuleData, m_flingForce) },
		{ "FlingForceVariance",								INI::parseReal,										nullptr, offsetof( SlowDeathBehaviorModuleData, m_flingForceVariance) },
		{ "FlingPitch",												INI::parseAngleReal,							nullptr, offsetof( SlowDeathBehaviorModuleData, m_flingPitch) },
		{ "FlingPitchVariance",								INI::parseAngleReal,							nullptr, offsetof( SlowDeathBehaviorModuleData, m_flingPitchVariance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
	p.add(DieMuxData::getFieldParse(), offsetof( SlowDeathBehaviorModuleData, m_dieMuxData ));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SlowDeathBehavior::SlowDeathBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_flags = 0;
	m_sinkFrame = 0;
	m_midpointFrame = 0;
	m_destructionFrame = 0;
	m_acceleratedTimeScale = 1.0f;

	if (getSlowDeathBehaviorModuleData()->m_probabilityModifier < 1)
	{
		DEBUG_CRASH(("ProbabilityModifier must be >= 1."));
		throw INI_INVALID_DATA;
	}

	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SlowDeathBehavior::~SlowDeathBehavior()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Int SlowDeathBehavior::getProbabilityModifier( const DamageInfo *damageInfo ) const
{
	// Calculating how far past dead we were allows us to pick more spectacular deaths when
	// severly killed, and more sedate ones when only slightly killed.
	// eg ( 200 hp max, had 10 left, took 50 damage, 40 overkill, (40/200) * 100 = 20 overkill %)
	Int overkillDamage = damageInfo->out.m_actualDamageDealt - damageInfo->out.m_actualDamageClipped;
	Real overkillPercent = (float)overkillDamage / (float)getObject()->getBodyModule()->getMaxHealth();
	Int overkillModifier = overkillPercent * getSlowDeathBehaviorModuleData()->m_modifierBonusPerOverkillPercent;

	return max( getSlowDeathBehaviorModuleData()->m_probabilityModifier + overkillModifier, 1 );
}

//-------------------------------------------------------------------------------------------------
static void calcRandomForce(Real minMag, Real maxMag, Real minPitch, Real maxPitch, Coord3D& force)
{
	Real angle = GameLogicRandomValueReal(-PI, PI);
	Real pitch = GameLogicRandomValueReal(minPitch, maxPitch);
	Real mag = GameLogicRandomValueReal(minMag, maxMag);

	Matrix3D mtx(1);
	mtx.Scale(mag);
	mtx.Rotate_Z(angle);
	mtx.Rotate_Y(-pitch);

	Vector3 v = mtx.Get_X_Vector();

	force.x = v.X;
	force.y = v.Y;
	force.z = v.Z;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SlowDeathBehavior::beginSlowDeath(const DamageInfo *damageInfo)
{
	if (!isSlowDeathActivated())
	{
		const SlowDeathBehaviorModuleData* d = getSlowDeathBehaviorModuleData();
		Object* obj = getObject();

		if (d->m_sinkRate && obj->isKindOf(KINDOF_INFANTRY))
		{

			Drawable *draw = getObject()->getDrawable();
			if ( draw )
			{
				// this object sinks slowly after it dies so don't draw a
				// floating shadow decal on the ground above it.
				obj->getDrawable()->setShadowsEnabled(false);
				draw->setTerrainDecalFadeTarget( 0.0f, -0.2f );
			}



		}


		// Ask game detail manager if we need to speedup all deaths to improve performance
		Real timeScale = TheGameLODManager->getSlowDeathScale();
		m_acceleratedTimeScale = 1.0f;	// assume normal death speed.

		if (timeScale == 0.0f && !d->hasNonLodEffects())
		{
			// Deaths happen instantly so just delete the object and return
			TheGameLogic->destroyObject(obj);
			return;
		}
		else
		{
			// timescale is some non-zero value so we may need to speed up death
			if( getObject()->isKindOf( KINDOF_HULK ) && TheGameLogic->getHulkMaxLifetimeOverride() != -1 )
			{
				//Scripts don't want hulks around, so start sinking immediately!
				m_sinkFrame = 1;
				m_midpointFrame = (LOGICFRAMES_PER_SECOND/2) + 1;
				m_destructionFrame = LOGICFRAMES_PER_SECOND + 1;
				m_acceleratedTimeScale = 1.0f;
			}
			else
			{
				m_sinkFrame = timeScale * (d->m_sinkDelay + GameLogicRandomValue(0, d->m_sinkDelayVariance));
				m_destructionFrame = timeScale * (d->m_destructionDelay + GameLogicRandomValue(0, d->m_destructionDelayVariance));
				m_midpointFrame = GameLogicRandomValue( BEGIN_MIDPOINT_RATIO * m_destructionFrame, END_MIDPOINT_RATIO * m_destructionFrame );
				m_acceleratedTimeScale = timeScale;
			}
		}

		UnsignedInt now = TheGameLogic->getFrame();

		if (d->m_flingForce > 0)
		{


			//Just in case this is a stingersoldier or other HELD object, lets set them free so they will fly
			// with their own physics during slow death
			if( obj->isDisabledByType( DISABLED_HELD ) )
			{
				static NameKeyType key_SlavedUpdate = NAMEKEY( "SlavedUpdate" );
				SlavedUpdate* slave = (SlavedUpdate*)obj->findUpdateModule( key_SlavedUpdate );
				if( slave )
				{
					slave->onSlaverDie( nullptr );
				}
			}

			PhysicsBehavior* physics = obj->getPhysics();
			if (physics)
			{
				// make sure we are at least a bit above the ground
				const Real MIN_ALTITUDE = 1.0f;
				Real altitude = obj->getHeightAboveTerrain();
				if (altitude < MIN_ALTITUDE)
				{
					Coord3D pos = *obj->getPosition();
					pos.z += MIN_ALTITUDE;
					obj->setPosition(&pos);
				}

				Coord3D force;
				calcRandomForce(d->m_flingForce, d->m_flingForce + d->m_flingForceVariance,
												d->m_flingPitch, d->m_flingPitch + d->m_flingPitchVariance, force);
				physics->setAllowToFall(true);
				physics->applyForce(&force);
				physics->setExtraBounciness(-1.0);					// we don't want this guy to bounce at all
				physics->setExtraFriction(-3 * SECONDS_PER_LOGICFRAME_REAL);							// reduce his ground friction a bit
				physics->setAllowBouncing(true);
				Real orientation = WWMath::Atan2Origin(force.y, force.x);
				physics->setAngles(orientation, 0, 0);
				obj->getDrawable()->setModelConditionState(MODELCONDITION_EXPLODED_FLAILING);
				m_flags |= (1<<FLUNG_INTO_AIR);
			}
			setWakeFrame(obj, UPDATE_SLEEP_NONE);
		}
		else
		{
			// we don't need to wake up immediately, but only when the first of these
			// counters wants to trigger....
			Int whenToWakeTime = m_sinkFrame;
			if (whenToWakeTime > m_destructionFrame)
				whenToWakeTime = m_destructionFrame;
			if (whenToWakeTime > m_midpointFrame)
				whenToWakeTime = m_midpointFrame;
			setWakeFrame(obj, UPDATE_SLEEP(whenToWakeTime));
		}
		m_sinkFrame += now;
		m_destructionFrame += now;
		m_midpointFrame += now;

		m_flags |= (1<<SLOW_DEATH_ACTIVATED);

		doPhaseStuff(SDPHASE_INITIAL);

	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SlowDeathBehavior::doPhaseStuff(SlowDeathPhaseType sdphase)
{
	const SlowDeathBehaviorModuleData* d = getSlowDeathBehaviorModuleData();
	Int idx, listSize;

	if (!d->m_maskOfLoadedEffects)
		return;	//has no ocl, fx, or weapons.

	listSize = d->m_fx[sdphase].size();
	if (listSize > 0)
	{
		idx = GameLogicRandomValue(0, listSize-1);
		const FXListVec& v = d->m_fx[sdphase];
		DEBUG_ASSERTCRASH(idx>=0&&idx<v.size(),("bad idx"));
		const FXList* fxl = v[idx];
		FXList::doFXObj(fxl, getObject(), nullptr);
	}

	listSize = d->m_ocls[sdphase].size();
	if (listSize > 0)
	{
		idx = GameLogicRandomValue(0, listSize-1);
		const OCLVec& v = d->m_ocls[sdphase];
		DEBUG_ASSERTCRASH(idx>=0&&idx<v.size(),("bad idx"));
		const ObjectCreationList* ocl = v[idx];
		ObjectCreationList::create(ocl, getObject(), nullptr);
	}

	listSize = d->m_weapons[sdphase].size();
	if (listSize > 0)
	{
		idx = GameLogicRandomValue(0, listSize-1);
		const WeaponTemplateVec& v = d->m_weapons[sdphase];
		DEBUG_ASSERTCRASH(idx>=0&&idx<v.size(),("bad idx"));
		const WeaponTemplate* wt = v[idx];
		if (wt)
		{
			TheWeaponStore->createAndFireTempWeapon(wt, getObject(), getObject()->getPosition());
		}
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime SlowDeathBehavior::update()
{
	//DEBUG_LOG(("updating SlowDeathBehavior %08lx",this));
	DEBUG_ASSERTCRASH(isSlowDeathActivated(), ("hmm, this should not be possible"));

	const SlowDeathBehaviorModuleData* d = getSlowDeathBehaviorModuleData();
	Object* obj = getObject();

	Real timeScale = TheGameLODManager->getSlowDeathScale();

	// Check if we have normal time scale but LODManager is requesting acceleration
	if (timeScale != 1.0f && m_acceleratedTimeScale == 1.0f && !d->hasNonLodEffects())
	{
		// speed of deaths has been increased since beginning of death
		// so adjust it to current levels.
		if (timeScale == 0)
		{
			// instant death
			TheGameLogic->destroyObject(obj);
			return UPDATE_SLEEP_NONE;
		}

		m_sinkFrame = (Real)m_sinkFrame * timeScale;
		m_midpointFrame = (Real)m_midpointFrame * timeScale;
		m_destructionFrame = (Real)m_destructionFrame * timeScale;
		m_acceleratedTimeScale = timeScale;
	};

	UnsignedInt now = TheGameLogic->getFrame();


	if ((m_flags & (1<<FLUNG_INTO_AIR)) != 0)
	{
		if ((m_flags & (1<<BOUNCED)) == 0)
		{
			++m_sinkFrame;
			++m_midpointFrame;
			++m_destructionFrame;
			if (!obj->isAboveTerrain())
			{
				obj->clearAndSetModelConditionFlags(MAKE_MODELCONDITION_MASK(MODELCONDITION_EXPLODED_FLAILING),
																						MAKE_MODELCONDITION_MASK(MODELCONDITION_EXPLODED_BOUNCING));
				m_flags |= (1<<BOUNCED);
			}

			// Here we want to make sure we die if we collide with a tree on the way down
			PhysicsBehavior *phys = obj->getPhysics();
			if ( phys )
			{
				ObjectID treeID = phys->getLastCollidee();
				Object *tree = TheGameLogic->findObjectByID( treeID );
				if ( tree )
				{
					if (tree->isKindOf( KINDOF_SHRUBBERY ) )
					{
						obj->setDisabled( DISABLED_HELD );
						obj->clearModelConditionFlags( MAKE_MODELCONDITION_MASK(MODELCONDITION_EXPLODED_FLAILING) );
						obj->clearModelConditionFlags( MAKE_MODELCONDITION_MASK(MODELCONDITION_EXPLODED_BOUNCING) );
						obj->setModelConditionFlags(   MAKE_MODELCONDITION_MASK(MODELCONDITION_PARACHUTING) ); //looks like he is snagged in a tree
						obj->setPositionZ( obj->getPosition()->z - (d->m_sinkRate * 50.0f) );// make him sink faster
						if ( !obj->isAboveTerrain() )
							TheGameLogic->destroyObject(obj);

					}
				}
			}



		}
	}

	if ( (now >= m_sinkFrame && d->m_sinkRate > 0.0f) )
	{
		// disable Physics (if any) so that we can control the sink...
		obj->setDisabled( DISABLED_HELD );
		Coord3D pos = *obj->getPosition();
		pos.z -= d->m_sinkRate / m_acceleratedTimeScale;
		obj->setPosition( &pos );
	}

	if( now >= m_midpointFrame && (m_flags & (1<<MIDPOINT_EXECUTED)) == 0 )
	{
		doPhaseStuff(SDPHASE_MIDPOINT);
		m_flags |= (1<<MIDPOINT_EXECUTED);
	}

	if (now >= m_destructionFrame)
	{
		doPhaseStuff(SDPHASE_FINAL);
		TheGameLogic->destroyObject(obj);
	}

	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
void SlowDeathBehavior::onDie( const DamageInfo *damageInfo )
{
	if (!isDieApplicable(damageInfo))
		return;

	AIUpdateInterface *ai = getObject()->getAIUpdateInterface();
	if (ai)
	{
		// has another AI already handled us. (hopefully another SlowDeathBehavior)
		if (ai->isAiInDeadState())
			return;
		ai->markAsDead();
	}

	// deselect this unit for all players.
	TheGameLogic->deselectObject(getObject(), PLAYERMASK_ALL, TRUE);

	Int total = 0;
	BehaviorModule** update = getObject()->getBehaviorModules();
	for (; *update; ++update)
	{
		SlowDeathBehaviorInterface* sdu = (*update)->getSlowDeathBehaviorInterface();
		if (sdu != nullptr && sdu->isDieApplicable(damageInfo))
		{
			total += sdu->getProbabilityModifier( damageInfo );
		}
	}
	DEBUG_ASSERTCRASH(total > 0, ("Hmm, this is wrong"));


	// this returns a value from 1...total, inclusive
	Int roll = GameLogicRandomValue(1, total);

	for (/* UpdateModuleInterface** */ update = getObject()->getBehaviorModules(); *update; ++update)
	{
		SlowDeathBehaviorInterface* sdu = (*update)->getSlowDeathBehaviorInterface();
		if (sdu != nullptr && sdu->isDieApplicable(damageInfo))
		{
			roll -= sdu->getProbabilityModifier( damageInfo );
			if (roll <= 0)
			{
				sdu->beginSlowDeath(damageInfo);
				return;
			}
		}
	}

	DEBUG_CRASH(("We should never get here"));
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SlowDeathBehavior::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SlowDeathBehavior::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// sink frame
	xfer->xferUnsignedInt( &m_sinkFrame );

	// midpoint frame
	xfer->xferUnsignedInt( &m_midpointFrame );

	// destruction frame
	xfer->xferUnsignedInt( &m_destructionFrame );

	// accelerated time scale
	xfer->xferReal( &m_acceleratedTimeScale );

	// flags
	xfer->xferUnsignedInt( &m_flags );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SlowDeathBehavior::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
