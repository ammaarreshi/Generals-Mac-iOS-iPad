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

// FILE: W3DWaypointBuffer.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   Command & Conquers: Generals
//
// File name: W3DWaypointBuffer.cpp
//
// Created:   Kris Morness, October 2002
//
// Desc:      Draw buffer to handle all the waypoints in the scene. Waypoints
//            are rendered after terrain, after roads & bridges, and after
//            global fog, but before structures, objects, units, trees, etc.
//            This way if we have two waypoints at the bottom of a hill but
//            going through the hill, the line won't get cut off. However,
//            structures and units on top of paths will render above it. Waypoints
//            are only shown for selected units while in waypoint plotting mode.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//         Includes
//-----------------------------------------------------------------------------

#include "W3DDevice/GameClient/W3DWaypointBuffer.h"

#include <assetmgr.h>
#include <texture.h>

#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/RandomValue.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"

#include "GameLogic/Object.h"

#include "GameLogic/Module/AIUpdate.h"

#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/HeightMap.h"

#include "WW3D2/camera.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/segline.h"


#define MAX_DISPLAY_NODES 512





//=============================================================================
// W3DWaypointBuffer::W3DWaypointBuffer
//=============================================================================
/** Constructor. Sets m_initialized to true if it finds the w3d models it needs
for the bibs. */
//=============================================================================
W3DWaypointBuffer::W3DWaypointBuffer()
{
	m_waypointNodeRobj = WW3DAssetManager::Get_Instance()->Create_Render_Obj( "SCMNode" );
	m_line = new SegmentedLineClass;

	m_texture = WW3DAssetManager::Get_Instance()->Get_Texture( "EXLaser.tga" );


  setDefaultLineStyle();
}

//=============================================================================
// W3DWaypointBuffer::~W3DWaypointBuffer
//=============================================================================
/** Destructor. Releases w3d assets. */
//=============================================================================
W3DWaypointBuffer::~W3DWaypointBuffer()
{
	REF_PTR_RELEASE( m_waypointNodeRobj );
	REF_PTR_RELEASE( m_texture );
	REF_PTR_RELEASE( m_line );
}

//=============================================================================
// W3DWaypointBuffer::freeBibBuffers
//=============================================================================
/** Frees the index and vertex buffers. */
//=============================================================================
void W3DWaypointBuffer::freeWaypointBuffers()
{
}


void W3DWaypointBuffer::setDefaultLineStyle()
{
	if( m_texture )
	{
		m_line->Set_Texture( m_texture );
	}
	ShaderClass lineShader=ShaderClass::_PresetAdditiveShader;
	lineShader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
	m_line->Set_Shader( lineShader );	//pick the alpha blending mode you want - see shader.h for others.
	m_line->Set_Width( 1.5f );
	m_line->Set_Color( Vector3( 0.25f, 0.5f, 1.0f ) );
	m_line->Set_Texture_Mapping_Mode( SegLineRendererClass::TILED_TEXTURE_MAP );	//this tiles the texture across the line
}


