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

// FILE: PlaceEventTranslator.cpp ///////////////////////////////////////////////////////////
// Author: Steven Johnson, Dec 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/BuildAssistant.h"
#include "Common/GameAudio.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"

#include "GameClient/CommandXlat.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/Eva.h"
#include "GameClient/PlaceEventTranslator.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

#include "GameLogic/Module/ProductionUpdate.h"



//-------------------------------------------------------------------------------------------------
PlaceEventTranslator::PlaceEventTranslator() : m_frameOfUpButton(-1)
{
}

//-------------------------------------------------------------------------------------------------
PlaceEventTranslator::~PlaceEventTranslator()
{
}

//-------------------------------------------------------------------------------------------------
/** Translator to process raw input messages into the "place something" message(s) */
//-------------------------------------------------------------------------------------------------
GameMessageDisposition PlaceEventTranslator::translateGameMessage(const GameMessage *msg)
{
	GameMessageDisposition disp = KEEP_MESSAGE;

	switch(msg->getType())
	{

		//---------------------------------------------------------------------------------------------
		// TheSuperHackers @bugfix Prevent double-clicks from falling through to other translators during building placement
		case GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK:
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
		{
			// if we're in a building placement mode, do the place and send to all players
			const ThingTemplate *build = TheInGameUI->getPendingPlaceType();
			if( build && TheInGameUI->isPlacementAnchored() == FALSE )
			{
				ICoord2D mouse = msg->getArgument(0)->pixel;
				Coord3D world;

				// translate mouse position to world position
				if( !TheTacticalView->screenToTerrain( &mouse, &world ) )
				{
					TheInGameUI->placeBuildAvailable( nullptr, nullptr );
					break;
				}

				//
				// placing things causes a dozer to go over and build it ... get the dozer in question
				// from the in game UI
				//
				Object *builderObject = TheGameLogic->findObjectByID( TheInGameUI->getPendingPlaceSourceObjectID() );

				// if our source object is gone cancel this whole placement process
				if( builderObject == nullptr )
				{

					TheInGameUI->placeBuildAvailable( nullptr, nullptr );
					break;

				}

				// set this location as the placement anchor
				TheInGameUI->setPlacementStart( &mouse );

/*
//
// This block of code checks for valid placement on a down mouse click, but since we can
// rotate a building into a valid location, this check prevents us from placing things
// down in some legal locations
//
				// get the type of thing we want to build
				const ThingTemplate *whatToBuild = TheInGameUI->getPendingPlaceType();

				//
				// if the spot at which they choose to place this thing is illegal we won't start
				// the placement anchor, instead we play a "can't do that" sound
				//
				LegalBuildCode lbc;
				lbc = TheBuildAssistant->isLocationLegalToBuild( &world,
																												 whatToBuild,
																												 TheInGameUI->getPlacementAngle(),
																												 BuildAssistant::USE_QUICK_PATHFIND |
																												 BuildAssistant::TERRAIN_RESTRICTIONS |
																												 BuildAssistant::CLEAR_PATH |
																												 BuildAssistant::NO_OBJECT_OVERLAP,
																												 builderObject );
				if( lbc != LBC_OK )
				{
					static const Sound *noCanDoSound = TheAudio->Sounds->getSound( "NoCanDoSound" );

					// play a can't do that sound
					TheAudio->Sounds->playSound( noCanDoSound );

					// display a message to the user as to why you can't build there
					TheInGameUI->displayCantBuildMessage( lbc );

				}
				else
				{

					// start placement anchor
					TheInGameUI->setPlacementStart(&mouse);

				}
*/

				// used the input
				disp = DESTROY_MESSAGE;

			}
			break;
		}

		//---------------------------------------------------------------------------------------------
		case GameMessage::MSG_MOUSE_LEFT_DOUBLE_CLICK:
		case GameMessage::MSG_MOUSE_LEFT_CLICK:
		{
			// if we're in a building placement mode, do the place and send to all players
			const ThingTemplate *build = TheInGameUI->getPendingPlaceType();

			// ... and also remove any radius cursor that is active.
			// (srj sez: not sure if this is always necessary... more of a failsafe to make it go away.)
			TheInGameUI->setRadiusCursorNone();

			if (build && TheInGameUI->isPlacementAnchored())
			{
				GameMessage *placeMsg;
				Coord3D worldStart, worldEnd;
				Real angle;
				ICoord2D anchorStart, anchorEnd;
				Bool isLineBuild = TheBuildAssistant->isLineBuildTemplate( build );

				// get the angle of the drawable at the cursor to use as the initial angle
				angle = TheInGameUI->getPlacementAngle();

				// get start point from the anchor arrow used to place and select angles
				TheInGameUI->getPlacementPoints( &anchorStart, &anchorEnd );

				// translate the screen position of start to world target location
				if( !TheTacticalView->screenToTerrain( &anchorStart, &worldStart ) )
					break;

				if( isLineBuild && !TheTacticalView->screenToTerrain( &anchorEnd, &worldEnd ) )
					break;

				Object *builderObj = TheGameLogic->findObjectByID( TheInGameUI->getPendingPlaceSourceObjectID() );

				//Kris: September 27, 2002
				//Make sure we have enough CASH to build it! It's possible that between the
				//time we initiated it and the time we confirm it, a hacker has stolen some of
				//our cash!
				CanMakeType cmt = TheBuildAssistant->canMakeUnit( builderObj, build );
				if( cmt != CANMAKE_OK )
				{
					if (cmt == CANMAKE_NO_MONEY)
					{
						TheEva->setShouldPlay(EVA_InsufficientFunds);
						TheInGameUI->message( "GUI:NotEnoughMoneyToBuild" );
						break;
					}
					else if (cmt == CANMAKE_QUEUE_FULL)
					{
						TheInGameUI->message( "GUI:ProductionQueueFull" );
						break;
					}
					else if (cmt == CANMAKE_PARKING_PLACES_FULL)
					{
						TheInGameUI->message( "GUI:ParkingPlacesFull" );
						break;
					}
					else if( cmt == CANMAKE_MAXED_OUT_FOR_PLAYER )
					{
						TheInGameUI->message( "GUI:UnitMaxedOut" );
						break;
					}
					// get out of pending placement mode, this will also clear the arrow anchor status
					TheInGameUI->placeBuildAvailable( nullptr, nullptr );
					break;
				}

				DEBUG_ASSERTCRASH(builderObj != nullptr, ("builderObj is null"));

				// check to see if this is a legal location to build something at
				LegalBuildCode lbc;
				lbc = TheBuildAssistant->isLocationLegalToBuild( &worldStart,
																												 build,
																												 angle,
																												 BuildAssistant::USE_QUICK_PATHFIND |
																												 BuildAssistant::TERRAIN_RESTRICTIONS |
																												 BuildAssistant::CLEAR_PATH |
																												 BuildAssistant::NO_OBJECT_OVERLAP |
																												 BuildAssistant::SHROUD_REVEALED |
																												 BuildAssistant::IGNORE_STEALTHED |
																												 BuildAssistant::FAIL_STEALTHED_WITHOUT_FEEDBACK,
																												 builderObj, nullptr );
				if( lbc == LBC_OK )
				{
					//Are we building this structure via the special power system? (special case for sneak attack)
					if( builderObj )
					{
						ProductionUpdateInterface *puInterface = builderObj->getProductionUpdateInterface();
						if( puInterface )
						{
							const CommandButton *commandButton = puInterface->getSpecialPowerConstructionCommandButton();
							if( commandButton )
							{
								//If we get this far, then we aren't going to really build the object using the production update
								//interface. Instead, we're going to trigger the special power to create it magically without a
								//dozer/worker.
								placeMsg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION );
								placeMsg->appendIntegerArgument( commandButton->getSpecialPowerTemplate()->getID() ); //The ID of the special power template.
								placeMsg->appendLocationArgument( worldStart ); //Position of special to be fired.
								placeMsg->appendRealArgument( angle ); //Angle of special to be fired.
								placeMsg->appendObjectIDArgument( INVALID_ID ); //There is no object in the way.
								placeMsg->appendIntegerArgument( commandButton->getOptions() ); //Command button options.
								placeMsg->appendObjectIDArgument( builderObj->getID() ); //The source object responsible for firing the special.

								// get out of pending placement mode, this will also clear the arrow anchor status
								TheInGameUI->placeBuildAvailable( nullptr, nullptr );

								// used the input
								disp = DESTROY_MESSAGE;
								m_frameOfUpButton = TheGameLogic->getFrame();
								break;
							}
						}
					}

					// create the right kind of message
					if( isLineBuild )
						placeMsg = TheMessageStream->appendMessage( GameMessage::MSG_DOZER_CONSTRUCT_LINE );
					else
						placeMsg = TheMessageStream->appendMessage( GameMessage::MSG_DOZER_CONSTRUCT );

					placeMsg->appendIntegerArgument(build->getTemplateID());
					placeMsg->appendLocationArgument(worldStart);
					placeMsg->appendRealArgument(angle);
					if( isLineBuild )
					{
						placeMsg->appendLocationArgument( worldEnd );
					}

					pickAndPlayUnitVoiceResponse( TheInGameUI->getAllSelectedDrawables(), placeMsg->getType() );

					// get out of pending placement mode, this will also clear the arrow anchor status
					TheInGameUI->placeBuildAvailable( nullptr, nullptr );

				}
				else
				{
					// can't place, display why
					TheInGameUI->displayCantBuildMessage( lbc );

					//Cannot build here -- play the voice sound from the dozer
					AudioEventRTS sound = *builderObj->getTemplate()->getPerUnitSound( "VoiceNoBuild" );
					sound.setObjectID( builderObj->getID() );
					TheAudio->addAudioEvent( &sound );

					// play a can't do that sound (UI beep type sound)
					static AudioEventRTS noCanDoSound( "NoCanDoSound" );
					TheAudio->addAudioEvent( &noCanDoSound );

					// unhook the anchor so they can try again
					TheInGameUI->setPlacementStart( nullptr );

				}

				// used the input
				disp = DESTROY_MESSAGE;
				m_frameOfUpButton = TheGameLogic->getFrame();

			}

			if (disp == DESTROY_MESSAGE)
				TheInGameUI->clearAttackMoveToMode();

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GameMessage::MSG_RAW_MOUSE_POSITION:
		{
			// if a building placement is in progress update the destination position
			if (TheInGameUI->isPlacementAnchored())
			{
				const Int PLACEMENT_DRAG_THRESHOLD_DIST = 5;  // in pixels away from anchor point
				ICoord2D mouse = msg->getArgument(0)->pixel;

				//
				// we will only process placement end point sets (clicking, and dragging to set angles)
				// if we have moved far enough away from the start point
				//
				ICoord2D start;
				TheInGameUI->getPlacementPoints( &start, nullptr );

				Int x, y;
				x = mouse.x - start.x;
				y = mouse.y - start.y;
				if( WWMath::SqrtOrigin( (x * x) + (y * y) ) >= PLACEMENT_DRAG_THRESHOLD_DIST )
				{

					TheInGameUI->setPlacementEnd(&mouse);
					disp = DESTROY_MESSAGE;

				}

			}
			break;
		}
	}

	return disp;
}


