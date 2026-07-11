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

// FILE: ParticleUplinkCannonUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, September 2002
// Desc:   Update module to handle building states and weapon firing of the particle uplink cannon.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_DEATH_NAMES

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameUtility.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Xfer.h"
#include "Common/ClientUpdateModule.h"

#include "GameClient/ControlBar.h"
#include "GameClient/GameClient.h"
#include "GameClient/Drawable.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/FXList.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/ParticleUplinkCannonUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/ActiveBody.h"

// TheSuperHackers @fix Mirelle 04/02/2026: Raised from 500.0f so that
// enormous camera heights cannot see above the laser origin.
constexpr const Real ORBITAL_BEAM_Z_OFFSET = 3500.0f;
// TheSuperHackers @fix The positional audio is now decoupled from the beam origin.
// 500 units represent the height of the original audio emitter.
constexpr const Real ORBITAL_BEAM_AUDIO_Z_OFFSET = 500.0f;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ParticleUplinkCannonUpdateModuleData::ParticleUplinkCannonUpdateModuleData()
{
	m_specialPowerTemplate					= nullptr;
	m_beginChargeFrames							= 0;
	m_raiseAntennaFrames						= 0;
	m_readyDelayFrames							= 0;
	m_outerEffectNumBones						= 0;
	m_beamTravelFrames							= 0;
	m_widthGrowFrames								= 0;
	m_totalFiringFrames							= 0;
	m_totalScorchMarks							= 0;
	m_scorchMarkScalar							= 1.0f;
	m_damageRadiusScalar						= 1.0f;
	m_groundHitFX										= nullptr;
	m_beamLaunchFX									= nullptr;
	m_framesBetweenLaunchFXRefresh  = 30;
	m_totalDamagePulses							= 0;
	m_damagePerSecond								= 0.0f;
	m_damageType										= DAMAGE_LASER;
	m_deathType											= DEATH_LASERED;
	m_revealRange										= 0.0f;
  m_manualDrivingSpeed						= 0.0f;
  m_manualFastDrivingSpeed				= 0.0f;
  m_doubleClickToFastDriveDelay		= 500;
	m_swathOfDeathAmplitude					= 0.0f;
	m_swathOfDeathDistance					=	0.0f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void ParticleUplinkCannonUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "SpecialPowerTemplate",									INI::parseSpecialPowerTemplate,	nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_specialPowerTemplate ) },
    { "BeginChargeTime",											INI::parseDurationUnsignedInt,	nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_beginChargeFrames ) },
    { "RaiseAntennaTime",											INI::parseDurationUnsignedInt,	nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_raiseAntennaFrames ) },
		{ "ReadyDelayTime",												INI::parseDurationUnsignedInt,  nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_readyDelayFrames ) },
		{ "WidthGrowTime",												INI::parseDurationUnsignedInt,	nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_widthGrowFrames ) },
		{ "BeamTravelTime",												INI::parseDurationUnsignedInt,	nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_beamTravelFrames ) },
		{ "TotalFiringTime",											INI::parseDurationUnsignedInt,  nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_totalFiringFrames ) },
		{ "RevealRange",													INI::parseReal,									nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_revealRange ) },

		{ "OuterEffectBoneName",									INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_outerEffectBaseBoneName ) },
		{ "OuterEffectNumBones",									INI::parseUnsignedInt,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_outerEffectNumBones ) },
		{ "OuterNodesLightFlareParticleSystem",		INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_outerNodesLightFlareParticleSystemName ) },
		{ "OuterNodesMediumFlareParticleSystem",	INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_outerNodesMediumFlareParticleSystemName ) },
		{ "OuterNodesIntenseFlareParticleSystem",	INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_outerNodesIntenseFlareParticleSystemName ) },

		{ "ConnectorBoneName",										INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_connectorBoneName ) },
		{ "ConnectorMediumLaserName",							INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_connectorMediumLaserNameName ) },
		{ "ConnectorIntenseLaserName",						INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_connectorIntenseLaserNameName ) },
		{ "ConnectorMediumFlare",									INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_connectorMediumFlareParticleSystemName ) },
		{ "ConnectorIntenseFlare",								INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_connectorIntenseFlareParticleSystemName ) },

		{ "FireBoneName",													   INI::parseAsciiString,				nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_fireBoneName ) },
		{ "LaserBaseLightFlareParticleSystemName",   INI::parseAsciiString,				nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_laserBaseLightFlareParticleSystemName ) },
		{ "LaserBaseMediumFlareParticleSystemName",	 INI::parseAsciiString,				nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_laserBaseMediumFlareParticleSystemName ) },
		{ "LaserBaseIntenseFlareParticleSystemName", INI::parseAsciiString,				nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_laserBaseIntenseFlareParticleSystemName ) },

		{ "ParticleBeamLaserName",								INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_particleBeamLaserName ) },

		{ "SwathOfDeathDistance",									INI::parseReal,									nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_swathOfDeathDistance ) },
		{ "SwathOfDeathAmplitude",								INI::parseReal,									nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_swathOfDeathAmplitude ) },
		{ "TotalScorchMarks",											INI::parseUnsignedInt,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_totalScorchMarks ) },
		{ "ScorchMarkScalar",											INI::parseReal,									nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_scorchMarkScalar ) },
		{ "BeamLaunchFX",													INI::parseFXList,								nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_beamLaunchFX ) },
		{ "DelayBetweenLaunchFX",									INI::parseDurationUnsignedInt,  nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_framesBetweenLaunchFXRefresh ) },
		{ "GroundHitFX",													INI::parseFXList,								nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_groundHitFX ) },

		{ "DamagePerSecond",											INI::parseReal,									nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_damagePerSecond ) },
		{ "TotalDamagePulses",										INI::parseUnsignedInt,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_totalDamagePulses ) },
		{ "DamageType",														DamageTypeFlags::parseSingleBitFromINI,	nullptr,	offsetof( ParticleUplinkCannonUpdateModuleData, m_damageType ) },
		{ "DeathType",														INI::parseIndexList,						TheDeathNames,	offsetof( ParticleUplinkCannonUpdateModuleData, m_deathType ) },
		{ "DamageRadiusScalar",										INI::parseReal,									nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_damageRadiusScalar ) },

		{ "PoweringUpSoundLoop",									INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_powerupSoundName ) },
		{ "UnpackToIdleSoundLoop",								INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_unpackToReadySoundName ) },
		{ "FiringToPackSoundLoop",								INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_firingToIdleSoundName ) },
		{ "GroundAnnihilationSoundLoop",					INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_annihilationSoundName ) },
		{ "DamagePulseRemnantObjectName",					INI::parseAsciiString,					nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_damagePulseRemnantObjectName ) },

    { "ManualDrivingSpeed",										INI::parseReal,									nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_manualDrivingSpeed ) },
    { "ManualFastDrivingSpeed",								INI::parseReal,									nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_manualFastDrivingSpeed ) },
    { "DoubleClickToFastDriveDelay",					INI::parseDurationUnsignedInt,	nullptr, offsetof( ParticleUplinkCannonUpdateModuleData, m_doubleClickToFastDriveDelay ) },

		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
