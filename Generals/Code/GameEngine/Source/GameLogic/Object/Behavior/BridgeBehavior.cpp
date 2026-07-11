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

// FILE: BridgeBehavior.cpp ///////////////////////////////////////////////////////////////////////
// Author: Colin Day, July 2002
// Desc:   Behavior module for bridges
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Radar.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/InGameUI.h"
#include "GameClient/FXList.h"
#include "GameClient/Line2D.h"
#include "GameClient/TerrainRoads.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/BridgeBehavior.h"
#include "GameLogic/Module/BridgeScaffoldBehavior.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
BridgeBehaviorModuleData::BridgeBehaviorModuleData()
{

	m_lateralScaffoldSpeed = 1.0f;
	m_verticalScaffoldSpeed = 1.0f;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
BridgeBehaviorModuleData::~BridgeBehaviorModuleData()
{

	// clear the fx list
	m_fx.clear();

	// clear the ocl list
	m_ocl.clear();

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void BridgeBehaviorModuleData::buildFieldParse( MultiIniFieldParse &p )
{
  BehaviorModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "LateralScaffoldSpeed",			INI::parseVelocityReal, nullptr, offsetof( BridgeBehaviorModuleData, m_lateralScaffoldSpeed ) },
		{ "VerticalScaffoldSpeed",		INI::parseVelocityReal, nullptr, offsetof( BridgeBehaviorModuleData, m_verticalScaffoldSpeed ) },
		{ "BridgeDieFX",		parseFX,		nullptr,			offsetof( BridgeBehaviorModuleData, m_fx ) },
		{ "BridgeDieOCL",		parseOCL,		nullptr,			offsetof( BridgeBehaviorModuleData, m_ocl ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  p.add( dataFieldParse );

}

// ------------------------------------------------------------------------------------------------
/** Parse time and location info in the form of:
	* Delay:#### <Bone:BoneName> */
// ------------------------------------------------------------------------------------------------
static void parseTimeAndLocationInfo( INI *ini, void *instance,
																			TimeAndLocationInfo *timeAndLocationInfo )
{

	// delay label
	const char *token = ini->getNextToken( ini->getSepsColon() );
	if( stricmp( token, "Delay" ) != 0 )
	{

		DEBUG_CRASH(( "Expected 'Delay' token, found '%s'", token ));
		throw INI_INVALID_DATA;

	}

	// delay value
	ini->parseDurationUnsignedInt( ini, instance, &timeAndLocationInfo->delay, nullptr );

	// get optional bone label
	token = ini->getNextTokenOrNull( ini->getSepsColon() );
	if( token )
	{

		// token must be a label for bone location
		if( stricmp( token, "Bone" ) != 0 )
		{

			DEBUG_CRASH(( "Expected 'Bone' token, found '%s'", token ));
			throw INI_INVALID_DATA;

		}

		// read bone name and store
		timeAndLocationInfo->boneName = ini->getNextAsciiString();

	}

}

//-------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void BridgeBehaviorModuleData::parseFX( INI *ini,
											 														 void *instance,
																									 void *store,
																									 const void *userData )
{
	const char *token;

	// create item we will read into and push on list
	BridgeFXInfo item;
	item.fx = nullptr;

	// get list to store at
	BridgeFXList *bridgeFXList = (BridgeFXList *)store;

	// fx list label
	token = ini->getNextToken( ini->getSepsColon() );
	if( stricmp( token, "FX" ) != 0 )
	{

		DEBUG_CRASH(( "Expected 'FX' token, found '%s'", token ));
		throw INI_INVALID_DATA;

	}

	// fx list name and store as pointer
	FXList *fx;
	ini->parseFXList( ini, instance, &fx, nullptr );

	// store fx list to item
	item.fx = fx;

	// parse the timing and location info
	parseTimeAndLocationInfo( ini, instance, &item.timeAndLocationInfo );

	// put on list
	bridgeFXList->push_back( item );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void BridgeBehaviorModuleData::parseOCL( INI *ini,
																										void *instance,
																										void *store,
																										const void *userData )
{
	const char *token;

	// create item we will read into and push on list
	BridgeOCLInfo item;
	item.ocl = nullptr;

	// get list to store at
	BridgeOCLList *bridgeOCLList = (BridgeOCLList *)store;

	// fx list label
	token = ini->getNextToken( ini->getSepsColon() );
	if( stricmp( token, "OCL" ) != 0 )
	{

		DEBUG_CRASH(( "Expected 'OCL' token, found '%s'", token ));
		throw INI_INVALID_DATA;

	}

	// fx list name and store as pointer
	ObjectCreationList *ocl;
	ini->parseObjectCreationList( ini, instance, &ocl, nullptr );

	// store ocl list to item
	item.ocl = ocl;

	// parse the timing and location info
	parseTimeAndLocationInfo( ini, instance, &item.timeAndLocationInfo );

	// put on list
	bridgeOCLList->push_back( item );

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
BridgeBehavior::BridgeBehavior( Thing *thing, const ModuleData *moduleData )
							: UpdateModule( thing, moduleData )
{
	Int i;
	m_scaffoldObjectIDList.clear();
	m_scaffoldPresent = FALSE;

	for( i = 0; i < BRIDGE_MAX_TOWERS; ++i )
		m_towerID[ i ] = INVALID_ID;

	m_fxResolved = FALSE;
	for( Int bodyState = BODY_PRISTINE; bodyState < BODYDAMAGETYPE_COUNT; ++bodyState )
	{

		// initialize the fx and ocl lists
		for( i = 0; i < MAX_BRIDGE_BODY_FX; ++i )
		{

			m_damageToOCL[ bodyState ][ i ] = nullptr;
			m_damageToFX[ bodyState ][ i ] = nullptr;
			m_repairToOCL[ bodyState ][ i ] = nullptr;
			m_repairToFX[ bodyState ][ i ] = nullptr;

		}

		// these don't need initialization, they have constructors
		// m_damageToSound[ bodyState ] = ???
		// m_repairToSound[ bodyState ] = ???

	}

	m_deathFrame = 0;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
BridgeBehavior::~BridgeBehavior()
{

	//
	// destroy our four towers if they are still present in the world (they wouldn't be if we
	// are resetting the engine, and those towers were earlier in the destruction list than
	// we were)
	//
	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
	{
		Object *tower;

		// get the tower and destroy
		tower = TheGameLogic->findObjectByID( getTowerID( (BridgeTowerType)i ) );
		if( tower )
			TheGameLogic->destroyObject( tower );

	}

}

// ------------------------------------------------------------------------------------------------
/** Get bridge behavior interface */
// ------------------------------------------------------------------------------------------------
/*static */BridgeBehaviorInterface *BridgeBehavior::getBridgeBehaviorInterfaceFromObject( Object *obj )
{

	// sanity
	if( obj == nullptr )
		return nullptr;

	BehaviorModule **bmi;
	BridgeBehaviorInterface *bbi = nullptr;
	for( bmi = obj->getBehaviorModules(); *bmi; ++bmi )
	{

		bbi = (*bmi)->getBridgeBehaviorInterface();
		if( bbi )
			return bbi;

	}

	// interface not found
	return nullptr;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::onDelete()
{

	// clear the list of scaffold objects
	m_scaffoldObjectIDList.clear();

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::resolveFX()
{
	Object *us = getObject();
	Bridge *bridge = TheTerrainLogic->findBridgeAt( us->getPosition() );

	// sanity
	if( bridge == nullptr )
		return;

	// get the bridge template name
	AsciiString bridgeTemplateName = bridge->getBridgeTemplateName();

	// find the bridge template
	TerrainRoadType *bridgeTemplate = TheTerrainRoads->findBridge( bridgeTemplateName );

	// sanity
	if( bridgeTemplate == nullptr )
		return;

	AsciiString name;
	for( Int bodyState = BODY_PRISTINE; bodyState < BODYDAMAGETYPE_COUNT; ++bodyState )
	{

		// initialize the fx and ocl lists
		for( Int i = 0; i < MAX_BRIDGE_BODY_FX; ++i )
		{

			name = bridgeTemplate->getDamageToOCLString( (BodyDamageType)bodyState, i );
			m_damageToOCL[ bodyState ][ i ] = TheObjectCreationListStore->findObjectCreationList( name.str() );
			if( name.isEmpty() == FALSE && m_damageToOCL[ bodyState ][ i ] == nullptr )
				DEBUG_CRASH(( "OCL list '%s' not found", name.str() ));

			name = bridgeTemplate->getDamageToFXString( (BodyDamageType)bodyState, i );
			m_damageToFX[ bodyState ][ i ] = TheFXListStore->findFXList( name.str() );
			if( name.isEmpty() == FALSE && m_damageToFX[ bodyState ][ i ] == nullptr )
				DEBUG_CRASH(( "FX list '%s' not found", name.str() ));

			name = bridgeTemplate->getRepairedToOCLString( (BodyDamageType)bodyState, i );
			m_repairToOCL[ bodyState ][ i ] = TheObjectCreationListStore->findObjectCreationList( name.str() );
			if( name.isEmpty() == FALSE && m_repairToOCL[ bodyState ][ i ] == nullptr )
				DEBUG_CRASH(( "OCL list '%s' not found", name.str() ));

			name = bridgeTemplate->getRepairedToFXString( (BodyDamageType)bodyState, i );
			m_repairToFX[ bodyState ][ i ] = TheFXListStore->findFXList( name.str() );
			if( name.isEmpty() == FALSE && m_repairToFX[ bodyState ][ i ] == nullptr )
				DEBUG_CRASH(( "FX list '%s' not found", name.str() ));

		}

		// audio sounds
		name = bridgeTemplate->getDamageToSoundString( (BodyDamageType)bodyState );
		m_damageToSound[ bodyState ].setEventName( name );
		m_damageToSound[ bodyState ].setObjectID( us->getID() );

		name = bridgeTemplate->getRepairedToSoundString( (BodyDamageType)bodyState );
		m_repairToSound[ bodyState ].setEventName( name );
		m_repairToSound[ bodyState ].setObjectID( us->getID() );

	}

	// fx are now "resolved"
	m_fxResolved = TRUE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::setTower( BridgeTowerType towerType, Object *tower )
{

	// sanity
	if( towerType < 0 || towerType >= BRIDGE_MAX_TOWERS )
	{

		DEBUG_CRASH(( "BridgeBehavior::setTower - Invalid tower type index '%d'", towerType ));
		return;

	}

	// store it
	if( tower )
		m_towerID[ towerType ] = tower->getID();
	else
		m_towerID[ towerType ] = INVALID_ID;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
ObjectID BridgeBehavior::getTowerID( BridgeTowerType towerType )
{

	// sanity
	if( towerType < 0 || towerType >= BRIDGE_MAX_TOWERS )
	{

		DEBUG_CRASH(( "BridgeBehavior::setTower - Invalid tower type index '%d'", towerType ));
		return INVALID_ID;

	}

	// return the stored ID
	return m_towerID[ towerType ];

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::onDamage( DamageInfo *damageInfo )
{

	//
	// get our body info so we now how much damage percent is being done to us ... we need this
	// so that we can propagate the same damage percentage amount the towers and the bridge
	//
	BodyModuleInterface *body = getObject()->getBodyModule();
	Real damagePercentage = damageInfo->in.m_amount / body->getMaxHealth();

	//
	// if the damage didn't come from a bridge tower, then we must propagate this damage
	// to all our towers
	//
	Object *source = TheGameLogic->findObjectByID( damageInfo->in.m_sourceID );
	if( source == nullptr || source->isKindOf( KINDOF_BRIDGE_TOWER ) == FALSE )
	{
		Object *tower;

		for( Int i = 0; i < BRIDGE_MAX_TOWERS; i++ )
		{

			tower = TheGameLogic->findObjectByID( getTowerID( (BridgeTowerType)i ) );
			if( tower )
			{
				BodyModuleInterface *towerBody = tower->getBodyModule();
				DamageInfo towerDamage;

				towerDamage.in.m_amount = damagePercentage * towerBody->getMaxHealth();
				towerDamage.in.m_sourceID = getObject()->getID();  // we're now the source
				towerDamage.in.m_damageType = damageInfo->in.m_damageType;
				towerDamage.in.m_deathType = damageInfo->in.m_deathType;
				tower->attemptDamage( &towerDamage );

			}

		}

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::onHealing( DamageInfo *damageInfo )
{

	//
	// get our body info so we now how much healing percent is being done to us ... we need this
	// so that we can propagate the same healing percentage amount the towers and the bridge
	//
	BodyModuleInterface *body = getObject()->getBodyModule();
	Real healingPercentage = damageInfo->in.m_amount / body->getMaxHealth();

	//
	// if the healing didn't come from a bridge tower, then we must propagate this healing
	// to all our towers
	//
	Object *source = TheGameLogic->findObjectByID( damageInfo->in.m_sourceID );
	if( source == nullptr || source->isKindOf( KINDOF_BRIDGE_TOWER ) == FALSE )
	{
		Object *tower;

		for( Int i = 0; i < BRIDGE_MAX_TOWERS; i++ )
		{

			tower = TheGameLogic->findObjectByID( getTowerID( (BridgeTowerType)i ) );
			if( tower )
			{
				BodyModuleInterface *towerBody = tower->getBodyModule();
				tower->attemptHealing(healingPercentage * towerBody->getMaxHealth(), getObject());

			}

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Pick a random surface spot on the bridge surface */
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::getRandomSurfacePosition( TerrainRoadType *bridgeTemplate,
																							 const BridgeInfo *bridgeInfo,
																							 Coord3D *pos )
{

	// sanity
	if( bridgeInfo == nullptr || pos == nullptr )
		return;

	//
	// pick the spot by finding vectors along the edge of the bridge region, scaling
	// them and then adding them together
	//
	Real scale;

	Coord3D v1;
	v1.x = bridgeInfo->toLeft.x - bridgeInfo->fromLeft.x;
	v1.y = bridgeInfo->toLeft.y - bridgeInfo->fromLeft.y;
	v1.z = bridgeInfo->toLeft.z - bridgeInfo->fromLeft.z;
	scale = GameLogicRandomValueReal( 0.0f, 1.0f );
	v1.x *= scale;
	v1.y *= scale;
	v1.z *= scale;

	Coord3D v2;
	v2.x = bridgeInfo->fromRight.x - bridgeInfo->fromLeft.x;
	v2.y = bridgeInfo->fromRight.y - bridgeInfo->fromLeft.y;
	v2.z = bridgeInfo->fromRight.z - bridgeInfo->fromLeft.z;
	scale = GameLogicRandomValueReal( 0.0f, 1.0f );
	v2.x *= scale;
	v2.y *= scale;
	v2.z *= scale;

	// set the position
	pos->x = v1.x + v2.x + bridgeInfo->fromLeft.x;
	pos->y = v1.y + v2.y + bridgeInfo->fromLeft.y;
	pos->z = v1.z + v2.z + bridgeInfo->fromLeft.z;

	//
	// we now have a position picked, the last thing to do is add in an additional
	// Z component so that effects can be created in a "cube" area on and above the bridge
	//
	pos->z += GameLogicRandomValueReal( 0.0f, bridgeTemplate->getTransitionEffectsHeight() );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::doAreaEffects( TerrainRoadType *bridgeTemplate,
																		Bridge *bridge,
																		const ObjectCreationList *ocl,
																		const FXList *fx )
{

	// sanity
	if( bridge == nullptr )
		return;

	// if no effects, don't bother
	if( ocl == nullptr && fx == nullptr )
		return;

	// get bridge info
	const BridgeInfo *bridgeInfo = bridge->peekBridgeInfo();

	// play effects in the bridge area
	const Int maxEffects = bridgeTemplate->getNumFXPerType();
	Coord3D pos;
	for( Int i = 0; i < maxEffects; ++i )
	{

		// pick spot in the bridge area and do FX
		if( fx )
		{

			getRandomSurfacePosition( bridgeTemplate, bridgeInfo, &pos );
			FXList::doFXPos( fx, &pos );

		}

		// pick spot in the bridge area and do OCL
		if( ocl )
		{

			getRandomSurfacePosition( bridgeTemplate, bridgeInfo, &pos );
			ObjectCreationList::create( ocl, getObject(), &pos, nullptr );

		}

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::onBodyDamageStateChange( const DamageInfo* damageInfo,
																							BodyDamageType oldState,
																							BodyDamageType newState )
{

	//
	// check for coming back from the dead, if our new state is not the rubble state, we can't
	// possibly be dead
	//
	if( newState != BODY_RUBBLE )
		m_deathFrame = 0;

	// first resolve any fx stuff if we need to
	if( m_fxResolved == FALSE )
		resolveFX();

	// sanity
	if( m_fxResolved == FALSE )
		return;

	// sanity
	DEBUG_ASSERTCRASH( oldState != newState, ("BridgeBehavior::onBodyDamageStateChange - oldState and newState should be different if this is getting called") );

	Object *us = getObject();
	Bridge *bridge = TheTerrainLogic->findBridgeAt( us->getPosition() );

	// sanity
	if( bridge == nullptr )
	{

		DEBUG_CRASH(( "BridgeBehavior - Unable to find bridge" ));
		return;

	}

	// get the bridge template name
	AsciiString bridgeTemplateName = bridge->getBridgeTemplateName();

	// find the bridge template
	TerrainRoadType *bridgeTemplate = TheTerrainRoads->findBridge( bridgeTemplateName );

	// sanity
	DEBUG_ASSERTCRASH( bridgeTemplate, ("BridgeBehavior: Unable to find bridge template '%s' in bridge object '%s'",
																		 bridgeTemplateName.str(),
																		 us->getTemplate()->getName().str()) );

	//
	// given the old state and the new state, did we get worse (damaged) or did
	// we get better (repaired)?
	//
	Bool gotRepaired = IS_CONDITION_WORSE( oldState, newState );

	// get the effect data
	AsciiString soundString;
	AsciiString oclString[ MAX_BRIDGE_BODY_FX ];
	AsciiString fxString[ MAX_BRIDGE_BODY_FX ];
	if( gotRepaired )
	{

		// play the sound
		TheAudio->addAudioEvent( &m_repairToSound[ newState ] );

		for( Int i = 0; i < MAX_BRIDGE_BODY_FX; i++ )
			doAreaEffects( bridgeTemplate, bridge, m_repairToOCL[ newState ][ i ], m_repairToFX[ newState ][ i ] );

	}
	else
	{

		// play the sound
		TheAudio->addAudioEvent( &m_damageToSound[ newState ] );

		for( Int i = 0; i < MAX_BRIDGE_BODY_FX; i++ )
			doAreaEffects( bridgeTemplate, bridge, m_damageToOCL[ newState ][ i ], m_damageToFX[ newState ][ i ] );

	}

	// update bridge damage states
	///@todo this should be re-written, there no need for this looping bridge examination
	TheTerrainLogic->updateBridgeDamageStates();

	//
	// for the local player, if this bridge has switched from rubble, to usable, or from
	// usable to rubble, we should reflect the change on the radar.  note that we
	// request that the radar queue a refresh sometime in the future because it keeps
	// track of how often we makes requests to do a refresh and doesn't do them too
	// often because it's expensive to refresh the terrain
	//
	if( oldState == BODY_RUBBLE || newState == BODY_RUBBLE )
		TheRadar->queueTerrainRefresh();

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime BridgeBehavior::update()
{

	// if we're dead, we need to possibly throw off some effects
	if( m_deathFrame != 0 )
	{
		AsciiString boneName;

		// get object
		Object *us = getObject();

		// get module data
		const BridgeBehaviorModuleData *modData = getBridgeBehaviorModuleData();

		// get bridge information
		Bridge *bridge = TheTerrainLogic->findBridgeAt( us->getPosition() );
		const BridgeInfo *bridgeInfo = nullptr;
		TerrainRoadType *bridgeTemplate = nullptr;
		if ( bridge )
		{
			DEBUG_ASSERTCRASH( bridge, ("BridgeBehavior::update - Unable to find bridge") );

			// get bridge info
			bridgeInfo = bridge->peekBridgeInfo();

			// get the bridge template info
			AsciiString bridgeTemplateName = bridge->getBridgeTemplateName();
			bridgeTemplate = TheTerrainRoads->findBridge( bridgeTemplateName );
			DEBUG_ASSERTCRASH( bridgeTemplate, ("BridgeBehavior::getRandomSurfacePosition - Encountered a bridge with no template!") );
		}

		// how much time has passed between now and our destruction frame
		UnsignedInt deathTime = TheGameLogic->getFrame() - m_deathFrame;

		// see if there are any fx visuals we need to execute
		BridgeFXList::const_iterator fxIt;
		for( fxIt = modData->m_fx.begin(); fxIt != modData->m_fx.end(); ++fxIt )
		{

			//
			// we'll launch an fx list if our death time is equal to exactly the delay
			// we're waiting for to launch the list
			//
			if( deathTime == (*fxIt).timeAndLocationInfo.delay )
			{
				Coord3D pos;

				//
				// if a bone name is present, we'll use the bone position, otherwise we'll pick a
				// spot somewhere on the bridge surface
				//
				boneName = (*fxIt).timeAndLocationInfo.boneName;
				if( boneName.isEmpty() == FALSE )
					us->getSingleLogicalBonePosition( boneName.str(), &pos, nullptr );
				else if ( bridge && bridgeTemplate && bridgeInfo)//we have valid Terrain data for the bridge
					getRandomSurfacePosition( bridgeTemplate, bridgeInfo, &pos );
				else
					pos.set( *getObject()->getPosition() );


				// launch the fx list
				FXList::doFXPos( (*fxIt).fx, &pos );

			}

		}

		// see if there are any ocl visuals we need to execute
		BridgeOCLList::const_iterator oclIt;
		for( oclIt = modData->m_ocl.begin(); oclIt != modData->m_ocl.end(); ++oclIt )
		{

			//
			// we'll launch an ocl list if our death time is equal to exactly the delay
			// we're waiting for to launch the list
			//
			if( deathTime == (*oclIt).timeAndLocationInfo.delay )
			{
				Coord3D pos;

				//
				// if a bone name is present, we'll use the bone position, otherwise we'll pick a
				// spot somewhere on the bridge surface
				//
				boneName = (*oclIt).timeAndLocationInfo.boneName;
				if( boneName.isEmpty() == FALSE )
				{

					//
					// special case for creating an OCL for using the bridge object parent center location
					// why do we have this ... well, apparently the OCL ignores the obj parameter
					// passed in if you give it a position parameter, and we need it to pay attention
					// to that object parameteter so the OCL stuff can inherit the LIKE_EXISTING
					// properties from its parent
					//
					if( boneName.compare( "ParentObject" ) == 0 )
					{

						// launch the effects just using the parent object for location info
						ObjectCreationList::create( (*oclIt).ocl, us, nullptr );

					}
					else
					{

						// get bone position
						us->getSingleLogicalBonePosition( boneName.str(), &pos, nullptr );

						// launch the fx list
						ObjectCreationList::create( (*oclIt).ocl, us, &pos, nullptr );

					}

				}
				else
				{

					// get random place on bridge
					if ( bridge && bridgeTemplate && bridgeInfo )//we have valid Terrain data for the bridge
						getRandomSurfacePosition( bridgeTemplate, bridgeInfo, &pos );
					else
						pos.set( *getObject()->getPosition() );

					// launch the fx list
					ObjectCreationList::create( (*oclIt).ocl, us, &pos, nullptr );

				}

			}

		}

	}

	return UPDATE_SLEEP_NONE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::onDie( const DamageInfo *damageInfo )
{

	// kill the towers associated with us
	Object *tower;
	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
	{

		tower = TheGameLogic->findObjectByID( getTowerID( (BridgeTowerType)i ) );
		if( tower )
			tower->kill();

	}

	// we need to handle anything that was on top of us now that we've been destroyed
	handleObjectsOnBridgeOnDie();

	// we have now died, record the death frame
	m_deathFrame = TheGameLogic->getFrame();

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::handleObjectsOnBridgeOnDie()
{
	const Object *bridge = getObject();
	const Coord3D *bridgePos = bridge->getPosition();

	Bridge *terrainBridge = TheTerrainLogic->findBridgeAt( getObject()->getPosition() );
	if( terrainBridge )
	{
		PathfindLayerEnum bridgeLayer = terrainBridge->getLayer();

		BridgeInfo bridgeInfo;

		// get the bridge info
		terrainBridge->getBridgeInfo( &bridgeInfo );

		// setup a polygon area using the bridge extents
		Coord3D bridgePolygon[ 4 ];
		bridgePolygon[ 0 ] = bridgeInfo.fromLeft;
		bridgePolygon[ 1 ] = bridgeInfo.fromRight;
		bridgePolygon[ 2 ] = bridgeInfo.toRight;
		bridgePolygon[ 3 ] = bridgeInfo.toLeft;

		//
		// find the lowest Z point of the bridge area ... we will use this to figure out of
		// objects in the bridge area are "on top" of the bridge
		//
		Real lowBridgeZ = bridgePolygon[ 0 ].z;
		for( Int i = 0; i < 4; ++i )
			if( bridgePolygon[ i ].z < lowBridgeZ )
				lowBridgeZ = bridgePolygon[ i ].z;

		//
		// given the polygon area, how big is the radius that we need to scan in the world
		// to cover from the center of the bridge (the bridge object position) to the edge
		// of the bridge
		//
		Coord2D v;
		v.x = bridgeInfo.toLeft.x - bridgePos->x;
		v.y = bridgeInfo.toLeft.y - bridgePos->y;
		Real radius = v.length();

		// scan the objects in the radius
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( bridgePos,
																																			 radius,
																																			 FROM_CENTER_2D );
		MemoryPoolObjectHolder hold( iter );
		Object *other;
		for( other = iter->first(); other; other = iter->next() )
		{

			// ignore some kind of objects
			if( other->isKindOf( KINDOF_BRIDGE ) || other->isKindOf( KINDOF_BRIDGE_TOWER ) )
				continue;

			// ignore airborne objects
			if( other->isAboveTerrain() )
				continue;

			// ignore objects that were not actually on the bridge
			if( other->getPosition()->z < lowBridgeZ )
				continue;

			// ignore objects that are not inside the bridge polygon
			if( PointInsideArea2D( other->getPosition(), bridgePolygon, 4 ) == FALSE )
				continue;

			// if object not on same layer as bridge do nothing
			if( bridgeLayer != other->getLayer() )
				continue;

			if (other->getLayer() == bridgeLayer)
				other->setLayer(LAYER_GROUND);

			// if they have physics, let 'em fall, otherwise just kill 'em
			PhysicsBehavior* physics = other->getPhysics();
			if (physics)
				physics->setAllowToFall(true);
			else
				other->kill();

		}

	}

}

// ------------------------------------------------------------------------------------------------
/** Set all the position, angle, and speed data we need to for a single scaffold object */
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::setScaffoldData( Object *obj,
																			Real *angle,
																			Real *sunkenHeight,
																			const Coord3D *riseToPos,
																			const Coord3D *buildPos,
																			const Coord3D *bridgeCenter )
{

	// sanity
	if( obj == nullptr || angle == nullptr || riseToPos == nullptr || buildPos == nullptr )
		return;

	const BridgeBehaviorModuleData *modData = getBridgeBehaviorModuleData();

	// get the scaffold behavior interface
	BridgeScaffoldBehaviorInterface *scaffoldBehavior;
	scaffoldBehavior = BridgeScaffoldBehavior::getBridgeScaffoldBehaviorInterfaceFromObject( obj );
	DEBUG_ASSERTCRASH( scaffoldBehavior, ("Unable to find bridge scaffold behavior interface") );

	// compute the sunken position that the object will initially start at
	Real fudge = 8.0f;
	Coord3D sunkenPos = *riseToPos;
	sunkenPos.z = sunkenPos.z - *sunkenHeight - fudge;

	// set object initial position
	obj->setPosition( &sunkenPos );

	// set all the destination points for all scaffold motion
	scaffoldBehavior->setPositions( &sunkenPos, riseToPos, buildPos );

	// set the scaffold object in motion rising up out of the ground
	scaffoldBehavior->setMotion( STM_RISE );

	// set object angle
	obj->setOrientation( *angle );

	//
	// set the speed of the scaffold "animation" which is based on how big of a distance
	// all the scaffold objects have to traverse in order to meet up and be complete in
	// the center of the bridge in an interesting way
	//
	Real lateralSpeed = modData->m_lateralScaffoldSpeed;
	Coord3D buildUpPosToBridgeCenter, riseToPosToBridgeCenter;
	buildUpPosToBridgeCenter.x = buildPos->x - riseToPos->x;
	buildUpPosToBridgeCenter.y = buildPos->y - riseToPos->y;
	buildUpPosToBridgeCenter.z = buildPos->z - riseToPos->z;
	riseToPosToBridgeCenter.x = bridgeCenter->x - riseToPos->x;
	riseToPosToBridgeCenter.y = bridgeCenter->y - riseToPos->y;
	riseToPosToBridgeCenter.z = bridgeCenter->z - riseToPos->z;
	Real distBuildUpPosToBridgeCenter = buildUpPosToBridgeCenter.length();
	Real distRiseToPosToBridgeCenter = riseToPosToBridgeCenter.length();
	scaffoldBehavior->setLateralSpeed( lateralSpeed * (distBuildUpPosToBridgeCenter / distRiseToPosToBridgeCenter) );

	// rising speed is always the same for all objects
	Real verticalSpeed = modData->m_verticalScaffoldSpeed;
	scaffoldBehavior->setVerticalSpeed( verticalSpeed );

}

// ------------------------------------------------------------------------------------------------
/** Start the bridge repair scaffolding.  If we already have scaffolding this call
	* is ignored */
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::createScaffolding()
{

	// if we have scaffolding up already do nothing
	if( m_scaffoldPresent == TRUE )
		return;

	// get the bridge world object
	Object *us = getObject();
	const Coord3D *center = us->getPosition();

	// get our bridge object
	Bridge *bridge = TheTerrainLogic->findBridgeAt( us->getPosition() );

	// get the bridge template
	AsciiString bridgeTemplateName = bridge->getBridgeTemplateName();
	TerrainRoadType *bridgeTemplate = TheTerrainRoads->findBridge( bridgeTemplateName );
	DEBUG_ASSERTCRASH( bridgeTemplate, ("Unable to find bridge template to create scaffolding") );

	// get the thing template for the scaffold object we're going to use
	AsciiString scaffoldObjectName = bridgeTemplate->getScaffoldObjectName();
	const ThingTemplate *scaffoldTemplate = TheThingFactory->findTemplate( scaffoldObjectName );
	if( scaffoldTemplate == nullptr )
	{

		DEBUG_CRASH(( "Unable to find bridge scaffold template" ));
		return;

	}

	// get thing template for scaffold support object
	AsciiString scaffoldSupportObjectName = bridgeTemplate->getScaffoldSupportObjectName();
	const ThingTemplate *scaffoldSupportTemplate = TheThingFactory->findTemplate( scaffoldSupportObjectName );
	if( scaffoldSupportTemplate == nullptr )
	{

		DEBUG_CRASH(( "Unable to find bridge support scaffold template" ));
		return;

	}

	// how much space is going to be between each of the scaffold objects at their final positions
	Real spacing = scaffoldTemplate->getTemplateGeometryInfo().getMajorRadius() * 2.0f;

	// how tall are the scaffold objects
	Real scaffoldHeight = scaffoldTemplate->getTemplateGeometryInfo().getMaxHeightAbovePosition() +
												scaffoldTemplate->getTemplateGeometryInfo().getMaxHeightBelowPosition();
	Real scaffoldSupportHeight = scaffoldSupportTemplate->getTemplateGeometryInfo().getMaxHeightAbovePosition() +
															 scaffoldSupportTemplate->getTemplateGeometryInfo().getMaxHeightBelowPosition();

	// get the bridge info
	BridgeInfo bridgeInfo;
	bridge->getBridgeInfo( &bridgeInfo );

	//
	// given the area of the bridge, figure out what the start and end points are to create
	// all the scaffold objects at (just in the 2D bridge plane, not thinking about
	// rising the objects up through the ground yet)
	//
	Coord3D leftStart;
	leftStart.x = ((bridgeInfo.fromLeft.x - bridgeInfo.fromRight.x) / 2.0f) + bridgeInfo.fromRight.x;
	leftStart.y = ((bridgeInfo.fromLeft.y - bridgeInfo.fromRight.y) / 2.0f) + bridgeInfo.fromRight.y;
	leftStart.z = ((bridgeInfo.fromLeft.z - bridgeInfo.fromRight.z) / 2.0f) + bridgeInfo.fromRight.z;

	Coord3D rightStart;
	rightStart.x = ((bridgeInfo.toLeft.x - bridgeInfo.toRight.x) / 2.0f) + bridgeInfo.toRight.x;
	rightStart.y = ((bridgeInfo.toLeft.y - bridgeInfo.toRight.y) / 2.0f) + bridgeInfo.toRight.y;
	rightStart.z = ((bridgeInfo.toLeft.z - bridgeInfo.toRight.z) / 2.0f) + bridgeInfo.toRight.z;

	//
	// now that we have the left and right start points, we will compute two angles to
	// use for all the objects that we will create ... objects on the left side of the
	// bridge will be given a 'leftAngle' pointing from 'leftStart' to 'rightStart' and
	// the opposite will be given to objects on the right side of the bridge
	//
	Coord2D angleV;
	angleV.x = rightStart.x - leftStart.x;
	angleV.y = rightStart.y - leftStart.y;
	Real leftAngle = angleV.toAngle();
	Real rightAngle = leftAngle + TWO_PI;

	// compute vector from left to right across bridge and the reverse
	Coord3D leftVector;
	leftVector.x = rightStart.x - leftStart.x;
	leftVector.y = rightStart.y - leftStart.y;
	leftVector.z = rightStart.z - leftStart.z;
	Coord3D rightVector;
	rightVector.x = leftStart.x - rightStart.x;
	rightVector.y = leftStart.y - rightStart.y;
	rightVector.z = leftStart.z - rightStart.z;

	//
	// how many of these scaffold objects will take to tile from each of the endpoints
	// to the center area of the bridge
	//
	Real tileDistance = leftVector.length();
	Int numObjects = REAL_TO_INT_CEIL( tileDistance / spacing ) + 1;

	//
	// given the number of objects that we need to tile across the whole bridge, we will
	// go through the creation loop WWMath::Ceil( numObjects / 2.0f ) times, and each
	// time through the loop we'll create an object to move from each side of the
	// bridge, except the last object if the number of objects is odd is dead in the
	// center
	//
	Int numIterations = REAL_TO_INT_CEIL( INT_TO_REAL( numObjects ) / 2.0f );

	//
	// normalize left and right vectors as it is a vector that goes from our left start
	// position to the destination right start position ... we will multiply this vector by a
	// spacing amount and add to the left start position to find a destination position
	// for a particular scaffold object along the bridge surface
	//
	leftVector.normalize();
	rightVector.normalize();

	// create the scaffold objects for now
	Int scaffoldObjectsCreated = 0;
	Coord3D destinationPos, *riseToPos;
	Real *angle;
	Object *obj;
	for( Int i = 0; i < numIterations; ++i )
	{

		// sanity
		DEBUG_ASSERTCRASH( scaffoldObjectsCreated < numObjects,
											 ("Creating too many scaffold objects") );

		// create object
		obj = TheThingFactory->newObject( scaffoldTemplate, us->getTeam() );

		// this object is from the "left" side of the bridge
		riseToPos = &leftStart;
		angle = &leftAngle;

		//
		// compute position for object moving from the left side, we're adding 0.1 here
		// so that all scaffold objects *have* to move some distance ... that way we
		// can just assign the speeds so they line up perfectly and not have to worry
		// about any object reaching a destination before any other object
		//
		destinationPos.x = leftVector.x * (spacing * i) + riseToPos->x + 0.1f;
		destinationPos.y = leftVector.y * (spacing * i) + riseToPos->y;
		destinationPos.z = leftVector.z * (spacing * i) + riseToPos->z;

		//
		// now that they key positions are calculated, set the rest of the position data
		// and movement speeds for the object
		//
		setScaffoldData( obj, angle, &scaffoldHeight, riseToPos, &destinationPos, center );

		// keeping track of objects created
		scaffoldObjectsCreated++;
		m_scaffoldObjectIDList.push_back( obj->getID() );

		//
		// create support object layers under the scaffold object, this object mirrors the scaffold
		// object except is on a lower layer
		//
		Real offset = riseToPos->z;
		Coord3D supportRiseToPos = *riseToPos;
		Coord3D supportDestinationPos = destinationPos;
		Coord3D supportBridgeCenter = *center;
		while( offset >= 0.0f )
		{

			supportRiseToPos.z -= scaffoldSupportHeight;
			supportDestinationPos.z -= scaffoldSupportHeight;
			supportBridgeCenter.z -= scaffoldSupportHeight;
			obj = TheThingFactory->newObject( scaffoldSupportTemplate, us->getTeam() );
			setScaffoldData( obj,
											 angle,
											 &scaffoldSupportHeight,
											 &supportRiseToPos,
											 &supportDestinationPos,
											 &supportBridgeCenter );
			m_scaffoldObjectIDList.push_back( obj->getID() );


			// off to the next layer
			offset -= scaffoldSupportHeight;

		}

		//
		// now create the object from the "right" side of the bridge ... but note that
		// we don't do this on the last iteration through this loop when we have an odd
		// number of objects we're tiling because all the space is already perfectly used up)
		//
		if( scaffoldObjectsCreated < numObjects )
		{

			// sanity
			DEBUG_ASSERTCRASH( scaffoldObjectsCreated < numObjects,
												 ("Creating too many scaffold objects") );

			// create new object
			obj = TheThingFactory->newObject( scaffoldTemplate, us->getTeam() );

			// this object is on the "right" side of the bridge
			riseToPos = &rightStart;
			angle = &rightAngle;

			//
			// compute position for object moving from the left side, we're adding 0.1 here
			// so that all scaffold objects *have* to move some distance ... that way we
			// can just assign the speeds so they line up perfectly and not have to worry
			// about any object reaching a destination before any other object
			//
			destinationPos.x = rightVector.x * (spacing * i) + riseToPos->x + 0.1f;
			destinationPos.y = rightVector.y * (spacing * i) + riseToPos->y;
			destinationPos.z = rightVector.z * (spacing * i) + riseToPos->z;

			// set the rest scaffold data again
			setScaffoldData( obj, angle, &scaffoldHeight, riseToPos, &destinationPos, center );

			// keeping track of objects created
			scaffoldObjectsCreated++;
			m_scaffoldObjectIDList.push_back( obj->getID() );

			//
			// create support object layers under the scaffold object, this object mirrors the scaffold
			// object except is on a lower layer
			//
			Real offset = riseToPos->z;
			Coord3D supportRiseToPos = *riseToPos;
			Coord3D supportDestinationPos = destinationPos;
			Coord3D supportBridgeCenter = *center;
			while( offset >= 0.0f )
			{

				supportRiseToPos.z -= scaffoldSupportHeight;
				supportDestinationPos.z -= scaffoldSupportHeight;
				supportBridgeCenter.z -= scaffoldSupportHeight;
				obj = TheThingFactory->newObject( scaffoldSupportTemplate, us->getTeam() );
				setScaffoldData( obj,
												 angle,
												 &scaffoldSupportHeight,
												 &supportRiseToPos,
												 &supportDestinationPos,
												 &supportBridgeCenter );
				m_scaffoldObjectIDList.push_back( obj->getID() );


				// off to the next layer
				offset -= scaffoldSupportHeight;

			}

		}

	}

	// scaffolding is now present
	m_scaffoldPresent = TRUE;

	// when scaffolding is present, a bridge cannot be used
	TheAI->pathfinder()->changeBridgeState( bridge->getLayer(), FALSE );

}

// ------------------------------------------------------------------------------------------------
/** Remove the bridge scaffolding.  If we don't have any then this call is ignored */
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::removeScaffolding()
{

	// if we have no scaffolding, do nothing
	if( m_scaffoldPresent == FALSE )
		return;

	// go through each object and tell them to reverse course
	Object *obj;
	ObjectIDListIterator it;
	BridgeScaffoldBehaviorInterface *scaffoldBehavior;
	for( it = m_scaffoldObjectIDList.begin(); it != m_scaffoldObjectIDList.end(); ++it )
	{

		// get the object
		obj = TheGameLogic->findObjectByID( (*it) );
		if( obj == nullptr )
			continue;

		// get the scaffold behavior
		scaffoldBehavior = BridgeScaffoldBehavior::getBridgeScaffoldBehaviorInterfaceFromObject( obj );
		DEBUG_ASSERTCRASH( scaffoldBehavior, ("Unable to find bridge scaffold behavior interface") );

		// reverse the motion
		scaffoldBehavior->reverseMotion();

	}

	// clear our scaffold object list
	m_scaffoldObjectIDList.clear();

	// scaffolding is no longer present
	m_scaffoldPresent = FALSE;

	// when scaffolding is gone, a bridge can be used again if we're not in a rubble state
	Object *us = getObject();
	BodyModuleInterface *body = us->getBodyModule();

	if( body->getDamageState() != BODY_RUBBLE )
	{
		Bridge *bridge = TheTerrainLogic->findBridgeAt( us->getPosition() );

		if( bridge )
			TheAI->pathfinder()->changeBridgeState( bridge->getLayer(), TRUE );

	}

}

// ------------------------------------------------------------------------------------------------
/** Is any of the scaffolding in motion */
// ------------------------------------------------------------------------------------------------
Bool BridgeBehavior::isScaffoldInMotion()
{
	Object *obj;

	// go through the scaffold objects, if any of them are in motion the scaffold is in motion
	ObjectIDListIterator it;
	for( it = m_scaffoldObjectIDList.begin(); it != m_scaffoldObjectIDList.end(); ++it )
	{

		// get object
		obj = TheGameLogic->findObjectByID( (*it) );
		if( obj == nullptr )
			continue;

		// get scaffold interface
		BridgeScaffoldBehaviorInterface *bsbi = BridgeScaffoldBehavior::getBridgeScaffoldBehaviorInterfaceFromObject( obj );
		if( bsbi == nullptr )
			continue;

		// check in motion
		if( bsbi->getCurrentMotion() != STM_STILL )
			return TRUE;

	}

	// not in motion
	return FALSE;

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::xfer( Xfer *xfer )
{
	Object *us = getObject();

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// set us as the bridge object in the bridge info
	if( xfer->getXferMode() == XFER_LOAD )
	{
		Bridge *bridge = TheTerrainLogic->findBridgeAt( us->getPosition() );

		// sanity
		DEBUG_ASSERTCRASH( bridge, ("BridgeBehavior::xfer - Unable to find bridge" ));

		// set new object ID in bridge info to us
		bridge->setBridgeObjectID( us->getID() );

	}

	// xfer the tower object ids
	for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
		xfer->xferObjectID( &m_towerID[ i ] );

	// set the tower object ids in the bridge info
	if( xfer->getXferMode() == XFER_LOAD )
	{
		Object *us = getObject();
		Bridge *bridge = TheTerrainLogic->findBridgeAt( us->getPosition() );

		// sanity
		DEBUG_ASSERTCRASH( bridge, ("BridgeBehavior::xfer - Unable to find bridge" ));

		// set new object ID in bridge info to us
		for( Int i = 0; i < BRIDGE_MAX_TOWERS; ++i )
			bridge->setTowerObjectID( m_towerID[ i ], (BridgeTowerType)i );

	}

	// scaffold present flag
	xfer->xferBool( &m_scaffoldPresent );

	// scaffold object id list
	UnsignedShort scaffoldObjectCount = 0;
	scaffoldObjectCount = m_scaffoldObjectIDList.size();
	xfer->xferUnsignedShort( &scaffoldObjectCount );
	ObjectID scaffoldObjectID;
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// write out all object IDs
		ObjectIDListIterator it;
		for( it = m_scaffoldObjectIDList.begin(); it != m_scaffoldObjectIDList.end(); ++it )
		{

			scaffoldObjectID = *it;
			xfer->xferObjectID( &scaffoldObjectID );

		}

	}
	else
	{

		// read all object IDs
		DEBUG_ASSERTCRASH( m_scaffoldObjectIDList.empty(),
											 ("BridgeBehavior::xfer - scaffold object list should be empty") );
		for( Int i = 0; i < scaffoldObjectCount; ++i )
		{

			// read id
			xfer->xferObjectID( &scaffoldObjectID );

			// put on list
			m_scaffoldObjectIDList.push_back( scaffoldObjectID );

		}

	}

	// death frame
	xfer->xferUnsignedInt( &m_deathFrame );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void BridgeBehavior::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
