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

// FILE: ObjectCreationList.cpp ///////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, December 2001
// Desc:   ObjectCreationList descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_SHADOW_NAMES								// for TheShadowNames[]
#define DEFINE_WEAPONSLOTTYPE_NAMES

#define NO_DEBUG_CRC

#include "Common/AudioEventRTS.h"
#include "Common/DrawModule.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/GameLOD.h"

#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Shadow.h"

#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/DeliverPayloadAIUpdate.h"
#include "GameLogic/Module/FloatUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/SpecialPowerCompletionDie.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Module/RadiusDecalUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"

#include "GameLogic/AIPathfind.h"


#include "Common/CRCDebug.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

ObjectCreationListStore *TheObjectCreationListStore = nullptr;					///< the ObjectCreationList store definition

//-------------------------------------------------------------------------------------------------
static void adjustVector(Coord3D *vec, const Matrix3D* mtx)
{
	if (mtx)
	{
		Vector3 vectmp;
		vectmp.X = vec->x;
		vectmp.Y = vec->y;
		vectmp.Z = vec->z;
		vectmp = mtx->Rotate_Vector(vectmp);
		vec->x = vectmp.X;
		vec->y = vectmp.Y;
		vec->z = vectmp.Z;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE CLASSES ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
Object* ObjectCreationNugget::create( const Object* primary, const Object* secondary, UnsignedInt lifetimeFrames ) const
{
	return create( primary, primary ? primary->getPosition() : nullptr, secondary ? secondary->getPosition() : nullptr, INVALID_ANGLE, lifetimeFrames );
}

//-------------------------------------------------------------------------------------------------
//void ObjectCreationNugget::create( const Object* primaryObj, const Coord3D *primary, const Coord3D *secondary, Real angle, UnsignedInt lifetimeFrames ) const
//{
//	create( primaryObj, primary ? primary->getPosition() : nullptr, secondary ? secondary->getPosition() : nullptr, angle, lifetimeFrames );
//}

//-------------------------------------------------------------------------------------------------
//This one is called only when we have a nugget that doesn't care about createOwner.
Object* ObjectCreationNugget::create( const Object *primaryObj, const Coord3D *primary, const Coord3D *secondary, Bool createOwner, UnsignedInt lifetimeFrames ) const
{
	return create( primaryObj, primary, secondary, INVALID_ANGLE, lifetimeFrames );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class FireWeaponNugget : public ObjectCreationNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(FireWeaponNugget, "FireWeaponNugget")

public:

	FireWeaponNugget() :
    m_weapon(nullptr)
	{
	}

	virtual Object* create( const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Real angle, UnsignedInt lifetimeFrames = 0 ) const override
	{
		if (!primaryObj || !primary || !secondary)
		{
			DEBUG_CRASH(("You must have a primary and secondary source for this effect"));
      return nullptr;
    }

	  if (m_weapon)
	  {
		  TheWeaponStore->createAndFireTempWeapon( m_weapon, primaryObj, secondary );
	  }
		return nullptr;
  }

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Weapon", INI::parseWeaponTemplate,	nullptr, offsetof( FireWeaponNugget, m_weapon ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		FireWeaponNugget* nugget = newInstance(FireWeaponNugget);
		ini->initFromINI(nugget, myFieldParse);
		((ObjectCreationList*)instance)->addObjectCreationNugget(nugget);
	}

private:
	const WeaponTemplate* m_weapon;
};
EMPTY_DTOR(FireWeaponNugget)

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class AttackNugget : public ObjectCreationNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AttackNugget, "AttackNugget")
public:

	AttackNugget() :
    m_numberOfShots(1),
		m_weaponSlot(PRIMARY_WEAPON),
		m_deliveryDecalRadius(0)
	{
	}

	virtual Object* create( const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Real angle, UnsignedInt lifetimeFrames = 0 ) const override
	{
		if (!primaryObj || !primary || !secondary)
		{
			DEBUG_CRASH(("You must have a primary and secondary source for this effect"));
      return nullptr;
    }

		// Star trekkin, across the universe.
		// Boldly going forward now, cause we can't find reverse!

		// 1:30 left on the clock, Demo looming, should I de-const all of OCL since this one effect needs the
		// Primary to help make the objects?  Should I rewrite superweapons to completely subsume them
		// into normal special powers, so I can have a spell attack with a global timer and a spinny clock?
		// Should I const cast?  Should I eat them?  No!  I'd rather hurt them stomp them crush them hurt them
		// stomp them while I dance!  Down.  Those insects make me dance!  Down.  I'll hurt them while I dance.

		Object *primaryObject = const_cast<Object *>(primaryObj);
		AIUpdateInterface *ai = primaryObject->getAIUpdateInterface();
		if( ai )
		{
			// lock merely fires till the weapon is empty or the attack is "done"
			primaryObject->setWeaponLock( m_weaponSlot, LOCKED_TEMPORARILY );
			ai->aiAttackPosition( secondary, m_numberOfShots, CMD_FROM_AI );
		}

		static NameKeyType key_RadiusDecalUpdate = NAMEKEY("RadiusDecalUpdate");
		RadiusDecalUpdate *rd = (RadiusDecalUpdate*)primaryObject->findUpdateModule(key_RadiusDecalUpdate);
		if (rd)
		{
			rd->createRadiusDecal(m_deliveryDecalTemplate, m_deliveryDecalRadius, *secondary);
			rd->killWhenNoLongerAttacking(true);
		}
		return nullptr;
  }

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "NumberOfShots",	INI::parseInt,				nullptr, offsetof( AttackNugget, m_numberOfShots ) },
			{ "WeaponSlot",			INI::parseLookupList,	TheWeaponSlotTypeNamesLookupList, offsetof( AttackNugget, m_weaponSlot ) },
			{ "DeliveryDecal",				RadiusDecalTemplate::parseRadiusDecalTemplate,	nullptr, offsetof( AttackNugget, m_deliveryDecalTemplate ) },
			{ "DeliveryDecalRadius",	INI::parseReal, nullptr, offsetof(AttackNugget, m_deliveryDecalRadius) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		AttackNugget* nugget = newInstance(AttackNugget);
		ini->initFromINI(nugget, myFieldParse);
		((ObjectCreationList*)instance)->addObjectCreationNugget(nugget);
	}