//=============================================================================
// W3DWaypointBuffer::drawWaypoints
//=============================================================================
/** Draws the waypoints. Uses camera to cull */
//=============================================================================
void W3DWaypointBuffer::drawWaypoints(RenderInfoClass &rinfo)
{

  if ( ! TheInGameUI )
    return;


  setDefaultLineStyle();



	if( TheInGameUI->isInWaypointMode() )
	{
		//Create a default light environment with no lights and only full ambient.
		//@todo: Fix later by copying default scene light environment from W3DScene.cpp.
		LightEnvironmentClass lightEnv;
		lightEnv.Reset(Vector3(0,0,0), Vector3(1.0f,1.0f,1.0f));
		lightEnv.Pre_Render_Update(rinfo.Camera.Get_Transform());
		RenderInfoClass localRinfo(rinfo.Camera);
		localRinfo.light_environment=&lightEnv;
		Vector3 points[ MAX_DISPLAY_NODES + 1 ]; //Lines have nodes + 1 points.

		const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
		Drawable *draw;
		for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
		{
			draw = *it;
			Object *obj = draw->getObject();
			Int numPoints = 1;
			if( obj && ! obj->isKindOf( KINDOF_IGNORED_IN_GUI ))//so mobs and stuff sont make a gazillion lines
			{
				AIUpdateInterface *ai = obj->getAI();
				Int goalSize = ai ? ai->friend_getWaypointGoalPathSize() : 0;
				Int gpIdx = ai ? ai->friend_getCurrentGoalPathIndex() : 0;
				if( ai && gpIdx >= 0 && gpIdx < goalSize )
				{
					const Coord3D *pos = obj->getPosition();
					points[ 0 ].Set( Vector3( pos->x, pos->y, pos->z ) );

					for( int i = gpIdx; i < goalSize; i++ )
					{
						const Coord3D *waypoint = ai->friend_getGoalPathPosition( i );
						if( waypoint )
						{
							//Render line from previous point to current node.

							if( numPoints < MAX_DISPLAY_NODES + 1 )
							{
								points[ numPoints ].Set( Vector3( waypoint->x, waypoint->y, waypoint->z ) );
								numPoints++;
							}

							m_waypointNodeRobj->Set_Position(Vector3(waypoint->x,waypoint->y,waypoint->z));
							WW3D::Render(*m_waypointNodeRobj,localRinfo);
						}
					}
					//Now render the lines in one pass!
					m_line->Set_Points( numPoints, points );
					m_line->Render( localRinfo );
				}
			}
		}
	}
	else // maybe we want to draw rally points, then?
	{
		//Create a default light environment with no lights and only full ambient.
		//@todo: Fix later by copying default scene light environment from W3DScene.cpp.
		LightEnvironmentClass lightEnv;
		lightEnv.Reset(Vector3(0,0,0), Vector3(1.0f,1.0f,1.0f));
		lightEnv.Pre_Render_Update(rinfo.Camera.Get_Transform());
		RenderInfoClass localRinfo(rinfo.Camera);
		localRinfo.light_environment=&lightEnv;
		Vector3 points[ MAX_DISPLAY_NODES + 1 ]; //Lines have nodes + 1 points.

		const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
		Drawable *draw;
		for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
		{
			draw = *it;
			Object *obj = draw->getObject();

			Int numPoints = 0;
			if( obj )
			{
				if ( obj->getControllingPlayer() != rts::getObservedOrLocalPlayer())
					continue;



        // WAIT! before we go browsing the drawable list for buildings that want to draw their rally points
        // lets test for that very special case of having a listeningoutpost selected, and some enemy drawable moused-over
        if ( obj->isKindOf( KINDOF_REVEALS_ENEMY_PATHS ) )
        {

          DrawableID enemyID = TheInGameUI->getMousedOverDrawableID();
          Drawable *enemyDraw = TheGameClient->findDrawableByID( enemyID );
          if ( enemyDraw )
          {
            Object *enemy = enemyDraw->getObject();
            if ( enemy )
            {
              if ( enemy->getRelationship( obj ) == ENEMIES )
              {

                Coord3D delta = *obj->getPosition();
                delta.sub( *enemy->getPosition() );
                if ( delta.length() <= obj->getVisionRange() ) // is listening outpost close enough to do this?
                {


                  //////////////////////////////////////////////////////////////////////
                  AIUpdateInterface *ai = enemy->getAI();
				          Int goalSize = ai ? ai->friend_getWaypointGoalPathSize() : 0;
				          Int gpIdx = ai ? ai->friend_getCurrentGoalPathIndex() : 0;
                  if( ai )
                  {
                    Bool lineExists = FALSE;

					          const Coord3D *pos = enemy->getPosition();
					          points[ numPoints++ ].Set( Vector3( pos->x, pos->y, pos->z ) );

                    if ( gpIdx >= 0 && gpIdx < goalSize )// Ooh, the enemy is in waypoint mode
				            {

					            for( int i = gpIdx; i < goalSize; i++ )
					            {
						            const Coord3D *waypoint = ai->friend_getGoalPathPosition( i );
						            if( waypoint )
						            {
							            //Render line from previous point to current node.

							            if( numPoints < MAX_DISPLAY_NODES + 1 )
							            {
								            points[ numPoints++ ].Set( Vector3( waypoint->x, waypoint->y, waypoint->z ) );
							            }

							            m_waypointNodeRobj->Set_Position(Vector3(waypoint->x,waypoint->y,waypoint->z));
							            WW3D::Render(*m_waypointNodeRobj,localRinfo);
                          lineExists = TRUE;
						            }
					            }
                    }
                    else // then enemy may be moving to a goal position
                    {
                      const Coord3D *destinationPoint = ai->getGoalPosition();
                      if ( destinationPoint->length() > 1.0f )
                      {
								        points[ numPoints++ ].Set( Vector3( destinationPoint->x, destinationPoint->y, destinationPoint->z ) );
							          m_waypointNodeRobj->Set_Position(Vector3(destinationPoint->x,destinationPoint->y,destinationPoint->z));
							          WW3D::Render(*m_waypointNodeRobj,localRinfo);
                        lineExists = TRUE;
                      }
                    }

                    if ( lineExists )
                    {
					            //Now render the lines in one pass!

                      m_line->Set_Color( Vector3( 0.95f, 0.5f, 0.0f ) );
                      m_line->Set_Width( 3.0f );

					            m_line->Set_Points( numPoints, points );
					            m_line->Render( localRinfo );
                    }
                  }
                  //////////////////////////////////////////////////////////////////////




                }
              }
            }
          }

          break;// dont even bother with the rest, since this one listening outpost satisfies the single path-line limit
        }




				ExitInterface *exitInterface = obj->getObjectExitInterface();
				if( exitInterface )
				{

					Coord3D exitPoint;
					if ( ! exitInterface->getExitPosition(exitPoint))
						exitPoint = *obj->getPosition();

					points[ numPoints ].Set( Vector3( exitPoint.x, exitPoint.y, exitPoint.z ) );
					numPoints++;

					Bool boxWrap = TRUE;
					Coord3D naturalRallyPoint;
					if (exitInterface->getNaturalRallyPoint(naturalRallyPoint, FALSE))//FALSE means "without the extra offset"
					{
						if( !naturalRallyPoint.equals( exitPoint ) )
						{
							points[ numPoints ].Set( Vector3( naturalRallyPoint.x, naturalRallyPoint.y, naturalRallyPoint.z ) );
							numPoints++;
						}
						else
						{
							//Helipad rally point -- so don't use box wrapping.
							boxWrap = FALSE;
						}
					}
					else
						continue; //next drawable

					const Coord3D *rallyPoint = exitInterface->getRallyPoint();
					if( rallyPoint )
					{
						if( boxWrap )
						{

							//test to se whether the rally point flanks the side of the natural rally point
							//to do this we find the two corners of the geometry extents that are nearest the natural rally point
							// these define the natural rally point edge
							// an intermediate point should be inserted at the corner nearest the rally point
							const GeometryInfo& geom = obj->getGeometryInfo();
							const Coord3D *ctr = obj->getPosition();
							Coord3D NRPDelta;
							NRPDelta.x = naturalRallyPoint.x - exitPoint.x;
							NRPDelta.y = naturalRallyPoint.y - exitPoint.y;
							NRPDelta.z = 0.0f;


							//This is a quick idiot test to se whether the rally line needs to wrap the box at all
							Coord3D wayOutPoint = NRPDelta;
							wayOutPoint.normalize();
							wayOutPoint.scale( 99999.9f );
							Real wayOutLength = wayOutPoint.length();
							wayOutPoint.add(naturalRallyPoint);


							//if the rallypoint is closer to the wayoutpoint than it is to the natural rally point then we definitely do not wrap
							Coord3D rallyToWayOutDelta = wayOutPoint;
							rallyToWayOutDelta.sub(*rallyPoint);
							if ( (100.0f + rallyToWayOutDelta.length()) > wayOutLength)
							{

								//if we passed the above idiot test, now lets be sure by testing the dotproduct of the rp against the wayoutpoint
								wayOutPoint.normalize();// a normal shooting straight out the door
								//next comes the delta between the NRP and the RP
								Coord3D NRPToRPDelta = naturalRallyPoint;
								NRPToRPDelta.sub(*rallyPoint);
								NRPToRPDelta.normalize();
								Real dot = NRPToRPDelta.x * wayOutPoint.x + NRPToRPDelta.y * wayOutPoint.y;
								if (dot > 0)
								{

									Real angle = obj->getOrientation();
									Real c = (Real)cos(angle);
									Real s = (Real)sin(angle);

									Coord3D NRPToCtrDelta;
									NRPToCtrDelta.x = naturalRallyPoint.x - ctr->x;
									NRPToCtrDelta.y = naturalRallyPoint.y - ctr->y;
									NRPToCtrDelta.x = 0.0f;

									Real exc = geom.getMajorRadius() * c;
									Real eyc = geom.getMinorRadius() * c;
									Real exs = geom.getMajorRadius() * s;
									Real eys = geom.getMinorRadius() * s;

									Coord2D corners[ 4 ];

									corners[0].x = ctr->x - exc - eys;
									corners[0].y = ctr->y + eyc - exs;
									corners[1].x = ctr->x + exc - eys;
									corners[1].y = ctr->y + eyc + exs;
									corners[2].x = ctr->x + exc + eys;
									corners[2].y = ctr->y - eyc + exs;
									corners[3].x = ctr->x - exc + eys;
									corners[3].y = ctr->y - eyc - exs;

									Coord2D *pNearElbow = nullptr;//find the closest corner to the rallyPoint same end as door
									Coord2D *pFarElbow = nullptr; //find the closest corner to the rallypoint away from door
									Coord2D *nearCandidate = nullptr;
									Coord3D cornerToRPDelta, cornerToExitDelta;
									cornerToRPDelta.z = 0.0f;
									cornerToExitDelta.z = 0.0f;
									Real elbowDistanceNear = 99999.9f;
									Real elbowDistanceFar = 99999.9f;

									for (UnsignedInt cornerIndex = 0; cornerIndex < 4; ++ cornerIndex)
									{
										nearCandidate = &corners[cornerIndex];//for quicker array access
										cornerToExitDelta.x = exitPoint.x - nearCandidate->x;
										cornerToExitDelta.y = exitPoint.y - nearCandidate->y;
										cornerToExitDelta.normalize();

										dot = cornerToExitDelta.x * wayOutPoint.x + cornerToExitDelta.y * wayOutPoint.y;
										if ( dot < 0.0f )
										{
											cornerToRPDelta.x = rallyPoint->x - nearCandidate->x;
											cornerToRPDelta.y = rallyPoint->y - nearCandidate->y;
											if (cornerToRPDelta.length() < elbowDistanceNear)
											{
												elbowDistanceNear = cornerToRPDelta.length();
												pNearElbow = nearCandidate;
											}
										}
										else
										{
											cornerToRPDelta.x = rallyPoint->x - nearCandidate->x;
											cornerToRPDelta.y = rallyPoint->y - nearCandidate->y;
											if (cornerToRPDelta.length() < elbowDistanceFar)
											{
												elbowDistanceFar = cornerToRPDelta.length();
												pFarElbow = nearCandidate;
											}
										}



									}

									if (pNearElbow)//did we find a nearest corner?
									{
										m_waypointNodeRobj->Set_Position(Vector3(pNearElbow->x,pNearElbow->y,ctr->z));
										WW3D::Render(*m_waypointNodeRobj,localRinfo); //The little hockey puck
										points[ numPoints ].Set( Vector3( pNearElbow->x, pNearElbow->y, ctr->z ) );
										numPoints++;


										//and for that matter did we find a far side corner?
										if (pFarElbow)//did we find a nearest corner?
										{
											// but let's test the dot of the first elbow against the rally point to find out
											//whethet the rally point wraps around this one, too
											Coord3D firstElbowDelta;
											firstElbowDelta.x = naturalRallyPoint.x - pNearElbow->x;
											firstElbowDelta.y = naturalRallyPoint.y - pNearElbow->y;
											firstElbowDelta.z = 0.0f;
											firstElbowDelta.normalize();

											Coord3D firstToRPDelta;
											firstToRPDelta.x = pNearElbow->x - rallyPoint->x;
											firstToRPDelta.y = pNearElbow->y - rallyPoint->y;
											firstToRPDelta.z = 0.0f;
											firstToRPDelta.normalize();

											dot = firstToRPDelta.x * firstElbowDelta.x + firstToRPDelta.y * firstElbowDelta.y;
											if (dot < 0)// we have a second elbow
											{
												m_waypointNodeRobj->Set_Position(Vector3(pFarElbow->x,pFarElbow->y,ctr->z));
												WW3D::Render(*m_waypointNodeRobj,localRinfo); //The little hockey puck
												points[ numPoints ].Set( Vector3( pFarElbow->x, pFarElbow->y, ctr->z ) );
												numPoints++;
											}
										}

									}

								}

							}
						}

						// Finally draw the line out to the RallyPoint
						points[ numPoints ].Set( Vector3( rallyPoint->x, rallyPoint->y, rallyPoint->z ) );
						numPoints++;

					}
					else
						continue;

					m_waypointNodeRobj->Set_Position(Vector3(naturalRallyPoint.x,naturalRallyPoint.y,naturalRallyPoint.z));
					WW3D::Render(*m_waypointNodeRobj,localRinfo); //The little hockey puck


					m_line->Set_Points( numPoints, points );
					m_line->Render( localRinfo );

				}

			}
		}

	}
}


