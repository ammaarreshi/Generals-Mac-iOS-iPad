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

// AIPlayer.cpp ///////////////////////////////////////////////////////////////////////////////////
// Computerized opponent
// Author: Michael S. Booth, January 2002
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "Common/GameMemory.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/PlayerList.h"
#include "Common/BuildAssistant.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/WellKnownKeys.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameClient/TerrainVisual.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/AIPlayer.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/RebuildHoleBehavior.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/PartitionManager.h"


#define SUPPLY_CENTER_CLOSE_DIST (20*PATHFIND_CELL_SIZE_F)

#define USE_DOZER 1

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
AIPlayer::AIPlayer( Player *p ) :
m_player(p),
m_buildDelay(0),
m_teamDelay(0),
m_teamTimer(2),	// Important - don't start building teams until frame 1.
m_structureTimer(2), // Important - don't start building structures until frame 1.
m_readyToBuildTeam(false),
m_readyToBuildStructure(false),
m_structuresInQueue(0),
m_repairDozer(INVALID_ID),
m_skillsetSelector(INVALID_SKILLSET_SELECTION),
m_dozerQueuedForRepair(false),
m_supplySourceAttackCheckFrame(0),
m_attackedSupplyCenter(INVALID_ID),
m_teamSeconds(10),
m_curWarehouseID(INVALID_ID)
{
	m_frameLastBuildingBuilt = TheGameLogic->getFrame();
	p->setCanBuildUnits(false); // turn off ai production by default.

	Int i;
	for (i=0; i<MAX_STRUCTURES_TO_REPAIR; i++) {
		m_structuresToRepair[i] = INVALID_ID;
	}
	m_repairDozerOrigin.zero();
	m_baseCenter.zero();
	m_baseCenterSet = false;
	m_difficulty = TheScriptEngine->getGlobalDifficulty();
	m_teamSeconds = TheAI->getAiData()->m_teamSeconds;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
AIPlayer::~AIPlayer()
{
	clearTeamsInQueue();
}

// ------------------------------------------------------------------------------------------------
/** Invoked when a structure I am building is finished building. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::onStructureProduced( Object *factory, Object *bldg )
{
	m_teamDelay = 0; // Cause the update queues & selection to happen immediately.
	m_buildDelay = 0; // Cause
	/* Find the info building this. */
	BuildListInfo *info;
	for( info = m_player->getBuildList(); info; info = info->getNext() )
	{
		if (info->getObjectID() != bldg->getID()) continue;
		Dict d;
		d.setAsciiString(TheKey_objectName, info->getBuildingName());
		d.setAsciiString(TheKey_objectScriptAttachment, info->getScript());
		// TheSuperHackers @bugfix bobtista 31/10/2025 Preserve scaffold health when AI building construction completes instead of resetting to initial health.
#if RETAIL_COMPATIBLE_CRC
		d.setInt(TheKey_objectInitialHealth, info->getHealth());
#endif
		d.setBool(TheKey_objectUnsellable, info->getUnsellable());

		info->setUnderConstruction(false);
		bldg->updateObjValuesFromMapProperties(&d);
		// clear the under construction status
		bldg->clearStatus( MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_UNDER_CONSTRUCTION, OBJECT_STATUS_RECONSTRUCTING ) );

		TheScriptEngine->addObjectToCache(bldg);
		TheScriptEngine->runObjectScript(info->getScript(), bldg);
		if (TheGlobalData->m_debugAI) {
			AsciiString bldgName = bldg->getTemplate()->getName();
			bldgName.concat(" - Building completed.");
			TheScriptEngine->AppendDebugMessage(bldgName, false);
		}
		checkForSupplyCenter(info, bldg);
		return;
	}

	// Look in build list & see if this is spawned from a hole.
	for( info = m_player->getBuildList(); info; info = info->getNext() )
	{
		const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( info->getTemplateName() );
		if (!bldgPlan) {
			continue;
		}
		if (!bldgPlan->isEquivalentTo(bldg->getTemplate())) {
			continue; // not the same kind of building we're looking for.
		}
		// check for hole.
		if (info->getObjectID() != INVALID_ID) {
			// used to have a building.
			Object *obj = TheGameLogic->findObjectByID( info->getObjectID() );
			if (obj!=nullptr) {
				if (obj->isKindOf(KINDOF_REBUILD_HOLE)) {
					RebuildHoleBehaviorInterface *rhbi = RebuildHoleBehavior::getRebuildHoleBehaviorInterfaceFromObject( obj );
					if( rhbi ) {
						ObjectID spawnedID = rhbi->getReconstructedBuildingID();
						if (bldg->getID() == spawnedID) {
							DEBUG_LOG(("AI got rebuilt %s", bldgPlan->getName().str()));
							info->setObjectID(bldg->getID());
							return;
						}
					}
				}
			}
		}
	}

	if (TheGameLogic->getFrame()>0) {
		DEBUG_LOG(("***AI PLAYER-Structure not found in production queue."));
	}
}

// ------------------------------------------------------------------------------------------------
/** See if the building is a supply center, and see how many supply trucks we want. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::checkForSupplyCenter( BuildListInfo *info, Object *bldg )
{
	class SupplyCenterDockUpdate;
	// if it is a supply center, I must have boxes
	static const NameKeyType key_centerUpdate = NAMEKEY("SupplyCenterDockUpdate");
	SupplyCenterDockUpdate *centerModule = (SupplyCenterDockUpdate*)bldg->findUpdateModule( key_centerUpdate );
	if( centerModule  )
	{
		info->setSupplyBuilding(true);
		Int desiredGatherers = 0;
		const AISideInfo *resInfo = TheAI->getAiData()->m_sideInfo;
		while (resInfo) {
			if (resInfo->m_side == m_player->getSide()) {
				GameDifficulty difficulty = m_difficulty;
				if (difficulty == DIFFICULTY_EASY) {
					desiredGatherers = resInfo->m_easy;
				}
				if (difficulty == DIFFICULTY_NORMAL) {
					desiredGatherers = resInfo->m_normal;
				}
				if (difficulty == DIFFICULTY_HARD) {
					desiredGatherers = resInfo->m_hard;
				}
			}
			resInfo = resInfo->m_next;
		}
		info->setSupplyBuilding(true);
		info->setCurrentGatherers(-1);
		info->setDesiredGatherers(desiredGatherers+1); // get a freebie with the supply depots.
	}
}

// ------------------------------------------------------------------------------------------------
/** Queue up a supply truck to be built. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::queueSupplyTruck()
{
	Bool truckInQueue = false;
	for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
	{
		TeamInQueue *team = iter.cur();
		WorkOrder *order;
		for( order = team->m_workOrders; order; order = order->m_next )
		{
			// GLA dozers (workers) are also resource gatherers, so make sure it isn't a worker. jba.
			if (order->m_isResourceGatherer) {
				truckInQueue = true;
			}
		}
	}

	if (truckInQueue) {
		return; // already building a supply truck.
	}
	Int totalHarvesters = 0;

	// See how many harvesters we have servicing this supply src.
	// Scan my units.
	Player::PlayerTeamList::const_iterator it;
	for (it = m_player->getPlayerTeams()->begin(); it != m_player->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance()) {
				Object *obj = objIter.cur();
				if (!obj)  continue;
				if (!obj->isKindOf(KINDOF_HARVESTER)) continue;
				if (!obj->getAI()) continue;

				SupplyTruckAIInterface* supplyTruckAI = obj->getAI()->getSupplyTruckAIInterface();
				if( supplyTruckAI )	{
					totalHarvesters++;
				}
			}
		}
	}

	/* Find the info building this. */
	for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
	{
		if (info->isSupplyBuilding() == false) continue;
		Int desiredGatherers = info->getDesiredGatherers();
		Int curGatherers = info->getCurrentGatherers();

		if (curGatherers>=desiredGatherers) {
			// Check & see if any have died.
			Object *supplyCenter = TheGameLogic->findObjectByID(info->getObjectID());
			// Check for supplies.
			if (supplyCenter) {
				if (supplyCenter->isKindOf(KINDOF_REBUILD_HOLE)) {
					continue; // don't consider rebuild holes.
				}
				// Make sure we have a supplies near it.
				Coord3D center = *supplyCenter->getPosition();
				Real radius = SUPPLY_CENTER_CLOSE_DIST + supplyCenter->getGeometryInfo().getBoundingCircleRadius();

				PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_SUPPLY_SOURCE), KINDOFMASK_NONE);
				PartitionFilterPlayer f2(m_player, false);	// Only find other.
				PartitionFilterOnMap filterMapStatus;

				PartitionFilter *filters[] = { &f1, &f2, &filterMapStatus, nullptr };

				Object *supplySource = ThePartitionManager->getClosestObject(&center, radius, FROM_BOUNDINGSPHERE_2D, filters);
				if (!supplySource) {
					// No supplies.
					continue;
				}
				static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
				SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*)supplySource->findUpdateModule( key_warehouseUpdate );
				if( warehouseModule )	{
					Int availableCash = warehouseModule->getBoxesStored()*TheGlobalData->m_baseValuePerSupplyBox;
					if (availableCash<=0) continue;
					if( m_player->getRelationship(supplySource->getTeam()) == ENEMIES ) {
						continue;
					}
				}
				// Ok, it has supplies available near it.
				checkForSupplyCenter(info, supplyCenter);
				Int curGatherers = 0;
				// See how many harvesters we have servicing this supply src.
				// Scan my units.
				Player::PlayerTeamList::const_iterator it;
				for (it = m_player->getPlayerTeams()->begin(); it != m_player->getPlayerTeams()->end(); ++it) {
					for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
						Team *team = iter.cur();
						if (!team) {
							continue;
						}
						for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance()) {
							Object *obj = objIter.cur();
							if (!obj)  continue;
							if (!obj->isKindOf(KINDOF_HARVESTER)) continue;
							if (!obj->getAI()) continue;

							SupplyTruckAIInterface* supplyTruckAI = obj->getAI()->getSupplyTruckAIInterface();
							if( supplyTruckAI )	{
								ObjectID dock = supplyTruckAI->getPreferredDockID();
								if (dock == supplyCenter->getID()) {
									curGatherers++;
									if (!supplyTruckAI->isCurrentlyFerryingSupplies()) {
										// Note - although this is the ai, we are sending in CMD_FROM_PLAYER.
										// This causes the dock object to stick in the docking interface.
										// The supply truck ai issues dock commands, and they become confused.
										// Thus, player.  jba.  ;(
										obj->getAI()->aiDock(supplyCenter, CMD_FROM_PLAYER);
									}
								}
							}
						}
					}
				}
				//DEBUG_LOG(("Expected %d harvesters, found %d, need %d", info->getDesiredGatherers(),
				//	curGatherers, info->getDesiredGatherers()-curGatherers) );
				info->setCurrentGatherers(curGatherers);
			}
		} else {
			/* See if we have any "loose" harvesters (cause my supply center got nuked.) */
			Player::PlayerTeamList::const_iterator it;
			for (it = m_player->getPlayerTeams()->begin(); it != m_player->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team *team = iter.cur();
					if (!team) continue;
					for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance()) {
						Object *obj = objIter.cur();
						if (!obj)  continue;
						if (!obj->isKindOf(KINDOF_HARVESTER)) continue;
						if (!obj->getAI()) continue;

						SupplyTruckAIInterface* supplyTruckAI = obj->getAI()->getSupplyTruckAIInterface();
						if( supplyTruckAI )	{
							ObjectID dock = supplyTruckAI->getPreferredDockID();
							if (TheGameLogic->findObjectByID(dock)!=nullptr) continue;
							if (supplyTruckAI->isCurrentlyFerryingSupplies() || supplyTruckAI->isForcedIntoWantingState())
							{
								// This thinks he is a gatherer, but doesn't have a preferred dock id.
								Object *center = TheGameLogic->findObjectByID(info->getObjectID());
								if (center) {
									info->setCurrentGatherers(info->getCurrentGatherers()+1);
									// Note - although this is the ai, we are sending in CMD_FROM_PLAYER.
									// This causes the dock object to stick in the docking interface.
									// The supply truck ai issues dock commands, and they become confused.
									// Thus, player.  jba.  ;(
									obj->getAI()->aiDock(center, CMD_FROM_PLAYER);
									DEBUG_LOG(("Re-attaching supply truck to supply center."));
									return;
								}
							}
						}
					}
				}
			}
			if (totalHarvesters >= desiredGatherers*3) {
				continue; // we got lotsa gatherers.
			}
			Bool canBuildUnits = m_player->getCanBuildUnits();
			// If we need a supply truck thingy, turn on unit building for a moment.
			m_player->setCanBuildUnits(true);
			const ThingTemplate *tTemplate = TheThingFactory->firstTemplate();
			while (tTemplate) {
				Bool isSupplyTruck = tTemplate->isKindOf(KINDOF_HARVESTER);
				if (isSupplyTruck) {
					Object *factory = findFactory(tTemplate, false);
					if (factory) {
						// we can build one.
						WorkOrder *order = newInstance(WorkOrder);
						order->m_thing = tTemplate;
						order->m_factoryID = INVALID_ID;
						order->m_numRequired = 1;
						order->m_required = true;
						order->m_isResourceGatherer =true;
						// prepend to head of list
						order->m_next = nullptr;
						TeamInQueue *team = newInstance(TeamInQueue);
						// Put in front of queue.
						prependTo_TeamBuildQueue(team);
						team->m_priorityBuild = true;
						team->m_workOrders = order;
						team->m_frameStarted = TheGameLogic->getFrame();
						// Stick it on the default team
						team->m_team = m_player->getDefaultTeam();
						AsciiString teamName = "Supply truck - building one at the ";
						teamName.concat(factory->getTemplate()->getName());
						TheScriptEngine->AppendDebugMessage(teamName, false);
						m_teamDelay = 0;
						if (info->getCurrentGatherers()==-1) {
							// First one is automatic. jba.
							order->m_factoryID = factory->getID();
							info->setCurrentGatherers(0);
						}	else {
							startTraining( order, team->m_priorityBuild, team->m_team->getName());
						}
						break;
					}
				}
				tTemplate = tTemplate->friend_getNextTemplate();
			}
			m_player->setCanBuildUnits(canBuildUnits);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static void deleteQueue(TeamInQueue* o)
{
	if (o)
	{
		deleteInstance(o);
	}
}