private:
	RadiusDecalTemplate	m_deliveryDecalTemplate;
	Real								m_deliveryDecalRadius;
	Int									m_numberOfShots;
	WeaponSlotType			m_weaponSlot;
};
EMPTY_DTOR(AttackNugget)

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class DeliverPayloadNugget : public ObjectCreationNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DeliverPayloadNugget, "DeliverPayloadNugget")
public:

	DeliverPayloadNugget() :
		m_startAtPreferredHeight(true),
		m_startAtMaxSpeed(false),
		m_formationSize(1),
		m_formationSpacing(25.0f),
		m_errorRadius(0.0f),
		m_delayDeliveryFramesMax(0),
		m_convergenceFactor( 0.0f )
	{
		//Note: m_data is constructed with default values.
		m_payload.clear();
		m_putInContainerName.clear();
		m_transportName.clear();
	}

	virtual Object* create(const Object *primaryObj, const Coord3D *primary, const Coord3D *secondary, Real angle, UnsignedInt lifetimeFrames = 0 ) const override
	{
		return create( primaryObj, primary, secondary, true, lifetimeFrames );
	}

	virtual Object* create(const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Bool createOwner, UnsignedInt lifetimeFrames = 0 ) const override
	{
		if (!primaryObj || !primary || !secondary)
		{
			DEBUG_CRASH(("You must have a primary and secondary source for this effect"));
      return nullptr;
    }

		Team* owner = primaryObj ? primaryObj->getControllingPlayer()->getDefaultTeam() : nullptr;


		//What I'm doing for the purposes of the formations is to calculate the relative positions of
		//each member of the formation. To do so, we take the vector from the target location to the
		//lead plane location, normalize it, then rotate it 90 degrees (CW and CCW). When we add the
		//resultant vectors to the initial vectors, we can calculate the delta positions for each plane.
		Real CCWx = 0.0f, CCWy = 0.0f, CWx = 0.0f, CWy = 0.0f;

		if( m_formationSize > 1 )
		{
			//Get the delta x and y values from the target to the origin.
			Real dx = primary->x - secondary->x;
			Real dy = primary->y - secondary->y;

			//Calc length
			Real length = WWMath::SqrtOrigin( dx*dx + dy*dy );

			//Normalize length
			dx /= length;
			dy /= length;

			//Rotate 90 degrees CCW.
			Real radians = 90.0f * PI / 180.0f;
			Real s = Sin( radians );
			Real c = Cos( radians );
			CCWx = dx * c + dy * -s + dx;
			CCWy = dx * s + dy * c + dy;

			//Rotate 90 degrees CW
			s = Sin( -radians );
			c = Cos( -radians );
			CWx = dx * c + dy * -s + dx;
			CWy = dx * s + dy * c + dy;
		}

		Object *firstTransport = nullptr;
		for( UnsignedInt formationIndex = 0; formationIndex < m_formationSize; formationIndex++ )
		{
			Coord3D offset;
			offset.zero();

			Int offsetMultiplier = ( formationIndex + 1 ) / 2 * m_formationSpacing;

			if( formationIndex % 2 )
			{
				//Formation index is odd -- use the CCW deltas
				offset.x = CCWx * offsetMultiplier;
				offset.y = CCWy * offsetMultiplier;
			}
			else
			{
				//Formation index is even -- use the CW deltas
				offset.x = CWx * offsetMultiplier;
				offset.y = CWy * offsetMultiplier;
			}

			Coord3D startPos = *primary;
			Coord3D moveToPos = *secondary;
			startPos.add( offset );
			//Also give our moveToPos the same offset to maintain perfect formation.
			moveToPos.add( offset );

			Coord3D targetPos = *secondary;


			//Our target position only applies when using fireweapon and when we have multiple planes,
			//as is the case with the napalm strike. The target position either be somewhere between the
			//moveToPos of the lead plane and that of the relative offset -- determined by the convergenceFactor.
			targetPos.x += offset.x * (1.0f - m_convergenceFactor);
			targetPos.y += offset.y * (1.0f - m_convergenceFactor);


			// first guy in each formation is always spot-on (to keep targeting cursor well-matched)
			if ( m_errorRadius > 1.0f && formationIndex > 0 )
			{
				Real randomRadius = GameLogicRandomValueReal(0, m_errorRadius );
				Real randomAngle = GameLogicRandomValueReal(0, PI*2 );
				targetPos.x += randomRadius * Cos( randomAngle );
				targetPos.y += randomRadius * Sin( randomAngle );
			}


			Real orient = WWMath::Atan2Origin( moveToPos.y - startPos.y, moveToPos.x - startPos.x);
			if( m_data.m_distToTarget > 0 )
			{
				const Real SLOP = 1.5f;
				startPos.x -= Cos(orient) * m_data.m_distToTarget * SLOP;
				startPos.y -= Sin(orient) * m_data.m_distToTarget * SLOP;
			}

			Object *transport;

			if( createOwner )
			{
				const ThingTemplate* ttn = TheThingFactory->findTemplate(m_transportName);
				transport = TheThingFactory->newObject( ttn, owner );
				if( !transport )
				{
					return nullptr;
				}
				if( !firstTransport )
				{
					firstTransport = transport;
				}
				transport->setPosition(&startPos);
				transport->setOrientation(orient);
				transport->setProducer(primaryObj);
				//Adding this nifty flag allows enemy players to target it manually with weapons :)
				transport->setScriptStatus( OBJECT_STATUS_SCRIPT_TARGETABLE );

				if ( m_delayDeliveryFramesMax > 0 )
				{
					transport->setDisabledUntil( DISABLED_DEFAULT, TheGameLogic->getFrame() + GameLogicRandomValue(0, m_delayDeliveryFramesMax) );
				}
			}
			else
			{
				transport = (Object*)primaryObj;
			}

			// Notify special power tracking
			SpecialPowerCompletionDie *die = transport->findSpecialPowerCompletionDie();
			if (die)
			{
				if (formationIndex == 0)
					die->setCreator(primaryObj->getID());
				else
					die->setCreator(INVALID_ID);
			}

			static NameKeyType key_DeliverPayloadAIUpdate = NAMEKEY("DeliverPayloadAIUpdate");
			DeliverPayloadAIUpdate *ai = (DeliverPayloadAIUpdate*)transport->findUpdateModule(key_DeliverPayloadAIUpdate);
			if( ai )
			{
				if( m_startAtMaxSpeed && createOwner )
				{
					PhysicsBehavior* physics = transport->getPhysics();
					if (physics)
					{
						Coord3D startingForce = *transport->getUnitDirectionVector2D();
						Real maxSpeed = ai->getCurLocomotor()->getMaxSpeedForCondition(transport->getBodyModule()->getDamageState());
						Real factor = maxSpeed * physics->getMass();
						startingForce.x *= factor;
						startingForce.y *= factor;
						startingForce.z *= factor;
						physics->applyMotiveForce( &startingForce );
					}
				}

				// only the first guy in each formation gets a delivery decal
				DeliverPayloadData data = m_data;
				if (formationIndex != 0)
					data.m_deliveryDecalRadius = 0;
				ai->deliverPayload( &moveToPos, &targetPos, &data );
				if( m_startAtPreferredHeight && createOwner )
				{
					startPos.z = TheTerrainLogic->getGroundHeight(startPos.x, startPos.y) + ai->getCurLocomotor()->getPreferredHeight();
					transport->setPosition(&startPos);
				}

				const ThingTemplate* putInContainerTmpl = m_putInContainerName.isEmpty() ? nullptr : TheThingFactory->findTemplate(m_putInContainerName);
				for (std::vector<Payload>::const_iterator it = m_payload.begin(); it != m_payload.end(); ++it)
  			{
					const ThingTemplate* payloadTmpl = TheThingFactory->findTemplate(it->m_payloadName);
					if( !payloadTmpl )
					{
						DEBUG_CRASH( ("DeliverPayloadNugget::create() -- %s couldn't create %s (template not found).",
							transport->getTemplate()->getName().str(), it->m_payloadName.str() ) );
						return nullptr;
					}
					for (int i = 0; i < it->m_payloadCount; ++i)
					{
						Object* payload = TheThingFactory->newObject( payloadTmpl, owner );
						payload->setPosition(&startPos);
						payload->setProducer(transport);

						// Notify special power tracking
						SpecialPowerCompletionDie *die = payload->findSpecialPowerCompletionDie();
						if (die)
						{
							if (formationIndex == 0 && i == 0)
								die->setCreator(primaryObj->getID());
							else
								die->setCreator(INVALID_ID);
						}

						if (putInContainerTmpl)
						{
							Object* container = TheThingFactory->newObject( putInContainerTmpl, owner );
							container->setPosition(&startPos);
							container->setProducer(transport);

							// Notify special power tracking
							SpecialPowerCompletionDie *die = container->findSpecialPowerCompletionDie();
							if (die)
							{
								if (formationIndex == 0 && i == 0)
									die->setCreator(primaryObj->getID());
								else
									die->setCreator(INVALID_ID);
							}

							if (container->getContain() && container->getContain()->isValidContainerFor(payload, true))
							{
								container->getContain()->addToContain(payload);
								payload = container;
							}
							else
							{
								DEBUG_CRASH(("DeliverPayload: PutInContainer %s is full, or not valid for the payload %s!",m_putInContainerName.str(),it->m_payloadName.str()));
							}
						}

						if (transport->getContain() && transport->getContain()->isValidContainerFor(payload, true))
						{
							transport->getContain()->addToContain(payload);
						}
						else
						{
							DEBUG_CRASH(("DeliverPayload: transport %s is full, or not valid for the payload %s!",m_transportName.str(),it->m_payloadName.str()));
						}
					}
				}
			}
			else
			{
				DEBUG_CRASH(("You should really have a DeliverPayloadAIUpdate here"));
			}
		}
		return firstTransport;
	}

	static void parsePayload( INI* ini, void *instance, void *store, const void* /*userData*/ )
	{
		DeliverPayloadNugget* self = (DeliverPayloadNugget*)instance;
		const char* name = ini->getNextToken();
		const char* countStr = ini->getNextTokenOrNull();
		Int count = countStr ? INI::scanInt(countStr) : 1;

		Payload p;
		p.m_payloadName.set(name);
		p.m_payloadCount = count;
		self->m_payload.push_back(p);
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			//***************************************************************
			//OBJECT CREATION LIST SPECIFIC DATA -- once created data no longer needed
			//The transport(s) that carry all the payload items (and initial physics information)
			{ "Transport",								INI::parseAsciiString,				nullptr, offsetof(DeliverPayloadNugget, m_transportName) },
			{ "StartAtPreferredHeight",		INI::parseBool,								nullptr, offsetof(DeliverPayloadNugget, m_startAtPreferredHeight) },
			{ "StartAtMaxSpeed",					INI::parseBool,								nullptr, offsetof(DeliverPayloadNugget, m_startAtMaxSpeed) },

			//For multiple transports, this defines the formation (and convergence if all weapons will hit same target)
			{ "FormationSize",						INI::parseUnsignedInt,					nullptr, offsetof( DeliverPayloadNugget, m_formationSize) },
			{ "FormationSpacing",					INI::parseReal,									nullptr, offsetof( DeliverPayloadNugget, m_formationSpacing) },
			{ "WeaponConvergenceFactor",	INI::parseReal,									nullptr, offsetof( DeliverPayloadNugget, m_convergenceFactor ) },
			{ "WeaponErrorRadius",				INI::parseReal,									nullptr, offsetof( DeliverPayloadNugget, m_errorRadius ) },
			{ "DelayDeliveryMax",					INI::parseDurationUnsignedInt,	nullptr, offsetof( DeliverPayloadNugget, m_delayDeliveryFramesMax ) },

			//Payload information (it's all created now and stored inside)
			{ "Payload",									parsePayload,									nullptr, 0 },
			{ "PutInContainer",						INI::parseAsciiString,				nullptr, offsetof( DeliverPayloadNugget, m_putInContainerName) },
			//END OBJECT CREATION LIST SPECIFIC DATA
			//***************************************************************

			//***************************************************************
			//DELIVERPAYLOADDATA contains the rest (and most) of the parsed data.
			//***************************************************************
			{ nullptr, nullptr, nullptr, 0 }
		};

		DeliverPayloadNugget* nugget = newInstance(DeliverPayloadNugget);

		MultiIniFieldParse p;
		p.add(myFieldParse);
		p.add(DeliverPayloadData::getFieldParse(), offsetof( DeliverPayloadNugget, m_data ));
 		ini->initFromINIMulti(nugget, p);
		((ObjectCreationList*)instance)->addObjectCreationNugget(nugget);
	}

