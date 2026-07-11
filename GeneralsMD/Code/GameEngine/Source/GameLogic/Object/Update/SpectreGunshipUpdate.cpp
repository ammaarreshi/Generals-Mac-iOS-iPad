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

// FILE: SpectreGunshipUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Author: Mark Lorenzen, April 2003
// Desc:   Update module to handle weapon firing of the SpectreGunship Generals special power.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_DEATH_NAMES

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameAudio.h"
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

#include "GameLogic/Locomotor.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpectreGunshipUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/LaserUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"





#define ORBIT_INSERTION_SLOPE_MAX (0.8f)
#define ORBIT_INSERTION_SLOPE_MIN (0.5f)
#define ONE (1.0f)
#define ZERO (0.0f)
#define LOTS_OF_SHOTS (9999)




//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpectreGunshipUpdateModuleData::SpectreGunshipUpdateModuleData()
{
	m_specialPowerTemplate			   = nullptr;
/******BOTH*******//*BOTH*//******BOTH*******//******BOTH*******/  m_attackAreaRadius             = 200.0f;
/*************/  m_gattlingStrafeFXParticleSystem = nullptr;
/*************/  m_howitzerWeaponTemplate = nullptr;
/*************/  m_orbitFrames                  = 0;
/*************/  m_targetingReticleRadius       = 25.0f;
/*************/  m_gunshipOrbitRadius           = 250.0f;
  m_strafingIncrement = 20.0f;
  m_orbitInsertionSlope = 0.7f;
  m_howitzerFiringRate = 10;
  m_howitzerFollowLag = 0;
  m_randomOffsetForHowitzer = 20.0f;
}

static Real zero = 0.0f;
//-------------------------------------------------------------------------------------------------
/*static*/ void SpectreGunshipUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
    { "SpecialPowerTemplate",           INI::parseSpecialPowerTemplate,   nullptr, offsetof( SpectreGunshipUpdateModuleData, m_specialPowerTemplate ) },
    { "GattlingTemplateName",           INI::parseAsciiString,				    nullptr, offsetof( SpectreGunshipUpdateModuleData, m_gattlingTemplateName ) },
		{ "HowitzerFiringRate",	            INI::parseDurationUnsignedInt,    nullptr, offsetof( SpectreGunshipUpdateModuleData, m_howitzerFiringRate ) },
		{ "OrbitTime",	                    INI::parseDurationUnsignedInt,		nullptr, offsetof( SpectreGunshipUpdateModuleData, m_orbitFrames ) },
		{ "HowitzerFollowLag",	            INI::parseDurationUnsignedInt,		nullptr, offsetof( SpectreGunshipUpdateModuleData, m_howitzerFollowLag ) },
    { "AttackAreaRadius",	              INI::parseReal,				            nullptr, offsetof( SpectreGunshipUpdateModuleData, m_attackAreaRadius ) },
		{ "StrafingIncrement",	            INI::parseReal,				            nullptr, offsetof( SpectreGunshipUpdateModuleData, m_strafingIncrement ) },
		{ "OrbitInsertionSlope",	          INI::parseReal,	  			          nullptr, offsetof( SpectreGunshipUpdateModuleData, m_orbitInsertionSlope ) },
		{ "RandomOffsetForHowitzer",        INI::parseReal,	                  nullptr, offsetof( SpectreGunshipUpdateModuleData, m_randomOffsetForHowitzer ) },
		{ "TargetingReticleRadius",	        INI::parseReal,				            nullptr, offsetof( SpectreGunshipUpdateModuleData, m_targetingReticleRadius ) },
		{ "GunshipOrbitRadius",	            INI::parseReal,				            nullptr, offsetof( SpectreGunshipUpdateModuleData, m_gunshipOrbitRadius ) },
		{ "HowitzerWeaponTemplate",				  INI::parseWeaponTemplate,				  nullptr, offsetof( SpectreGunshipUpdateModuleData, m_howitzerWeaponTemplate ) },
		{ "GattlingStrafeFXParticleSystem",	INI::parseParticleSystemTemplate, nullptr, offsetof( SpectreGunshipUpdateModuleData, m_gattlingStrafeFXParticleSystem ) },
		{ "AttackAreaDecal",		            RadiusDecalTemplate::parseRadiusDecalTemplate,	nullptr, offsetof( SpectreGunshipUpdateModuleData, m_attackAreaDecalTemplate ) },
		{ "TargetingReticleDecal",		      RadiusDecalTemplate::parseRadiusDecalTemplate,	nullptr, offsetof( SpectreGunshipUpdateModuleData, m_targetingReticleDecalTemplate ) },




    { nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
SpectreGunshipUpdate::SpectreGunshipUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_specialPowerModule = nullptr;
  m_gattlingID = INVALID_ID;
	m_status = GUNSHIP_STATUS_IDLE;
	m_initialTargetPosition.zero();
	m_overrideTargetDestination.zero();
  m_gattlingTargetPosition.zero();
  m_positionToShootAt.zero();
	m_attackAreaDecal.clear();
	m_targetingReticleDecal.clear();
  m_orbitEscapeFrame = 0;
	m_afterburnerSound = *(getObject()->getTemplate()->getPerUnitSound("Afterburner"));
	m_howitzerFireSound = *(getObject()->getTemplate()->getPerUnitSound("HowitzerFire"));

  m_okToFireHowitzerCounter = 0;

#if defined TRACKERS
m_howitzerTrackerDecal.clear();
#endif

}