// ------------------------------------------------------------------------------------------------
/** Clear the current work order */
// ------------------------------------------------------------------------------------------------
void AIPlayer::clearTeamsInQueue()
{
	removeAll_TeamBuildQueue(deleteQueue);
	removeAll_TeamReadyQueue(deleteQueue);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Object *AIPlayer::buildStructureNow(const ThingTemplate *bldgPlan, BuildListInfo *info)
{

	// inst-construct the building
	Object *bldg = TheBuildAssistant->buildObjectNow( nullptr,
																						bldgPlan,
																						info->getLocation(),
																						info->getAngle(),
																						m_player );

	// store the object with the build order
	if (bldg)
	{
		Dict d;
		d.setAsciiString(TheKey_objectName, info->getBuildingName());
		d.setAsciiString(TheKey_objectScriptAttachment, info->getScript());
		// TheSuperHackers @bugfix bobtista 31/10/2025 Preserve scaffold health when AI building construction completes instead of resetting to initial health.
#if RETAIL_COMPATIBLE_CRC
		d.setInt(TheKey_objectInitialHealth, info->getHealth());
#endif
		d.setBool(TheKey_objectUnsellable, info->getUnsellable());

		bldg->updateObjValuesFromMapProperties(&d);

		info->setObjectID( bldg->getID() );
		info->setObjectTimestamp( TheGameLogic->getFrame()+1 );	// has to be non-zero, so just add 1.

		// clear the under construction status
		bldg->clearStatus( MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_UNDER_CONSTRUCTION, OBJECT_STATUS_RECONSTRUCTING ) );

		if (TheGlobalData->m_debugAI) {
			AsciiString bldgName = bldgPlan->getName();
			bldgName.concat(" - Building completed.");
			TheScriptEngine->AppendDebugMessage(bldgName, false);
		}
		TheScriptEngine->addObjectToCache(bldg);
		TheScriptEngine->runObjectScript(info->getScript(), bldg);
		checkForSupplyCenter(info, bldg);
		ExitInterface *exitInterface = bldg->getObjectExitInterface();
		if( exitInterface )
		{
			Coord3D rallyPoint;
			Bool gotOffset = false;
			if (fabs(info->getRallyOffset()->x) > 1.0f || fabs(info->getRallyOffset()->y)>1.0f) {
				gotOffset;
			}
			if (!exitInterface->getNaturalRallyPoint(rallyPoint)) {
				rallyPoint = *info->getLocation();
			}
			if (gotOffset) {
				rallyPoint.x += info->getRallyOffset()->x;
				rallyPoint.y += info->getRallyOffset()->y;
				exitInterface->setRallyPoint(&rallyPoint);
			}
		}
	}
	return bldg;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Object *AIPlayer::buildStructureWithDozer(const ThingTemplate *bldgPlan, BuildListInfo *info)
{
	// Find a dozer.
	Object *dozer = findDozer(info->getLocation());
	if (dozer==nullptr) {
		return nullptr;
	}
	// Check available funds.
	Money *money = m_player->getMoney();
	if (money->countMoney()<bldgPlan->calcCostToBuild(m_player)) {
		return nullptr;
	}
	// construct the building
	Coord3D pos = *info->getLocation();
	pos.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
	if( !dozer->getAIUpdateInterface() )
	{
		return nullptr;
	}
	Real angle = info->getAngle();
 	if( TheBuildAssistant->isLocationLegalToBuild( &pos, bldgPlan, angle,
																								 BuildAssistant::NO_ENEMY_OBJECT_OVERLAP,
																								 dozer, m_player ) != LBC_OK ) {
		// If there's enemy units or structures, don't build/rebuild.
		TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback, turn it off.  jba.
		return nullptr;
	}

	// validate the the position to build at is valid
	if( TheBuildAssistant->isLocationLegalToBuild( &pos, bldgPlan, angle,
																								 BuildAssistant::CLEAR_PATH |
																								 BuildAssistant::TERRAIN_RESTRICTIONS |
																								 BuildAssistant::NO_OBJECT_OVERLAP,
																								 dozer, m_player ) != LBC_OK ) {
			// Warn.
			AsciiString bldgName = bldgPlan->getName();
			bldgName.concat(" - Dozer unable to place.  Attempting to adjust position.");
			TheScriptEngine->AppendDebugMessage(bldgName, false);
			// try to fix.
			Real posOffset;
			Bool valid = false;
			// Wiggle it a little :)
			Real limit = 10*PATHFIND_CELL_SIZE_F;
			if (isSkirmishAI()) {
				limit = 120*PATHFIND_CELL_SIZE_F;
			}
			Coord3D newPos = pos;
			for (posOffset = 0; posOffset<limit; posOffset += 2*PATHFIND_CELL_SIZE_F) {
				if (isSkirmishAI()) {
					posOffset += 2*PATHFIND_CELL_SIZE_F;
				}
				Real offset = posOffset/2;
				Real xPos, yPos;
				yPos = pos.y-offset;
				for (xPos = pos.x-offset; xPos <= pos.x+offset; xPos+=PATHFIND_CELL_SIZE_F) {
					if (isSkirmishAI()) xPos += PATHFIND_CELL_SIZE_F;
					newPos.x = xPos;
					newPos.y = yPos;
					valid = TheBuildAssistant->isLocationLegalToBuild( &newPos, bldgPlan, angle,
																							 BuildAssistant::CLEAR_PATH |
																							 BuildAssistant::TERRAIN_RESTRICTIONS |
																							 BuildAssistant::NO_OBJECT_OVERLAP,
																							 dozer, m_player ) == LBC_OK;
					if (valid) break;
					newPos.y = yPos+posOffset;
					valid = TheBuildAssistant->isLocationLegalToBuild( &newPos, bldgPlan, angle,
																							 BuildAssistant::CLEAR_PATH |
																							 BuildAssistant::TERRAIN_RESTRICTIONS |
																							 BuildAssistant::NO_OBJECT_OVERLAP,
																							 dozer, m_player ) == LBC_OK;
				}
				if (valid) break;
				xPos = pos.x-offset;
				for (yPos = pos.y-offset; yPos <= pos.y+offset; yPos+=PATHFIND_CELL_SIZE_F) {
					if (isSkirmishAI()) yPos += PATHFIND_CELL_SIZE_F;
					newPos.x = xPos;
					newPos.y = yPos;
					valid = TheBuildAssistant->isLocationLegalToBuild( &newPos, bldgPlan, angle,
																							 BuildAssistant::CLEAR_PATH |
																							 BuildAssistant::TERRAIN_RESTRICTIONS |
																							 BuildAssistant::NO_OBJECT_OVERLAP,
																							 dozer, m_player ) == LBC_OK;
					if (valid) break;
					newPos.x = xPos+posOffset;
					valid = TheBuildAssistant->isLocationLegalToBuild( &newPos, bldgPlan, angle,
																							 BuildAssistant::CLEAR_PATH |
																							 BuildAssistant::TERRAIN_RESTRICTIONS |
																							 BuildAssistant::NO_OBJECT_OVERLAP,
																							 dozer, m_player ) == LBC_OK;
				}
				if (valid) break;
			}
			if (valid) pos = newPos;
			if (!valid) {
				valid = TheBuildAssistant->isLocationLegalToBuild( &pos, bldgPlan, angle,
																						 BuildAssistant::NO_ENEMY_OBJECT_OVERLAP,
																						 dozer, m_player ) == LBC_OK;
				if (!valid) {
					return nullptr;
				}
			}

	}

	TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback, turn it off.  jba.
	if (!TheAI->pathfinder()->clientSafeQuickDoesPathExist(dozer->getAI()->getLocomotorSet(),
		dozer->getPosition(), &pos)) {
		AsciiString bldgName = bldgPlan->getName();
		bldgName.concat(" - Dozer unable to reach building.  Teleporting.");
		TheScriptEngine->AppendDebugMessage(bldgName, false);
		dozer->setPosition(&pos);
	}

	Object *bldg = TheBuildAssistant->buildObjectNow( dozer,
																						bldgPlan,
																						&pos,
																						angle,
																						m_player );



#if defined(RTS_DEBUG)
	if (TheGlobalData->m_debugAI == AI_DEBUG_PATHS)
	{
		extern void addIcon(const Coord3D *pos, Real width, Int numFramesDuration, RGBColor color);
 		RGBColor color;
		color.blue = 0;
		color.red = 1;
		color.green = 0;
		Coord3D myPos;
		myPos = *dozer->getPosition();
		myPos.z = TheTerrainLogic->getGroundHeight( myPos.x, myPos.y ) + 0.5f;
		addIcon(&myPos, 2*PATHFIND_CELL_SIZE_F, 120, color);
		myPos = pos;
		myPos.z = TheTerrainLogic->getGroundHeight( myPos.x, myPos.y ) + 0.5f;
		addIcon(&myPos, 2*PATHFIND_CELL_SIZE_F, 120, color);
		Real dx, dy;
		dx = dozer->getPosition()->x - pos.x;
		dy = dozer->getPosition()->y - pos.y;

		Int count = WWMath::SqrtOrigin(dx*dx+dy*dy)/(PATHFIND_CELL_SIZE_F/2);
		if (count<2) count = 2;
		Int i;
		color.green = 1;
		for (i=1; i<count; i++) {
			myPos.x = dozer->getPosition()->x + (pos.x-dozer->getPosition()->x)*i/count;
			myPos.y = dozer->getPosition()->y + (pos.y-dozer->getPosition()->y)*i/count;
			myPos.z = TheTerrainLogic->getGroundHeight( myPos.x, myPos.y ) + 0.5f;
			addIcon(&myPos, PATHFIND_CELL_SIZE_F/2, 120, color);

		}
	}
#endif

	// store the object with the build order
	if (bldg)
	{
		ExitInterface *exitInterface = bldg->getObjectExitInterface();
		if( exitInterface )
		{
			Coord3D rallyPoint;
			Bool gotOffset = false;
			if (fabs(info->getRallyOffset()->x) > 1.0f || fabs(info->getRallyOffset()->y)>1.0f) {
				gotOffset;
			}
			if (!exitInterface->getNaturalRallyPoint(rallyPoint)) {
				rallyPoint = *info->getLocation();
			}
			if (gotOffset) {
				rallyPoint.x += info->getRallyOffset()->x;
				rallyPoint.y += info->getRallyOffset()->y;
				exitInterface->setRallyPoint(&rallyPoint);
			}
		}
		info->setObjectID( bldg->getID() );
		info->setObjectTimestamp( TheGameLogic->getFrame()+1 );	// Has to be non-zero, so add 1.
		info->setUnderConstruction(true);

		if (TheGlobalData->m_debugAI) {
			AsciiString bldgName = bldgPlan->getName();
			bldgName.concat(" - Building started.");
			TheScriptEngine->AppendDebugMessage(bldgName, false);
		}
	}
	TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback, turn it off.  jba.
	return bldg;
}

// ------------------------------------------------------------------------------------------------
/** Build our base. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::processBaseBuilding()
{
	//
	// Refresh base buildings. Scan through list, if a building is missing,
	// rebuild it, unless it's rebuild count is zero.
	//
	if (m_readyToBuildStructure)
	{

		for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
		{
			AsciiString name = info->getTemplateName();
			if (name.isEmpty()) continue;
			const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( name );
			if (!bldgPlan) {
				DEBUG_LOG(("*** ERROR - Build list building '%s' doesn't exist.", name.str()));
				continue;
			}
			// check for hole.
			if (info->getObjectID() != INVALID_ID) {
				// used to have a building.
				Object *bldg = TheGameLogic->findObjectByID( info->getObjectID() );
				if (bldg==nullptr) {
					// got destroyed.
					ObjectID priorID;
					priorID = info->getObjectID();
					info->setObjectID(INVALID_ID);
					info->setObjectTimestamp(TheGameLogic->getFrame()+1);
					// Scan for a GLA hole.	KINDOF_REBUILD_HOLE
					Object *obj;
					for( obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() ) {
						if (!obj->isKindOf(KINDOF_REBUILD_HOLE)) continue;
						RebuildHoleBehaviorInterface *rhbi = RebuildHoleBehavior::getRebuildHoleBehaviorInterfaceFromObject( obj );
						if( rhbi ) {
							ObjectID spawnerID = rhbi->getSpawnerID();
							if (priorID == spawnerID) {
								DEBUG_LOG(("AI Found hole to rebuild %s", bldgPlan->getName().str()));
								info->setObjectID(obj->getID());
							}
						}
 					}
				}	else {
					if (bldg->getControllingPlayer() == m_player) {
						// Check for built or dozer missing.
						if( bldg->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
						{
							// make sure dozer is working on him.
							ObjectID builder = bldg->getBuilderID();
							Object* myDozer = TheGameLogic->findObjectByID(builder);
							if (myDozer==nullptr) {
								DEBUG_LOG(("AI's Dozer got killed.  Find another dozer."));
 								myDozer = findDozer(bldg->getPosition());
								if (myDozer==nullptr || myDozer->getAI()==nullptr) {
									continue;
								}
								myDozer->getAI()->aiResumeConstruction(bldg, CMD_FROM_AI);
							}	else {
								// make sure he is building.
								myDozer->getAI()->aiResumeConstruction(bldg, CMD_FROM_AI);
							}
						}
					} else {
						// oops, got captured.
						info->setObjectID(INVALID_ID);
						info->setObjectTimestamp(TheGameLogic->getFrame()+1);
					}
				}
			}
			if (info->getObjectID()==INVALID_ID && info->getObjectTimestamp()>0) {
				// this object was built at some time, and got destroyed at or near objectTimestamp.
				// Wait a few seconds before initiating a rebuild.
				if (info->getObjectTimestamp()+TheAI->getAiData()->m_rebuildDelaySeconds*LOGICFRAMES_PER_SECOND > TheGameLogic->getFrame()) {
					continue;
				}	else {
					DEBUG_LOG(("Enabling rebuild for %s", info->getTemplateName().str()));
					info->setObjectTimestamp(0); // ready to build.
				}
			}
			// check if this building has any "rebuilds" left
			if (info->isBuildable())
			{
				Object *bldg = TheGameLogic->findObjectByID( info->getObjectID() );

				if (bldg == nullptr)
				{


#ifdef USE_DOZER
					// dozer-construct the building
					bldg = buildStructureWithDozer(bldgPlan, info);
					// store the object with the build order
					if (bldg)
					{
						info->setObjectID( bldg->getID() );
						info->decrementNumRebuilds();

						m_readyToBuildStructure = false;
						m_structureTimer = TheAI->getAiData()->m_structureSeconds*LOGICFRAMES_PER_SECOND;
						if (m_player->getMoney()->countMoney() < TheAI->getAiData()->m_resourcesPoor) {
							m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresPoorMod;
						}	else if (m_player->getMoney()->countMoney() > TheAI->getAiData()->m_resourcesWealthy) {
							m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresWealthyMod;
						}
						m_frameLastBuildingBuilt = TheGameLogic->getFrame();
						// only build one building per delay loop
						break;
					}

#else
					// force delay between rebuilds
					Int framesToBuild = bldgPlan->calcTimeToBuild(m_player);
					if (TheGameLogic->getFrame() - m_frameLastBuildingBuilt < framesToBuild)
					{
						m_buildDelay = framesToBuild - (TheGameLogic->getFrame() - m_frameLastBuildingBuilt);
						return;
					}	else {
						// building is missing, (re)build it
						// deduct money to build, if we have it
						Int cost = bldgPlan->calcCostToBuild( m_player );
						if (m_player->getMoney()->countMoney() >= cost)
						{
							// we have the money, deduct it
							m_player->getMoney()->withdraw( cost );

							// inst-construct the building
							bldg = buildStructureNow(bldgPlan, info);
							// store the object with the build order
							if (bldg)
							{
								info->setObjectID( bldg->getID() );
								info->decrementNumRebuilds();

								m_readyToBuildStructure = false;
								m_structureTimer = TheAI->getAiData()->m_structureSeconds*LOGICFRAMES_PER_SECOND;
								if (m_player->getMoney()->countMoney() < TheAI->getAiData()->m_resourcesPoor) {
									m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresPoorMod;
								}	else if (m_player->getMoney()->countMoney() > TheAI->getAiData()->m_resourcesWealthy) {
									m_structureTimer = m_structureTimer/TheAI->getAiData()->m_structuresWealthyMod;
								}
								m_frameLastBuildingBuilt = TheGameLogic->getFrame();
								// only build one building per delay loop
								break;
							}
						}
					}
#endif
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** A team is about to be destroyed */
//-------------------------------------------------------------------------------------------------
void AIPlayer::aiPreTeamDestroy( const Team *deletedTeam )
{
	{
		for (DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
		{
			TeamInQueue *team = iter.cur();
			if (team->m_team == deletedTeam) {
				// The members of the team all got killed before we could finish building the team.
				removeFrom_TeamBuildQueue(team);
				deleteInstance(team);
				iter = iterate_TeamBuildQueue();
			}
		}
	}
	{
		for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamReadyQueue(); !iter.done(); iter.advance())
		{
			TeamInQueue *team = iter.cur();
			if (team->m_team == deletedTeam) {
				// The members of the team all got killed before we could activate the team.
				removeFrom_TeamReadyQueue(team);
				deleteInstance(team);
				iter = iterate_TeamReadyQueue();
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Guard supply center */
//-------------------------------------------------------------------------------------------------
void AIPlayer::guardSupplyCenter( Team *team, Int minSupplies )
{
	m_supplySourceAttackCheckFrame = 0; // force check.
	Object *warehouse = nullptr;
	if (isSupplySourceAttacked()) {
		warehouse = TheGameLogic->findObjectByID(m_attackedSupplyCenter);
	}
	if (warehouse==nullptr) {
		warehouse = findSupplyCenter(minSupplies);
	}
	if (warehouse) {

		AIGroupPtr theGroup = TheAI->createGroup();
		if (!theGroup) {
			return;
		}
#if RETAIL_COMPATIBLE_AIGROUP
		team->getTeamAsAIGroup(theGroup);
#else
		team->getTeamAsAIGroup(theGroup.Peek());
#endif
		Coord3D location = *warehouse->getPosition();
		// It's probably a defensive move - position towards the enemy.
		Region2D bounds;
		Int enemyNdx = TheScriptEngine->getSkirmishEnemyPlayer()->getPlayerIndex();
		getPlayerStructureBounds(&bounds, enemyNdx);
		Coord3D offset;
		offset.zero();
		offset.x = location.x - (bounds.lo.x+bounds.hi.x)*0.5f;
		offset.y = location.y - (bounds.lo.y+bounds.hi.y)*0.5f;
		offset.normalize();
		Real radius = warehouse->getGeometryInfo().getBoundingCircleRadius()*0.8f;

		location.x -= offset.x*radius;
		location.y -= offset.y*radius;
		theGroup->groupGuardPosition( &location, GUARDMODE_NORMAL, CMD_FROM_SCRIPT );

	}
}

//-------------------------------------------------------------------------------------------------
/** Is a supply source attacked? */
//-------------------------------------------------------------------------------------------------
Bool AIPlayer::isSupplySourceAttacked()
{
	const Int SCAN_RATE = 10; // don't scan more often than every 10 seconds.
	UnsignedInt curFrame = TheGameLogic->getFrame();
	if (curFrame==0) {
		m_supplySourceAttackCheckFrame = curFrame+SCAN_RATE;
		return false; // can't be attacked on first frame.
	}
	m_attackedSupplyCenter = INVALID_ID;
	if (curFrame < m_supplySourceAttackCheckFrame) {
		return false;
	}
	if (m_player->getAttackedFrame()+SCAN_RATE < curFrame) {
		return false; // haven't been attacked recently.
	}
	m_supplySourceAttackCheckFrame = curFrame+SCAN_RATE;

	// Scan my units.
	Player::PlayerTeamList::const_iterator it;
	for (it = m_player->getPlayerTeams()->begin(); it != m_player->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance()) {
				Object *obj = objIter.cur();
				if (!obj) {
					continue;
				}
				if (!obj->isKindOf(KINDOF_CASH_GENERATOR) && !obj->isKindOf(KINDOF_DOZER) &&
					!obj->isKindOf(KINDOF_HARVESTER)) {
					continue;
				}
				// check for attacked.
				BodyModuleInterface *body = obj->getBodyModule();
				if (body) {
					const DamageInfo *info = body->getLastDamageInfo();
					if (info) {
						if (info->out.m_noEffect) {
							continue;
						}
						if (body->getLastDamageTimestamp() + SCAN_RATE > curFrame) {
							// winner.
							m_attackedSupplyCenter = obj->getID();
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** Is the nearest supply source safe? */
//-------------------------------------------------------------------------------------------------
Bool AIPlayer::isSupplySourceSafe( Int minSupplies )
{
	Object *warehouse = findSupplyCenter(minSupplies);
	if (warehouse==nullptr) return true; // it's safe cause it doesn't exist.
	return (isLocationSafe(warehouse->getPosition(), warehouse->getTemplate()));
}

//-------------------------------------------------------------------------------------------------
/** Is this location safe for building this thing? */
//-------------------------------------------------------------------------------------------------
Bool AIPlayer::isLocationSafe(const Coord3D *pos, const ThingTemplate *tthing )
{
	if (tthing == nullptr) return 0;

	// See if we have enemies.
	Real radius = TheAI->getAiData()->m_supplyCenterSafeRadius;
	radius += tthing->getTemplateGeometryInfo().getBoundingCircleRadius();

	// only consider enemies.
	PartitionFilterPlayerAffiliation	filterTeam(m_player, (ALLOW_ALLIES|ALLOW_NEUTRAL), false);

	// and only stuff that is not dead
	PartitionFilterAlive filterAlive;

	// and only stuff that isn't stealthed (and not detected)
	// (note that stealthed allies aren't hidden from us, but we're only looking for enemies here)
	PartitionFilterRejectByObjectStatus filterStealth( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_STEALTHED ),
																										 MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_DETECTED ) );

	// (optional) only stuff that is significant
	PartitionFilterInsignificantBuildings filterInsignificant(true, false);

	PartitionFilterRejectByKindOf	filterHarvesters(MAKE_KINDOF_MASK(KINDOF_HARVESTER), KINDOFMASK_NONE);

	PartitionFilterRejectByKindOf	filterDozer(MAKE_KINDOF_MASK(KINDOF_DOZER), KINDOFMASK_NONE);

	PartitionFilter *filters[16];
	Int numFilters = 0;

	filters[numFilters++] = &filterTeam;
	filters[numFilters++] = &filterAlive;
	filters[numFilters++] = &filterStealth;

	filters[numFilters++] = &filterInsignificant;
	filters[numFilters++] = &filterHarvesters;
	filters[numFilters++] = &filterDozer;
	filters[numFilters] = nullptr;

	Object *enemy = ThePartitionManager->getClosestObject(  pos, radius, FROM_BOUNDINGSPHERE_2D, filters );
	if (enemy!=nullptr) {
		return false;
	}
	return true;

}


// ------------------------------------------------------------------------------------------------
/** Invoked when a unit I am training comes into existence */
// ------------------------------------------------------------------------------------------------
void AIPlayer::onUnitProduced( Object *factory, Object *unit )
{
	Bool found = false;
	// TheSuperHackers @fix Mauller 26/04/2025 Fixes uninitialized variable.
	// To keep retail compatibility it needs to be set true in VS6 builds.
#if defined(_MSC_VER) && _MSC_VER < 1300
	Bool supplyTruck = true;
#else
	Bool supplyTruck = false;
#endif

	// factory could be null at the start of the game.
	if (factory == nullptr) {
		return;
	}

	for (DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
	{
		TeamInQueue *team = iter.cur();
		// find work order entry and delete it
		WorkOrder *order;
		if (found) break;
		for( order = team->m_workOrders; order; order = order->m_next )
		{
			if (order->m_factoryID == factory->getID() && order->m_numCompleted < order->m_numRequired && unit->getTemplate()->isEquivalentTo(order->m_thing))
			{
				// found associated order, mark it complete.
				order->m_numCompleted++;
				// put new unit into the team under construction
				if (team->m_team)
					unit->setTeam( team->m_team );
				if (team->m_reinforcement) {
					team->m_reinforcementID = unit->getID();
				}
				AIUpdateInterface *ai = unit->getAIUpdateInterface();
				if (team->m_team->getPrototype()->getTemplateInfo()->m_hasHomeLocation) {
					if (ai) {
						std::vector<Coord3D> path;
						path.push_back( *ai->getGoalPosition() );
						path.push_back(team->m_team->getPrototype()->getTemplateInfo()->m_homeLocation);
						ai->aiFollowExitProductionPath(&path, nullptr, CMD_FROM_AI);
					}
				}

				order->m_factoryID = INVALID_ID; // no longer using this factory.
				if (ai) {
					// tell it to start gathering resources.
					// Here is the special bit for this exit style, force wanting on SupplyTruck types
					SupplyTruckAIInterface* supplyTruckAI = ai->getSupplyTruckAIInterface();
					if( supplyTruckAI )	{
						if (order->m_isResourceGatherer) {
							supplyTruck = true;
						} else {
							supplyTruck = false;
						}
						supplyTruckAI->setForceWantingState(supplyTruck);
						if (supplyTruck) {
							// assign to a supply depot.
							for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
							{
								if (info->isSupplyBuilding() && info->getDesiredGatherers()>0 &&
									info->getDesiredGatherers()>info->getCurrentGatherers()) {
										Object *obj = TheGameLogic->findObjectByID(info->getObjectID());
										if (obj) {
											info->setCurrentGatherers(info->getCurrentGatherers()+1);
											// Note - although this is the ai, we are sending in CMD_FROM_PLAYER.
											// This causes the dock object to stick in the docking interface.
											// The supply truck ai issues dock commands, and they become confused.
											// Thus, player.  jba.  ;(
											ai->aiDock(obj, CMD_FROM_PLAYER);
										}
									}
							}

						}
					}
				}
				found = true;
				break;
			}
		}
	}
	if (!supplyTruck && unit->isKindOf(KINDOF_DOZER)) {
		if (m_dozerQueuedForRepair) {
			m_repairDozer = unit->getID();
			m_dozerQueuedForRepair =false;
		} else {
			m_buildDelay = 0;
			m_structureTimer = 1;
		}
	}
	if (!found) {
		DEBUG_LOG(("***AI PLAYER-Unit not found in production queue."));
	}

	m_teamDelay = 0; // Cause the update queues & selection to happen immediately.
}

//----------------------------------------------------------------------------------------------------------
/**
 * Find a good spot to fire a superweapon.
 */
void AIPlayer::computeSuperweaponTarget(const SpecialPowerTemplate *power, Coord3D *retPos, Int playerNdx, Real weaponRadius)
{

	Region2D bounds;
	getPlayerStructureBounds(&bounds, playerNdx);

	if (weaponRadius<1.0f) {
		weaponRadius = 1.0f; // sanity to avoid divide by 0.
	}

	Int xCount, yCount;
	bounds.lo.x += weaponRadius;
	bounds.hi.x -= weaponRadius;
	if (bounds.hi.x<bounds.lo.x) {
		bounds.hi.x = bounds.lo.x = (bounds.hi.x+bounds.lo.x)/2.0f;
	}
	if (bounds.hi.y<bounds.lo.y) {
		bounds.hi.y = bounds.lo.y = (bounds.hi.y+bounds.lo.y)/2.0f;
	}

	xCount = REAL_TO_INT_CEIL(bounds.width()/weaponRadius)+1;
	yCount = REAL_TO_INT_CEIL(bounds.height()/weaponRadius)+1;

	if (xCount>10) xCount = 10;
	if (yCount>10) yCount = 10;

	Int cash = -1;
	Coord3D pos;
	Coord3D bestPos;
	Int i, j;

	for (i=0; i<xCount; i++) {
		for (j=0; j<yCount; j++) {
			pos.x = bounds.lo.x + (bounds.width()*i)/xCount;
			pos.y = bounds.lo.y + (bounds.height()*j)/yCount;
			pos.z = 0;
			Int curCash = getPlayerSuperweaponValue(&pos, playerNdx, 2*weaponRadius);
			if ( curCash > cash) {
				cash = curCash;
				bestPos = pos;
			}
		}
	}

	Coord3D veryBestPos;
	xCount = 11;
	yCount = 11;
	cash = -1;
	Int count = 0;
	for (i=0; i<xCount; i++) {
		for (j=0; j<yCount; j++) {
			pos.x = bestPos.x + (i-5)*(weaponRadius/10);
			pos.y = bestPos.y+ (j-5)*(weaponRadius/10);
			pos.z = 0;
			Int curCash = getPlayerSuperweaponValue(&pos, playerNdx, weaponRadius);
			if ( curCash > cash) {
				cash = curCash;
				veryBestPos = pos;
				count = 1;
			}	else if (curCash==cash) {
				veryBestPos.x += pos.x;
				veryBestPos.y += pos.y;
				count++;
			}
		}
	}
	if (count>1) {
		veryBestPos.x /= count;
		veryBestPos.y /= count;
	}
	veryBestPos.z = TheTerrainLogic->getGroundHeight(veryBestPos.x, veryBestPos.y);
	*retPos = veryBestPos;
}

//----------------------------------------------------------------------------------------------------------
/**
 * Get the target value for structures in an area.
 */
Int AIPlayer::getPlayerSuperweaponValue(Coord3D *center, Int playerNdx, Real radius )
{
	if (radius < 4*PATHFIND_CELL_SIZE_F) {
		radius = 4*PATHFIND_CELL_SIZE_F;
	}
	Player::PlayerTeamList::const_iterator it;
	Real cash = 0;
	Real radSqr = sqr(radius);

	Player* pPlayer = ThePlayerList->getNthPlayer(playerNdx);
	if (pPlayer == nullptr) return 0;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) continue;
			for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object *pObj = iter.cur();
				if (!pObj) continue;
				if (pObj->isKindOf(KINDOF_AIRCRAFT)) {
					if (pObj->isSignificantlyAboveTerrain()) {
						continue; // Don't target flying aircraft.  OK if in the airstrip.
					}
				}
				Coord3D pos = *pObj->getPosition();
				Real dx = center->x - pos.x;
				Real dy = center->y - pos.y;
				if (dx*dx+dy*dy<radSqr) {
					Real dist = WWMath::SqrtOrigin(dx*dx+dy*dy);
					Real factor = 1.0f - (dist/(2*radius)); // 1.0 in center, 0.5 on edges.
					Real value = pObj->getTemplate()->calcCostToBuild(pPlayer);
					if (pObj->isKindOf(KINDOF_COMMANDCENTER)) {
						value = value/10; // Command centers cannot be killed by any superweapon, so we don't want to target them as highly. jba.
					}
					if (value > 3000) {
						value = value/10; // Superweapons can't be killed by superweapons, so we don't want to value them highly.
					}
					cash += factor * value;
				}
			}
		}
	}
	return cash;
}
// ------------------------------------------------------------------------------------------------
/** Search the computer player's buildings for one that can build the given request
	* and start training the unit.
	* If busyOK is true, it will queue a unit even if one is building.  This lets
	* script invoked teams "push" to the front of the queue. */
// ------------------------------------------------------------------------------------------------
Bool AIPlayer::startTraining( WorkOrder *order, Bool busyOK, AsciiString teamName)
{
	Object *factory = findFactory(order->m_thing, busyOK);
	if( factory )
	{
		ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
		if (pu && pu->queueCreateUnit( order->m_thing, pu->requestUniqueUnitID() )) {
			order->m_factoryID = factory->getID();
			if (TheGlobalData->m_debugAI) {
				AsciiString teamStr = "Queuing ";
				teamStr.concat(order->m_thing->getName());
				teamStr.concat(" for ");
				teamStr.concat(teamName);
				TheScriptEngine->AppendDebugMessage(teamStr, false);
			}
			return true;
		}
	}

	return FALSE;

}

// ------------------------------------------------------------------------------------------------
/** Search the computer player's buildings for one that can build the given request.
	* If busyOK is true, it will return a busy factory if there are no idle ones.  This is
	* used for script invoked teams "push" to the front of the queue. */
// ------------------------------------------------------------------------------------------------
Object *AIPlayer::findFactory(const ThingTemplate *thing, Bool busyOK)
{
	Object *busyFactory = nullptr; // We prefer a factory that isn't busy.
	for( BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
	{
		Object *factory = TheGameLogic->findObjectByID( info->getObjectID() );
		if( factory )
		{
			if (factory->getControllingPlayer() != m_player) {
				info->setObjectID(INVALID_ID);
				continue;
			}
			// ignore buildings that are under construction.
			if (factory->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
				continue;
			// also ignore buildings that are being sold.
			if (factory->testStatus(OBJECT_STATUS_SOLD))
				continue;
			ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
			// If it doesn't produce, continue.
			if (!pu) continue;
			// if we can't create the unit do nothing
			if( TheBuildAssistant->isPossibleToMakeUnit( factory, thing ) == FALSE )
				continue;
			// If the factory is not busy, return it.
			Bool busy = pu->getProductionCount()>0;
			if (!busy) return factory; // found a not busy factory.
			if (busyOK) busyFactory = factory;
		}

	}
	// We didn't find an idle factory, so return the busy one.
	if (busyOK) return busyFactory;
	return nullptr;
}

// ------------------------------------------------------------------------------------------------
/** Return true if team can be considered for building */
// ------------------------------------------------------------------------------------------------
Bool AIPlayer::isPossibleToBuildTeam( TeamPrototype *proto, Bool requireIdleFactory, Bool &notEnoughMoney)
{
	/* Make sure we have at least one idle factory, and factories for all unit types. */
	Bool anyIdle = false;
	Int cost=0;
	notEnoughMoney = false;
	for( int i=0; i<proto->getTemplateInfo()->m_numUnitsInfo; i++ )
	{
		const TCreateUnitsInfo *unitInfo = &proto->getTemplateInfo()->m_unitsInfo[0];
		const ThingTemplate *thing = TheThingFactory->findTemplate( unitInfo[i].unitThingName );
		if (thing) {
			Int thingCost = thing->calcCostToBuild(m_player);
			if (nullptr == findFactory(thing, true)) {
				// Couldn't find a factory.
				return false;
			}
			if (nullptr != findFactory(thing, false)) {
				// Found an idle factory.
				anyIdle = true;
			}
			cost += thingCost * ((unitInfo[i].maxUnits+unitInfo[i].minUnits)/2.0f);
		}
	}
	cost *= TheAI->getAiData()->m_teamResourcesToBuild;
	if (m_player->getMoney()->countMoney() < cost)	{
		notEnoughMoney = true;
		return false; // too expensive
	}
	if (anyIdle)
	{
		return true;
	}
	if (!requireIdleFactory)
	{
		// Doesn't require an idle factory, so we're ok.
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
/** Check if this team is buildable, doesn't exceed maximum limits, meets conditions,
	* and isn't under construction. */
// ------------------------------------------------------------------------------------------------
Bool AIPlayer::isAGoodIdeaToBuildTeam( TeamPrototype *proto )
{
	// Check condition.
	if (!proto->evaluateProductionCondition()) {
		return false;
	}
	// check build limit
	if (proto->countTeamInstances() >= proto->getTemplateInfo()->m_maxInstances){
		if (TheGlobalData->m_debugAI) {
			AsciiString str;
			str.format("Team %s not chosen - %d already exist.", proto->getName().str(), proto->countTeamInstances());
			TheScriptEngine->AppendDebugMessage(str, false);
		}
		return false;	// Max already built.
	}

	for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
	{
		TeamInQueue *team = iter.cur();
		if (team->m_team->getPrototype() == proto) {
			return false; // currently building one of these.
		}
	}
	Bool needMoney;
	if (!isPossibleToBuildTeam( proto, true, needMoney)) {
		if (TheGlobalData->m_debugAI) {
			AsciiString str;
			if (needMoney) {
				str.format("Team %s not chosen - Not enough money.", proto->getName().str());
			} else {
				str.format("Team %s not chosen - Factory/tech missing or busy.", proto->getName().str());
			}
			TheScriptEngine->AppendDebugMessage(str, false);
		}
		return false;
	}
	return true;
}

// ------------------------------------------------------------------------------------------------
/** See if any existing teams need reinforcements, and have higher priority. */
// ------------------------------------------------------------------------------------------------
Bool AIPlayer::selectTeamToReinforce( Int minPriority )
{
	// Find a high production priority team that needs reinforcements.
	Player::PlayerTeamList::const_iterator t;
	Team *curTeam = nullptr;
	Int curPriority = minPriority; // Don't reinforce a team unless it is above min priority.
	const ThingTemplate *curThing = nullptr;
	for (t = m_player->getPlayerTeams()->begin(); t != m_player->getPlayerTeams()->end(); ++t)
	{
		TeamPrototype *proto = (*t);
		Bool busy = false;
		for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
		{
			TeamInQueue *team = iter.cur();
			if (team->m_team->getPrototype() == proto) {
				busy = true; // currently building one of these.
			}
		}
		if (busy) continue;
		if (proto->getTemplateInfo()->m_automaticallyReinforce && proto->getTemplateInfo()->m_productionPriority>curPriority) {
			// Check the team instances.
			for (DLINK_ITERATOR<Team> iter = proto->iterate_TeamInstanceList(); !iter.done(); iter.advance())
			{
				Team *team = iter.cur();
				if (team->hasAnyUnits() == false)
				{
					continue; // empty.
				}
				const TCreateUnitsInfo *unitInfo = &team->getPrototype()->getTemplateInfo()->m_unitsInfo[0];
				for( int i=0; i<team->getPrototype()->getTemplateInfo()->m_numUnitsInfo; i++ )
				{
					if (unitInfo[i].maxUnits < 1) continue;
					const ThingTemplate *thing = TheThingFactory->findTemplate( unitInfo[i].unitThingName );
					if (thing==nullptr) continue;
					Int count=0;
					team->countObjectsByThingTemplate(1, &thing, false, &count);
					if (count < unitInfo[i].maxUnits)
					{
						// See if there is a factory available.
						if (nullptr != findFactory(thing, false))
						{
							curTeam = team;
							curPriority = proto->getTemplateInfo()->m_productionPriority;
							curThing = thing;
						}
					}
				}
			}
		}
	}
	if (curTeam && curThing)
	{
		/* We have something to build. */
		TeamInQueue *teamQ = newInstance(TeamInQueue);
		// Put in front of queue.
		prependTo_TeamBuildQueue(teamQ);
		teamQ->m_priorityBuild = false;
		teamQ->m_reinforcement = true;

		WorkOrder *order = newInstance(WorkOrder);
		order->m_thing = curThing;
		order->m_factoryID = INVALID_ID;
		order->m_numRequired = 1;
		order->m_required = true;
		// prepend to head of list
		order->m_next = nullptr;
		teamQ->m_workOrders = order;
		teamQ->m_frameStarted = TheGameLogic->getFrame();
		teamQ->m_team = curTeam;

		AsciiString teamName = curTeam->getPrototype()->getName();
		teamName.concat(" - AutoReinforcing one ");
		teamName.concat(curThing->getName());
		TheScriptEngine->AppendDebugMessage(teamName, false);

		// start the creation of a new unit
		Coord3D origin;
		origin = curTeam->getPrototype()->getTemplateInfo()->m_homeLocation;
		if (curTeam->getFirstItemIn_TeamMemberList())
		{
			origin = *curTeam->getFirstItemIn_TeamMemberList()->getPosition();
		}
		Object *unit = curTeam->tryToRecruit(curThing, &origin, TheAI->getAiData()->m_maxRecruitDistance);
		if (unit)
		{
			order->m_numCompleted = 1;

			AsciiString teamStr = "Team '";
			teamStr.concat(curTeam->getPrototype()->getName());
			teamStr.concat("' recruits ");
			teamStr.concat(curThing->getName());
			teamStr.concat(" from team '");
			teamStr.concat(unit->getTeam()->getPrototype()->getName());
			teamStr.concat("'");
			TheScriptEngine->AppendDebugMessage(teamStr, false);

			unit->setTeam(curTeam);

			teamQ->m_reinforcementID = unit->getID();

			AIUpdateInterface *ai = unit->getAIUpdateInterface();
			if (ai)
			{
				ai->aiIdle(CMD_FROM_AI);
			}
		} else {
			startTraining( order, teamQ->m_priorityBuild, teamQ->m_team->getName());
		}
		m_teamDelay = 0;
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
/** Determine the next team to build.  Return true if one was selected. */
// ------------------------------------------------------------------------------------------------
Bool AIPlayer::selectTeamToBuild()
{

	// find the highest priority of all teams
	Player::PlayerTeamList::const_iterator t;
	const Int invalidPri = -99999;
	Int hiPri = invalidPri;
	// collect all teams that are possible to build, and are at the highest priority
	Player::PlayerTeamList candidateList1;
	for (t = m_player->getPlayerTeams()->begin(); t != m_player->getPlayerTeams()->end(); ++t)
	{
		if (isAGoodIdeaToBuildTeam(*t))
		{
			candidateList1.push_back( (*t) );
			Int pri = (*t)->getTemplateInfo()->m_productionPriority;

			if (pri > hiPri)
			{
				hiPri = pri;
			}
		}
	}

	if (selectTeamToReinforce(hiPri)) {
		return true;
	}

	// check if no team prototypes are valid for production
	if (hiPri == invalidPri)
		return false;

	if (TheGlobalData->m_debugAI) {
		TheScriptEngine->AppendDebugMessage("**AI** Selecting team to build", false);
	}

	// collect all teams that are possible to build, and are at the highest priority
	Player::PlayerTeamList candidateList;
	Int count = 0;
	for (t = candidateList1.begin(); t != candidateList1.end(); ++t)
	{
		if ((*t)->getTemplateInfo()->m_productionPriority == hiPri)
		{
			candidateList.push_back( (*t) );
			count++;
		}
	}

	// pick a random team from the hi-priority set
	Int which = GameLogicRandomValue( 0, count-1 );

	TeamPrototype *teamProto = nullptr;
	Int i = 0;
	for (t = candidateList.begin(); t != candidateList.end(); ++t)
	{
		if (i == which)
		{
			teamProto = (*t);
			break;
		}

		i++;
	}
	if (teamProto) {
		if (!teamProto->getTemplateInfo()->m_hasHomeLocation && !isSkirmishAI()) {
			AsciiString teamStr = "Error : team '";
			teamStr.concat(teamProto->getName());
			teamStr.concat("' has no Home Position (or Origin).");
			TheScriptEngine->AppendDebugMessage(teamStr, false);
		}
		// Build it at low priority, as we have selected it automagically.
		buildSpecificAITeam(teamProto, false);
		m_readyToBuildTeam = false;
		m_teamTimer = m_teamSeconds*LOGICFRAMES_PER_SECOND;
		if (m_player->getMoney()->countMoney() < TheAI->getAiData()->m_resourcesPoor) {
			m_teamTimer = m_teamTimer/TheAI->getAiData()->m_teamPoorMod;
		}	else if (m_player->getMoney()->countMoney() > TheAI->getAiData()->m_resourcesWealthy) {
			m_teamTimer = m_teamTimer/TheAI->getAiData()->m_teamWealthyMod;
		}
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
/** Build a specific team.  If priorityBuild, put at front of queue with priority set. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::buildSpecificAIBuilding(const AsciiString &thingName)
{
	//
	AsciiString teamStr = "Error : Solo ai doesn't support BuildSpecificBuilding. '";
	teamStr.concat(thingName);
	teamStr.concat("' not built.");
	TheScriptEngine->AppendDebugMessage(teamStr, false);
}

// ------------------------------------------------------------------------------------------------
/** Build an upgrade. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::buildUpgrade(const AsciiString &upgrade)
{
	const UpgradeTemplate *curUpgrade = TheUpgradeCenter->findUpgrade(upgrade);
	if (curUpgrade==nullptr) {
		AsciiString msg = "Upgrade ";
		msg.concat(upgrade);
		msg.concat(" does not exist.  Ignoring request.");
		TheScriptEngine->AppendDebugMessage( msg, false);
		return;
	}
 	if (curUpgrade->getUpgradeType()==UPGRADE_TYPE_OBJECT) {
		AsciiString msg = "Player build upgrade: Upgrade ";
		msg.concat(upgrade);
		msg.concat(" is an object, not a player upgrade.  Ignoring request.");
		TheScriptEngine->AppendDebugMessage( msg, false);
		return;
	}
	// See if it is in progress.
	if (m_player->hasUpgradeInProduction(curUpgrade)) {
		AsciiString msg = TheNameKeyGenerator->keyToName(m_player->getPlayerNameKey());
		msg.concat(" already has upgrade ");
		msg.concat(upgrade);
		msg.concat(" queued.  Ignoring request.");
		TheScriptEngine->AppendDebugMessage( msg, false);
		return;
	}
	// See if it is in progress.
	if (m_player->hasUpgradeComplete(curUpgrade)) {
		AsciiString msg = TheNameKeyGenerator->keyToName(m_player->getPlayerNameKey());
		msg.concat(" already has upgrade ");
		msg.concat(upgrade);
		msg.concat(" completed.  Ignoring request.");
		TheScriptEngine->AppendDebugMessage( msg, false);
		return;
	}


	// No money.
	if( TheUpgradeCenter->canAffordUpgrade( m_player, curUpgrade ) == FALSE ) {
		AsciiString msg = TheNameKeyGenerator->keyToName(m_player->getPlayerNameKey());
		msg.concat(" lacks money to build upgrade ");
		msg.concat(upgrade);
		msg.concat(" at this time.  Ignoring request.");
		TheScriptEngine->AppendDebugMessage( msg, false);
		return;
	}
	// Find a production queue.
	for( const BuildListInfo *info = m_player->getBuildList(); info; info = info->getNext() )
	{
		Object *factory = TheGameLogic->findObjectByID( info->getObjectID() );
		if( factory )
		{
			if( factory->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
				continue;
			if( factory->getStatusBits().test( OBJECT_STATUS_SOLD ) )
				continue;
			Bool canUpgradeHere = false;
			const CommandSet *commandSet = TheControlBar->findCommandSet( factory->getCommandSetString() );
			if( commandSet == nullptr) continue;
			for( Int j = 0; j < MAX_COMMANDS_PER_SET; j++ )
			{
				//Get the command button.
				const CommandButton *commandButton = commandSet->getCommandButton(j);
				if (commandButton==nullptr) continue;
				if (commandButton->getName().isEmpty() )	continue;
				if (commandButton->getUpgradeTemplate() == nullptr )	continue;
 				if (commandButton->getUpgradeTemplate()->getUpgradeName() == curUpgrade->getUpgradeName()) {
					canUpgradeHere = true;
				}
			}
			if (!canUpgradeHere) continue;
			ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
			// If it doesn't produce, continue.
			if (!pu) continue;
			// Try to queue it.
			if (pu->queueUpgrade(curUpgrade)) {
				AsciiString msg = TheNameKeyGenerator->keyToName(m_player->getPlayerNameKey());
				msg.concat(" queues ");
				msg.concat(curUpgrade->getUpgradeName());
				msg.concat(" at ");
				msg.concat(factory->getTemplate()->getName());
				TheScriptEngine->AppendDebugMessage( msg, false);
				return;
			}
		}
	}

	AsciiString msg = TheNameKeyGenerator->keyToName(m_player->getPlayerNameKey());
	msg.concat(" lacks factory to build upgrade ");
	msg.concat(upgrade);
	msg.concat(" at this time.  Ignoring request.");
	TheScriptEngine->AppendDebugMessage( msg, false);
}

// ------------------------------------------------------------------------------------------------
/** Build a supply center near a supply source with minimumCash or more resources. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::buildBySupplies(Int minimumCash, const AsciiString& thingName)
{
	const ThingTemplate* tTemplate = TheThingFactory->findTemplate(thingName);
	if (!tTemplate)
	{
		DEBUG_CRASH(("Template %s should exist; check ini and script files.", thingName.str()));
		return;
	}

	Object *bestSupplyWarehouse = findSupplyCenter(minimumCash);

	if (!tTemplate->isKindOf(KINDOF_CASH_GENERATOR)) {
		// Build by the current warehouse.
		Object *curWarehouse = TheGameLogic->findObjectByID(m_curWarehouseID);
		if (curWarehouse) {
			bestSupplyWarehouse = curWarehouse;
		}
	}


	if (bestSupplyWarehouse) {
		Coord3D location;
		location = *bestSupplyWarehouse->getPosition();
		// offset back towards the base.
		Coord2D offset;
		offset.x = location.x - m_baseCenter.x;
		offset.y = location.y - m_baseCenter.y;
		offset.normalize();
		Real radius = 3*PATHFIND_CELL_SIZE_F;
		if (!tTemplate->isKindOf(KINDOF_CASH_GENERATOR)) {
			// It's probably a defensive structure - build towards the enemy.
			Region2D bounds;
			Int enemyNdx = TheScriptEngine->getSkirmishEnemyPlayer()->getPlayerIndex();
			getPlayerStructureBounds(&bounds, enemyNdx);
			offset.x = location.x - (bounds.lo.x+bounds.hi.x)*0.5f;
			offset.y = location.y - (bounds.lo.y+bounds.hi.y)*0.5f;
			offset.normalize();
			radius = bestSupplyWarehouse->getGeometryInfo().getBoundingCircleRadius();
		}
		location.x -= offset.x*radius;
		location.y -= offset.y*radius;
		Real angle = tTemplate->getPlacementViewAngle();

 		// validate the the position to build at is valid
		Bool valid=false;
		Coord3D newPos = location;
		if( TheBuildAssistant->isLocationLegalToBuild( &location, tTemplate, angle,
																									 BuildAssistant::NO_OBJECT_OVERLAP,
																									 nullptr, m_player ) != LBC_OK ) {
			// Warn.
			const Coord3D *warehouseLocation = bestSupplyWarehouse->getPosition();
			AsciiString debugMessage;
			debugMessage.format(" %s - buildBySupplies unable to place near dock at (%.2f,%.2f).  Attempting to adjust position.",
													tTemplate->getName().str(),
													warehouseLocation->x,
													warehouseLocation->y
													);
			TheScriptEngine->AppendDebugMessage(debugMessage, false);
			if( TheGlobalData->m_debugSupplyCenterPlacement )
				DEBUG_LOG(("%s", debugMessage.str()));
			// try to fix.
			Real posOffset;
			// Wiggle it a little :)
			for (posOffset = 0; posOffset<2*SUPPLY_CENTER_CLOSE_DIST; posOffset += 2*PATHFIND_CELL_SIZE_F) {
				Real offset = posOffset/2;
				Real xPos, yPos;
				yPos = location.y-offset;
				for (xPos = location.x-offset; xPos <= location.x+offset; xPos+=PATHFIND_CELL_SIZE_F) {
					newPos.x = xPos;
					newPos.y = yPos;
					valid = TheBuildAssistant->isLocationLegalToBuild( &newPos, tTemplate, angle,
																							 BuildAssistant::CLEAR_PATH |
																							 BuildAssistant::TERRAIN_RESTRICTIONS |
																							 BuildAssistant::NO_OBJECT_OVERLAP,
																							 nullptr, m_player ) == LBC_OK;
					if (valid) break;
					if( TheGlobalData->m_debugSupplyCenterPlacement )
						DEBUG_LOG(("buildBySupplies -- Fail at (%.2f,%.2f)", newPos.x, newPos.y));
					newPos.y = yPos+posOffset;
					valid = TheBuildAssistant->isLocationLegalToBuild( &newPos, tTemplate, angle,
																							 BuildAssistant::CLEAR_PATH |
																							 BuildAssistant::TERRAIN_RESTRICTIONS |
																							 BuildAssistant::NO_OBJECT_OVERLAP,
																							 nullptr, m_player ) == LBC_OK;
					if( !valid && TheGlobalData->m_debugSupplyCenterPlacement )
						DEBUG_LOG(("buildBySupplies -- Fail at (%.2f,%.2f)", newPos.x, newPos.y));
				}
				if (valid) break;
				xPos = location.x-offset;
				for (yPos = location.y-offset; yPos <= location.y+offset; yPos+=PATHFIND_CELL_SIZE_F) {
					newPos.x = xPos;
					newPos.y = yPos;
					valid = TheBuildAssistant->isLocationLegalToBuild( &newPos, tTemplate, angle,
																							 BuildAssistant::CLEAR_PATH |
																							 BuildAssistant::TERRAIN_RESTRICTIONS |
																							 BuildAssistant::NO_OBJECT_OVERLAP,
																							 nullptr, m_player ) == LBC_OK;
					if (valid) break;
					if( TheGlobalData->m_debugSupplyCenterPlacement )
						DEBUG_LOG(("buildBySupplies -- Fail at (%.2f,%.2f)", newPos.x, newPos.y));
					newPos.x = xPos+posOffset;
					valid = TheBuildAssistant->isLocationLegalToBuild( &newPos, tTemplate, angle,
																							 BuildAssistant::CLEAR_PATH |
																							 BuildAssistant::TERRAIN_RESTRICTIONS |
																							 BuildAssistant::NO_OBJECT_OVERLAP,
																							 nullptr, m_player ) == LBC_OK;
					if( !valid && TheGlobalData->m_debugSupplyCenterPlacement )
						DEBUG_LOG(("buildBySupplies -- Fail at (%.2f,%.2f)", newPos.x, newPos.y));
				}
				if (valid) break;
			}
		}
		if (valid)
		{
			if( TheGlobalData->m_debugSupplyCenterPlacement )
				DEBUG_LOG(("buildAISupplyCenter -- SUCCESS at (%.2f,%.2f)", newPos.x, newPos.y));
			location = newPos;
		}
		TheTerrainVisual->removeAllBibs();	// isLocationLegalToBuild adds bib feedback, turn it off.  jba.
		location.z = 0; // All build list locations are ground relative.
		m_player->addToPriorityBuildList(thingName, &location, angle);
		m_curWarehouseID = bestSupplyWarehouse->getID();
	}
}

// ------------------------------------------------------------------------------------------------
/** Find a supply center we haven't built a supply depot near yet. */
// ------------------------------------------------------------------------------------------------
Object *AIPlayer::findSupplyCenter(Int minimumCash)
{
	Object *bestSupplyWarehouse = nullptr;
	Real bestDistSqr = 0;
	Object *obj;
	Coord3D enemyCenter;
	enemyCenter.zero();
	Region2D bounds;
	Player *enemy = getAiEnemy();
	if (enemy) {
		getPlayerStructureBounds(&bounds, enemy->getPlayerIndex());
		enemyCenter.set( (bounds.lo.x+bounds.hi.x)*0.5f, (bounds.lo.y+bounds.hi.y)*0.5f, 0);
	}

	do {
		for( obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() )
		{
			if (!obj->isKindOf(KINDOF_STRUCTURE)) continue;
			if (!obj->isKindOf(KINDOF_SUPPLY_SOURCE)) continue;
			static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
			SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*)obj->findUpdateModule( key_warehouseUpdate );
			if( warehouseModule )	{
				Int availableCash = warehouseModule->getBoxesStored()*TheGlobalData->m_baseValuePerSupplyBox;
				if (availableCash<minimumCash) continue;
				if( m_player->getRelationship(obj->getTeam()) == ENEMIES ) {
					continue;
				}

				// Make sure we don't have a supply center near it.
				Coord3D center = *obj->getPosition();
				Real radius = SUPPLY_CENTER_CLOSE_DIST + obj->getGeometryInfo().getBoundingCircleRadius();

				PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_CASH_GENERATOR), KINDOFMASK_NONE);
				PartitionFilterPlayer f2(m_player, true);	// Only find your own units.
				PartitionFilterOnMap filterMapStatus;


				PartitionFilter *filters[] = { &f1, &f2, &filterMapStatus, nullptr };

				Object *supplyCenter = ThePartitionManager->getClosestObject(&center, radius, FROM_BOUNDINGSPHERE_2D, filters);
				if (supplyCenter) {
					// We already have a supply center.
					continue;
				}

				Real dx, dy;
				dx = obj->getPosition()->x - m_baseCenter.x;
				dy = obj->getPosition()->y - m_baseCenter.y;
				Real distSqr = dx*dx + dy*dy;
				if (enemy) {
					// make sure this isn't closer to our enemy than us.
					dx = obj->getPosition()->x - enemyCenter.x;
					dy = obj->getPosition()->y - enemyCenter.y;
					if (distSqr*0.4>(dx*dx+dy*dy)*0.6f) {
						// closer than 60/40 to enemy than to us, probably not a good candidate for expansion.
						continue;
					}
				}

				if (bestSupplyWarehouse==nullptr) {
					bestSupplyWarehouse = obj;
					bestDistSqr = distSqr;
				} else if (bestDistSqr>distSqr) {
					bestSupplyWarehouse = obj;
					bestDistSqr = distSqr;
				}
			}
		}
		if (bestSupplyWarehouse) break;
		minimumCash /= 2;
 	} while (minimumCash > 100);

	return bestSupplyWarehouse;
}

// ------------------------------------------------------------------------------------------------
/**  Build a base defense. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::buildAIBaseDefense(Bool flank)
{
	//
	AsciiString teamStr = "Error : Solo ai doesn't support buildAIBaseDefense. '";
	TheScriptEngine->AppendDebugMessage(teamStr, false);
}

// ------------------------------------------------------------------------------------------------
/** Build a base defense. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::buildAIBaseDefenseStructure(const AsciiString &thingName, Bool flank)
{
	//
	AsciiString teamStr = "Error : Solo ai doesn't support buildAIBaseDefenseStructure. '";
	TheScriptEngine->AppendDebugMessage(teamStr, false);
}

// ------------------------------------------------------------------------------------------------
/** Repair a bridge or other structure. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::repairStructure(ObjectID structure)
{
	Object *structureObj = TheGameLogic->findObjectByID(structure);
	if (structureObj==nullptr) return;
	if (structureObj->getBodyModule()==nullptr) return;
	// If the structure is not noticeably damaged, don't bother.
	BodyDamageType structureState = structureObj->getBodyModule()->getDamageState();
	if (structureState==BODY_PRISTINE) {
		return;
	}
	if (structureObj->isKindOf(KINDOF_BRIDGE)) {
		// Locate the correct post to repair.
	}
	Int i;
	for (i=0; i<m_structuresInQueue; i++) {
		if (m_structuresToRepair[i] == structureObj->getID()) {
			DEBUG_LOG(("info - Bridge already queued for repair."));
			return;
		}
	}
	if (m_structuresInQueue>=MAX_STRUCTURES_TO_REPAIR) {
		DEBUG_LOG(("Structure repair queue is full, ignoring repair request. JBA"));
		return;
	}
	m_structuresToRepair[m_structuresInQueue] = structureObj->getID();
	m_structuresInQueue++;
}

// ------------------------------------------------------------------------------------------------
/** select a skillset for the player. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::selectSkillset(Int skillset)
{
	DEBUG_ASSERTCRASH(m_skillsetSelector == INVALID_SKILLSET_SELECTION,
		("Selecting a skill set (%d) after one has already been chosen (%d) means some points have been incorrectly spent.", skillset + 1, m_skillsetSelector + 1));

	m_skillsetSelector = skillset;
}

// ------------------------------------------------------------------------------------------------
/** Do per frame work (if any) repairing bridges. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::updateBridgeRepair()
{
	if (m_structuresInQueue == 0) return;
	// Check once a second.
	m_bridgeTimer--;
	if (m_bridgeTimer>0) return;
	m_bridgeTimer = LOGICFRAMES_PER_SECOND;
	Object *bridgeObj=nullptr;
	while (bridgeObj==nullptr && m_structuresInQueue>0) {
		bridgeObj = TheGameLogic->findObjectByID(m_structuresToRepair[0]);
		if (bridgeObj==nullptr) {
			Int i;
			for (i=0; i<m_structuresInQueue-1; i++) {
				m_structuresToRepair[i] = m_structuresToRepair[i+1];
			}
			m_structuresInQueue--;
		}
	}
	if (m_structuresInQueue == 0) return;

	// Got a bridge to repair.
	Object *dozer = nullptr;
	Coord3D bridgePos = *bridgeObj->getPosition();
	BodyDamageType bridgeState = bridgeObj->getBodyModule()->getDamageState();
	if (m_repairDozer==INVALID_ID) {
		m_dozerIsRepairing = false;
		// Need a dozer.
		if (m_dozerQueuedForRepair) {
			return; // we're waiting for one.
		}
		dozer = findDozer(&bridgePos);
		if (dozer) {
			m_repairDozer = dozer->getID();
			m_repairDozerOrigin = *dozer->getPosition();
			dozer->getAI()->aiRepair(bridgeObj, CMD_FROM_AI);
			DEBUG_LOG(("Telling dozer to repair"));
			m_dozerIsRepairing = true;
			return;
		}
		queueDozer();
		m_dozerQueuedForRepair = true;
		return;
	}

	dozer = TheGameLogic->findObjectByID(m_repairDozer);
	if (dozer==nullptr) {
		m_repairDozer=INVALID_ID; // we got killed.
		m_bridgeTimer=0;
		return; // Just try to find a dozer next frame.
	}

	DozerAIInterface* dozerAI = dozer->getAI()->getDozerAIInterface();
	if (dozerAI==nullptr) {
		DEBUG_CRASH(("Unexpected - dozer doesn't have dozer interface."));
		return;
	}
	if (m_dozerIsRepairing) {
		if (!dozerAI->isAnyTaskPending())	{
			// should be done repairing.
			if (bridgeState==BODY_PRISTINE) {
				DEBUG_LOG(("Dozer finished repairing structure."));
				// we're done.
				Int i;
				for (i=0; i<m_structuresInQueue-1; i++) {
					m_structuresToRepair[i] = m_structuresToRepair[i+1];
				}
				m_structuresInQueue--;
				m_dozerIsRepairing = false;
				if (m_structuresInQueue==0) {
					// Go home.
					Coord3D pos = m_baseCenter;
					if (!m_baseCenterSet) {
						pos = m_repairDozerOrigin;
					}
					AIUpdateInterface *ai=dozer->getAI();
					TheAI->pathfinder()->adjustToPossibleDestination(dozer, ai->getLocomotorSet(), &pos);
					dozer->getAI()->aiMoveToPosition(&pos, CMD_FROM_AI);
					return;
				}
			}
		}	else {
			// dozer should be working on the bridge.
			return;
		}
	}
	dozer->getAI()->aiRepair(bridgeObj, CMD_FROM_AI);
	m_dozerIsRepairing = true;
	DEBUG_LOG(("Telling dozer to repair"));
}

// ------------------------------------------------------------------------------------------------
/** Build a specific team.  If priorityBuild, put at front of queue with priority set. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::buildSpecificAITeam( TeamPrototype *teamProto, Bool priorityBuild)
{
	//
	// Create "Team in queue" based on team population
	//
	if (teamProto)
	{
		if (!m_player->getCanBuildUnits()) {
			AsciiString teamStr = "Can't build team '";
			teamStr.concat(teamProto->getName());
			teamStr.concat("' because build units is disabled.");
			TheScriptEngine->AppendDebugMessage(teamStr, false);
			return;
		}
		if (priorityBuild && teamProto->getIsSingleton()) {
			Team *singletonTeam = TheTeamFactory->findTeam( teamProto->getName() );
			if (singletonTeam && singletonTeam->hasAnyObjects()) {
				AsciiString teamStr = "Unable to build singleton team '";
				teamStr.concat("' because team already exists.");
				TheScriptEngine->AppendDebugMessage(teamStr, false);
				return;
			}
		}
		// Check & make sure we have factories.
		Bool needMoney;
		if (!isPossibleToBuildTeam(teamProto, false, needMoney)) {
			if (needMoney) {
				// Queue it up anyway.
				AsciiString teamStr = "Note - queueing team '";
				teamStr.concat(teamProto->getName());
				teamStr.concat("' but there is enough money.");
				TheScriptEngine->AppendDebugMessage(teamStr, false);
			} else {
				// Tech tree doesn't work.
				AsciiString teamStr = "Unable to build team '";
				teamStr.concat(teamProto->getName());
				if (needMoney) {
					teamStr.concat("' - Not enough money.");
				} else {
					teamStr.concat("' because required factories/tech don't exist.");
				}
				TheScriptEngine->AppendDebugMessage(teamStr, false);
				return;
			}
		}
		const TCreateUnitsInfo *unitInfo = &teamProto->getTemplateInfo()->m_unitsInfo[0];
		WorkOrder *orders = nullptr;
		Int i;
		// Queue up optional units.
		for( i=0; i<teamProto->getTemplateInfo()->m_numUnitsInfo; i++ )
		{
			const ThingTemplate *thing = TheThingFactory->findTemplate( unitInfo[i].unitThingName );
			if (thing)
			{
				int count = unitInfo[i].maxUnits-unitInfo[i].minUnits;
				if (count>0) {
					WorkOrder *order = newInstance(WorkOrder);
					order->m_thing = thing;
					order->m_factoryID = INVALID_ID;
					order->m_numRequired = count;
					// prepend to head of list
					order->m_next = orders;
					orders = order;
				}
			}
		}
		// Queue up required units.
		for( i=0; i<teamProto->getTemplateInfo()->m_numUnitsInfo; i++ )
		{
			const ThingTemplate *thing = TheThingFactory->findTemplate( unitInfo[i].unitThingName );
			if (thing)
			{
				int count = unitInfo[i].minUnits;
				WorkOrder *order = newInstance(WorkOrder);
				order->m_thing = thing;
				order->m_factoryID = INVALID_ID;
				order->m_numRequired = count;
				order->m_required = true;
				// prepend to head of list
				order->m_next = orders;
				orders = order;
			}
		}
		if (orders)
		{
			/* We have something to build. */
			TeamInQueue *team = newInstance(TeamInQueue);
			if (priorityBuild) {
				// Put in front of queue.
				prependTo_TeamBuildQueue(team);
				team->m_priorityBuild = true;
			}	else {
				// Put in back of queue.
				reverse_TeamBuildQueue();
				prependTo_TeamBuildQueue(team);
				reverse_TeamBuildQueue();
				team->m_priorityBuild = false;
			}
			team->m_workOrders = orders;
			team->m_frameStarted = TheGameLogic->getFrame();
			// create inactive team to place members into as they are built
			// when team is complete, the team is activated
			team->m_team = TheTeamFactory->createInactiveTeam( teamProto->getName() );
			AsciiString teamName = teamProto->getName();
			teamName.concat(" - starting team build.");
			TheScriptEngine->AppendDebugMessage(teamName, false);
			m_teamDelay = 0;
			if (team->m_team->getPrototype()->getTemplateInfo()->m_executeActions) {
				const Script *script = TheScriptEngine->findScriptByName(team->m_team->getPrototype()->getTemplateInfo()->m_productionCondition);
				if (script && script->getAction()) {
					TheScriptEngine->friend_executeAction(script->getAction(), team->m_team);
				}
			}
		} else {
			if (TheGlobalData->m_debugAI) {
				AsciiString teamName = teamProto->getName();
				teamName.concat(" - contains 0 buildable units.");
				TheScriptEngine->AppendDebugMessage(teamName, false);
			}
		}

	}
}

// ------------------------------------------------------------------------------------------------
/** Recruit a specific team, within the specific radius of the home position. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::recruitSpecificAITeam(TeamPrototype *teamProto, Real recruitRadius)
{
	if (recruitRadius < 1) recruitRadius = 99999.0f;
	//
	// Create "Team in queue" based on team population
	//
	if (teamProto)
	{
		if (teamProto->getIsSingleton()) {
			Team *singletonTeam = TheTeamFactory->findTeam( teamProto->getName() );
			if (singletonTeam && singletonTeam->hasAnyObjects()) {
				AsciiString teamStr = "Unable to recruit singleton team '";
				teamStr.concat("' because team already exists.");
				TheScriptEngine->AppendDebugMessage(teamStr, false);
				return;
			}
		}
		if (!teamProto->getTemplateInfo()->m_hasHomeLocation && !isSkirmishAI())
		{
			AsciiString teamStr = "Error : team '";
			teamStr.concat(teamProto->getName());
			teamStr.concat("' has no Home Position (or Origin).");
			TheScriptEngine->AppendDebugMessage(teamStr, false);
		}
		// create inactive team to place members into as they are built
		// when team is complete, the team is activated
		Team *theTeam = TheTeamFactory->createInactiveTeam( teamProto->getName() );
		AsciiString teamName = teamProto->getName();
		teamName.concat(" - Recruiting.");
		TheScriptEngine->AppendDebugMessage(teamName, false);
		const TCreateUnitsInfo *unitInfo = &teamProto->getTemplateInfo()->m_unitsInfo[0];
//		WorkOrder *orders = nullptr;
		Int i;
		Int unitsRecruited = 0;
		// Recruit.
		for( i=0; i<teamProto->getTemplateInfo()->m_numUnitsInfo; i++ )
		{
			const ThingTemplate *thing = TheThingFactory->findTemplate( unitInfo[i].unitThingName );
			if (thing)
			{
				int count = unitInfo[i].maxUnits;
				while (count>0) {
					Object *unit = theTeam->tryToRecruit(thing, &teamProto->getTemplateInfo()->m_homeLocation, recruitRadius);
					if (unit)
					{
						unitsRecruited++;

						AsciiString teamStr = "Team '";
						teamStr.concat(theTeam->getPrototype()->getName());
						teamStr.concat("' recruits ");
						teamStr.concat(thing->getName());
						teamStr.concat(" from team '");
						teamStr.concat(unit->getTeam()->getPrototype()->getName());
						teamStr.concat("'");
						TheScriptEngine->AppendDebugMessage(teamStr, false);

						unit->setTeam(theTeam);

						AIUpdateInterface *ai = unit->getAIUpdateInterface();
						if (ai)
						{
#ifdef DEBUG_LOGGING
							Coord3D pos = *unit->getPosition();
							Coord3D to = teamProto->getTemplateInfo()->m_homeLocation;
							DEBUG_LOG(("Moving unit from %f,%f to %f,%f", pos.x, pos.y , to.x, to.y ));
#endif
							ai->aiMoveToPosition( &teamProto->getTemplateInfo()->m_homeLocation, CMD_FROM_AI);
						}
					} else {
						break;
					}
					count--;
				}
			}
		}
		if (unitsRecruited>0)
		{
			/* We have something to build. */
			TeamInQueue *team = newInstance(TeamInQueue);
			// Put in front of queue.
			prependTo_TeamReadyQueue(team);
			team->m_priorityBuild = false;
			team->m_workOrders = nullptr;
			team->m_frameStarted = TheGameLogic->getFrame();
			team->m_team = theTeam;
			AsciiString teamName = teamProto->getName();
			teamName.concat(" - Finished recruiting.");
			TheScriptEngine->AppendDebugMessage(teamName, false);
		}	else {
			//disband.
			if (!theTeam->getPrototype()->getIsSingleton()) {
				deleteInstance(theTeam);
				theTeam = nullptr;
			}
			AsciiString teamName = teamProto->getName();
			teamName.concat(" - Recruited 0 units, disbanding.");
			TheScriptEngine->AppendDebugMessage(teamName, false);
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** Train our teams. */
// ------------------------------------------------------------------------------------------------
void AIPlayer::processTeamBuilding()
{
	// select a new team
	if (selectTeamToBuild()) {
		queueUnits();
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void AIPlayer::queueUnits()
{

	queueSupplyTruck();

	// For each member of the current team to build, try to find a faction building to build it.
	//
	for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
	{
		TeamInQueue *team = iter.cur();
		for( WorkOrder *order = team->m_workOrders; order; order = order->m_next )
		{
			// check if there is a unit on the map that we can steal (recruit) instead of building
			// @todo: Should this try to alter the home location of the recruiting area to
			// the center of the team, or to the home area of this player?
			Coord3D home = team->m_team->getPrototype()->getTemplateInfo()->m_homeLocation;
			Bool hasHome = false;
			if (team->m_team->getPrototype()->getTemplateInfo()->m_hasHomeLocation) {
				hasHome = true;
			} else {
				hasHome = getBaseCenter(&home);
			}
			while (order->isWaitingToBuild()) {

				Object *unit = team->m_team->tryToRecruit(order->m_thing, &home, TheAI->getAiData()->m_maxRecruitDistance);
				if (unit)
				{
					order->m_numCompleted++;

					AsciiString teamStr = "Team '";
					teamStr.concat(team->m_team->getPrototype()->getName());
					teamStr.concat("' recruits ");
					teamStr.concat(order->m_thing->getName());
					teamStr.concat(" from team '");
					teamStr.concat(unit->getTeam()->getPrototype()->getName());
					teamStr.concat("'");
					TheScriptEngine->AppendDebugMessage(teamStr, false);

					unit->setTeam(team->m_team);

					AIUpdateInterface *ai = unit->getAIUpdateInterface();
					if (hasHome) {
						ai->aiMoveToPosition( &home, CMD_FROM_AI);
					} else {
						ai->aiIdle(CMD_FROM_AI); // stop, you've been recruited.
					}
				}	else {
					break;
				}
			}
			if (order->isWaitingToBuild())
			{

				// start the creation of a new unit
				startTraining( order, team->m_priorityBuild, team->m_team->getName());
			}
			else
			{
				// we are under construction, verify our factory still exists
				order->validateFactory(m_player);
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if it's time to build another base building.
 */
void AIPlayer::doBaseBuilding()
{
	if (m_player->getCanBuildBase()) {
		// See if we are ready to start trying a structure.
		if (!m_readyToBuildStructure) {
			m_structureTimer--;
			if (m_structureTimer<=0) {
				m_readyToBuildStructure = true;
				m_buildDelay = 0;
			}
		}
		// This timer is to keep from banging on the logic each frame.  If something interesting
		// happens, like a building is added or a unit finished, the timers are shortcut.
		m_buildDelay--;
		if (m_buildDelay<1) {
			if (m_readyToBuildStructure) {
				processBaseBuilding();
			}
			if (m_buildDelay<1) {	// processBaseBuilding may reset m_buildDelay.
				m_buildDelay = 2*LOGICFRAMES_PER_SECOND; // check again in 2 seconds.
			}
			// Note that this timer gets shortcut when a building is completed.
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if any ready teams have finished moving to the rally point.
 */
void AIPlayer::checkReadyTeams()
{
	// See if any ready teams are gathered at their rally point
	{	// needed to scope iter.  silly ms c++.
		for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamReadyQueue(); !iter.done(); iter.advance())
		{
			TeamInQueue *team = iter.cur();
			// If 60 seconds passed, start anyway.
			Bool timeExpired = team->m_frameStarted+60*LOGICFRAMES_PER_SECOND < TheGameLogic->getFrame();
			Bool allIdle=TRUE;
			Bool anyIdle = FALSE;
			if (team->m_reinforcement) {
				Object *obj = TheGameLogic->findObjectByID(team->m_reinforcementID);
				if (obj && obj->getAIUpdateInterface()) {
					allIdle = obj->getAIUpdateInterface()->isIdle();
					anyIdle = allIdle;
				}
			} else {
				allIdle = team->m_team->isIdle();
				for (DLINK_ITERATOR<Object> iter = team->m_team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
					Object *obj = iter.cur();
					if (obj->getAI() && obj->getAI()->isIdle()) {
						anyIdle = true;
					}
				}
			}
			if (anyIdle && team->m_team->getPrototype()->getTemplateInfo()->m_executeActions) {
				const Script *script = TheScriptEngine->findScriptByName(team->m_team->getPrototype()->getTemplateInfo()->m_productionCondition);
				if (script && script->getAction()) {
					// we have a start action.  So don't wait for allIdle as the team may be guarding.
					allIdle = true;
				}
			}
			if (timeExpired) allIdle = true;
			if (allIdle) {
				if (!team->m_sentToStartLocation) {
					team->m_sentToStartLocation = true;
					/*
					if (team->m_team->getPrototype()->getTemplateInfo()->m_hasHomeLocation &&
							!team->m_reinforcement) {
 						AIGroupPtr theGroup = TheAI->createGroup();
						if (theGroup) {
							team->m_team->getTeamAsAIGroup(theGroup.Peek());
							Coord3D destination = team->m_team->getPrototype()->getTemplateInfo()->m_homeLocation;
							theGroup->groupTightenToPosition( &destination, false, CMD_FROM_AI );
							team->m_frameStarted = TheGameLogic->getFrame();
							continue;
						}
					}
					*/
				}
				// Start the team up.
				removeFrom_TeamReadyQueue(team);
				if (team->m_reinforcement) {
					Object *obj = TheGameLogic->findObjectByID(team->m_reinforcementID);
					if (obj&&obj->getAIUpdateInterface()) {
						obj->getAIUpdateInterface()->joinTeam();
					}
				} else {
					// mark our completed team as "active" - this will invoke any OnCreate scripts, etc.
					team->m_team->setActive();
					if (isSkirmishAI()) {
						TheScriptEngine->clearTeamFlags();
					}
					if (TheGlobalData->m_debugAI) {
						AsciiString teamName = team->m_team->getPrototype()->getName();
						teamName.concat(" - team activated.");
						TheScriptEngine->AppendDebugMessage(teamName, false);
					}
				}
				deleteInstance(team);
				iter = iterate_TeamReadyQueue();
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if any queued teams have finished building, or have run out of time.
 */
void AIPlayer::checkQueuedTeams()
{
	// See if any teams are expired.
	{	// needed to scope iter.  silly ms c++.
		for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
		{
			TeamInQueue *team = iter.cur();
			if (team && team->isBuildTimeExpired())	{
				if (team->isMinimumBuilt())
				{
					if (team->areBuildsComplete()) {
						// Move to ready queue
						removeFrom_TeamBuildQueue(team);
						prependTo_TeamReadyQueue(team);
					}	else {
						continue;
					}
				}	else {
					// Disband.
					removeFrom_TeamBuildQueue(team);
					team->disband();
					deleteInstance(team);
					if (isSkirmishAI()) {
						TheScriptEngine->clearTeamFlags();
					}
				}
				iter = iterate_TeamBuildQueue();
			}
		}
	}

	// See if any teams are ready.
	{	// needed to scope iter.  silly ms c++.
		for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
		{
			TeamInQueue *team = iter.cur();
			if (team && team->isAllBuilt())
			{
				// Move to ready queue
				removeFrom_TeamBuildQueue(team);
				prependTo_TeamReadyQueue(team);
				iter = iterate_TeamBuildQueue();
				continue;
			}
			Bool anyIdle = false;
			for (DLINK_ITERATOR<Object> iter = team->m_team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object *obj = iter.cur();
				if (obj && obj->getAI() && obj->getAI()->isIdle()) {
					anyIdle = true;
				}
			}
			if (anyIdle) {
				if (team->m_team->getPrototype()->getTemplateInfo()->m_executeActions) {
					const Script *script = TheScriptEngine->findScriptByName(team->m_team->getPrototype()->getTemplateInfo()->m_productionCondition);
					if (script) {
						TheScriptEngine->friend_executeAction(script->getAction(), team->m_team);
					}
				}
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if it is time to start another ai team building.
 */
void AIPlayer::doTeamBuilding()
{
	// See if any teams are expired.
	if (m_player->getCanBuildUnits()) {
		// See if we are ready to start trying a team.
		if (!m_readyToBuildTeam) {
			m_teamTimer--;
			if (m_teamTimer<=0) {
				m_readyToBuildTeam = true;
				m_teamDelay = 0;
			}
		}

		// This timer is to keep from banging on the logic each frame.  If something interesting
		// happens, like a building is added or a unit finished, the timers are shortcut.
		m_teamDelay--;
		if (m_teamDelay<1) {
			queueUnits(); // update the queues.
			if (m_readyToBuildTeam) {
				processTeamBuilding();
			}
			m_teamDelay = 5*LOGICFRAMES_PER_SECOND; // check again in 5 seconds.
			// Note that this timer gets shortcut when a unit or building is completed.
		}
	}
}

//----------------------------------------------------------------------------------------------------------
/**
 * See if it is time to start another upgrade or skill building.
 */
void AIPlayer::doUpgradesAndSkills()
{
	if (TheGameLogic->getFrame() < 2) {
		// can't do updates on the first few frames
		return;
	}

	Bool checkScience = m_player->getSciencePurchasePoints()>0;
	if (!checkScience) {
		return;
	}
	const AISideInfo *sideInfo = TheAI->getAiData()->m_sideInfo;
	while (sideInfo) {
		if (sideInfo->m_side == m_player->getSide()) {
			break;
		}
		sideInfo = sideInfo->m_next;
	}
	if (sideInfo == nullptr) return;

	if (m_skillsetSelector == INVALID_SKILLSET_SELECTION) {
		Int limit = 0;
		// Pick randomly among the skillsets that have skills.
		// Designers sometimes only define skillset 1 & 2, or some such.  jba.
		if (sideInfo->m_skillSet2.m_numSkills>0) {
			limit = 1;
			if (sideInfo->m_skillSet3.m_numSkills>0) {
				limit = 2;
				if (sideInfo->m_skillSet4.m_numSkills>0) {
					limit = 3;
					if (sideInfo->m_skillSet5.m_numSkills>0) {
						limit = 4;
					}
				}
			}
		}
		if (isSkirmishAI()) {
			m_skillsetSelector = GameLogicRandomValue(0, limit);
		} else {
			m_skillsetSelector = 0; // Non-skirmish default to 0.  jba.
		}
	}

	// SKILLS
	if (m_player->getSciencePurchasePoints()>0) {
		const TSkillSet *skillset;
		switch(m_skillsetSelector) {
			default:
			case 0: skillset = &sideInfo->m_skillSet1; break;
			case 1: skillset = &sideInfo->m_skillSet2; break;
			case 2: skillset = &sideInfo->m_skillSet3; break;
			case 3: skillset = &sideInfo->m_skillSet4; break;
			case 4: skillset = &sideInfo->m_skillSet5; break;
		}
		Int i;
		for (i=0; i<skillset->m_numSkills; i++) {
			ScienceType science = skillset->m_skills[i];
			if (m_player->isCapableOfPurchasingScience(science)) {
				if (m_player->attemptToPurchaseScience(science)) {
						AsciiString msg = TheNameKeyGenerator->keyToName(m_player->getPlayerNameKey());
						msg.concat(" purchases from SkillSet");
						msg.concat('1'+m_skillsetSelector);
						msg.concat(' ');
						msg.concat(TheScienceStore->getInternalNameForScience(science));
						msg.concat(".");
						TheScriptEngine->AppendDebugMessage( msg, false);
				}
			}
		}
	}


}

//----------------------------------------------------------------------------------------------------------
/**
 * Perform computer-controlled player AI
 */
//DECLARE_PERF_TIMER(AIPlayer_update)
void AIPlayer::update()
{
	//USE_PERF_TIMER(AIPlayer_update)

	doBaseBuilding();		// See if it's time to build another building.

	checkReadyTeams(); // See if any teams are ready to start.

	checkQueuedTeams(); // See if any teams are complete.

	doTeamBuilding(); // See if it's time to start another team.

	doUpgradesAndSkills(); // See if it's time to build an upgrade or buy a skill.

	updateBridgeRepair(); // Handle any bridge repairs.

}

//----------------------------------------------------------------------------------------------------------
/**
 * Find any things that build stuff & add them to the build list.  Then build any initially built
 * buildings.
 */
void AIPlayer::newMap()
{
	BuildListInfo *info = m_player->getBuildList();
	// Add any factories placed to the build list.
	Object *obj;
	for( obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() )
	{

		Player *owner = obj->getControllingPlayer();
		if (owner==m_player) {
			// See if it's a factory.
			ProductionUpdateInterface *pu = obj->getProductionUpdateInterface();
			// If it doesn't produce, continue.
			if (!pu) continue;
			m_player->addToBuildList(obj);
		}

	}
	computeCenterAndRadiusOfBase(&m_baseCenter, &m_baseRadius);

	// Build any with the initially built flag.
	for( /* nothing */; info; info = info->getNext() )
	{
		AsciiString name = info->getTemplateName();
		if (name.isEmpty()) continue;
		const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( name );
		if (!bldgPlan) {
			DEBUG_LOG(("*** ERROR - Build list building '%s' doesn't exist.", name.str()));
			continue;
		}
		if (info->isInitiallyBuilt()) {
			buildStructureNow(bldgPlan, info);
		} else {
			info->incrementNumRebuilds(); // the initial build in the normal build list consumes a rebuild, so add one.
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** Find the center of the base and the radius of buildings.  */
// ------------------------------------------------------------------------------------------------
void AIPlayer::computeCenterAndRadiusOfBase(Coord3D *center, Real *radius)
{
	//
	BuildListInfo *info;
	Coord2D totalPos;
	totalPos.x = 0;
	totalPos.y = 0;
	Int numBldg=0;
	for( info = m_player->getBuildList(); info; info = info->getNext() )
	{
		AsciiString name = info->getTemplateName();
		if (name.isEmpty()) continue;
		const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( name );
		if (!bldgPlan) {
			continue;
		}
		Coord3D pos = *info->getLocation();
		totalPos.x += pos.x;
		totalPos.y += pos.y;
		numBldg++;
	}
	if (numBldg>0) {
		totalPos.x /= numBldg;
		totalPos.y /= numBldg;
	}

	m_baseCenterSet = numBldg>0;
	center->x = totalPos.x;
	center->y = totalPos.y;

	Real maxRadSqr = 0;
	//
	for( info = m_player->getBuildList(); info; info = info->getNext() )
	{
		AsciiString name = info->getTemplateName();
		if (name.isEmpty()) continue;
		const ThingTemplate *bldgPlan = TheThingFactory->findTemplate( name );
		if (!bldgPlan) {
			continue;
		}
		Coord3D pos = *info->getLocation();
		Real dx = pos.x-center->x;
		Real dy = pos.y-center->y;
		if (dx<0) dx = -dx;
		if (dy<0) dy = -dy;
		Real bldgRadius = bldgPlan->getTemplateGeometryInfo().getBoundingCircleRadius()*0.4f;
		dx += bldgRadius;
		dy += bldgRadius;
		Real radSqr = dx*dx+dy*dy;
		if (radSqr>maxRadSqr) maxRadSqr=radSqr;
	}
	*radius = WWMath::SqrtOrigin(maxRadSqr);
}

//----------------------------------------------------------------------------------------------------------
/**
 * Checks to see if we're building a dozer.
 */
Bool AIPlayer::dozerInQueue()
{
	{	// needed to scope iter.  silly ms c++.
		for ( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance())
		{
			TeamInQueue *team = iter.cur();
			if (team && team->includesADozer() )
			{
				return true; // dozer is building already.
			}
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------
/**
 * Queues up a dozer.
 */
void AIPlayer::queueDozer()
{

	if (dozerInQueue()) return;
	// Find a factory that can build a dozer.

	Bool canBuildUnits = m_player->getCanBuildUnits();
	// If we need a dozer, turn on unit building for a moment.
	m_player->setCanBuildUnits(true);
	const ThingTemplate *tTemplate = TheThingFactory->firstTemplate();
	while (tTemplate) {
		if (tTemplate->isKindOf(KINDOF_DOZER)) {
			Object *factory = findFactory(tTemplate, true);
			if (factory) {
				// we can build one.
				WorkOrder *order = newInstance(WorkOrder);
				order->m_thing = tTemplate;
				order->m_factoryID = INVALID_ID;
				order->m_numRequired = 1;
				order->m_required = true;
				order->m_isResourceGatherer = FALSE;
				// prepend to head of list
				order->m_next = nullptr;
				TeamInQueue *team = newInstance(TeamInQueue);
				// Put in front of queue.
				prependTo_TeamBuildQueue(team);
				team->m_priorityBuild = true;
				team->m_workOrders = order;
				team->m_frameStarted = TheGameLogic->getFrame();
				// Stick it on the default team
				team->m_team = m_player->getDefaultTeam();
				AsciiString teamName = "DOZER - building one at the ";
				teamName.concat(factory->getTemplate()->getName());
				TheScriptEngine->AppendDebugMessage(teamName, false);
				m_teamDelay = 0;
				startTraining( order, team->m_priorityBuild, team->m_team->getName());
				break;
			}
		}
		tTemplate = tTemplate->friend_getNextTemplate();
	}
	// restore canbuildunits.
	m_player->setCanBuildUnits(canBuildUnits);
}


//-------------------------------------------------------------------------------------------------
/** Difficulty level for this player */
//-------------------------------------------------------------------------------------------------
enum GameDifficulty AIPlayer::getAIDifficulty() const
{
	return m_difficulty;
}


//----------------------------------------------------------------------------------------------------------
/**
 * Finds a dozer that isn't building or collecting resources.
 */
Object * AIPlayer::findDozer( const Coord3D *pos )
{
	// Add any factories placed to the build list.
	Object *obj;
	Object *dozer = nullptr;
	Bool needDozer = true;
	Object *closestDozer=nullptr;
	Real closestDistSqr = 0;

	for( obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject() )
	{

		Player *owner = obj->getControllingPlayer();
		if (owner==m_player) {
			// See if it's a dozer.
			if (obj->isKindOf(KINDOF_DOZER)) {

				AIUpdateInterface *ai = obj->getAIUpdateInterface();
				if (ai==nullptr) {
					continue;
				}


				DozerAIInterface* dozerAI = ai->getDozerAIInterface();
				if (dozerAI) {
					// Since workers can be dozers, hmmm....
					SupplyTruckAIInterface* supplyTruckAI = ai->getSupplyTruckAIInterface();
					if( !dozerAI->isAnyTaskPending() && supplyTruckAI ) {
						// If it is gathering supplies, don't steal it.
						if (supplyTruckAI->isCurrentlyFerryingSupplies() || supplyTruckAI->isForcedIntoWantingState())
						{
							continue;
						}
					}
					if (obj->getID() == m_repairDozer) {
						continue; // don't steal the repair dozer.
					}
					needDozer = false; // dozer exists, may be busy.
					if (dozerAI->isTaskPending(DOZER_TASK_BUILD)) {
						continue; // already building.
					}
					if (!dozerAI->isAnyTaskPending()) {
						dozer = obj; // prefer an idle dozer
					}
					if (dozer==nullptr) {
						dozer = obj; // but we'll take one doing stuff.
					}
					if (dozer && !dozerAI->isAnyTaskPending()) {
						// Got a good one, track closest.
						Real distSqr;
						Real dx, dy;
						dx = pos->x - dozer->getPosition()->x;
						dy = pos->y - dozer->getPosition()->y;
						distSqr = dx*dx+dy*dy;
						if (closestDozer == nullptr) {
							closestDozer = dozer;
							closestDistSqr = distSqr;
						} else if (distSqr < closestDistSqr) {
							closestDozer = dozer;
							closestDistSqr = distSqr;
						}
					}
				}
			}
		}

	}
	if (needDozer) {
		queueDozer();
	}
	if (closestDozer) return closestDozer;
	return dozer;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void AIPlayer::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: added m_teamSeconds delay.
	* 3: Added m_curWarehouseID.
	* 1: Reset back to 1 with major save file changes.
*/
// ------------------------------------------------------------------------------------------------
void AIPlayer::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// team build queue count
	UnsignedShort teamBuildQueueCount = 0;
	for( DLINK_ITERATOR< TeamInQueue > teamInQueueIt = iterate_TeamBuildQueue();
			 teamInQueueIt.done() == FALSE;
			 teamInQueueIt.advance() )
		teamBuildQueueCount++;
	xfer->xferUnsignedShort( &teamBuildQueueCount );

	// team build queue data
	TeamInQueue *teamInQueue;
	if( xfer->getXferMode() == XFER_SAVE )
	{

		for( DLINK_ITERATOR< TeamInQueue > teamInQueueIt = iterate_TeamBuildQueue();
				 teamInQueueIt.done() == FALSE;
				 teamInQueueIt.advance() )
		{

			// get element data
			teamInQueue = teamInQueueIt.cur();

			// xfer it
			xfer->xferSnapshot( teamInQueue );

		}

	}
	else
	{

		// sanity, the list must be empty
		if( getFirstItemIn_TeamBuildQueue() != nullptr )
		{

			DEBUG_CRASH(( "AIPlayer::xfer - TeamBuildQueue head is not null, you should delete it or something before loading a new list" ));
			throw SC_INVALID_DATA;

		}

		// ready all data
		for( UnsignedShort i = 0; i < teamBuildQueueCount; ++i )
		{

			// allocate new team in queue instance
			teamInQueue = newInstance(TeamInQueue);

			// attach to end of list
			prependTo_TeamBuildQueue( teamInQueue );

			// xfer data
			xfer->xferSnapshot( teamInQueue );

		}

		// the list was loaded in reverse order, reverse the list so it's in the same order as before
		reverse_TeamBuildQueue();

	}

	// team ready queue count
	UnsignedShort teamReadyQueueCount = 0;
	for( DLINK_ITERATOR< TeamInQueue > teamReadyQueueIt = iterate_TeamReadyQueue();
			 teamReadyQueueIt.done() == FALSE;
			 teamReadyQueueIt.advance() )
		teamReadyQueueCount++;
	xfer->xferUnsignedShort( &teamReadyQueueCount );

	// team Ready queue data
	TeamInQueue *teamReadyQueue;
	if( xfer->getXferMode() == XFER_SAVE )
	{

		for( DLINK_ITERATOR< TeamInQueue > teamReadyQueueIt = iterate_TeamReadyQueue();
				 teamReadyQueueIt.done() == FALSE;
				 teamReadyQueueIt.advance() )
		{

			// get element
			teamReadyQueue = teamReadyQueueIt.cur();

			// xfer data
			xfer->xferSnapshot( teamReadyQueue );

		}

	}
	else
	{

		// sanity, the list must be empty
		if( getFirstItemIn_TeamReadyQueue() != nullptr )
		{

			DEBUG_CRASH(( "AIPlayer::xfer - TeamReadyQueue head is not null, you should delete it or something before loading a new list" ));
			throw SC_INVALID_DATA;

		}

		// read all data
		for( UnsignedShort i = 0; i < teamReadyQueueCount; ++i )
		{

			// allocate new team in queue instance
			teamInQueue = newInstance(TeamInQueue);

			// attach to end of list
			prependTo_TeamReadyQueue( teamInQueue );

			// xfer data
			xfer->xferSnapshot( teamInQueue );

		}

		// reverse the list since it was loaded in reverse order due to the prepend
		reverse_TeamReadyQueue();

	}

	// xfer player index ... this is really just for sanity
	PlayerIndex playerIndex = m_player->getPlayerIndex();
	xfer->xferUser( &playerIndex, sizeof( PlayerIndex ) );
	if( playerIndex != m_player->getPlayerIndex() )
	{

		DEBUG_CRASH(( "AIPlayer::xfer - player index mismatch" ));
		throw SC_INVALID_DATA;

	}

	// xfer the rest of the ai player data (it's pretty straight forward)
	xfer->xferBool( &m_readyToBuildTeam );
	xfer->xferBool( &m_readyToBuildStructure );
	xfer->xferInt( &m_teamTimer );
	xfer->xferInt( &m_structureTimer );

	xfer->xferInt( &m_buildDelay );
	xfer->xferInt( &m_teamDelay );

	xfer->xferInt(&m_teamSeconds);
 	xfer->xferObjectID(&m_curWarehouseID);

	xfer->xferInt( &m_frameLastBuildingBuilt );

	xfer->xferUser( &m_difficulty, sizeof( GameDifficulty ) );
	xfer->xferInt( &m_skillsetSelector );

	xfer->xferCoord3D( &m_baseCenter );
	xfer->xferBool( &m_baseCenterSet );
	xfer->xferReal( &m_baseRadius );

	xfer->xferUser( m_structuresToRepair, sizeof( ObjectID ) * MAX_STRUCTURES_TO_REPAIR );
	xfer->xferObjectID( &m_repairDozer );
	xfer->xferInt( &m_structuresInQueue );
	xfer->xferBool( &m_dozerQueuedForRepair );
	xfer->xferBool( &m_dozerIsRepairing );
	xfer->xferInt( &m_bridgeTimer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void AIPlayer::loadPostProcess()
{

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
TeamInQueue::~TeamInQueue()
{
	WorkOrder *order, *next;

	for( order = m_workOrders; order; order = next )
	{
		next = order->m_next;
		deleteInstance(order);
	}
	// If we have a team, activate it.  If it is empty, Team.cpp will remove empty active teams.
	if (m_team) m_team->setActive();
	m_workOrders = nullptr;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TeamInQueue::isAllBuilt()
{
	WorkOrder *order;

	Bool stillBuilding = false;
	for( order = m_workOrders; order; order = order->m_next )
	{
		if (order->m_numRequired>order->m_numCompleted)
		{
			stillBuilding = true;
		}
	}
	return !stillBuilding;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TeamInQueue::isBuildTimeExpired()
{
	if (m_team->getPrototype()->getTemplateInfo()->m_initialIdleFrames<1) {
		return false; // Unlimited time.
	}
	if (TheGameLogic->getFrame() > m_frameStarted + m_team->getPrototype()->getTemplateInfo()->m_initialIdleFrames) {
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TeamInQueue::isMinimumBuilt()
{
	WorkOrder *order;

	for( order = m_workOrders; order; order = order->m_next )
	{
		Int count = order->m_numCompleted;
		if (order->m_factoryID != INVALID_ID) {
			count++; // we have one building.
		}
		if (order->m_numRequired>count)
		{
			if (order->m_required) {
				return false; // required units not built.
			}
		}
	}
	return true;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TeamInQueue::includesADozer()
{
	WorkOrder *order;

	for( order = m_workOrders; order; order = order->m_next )
	{
		// GLA dozers (workers) are also resource gatherers, so make sure it isn't a gatherer. jba.
		if (order->m_thing->isKindOf(KINDOF_DOZER) && !order->m_isResourceGatherer) {
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TeamInQueue::areBuildsComplete()
{
	WorkOrder *order;

	for( order = m_workOrders; order; order = order->m_next )
	{
		if (order->m_factoryID != INVALID_ID) {
			return false; // we have one building.
		}
	}
	return true;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TeamInQueue::disband()
{
	Team *newTeam = m_team->getPrototype()->getControllingPlayer()->getDefaultTeam();
	AsciiString teamName = m_team->getPrototype()->getName();
	teamName.concat(" - team disbanded, build time expired.");
	TheScriptEngine->AppendDebugMessage(teamName, false);
	if (m_team != newTeam) {
		m_team->transferUnitsTo(newTeam);
		if (!m_team->getPrototype()->getIsSingleton()) {
			deleteInstance(m_team);
		}
		m_team = nullptr;
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TeamInQueue::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void TeamInQueue::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// xfer work order count
	UnsignedShort workOrderCount = 0;
	WorkOrder *workOrder;
	for( workOrder = m_workOrders; workOrder; workOrder = workOrder->m_next )
		workOrderCount++;
	xfer->xferUnsignedShort( &workOrderCount );

	// xfer work orders
	if( xfer->getXferMode() == XFER_SAVE )
	{

		// xfer each work order
		for( workOrder = m_workOrders; workOrder; workOrder = workOrder->m_next )
		{

			// xfer work order data
			xfer->xferSnapshot( workOrder );

		}

	}
	else
	{

		// sanity
		if( m_workOrders != nullptr )
		{

			DEBUG_CRASH(( "TeamInQueue::xfer - m_workOrders should be null but isn't.  Perhaps you should blow it away before loading" ));
			throw SC_INVALID_DATA;

		}

		// load all work orders
		for( UnsignedShort i = 0; i < workOrderCount; ++i )
		{

			// allocate new work order
			workOrder = newInstance(WorkOrder);

			// attach to list at the end
			workOrder->m_next = nullptr;
			if( m_workOrders == nullptr )
				m_workOrders = workOrder;
			else
			{
				WorkOrder *last = m_workOrders;

				while( last->m_next != nullptr )
					last = last->m_next;

				last->m_next = workOrder;

			}

			// load work order data
			xfer->xferSnapshot( workOrder );

		}

	}

	// xfer the rest of the team in queue data
	xfer->xferBool( &m_priorityBuild );
	TeamID teamID = m_team ? m_team->getID() : TEAM_ID_INVALID;
	xfer->xferUser( &teamID, sizeof( TeamID ) );
	if( xfer->getXferMode() == XFER_LOAD )
		m_team = TheTeamFactory->findTeamByID( teamID );
	xfer->xferInt( &m_frameStarted );
	xfer->xferBool( &m_sentToStartLocation );
	xfer->xferBool( &m_stopQueueing );
	xfer->xferBool( &m_reinforcement );
	xfer->xferObjectID( &m_reinforcementID );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TeamInQueue::loadPostProcess()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WorkOrder::~WorkOrder()
{

}

// ------------------------------------------------------------------------------------------------
/** Verify factoryID still refers to an active object */
// ------------------------------------------------------------------------------------------------
void WorkOrder::validateFactory( Player *thisPlayer )
{

	if (m_factoryID == INVALID_ID)
		return;
	Object *factory = TheGameLogic->findObjectByID( m_factoryID );
	if ( factory == nullptr) {
		m_factoryID = INVALID_ID;
		return;
	}
	if (factory->getControllingPlayer()!=thisPlayer) {
		m_factoryID = INVALID_ID;
	}

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WorkOrder::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void WorkOrder::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// thing template
	AsciiString thingTemplateName = m_thing ? m_thing->getName() : AsciiString::TheEmptyString;
	xfer->xferAsciiString( &thingTemplateName );
	if( xfer->getXferMode() == XFER_LOAD )
		m_thing = TheThingFactory->findTemplate( thingTemplateName );

	// factory id
	xfer->xferObjectID( &m_factoryID );

	// num completed
	xfer->xferInt( &m_numCompleted );

	// num required
	xfer->xferInt( &m_numRequired );

	// is required
	xfer->xferBool( &m_required );

	// is resource gatherer
	xfer->xferBool( &m_isResourceGatherer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WorkOrder::loadPostProcess()
{

}


//----------------------------------------------------------------------------------------------------------
/**
 * Get the bounds for a player's structure.
 */
void AIPlayer::getPlayerStructureBounds(Region2D *bounds, Int playerNdx )
{
	Player::PlayerTeamList::const_iterator it;
	Bool firstObject = true;
	Bool firstStructure = true;
	bounds->hi.x = bounds->lo.x = bounds->hi.y = bounds->lo.x = 0;
	Region2D objBounds;
	objBounds.hi.x = objBounds.lo.x = objBounds.hi.y = objBounds.lo.x = 0;

	Player* pPlayer = ThePlayerList->getNthPlayer(playerNdx);
	if (pPlayer == nullptr) return;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) continue;
			for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object *pObj = iter.cur();
				if (!pObj) continue;
				if (pObj->isKindOf(KINDOF_STRUCTURE)) {
					Coord3D pos = *pObj->getPosition();
					if (firstObject) {
						objBounds.lo.x = objBounds.hi.x = pos.x;
						objBounds.lo.y = objBounds.hi.y = pos.y;
						firstObject = false;
					}	else {
						if (objBounds.lo.x>pos.x) objBounds.lo.x = pos.x;
						if (objBounds.lo.y>pos.y) objBounds.lo.y = pos.y;
						if (objBounds.hi.x<pos.x) objBounds.hi.x = pos.x;
						if (objBounds.hi.y<pos.y) objBounds.hi.y = pos.y;
					}
					if (firstStructure) {
						bounds->lo.x = bounds->hi.x = pos.x;
						bounds->lo.y = bounds->hi.y = pos.y;
						firstStructure = false;
					}	else {
						if (bounds->lo.x>pos.x) bounds->lo.x = pos.x;
						if (bounds->lo.y>pos.y) bounds->lo.y = pos.y;
						if (bounds->hi.x<pos.x) bounds->hi.x = pos.x;
						if (bounds->hi.y<pos.y) bounds->hi.y = pos.y;
					}
				}
			}
		}
	}
	if (!firstStructure) {
		// Player had no structures, so use unit bounds.
		*bounds = objBounds;
	}
}