ParticleUplinkCannonUpdate::ParticleUplinkCannonUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{

	m_status = STATUS_IDLE;
	m_laserStatus = LASERSTATUS_NONE;
	m_frames = 0;
	m_specialPowerModule = nullptr;
	m_groundToOrbitBeamID = INVALID_DRAWABLE_ID;
	m_orbitToTargetBeamID = INVALID_DRAWABLE_ID;
	m_connectorSystemID = INVALID_PARTICLE_SYSTEM_ID;
	m_laserBaseSystemID = INVALID_PARTICLE_SYSTEM_ID;
	m_connectorNodePosition.zero();
	m_laserOriginPosition.zero();
	m_upBonesCached = false;
	m_defaultInfoCached = false;
	m_invalidSettings = false;
	m_manualTargetMode = false;
	m_xferVersion = 1;
	m_initialTargetPosition.zero();
	m_currentTargetPosition.zero();
	m_overrideTargetDestination.zero();
	m_scorchMarksMade= 0;
	m_nextScorchMarkFrame = 0;
	m_nextLaunchFXFrame = 0;
	m_damagePulsesMade = 0;
	m_nextDamagePulseFrame = 0;
	m_startAttackFrame = 0;
	m_startDecayFrame = 0;
	m_lastDrivingClickFrame = 0;
	m_2ndLastDrivingClickFrame = 0;
	m_clientShroudedLastFrame = FALSE;

	for( Int i = 0; i < MAX_OUTER_NODES; i++ )
	{
		m_outerSystemIDs[ i ] =	INVALID_PARTICLE_SYSTEM_ID;
		m_laserBeamIDs[ i ] = INVALID_DRAWABLE_ID;
		//Initializing (even though they will get cached properly).
		m_outerNodePositions[ i ].zero();
		m_outerNodeOrientations[ i ].Make_Identity();
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::killEverything()
{
	removeAllEffects();

	//This laser is independent from the other effects and needs to be specially handled.
	if( m_orbitToTargetBeamID != INVALID_DRAWABLE_ID )
	{
		Drawable *beam = TheGameClient->findDrawableByID( m_orbitToTargetBeamID );
		if( beam )
		{
			TheGameClient->destroyDrawable( beam );
		}
		m_orbitToTargetBeamID = INVALID_DRAWABLE_ID;
	}
	m_orbitToTargetLaserRadius = LaserRadiusUpdate();

	TheAudio->removeAudioEvent( m_powerupSound.getPlayingHandle() );
	TheAudio->removeAudioEvent( m_unpackToReadySound.getPlayingHandle() );
	TheAudio->removeAudioEvent( m_firingToIdleSound.getPlayingHandle() );
	TheAudio->removeAudioEvent( m_annihilationSound.getPlayingHandle() );

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ParticleUplinkCannonUpdate::~ParticleUplinkCannonUpdate()
{
	killEverything();
}


//-------------------------------------------------------------------------------------------------
// Validate that we have the necessary data from the ini file.
//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::onObjectCreated()
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();
	Object *obj = getObject();

	if( !data->m_specialPowerTemplate )
	{
		DEBUG_CRASH( ("%s object's ParticleUplinkCannonUpdate lacks access to the SpecialPowerTemplate. Needs to be specified in ini.", obj->getTemplate()->getName().str() ) );
		m_invalidSettings = true;
		return;
	}

	m_specialPowerModule = obj->getSpecialPowerModule( data->m_specialPowerTemplate );
	m_connectorNodePosition.set( *obj->getPosition() );
	m_laserOriginPosition.set( *obj->getPosition() );

	//Create instances of the sounds required.
	m_powerupSound.setEventName( data->m_powerupSoundName );
	m_unpackToReadySound.setEventName( data->m_unpackToReadySoundName );
	m_firingToIdleSound.setEventName( data->m_firingToIdleSoundName );
	m_annihilationSound.setEventName( data->m_annihilationSoundName );
	TheAudio->getInfoForAudioEvent( &m_powerupSound );
	TheAudio->getInfoForAudioEvent( &m_unpackToReadySound );
	TheAudio->getInfoForAudioEvent( &m_firingToIdleSound );
	TheAudio->getInfoForAudioEvent( &m_annihilationSound );
}

//-------------------------------------------------------------------------------------------------
Bool ParticleUplinkCannonUpdate::initiateIntentToDoSpecialPower(const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();

	if( m_specialPowerModule->getSpecialPowerTemplate() != specialPowerTemplate )
	{
		//Check to make sure our modules are connected.
		return FALSE;
	}

	if( !BitIsSet( commandOptions, COMMAND_FIRED_BY_SCRIPT ) )
	{
		DEBUG_ASSERTCRASH(targetPos, ("Particle Cannon target data must not be null"));

		//All human players have manual control and must "drive" the beam around!
		m_startAttackFrame = TheGameLogic->getFrame();
		m_laserStatus = LASERSTATUS_NONE;
		m_manualTargetMode = true;
		m_initialTargetPosition.set( *targetPos );
		m_overrideTargetDestination.set( *targetPos );
		m_currentTargetPosition.set( *targetPos );
	}
	else
	{
		DEBUG_ASSERTCRASH(targetPos, ("Particle Cannon target data must not be null"));

		//All computer controlled players have automatic control -- the "S" curve.
		UnsignedInt now = TheGameLogic->getFrame();
		m_initialTargetPosition.set( *targetPos );
		m_startAttackFrame = max( now, (UnsignedInt)1 );
		m_laserStatus = LASERSTATUS_NONE;
		setLogicalStatus( STATUS_READY_TO_FIRE );
		m_specialPowerModule->setReadyFrame( now );
	}

	m_startDecayFrame = m_startAttackFrame + data->m_totalFiringFrames;

	SpecialPowerModuleInterface *spmInterface = getObject()->getSpecialPowerModule( specialPowerTemplate );
	if( spmInterface )
	{
		SpecialPowerModule *spModule = (SpecialPowerModule*)spmInterface;
		spModule->markSpecialPowerTriggered( &m_initialTargetPosition );
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool ParticleUplinkCannonUpdate::isPowerCurrentlyInUse( const CommandButton *command ) const
{
	if( m_startAttackFrame != 0 && m_startAttackFrame <= TheGameLogic->getFrame() )
	{
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::setSpecialPowerOverridableDestination( const Coord3D *loc )
{
	if( !getObject()->isDisabled() )
	{
		m_overrideTargetDestination = *loc;
		m_manualTargetMode = true;
		m_2ndLastDrivingClickFrame = m_lastDrivingClickFrame;
		m_lastDrivingClickFrame = TheGameLogic->getFrame();
	}
}


//-------------------------------------------------------------------------------------------------
/** The update callback. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime ParticleUplinkCannonUpdate::update()
{
	if( m_invalidSettings )
	{
		// can't return UPDATE_SLEEP_FOREVER unless we are sleepy...
		return UPDATE_SLEEP_NONE;
		///return UPDATE_SLEEP_FOREVER;
	}

	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();

	Object *me = getObject();

	// must test SOLD *before* the others.
	if( me->testStatus(OBJECT_STATUS_SOLD))
	{
		if (m_status != STATUS_IDLE)
		{
			setLogicalStatus( STATUS_IDLE );
			killEverything();
		}
		return UPDATE_SLEEP_NONE;
	}

	if( me->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) )
	{
		return UPDATE_SLEEP_NONE;
	}

	if( me->isEffectivelyDead() )
	{
		return UPDATE_SLEEP_NONE;
	}

	//Check to see what our status is -- there are a couple:
	//1) Idle while waiting to get near the time when we should be preparing to be ready.
	//2) If superweapon delay is nuked (cheat) then we need to make the building ready now.
	UnsignedInt now = TheGameLogic->getFrame();
	UnsignedInt readyToFireFrame = m_specialPowerModule->isReady() ? now : m_specialPowerModule->getReadyFrame();
	UnsignedInt almostReadyFrame = readyToFireFrame - data->m_readyDelayFrames;
	UnsignedInt raiseAntennaFrame = almostReadyFrame - data->m_raiseAntennaFrames;
	UnsignedInt beginChargeFrame = raiseAntennaFrame - data->m_beginChargeFrames;

	if( m_startAttackFrame != 0 && m_startAttackFrame <= now )
	{
		if( m_startDecayFrame > now )
		{
			if( me->isDisabledByType( DISABLED_UNDERPOWERED ) ||
					me->isDisabledByType( DISABLED_EMP ) ||
					me->isDisabledByType( DISABLED_HACKED ) )
			{
				//We must end the special power early! ABORT! ABORT!
				m_startDecayFrame = now;
			}
		}
		UnsignedInt endDecayFrame			= m_startDecayFrame + data->m_widthGrowFrames;
		UnsignedInt orbitalBirthFrame = m_startAttackFrame + data->m_beamTravelFrames;
		UnsignedInt orbitalDecayStart = m_startDecayFrame + data->m_beamTravelFrames;
		UnsignedInt orbitalDeathFrame = orbitalDecayStart + data->m_widthGrowFrames;
		switch( m_laserStatus )
		{
			case LASERSTATUS_NONE:
				//Means we are eligible to fire!
				if( orbitalBirthFrame <= now )
				{
					createOrbitToTargetLaser( data->m_widthGrowFrames );
					m_laserStatus = LASERSTATUS_BORN;
					m_scorchMarksMade		= 0;
					m_nextScorchMarkFrame = now;
					m_damagePulsesMade = 0;
					m_nextDamagePulseFrame = now;
				}
				break;
			case LASERSTATUS_BORN:
			{
				if( orbitalDecayStart <= now )
				{
					Drawable *beam = TheGameClient->findDrawableByID( m_orbitToTargetBeamID );
					if( beam )
					{
						//m_annihilationSound.setPosition( beam->getPosition() );
						static NameKeyType nameKeyClientUpdate = NAMEKEY( "LaserUpdate" );
						LaserUpdate *update = (LaserUpdate*)beam->findClientUpdateModule( nameKeyClientUpdate );
						if( update )
						{
							update->setDecayFrames( data->m_widthGrowFrames );
						}
					}
					m_orbitToTargetLaserRadius.setDecayFrames( data->m_widthGrowFrames );
					m_laserStatus = LASERSTATUS_DECAYING;
				}
				break;
			}
			case LASERSTATUS_DECAYING:
			{
				if( orbitalDeathFrame <= now )
				{
					TheAudio->removeAudioEvent( m_annihilationSound.getPlayingHandle() );
					if ( m_orbitToTargetBeamID != INVALID_DRAWABLE_ID )
					{
						Drawable *beam = TheGameClient->findDrawableByID( m_orbitToTargetBeamID );
						if( beam )
						{
							//m_annihilationSound.setPosition( beam->getPosition() );
							TheGameClient->destroyDrawable( beam );
						}
						m_orbitToTargetBeamID = INVALID_DRAWABLE_ID;
					}
					m_orbitToTargetLaserRadius = LaserRadiusUpdate();
					m_laserStatus = LASERSTATUS_DEAD;
					m_startAttackFrame = 0;
					setLogicalStatus( STATUS_IDLE );
				}
				break;
			}
			case LASERSTATUS_DEAD:
				//Nothing... this is just a wait state.
				break;
		}

#if RETAIL_COMPATIBLE_CRC
		// TheSuperHackers @info helmutbuhler 12/06/2025
		// Note that this code is very brittle for retail compatibility. Inlining isFiring
		// can cause incompatibility in some circumstances.
#endif
		const Bool isFiring = orbitalBirthFrame <= now && now < orbitalDeathFrame;
		if ( isFiring )
		{

			if( !m_manualTargetMode )
			{
				//Calculate the position of the beam because it swaths -- a nice S curve centering at the target location!

				//First determine the factor of time completed (ranges between 0.0 and 1.0)
				Real factor = (Real)(now - orbitalBirthFrame) / (Real)(orbitalDeathFrame - orbitalBirthFrame);

				//We're generating a swath that travels the points between WWMath::SinTrig( -1PI ) and WWMath::SinTrig( 1PI )
				Real radians = (factor * TWO_PI) - PI;
				Real cxDistance = (factor * data->m_swathOfDeathDistance ) - (data->m_swathOfDeathDistance * 0.5f); //cx is cartesian x

				//Now calculate the amplitude value.
				Real height = WWMath::SinTrig( radians );
				Real cxHeight = height * data->m_swathOfDeathAmplitude;

				Coord3D buildingToInitialTargetVector;
				buildingToInitialTargetVector.set( m_initialTargetPosition );
				buildingToInitialTargetVector.sub( *me->getPosition() );
				Real targetDistance = buildingToInitialTargetVector.length();

				//Calculate the point position assuming the target position is on the x axis relative to the building.
				m_currentTargetPosition.x = cxDistance + targetDistance;
				m_currentTargetPosition.y = cxHeight;
				m_currentTargetPosition.z = 0.0f;
				targetDistance = m_currentTargetPosition.length();

				//Now that we have our cartesian offset relative to the target coordinate, we need to rotate that offset
				//so it's aligned to along the building -> target vector.
				Vector2 buildingToTargetVector( m_initialTargetPosition.x - me->getPosition()->x, m_initialTargetPosition.y - me->getPosition()->y );
				buildingToTargetVector.Normalize();
				Vector2 cartesianTargetVector( m_currentTargetPosition.x, m_currentTargetPosition.y );
				cartesianTargetVector.Normalize();

				Real dotProduct = Vector2::Dot_Product( buildingToTargetVector, cartesianTargetVector );
				dotProduct = __min( 0.99999f, __max( -0.99999f, dotProduct ) ); //Account for numerical errors.  Also, WWMath::ACosTrig(-1.00000) is coming out QNAN on the superweapon general map.  Heh.

				Real angle = (Real)ACos( dotProduct );

				if( buildingToTargetVector.Y >= 0 )
				{
					angle = -angle;
				}

				Matrix3D mtx( Vector3( 1.0f, 0.0f, 0.0f ) );
				mtx.Scale( targetDistance );
				mtx.Rotate_Z( -angle );

				Vector3 v = mtx.Get_X_Vector();

				m_currentTargetPosition.x = me->getPosition()->x + v.X;
				m_currentTargetPosition.y = me->getPosition()->y + v.Y;
			}
			else
			{
				Real speed = data->m_manualDrivingSpeed;
				if( m_lastDrivingClickFrame - m_2ndLastDrivingClickFrame < data->m_doubleClickToFastDriveDelay )
				{
					//Because we double clicked, use the faster driving speed.
					speed = data->m_manualFastDrivingSpeed;
				}

				//Convert speed to speed per frame.
				speed /= LOGICFRAMES_PER_SECOND;

				//Calculate the distance from our current position to our target dest.
				Coord3D vector = m_overrideTargetDestination;
				vector.sub( m_currentTargetPosition );
				Real distance = vector.length();
				if( distance < speed )
				{
					//Don't allow the speed to overshoot the target point if close.
					speed = distance;
				}

				//Unitize the vector then apply the distance we will move.
				vector.normalize();
				vector.scale( speed );

				//Move the current target position in the desired direction and speed.
				m_currentTargetPosition.x += vector.x;
				m_currentTargetPosition.y += vector.y;
			}

			//Regardless of which method we used to set the target position, make sure the z position is at the terrain.
			m_currentTargetPosition.z = TheTerrainLogic->getGroundHeight( m_currentTargetPosition.x, m_currentTargetPosition.y );

			Coord3D orbitPosition;
			orbitPosition.set( m_currentTargetPosition );
			orbitPosition.z += ORBITAL_BEAM_Z_OFFSET;

			Real scorchRadius = 0.0f;
			Real damageRadius = 0.0f;
			Real templateLaserRadius = 13.0f;
			Real visualLaserRadius = 0.0f;

			//Reset the laser position
			Drawable *beam = TheGameClient->findDrawableByID( m_orbitToTargetBeamID );
			if ( beam )
			{
				static NameKeyType nameKeyClientUpdate = NAMEKEY( "LaserUpdate" );
				LaserUpdate *update = (LaserUpdate*)beam->findClientUpdateModule( nameKeyClientUpdate );
				if( update )
				{
					update->initLaser( nullptr, &orbitPosition, &m_currentTargetPosition );
					// TheSuperHackers @logic-client-separation The GameLogic has a dependency on this drawable.
					// The logical laser radius for the damage should probably be part of ParticleUplinkCannonUpdateModuleData.
					templateLaserRadius = update->getTemplateLaserRadius();
					visualLaserRadius = update->getCurrentLaserRadius();
				}
				Coord3D audioPos = m_currentTargetPosition;
				audioPos.z += ORBITAL_BEAM_AUDIO_Z_OFFSET;
				beam->setPosition( &audioPos );
			}
			// TheSuperHackers @refactor helmutbuhler/xezon 17/05/2025
			// Originally the damageRadius was calculated with a value updated by LaserUpdate::clientUpdate.
			// To no longer rely on GameClient updates, this class now maintains a copy of the LaserRadiusUpdate.
			m_orbitToTargetLaserRadius.updateRadius();
			const Real logicalLaserRadius = templateLaserRadius * m_orbitToTargetLaserRadius.getWidthScale();
			damageRadius = logicalLaserRadius * data->m_damageRadiusScalar;
			scorchRadius = logicalLaserRadius * data->m_scorchMarkScalar;
#if defined(RETAIL_COMPATIBLE_CRC)
			DEBUG_ASSERTCRASH(logicalLaserRadius == visualLaserRadius,
				("ParticleUplinkCannonUpdate's laser radius does not match LaserUpdate's laser radius - will cause mismatch in VS6 retail compatible builds"));
#endif

			//Create scorch marks periodically
			if( m_nextScorchMarkFrame <= now )
			{
				m_scorchMarksMade++;

				//Create the scorch mark now!
				Scorches scorchID = (Scorches)GameClientRandomValue( SCORCH_1, SCORCH_4 ); //Yes, this is just client fluff!
				TheGameClient->addScorch( &m_currentTargetPosition, scorchRadius, scorchID );

				//Calculate next scorch mark frame.
				Real nextFactor = (Real)m_scorchMarksMade / (Real)data->m_totalScorchMarks;
				m_nextScorchMarkFrame = orbitalBirthFrame + nextFactor * (orbitalDeathFrame - orbitalBirthFrame);

				//Generate iteration of fxlist for beam hitting ground.
				if( data->m_groundHitFX )
				{
					FXList::doFXPos( data->m_groundHitFX, &m_currentTargetPosition, nullptr );
				}

				//Also reveal vision because the owning player has full rights to watch the carnage he created!
				ThePartitionManager->doShroudReveal( m_currentTargetPosition.x, m_currentTargetPosition.y, data->m_revealRange, me->getControllingPlayer()->getPlayerMask() );
				ThePartitionManager->undoShroudReveal( m_currentTargetPosition.x, m_currentTargetPosition.y, data->m_revealRange, me->getControllingPlayer()->getPlayerMask() );
			}

			//Handle damage pulses
			if( m_nextDamagePulseFrame <= now )
			{
				m_damagePulsesMade++;

				DamageInfo damageInfo;

				Real totalFiringSeconds = data->m_totalFiringFrames / LOGICFRAMES_PER_SECOND;
				Real damagePerPulse = (Real)(totalFiringSeconds * data->m_damagePerSecond) / (Real)data->m_totalDamagePulses;

				damageInfo.in.m_amount = damagePerPulse;
				damageInfo.in.m_sourceID = me->getID();
				damageInfo.in.m_damageType = data->m_damageType;
				damageInfo.in.m_deathType = data->m_deathType;

				PartitionFilterAlive filterAlive;
				PartitionFilter *filters[] = { &filterAlive, nullptr };

				ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( &m_currentTargetPosition, damageRadius, FROM_CENTER_2D, filters );
				MemoryPoolObjectHolder hold( iter );
				for( Object *obj = iter->first(); obj; obj = iter->next() )
				{
					BodyModuleInterface *body = obj->getBodyModule();
					if( body )
					{
						body->attemptDamage( &damageInfo );
					}
				}

				if( data->m_damagePulseRemnantObjectName.isNotEmpty() )
				{
					//Create a remnant damaging object that will fade over time to represent the burning trail.
					const ThingTemplate *thing = TheThingFactory->findTemplate( data->m_damagePulseRemnantObjectName );
					if( thing )
					{
						//Fire and forget
						Object *remnant = TheThingFactory->newObject( thing, me->getTeam() );
						if( remnant )
						{
							remnant->setPosition( &m_currentTargetPosition );
						}
					}
				}

				//Calculate next damage pulse frame.
				Real nextFactor = (Real)m_damagePulsesMade / (Real)data->m_totalDamagePulses;
				m_nextDamagePulseFrame = orbitalBirthFrame + nextFactor * (orbitalDeathFrame - orbitalBirthFrame);
			}
		}

		if( endDecayFrame <= now )
		{
			setLogicalStatus( STATUS_PACKING );
		}
		else if( m_startDecayFrame <= now )
		{
			setLogicalStatus( STATUS_POSTFIRE );
		}
		else
		{
			setLogicalStatus( STATUS_FIRING );
		}
	}
	else if( readyToFireFrame <= now )
	{
		setLogicalStatus( STATUS_READY_TO_FIRE );
	}
	else if( almostReadyFrame <= now )
	{
		setLogicalStatus( STATUS_ALMOST_READY );
	}
	else if( raiseAntennaFrame <= now )
	{
		setLogicalStatus( STATUS_PREPARING );
	}
	else if( beginChargeFrame <= now )
	{
		setLogicalStatus( STATUS_CHARGING );
	}

	if( m_status == STATUS_FIRING )
	{
		if( m_nextLaunchFXFrame <= now )
		{
			//Generate iteration of fxlist for beam launching
			if( data->m_beamLaunchFX )
			{
				FXList::doFXPos( data->m_beamLaunchFX, &m_laserOriginPosition, nullptr );
			}
			m_nextLaunchFXFrame = now + data->m_framesBetweenLaunchFXRefresh;
		}
	}

	//Handle shrouded status changes for the client player.
	Player *localPlayer = rts::getObservedOrLocalPlayer();
	if( localPlayer )
	{
		Bool shrouded = me->getShroudedStatus( localPlayer->getPlayerIndex() ) != OBJECTSHROUD_CLEAR;
		if( shrouded )
		{
			//We can't see it so any client effects that have been added logically needs to be removed!
			removeAllEffects();
		}
		else
		{
			//We can see it -- we only want to do anything if we just started seeing it, which means
			//we want to add client effects again.
			Bool revealThisFrame = m_clientShroudedLastFrame != shrouded;
			if( revealThisFrame )
			{
				//Only if we reveal this frame, will we add client effects. The logic can take it from
				//here on... unless of course we lose sight again.
				setClientStatus( m_status, revealThisFrame );
			}
		}
		m_clientShroudedLastFrame = shrouded;
	}

	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::createOuterNodeParticleSystems( IntensityTypes intensity )
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();
	AsciiString str;
	switch( intensity )
	{
		case IT_LIGHT:
			str = data->m_outerNodesLightFlareParticleSystemName;
			break;
		case IT_MEDIUM:
			str = data->m_outerNodesMediumFlareParticleSystemName;
			break;
		case IT_INTENSE:
			str = data->m_outerNodesIntenseFlareParticleSystemName;
			break;
		case IT_FINISH:
			break;
	}

	if( str.isNotEmpty() )
	{
		const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate( str );
		if( tmp )
		{
			ParticleSystem *system;
			for( UnsignedInt i = 0; i < data->m_outerEffectNumBones; i++ )
			{
				system = TheParticleSystemManager->createParticleSystem( tmp );
				if( system )
				{
					m_outerSystemIDs[ i ] = system->getSystemID();
					system->setPosition( &m_outerNodePositions[ i ] );
					system->setLocalTransform( &m_outerNodeOrientations[ i ] );
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::createConnectorLasers( IntensityTypes intensity )
{
	//Cache bone positions for the laser when it is ready to fire
	if( !m_upBonesCached )
	{
		calculateUpBonePositions();
		m_upBonesCached = true;
	}

	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();
	AsciiString str;
	switch( intensity )
	{
		case IT_LIGHT:
			break;
		case IT_MEDIUM:
			str = data->m_connectorMediumLaserNameName;
			break;
		case IT_INTENSE:
			str = data->m_connectorIntenseLaserNameName;
			break;
		case IT_FINISH:
			break;
	}

	if( str.isNotEmpty() )
	{
		const ThingTemplate *thingTemplate = TheThingFactory->findTemplate( str );
		if( thingTemplate )
		{
			for( UnsignedInt i = 0; i < data->m_outerEffectNumBones; i++ )
			{
				Drawable *beam = TheThingFactory->newDrawable( thingTemplate );
				if( beam )
				{
					m_laserBeamIDs[ i ] = beam->getID();

					static NameKeyType nameKeyClientUpdate = NAMEKEY( "LaserUpdate" );
					LaserUpdate *update = (LaserUpdate*)beam->findClientUpdateModule( nameKeyClientUpdate );
					if( update )
					{
						update->initLaser( nullptr, &m_outerNodePositions[ i ], &m_connectorNodePosition );
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::createConnectorFlare( IntensityTypes intensity )
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();
	AsciiString str;
	switch( intensity )
	{
		case IT_LIGHT:
			break;
		case IT_MEDIUM:
			str = data->m_connectorMediumFlareParticleSystemName;
			break;
		case IT_INTENSE:
			str = data->m_connectorIntenseFlareParticleSystemName;
			break;
		case IT_FINISH:
			break;
	}

	if( str.isNotEmpty() )
	{
		const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate( str );
		ParticleSystem *system;
		if( tmp )
		{
			system = TheParticleSystemManager->createParticleSystem( tmp );
			if( system )
			{
				m_connectorSystemID = system->getSystemID();
				system->setPosition( &m_connectorNodePosition );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::createLaserBaseFlare( IntensityTypes intensity )
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();
	AsciiString str;
	switch( intensity )
	{
		case IT_LIGHT:
			str = data->m_laserBaseLightFlareParticleSystemName;
			break;
		case IT_MEDIUM:
			str = data->m_laserBaseMediumFlareParticleSystemName;
			break;
		case IT_INTENSE:
			str = data->m_laserBaseIntenseFlareParticleSystemName;
			break;
		case IT_FINISH:
			break;
	}

	if( str.isNotEmpty() )
	{
		const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate( str );
		if( tmp )
		{
			ParticleSystem *system = TheParticleSystemManager->createParticleSystem( tmp );
			if( system )
			{
				m_laserBaseSystemID = system->getSystemID();
				system->setPosition( &m_laserOriginPosition );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::createGroundToOrbitLaser( UnsignedInt growthFrames )
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();

	Drawable *beam = TheGameClient->findDrawableByID( m_groundToOrbitBeamID );
	if( beam )
	{
		TheGameClient->destroyDrawable( beam );
		// TheSuperHackers @fix helmutbuhler 19/04/2025 Invalidate the relevant drawable ID.
		m_groundToOrbitBeamID = INVALID_DRAWABLE_ID;
	}

	if( data->m_particleBeamLaserName.isNotEmpty() )
	{
		const ThingTemplate *thingTemplate = TheThingFactory->findTemplate( data->m_particleBeamLaserName );
		if( thingTemplate )
		{
			Drawable *beam = TheThingFactory->newDrawable( thingTemplate );
			if( beam )
			{
				m_groundToOrbitBeamID = beam->getID();

				static NameKeyType nameKeyClientUpdate = NAMEKEY( "LaserUpdate" );
				LaserUpdate *update = (LaserUpdate*)beam->findClientUpdateModule( nameKeyClientUpdate );
				if( update )
				{
					Coord3D orbitPosition;
					orbitPosition.set( m_laserOriginPosition );
					orbitPosition.z += ORBITAL_BEAM_Z_OFFSET;
					update->initLaser( nullptr, &m_laserOriginPosition, &orbitPosition, growthFrames );
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::createOrbitToTargetLaser( UnsignedInt growthFrames )
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();

	if ( m_orbitToTargetBeamID != INVALID_DRAWABLE_ID )
	{
		Drawable *beam = TheGameClient->findDrawableByID( m_orbitToTargetBeamID );
		if( beam )
		{
			TheAudio->removeAudioEvent( m_annihilationSound.getPlayingHandle() );
			TheGameClient->destroyDrawable( beam );
		}
		m_orbitToTargetBeamID = INVALID_DRAWABLE_ID;
	}

	if( data->m_particleBeamLaserName.isNotEmpty() )
	{
		const ThingTemplate *thingTemplate = TheThingFactory->findTemplate( data->m_particleBeamLaserName );
		if( thingTemplate )
		{
			Drawable *beam = TheThingFactory->newDrawable( thingTemplate );
			if( beam )
			{
				m_orbitToTargetBeamID = beam->getID();
				static NameKeyType nameKeyClientUpdate = NAMEKEY( "LaserUpdate" );
				LaserUpdate *update = (LaserUpdate*)beam->findClientUpdateModule( nameKeyClientUpdate );
				if( update )
				{
					Coord3D orbitPosition;
					orbitPosition.set( m_initialTargetPosition );
					orbitPosition.z += ORBITAL_BEAM_Z_OFFSET;
					update->initLaser( nullptr, &orbitPosition, &m_initialTargetPosition, growthFrames );
				}
				Coord3D audioPos = m_initialTargetPosition;
				audioPos.z += ORBITAL_BEAM_AUDIO_Z_OFFSET;
				beam->setPosition( &audioPos );
			}
		}
		if( m_annihilationSound.getEventName().isNotEmpty() )
		{
			m_annihilationSound.setDrawableID( m_orbitToTargetBeamID );
			//m_annihilationSound.setPosition( &m_initialTargetPosition );
			m_annihilationSound.setPlayingHandle( TheAudio->addAudioEvent( &m_annihilationSound ) );
		}
	}

	m_orbitToTargetLaserRadius = LaserRadiusUpdate();
	m_orbitToTargetLaserRadius.initRadius( growthFrames );
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::createGroundHitParticleSystem( IntensityTypes intensity )
{
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::removeAllEffects()
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();
	for( UnsignedInt i = 0; i < data->m_outerEffectNumBones; i++ )
	{
		if( m_outerSystemIDs && m_outerSystemIDs[ i ] )
		{
			TheParticleSystemManager->destroyParticleSystemByID( m_outerSystemIDs[ i ] );
			m_outerSystemIDs[ i ] = INVALID_PARTICLE_SYSTEM_ID;
		}
		Drawable *beam = TheGameClient->findDrawableByID( m_laserBeamIDs[ i ] );
		if( beam )
		{
			TheGameClient->destroyDrawable( beam );
			m_laserBeamIDs[ i ] = INVALID_DRAWABLE_ID;
		}
	}
	if( m_connectorSystemID )
	{
		TheParticleSystemManager->destroyParticleSystemByID( m_connectorSystemID );
		m_connectorSystemID = INVALID_PARTICLE_SYSTEM_ID;
	}
	if( m_laserBaseSystemID )
	{
		TheParticleSystemManager->destroyParticleSystemByID( m_laserBaseSystemID );
		m_laserBaseSystemID = INVALID_PARTICLE_SYSTEM_ID;
	}
	Drawable *beam = TheGameClient->findDrawableByID( m_groundToOrbitBeamID );
	if( beam )
	{
		TheGameClient->destroyDrawable( beam );
		m_groundToOrbitBeamID = INVALID_DRAWABLE_ID;
	}

	//Remove all sound hooks
}


//-------------------------------------------------------------------------------------------------
Bool ParticleUplinkCannonUpdate::calculateDefaultInformation()
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();
	Object *obj = getObject();

	//Get the local bone positions for each of the outer nodes.
	Coord3D bonePositions[ MAX_OUTER_NODES ];
	Matrix3D boneMatrices[ MAX_OUTER_NODES ];
	int numBones = obj->getMultiLogicalBonePosition( data->m_outerEffectBaseBoneName.str(), data->m_outerEffectNumBones, bonePositions, boneMatrices, FALSE );

	if( numBones != data->m_outerEffectNumBones )
	{
		DEBUG_CRASH( ("Particle cannon requires %d outer node bones, but can only find %d bones.", data->m_outerEffectNumBones, numBones ) );
		m_invalidSettings = true;
		return false;
	}

	for( UnsignedInt i = 0; i < data->m_outerEffectNumBones; i++ )
	{
		m_laserBeamIDs[ i ] = INVALID_DRAWABLE_ID;
		m_outerSystemIDs[ i ] = INVALID_PARTICLE_SYSTEM_ID;

		//Convert the local bone position into world space.
		Matrix3D nodeMatrix;
		getObject()->convertBonePosToWorldPos( &bonePositions[i], &boneMatrices[i], &m_outerNodePositions[ i ], &m_outerNodeOrientations[ i ] );
	}

	return true;

}

//-------------------------------------------------------------------------------------------------
Bool ParticleUplinkCannonUpdate::calculateUpBonePositions()
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();
	Object *obj = getObject();
	//Due to instant special weapons (or scripting)... it's possible to be in many states that are ready to fire (or firing)
	Drawable *draw = obj->getDrawable();
	Matrix3D mtx;
	Coord3D pos;
	if( draw )
	{
		if( data->m_connectorBoneName.isNotEmpty() && draw->getCurrentClientBonePositions( data->m_connectorBoneName.str(), 0, &pos, &mtx, 1 ) )
		{
			obj->convertBonePosToWorldPos( &pos, &mtx, &m_connectorNodePosition, &mtx );
		}
		if( data->m_connectorBoneName.isNotEmpty() && draw->getCurrentClientBonePositions( data->m_fireBoneName.str(), 0, &pos, &mtx, 1 ) )
		{
			obj->convertBonePosToWorldPos( &pos, &mtx, &m_laserOriginPosition, &mtx );
		}
	}
	return true;
}


//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::setLogicalStatus( PUCStatus newStatus )
{
	Object *obj = getObject();

	if( m_status == newStatus )
	{
		return;
	}

	//Handle entering new status
	switch( newStatus )
	{
		case STATUS_IDLE:
		{
			//Set unpacked animation
			obj->clearModelConditionFlags( MAKE_MODELCONDITION_MASK3( MODELCONDITION_PACKING, MODELCONDITION_UNPACKING, MODELCONDITION_DEPLOYED ) );
			TheAudio->removeAudioEvent( m_powerupSound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_unpackToReadySound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_firingToIdleSound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_annihilationSound.getPlayingHandle() );
			break;
		}
		case STATUS_CHARGING:
		{
			m_laserStatus = LASERSTATUS_NONE;
			m_powerupSound.setObjectID( obj->getID() );
			m_powerupSound.setPlayingHandle( TheAudio->addAudioEvent( &m_powerupSound ) );
			TheAudio->removeAudioEvent( m_unpackToReadySound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_firingToIdleSound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_annihilationSound.getPlayingHandle() );
			break;
		}
		case STATUS_PREPARING:
		{
			//Set unpacking animation
			obj->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PACKING, MODELCONDITION_DEPLOYED ),
																					MAKE_MODELCONDITION_MASK( MODELCONDITION_UNPACKING ) );
			if( m_unpackToReadySound.getEventName().isNotEmpty() )
			{
				m_unpackToReadySound.setObjectID( obj->getID() );
				m_unpackToReadySound.setPlayingHandle( TheAudio->addAudioEvent( &m_unpackToReadySound ) );
			}
			TheAudio->removeAudioEvent( m_firingToIdleSound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_annihilationSound.getPlayingHandle() );

			m_laserStatus = LASERSTATUS_NONE;
			break;
		}
		case STATUS_ALMOST_READY:
		{
			//Set deployed animation
			obj->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PACKING, MODELCONDITION_UNPACKING ),
																					 MAKE_MODELCONDITION_MASK( MODELCONDITION_DEPLOYED ) );
			m_laserStatus = LASERSTATUS_NONE;
			break;
		}
		case STATUS_READY_TO_FIRE:
		{
			obj->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PACKING, MODELCONDITION_UNPACKING ),
																					 MAKE_MODELCONDITION_MASK( MODELCONDITION_DEPLOYED ) );
			TheAudio->removeAudioEvent( m_powerupSound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_firingToIdleSound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_annihilationSound.getPlayingHandle() );
			m_laserStatus = LASERSTATUS_NONE;
			break;
		}
		case STATUS_PREFIRE:
		{
			break;
		}
		case STATUS_FIRING:
		{
			obj->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PACKING, MODELCONDITION_UNPACKING ),
																					 MAKE_MODELCONDITION_MASK( MODELCONDITION_DEPLOYED ) );
			if( m_firingToIdleSound.getEventName().isNotEmpty() )
			{
				m_firingToIdleSound.setObjectID( obj->getID() );
				m_firingToIdleSound.setPlayingHandle( TheAudio->addAudioEvent( &m_firingToIdleSound ) );
			}
			TheAudio->removeAudioEvent( m_powerupSound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_unpackToReadySound.getPlayingHandle() );
			TheAudio->removeAudioEvent( m_annihilationSound.getPlayingHandle() );
			m_nextLaunchFXFrame = 0;
			break;
		}
		case STATUS_POSTFIRE:
		{
			break;
		}
		case STATUS_PACKING:
		{
			//Set packing animation
			obj->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_UNPACKING, MODELCONDITION_DEPLOYED ),
																						 MAKE_MODELCONDITION_MASK( MODELCONDITION_PACKING ) );
			break;
		}
	}

	//Set new status.
	m_status = newStatus;

	setClientStatus( m_status, FALSE );
}

//-------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::setClientStatus( PUCStatus newStatus, Bool revealThisFrame )
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();

	//Caches bone positions for positioning the client beams.
	if( !m_defaultInfoCached )
	{
		if( !calculateDefaultInformation() )
		{
			m_invalidSettings = true;
			return;
		}
		m_defaultInfoCached = true;
	}

	//This isn't the most efficient way to do this. Various effects can live in more than
	//one status state, but keeping track of them properly is error-prone and hard to read.
	//Therefore, the new philosophy is to delete EVERYTHING between status changes and
	//create the ones we want!
	removeAllEffects();

	//Handle entering new status
	switch( newStatus )
	{
		case STATUS_IDLE:
		{
			break;
		}
		case STATUS_CHARGING:
		{
			createOuterNodeParticleSystems( IT_LIGHT );
			break;
		}
		case STATUS_PREPARING:
		{
			createOuterNodeParticleSystems( IT_MEDIUM );
			break;
		}
		case STATUS_ALMOST_READY:
		{
			createOuterNodeParticleSystems( IT_MEDIUM );
			createConnectorLasers( IT_MEDIUM );
			createConnectorFlare( IT_MEDIUM );
			break;
		}
		case STATUS_READY_TO_FIRE:
		{
			createOuterNodeParticleSystems( IT_MEDIUM );
			createConnectorLasers( IT_MEDIUM );
			createConnectorFlare( IT_MEDIUM );
			createLaserBaseFlare( IT_LIGHT );
			break;
		}
		case STATUS_PREFIRE:
		{
			break;
		}
		case STATUS_FIRING:
		{
			if( revealThisFrame )
			{
				createGroundToOrbitLaser( 0 );
			}
			else
			{
				createGroundToOrbitLaser( data->m_widthGrowFrames );
			}
			createOuterNodeParticleSystems( IT_INTENSE );
			createConnectorLasers( IT_INTENSE );
			createConnectorFlare( IT_INTENSE );
			createLaserBaseFlare( IT_INTENSE );
			break;
		}
		case STATUS_POSTFIRE:
		{
			createOuterNodeParticleSystems( IT_MEDIUM );
			createConnectorLasers( IT_MEDIUM );
			createConnectorFlare( IT_MEDIUM );
			createLaserBaseFlare( IT_MEDIUM );
			createGroundToOrbitLaser( 0 );
			Drawable *beam = TheGameClient->findDrawableByID( m_groundToOrbitBeamID );
			if( beam )
			{
				static NameKeyType nameKeyClientUpdate = NAMEKEY( "LaserUpdate" );
				LaserUpdate *update = (LaserUpdate*)beam->findClientUpdateModule( nameKeyClientUpdate );
				if( update )
				{
					if( revealThisFrame )
					{
						update->setDecayFrames( 0 );
					}
					else
					{
						update->setDecayFrames( data->m_widthGrowFrames );
					}
				}
			}
			break;
		}
		case STATUS_PACKING:
		{
			break;
		}
	}
}


//-------------------------------------------------------------------------------------------------
Bool ParticleUplinkCannonUpdate::doesSpecialPowerHaveOverridableDestinationActive() const
{
	return m_status == STATUS_PREFIRE || m_status == STATUS_FIRING || m_status == STATUS_POSTFIRE;
}


// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: Serialize decay frames
	* 4: TheSuperHackers @tweak Serialize orbit to target laser radius
	*/
// ------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::xfer( Xfer *xfer )
{
	const ParticleUplinkCannonUpdateModuleData *data = getParticleUplinkCannonUpdateModuleData();

	// version
#if RETAIL_COMPATIBLE_XFER_SAVE
	const XferVersion currentVersion = 2;
#else
	const XferVersion currentVersion = 4;
#endif
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	m_xferVersion = version;

	// extend base class
	UpdateModule::xfer( xfer );

	// status
	xfer->xferUser( &m_status, sizeof( PUCStatus ) );

	// laser status
	xfer->xferUser( &m_laserStatus, sizeof( LaserStatus ) );

	// frames
	xfer->xferUnsignedInt( &m_frames );

	// we do not need to tie up the special power module pointer, it is done on object creation
	// SpecialPowerModuleInterface *m_specialPowerModule;

	// outer system ids
	xfer->xferUser( m_outerSystemIDs, sizeof( ParticleSystemID ) * MAX_OUTER_NODES );

	// laser beam ids
	xfer->xferUser( m_laserBeamIDs, sizeof( DrawableID ) * MAX_OUTER_NODES );

	// ground to orbit beam id
	xfer->xferDrawableID( &m_groundToOrbitBeamID );

	// orbit to target beam
	xfer->xferDrawableID( &m_orbitToTargetBeamID );

	// connector system ID
	xfer->xferUser( &m_connectorSystemID, sizeof( ParticleSystemID ) );

	// laser base system id
	xfer->xferUser( &m_laserBaseSystemID, sizeof( ParticleSystemID ) );

	// outer node positions
	xfer->xferUser( m_outerNodePositions, sizeof( Coord3D ) * MAX_OUTER_NODES );

	// outer node orientations
	xfer->xferUser( m_outerNodeOrientations, sizeof( Matrix3D ) * MAX_OUTER_NODES );

	// connector node position
	xfer->xferCoord3D( &m_connectorNodePosition );

	// laser origin position
	xfer->xferCoord3D( &m_laserOriginPosition );

	// The manual override target destination.
	xfer->xferCoord3D( &m_overrideTargetDestination );

	// up bones cached
	xfer->xferBool( &m_upBonesCached );

	// default info cached
	xfer->xferBool( &m_defaultInfoCached );

	// invalid settings
	xfer->xferBool( &m_invalidSettings );

	// initial target position
	xfer->xferCoord3D( &m_initialTargetPosition );

	// current target position
	xfer->xferCoord3D( &m_currentTargetPosition );

	// scorch marks make
	xfer->xferUnsignedInt( &m_scorchMarksMade );

	// next scorch mark frame
	xfer->xferUnsignedInt( &m_nextScorchMarkFrame );

	// next launch FX frame
	xfer->xferUnsignedInt( &m_nextLaunchFXFrame );

	// damage pluses made
	xfer->xferUnsignedInt( &m_damagePulsesMade );

	// next damage pulse frame
	xfer->xferUnsignedInt( &m_nextDamagePulseFrame );

	// start attack frame
	xfer->xferUnsignedInt( &m_startAttackFrame );

	// start attack frame
	if( xfer->getXferMode() == XFER_SAVE || version >= 2 )
	{
		xfer->xferUnsignedInt( &m_startDecayFrame );
	}
	else
	{
		m_startDecayFrame = m_startAttackFrame + data->m_totalFiringFrames;
	}

	// the time of last manual target click
	xfer->xferUnsignedInt( & m_lastDrivingClickFrame );

	// the time of the 2nd last manual target click
	xfer->xferUnsignedInt( &m_2ndLastDrivingClickFrame );

	if( version >= 4 )
	{
		m_orbitToTargetLaserRadius.xfer( xfer );
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ParticleUplinkCannonUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

	if (m_xferVersion <= 3)
	{
		// TheSuperHackers @info xezon 17/05/2025
		// For retail game compatibility, this transfers the loaded visual laser radius
		// settings from the Drawable's LaserUpdate to the local LaserRadiusUpdate.
		if( m_orbitToTargetBeamID != INVALID_DRAWABLE_ID )
		{
			Drawable* drawable = TheGameClient->findDrawableByID( m_orbitToTargetBeamID );
			if( drawable != nullptr )
			{
				static NameKeyType nameKeyClientUpdate = NAMEKEY( "LaserUpdate" );
				LaserUpdate *update = (LaserUpdate*)drawable->findClientUpdateModule( nameKeyClientUpdate );
				if( update != nullptr )
				{
					m_orbitToTargetLaserRadius = update->getLaserRadiusUpdate();
				}
			}
			else
			{
				DEBUG_CRASH(( "ParticleUplinkCannonUpdate::loadPostProcess - Unable to find drawable for m_orbitToTargetBeamID" ));
			}
		}

		// TheSuperHackers @bugfix stephanmeesters 13/02/2026
		// For retail game compatibility, this fixes an issue where particle cannon sounds are not audible after saveload.
		if( (m_status == STATUS_CHARGING || m_status == STATUS_PREPARING || m_status == STATUS_ALMOST_READY) &&
			m_powerupSound.getEventName().isNotEmpty() )
		{
			m_powerupSound.setObjectID( getObject()->getID() );
			m_powerupSound.setPlayingHandle( TheAudio->addAudioEvent( &m_powerupSound ) );
		}

		if( (m_status == STATUS_PREPARING || m_status == STATUS_ALMOST_READY || m_status == STATUS_READY_TO_FIRE || m_status == STATUS_PREFIRE) &&
			m_unpackToReadySound.getEventName().isNotEmpty() )
		{
			m_unpackToReadySound.setObjectID( getObject()->getID() );
			m_unpackToReadySound.setPlayingHandle( TheAudio->addAudioEvent( &m_unpackToReadySound ) );
		}

		if( m_status == STATUS_FIRING || m_status == STATUS_POSTFIRE ||
			(m_status == STATUS_PACKING && (m_laserStatus == LASERSTATUS_DECAYING || m_laserStatus == LASERSTATUS_DEAD)) )
		{
			if( m_firingToIdleSound.getEventName().isNotEmpty() )
			{
				m_firingToIdleSound.setObjectID( getObject()->getID() );
				m_firingToIdleSound.setPlayingHandle( TheAudio->addAudioEvent( &m_firingToIdleSound ) );
			}

			if( m_orbitToTargetBeamID != INVALID_DRAWABLE_ID && m_annihilationSound.getEventName().isNotEmpty() )
			{
				m_annihilationSound.setDrawableID( m_orbitToTargetBeamID );
				m_annihilationSound.setPlayingHandle( TheAudio->addAudioEvent( &m_annihilationSound ) );
			}
		}
	}

}