//-------------------------------------------------------------------------------------------------
SpectreGunshipUpdate::~SpectreGunshipUpdate()
{
	m_attackAreaDecal.clear();
	m_targetingReticleDecal.clear();

#if defined TRACKERS
m_howitzerTrackerDecal.clear();
#endif

}

//-------------------------------------------------------------------------------------------------
// Validate that we have the necessary data from the ini file.
//-------------------------------------------------------------------------------------------------
void SpectreGunshipUpdate::onObjectCreated()
{
	const SpectreGunshipUpdateModuleData *data = getSpectreGunshipUpdateModuleData();
	Object *obj = getObject();

	if( !data->m_specialPowerTemplate )
	{
		DEBUG_CRASH( ("%s object's SpectreGunshipUpdate lacks access to the SpecialPowerTemplate. Needs to be specified in ini.", obj->getTemplate()->getName().str() ) );
		return;
	}

	m_specialPowerModule = obj->getSpecialPowerModule( data->m_specialPowerTemplate );
  m_satellitePosition.set( *obj->getPosition() );
}

//-------------------------------------------------------------------------------------------------
Bool SpectreGunshipUpdate::initiateIntentToDoSpecialPower(const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const SpectreGunshipUpdateModuleData *data = getSpectreGunshipUpdateModuleData();

	if( m_specialPowerModule->getSpecialPowerTemplate() != specialPowerTemplate )
	{
		//Check to make sure our modules are connected.
		return FALSE;
	}

	if( !BitIsSet( commandOptions, COMMAND_FIRED_BY_SCRIPT ) )
	{
		m_initialTargetPosition.set( *targetPos );
		m_overrideTargetDestination.set( *targetPos );
    m_gattlingTargetPosition.set( *targetPos );
	}
	else
	{
		UnsignedInt now = TheGameLogic->getFrame();
		m_specialPowerModule->setReadyFrame( now );
		m_initialTargetPosition.set( *targetPos );
		setLogicalStatus( GUNSHIP_STATUS_INSERTING );
	}


  Object * gunShip = getObject();


  if ( gunShip )
  {
    // MAKE TWEAKS TO AI SO SHIP MOVES PRETTY
    AIUpdateInterface *shipAI = gunShip->getAIUpdateInterface();
    if ( shipAI)
    {
     shipAI->chooseLocomotorSet( LOCOMOTORSET_PANIC );
	    shipAI->getCurLocomotor()->setAllowInvalidPosition(TRUE);
	    shipAI->getCurLocomotor()->setUltraAccurate(TRUE);	// set ultra-accurate just so AI won't try to adjust our dest
    }

    Drawable *draw = gunShip->getDrawable();
    if ( draw )
      draw->clearAndSetModelConditionState( MODELCONDITION_DOOR_1_OPENING, MODELCONDITION_DOOR_1_CLOSING );
    friend_enableAfterburners(TRUE);

    setLogicalStatus( GUNSHIP_STATUS_INSERTING ); // The gunship is en route to the tharget area, from map-edge


    // TELL THE GUNNERS ABOARD THE GUNSHIP TO HOLD THEIR FIRE UNTIL ORBIT INSERTION
        ContainModuleInterface *shipContain = gunShip->getContain();
    if ( shipContain )
    {

      Object *newGattling = TheGameLogic->findObjectByID( m_gattlingID );
	    const ThingTemplate *gattlingTemplate = TheThingFactory->findTemplate( data->m_gattlingTemplateName );
	    if( newGattling != nullptr )
      {
        m_gattlingID = INVALID_ID;
        newGattling = nullptr;
      }
      if ( gattlingTemplate )
      {
        newGattling = TheThingFactory->newObject( gattlingTemplate, getObject()->getTeam() );
        DEBUG_ASSERTCRASH( gunShip, ("SpecterGunshipUpdate failed to find or create a GATTLING object"));
        shipContain->addToContain( newGattling );
        m_gattlingID = newGattling->getID();
      }


      Object *gattling = TheGameLogic->findObjectByID( m_gattlingID );
      if ( gattling )
        gattling->setDisabled ( DISABLED_PARALYZED );

    }

  }


	data->m_attackAreaDecalTemplate.createRadiusDecal( *getObject()->getPosition(), data->m_attackAreaRadius, getObject()->getControllingPlayer(), m_attackAreaDecal);
	data->m_targetingReticleDecalTemplate.createRadiusDecal( *getObject()->getPosition(), data->m_targetingReticleRadius, getObject()->getControllingPlayer(), m_targetingReticleDecal);


#if defined TRACKERS
	data->m_targetingReticleDecalTemplate.createRadiusDecal( *getObject()->getPosition(), data->m_targetingReticleRadius, getObject()->getControllingPlayer(), m_howitzerTrackerDecal);
#endif


	SpecialPowerModuleInterface *spmInterface = getObject()->getSpecialPowerModule( specialPowerTemplate );
	if( spmInterface )
	{
		SpecialPowerModule *spModule = (SpecialPowerModule*)spmInterface;
		spModule->markSpecialPowerTriggered( &m_initialTargetPosition );
	}

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool SpectreGunshipUpdate::isPowerCurrentlyInUse( const CommandButton *command ) const
{
	return false;
}

//-------------------------------------------------------------------------------------------------
void SpectreGunshipUpdate::setSpecialPowerOverridableDestination( const Coord3D *loc )
{
	Object *me = getObject();
	if( !me->isDisabled() )
	{
		m_overrideTargetDestination = *loc;

		if( me->getControllingPlayer()  &&  me->getControllingPlayer()->isLocalPlayer() )
		{
			AudioEventRTS soundToPlay = *me->getTemplate()->getVoiceAttack();
			soundToPlay.setObjectID( me->getID() );
			TheAudio->addAudioEvent(&soundToPlay);
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool SpectreGunshipUpdate::isPointOffMap( const Coord3D& testPos ) const
{
	Region3D mapRegion;
	TheTerrainLogic->getExtentIncludingBorder( &mapRegion );

	if (!mapRegion.isInRegionNoZ( testPos ))
		return true;

	return false;
}


// PARTITION FILTERS!
//-----------------------------------------------------------------------------
class PartitionFilterLiveMapEnemies : public PartitionFilter
{
private:
	const Object *m_obj;
public:
	PartitionFilterLiveMapEnemies(const Object *obj) : m_obj(obj) { }

	virtual Bool allow(Object *objOther) override
	{
		// this is way fast (bit test) so do it first.
		if (objOther->isEffectivelyDead())
			return false;

		// this is also way fast (bit test) so do it next.
		if (objOther->isOffMap() != m_obj->isOffMap())
			return false;

		Relationship r = m_obj->getRelationship(objOther);
		if (r != ENEMIES)
			return false;

		return true;
	}

#if defined(RTS_DEBUG)
	virtual const char* debugGetName() override { return "PartitionFilterLiveMapEnemies"; }
#endif
};
//-----------------------------------------------------------------------------





//-------------------------------------------------------------------------------------------------
/** The update callback. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime SpectreGunshipUpdate::update()
{
	const SpectreGunshipUpdateModuleData *data = getSpectreGunshipUpdateModuleData();

   Object *gunship = getObject();
   if ( gunship )
   {


      if ( gunship->isEffectivelyDead() )
        return UPDATE_SLEEP_FOREVER;




      m_attackAreaDecal.update();
      m_targetingReticleDecal.update();
#if defined TRACKERS
	    m_howitzerTrackerDecal.update();
#endif

      AIUpdateInterface *shipAI = gunship->getAIUpdateInterface();
      AIUpdateInterface *gattlingAI = nullptr;

      Object *gattling = TheGameLogic->findObjectByID( m_gattlingID );
      if ( gattling )
      {
				gattlingAI = gattling->getAIUpdateInterface();
      }



      if ( m_status == GUNSHIP_STATUS_INSERTING || m_status == GUNSHIP_STATUS_ORBITING )
      {


  //   init'l    apogee
  //   target *<-------->*
  //    posi  ^\         /
  //          | \      /
  //  perigee |  \   /
  //          |   */ declination
  //          |  / \
  //          v/    * m_satelliteposition
  //          *
  //          |
  //          |
  //          |
  //          |
  //          |
  //          * gunship->getPosition()

        //perigee is the point in the orbital arc nearest the satellite being captured
        Coord3D perigee = *gunship->getPosition();
        perigee.sub( m_initialTargetPosition );
        perigee.z = zero;
        Real distanceToTarget = perigee.length();
        perigee.normalize();

        //apogee is the anteclockwise point farthest from the perigee line
        Coord3D apogee;
        apogee.z = zero;
        apogee.x = -perigee.y;
        apogee.y =  perigee.x;

        // declination intersects line [p][a], given an attack slope of n1/n2
        Coord3D declination;
        Real n1 = min( ORBIT_INSERTION_SLOPE_MAX, max(ORBIT_INSERTION_SLOPE_MIN, data->m_orbitInsertionSlope) );
        Real n2 = ONE - n1;
        declination.z = zero;
        declination.x = ( perigee.x * n1 ) + ( apogee.x * n2 );
        declination.y = ( perigee.y * n1 ) + ( apogee.y * n2 );

        //scale out to the orbital radius
        Real orbitalRadius = data->m_gunshipOrbitRadius;
        declination.x *= orbitalRadius;
        declination.y *= orbitalRadius;

        m_satellitePosition.x = m_initialTargetPosition.x + declination.x;
        m_satellitePosition.y = m_initialTargetPosition.y + declination.y;

        if ( shipAI)
        {
           shipAI->aiMoveToPosition( &m_satellitePosition, CMD_FROM_AI );
        }

        Real constraintRadius = data->m_attackAreaRadius - data->m_targetingReticleRadius;

        //Constrain Target Override to the targeting radius
        Coord3D overrideTargetDelta = m_initialTargetPosition;
        overrideTargetDelta.sub( m_overrideTargetDestination );
        if ( overrideTargetDelta.length() > constraintRadius )
        {
          overrideTargetDelta.normalize();
          overrideTargetDelta.x *= constraintRadius;
          overrideTargetDelta.y *= constraintRadius;

          m_overrideTargetDestination.x = m_initialTargetPosition.x - overrideTargetDelta.x;
          m_overrideTargetDestination.y = m_initialTargetPosition.y - overrideTargetDelta.y;

        }

        m_attackAreaDecal.setPosition( m_initialTargetPosition );
        m_targetingReticleDecal.setPosition( m_overrideTargetDestination );


#if defined TRACKERS
	 m_howitzerTrackerDecal.setPosition( m_gattlingTargetPosition );
#endif


        if ( (m_status == GUNSHIP_STATUS_INSERTING) && (distanceToTarget < orbitalRadius ) )// close enough to shoot
        {
          setLogicalStatus( GUNSHIP_STATUS_ORBITING );
          m_orbitEscapeFrame = TheGameLogic->getFrame() + data->m_orbitFrames;

          Object *gattling = TheGameLogic->findObjectByID( m_gattlingID );
          if ( gattling )
            gattling->clearDisabled ( DISABLED_PARALYZED );

          AIUpdateInterface *shipAI = gunship->getAIUpdateInterface();
          if ( shipAI)
          {
            shipAI->chooseLocomotorSet( LOCOMOTORSET_NORMAL );
	          shipAI->getCurLocomotor()->setAllowInvalidPosition(TRUE);
	          shipAI->getCurLocomotor()->setUltraAccurate(TRUE);	// set ultra-accurate just so AI won't try to adjust our dest
          }

          Drawable *draw = gunship->getDrawable();
          if ( draw )
            draw->clearAndSetModelConditionState( MODELCONDITION_DOOR_1_CLOSING, MODELCONDITION_DOOR_1_OPENING );
          friend_enableAfterburners(FALSE);

        }

      }


      if ( m_status == GUNSHIP_STATUS_ORBITING )
      {
        Object *validTargetObject = nullptr;


        if ( TheGameLogic->getFrame() >= m_orbitEscapeFrame )
        {
          cleanUp();
          setLogicalStatus( GUNSHIP_STATUS_DEPARTING );

          // CEASE FIRE, RETURN TO BASE
          disengageAndDepartAO( gunship );


        }
        else
        {

          // ONLY EVERY FEW FRAMES DO WE RE_EVALUATE THE TARGET OBJECT
          if ( TheGameLogic->getFrame() %data->m_howitzerFiringRate < ONE )
          {

            m_positionToShootAt = m_overrideTargetDestination; // unless we get a hit, below

	          PartitionFilterLiveMapEnemies filterObvious( gunship );
	          PartitionFilterStealthedAndUndetected filterStealth( gunship, false );
	          PartitionFilterPossibleToAttack filterAttack(ATTACK_NEW_TARGET, gunship, CMD_FROM_AI);
	          PartitionFilterFreeOfFog filterFogged( gunship->getControllingPlayer()->getPlayerIndex() );
	          PartitionFilter *filters[6];
	          Int numFilters = 0;
	          filters[numFilters++] = &filterObvious;
	          filters[numFilters++] = &filterStealth;
	          filters[numFilters++] = &filterAttack;
	          filters[numFilters++] = &filterFogged;
	          filters[numFilters] = nullptr;



            // THIS WILL FIND A VALID TARGET WITHIN THE TARGETING RETICLE
	          ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange(&m_overrideTargetDestination,
              data->m_targetingReticleRadius,
              FROM_BOUNDINGSPHERE_2D,
              filters,
              ITER_SORTED_NEAR_TO_FAR);
	          MemoryPoolObjectHolder holder(iter);
	          for (Object *theEnemy = iter->first(); theEnemy; theEnemy = iter->next())
	          {
              if ( theEnemy && isFairDistanceFromShip( theEnemy ) )
              {
                validTargetObject = theEnemy;
                break;
              }
	          }



            // WE WANT THE WIDE_RANGE AUTOACQUIRE POWER DISABLED FOR HUMAN PLAYERS
            // SO THAT THE SPECTREGUNSHIP REQUIRES BABYSITTING AT ALL TIMES
            if (gunship->getControllingPlayer()->getPlayerType() != PLAYER_HUMAN )
            {
              if ( ! validTargetObject )
              {
                // set a flag to start the targeting decal fading, since there is nothing to kill there
                // THIS WILL FIND A VALID TARGET ANYWHERE INSIDE THE TARGETING AREA (THE BIG CIRCLE)
	              ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange(&m_initialTargetPosition,
                  data->m_attackAreaRadius,
                  FROM_BOUNDINGSPHERE_2D,
                  filters,
                  ITER_SORTED_NEAR_TO_FAR);
	              MemoryPoolObjectHolder holder(iter);
	              for (Object *theEnemy = iter->first(); theEnemy; theEnemy = iter->next())
	              {
                  if ( theEnemy && isFairDistanceFromShip( theEnemy ) )
                  {
                    // WE GOT A HIT!!!! SHOOT HIM!
                    validTargetObject = theEnemy;
                    m_positionToShootAt = *validTargetObject->getPosition();

                    break;
                  }
	              }
              }
            }



            //lets keep a constant barrage of gattling bullets on the current aim location
				    if( gattlingAI )
				    {
              if ( validTargetObject )// either in the reticle or the targeting area
                gattlingAI->aiAttackObject( validTargetObject, LOTS_OF_SHOTS, CMD_FROM_AI );
              else
  					    gattlingAI->aiAttackPosition( &m_gattlingTargetPosition, LOTS_OF_SHOTS, CMD_FROM_AI );
				    }




            // IF THE GATTLING GUN HAS BEEN PLINKING THE TARGET LONG ENOUGH, LETS FOLLOW WITH A VOLLEY OF HOWITZER FIRE
            if ( m_okToFireHowitzerCounter > data->m_howitzerFollowLag )
            {
              WeaponTemplate *wt = data->m_howitzerWeaponTemplate;
              if( wt )
              {
                Coord3D attackPositionWithRandomOffset;
                Real offs = data->m_randomOffsetForHowitzer;
                attackPositionWithRandomOffset.x = m_gattlingTargetPosition.x + GameLogicRandomValue( -offs, offs );
                attackPositionWithRandomOffset.y = m_gattlingTargetPosition.y + GameLogicRandomValue( -offs, offs );
                attackPositionWithRandomOffset.z = m_gattlingTargetPosition.z;
	              TheWeaponStore->createAndFireTempWeapon( wt, gunship, &attackPositionWithRandomOffset );

                m_howitzerFireSound.setObjectID(gunship->getID());
                TheAudio->addAudioEvent( &m_howitzerFireSound );

              }
            }


          }





          // GATTLING TARGETING LOGIC------------------------------------------
				  const ParticleSystemTemplate *tmp = data->m_gattlingStrafeFXParticleSystem;
				  if (tmp && gattling && gattling->testStatus( OBJECT_STATUS_IS_FIRING_WEAPON) )
				  {



            // I am going to wind my gattling gun toward the next good spot,
            //whether an object's position or a cursor position


            Coord3D delta = m_positionToShootAt;
            delta.sub( m_gattlingTargetPosition );
            Real dist = delta.length();
            if ( dist < data->m_strafingIncrement )
            {
              m_gattlingTargetPosition = m_positionToShootAt;
              ++m_okToFireHowitzerCounter;
            }
            else
            {
              m_okToFireHowitzerCounter = ZERO;
              delta.normalize();
              delta.scale( data->m_strafingIncrement );
              m_gattlingTargetPosition.add( delta );
            }


			const Player *localPlayer = rts::getObservedOrLocalPlayer();

			//Make sure the gunship is visible to the player before drawing effects.
			if ( gunship->getShroudedStatus( localPlayer->getPlayerIndex() ) <= OBJECTSHROUD_PARTIAL_CLEAR )
			{

				// This makes the client smoke effects of the gattling cannon strafing the ground toward the attack position
						  ParticleSystem *sys = TheParticleSystemManager->createParticleSystem(tmp);
						  if (sys)
				{
				  Coord3D impactPosition;
				  impactPosition.x = m_gattlingTargetPosition.x + GameClientRandomValueReal( -5.0f, 5.0f );
				  impactPosition.y = m_gattlingTargetPosition.y + GameClientRandomValueReal( -5.0f, 5.0f );
				  impactPosition.z = TheTerrainLogic->getGroundHeight( impactPosition.x, impactPosition.y );
							  sys->setPosition( &impactPosition );

				}
			}

          }
        }


      }
      else if ( m_status == GUNSHIP_STATUS_DEPARTING )
      {
        if ( isPointOffMap( *gunship->getPosition() ) )
        {

          TheGameLogic->destroyObject( gunship );
          setLogicalStatus( GUNSHIP_STATUS_IDLE );

          cleanUp();

          // HERE WE NEED TO CLEAN UP THE TERRAIN DECALS, AND ANYTHING ELSE THAT WAS CREATED IN INIT-INTENT[]

        }
      }

   }
   else if ( m_status != GUNSHIP_STATUS_IDLE )
   {
     //OH MY GOODNESS, THE GUNSHIP MUST HAVE GOTTEN SHOT DOWN!
      setLogicalStatus( GUNSHIP_STATUS_IDLE );

      cleanUp();
   }










	return UPDATE_SLEEP_NONE;

}


//-------------------------------------------------------------------------------------------------
void SpectreGunshipUpdate::friend_enableAfterburners(Bool v)
{
	Object* gunship = getObject();
	if (v)
	{
		gunship->setModelConditionState(MODELCONDITION_JETAFTERBURNER);
		if (!m_afterburnerSound.isCurrentlyPlaying())
		{
			m_afterburnerSound.setObjectID(gunship->getID());
			m_afterburnerSound.setPlayingHandle(TheAudio->addAudioEvent(&m_afterburnerSound));
		}
	}
	else
	{
		gunship->clearModelConditionState(MODELCONDITION_JETAFTERBURNER);
		if (m_afterburnerSound.isCurrentlyPlaying())
		{
			TheAudio->removeAudioEvent(m_afterburnerSound.getPlayingHandle());
		}
	}
}



Bool SpectreGunshipUpdate::isFairDistanceFromShip( Object *target )
{

  Object *gunship = getObject();
  if ( !gunship )
    return FALSE;

  if ( ! target )
    return FALSE;

  const Coord3D *targetPosition = target->getPosition();
  const Coord3D *gunshipPosition = gunship->getPosition();

  Coord3D shipToTargetDelta;
  shipToTargetDelta.x = gunshipPosition->x - targetPosition->x;
  shipToTargetDelta.y = gunshipPosition->y - targetPosition->y;
  shipToTargetDelta.z = 0.0f;

  return (shipToTargetDelta.length() > getSpectreGunshipUpdateModuleData()->m_gunshipOrbitRadius * 0.75f );

}







//-------------------------------------------------------------------------------------------------
void SpectreGunshipUpdate::cleanUp()
{
  m_attackAreaDecal.clear();
  m_targetingReticleDecal.clear();


  Object *gattling = TheGameLogic->findObjectByID( m_gattlingID );
  if ( gattling )
    TheGameLogic->destroyObject( gattling );

#if defined TRACKERS
	 m_howitzerTrackerDecal.clear();
#endif
}





// --------------------------------------------------------------------------------------------------
void SpectreGunshipUpdate::disengageAndDepartAO( Object *gunship )
{

  if ( gunship == nullptr )
    return;

  AIUpdateInterface *shipAI = gunship->getAIUpdateInterface();

  if ( shipAI)
  {
    Coord3D exitPoint;// head off the map in the direction you are facing
    gunship->getUnitDirectionVector3D( exitPoint );
    Real mapSize = 99999.0f;
    exitPoint.x *= mapSize;
    exitPoint.y *= mapSize;
    exitPoint.add( *gunship->getPosition() );

    shipAI->aiMoveToPosition( &exitPoint, CMD_FROM_AI );



  }


    Object *gattling = TheGameLogic->findObjectByID( m_gattlingID );
    if ( gattling )
      gattling->setDisabled ( DISABLED_PARALYZED );


    if ( shipAI)
    {
      shipAI->chooseLocomotorSet( LOCOMOTORSET_PANIC );
	    shipAI->getCurLocomotor()->setAllowInvalidPosition(TRUE);
	    shipAI->getCurLocomotor()->setUltraAccurate(TRUE);	// set ultra-accurate just so AI won't try to adjust our dest
    }

    Drawable *draw = gunship->getDrawable();
    if ( draw )
      draw->clearAndSetModelConditionState( MODELCONDITION_DOOR_1_OPENING, MODELCONDITION_DOOR_1_CLOSING );
          friend_enableAfterburners(TRUE);


  cleanUp();

}




//-------------------------------------------------------------------------------------------------
Bool SpectreGunshipUpdate::doesSpecialPowerHaveOverridableDestinationActive() const
{
	 return m_status < GUNSHIP_STATUS_DEPARTING;
}


// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void SpectreGunshipUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: Can't flat-save decals, that's a hella crash (they aren't saved (no memory) and they have a pointer in them).  And half the class wasn't saved.
*/
// ------------------------------------------------------------------------------------------------
void SpectreGunshipUpdate::xfer( Xfer *xfer )
{
//	const SpectreGunshipUpdateModuleData *data = getSpectreGunshipUpdateModuleData();

	// version
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );




  // The initial target destination.
	xfer->xferCoord3D( &m_initialTargetPosition );
	// The manual override target destination.
	xfer->xferCoord3D( &m_overrideTargetDestination );
	// The current move-to point of the gunship.
	xfer->xferCoord3D( &m_satellitePosition );
	// status
	xfer->xferUser( &m_status, sizeof( GunshipStatus ) );

  xfer->xferUnsignedInt( &m_orbitEscapeFrame );


	if( version < 2 )
	{
		xfer->xferUser( &m_attackAreaDecal, sizeof( RadiusDecal ) );
		xfer->xferUser( &m_targetingReticleDecal, sizeof( RadiusDecal ) );

#if defined TRACKERS
		xfer->xferUser( &m_howitzerTrackerDecal, sizeof( RadiusDecal ) );
#endif
	}

	if( version >= 2 )
	{
		xfer->xferCoord3D( &m_gattlingTargetPosition );
		xfer->xferCoord3D( &m_positionToShootAt );
	  xfer->xferUnsignedInt( &m_okToFireHowitzerCounter );
		xfer->xferObjectID( &m_gattlingID );
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void SpectreGunshipUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