private:

	struct Payload
	{
		AsciiString m_payloadName;
		Int m_payloadCount;
	};

	//Specific data needed to create the transport(s), internal payload, and initial physics.
  AsciiString           m_transportName;
	AsciiString						m_putInContainerName;
	std::vector<Payload>	m_payload;
	Real									m_formationSpacing;
	Real									m_convergenceFactor;
	Real									m_errorRadius;
	UnsignedInt						m_delayDeliveryFramesMax;
	UnsignedInt						m_formationSize;
	Bool									m_startAtPreferredHeight;
	Bool									m_startAtMaxSpeed;

	//AI specific data passed over to DeliverPayloadAIUpdate::deliver()
	DeliverPayloadData		m_data;
};
EMPTY_DTOR(DeliverPayloadNugget)

//-------------------------------------------------------------------------------------------------
static void calcRandomForce(Real minMag, Real maxMag, Real minPitch, Real maxPitch, Coord3D* force)
{
	Real angle = GameLogicRandomValueReal(0, 2*PI);
	Real pitch = GameLogicRandomValueReal(minPitch, maxPitch);
	Real mag = GameLogicRandomValueReal(minMag, maxMag);

	Matrix3D mtx(1);
	mtx.Scale(mag);
	mtx.Rotate_Z(angle);
	mtx.Rotate_Y(-pitch);

	Vector3 v = mtx.Get_X_Vector();

	force->x = v.X;
	force->y = v.Y;
	force->z = v.Z;
}


//-------------------------------------------------------------------------------------------------
class ApplyRandomForceNugget : public ObjectCreationNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ApplyRandomForceNugget, "ApplyRandomForceNugget")
public:

	ApplyRandomForceNugget() :
		m_spinRate(0.0f),
		m_minMag(0.0f),
		m_maxMag(0.0f),
		m_minPitch(0.0f),
		m_maxPitch(0.0f)
	{
	}

	virtual Object* create( const Object* primary, const Object* secondary, UnsignedInt lifetimeFrames = 0 ) const override
	{
		if (primary)
		{
			/// @todo srj -- ack. const_cast is evil.
			PhysicsBehavior* p = const_cast<Object*>(primary)->getPhysics();
      if (p)
      {
				Coord3D force;
				calcRandomForce(m_minMag, m_maxMag, m_minPitch, m_maxPitch, &force);
				p->applyForce(&force);

			  Real yaw = GameLogicRandomValueReal( -m_spinRate, m_spinRate );
			  Real roll = GameLogicRandomValueReal( -m_spinRate, m_spinRate );
			  Real pitch = GameLogicRandomValueReal( -m_spinRate, m_spinRate );
			  p->setYawRate(yaw);
			  p->setRollRate(roll);
			  p->setPitchRate(pitch);
      }
      else
      {
  			DEBUG_CRASH(("You must have a Physics module source for this effect"));
      }
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
		return nullptr;
	}

	virtual Object* create(const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Real angle, UnsignedInt lifetimeFrames = 0 ) const override
	{
		DEBUG_CRASH(("You must call this effect with an object, not a location"));
		return nullptr;
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "SpinRate",					INI::parseAngularVelocityReal,	nullptr, offsetof(ApplyRandomForceNugget, m_spinRate) },
			{ "MinForceMagnitude",	INI::parseReal,	nullptr, offsetof(ApplyRandomForceNugget, m_minMag) },
			{ "MaxForceMagnitude",	INI::parseReal,	nullptr, offsetof(ApplyRandomForceNugget, m_maxMag) },
			{ "MinForcePitch",	INI::parseAngleReal,	nullptr, offsetof(ApplyRandomForceNugget, m_minPitch) },
			{ "MaxForcePitch",	INI::parseAngleReal,	nullptr, offsetof(ApplyRandomForceNugget, m_maxPitch) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		ApplyRandomForceNugget* nugget = newInstance(ApplyRandomForceNugget);
		ini->initFromINI(nugget, myFieldParse);
		((ObjectCreationList*)instance)->addObjectCreationNugget(nugget);
	}

protected:

private:
	Real											m_spinRate;
	Real											m_minMag, m_maxMag;
	Real											m_minPitch, m_maxPitch;
};
EMPTY_DTOR(ApplyRandomForceNugget)

//-------------------------------------------------------------------------------------------------
enum DebrisDisposition CPP_11(: Int)
{
	LIKE_EXISTING						= 0x00000001,
	ON_GROUND_ALIGNED				= 0x00000002,
	SEND_IT_FLYING					= 0x00000004,
	SEND_IT_UP							= 0x00000008,
	SEND_IT_OUT							= 0x00000010,
	RANDOM_FORCE						= 0x00000020,
	FLOATING								= 0x00000040,
	INHERIT_VELOCITY				= 0x00000080,
	WHIRLING								= 0x00000100
};

static const char* const DebrisDispositionNames[] =
{
	"LIKE_EXISTING",
	"ON_GROUND_ALIGNED",
	"SEND_IT_FLYING",
	"SEND_IT_UP",
	"SEND_IT_OUT",
	"RANDOM_FORCE",
	"FLOATING",
	"INHERIT_VELOCITY",
	"WHIRLING",
	nullptr
};

