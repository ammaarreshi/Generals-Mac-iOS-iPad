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

// FILE: W3DControlBar.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day
// Desc: Control bar callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/Radar.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameClient/GameWindow.h"
#include "W3DDevice/GameClient/W3DGameWindow.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Display.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/ControlBarScheme.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/GameLogic.h"

//-------------------------------------------------------------------------------------------------
void W3DCameoMovieDraw( GameWindow *window, WinInstanceData *instData )
{
	// draw video buffer
	VideoBuffer *video = TheInGameUI->cameoVideoBuffer();
	if ( video )
	{
		ICoord2D pos, size;
		window->winGetScreenPosition( &pos.x, &pos.y );
		window->winGetSize( &size.x, &size.y );

		TheDisplay->drawVideoBuffer( video, pos.x, pos.y, pos.x + size.x, pos.y + size.y );
	}
}


//-------------------------------------------------------------------------------------------------
void W3DLeftHUDDraw( GameWindow *window, WinInstanceData *instData )
{
	// draw the default stuff
	// draw video buffer
	VideoBuffer *video = TheInGameUI->videoBuffer();
	if ( video )
	{
		ICoord2D pos, size;
		window->winGetScreenPosition( &pos.x, &pos.y );
		window->winGetSize( &size.x, &size.y );

		TheDisplay->drawVideoBuffer( video, pos.x, pos.y, pos.x + size.x, pos.y + size.y );
	}
	else if( rts::localPlayerHasRadar() )
	{
		ICoord2D pos, size;
		//W3DGameWinDefaultDraw( window, instData );
		// window position and size on the display
		window->winGetScreenPosition( &pos.x, &pos.y );
		window->winGetSize( &size.x, &size.y );
		//TheDisplay->drawOpenRect(pos.x, pos.y, size.x,size.y, 1,GameMakeColor(100,100,255,255));
		// draw the radar on the screen now
		TheRadar->draw( pos.x + 1, pos.y + 1, size.x - 2, size.y - 2 );

	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DRightHUDDraw( GameWindow *window, WinInstanceData *instData )
{

	// draw the default stuff
	if( BitIsSet(window->winGetStatus(), WIN_STATUS_IMAGE ))
		W3DGameWinDefaultDraw( window, instData );

}

Real logN(Real value, Real logBase)
{
	return (Real)log10(value)/ log10(logBase);
}

//-------------------------------------------------------------------------------------------------
/** Temporary power meter drawing ... there are some problems with this meter, that is
	* what to do when the player doesn't have power, builds a building before a power plant
	* etc, but I don't want to spend time on this since it's not a task right now and I'm
	* sure it will be in the future.  This is just down and dirty to see SOMETHING  (CD) */
//-------------------------------------------------------------------------------------------------
void W3DPowerDraw( GameWindow *window, WinInstanceData *instData )
{
	//static const Image *endBarYellow = TheMappedImageCollection->findImageByName("PowerBarYellowEndR");
	//static const Image *beginBarYellow = TheMappedImageCollection->findImageByName("PowerBarYellowEndL");
	static const Image *centerBarYellow = TheMappedImageCollection->findImageByName("PowerPointY");
	//static const Image *endBarRed = TheMappedImageCollection->findImageByName("PowerBarRedEndR");
	//static const Image *beginBarRed = TheMappedImageCollection->findImageByName("PowerBarRedEndL");
	static const Image *centerBarRed = TheMappedImageCollection->findImageByName("PowerPointR");
	//static const Image *endBarGreen = TheMappedImageCollection->findImageByName("PowerBarGreenEndR");
	//static const Image *beginBarGreen = TheMappedImageCollection->findImageByName("PowerBarGreenEndL");
	static const Image *centerBarGreen = TheMappedImageCollection->findImageByName("PowerPointG");
	//const Image *endBar = nullptr;
	//const Image *beginBar = nullptr;
	const Image *centerBar = nullptr;
	static const Image *slider = TheMappedImageCollection->findImageByName("PowerBarSlider");
	Player* player = TheControlBar->getCurrentlyViewedPlayer();

	if(!player || !TheGlobalData)
		return;
	Energy *energy = player->getEnergy();
	if( energy == nullptr )
		return;

	Int consumption = energy->getConsumption();
	Int production = energy->getProduction();

	ICoord2D pos, size;
	window->winGetScreenPosition( &pos.x, &pos.y );
	window->winGetSize( &size.x, &size.y );

	static Real pixelsPerInterval = size.x / TheGlobalData->m_powerBarIntervals;
	Int delta = TheGlobalData->m_powerBarYellowRange;

	if((consumption > energy->getProduction() - delta) && (consumption <= energy->getProduction()))
	{
		// 6 and 1 is Green, 6 and 2 is yellow, 6 and 6 is yellow
		//endBar = endBarYellow;
		//beginBar = beginBarYellow;
		centerBar = centerBarYellow;
	}
	else if( consumption > production)
	{
	//	endBar = endBarRed;
//		beginBar = beginBarRed;
		centerBar = centerBarRed;
	}
	else
	{
	//	endBar = endBarGreen;
//		beginBar = beginBarGreen;
		centerBar = centerBarGreen;
	}
	//slider = TheMappedImageCollection->findImageByName("PowerBarSlider");
	if( !slider || !centerBar)
		return;

	Int range;
	range = logN(production, TheGlobalData->m_powerBarBase) * (size.x / TheGlobalData->m_powerBarIntervals);
	if(range >= size.x)
		range = size.x;
//	if(range < endBar->getImageWidth() + beginBar->getImageWidth())
//		range = endBar->getImageWidth() + beginBar->getImageWidth();



//	// get image sizes for the ends
//	ICoord2D leftSize, rightSize, start, end;
//	leftSize.x = beginBar->getImageWidth();
//	leftSize.y = beginBar->getImageHeight();
//	rightSize.x = endBar->getImageWidth();
//	rightSize.y = endBar->getImageHeight();
//
//	// get two key points used in the end drawing
//	ICoord2D leftEnd, rightStart;
//	leftEnd.x = pos.x + leftSize.x;
//	leftEnd.y = pos.y + size.y;
//	rightStart.x = pos.x + range - rightSize.x;
//	rightStart.y = pos.y;

	// draw the center repeating bar
	Int centerWidth, pieces;

	// get width we have to draw our repeating center in
	centerWidth = range; //rightStart.x - leftEnd.x;

	if( centerWidth > 0)
	{

		// how many whole repeating pieces will fit in that width
		pieces = centerWidth / centerBar->getImageWidth();
		ICoord2D start, end;
		// draw the pieces
		start.x = pos.x;
		start.y = pos.y;
		end.y = start.y + size.y;// + yOffset;
		for( Int i = 0; i < pieces; i++ )
		{

			end.x = start.x + centerBar->getImageWidth();
			TheWindowManager->winDrawImage( centerBar,
																			start.x, start.y,
																			end.x, end.y );
			start.x += centerBar->getImageWidth();

		}

		// we will draw the image but clip the parts we don't want to show
		IRegion2D reg;
		reg.lo.x = start.x;
		reg.lo.y = start.y;
		reg.hi.x = pos.x + size.x;//rightStart.x;
		reg.hi.y = pos.y + size.y;
		centerWidth = pos.x +size.x  - start.x;
		if( centerWidth > 0)
		{
			TheDisplay->setClipRegion(&reg);
			end.x = start.x + centerBar->getImageWidth();
			TheWindowManager->winDrawImage( centerBar,
																			start.x, start.y,
																			end.x, end.y );
			TheDisplay->enableClipping(FALSE);
		}

//		// draw left end
//		start.x = pos.x;
//		start.y = pos.y;
//		end = leftEnd;
//		TheWindowManager->winDrawImage(beginBar, start.x, start.y, end.x, end.y);
//
//		// draw right end
//		start = rightStart;
//		end.x = start.x + rightSize.x;
//		end.y = start.y + size.y;
//		TheWindowManager->winDrawImage(endBar, start.x, start.y, end.x, end.y);
	}
	Int posXstart;
	Int posXend;
	Real consumptionForNeedle = (consumption == 1) ? 1.5f : INT_TO_REAL(consumption);//Log(1) == 0, but we need to show something for 1 power used.
	range = logN(consumptionForNeedle, TheGlobalData->m_powerBarBase) * (size.x / TheGlobalData->m_powerBarIntervals);
	if(centerWidth <= 0 && range <= 0)
		return;
	if(range >= size.x)
	{
		posXstart = pos.x + size.x - slider->getImageWidth();
		posXend = pos.x + size.x;
	}
	else
	{
		posXstart = pos.x + range - slider->getImageWidth()/2;
		posXend = pos.x + range + slider->getImageWidth()/2;
	}
	if(posXstart <=pos.x)
	{
		posXstart	 = pos.x;
		posXend	= pos.x + slider->getImageWidth();
	}
	TheWindowManager->winDrawImage(slider, posXstart, pos.y + size.y - slider->getImageHeight(), posXend, pos.y + size.y);
}

void W3DPowerDrawA( GameWindow *window, WinInstanceData *instData )
{
	static const Image *endBarYellow = TheMappedImageCollection->findImageByName("PowerBarYellowEndR");
	static const Image *beginBarYellow = TheMappedImageCollection->findImageByName("PowerBarYellowEndL");
	static const Image *centerBarYellow = TheMappedImageCollection->findImageByName("PowerBarYellow");
	static const Image *endBarRed = TheMappedImageCollection->findImageByName("PowerBarRedEndR");
	static const Image *beginBarRed = TheMappedImageCollection->findImageByName("PowerBarRedEndL");
	static const Image *centerBarRed = TheMappedImageCollection->findImageByName("PowerBarRed");
	static const Image *endBarGreen = TheMappedImageCollection->findImageByName("PowerBarGreenEndR");
	static const Image *beginBarGreen = TheMappedImageCollection->findImageByName("PowerBarGreenEndL");
	static const Image *centerBarGreen = TheMappedImageCollection->findImageByName("PowerBarGreen");
	const Image *endBar = nullptr;
	const Image *beginBar = nullptr;
	const Image *centerBar = nullptr;
	static const Image *slider = TheMappedImageCollection->findImageByName("PowerBarSlider");
	Player* player = TheControlBar->getCurrentlyViewedPlayer();

	if(!player || !TheGlobalData)
		return;
	Energy *energy = player->getEnergy();
	if( energy == nullptr )
		return;

	Int consumption = energy->getConsumption();
	Int production = energy->getProduction();

	ICoord2D pos, size;
	window->winGetScreenPosition( &pos.x, &pos.y );
	window->winGetSize( &size.x, &size.y );

	static Real pixelsPerInterval = size.x / TheGlobalData->m_powerBarIntervals;
	Int delta = TheGlobalData->m_powerBarYellowRange;

	if((consumption > energy->getProduction() - delta) && (consumption <= energy->getProduction()))
	{
		// 6 and 1 is Green, 6 and 2 is yellow, 6 and 6 is yellow
		endBar = endBarYellow;
		beginBar = beginBarYellow;
		centerBar = centerBarYellow;
	}
	else if( consumption > production)
	{
		endBar = endBarRed;
		beginBar = beginBarRed;
		centerBar = centerBarRed;
	}
	else
	{
		endBar = endBarGreen;
		beginBar = beginBarGreen;
		centerBar = centerBarGreen;
	}
	//slider = TheMappedImageCollection->findImageByName("PowerBarSlider");
	if( !slider || !endBar || !beginBar || !centerBar)
		return;

	Int range;
	range = logN(production, TheGlobalData->m_powerBarBase) * (size.x / TheGlobalData->m_powerBarIntervals);
	if(range >= size.x)
		range = size.x;
	if(range < endBar->getImageWidth() + beginBar->getImageWidth())
		range = endBar->getImageWidth() + beginBar->getImageWidth();



	// get image sizes for the ends
	ICoord2D leftSize, rightSize, start, end;
	leftSize.x = beginBar->getImageWidth();
	leftSize.y = beginBar->getImageHeight();
	rightSize.x = endBar->getImageWidth();
	rightSize.y = endBar->getImageHeight();

	// get two key points used in the end drawing
	ICoord2D leftEnd, rightStart;
	leftEnd.x = pos.x + leftSize.x;
	leftEnd.y = pos.y + size.y;
	rightStart.x = pos.x + range - rightSize.x;
	rightStart.y = pos.y;

	// draw the center repeating bar
	Int centerWidth, pieces;

	// get width we have to draw our repeating center in
	centerWidth = rightStart.x - leftEnd.x;

	if( centerWidth <= 0)
	{
		// draw left end
		start.x = pos.x;
		start.y = pos.y;
		end.y = leftEnd.y;
		end.x = pos.x + range/2;
		TheWindowManager->winDrawImage(beginBar, start.x, start.y, end.x, end.y);

		// draw right end
		start.y = rightStart.y;
		start.x = end.x;
		end.x = pos.x + range;
		end.y = start.y + size.y;
		TheWindowManager->winDrawImage(endBar, start.x, start.y, end.x, end.y);
	}
	else
	{

		// how many whole repeating pieces will fit in that width
		pieces = centerWidth / centerBar->getImageWidth();

		// draw the pieces
		start.x = leftEnd.x;
		start.y = pos.y;
		end.y = start.y + size.y; //centerImage->getImageHeight() + yOffset;
		for( Int i = 0; i < pieces; i++ )
		{

			end.x = start.x + centerBar->getImageWidth();
			TheWindowManager->winDrawImage( centerBar,
																			start.x, start.y,
																			end.x, end.y );
			start.x += centerBar->getImageWidth();

		}

		// we will draw the image but clip the parts we don't want to show
		IRegion2D reg;
		reg.lo.x = start.x;
		reg.lo.y = start.y;
		reg.hi.x = rightStart.x;
		reg.hi.y = end.y;
		centerWidth = rightStart.x - start.x;
		if( centerWidth > 0)
		{
			TheDisplay->setClipRegion(&reg);
			end.x = start.x + centerBar->getImageWidth();
			TheWindowManager->winDrawImage( centerBar,
																			start.x, start.y,
																			end.x, end.y );
			TheDisplay->enableClipping(FALSE);
		}

		// draw left end
		start.x = pos.x;
		start.y = pos.y;
		end = leftEnd;
		TheWindowManager->winDrawImage(beginBar, start.x, start.y, end.x, end.y);

		// draw right end
		start = rightStart;
		end.x = start.x + rightSize.x;
		end.y = start.y + size.y;
		TheWindowManager->winDrawImage(endBar, start.x, start.y, end.x, end.y);
	}
	Int posXstart;
	Int posXend;
	Real consumptionForNeedle = (consumption == 1) ? 1.5f : INT_TO_REAL(consumption);//Log(1) == 0, but we need to show something for 1 power used.
	range = logN(consumptionForNeedle, TheGlobalData->m_powerBarBase) * (size.x / TheGlobalData->m_powerBarIntervals);
	if(range >= size.x)
	{
		posXstart = pos.x + size.x - slider->getImageWidth();
		posXend = pos.x + size.x;
	}
	else
	{
		posXstart = pos.x + range - slider->getImageWidth()/2;
		posXend = pos.x + range + slider->getImageWidth()/2;
	}
	if(posXstart <=pos.x)
	{
		posXstart	 = pos.x;
		posXend	= pos.x + slider->getImageWidth();
	}
	TheWindowManager->winDrawImage(slider, posXstart, pos.y + size.y - slider->getImageHeight(), posXend, pos.y + size.y);
}

void W3DCommandBarGridDraw( GameWindow *window, WinInstanceData *instData )
{
	if( BitIsSet(window->winGetStatus(), WIN_STATUS_IMAGE ))
	{
		W3DGameWinDefaultDraw( window, instData );
		return;
	}

	ICoord2D pos, size;
	window->winGetScreenPosition( &pos.x, &pos.y );
	window->winGetSize( &size.x, &size.y );

	Color color = TheControlBar->getBorderColor();
	window->winSetEnabledBorderColor(0, color);
	W3DGameWinDefaultDraw( window, instData );

	TheDisplay->drawLine(pos.x, pos.y + size.y * .33, pos.x + size.x, pos.y+ size.y*.33, 1,color);
	TheDisplay->drawLine(pos.x, pos.y + size.y * .66, pos.x + size.x, pos.y+ size.y*.66, 1,color);
	TheDisplay->drawLine(pos.x + size.x * .33, pos.y, pos.x + size.x *.33, pos.y +size.y, 1,color);
	TheDisplay->drawLine(pos.x + size.x  * .66, pos.y, pos.x + size.x *.66, pos.y + size.y, 1,color);




}

void W3DCommandBarGenExpDraw( GameWindow *window, WinInstanceData *instData )
{
	// TheSuperHackers @bugfix Stubbjax 08/08/2025 Show the experience bar for observers
	Player* player = TheControlBar->getCurrentlyViewedPlayer();

	if (!player)
		return;

	static const Image *endBar = TheMappedImageCollection->findImageByName("GenExpBarTop1");
	static const Image *beginBar = TheMappedImageCollection->findImageByName("GenExpBarBottom1");
	static const Image *centerBar = TheMappedImageCollection->findImageByName("GenExpBar1");
	Int progress = 0;
	Int skillPointsRequired = player->getSkillPointsLevelUp() - player->getSkillPointsLevelDown();

	// TheSuperHackers @bugfix Mauller 04/05/2025 Prevent possible division by zero
	if ( skillPointsRequired > 0)
	{
		progress = ( ((player->getSkillPoints() - player->getSkillPointsLevelDown()) * 100) / skillPointsRequired );
	}

	if(progress <= 0)
		return;

	// GS This should never be necessary, but scripts can change the points required or even disable a level.
	// A disabled level will be -1 for points required.  Just be totally safe and bind to 100, and we will
	// fix the scripts to bind the points gained later.
	if( progress > 100 )
		progress = 100;

	ICoord2D pos, size;
	window->winGetScreenPosition( &pos.x, &pos.y );
	window->winGetSize( &size.x, &size.y );



	if( !endBar || !beginBar || !centerBar)
		return;

	Int range;
	range = size.y * progress / 100;


	// get image sizes for the ends
	ICoord2D topSize, bottomSize, start, end;
	bottomSize.x = beginBar->getImageWidth();
	bottomSize.y = beginBar->getImageHeight();
	topSize.x = endBar->getImageWidth();
	topSize.y = endBar->getImageHeight();

	// get two key points used in the end drawing
	ICoord2D bottomEnd, topStart;
	bottomEnd.x = pos.x + size.x;
	bottomEnd.y = pos.y + size.y - bottomSize.y;
	topStart.x = pos.x;
	topStart.y = pos.y +size.y - range - topSize.y;

	// draw the center repeating bar
	Int centerWidth, pieces;

	// get width we have to draw our repeating center in
	centerWidth = bottomEnd.y - topStart.y;

	if( centerWidth <= 0)
	{
		// draw left end
		start.x = pos.x;
		start.y = pos.y + size.y - bottomSize.y;
		end.y = pos.y + size.y;
		end.x = pos.x + size.x;
		TheWindowManager->winDrawImage(beginBar, start.x, start.y, end.x, end.y);

		// draw right end
		start.y = pos.y + size.y - bottomSize.y - topSize.y;
		start.x = pos.x;
		end.x = pos.x + size.x;
		end.y = start.y + topSize.y;
		TheWindowManager->winDrawImage(endBar, start.x, start.y, end.x, end.y);
	}
	else
	{

		// how many whole repeating pieces will fit in that width
		pieces = centerWidth / centerBar->getImageHeight();

		// draw the pieces
		start.x = pos.x;
		start.y = topStart.y;
		end.x = start.x + size.x; //centerImage->getImageHeight() + yOffset;
		for( Int i = 0; i < pieces; i++ )
		{

			end.y = start.y + centerBar->getImageHeight();
			TheWindowManager->winDrawImage( centerBar,
																			start.x, start.y,
																			end.x, end.y );
			start.y += centerBar->getImageHeight();

		}

		// we will draw the image but clip the parts we don't want to show
		IRegion2D reg;
		reg.lo.x = start.x;
		reg.lo.y = start.y;
		reg.hi.x = bottomEnd.x;
		reg.hi.y = bottomEnd.y;
		centerWidth = bottomEnd.y - start.y;
		if( centerWidth > 0)
		{
			TheDisplay->setClipRegion(&reg);
			end.y = start.y + centerBar->getImageHeight();
			TheWindowManager->winDrawImage( centerBar,
																			start.x, start.y,
																			end.x, end.y );
			TheDisplay->enableClipping(FALSE);
		}

		// draw left end
		end.x = pos.x + size.x;
		end.y = pos.y + size.y;
		start.x = pos.x;
		start.y = bottomEnd.y;
		TheWindowManager->winDrawImage(beginBar, start.x, start.y, end.x, end.y);

		// draw right end
		start.x = pos.x;
		start.y = pos.y +size.y - range;
		end.x = pos.x + size.x;
		end.y = pos.y +size.y - range - topSize.y;
		TheWindowManager->winDrawImage(endBar, start.x, start.y, end.x, end.y);
	}

}

void W3DCommandBarTopDraw( GameWindow *window, WinInstanceData *instData )
{
	GameWindow *win = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("ControlBar.wnd:ButtonGeneral"));
	if(!win || win->winIsHidden() || !ThePlayerList->getLocalPlayer()->isPlayerActive())
		return;

//	TheControlBar->drawTransitionHandler();

//	ICoord2D pos, size, imageStart, imageEnd;
//	win->winGetScreenPosition(&pos.x, &pos.y);
//	win->winGetSize(&size.x, &size.y);
////	const Image *image =TheControlBar->getStarImage();
//	imageStart.x = pos.x	+ size.x/2 - image->getImageWidth()/2;
//	imageStart.y = pos.y	+ size.y/2 - image->getImageHeight()/2;
//	imageEnd.x = imageStart.x + image->getImageWidth();
//	imageEnd.y = imageStart.y + image->getImageHeight();
//	TheDisplay->drawImage(TheControlBar->getStarImage(), imageStart.x, imageStart.y, imageEnd.x, imageEnd.y);
}


void W3DCommandBarBackgroundDraw( GameWindow *window, WinInstanceData *instData )
{

	ControlBarSchemeManager *man = TheControlBar->getControlBarSchemeManager();
	if(!man)
		return;
	static NameKeyType winNamekey	= TheNameKeyGenerator->nameToKey( "ControlBar.wnd:BackgroundMarker" );
	GameWindow *win =  TheWindowManager->winGetWindowFromId(nullptr,winNamekey);
	static ICoord2D basePos;
	if(!win)
	{
		return;
		//win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey( "ControlBar.wnd:BackgroundMarker" ));
	}
	TheControlBar->getBackgroundMarkerPos(&basePos.x, &basePos.y);
	ICoord2D pos, offset;
	win->winGetScreenPosition(&pos.x,&pos.y);
	offset.x = pos.x - basePos.x;
	offset.y = pos.y - basePos.y;

	man->drawBackground(offset);
}


void W3DCommandBarForegroundDraw( GameWindow *window, WinInstanceData *instData )
{

	ControlBarSchemeManager *man = TheControlBar->getControlBarSchemeManager();
	if(!man)
		return;

	static NameKeyType winNamekey	= TheNameKeyGenerator->nameToKey( "ControlBar.wnd:BackgroundMarker" );
	GameWindow *win = TheWindowManager->winGetWindowFromId(nullptr,winNamekey);
	static ICoord2D basePos;
	if(!win)
	{
		return;
		//win = TheWindowManager->winGetWindowFromId(nullptr,TheNameKeyGenerator->nameToKey( "ControlBar.wnd:BackgroundMarker" ));
	}
	TheControlBar->getForegroundMarkerPos(&basePos.x, &basePos.y);
	ICoord2D pos, offset;
	win->winGetScreenPosition(&pos.x,&pos.y);
	offset.x = pos.x - basePos.x;
	offset.y = pos.y - basePos.y;

	man->drawForeground(offset);

}

void W3DNoDraw( GameWindow *window, WinInstanceData *instData )
{

	// draw the default stuff
//	W3DGameWinDefaultDraw( window, instData );

}

void drawSkinnyBorder( Int x, Int y, Int width, Int height);
void W3DDrawMapPreview( GameWindow *window, WinInstanceData *instData)
{
	MapMetaData *mmData = (MapMetaData *)window->winGetUserData();
	Int pixelX, pixelY, width, height;
	// if the local player does not have a radar then we can't draw anything
	window->winGetScreenPosition(&pixelX, &pixelY);
	window->winGetSize(&width, &height);
	if(!mmData)
	{
		W3DGameWinDefaultDraw( window, instData );
		drawSkinnyBorder(pixelX - 1, pixelY - 1, width + 2, height + 2);
		return;
	}




	//
	// given a upper left corner at pixelX|Y and a width and height to draw into, figure out
	// where we should start and end the image so that the final drawn image has the
	// same ratio as the map and isn't stretched or distorted
	//
	ICoord2D ul, lr;
	findDrawPositions( pixelX, pixelY, width, height, mmData->m_extent, &ul, &lr );

//	Int scaledWidth = lr.x - ul.x;
//	Int scaledHeight = lr.y - ul.y;

	// draw black border areas where we need map
	Color fillColor = GameMakeColor( 0, 0, 0, 255 );
	Color lineColor = GameMakeColor( 50, 50, 50, 255 );

	if( mmData->m_extent.width()/width >= mmData->m_extent.height()/height )
	{

		// draw horizontal bars at top and bottom
		TheDisplay->drawFillRect( pixelX, pixelY, width, ul.y - pixelY - 1, fillColor );
		TheDisplay->drawFillRect( pixelX, lr.y + 1, width, pixelY + height - lr.y - 1, fillColor);
		TheDisplay->drawLine(pixelX, ul.y, pixelX + width, ul.y, 1, lineColor);
		TheDisplay->drawLine(pixelX, lr.y + 1, pixelX + width, lr.y + 1, 1, lineColor);

	}
	else
	{

		// draw vertical bars to the left and right
		TheDisplay->drawFillRect( pixelX, pixelY, ul.x - pixelX - 1, height, fillColor );
		TheDisplay->drawFillRect( lr.x + 1, pixelY, width - (lr.x - pixelX) - 1, height, fillColor );
		TheDisplay->drawLine(ul.x, pixelY, ul.x, pixelY + height, 1, lineColor);
		TheDisplay->drawLine(lr.x + 1, pixelY, lr.x + 1, pixelY + height, 1, lineColor);

	}

	if(!BitIsSet(window->winGetStatus(), WIN_STATUS_IMAGE) || !window->winGetEnabledImage(0))
		TheDisplay->drawFillRect(ul.x, ul.y, lr.x -ul.x, lr.y-ul.y, lineColor);
	else
		TheDisplay->drawImage(window->winGetEnabledImage(0) , ul.x, ul.y, lr.x, lr.y );

	const Image *image = TheMappedImageCollection->findImageByName("TecBuilding");
	ICoord2DList::iterator it = TheSupplyAndTechImageLocations.m_techPosList.begin();
	if(image)
		while(it != TheSupplyAndTechImageLocations.m_techPosList.end())
		{
			TheDisplay->drawImage(image, pixelX + it->x, pixelY + it->y, pixelX + it->x + SUPPLY_TECH_SIZE, pixelY + it->y +SUPPLY_TECH_SIZE);
			it++;
		}

	image = TheMappedImageCollection->findImageByName("Cash");
	it = TheSupplyAndTechImageLocations.m_supplyPosList.begin();
	if(image)
		while(it != TheSupplyAndTechImageLocations.m_supplyPosList.end())
		{
			TheDisplay->drawImage(image, pixelX + it->x, pixelY + it->y, pixelX + it->x + SUPPLY_TECH_SIZE, pixelY + it->y +SUPPLY_TECH_SIZE);
			it++;
		}
	drawSkinnyBorder(pixelX - 1, pixelY - 1, width + 2, height + 2);
}

void drawSkinnyBorder( Int x, Int y, Int width, Int height)
{

	enum
	{
		BORDER_CORNER_SIZE	= 5,
		BORDER_LINE_SIZE		= 5,
	};
	Int Offset = 2;
	Int OffsetLower = 5;

	// save original x, y
	Int originalX = x;
	Int originalY = y;
	Int maxX = x + width;
	Int maxY = y + height;
	Int x2, y2;			// used for simultaneous drawing of line pairs
	Int size = 5;
	Int halfSize = size / 2;
	const Image *image1, *image2;
	// Draw Horizontal Lines
	// All border pieces are based on a 10 pixel offset from the centerline
	y = originalY - Offset;
	y2 = maxY - OffsetLower;
	x2 = maxX - (OffsetLower + BORDER_LINE_SIZE);
	image1 = TheMappedImageCollection->findImageByName("FrameT");
	image2 = TheMappedImageCollection->findImageByName("FrameB");
	for( x=(originalX + 3); x <= x2; x += BORDER_LINE_SIZE )
	{

		TheDisplay->drawImage( image1,
													 x, y, x + size, y + size );
		TheDisplay->drawImage( image2,
													 x, y2, x + size, y2 + size );

	}

	x2 = maxX - 5;//BORDER_CORNER_SIZE;

	// x == place to draw remainder if any
	if( (x2 - x) >= (BORDER_LINE_SIZE / 2) )
	{

		//Blit Half piece
		TheDisplay->drawImage( image1,
													 x, y, x + halfSize, y + size );
		TheDisplay->drawImage( image2,
													 x, y2, x + halfSize, y2 + size );

		x += (BORDER_LINE_SIZE / 2);

	}

	// x2 - x ... must now be less than a half piece
	// check for equals and if not blit an adjusted half piece border pieces have
	// a two pixel repeat so we will blit one pixel over if necessary to line up
	// the art, but we'll cover-up the overlap with the corners
	if( x < x2 )
	{
		x -= ((BORDER_LINE_SIZE / 2) - (((x2 - x) + 1) & ~1));

		//Blit Half piece
		TheDisplay->drawImage(image1,
													 x, y, x + halfSize, y + size );
		TheDisplay->drawImage( image2,
													 x, y2, x + halfSize, y2 + size );

	}

	// Draw Vertical Lines
	// All border pieces are based on a 10 pixel offset from the centerline
	image1 = TheMappedImageCollection->findImageByName("FrameL");
	image2 = TheMappedImageCollection->findImageByName("FrameR");

	x = originalX - Offset;
	x2 = maxX - OffsetLower;
	y2 = maxY - (OffsetLower + BORDER_LINE_SIZE);

	for( y=(originalY + 3); y <= y2; y += BORDER_LINE_SIZE )
	{

		TheDisplay->drawImage( image1,
													 x, y, x + size, y + size );
		TheDisplay->drawImage( image2,
													 x2, y, x2 + size, y + size );

	}

	y2 = maxY - OffsetLower;//BORDER_CORNER_SIZE;

	// y == place to draw remainder if any
	if( (y2 - y) >= (BORDER_LINE_SIZE / 2) )
	{

		//Blit Half piece
		TheDisplay->drawImage( image1,
													 x, y, x + size, y + halfSize );
		TheDisplay->drawImage( image2,
													 x2, y, x2 + size, y + halfSize );

		y += (BORDER_LINE_SIZE / 2);
	}

	// y2 - y ... must now be less than a half piece
	// check for equals and if not blit an adjusted half piece border pieces have
	// a two pixel repeat so we will blit one pixel over if necessary to line up
	// the art, but we'll cover-up the overlap with the corners
	if( y < y2 )
	{
		y -= ((BORDER_LINE_SIZE / 2) - (((y2 - y) + 1) & ~1));

		//Blit Half piece
		TheDisplay->drawImage( image1,
													 x, y, x + size, y + halfSize );
		TheDisplay->drawImage( image2,
													 x2, y, x2 + size, y + halfSize );

	}

	// Draw Corners
	x = originalX - 2;//BORDER_CORNER_SIZE ;
	y = originalY - 2;//BORDER_CORNER_SIZE;
	image1 = TheMappedImageCollection->findImageByName("FrameCornerUL");
	TheDisplay->drawImage( image1,
												 x, y, x + size, y + size );
	x = maxX - 5;//BORDER_CORNER_SIZE;
	y = originalY - 2;//BORDER_CORNER_SIZE;
	image1 = TheMappedImageCollection->findImageByName("FrameCornerUR");
	TheDisplay->drawImage(image1,
												 x, y, x + size, y + size );
	x = originalX - 2;//BORDER_CORNER_SIZE;
	y = maxY - 5;//BORDER_CORNER_SIZE;
	image1 = TheMappedImageCollection->findImageByName("FrameCornerLL");
	TheDisplay->drawImage( image1,
												 x, y, x + size, y + size );
	x = maxX - 5;//BORDER_CORNER_SIZE;
	y = maxY - 5;//BORDER_CORNER_SIZE;
	image1 = TheMappedImageCollection->findImageByName("FrameCornerLR");
	TheDisplay->drawImage(image1,
												 x, y, x + size, y + size );


}


void W3DCommandBarHelpPopupDraw( GameWindow *window, WinInstanceData *instData )
{

	static const Image *endBar = TheMappedImageCollection->findImageByName("Helpbox-top");
	static const Image *beginBar = TheMappedImageCollection->findImageByName("Helpbox-bottom");
	static const Image *centerBar = TheMappedImageCollection->findImageByName("Helpbox-middle");

	ICoord2D pos, size;
	window->winGetScreenPosition( &pos.x, &pos.y );
	window->winGetSize( &size.x, &size.y );



	if( !endBar || !beginBar || !centerBar)
		return;


//	Int range;
//	range = size.y;


	// get image sizes for the ends
	ICoord2D topSize, bottomSize, start, end;
	bottomSize.x = beginBar->getImageWidth();
	bottomSize.y = beginBar->getImageHeight();
	topSize.x = endBar->getImageWidth();
	topSize.y = endBar->getImageHeight();

	// get two key points used in the end drawing
	ICoord2D bottomEnd, topStart;
	bottomEnd.x = pos.x + size.x;
	bottomEnd.y = pos.y + size.y - bottomSize.y;
	topStart.x = pos.x;
	topStart.y = pos.y +size.y - topSize.y;

	// draw the center repeating bar
	Int centerWidth, pieces;

	// get width we have to draw our repeating center in
	centerWidth = size.y - topSize.y - bottomSize.y;

	if( centerWidth <= 0)
	{
		// draw left end
		start.x = pos.x;
		start.y = pos.y + size.y - bottomSize.y;
		end.y = pos.y + size.y;
		end.x = pos.x + size.x;
		TheWindowManager->winDrawImage(beginBar, start.x, start.y, end.x, end.y);

		// draw right end
		start.y = pos.y + size.y - bottomSize.y - topSize.y;
		start.x = pos.x;
		end.x = pos.x + size.x;
		end.y = start.y + topSize.y;
		TheWindowManager->winDrawImage(endBar, start.x, start.y, end.x, end.y);
	}
	else
	{

		// how many whole repeating pieces will fit in that width
		pieces = centerWidth / centerBar->getImageHeight();

		// draw the pieces
		start.x = pos.x;
		start.y = pos.y + topSize.y ;
		end.x = start.x + size.x; //centerImage->getImageHeight() + yOffset;
		for( Int i = 0; i < pieces; i++ )
		{

			end.y = start.y + centerBar->getImageHeight();
			TheWindowManager->winDrawImage( centerBar,
																			start.x, start.y,
																			end.x, end.y );
			start.y += centerBar->getImageHeight();

		}

		// we will draw the image but clip the parts we don't want to show
		IRegion2D reg;
		reg.lo.x = start.x;
		reg.lo.y = start.y;
		reg.hi.x = pos.x + size.x;
		reg.hi.y = pos.y + size.y - bottomSize.y;
		centerWidth = pos.y + size.y - bottomSize.y - start.y;
		if( centerWidth > 0)
		{
			TheDisplay->setClipRegion(&reg);
			end.y = start.y + centerBar->getImageHeight();
			TheWindowManager->winDrawImage( centerBar,
																			start.x, start.y,
																			end.x, end.y );
			TheDisplay->enableClipping(FALSE);
		}

		// draw left end
		end.x = pos.x + size.x;
		end.y = pos.y + size.y;
		start.x = pos.x;
		start.y = pos.y + size.y - bottomSize.y;
		TheWindowManager->winDrawImage(beginBar, start.x, start.y, end.x, end.y);

		// draw right end
		start.x = pos.x;
		start.y = pos.y ;
		end.x = pos.x + size.x;
		end.y = pos.y + topSize.y;
		TheWindowManager->winDrawImage(endBar, start.x, start.y, end.x, end.y);
	}


}
