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

// FILE: WaveGuideUpdate.cpp //////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2002
// Desc:   Update module for the waver wave objects to create flooding
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Radar.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/TerrainVisual.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/WaveGuideUpdate.h"
#include "GameLogic/Module/ToppleUpdate.h"

// DEFINES ////////////////////////////////////////////////////////////////////////////////////////
#define PATH_EXTRA_DISTANCE (10 * PATHFIND_CELL_SIZE_F)

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WaveGuideUpdateModuleData::WaveGuideUpdateModuleData()
{
	m_bridgeParticleAngleFudge = 0.0f;
	m_randomSplashSoundFrequency = 0;
	m_waveDelay = 0.0f;
	m_ySize = 0.0f;
	m_linearWaveSpacing = 0.0f;
	m_waveBendMagnitude = 0.0f;
	m_waterVelocity = 0.0f;
	m_preferredHeight = 0.0f;
	m_shorelineEffectDistance = 0.0f;
	m_damageRadius = 0.0f;
	m_damageAmount = 0.0f;
	m_toppleForce = 0.0f;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void WaveGuideUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{

  UpdateModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{

		{ "WaveDelay", INI::parseDurationReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_waveDelay ) },
		{ "YSize", INI::parseReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_ySize ) },
		{ "LinearWaveSpacing", INI::parseReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_linearWaveSpacing ) },
		{ "WaveBendMagnitude", INI::parseReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_waveBendMagnitude ) },
		{ "WaterVelocity", INI::parseVelocityReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_waterVelocity ) },
		{ "PreferredHeight", INI::parseReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_preferredHeight ) },
		{ "ShorelineEffectDistance", INI::parseReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_shorelineEffectDistance ) },
		{ "DamageRadius", INI::parseReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_damageRadius ) },
		{ "DamageAmount", INI::parseReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_damageAmount ) },
		{ "ToppleForce", INI::parseReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_toppleForce ) },
		{ "RandomSplashSound", INI::parseAudioEventRTS, nullptr, offsetof( WaveGuideUpdateModuleData, m_randomSplashSound ) },
		{ "RandomSplashSoundFrequency", INI::parseInt, nullptr, offsetof( WaveGuideUpdateModuleData, m_randomSplashSoundFrequency ) },
		{ "BridgeParticle",	INI::parseParticleSystemTemplate, nullptr, offsetof( WaveGuideUpdateModuleData, m_bridgeParticle ) },
		{ "BridgeParticleAngleFudge", INI::parseAngleReal, nullptr, offsetof( WaveGuideUpdateModuleData, m_bridgeParticleAngleFudge ) },
		{ "LoopingSound", INI::parseAudioEventRTS, nullptr, offsetof( WaveGuideUpdateModuleData, m_loopingSound ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  p.add(dataFieldParse);

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WaveGuideUpdate::WaveGuideUpdate( Thing *thing, const ModuleData *moduleData )
							  :UpdateModule( thing, moduleData )
{

//	Object *waveGuide = getObject();
	// water waves objects are disabled by default when they are created
// doh, except that (for now) you can't disable something in its ctor
//waveGuide->setDisabled();
	m_needDisable = true;
	m_activeFrame = 0;
	m_initialized = FALSE;
	m_shapePointCount = 0;
	m_splashSoundFrame = 0;
	m_finalDestination.zero();

	for( Int i = 0; i < MAX_WAVEGUIDE_SHAPE_POINTS; i++ )
	{

		m_shapePoints[ i ].zero();
		m_transformedShapePoints[ i ].zero();

		for( Int j = 0; j < MAX_SHAPE_EFFECTS; j++ )
			m_shapeEffects[ i ][ j ] = INVALID_PARTICLE_SYSTEM_ID;

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WaveGuideUpdate::~WaveGuideUpdate()
{

}

// ------------------------------------------------------------------------------------------------
/** Start the waveguide moving along its waypoint path, bringing water destruction and havok
	* upon the land */
// ------------------------------------------------------------------------------------------------
Bool WaveGuideUpdate::startMoving()
{
	Object *waveGuide = getObject();
	Waypoint *waypoint;
	const WaveGuideUpdateModuleData *data = getWaveGuideUpdateModuleData();

	AudioEventRTS loopingSound = data->m_loopingSound;
	loopingSound.setObjectID( getObject()->getID() );
	TheAudio->addAudioEvent( &loopingSound );

	waypoint = TheTerrainLogic->getWaypointByName( "WaveGuide1" );
	if( waypoint )
	{

		//
		// traverse the entire waypoint path, there must be one link in the path, and
		// at any of the links there cannot be multiple choices of where to go next
		//
		Waypoint *verify = waypoint;
		while( verify )
		{

			// can't have more than one link
			if( verify->getNumLinks() > 1 )
			{

				DEBUG_CRASH(( "WaveGuideUpdate::startMoving - The waypoint path cannot have multiple link choices at any node" ));
				return FALSE;

			}

			// set our destination location to this waypoint position
			m_finalDestination = *verify->getLocation();

			// on to the next
			verify = verify->getLink( 0 );

		}

		// there must be at least one link
		Waypoint *next = waypoint->getLink( 0 );
		if( next == nullptr )
		{

			DEBUG_CRASH(( "WaveGuideUpdate:startMoving - There must be a linked waypoint path to follow" ));
			return FALSE;

		}

		// get vector from next waypoint to first waypoint
		Coord2D v;
		v.x = next->getLocation()->x - waypoint->getLocation()->x;
		v.y = next->getLocation()->y - waypoint->getLocation()->y;

		// turn vector into angle
		Real angle = v.toAngle();

		// orient the waveguide the same direction
		waveGuide->setOrientation( angle );

		// get the ai update interface for the waveguide
		AIUpdateInterface *ai = waveGuide->getAIUpdateInterface();
		if( ai )
		{

			// instantly move to waypoint location on the terrain
			Coord3D pos;
			pos.x = waypoint->getLocation()->x;
			pos.y = waypoint->getLocation()->y;
			pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );
			waveGuide->setPosition( &pos );

			// follow the waypoint path here
 			ai->aiFollowWaypointPath( waypoint, CMD_FROM_AI );

			//
			// make sure it doesn't stop or slow down at each waypoint along the path if the path
			// we're following consists of multiple nodes
			//
			ai->setPathExtraDistance( PATH_EXTRA_DISTANCE );

		}

	}

	return TRUE;  // all is well

}

// ------------------------------------------------------------------------------------------------
/** The wave guide has started moving ... this is called once */
// ------------------------------------------------------------------------------------------------
Bool WaveGuideUpdate::initWaveGuide()
{

	// start the waveguide movement
	if( startMoving() == FALSE )
		return FALSE;  // don't begin moving

	// compute the wave shape points
	computeWaveShapePoints();

	static const ParticleSystemTemplate *wave1 = TheParticleSystemManager->findTemplate( "WaveSpray01" );
	static const ParticleSystemTemplate *wave2 = TheParticleSystemManager->findTemplate( "WaveSpray02" );
	static const ParticleSystemTemplate *wave3 = TheParticleSystemManager->findTemplate( "WaveSpray03" );
	ParticleSystem *particleSys;

	// create wavespray particle system and attach to object
	for( Int i = 0; i < m_shapePointCount; i++ )
	{

		// create spray 1 effect
		particleSys = TheParticleSystemManager->createParticleSystem( wave1 );
		if( particleSys )
		{

			particleSys->setPosition( &m_shapePoints[ i ] );
			particleSys->attachToObject( getObject() );
			m_shapeEffects[ i ][ 0 ] = particleSys->getSystemID();

		}

		// create spray 2 effect
		particleSys = TheParticleSystemManager->createParticleSystem( wave2 );
		if( particleSys )
		{

			particleSys->setPosition( &m_shapePoints[ i ] );
			particleSys->attachToObject( getObject() );
			m_shapeEffects[ i ][ 1 ] = particleSys->getSystemID();

		}

		// create spray 3 every few points across the wave
		if( i % 5  == 0 )
		{

			particleSys = TheParticleSystemManager->createParticleSystem( wave3 );
			if( particleSys )
			{

				particleSys->setPosition( &m_shapePoints[ i ] );
				particleSys->attachToObject( getObject() );
				m_shapeEffects[ i ][ 2 ] = particleSys->getSystemID();

			}

		}

	}

	return TRUE;  // all is well

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::computeWaveShapePoints()
{

	// get the module data
	const WaveGuideUpdateModuleData *modData = getWaveGuideUpdateModuleData();

	//
	// build an array of points that will represent the wavefront.  these points are
	// created in local object space and can be put through the transform matrix of the
	// object to get the world position given the waveguides' current position and orientation
	//
	Int halfY = modData->m_ySize / 2.0f;

	for( Int y = -halfY; y < halfY; y += modData->m_linearWaveSpacing )
	{

		// sanity
		if( m_shapePointCount >= MAX_WAVEGUIDE_SHAPE_POINTS )
			break;  // exit for

		//
		// if there is a bend magnitude we will use a form of "y = x^2" to make a parabola
		// that forms the wavefront ... otherwise we just make a straight line
		//
		if( modData->m_waveBendMagnitude )
			m_shapePoints[ m_shapePointCount ].x = -(y * y) / modData->m_waveBendMagnitude;
		else
			m_shapePoints[ m_shapePointCount ].x = 0.0f;
		m_shapePoints[ m_shapePointCount ].y = y;
		m_shapePoints[ m_shapePointCount ].z = 0.0f;
		m_shapePointCount++;

	}

}

// ------------------------------------------------------------------------------------------------
/** Given the current position and orientation of the wave guide, transform all the wave
	* shape points so we can quickly access them for multiple reasons */
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::transformWaveShape()
{
	Int i;
	Object *waveGuide = getObject();

	for( i = 0; i < m_shapePointCount; i++ )
	{

		// transform the point
		waveGuide->transformPoint( &m_shapePoints[ i ], &m_transformedShapePoints[ i ] );

		// the Z of the transformed point will be on the terrain
		m_transformedShapePoints[ i ].z = TheTerrainLogic->getGroundHeight( m_transformedShapePoints[ i ].x,
																																	m_transformedShapePoints[ i ].y );

	}

}

// ------------------------------------------------------------------------------------------------
/** Update phase for the effects that make up the front shape of the wave */
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::doShapeEffects()
{
	const WaveGuideUpdateModuleData *modData = getWaveGuideUpdateModuleData();

	//
	// the particle systems that make up the wave shape need to maintain a position that is
	// just above the ground
	//
	Int i, j;
	ParticleSystem *particleSys;
	Coord3D pos;
	for( i = 0; i < m_shapePointCount; i++ )
	{

		for( j = 0; j < MAX_SHAPE_EFFECTS; j++ )
		{

			// get the particle system here
			particleSys = TheParticleSystemManager->findParticleSystem( m_shapeEffects[ i ][ j ] );
			if( particleSys )
			{

				//
				// we have the height of the terrain at this shape point in the transformed shape
				// point array ... just set the LOCAL position for the particle system to reflect
				// this Z *IF* that Z will not take us above the target water height
				//
				if( m_transformedShapePoints[ i ].z < modData->m_preferredHeight )
				{

					particleSys->getPosition( &pos );
					pos.z = m_transformedShapePoints[ i ].z;
					particleSys->setPosition( &pos );

				}

			}

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Given all our sample points, make the wave go */
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::doWaterMotion()
{
//	Object *waveGuide = getObject();

	// get the module data for the wave guide
	const WaveGuideUpdateModuleData *modData = getWaveGuideUpdateModuleData();

	// iterate the waveguide shape points ... at certain points we will create water "up force"
	Int i;
//	const Matrix3D *transform = waveGuide->getTransformMatrix();
	for( i = 0; i < m_shapePointCount; i++ )
	{

		// push up the water here
		TheTerrainVisual->addWaterVelocity( m_transformedShapePoints[ i ].x,
																				m_transformedShapePoints[ i ].y,
																				modData->m_waterVelocity,
																				modData->m_preferredHeight );


	}

}

// ------------------------------------------------------------------------------------------------
/** Any points in our wave that are on the shoreline that are close enough to the effect
	* points for the shore will do those effects */
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::doShoreEffects()
{

	// this is expensive to do *all* the time
	if( TheGameLogic->getFrame() & 0x1 )
		return;

	Object *waveGuide = getObject();
	Int i;

	// get module data
	const WaveGuideUpdateModuleData *modData = getWaveGuideUpdateModuleData();

	//
	// setup an array of points just behind the front of the wave where the first wave
	// "crest" is rising up ... we will use these points to make collide with shore effects
	//
	Coord3D effectPoints[ MAX_WAVEGUIDE_SHAPE_POINTS ];
	for( i = 0; i < m_shapePointCount; i++ )
	{

		// setup point to be a distance "behind" the wave shape points
		effectPoints[ i ].x = m_shapePoints[ i ].x - modData->m_shorelineEffectDistance;
		effectPoints[ i ].y = m_shapePoints[ i ].y;
		effectPoints[ i ].z = m_shapePoints[ i ].z;

		// transform the point
		waveGuide->transformPoint( &effectPoints[ i ], &effectPoints[ i ] );

	}

	//
	// go across the shape of our wave ... when we detect a transition from underground to
	// above ground or vice verse, that is a shoreline ... play an effect between those points
	//
	static const ParticleSystemTemplate *left = TheParticleSystemManager->findTemplate( "WaveSplashLeft01" );
	static const ParticleSystemTemplate *right = TheParticleSystemManager->findTemplate( "WaveSplashRight01" );
	ParticleSystem *particleSystem;
	Real terrainZ;
	Bool underWater = TRUE;
	Coord3D *point;
	for( i = 0; i < m_shapePointCount; i++ )
	{

		// get the point in question
		point = &effectPoints[ i ];

		// get terrain height at this point
		terrainZ = TheTerrainLogic->getGroundHeight( point->x, point->y );

		// is the terrain at this point below or above the preferred water height
		if( terrainZ > modData->m_preferredHeight )
		{

			//
			// if we were previously 'under' we are not any longer, this is a shore
			// note that we're ignoring the first point i = 0
			//
			if( underWater == TRUE && i != 0 )
			{
				Coord3D *prevPoint = &effectPoints[ i - 1 ];  // the prev point is actually on the water so we'll use it

				particleSystem = TheParticleSystemManager->createParticleSystem( right );
				if( particleSystem )
					particleSystem->setPosition( prevPoint );

			}

			// we are now 'above'
			underWater = FALSE;

		}
		else
		{

			//
			// if we were previously 'above' we are not any longer, this is a shore
			// note that we're ignoring the first point i = 0
			//
			if( underWater == FALSE && i != 0 )
			{

				particleSystem = TheParticleSystemManager->createParticleSystem( left );
				if( particleSystem )
					particleSystem->setPosition( point );

			}

			// we are now 'under'
			underWater = TRUE;

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Do damage to things that have fallen victim in the path of this enormous wave */
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::doDamage()
{
	Object *waveGuide = getObject();
	const WaveGuideUpdateModuleData *modData = getWaveGuideUpdateModuleData();

	// get our position forward unit direction vector
//	const Coord3D *unitForward = waveGuide->getUnitDirectionVector2D();

	// iterate over all our sample points and kill stuff around us
	for( Int i = 0; i < m_shapePointCount; i++ )
	{

		// scan objects around us and do damage to objects we have "passed over" and are behind us
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( &m_transformedShapePoints[ i ],
																																			 modData->m_damageRadius,
																																			 FROM_CENTER_2D,
																																			 nullptr );
		MemoryPoolObjectHolder hold( iter );
		Object *obj;
		const Coord3D *objPos;
		Coord3D v;
		Real angle;
		for( obj = iter->first(); obj; obj = iter->next() )
		{

			// ignore all waveguide types
			if( obj->isKindOf( KINDOF_WAVEGUIDE ) )
				continue;

			//
			// ignore bridge towers ... water waves will destroy bridges by colliding with
			// the actual bridge object in the *center* of the bridge, not the towers
			//
			if( obj->isKindOf( KINDOF_BRIDGE_TOWER ) )
				continue;

			// get other object position
			objPos = obj->getPosition();

			//
			// only damage objects that are below the preferred height of the wave doing the damage,
			// bridges are an exception as their object is raised above the ground, but we'll
			// say the water is destroying the foundation of it
			//
			if( objPos->z > modData->m_preferredHeight && obj->isKindOf( KINDOF_BRIDGE ) == FALSE )
				continue;

			// get the vector from us to the object
			v.x = objPos->x - m_transformedShapePoints[ i ].x;
			v.y = objPos->y - m_transformedShapePoints[ i ].y;
			v.z = 0.0f;		// forget Z, this is really a top down 2D calculation
			v.normalize();

			//
			// get the cosine of the angle between the our forward direction and the vector to the obj
			// otherwise known as a dot product
			//
			angle = v.x * m_transformedShapePoints[ i ].x +
							v.y * m_transformedShapePoints[ i ].y +
							v.z * m_transformedShapePoints[ i ].z;

			//
			// we will only do damage if the object is "behind" the wave ... that is, behind
			// the 180 degrees mark behind us which we figure out by seeing if the cosine of
			// the angle between us (dot product) is less than zero
			//
			if( angle < 0 )
			{

				// if object was not wet before we kill it and play effects
				if( !obj->getStatusBits().test( OBJECT_STATUS_WET ) )
				{
					static const ParticleSystemTemplate *splash = TheParticleSystemManager->findTemplate( "WaveHit01" );
					ParticleSystem *particleSystem;

					// create particle system at position
					particleSystem = TheParticleSystemManager->createParticleSystem( splash );
					if( particleSystem )
					{
						Coord3D pos;

						//
						// we will use the position of the shape point (in local space) with the
						// Z of the object we're killin
						//
						pos.x = m_shapePoints[ i ].x;
						pos.y = m_shapePoints[ i ].y;
						pos.z = obj->getPosition()->z;
						particleSystem->setPosition( &pos );
						particleSystem->attachToObject( waveGuide );

					}

					// this object is now wet
					obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_WET ) );

					// some things can be toppled ... ooo, xtra special of us!
					Coord3D toppleVector;
					toppleVector.x = obj->getPosition()->x - m_transformedShapePoints[ i ].x;
					toppleVector.y = obj->getPosition()->y - m_transformedShapePoints[ i ].y;
					toppleVector.z = 0;
					obj->topple( &toppleVector, modData->m_toppleForce, TOPPLE_OPTIONS_NO_BOUNCE |
																															TOPPLE_OPTIONS_NO_FX );

					// do a lot of water damage
					DamageInfo damageInfo;
					damageInfo.in.m_damageType = DAMAGE_WATER;
					damageInfo.in.m_deathType = DEATH_FLOODED;
					damageInfo.in.m_sourceID = waveGuide->getID();
					damageInfo.in.m_amount = modData->m_damageAmount;
					obj->attemptDamage( &damageInfo );

					//
					// set flooded condition for model and turn off shadows cause we're inside water,
					// note that we want to do this after we damage the object so that we switch
					// off the shadows on the new model data loaded after damage is dealt
					//
					Drawable *draw = obj->getDrawable();
					if( draw )
					{

						draw->setModelConditionState( MODELCONDITION_FLOODED );
						draw->setShadowsEnabled( FALSE );

					}

					//
					// Temp demo hack, replace bridges destroyed with the special destroyed bridge art
					//
					if( obj->isKindOf( KINDOF_BRIDGE ) )
					{
						const ThingTemplate* ttn = TheThingFactory->findTemplate("WaterWaveBridge");
						Object *newBridge = TheThingFactory->newObject( ttn, nullptr );

						if( newBridge )
						{
							Real angle = 0.0f;

							// get the bridge represented by the object we're killing
							Bridge *oldBridge = TheTerrainLogic->findBridgeAt( obj->getPosition() );
							if( oldBridge )
							{
								BridgeInfo bridgeInfo;

								// get the bridge info
								oldBridge->getBridgeInfo( &bridgeInfo );

								// compute the angle of the object based on the angle of the bridge
								Coord2D v;
								v.x = bridgeInfo.to.x - bridgeInfo.from.x;
								v.y = bridgeInfo.to.y - bridgeInfo.from.y;
								angle = v.toAngle();

							}

							// put new bridge looking object in the world
							newBridge->setPosition( obj->getPosition() );
							newBridge->setOrientation( angle );

							// create and attach a wave hit bridge particle system to the new object
							ParticleSystem *particleSystem = TheParticleSystemManager->createParticleSystem( modData->m_bridgeParticle );
							if( particleSystem )
							{
								Coord3D u, x, y, z;
								Matrix3D transform;

								z.x = 0.0f;
								z.y = 0.0f;
								z.z = 1.0f;

								//
								// angle is rotated, because we computed from 'from' and 'to' points of
								// the bridge going *across* the valley, not pointing *down* it
								//
								u.x = Cos( angle + modData->m_bridgeParticleAngleFudge );
								u.y = Sin( angle + modData->m_bridgeParticleAngleFudge );
								u.z = 0.0f;

								y.crossProduct( z, u, y );
								x.crossProduct( y, z, x );

								transform.Set(  x.x, y.x, z.x, obj->getPosition()->x,
																x.y, y.y, z.y, obj->getPosition()->y,
																x.z, y.z, z.z, obj->getPosition()->z );

								particleSystem->setLocalTransform( &transform );

							}

							// destroy the old bridge and bridge object
							TheTerrainLogic->deleteBridge( oldBridge );

						}

					}

				}

			}

		}

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime WaveGuideUpdate::update()
{
/// @todo srj use SLEEPY_UPDATE here
	Object *waveGuide = getObject();

	if (m_needDisable)
	{
		m_needDisable = false;
		getObject()->setDisabled( DISABLED_DEFAULT );
		return UPDATE_SLEEP_NONE;
	}

	// do nothing if we're disabled
	if( waveGuide->isDisabled() )
		return UPDATE_SLEEP_NONE;

	// get module data
	const WaveGuideUpdateModuleData *modData = getWaveGuideUpdateModuleData();

	// set the frame we became active on if not set
	if( m_activeFrame == 0 )
		m_activeFrame = TheGameLogic->getFrame();

	// if we're waiting for a delay check it and get out of here
	if( TheGameLogic->getFrame() - m_activeFrame < modData->m_waveDelay )
		return UPDATE_SLEEP_NONE;

	// get our position
//	const Coord3D *waveGuidePos = waveGuide->getPosition();

	// when we become enabled we will start particle effects for our wave
	if( m_initialized == FALSE )
	{

		// start the effects we need
		if( initWaveGuide() == FALSE )
		{

			// initialization failed, destroy object and get out of here
			TheGameLogic->destroyObject( getObject() );
			return UPDATE_SLEEP_NONE;

		}

		// we are now in motion
		m_initialized = TRUE;

	}

	// every half second we try to play a random splash sound
	if( TheGameLogic->getFrame() - m_splashSoundFrame > static_cast<float>(LOGICFRAMES_PER_SECOND) / 2.0f )
	{

		// mark that we've had the opportunity to play
		m_splashSoundFrame = TheGameLogic->getFrame();

		// pick a random number and play according to frequency
		if( GameLogicRandomValue( 1, 100 ) > modData->m_randomSplashSoundFrequency )
		{
			AudioEventRTS randomSplash(modData->m_randomSplashSound);
			randomSplash.setObjectID(waveGuide->getID());
			TheAudio->addAudioEvent(&randomSplash);
		}
	}

	//
	// transform the wave shape points once for the current position ... we have this array
	// because we access this data several times and don't want to recompute it every time
	//
	transformWaveShape();

	// see if we are close enough to the end of our journey on the waypoint path
	const Coord3D *currentPos = waveGuide->getPosition();
	Real distSquared = PATH_EXTRA_DISTANCE * PATH_EXTRA_DISTANCE;
	Coord2D v;
	v.x = m_finalDestination.x - currentPos->x;
	v.y = m_finalDestination.y - currentPos->y;
	if( v.x * v.x + v.y * v.y <= distSquared )
	{
		static const ParticleSystemTemplate *waveSplash = TheParticleSystemManager->findTemplate( "WaveSplash01" );
		ParticleSystem *particleSys;

		// create splash effect
		particleSys = TheParticleSystemManager->createParticleSystem( waveSplash );
		if( particleSys )
			particleSys->setLocalTransform( waveGuide->getTransformMatrix() );

		// destroy object
		TheGameLogic->destroyObject( waveGuide );

		// update the radar with the final new water levels
		TheRadar->refreshTerrain( TheTerrainLogic );

		return UPDATE_SLEEP_NONE;

	}

	// do wavefront effects
	doShapeEffects();

	// do water effects
	doWaterMotion();

	// do shore effects
	doShoreEffects();

	// do damage to things in our way
	doDamage();

	return UPDATE_SLEEP_NONE;

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// active frame
	xfer->xferUnsignedInt( &m_activeFrame );

	// need disable
	xfer->xferBool( &m_needDisable );

	// initialized
	xfer->xferBool( &m_initialized );

	// shape points
	xfer->xferUser( m_shapePoints, sizeof( Coord3D ) * MAX_WAVEGUIDE_SHAPE_POINTS );

	// transformed shape points
	xfer->xferUser( &m_transformedShapePoints, sizeof( Coord3D ) * MAX_WAVEGUIDE_SHAPE_POINTS );

	// particle shape effects
	xfer->xferUser( m_shapeEffects, sizeof( ParticleSystemID ) * MAX_WAVEGUIDE_SHAPE_POINTS * MAX_SHAPE_EFFECTS );

	// shape point count
	xfer->xferInt( &m_shapePointCount );

	// splash sound frame
	xfer->xferUnsignedInt( &m_splashSoundFrame );

	// final destination
	xfer->xferCoord3D( &m_finalDestination );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WaveGuideUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