std::vector<AsciiString>	debrisModelNamesGlobalHack;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static void parseFrictionPerSec( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	Real fricPerSec = INI::scanReal(ini->getNextToken());
	Real fricPerFrame = fricPerSec * SECONDS_PER_LOGICFRAME_REAL;
	*(Real *)store = fricPerFrame;
}

//-------------------------------------------------------------------------------------------------
class GenericObjectCreationNugget : public ObjectCreationNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GenericObjectCreationNugget, "GenericObjectCreationNugget")
public:

	GenericObjectCreationNugget() :
		m_requiresLivePlayer(FALSE),
		m_debrisToGenerate(1),
		m_mass(0),
		m_extraBounciness(0),
		m_extraFriction(0),
		m_disposition(ON_GROUND_ALIGNED),
		m_dispositionIntensity(0.0f),
		m_spinRate(-1.0f),
		m_yawRate(-1.0f),
		m_rollRate(-1.0f),
		m_pitchRate(-1.0f),
		m_nameAreObjects(true),
		m_okToChangeModelColor(false),
		m_minLODRequired(STATIC_GAME_LOD_LOW),
		m_ignorePrimaryObstacle(false),
		m_inheritsVeterancy(false),
    m_diesOnBadLand(FALSE),
		m_skipIfSignificantlyAirborne(false),
		m_invulnerableTime(0),
		m_containInsideSourceObject(FALSE),
    m_minHealth(1.0f),
		m_maxHealth(1.0f),
		m_orientInForceDirection(false),
		m_spreadFormation(false),
		m_minDistanceAFormation(0.0f),
		m_minDistanceBFormation(0.0f),
		m_maxDistanceFormation(0.0f),
		m_fadeIn(false),
		m_fadeOut(false),
		m_fadeFrames(0),
		m_fadeSoundName(AsciiString::TheEmptyString),
		m_particleSysName(AsciiString::TheEmptyString),
		m_putInContainer(AsciiString::TheEmptyString),
		m_minMag(0.0f),
		m_maxMag(0.0f),
		m_minPitch(0.0f),
		m_maxPitch(0.0f),
		m_minFrames(0),
		m_maxFrames(0),
		m_shadowType(SHADOW_NONE),
		m_fxFinal(nullptr),
		m_preserveLayer(true),
		m_objectCount(0)
	{
		m_offset.zero();
	}

	virtual Object* create(const Object* primary, const Object* secondary, UnsignedInt lifetimeFrames = 0 ) const override
	{
		if (primary)
		{
			if (m_skipIfSignificantlyAirborne && primary->isSignificantlyAboveTerrain())
				return nullptr;

			return reallyCreate( primary->getPosition(), primary->getTransformMatrix(), primary->getOrientation(), primary, lifetimeFrames );
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
		return nullptr;
	}

	virtual Object* create(const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Real angle, UnsignedInt lifetimeFrames = 0 ) const override
	{
		if (primary)
		{
			const Matrix3D *xfrm = nullptr;
			if( angle == INVALID_ANGLE )
			{
				//Vast majority of OCL's don't care about the angle, so if it comes in invalid, default the angle to 0.
				angle = 0.0f;
			}
			return reallyCreate( primary, xfrm, angle, primaryObj, lifetimeFrames );
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
		return nullptr;
	}

	static const FieldParse* getCommonFieldParse()
	{
		static const FieldParse commonFieldParse[] =
		{
			{ "PutInContainer", INI::parseAsciiString, nullptr, offsetof( GenericObjectCreationNugget, m_putInContainer) },
			{ "ParticleSystem",		INI::parseAsciiString, nullptr, offsetof( GenericObjectCreationNugget, m_particleSysName) },
			{ "Count",						INI::parseInt,						nullptr, offsetof( GenericObjectCreationNugget, m_debrisToGenerate ) },
			{ "IgnorePrimaryObstacle", INI::parseBool, nullptr, offsetof(GenericObjectCreationNugget, m_ignorePrimaryObstacle) },
			{ "OrientInForceDirection", INI::parseBool, nullptr, offsetof(GenericObjectCreationNugget, m_orientInForceDirection) },
			{ "ExtraBounciness",				INI::parseReal,						nullptr, offsetof( GenericObjectCreationNugget, m_extraBounciness ) },
			{ "ExtraFriction",				parseFrictionPerSec,						nullptr, offsetof( GenericObjectCreationNugget, m_extraFriction ) },
			{ "Offset",						INI::parseCoord3D,				nullptr, offsetof( GenericObjectCreationNugget, m_offset ) },
			{ "Disposition",			INI::parseBitString32,			DebrisDispositionNames, offsetof( GenericObjectCreationNugget, m_disposition ) },
			{ "DispositionIntensity",	INI::parseReal,						nullptr,	offsetof( GenericObjectCreationNugget, m_dispositionIntensity ) },
			{ "SpinRate",					INI::parseAngularVelocityReal,	nullptr, offsetof(GenericObjectCreationNugget, m_spinRate) },
			{ "YawRate",					INI::parseAngularVelocityReal,	nullptr, offsetof(GenericObjectCreationNugget, m_yawRate) },
			{ "RollRate",					INI::parseAngularVelocityReal,	nullptr, offsetof(GenericObjectCreationNugget, m_rollRate) },
			{ "PitchRate",				INI::parseAngularVelocityReal,	nullptr, offsetof(GenericObjectCreationNugget, m_pitchRate) },
			{ "MinForceMagnitude",	INI::parseReal,	nullptr, offsetof(GenericObjectCreationNugget, m_minMag) },
			{ "MaxForceMagnitude",	INI::parseReal,	nullptr, offsetof(GenericObjectCreationNugget, m_maxMag) },
			{ "MinForcePitch",	INI::parseAngleReal,	nullptr, offsetof(GenericObjectCreationNugget, m_minPitch) },
			{ "MaxForcePitch",	INI::parseAngleReal,	nullptr, offsetof(GenericObjectCreationNugget, m_maxPitch) },
			{ "MinLifetime",					INI::parseDurationUnsignedInt,		nullptr, offsetof( GenericObjectCreationNugget, m_minFrames ) },
			{ "MaxLifetime",					INI::parseDurationUnsignedInt,		nullptr, offsetof( GenericObjectCreationNugget, m_maxFrames ) },
			{ "SpreadFormation",			INI::parseBool,	nullptr, offsetof(GenericObjectCreationNugget, m_spreadFormation) },
			{ "MinDistanceAFormation",	INI::parseReal, nullptr, offsetof(GenericObjectCreationNugget, m_minDistanceAFormation) },
			{ "MinDistanceBFormation",	INI::parseReal, nullptr, offsetof(GenericObjectCreationNugget, m_minDistanceBFormation) },
			{ "MaxDistanceFormation",	INI::parseReal, nullptr, offsetof(GenericObjectCreationNugget, m_maxDistanceFormation) },
			{ "FadeIn",			INI::parseBool,	nullptr, offsetof(GenericObjectCreationNugget, m_fadeIn) },
			{ "FadeOut",			INI::parseBool,	nullptr, offsetof(GenericObjectCreationNugget, m_fadeOut) },
			{ "FadeTime",	INI::parseDurationUnsignedInt,	nullptr, offsetof(GenericObjectCreationNugget, m_fadeFrames) },
			{ "FadeSound", INI::parseAsciiString, nullptr, offsetof( GenericObjectCreationNugget, m_fadeSoundName) },
			{ "PreserveLayer", INI::parseBool, nullptr, offsetof( GenericObjectCreationNugget, m_preserveLayer) },
			{ "DiesOnBadLand",	INI::parseBool, nullptr, offsetof(GenericObjectCreationNugget, m_diesOnBadLand) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		return commonFieldParse;
	}

	static void parseObject(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "ContainInsideSourceObject", INI::parseBool, nullptr, offsetof( GenericObjectCreationNugget, m_containInsideSourceObject) },
			{ "ObjectNames",				parseDebrisObjectNames,		nullptr, 0 },
			{ "ObjectCount",				INI::parseInt,  nullptr, offsetof(GenericObjectCreationNugget, m_objectCount) },
			{ "InheritsVeterancy",	INI::parseBool, nullptr, offsetof(GenericObjectCreationNugget, m_inheritsVeterancy) },
			{ "SkipIfSignificantlyAirborne", INI::parseBool, nullptr, offsetof(GenericObjectCreationNugget, m_skipIfSignificantlyAirborne) },
			{ "InvulnerableTime",		INI::parseDurationUnsignedInt, nullptr, offsetof(GenericObjectCreationNugget, m_invulnerableTime) },
			{ "MinHealth",					INI::parsePercentToReal, nullptr, offsetof(GenericObjectCreationNugget, m_minHealth) },
			{ "MaxHealth",					INI::parsePercentToReal, nullptr, offsetof(GenericObjectCreationNugget, m_maxHealth) },
			{ "RequiresLivePlayer",	INI::parseBool, nullptr, offsetof(GenericObjectCreationNugget, m_requiresLivePlayer) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		MultiIniFieldParse p;
		p.add(getCommonFieldParse());
		p.add(myFieldParse);

		GenericObjectCreationNugget* nugget = newInstance(GenericObjectCreationNugget);
		nugget->m_nameAreObjects = true;

		ini->initFromINIMulti(nugget, p);

		((ObjectCreationList*)instance)->addObjectCreationNugget(nugget);
	}

	static void parseDebris(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "ModelNames",							parseDebrisObjectNames,							nullptr,					0 },
			{ "Mass",										INI::parsePositiveNonZeroReal,			nullptr,					offsetof( GenericObjectCreationNugget, m_mass ) },
			{ "AnimationSet",						parseAnimSet,												nullptr,					offsetof( GenericObjectCreationNugget, m_animSets) },
			{ "FXFinal",								INI::parseFXList,										nullptr,					offsetof( GenericObjectCreationNugget, m_fxFinal) },
			{ "OkToChangeModelColor",		INI::parseBool,											nullptr,					offsetof(GenericObjectCreationNugget, m_okToChangeModelColor) },
			{ "MinLODRequired",					INI::parseStaticGameLODLevel,				nullptr,					offsetof(GenericObjectCreationNugget, m_minLODRequired) },
			{ "Shadow",									INI::parseBitString32,							TheShadowNames,	offsetof( GenericObjectCreationNugget, m_shadowType ) },
			{ "BounceSound",						INI::parseAudioEventRTS,						nullptr,					offsetof( GenericObjectCreationNugget, m_bounceSound) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		MultiIniFieldParse p;
		p.add(getCommonFieldParse());
		p.add(myFieldParse);

		GenericObjectCreationNugget* nugget = newInstance(GenericObjectCreationNugget);
		nugget->m_nameAreObjects = false;

		ini->initFromINIMulti(nugget, p);

		DEBUG_ASSERTCRASH(nugget->m_mass > 0.0f, ("Zero masses are not allowed for debris!"));
		((ObjectCreationList*)instance)->addObjectCreationNugget(nugget);
	}

	static void parseAnimSet(INI *ini, void * /*instance*/, void* store, const void* /*userData*/)
	{
		AnimSet anim;
		anim.m_animInitial = ini->getNextAsciiString();
		anim.m_animFlying = ini->getNextAsciiString();
		anim.m_animFinal = ini->getNextAsciiString();
		((std::vector<AnimSet>*)store)->push_back(anim);
	}

protected:

	void doStuffToObj(
		Object* obj,
		const AsciiString& modelName,
		const Coord3D *pos,
		const Matrix3D *mtx,
		Real orientation,
		const Object *sourceObj,
		UnsignedInt lifetimeFrames
	) const
	{
		obj->setProducer(sourceObj);

		static NameKeyType key_LifetimeUpdate = NAMEKEY("LifetimeUpdate");
		LifetimeUpdate* lup = (LifetimeUpdate*)obj->findUpdateModule(key_LifetimeUpdate);
		if( lup )
		{
			if( lifetimeFrames )
			{
				//Passed in override, use this value for a specific lifetime!!!
				lup->setLifetimeRange( lifetimeFrames, lifetimeFrames );
			}
			else if( m_maxFrames > 0 )
			{
				// They will both be zero if no lifetime was specified in the OCL.  It could be in the Object so don't mess with it.
				// So the OCL listing will override the Object listing for lifetime, but ONLY if there is one.
				lup->setLifetimeRange(m_minFrames, m_maxFrames);
			}
		}

		if (!m_nameAreObjects)
		{
			for (DrawModule** dm = obj->getDrawable()->getDrawModules(); *dm; ++dm)
			{
				DebrisDrawInterface* di = (*dm)->getDebrisDrawInterface();
				if (di)
				{
					di->setModelName(modelName, m_okToChangeModelColor ? obj->getIndicatorColor() : 0, m_shadowType);
					if (!m_animSets.empty())
					{
						Int which = GameLogicRandomValue(0, m_animSets.size()-1);
						di->setAnimNames(m_animSets[which].m_animInitial, m_animSets[which].m_animFlying, m_animSets[which].m_animFinal, m_fxFinal);
					}
				}
			}
		}

		Coord3D offset = m_offset;
		if (mtx)
			adjustVector(&offset, mtx);

		Coord3D chunkPos;
		chunkPos.x = pos->x + offset.x;
		chunkPos.y = pos->y + offset.y;
		chunkPos.z = pos->z + offset.z;

		if (!m_particleSysName.isEmpty())
		{
			const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate(m_particleSysName);
			if (tmp)
			{
				ParticleSystem *sys = TheParticleSystemManager->createParticleSystem(tmp);
				sys->attachToObject(obj);
			}
		}

		if (m_ignorePrimaryObstacle)
		{
			PhysicsBehavior* p = obj->getPhysics();
			if (p)
				p->setIgnoreCollisionsWith(sourceObj);
		}

		// set its beginning health
		BodyModuleInterface *body = obj->getBodyModule();
		Real healthPercent = GameLogicRandomValueReal( m_minHealth, m_maxHealth );
		if (body)
			body->setInitialHealth(healthPercent * 100.0f);

		// If they have a SlavedUpdate, then I have to tell them who their daddy is from now on.
		for (BehaviorModule** update = obj->getBehaviorModules(); *update; ++update)
		{
			SlavedUpdateInterface* sdu = (*update)->getSlavedUpdateInterface();
			if (sdu != nullptr)
			{
				sdu->onEnslave( sourceObj );
				break;
			}
		}

		if (m_inheritsVeterancy && sourceObj && obj->getExperienceTracker()->isTrainable())
		{
			DEBUG_LOG(("Object %s inherits veterancy level %d from %s",
				obj->getTemplate()->getName().str(), sourceObj->getVeterancyLevel(), sourceObj->getTemplate()->getName().str()));
			VeterancyLevel v = sourceObj->getVeterancyLevel();

			// TheSuperHackers @bugfix Caball009 22/04/2026 Disable audiovisual cues for a veterancy level change because this object was just created.
			// Otherwise the cues would be at an incorrect position, because the object's matrix is not set yet.
			obj->getExperienceTracker()->setVeterancyLevel(v, FALSE);

			//In order to make things easier for the designers, we are going to transfer the unit name
			//to the ejected thing... so the designer can control the pilot with the scripts.
			TheScriptEngine->transferObjectName( sourceObj->getName(), obj );
		}

		if ( m_invulnerableTime > 0 )
		{
			obj->goInvulnerable( m_invulnerableTime );
		}

		if( BitIsSet( m_disposition, INHERIT_VELOCITY ) && sourceObj )
		{
			const PhysicsBehavior *sourcePhysics = sourceObj->getPhysics();
			PhysicsBehavior *objectPhysics = obj->getPhysics();
			if( sourcePhysics && objectPhysics )
			{
				objectPhysics->applyForce( sourcePhysics->getVelocity() );
			}
		}

		if( BitIsSet( m_disposition, LIKE_EXISTING ) )
		{
			if (mtx)
				obj->setTransformMatrix(mtx);
			else
				obj->setOrientation(orientation);
			obj->setPosition(&chunkPos);
			if (sourceObj && sourceObj->isAboveTerrain())
			{
				PhysicsBehavior* physics = obj->getPhysics();
				if (physics)
					physics->setAllowToFall(true);
			}

      //Lorenzen sez:
      //Since the sneak attack is a structure created with an ocl, it bypasses a lot of the
      //goodness that it would have gotten from dozerAI::build( the normal way to make structures )
      // but, since it is a building... lets stamp it down in the pathfind map, here.
      if ( obj->isKindOf( KINDOF_STRUCTURE ) )
      {
	      // Flatten the terrain underneath the object, then adjust to the flattened height. jba.
	      TheTerrainLogic->flattenTerrain(obj);
	      Coord3D adjustedPos = *obj->getPosition();
	      adjustedPos.z = TheTerrainLogic->getGroundHeight(pos->x, pos->y);
	      obj->setPosition(&adjustedPos);
	      // Note - very important that we add to map AFTER we flatten terrain. jba.
	      TheAI->pathfinder()->addObjectToPathfindMap( obj );

      }







		}

		if( BitIsSet( m_disposition, ON_GROUND_ALIGNED ) )
		{
			chunkPos.z = 99999.0f;
			PathfindLayerEnum layer = TheTerrainLogic->getHighestLayerForDestination(&chunkPos);
			obj->setOrientation(GameLogicRandomValueReal(0.0f, 2 * PI));
			chunkPos.z = TheTerrainLogic->getLayerHeight( chunkPos.x, chunkPos.y, layer );
			// ensure we are slightly above the bridge, to account for fudge & sloppy art
			if (layer != LAYER_GROUND)
				chunkPos.z += 1.0f;
			obj->setLayer(layer);
			obj->setPosition(&chunkPos);
		}

		if( BitIsSet( m_disposition, SEND_IT_OUT ) )
		{
			obj->setOrientation(GameLogicRandomValueReal(0.0f, 2 * PI));
			chunkPos.z = TheTerrainLogic->getGroundHeight( chunkPos.x, chunkPos.y );
			obj->setPosition(&chunkPos);
			PhysicsBehavior* objUp = obj->getPhysics();
			if (objUp)
			{

				if (!m_nameAreObjects)
					objUp->setMass( m_mass );

				objUp->setExtraFriction(m_extraFriction);

				Coord3D force;
				Real horizForce = 4.0f * m_dispositionIntensity;		// 2
				force.x = GameLogicRandomValueReal( -horizForce, horizForce );
				force.y = GameLogicRandomValueReal( -horizForce, horizForce );
				force.z = 0;

				objUp->applyForce(&force);
				if (m_orientInForceDirection)
					orientation = WWMath::Atan2Origin(force.y, force.x);

			}
		}

		if( BitIsSet( m_disposition, SEND_IT_FLYING | SEND_IT_UP | RANDOM_FORCE ) )
		{
			if (mtx)
			{
				DUMPMATRIX3D(mtx);
				obj->setTransformMatrix(mtx);
			}
			obj->setPosition(&chunkPos);
			DUMPCOORD3D(&chunkPos);
			PhysicsBehavior* objUp = obj->getPhysics();
			if (objUp)
			{

				if (!m_nameAreObjects)
				{
					DUMPREAL(m_mass);
					objUp->setMass( m_mass );
				}
				DEBUG_ASSERTCRASH(objUp->getMass() > 0.0f, ("Zero masses are not allowed for obj!"));

				objUp->setExtraBounciness(m_extraBounciness);
				objUp->setExtraFriction(m_extraFriction);
				objUp->setAllowBouncing(true);
				objUp->setBounceSound(&m_bounceSound);
				DUMPREAL(m_extraBounciness);
				DUMPREAL(m_extraFriction);

				// if omitted from INI, calc it based on intensity.
				Real spinRate		= m_spinRate >= 0.0f ? m_spinRate : (PI/32.0f) * m_dispositionIntensity;

				// Treat these as overrides.
				Real yawRate		= m_yawRate		>= 0.0f ? m_yawRate		: spinRate;
				Real rollRate		= m_rollRate	>= 0.0f ? m_rollRate	: spinRate;
				Real pitchRate	= m_pitchRate >= 0.0f ? m_pitchRate : spinRate;

				DUMPREAL(spinRate);
				DUMPREAL(yawRate);
				DUMPREAL(rollRate);
				DUMPREAL(pitchRate);

				Real yaw = GameLogicRandomValueReal( -yawRate, yawRate );
				Real roll = GameLogicRandomValueReal( -rollRate, rollRate );
				Real pitch = GameLogicRandomValueReal( -pitchRate, pitchRate );
				DUMPREAL(yaw);
				DUMPREAL(roll);
				DUMPREAL(pitch);

				Coord3D force;
				if( BitIsSet( m_disposition, SEND_IT_FLYING ) )
				{
					Real horizForce = 4.0f * m_dispositionIntensity;		// 2
					Real vertForce = 3.0f * m_dispositionIntensity;		// 3
					force.x = GameLogicRandomValueReal( -horizForce, horizForce );
					force.y = GameLogicRandomValueReal( -horizForce, horizForce );
					force.z = GameLogicRandomValueReal( vertForce * 0.33f, vertForce );
					DUMPREAL(horizForce);
					DUMPREAL(vertForce);
					DUMPCOORD3D(&force);
				}
				else if (BitIsSet(m_disposition, SEND_IT_UP) )
				{
					Real horizForce = 2.0f * m_dispositionIntensity;
					Real vertForce = 4.0f * m_dispositionIntensity;

					force.x = GameLogicRandomValueReal( -horizForce, horizForce );
					force.y = GameLogicRandomValueReal( -horizForce, horizForce );
					force.z = GameLogicRandomValueReal( vertForce * 0.75f, vertForce );
					DUMPREAL(horizForce);
					DUMPREAL(vertForce);
					DUMPCOORD3D(&force);
				}
				else
				{
					calcRandomForce(m_minMag, m_maxMag, m_minPitch, m_maxPitch, &force);
					DUMPREAL(m_minMag);
					DUMPREAL(m_maxMag);
					DUMPREAL(m_minPitch);
					DUMPREAL(m_maxPitch);
					DUMPCOORD3D(&force);
				}
				objUp->applyForce(&force);
				if (m_orientInForceDirection)
				{
					orientation = WWMath::Atan2Origin(force.y, force.x);
				}
				DUMPREAL(orientation);
				objUp->setAngles(orientation, 0, 0);
				objUp->setYawRate(yaw);
				objUp->setRollRate(roll);
				objUp->setPitchRate(pitch);
				DUMPCOORD3D(objUp->getAcceleration());
				DUMPCOORD3D(objUp->getVelocity());
				DUMPMATRIX3D(obj->getTransformMatrix());

			}
		}
		if( BitIsSet( m_disposition, WHIRLING ) )
		{
			PhysicsBehavior* objUp = obj->getPhysics();
			if (objUp)
			{
				Real yaw = GameLogicRandomValueReal( -m_dispositionIntensity, m_dispositionIntensity );
				Real roll = GameLogicRandomValueReal( -m_dispositionIntensity, m_dispositionIntensity );
				Real pitch = GameLogicRandomValueReal( -m_dispositionIntensity, m_dispositionIntensity );

				objUp->setYawRate(yaw);
				objUp->setRollRate(roll);
				objUp->setPitchRate(pitch);
			}
		}

		if( BitIsSet( m_disposition, FLOATING ) )
		{
			static NameKeyType key = NAMEKEY( "FloatUpdate" );
			FloatUpdate *floatUpdate = (FloatUpdate *)obj->findUpdateModule( key );

			if( floatUpdate )
				floatUpdate->setEnabled( TRUE );

		}

		if( m_containInsideSourceObject )
		{
			// The Obj has been totally made, so stuff it inside ourselves if desired.
			if( sourceObj->getContain()  &&  sourceObj->getContain()->isValidContainerFor(obj, TRUE))
			{
				sourceObj->getContain()->addToContain( obj );

				// Need to hide if they are hidden.
				if( sourceObj->getDrawable() && obj->getDrawable() && sourceObj->getDrawable()->isDrawableEffectivelyHidden() )
					obj->getDrawable()->setDrawableHidden( TRUE );
			}
			else
			{
				DEBUG_CRASH(("A OCL with ContainInsideSourceObject failed the contain and is killing the new object."));
				// If we fail to contain it, we can't just leave it.  Stillborn it.
				TheGameLogic->destroyObject(obj);
			}
		}



    if ( m_diesOnBadLand && obj )
    {
	    // if we land in the water, we die. alas.
	    const Coord3D* riderPos = obj->getPosition();
	    Real waterZ, terrainZ;
	    if (TheTerrainLogic->isUnderwater(riderPos->x, riderPos->y, &waterZ, &terrainZ)
			    && riderPos->z <= waterZ + 10.0f
			    && obj->getLayer() == LAYER_GROUND)
	    {
		    // don't call kill(); do it manually, so we can specify DEATH_FLOODED
		    DamageInfo damageInfo;
		    damageInfo.in.m_damageType = DAMAGE_WATER;	// use this instead of UNRESISTABLE so we don't get a dusty damage effect
		    damageInfo.in.m_deathType = DEATH_FLOODED;
		    damageInfo.in.m_sourceID = INVALID_ID;
		    damageInfo.in.m_amount = HUGE_DAMAGE_AMOUNT;
		    obj->attemptDamage( &damageInfo );
	    }

	    // Kill if materialized on impassable ground
	    Int cellX = REAL_TO_INT( obj->getPosition()->x / PATHFIND_CELL_SIZE );
	    Int cellY = REAL_TO_INT( obj->getPosition()->y / PATHFIND_CELL_SIZE );

	    PathfindCell* cell = TheAI->pathfinder()->getCell( obj->getLayer(), cellX, cellY );
	    PathfindCell::CellType cellType = cell ? cell->getType() : PathfindCell::CELL_IMPASSABLE;

	    // If we land outside the map, we die too.
	    // Otherwise we exist outside the PartitionManger like a cheater.
	  if( obj->isOffMap()
      || (cellType == PathfindCell::CELL_CLIFF)
      || (cellType == PathfindCell::CELL_WATER)
      || (cellType == PathfindCell::CELL_IMPASSABLE) )
	    {
		    // We are sorry, for reasons beyond our control, we are experiencing technical difficulties. Please die.
		    obj->kill();
	    }

  // Note: for future enhancement of this feature, we should test the object against the cell type he is on,
  // using obj->getAI()->hasLocomotorForSurface( __ ). We cshould not assume here that the object can not
  // find happiness on cliffs or water or whatever.


    }




	}

	Object* reallyCreate(const Coord3D *pos, const Matrix3D *mtx, Real orientation, const Object *sourceObj, UnsignedInt lifetimeFrames ) const
	{
		static const ThingTemplate* debrisTemplate = TheThingFactory->findTemplate("GenericDebris");

		if (m_names.size() <= 0)
			return nullptr;

		if (m_requiresLivePlayer && (!sourceObj || !sourceObj->getControllingPlayer() || !sourceObj->getControllingPlayer()->isPlayerActive()))
			return nullptr; // don't spawn useful objects for dead players.  Avoid the zombie units from Yuri's.

		// Object type debris might need this information to process visual UpgradeModules.
		Team *debrisOwner = ThePlayerList->getNeutralPlayer() ? ThePlayerList->getNeutralPlayer()->getDefaultTeam() : nullptr;

		if( sourceObj && sourceObj->getControllingPlayer() )
			debrisOwner = sourceObj->getControllingPlayer()->getDefaultTeam();

		Object* container = nullptr;
		Object *firstObject = nullptr;
		if (!m_putInContainer.isEmpty())
		{
			const ThingTemplate* containerTmpl = TheThingFactory->findTemplate(m_putInContainer);
			if (containerTmpl)
			{
				container = TheThingFactory->newObject( containerTmpl, debrisOwner );
				if( !container )
				{
					DEBUG_CRASH( ("OCL::reallyCreate() failed to create container %s.", m_putInContainer.str() ) );
					return firstObject;
				}
				firstObject = container;
				container->setProducer(sourceObj);
			}
		}


		for (Int nn = 0; nn < m_debrisToGenerate; nn++)
		{
			Int pick = GameLogicRandomValue(0, m_names.size() - 1);

			const ThingTemplate* tmpl;
			if (m_nameAreObjects)
				tmpl = TheThingFactory->findTemplate(m_names[pick]);
			else
			{	//this is using the generic debris type so it's probably safe to
				//remove if requested by the GameLOD manager.
				if (TheGameLODManager->isDebrisSkipped())
					continue;

				tmpl = debrisTemplate;
			}
			DEBUG_ASSERTCRASH(tmpl, ("Object %s not found",m_names[pick].str()));
			if (!tmpl)
				continue;

			Object *debris = TheThingFactory->newObject( tmpl, debrisOwner );
			if( !debris )
			{
				DEBUG_CRASH( ("OCL::reallyCreate() failed to create debris %s.", tmpl->getName().str() ) );
				return firstObject;
			}
			if( !firstObject )
			{
				firstObject = debris;
			}
			debris->setProducer(sourceObj);
			if (m_preserveLayer && sourceObj != nullptr && container == nullptr)
			{
				PathfindLayerEnum layer = sourceObj->getLayer();
				if (layer != LAYER_GROUND)
					debris->setLayer(layer);
			}

			if (container != nullptr && container->getContain() != nullptr && container->getContain()->isValidContainerFor(debris, true))
				container->getContain()->addToContain(debris);

			// if we want the objects being created to appear in a spread formation
			// PLEASE NOTE --> if/when the object placement logic is modified so that
			// objects that are placed in the same location are no longer placed in a
			// diagonal line but rather in random locations nearby, this logic will no
			// longer be necessary and can be taken out -- amit
			if (m_spreadFormation)
			{
				FindPositionOptions fpOptions;
				fpOptions.minRadius = GameLogicRandomValueReal(m_minDistanceAFormation, m_minDistanceBFormation);
				fpOptions.maxRadius = m_maxDistanceFormation;
				fpOptions.flags = FPF_USE_HIGHEST_LAYER;

				// TheSuperHackers @bugfix Caball009 12/01/2026 Position variable needs to be initialized before use.
				// The non-deterministic behavior for retail clients cannot be fixed, so this will remain a source of potential mismatches for unpatched clients.
				// Fall back to the center position if no valid position was found, so that the behavior is deterministic for patched clients.
				Coord3D resultPos = *pos;

				if (!ThePartitionManager->findPositionAround(pos, &fpOptions, &resultPos))
				{
					DEBUG_ASSERTCRASH(resultPos == *pos, ("Position should not have been changed"));

#if RETAIL_COMPATIBLE_CRC
					DEBUG_CRASH(("A mismatch is likely to happen if this code path is used in a match with unpatched clients."));
#endif
				}
				doStuffToObj( debris, m_names[pick], &resultPos, mtx, orientation, sourceObj, lifetimeFrames );
			}
			else
			{
				// do stuff to contained objects too
				doStuffToObj( debris, m_names[pick], pos, mtx, orientation, sourceObj, lifetimeFrames );
			}

			if (m_fadeIn)
			{
				AudioEventRTS fadeAudioEvent(m_fadeSoundName);
				fadeAudioEvent.setObjectID(sourceObj->getID());
				TheAudio->addAudioEvent(&fadeAudioEvent);
				debris->getDrawable()->fadeIn(m_fadeFrames);
			}

			if (m_fadeOut)
			{
				AudioEventRTS fadeAudioEvent(m_fadeSoundName);
				fadeAudioEvent.setObjectID(sourceObj->getID());
				TheAudio->addAudioEvent(&fadeAudioEvent);
				debris->getDrawable()->fadeOut(m_fadeFrames);
			}
		}

#if !RETAIL_COMPATIBLE_CRC && !PRESERVE_NO_XP_FROM_OCL_KILLS
		ObjectID sinkID = sourceObj->getExperienceTracker()->getExperienceSink();
		firstObject->getExperienceTracker()->setExperienceSink(sinkID != INVALID_ID ? sinkID : sourceObj->getID());
#endif

		if (container)
			doStuffToObj( container, AsciiString::TheEmptyString, pos, mtx, orientation, sourceObj, lifetimeFrames );

		return firstObject;
	}

	static void parseDebrisObjectNames( INI* ini, void *instance, void *store, const void* /*userData*/ )
	{
		GenericObjectCreationNugget* debrisNugget = (GenericObjectCreationNugget*)instance;
		for (const char* token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
		{
			if (TheGlobalData->m_preloadAssets)
				debrisModelNamesGlobalHack.push_back(token);
			debrisNugget->m_names.push_back(AsciiString(token));

#if RETAIL_COMPATIBLE_CRC
			token = ini->getNextTokenOrNull();
#endif
		}
	}

private:
	struct AnimSet
	{
		AsciiString								m_animInitial;
		AsciiString								m_animFlying;
		AsciiString								m_animFinal;
	};
	std::vector<AsciiString>	m_names;
	AsciiString								m_putInContainer;
	std::vector<AnimSet>			m_animSets;
	const FXList*							m_fxFinal;
	AsciiString								m_particleSysName;
	Int												m_debrisToGenerate;
	Real											m_mass;
	Real											m_extraBounciness;
	Real											m_extraFriction;
	Coord3D										m_offset;
	DebrisDisposition					m_disposition;
	Real											m_dispositionIntensity;
	Real											m_spinRate;
	Real											m_yawRate;
	Real											m_rollRate;
	Real											m_pitchRate;
	Real											m_minMag, m_maxMag;
	Real											m_minPitch, m_maxPitch;
	UnsignedInt								m_minFrames, m_maxFrames;
	ShadowType								m_shadowType;
	StaticGameLODLevel				m_minLODRequired;
	UnsignedInt								m_invulnerableTime;
	Real											m_minHealth;
	Real											m_maxHealth;
	UnsignedInt								m_fadeFrames;
	AsciiString								m_fadeSoundName;
	Real											m_minDistanceAFormation;
	Real											m_minDistanceBFormation;
	Real											m_maxDistanceFormation;
	Int												m_objectCount; // how many objects will there be?
	AudioEventRTS							m_bounceSound;
	Bool											m_requiresLivePlayer;
	Bool											m_containInsideSourceObject; ///< The created stuff will be added to the Contain module of the SourceObject
	Bool											m_preserveLayer;
	Bool											m_nameAreObjects;
	Bool											m_okToChangeModelColor;
	Bool											m_orientInForceDirection;
	Bool											m_spreadFormation;
	Bool											m_fadeIn;
	Bool											m_fadeOut;
	Bool											m_ignorePrimaryObstacle;
	Bool											m_inheritsVeterancy;
  Bool                      m_diesOnBadLand;
	Bool											m_skipIfSignificantlyAirborne;

};
EMPTY_DTOR(GenericObjectCreationNugget)

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
static const FieldParse TheObjectCreationListFieldParse[] =
{
	{ "CreateObject",			GenericObjectCreationNugget::parseObject, nullptr, 0},
	{ "CreateDebris",			GenericObjectCreationNugget::parseDebris, nullptr, 0},
	{ "ApplyRandomForce",	ApplyRandomForceNugget::parse, nullptr, 0},
	{ "DeliverPayload",		DeliverPayloadNugget::parse, nullptr, 0},
	{ "FireWeapon",				FireWeaponNugget::parse, nullptr, 0},
	{ "Attack",						AttackNugget::parse, nullptr, 0},
	{ nullptr, nullptr, nullptr, 0 }
};

//-------------------------------------------------------------------------------------------------
void ObjectCreationList::clear()
{
	// do NOT delete the nuggets -- they're owned by the Store.
	m_nuggets.clear();
}

//-------------------------------------------------------------------------------------------------
void ObjectCreationList::addObjectCreationNugget(ObjectCreationNugget* nugget)
{
	m_nuggets.push_back(nugget);
	TheObjectCreationListStore->addObjectCreationNugget(nugget);
}

//-------------------------------------------------------------------------------------------------
Object* ObjectCreationList::createInternal( const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Bool createOwner, UnsignedInt lifetimeFrames ) const
{
	DEBUG_ASSERTCRASH(primaryObj != nullptr, ("You should always call OCLs with a non-null primary Obj, even for positional calls, to get team ownership right"));
	Object *theFirstObject = nullptr;
	for (ObjectCreationNuggetVector::const_iterator i = m_nuggets.begin(); i != m_nuggets.end(); ++i)
	{
		Object *curObj = (*i)->create( primaryObj, primary, secondary, createOwner, lifetimeFrames );
		if (theFirstObject==nullptr) {
			theFirstObject = curObj;
		}
	}
	return theFirstObject;
}

//-------------------------------------------------------------------------------------------------
Object* ObjectCreationList::createInternal( const Object* primaryObj, const Coord3D *primary, const Coord3D* secondary, Real angle, UnsignedInt lifetimeFrames ) const
{
	DEBUG_ASSERTCRASH(primaryObj != nullptr, ("You should always call OCLs with a non-null primary Obj, even for positional calls, to get team ownership right"));
	Object *theFirstObject = nullptr;
	for (ObjectCreationNuggetVector::const_iterator i = m_nuggets.begin(); i != m_nuggets.end(); ++i)
	{
		Object *curObj =  (*i)->create( primaryObj, primary, secondary, angle, lifetimeFrames );
		if (theFirstObject==nullptr) {
			theFirstObject = curObj;
		}
	}
	return theFirstObject;
}

//-------------------------------------------------------------------------------------------------
Object* ObjectCreationList::createInternal( const Object* primary, const Object* secondary, UnsignedInt lifetimeFrames ) const
{
	DEBUG_ASSERTCRASH(primary != nullptr, ("You should always call OCLs with a non-null primary Obj, even for positional calls, to get team ownership right"));
	Object *theFirstObject = nullptr;
	for (ObjectCreationNuggetVector::const_iterator i = m_nuggets.begin(); i != m_nuggets.end(); ++i)
	{
		Object *curObj =  (*i)->create( primary, secondary, lifetimeFrames );
		if (theFirstObject==nullptr) {
			theFirstObject = curObj;
		}
	}
	return theFirstObject;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
ObjectCreationListStore::ObjectCreationListStore()
{
}

//-------------------------------------------------------------------------------------------------
ObjectCreationListStore::~ObjectCreationListStore()
{
	for (ObjectCreationNuggetVector::iterator i = m_nuggets.begin(); i != m_nuggets.end(); ++i)
	{
		deleteInstance(*i);
	}
	m_nuggets.clear();
}

//-------------------------------------------------------------------------------------------------
const ObjectCreationList *ObjectCreationListStore::findObjectCreationList(const char* name) const
{
	if (stricmp(name, "None") == 0)
		return nullptr;

  ObjectCreationListMap::const_iterator it = m_ocls.find(NAMEKEY(name));
  if (it == m_ocls.end())
	{
		return nullptr;
	}
	else
	{
		return &(*it).second;
	}
}

//-------------------------------------------------------------------------------------------------
void ObjectCreationListStore::addObjectCreationNugget(ObjectCreationNugget* nugget)
{
	m_nuggets.push_back(nugget);
}

//-------------------------------------------------------------------------------------------------
/*static */ void ObjectCreationListStore::parseObjectCreationListDefinition(INI *ini)
{
	// read the ObjectCreationList name
	const char *c = ini->getNextToken();
	NameKeyType key = TheNameKeyGenerator->nameToKey(c);
	ObjectCreationList& ocl = TheObjectCreationListStore->m_ocls[key];
	ocl.clear();
	ini->initFromINI(&ocl, TheObjectCreationListFieldParse);
}

//-------------------------------------------------------------------------------------------------
/*static*/ void INI::parseObjectCreationListDefinition(INI *ini)
{
	ObjectCreationListStore::parseObjectCreationListDefinition(ini);
}
