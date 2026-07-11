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

// FILE: W3DLaserDraw.cpp /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, May 2001
// Desc:   W3DLaserDraw
// Updated: Kris Morness July 2002 -- made it data driven and added new features to make it flexible.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>

#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Color.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/RayEffect.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/LaserUpdate.h"
#include "W3DDevice/GameClient/Module/W3DLaserDraw.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/segline.h"
#include "WWMath/vector3.h"
#include "WW3D2/assetmgr.h"



// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DLaserDrawModuleData::W3DLaserDrawModuleData()
{
	m_innerBeamWidth = 0.0f;         //The total width of beam
	m_outerBeamWidth = 1.0f;         //The total width of beam
  m_numBeams = 1;                 //Number of overlapping cylinders that make the beam. 1 beam will just use inner data.
  m_maxIntensityFrames = 0;				//Laser stays at max intensity for specified time in ms.
  m_fadeFrames = 0;               //Laser will fade and delete.
	m_scrollRate = 0.0f;
	m_tile = false;
	m_segments = 1;
	m_arcHeight = 0.0f;
	m_segmentOverlapRatio = 0.0f;
	m_tilingScalar = 1.0f;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DLaserDrawModuleData::~W3DLaserDrawModuleData()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "NumBeams",							INI::parseUnsignedInt,					nullptr, offsetof( W3DLaserDrawModuleData, m_numBeams ) },
		{ "InnerBeamWidth",				INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_innerBeamWidth ) },
		{ "OuterBeamWidth",				INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_outerBeamWidth ) },
		{ "InnerColor",						INI::parseColorInt,							nullptr, offsetof( W3DLaserDrawModuleData, m_innerColor ) },
		{ "OuterColor",						INI::parseColorInt,							nullptr, offsetof( W3DLaserDrawModuleData, m_outerColor ) },
		{ "MaxIntensityLifetime",	INI::parseDurationUnsignedInt,	nullptr, offsetof( W3DLaserDrawModuleData, m_maxIntensityFrames ) },
		{ "FadeLifetime",					INI::parseDurationUnsignedInt,	nullptr, offsetof( W3DLaserDrawModuleData, m_fadeFrames ) },
		{ "Texture",							INI::parseAsciiString,					nullptr, offsetof( W3DLaserDrawModuleData, m_textureName ) },
		{ "ScrollRate",						INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_scrollRate ) },
		{ "Tile",									INI::parseBool,									nullptr, offsetof( W3DLaserDrawModuleData, m_tile ) },
		{ "Segments",							INI::parseUnsignedInt,					nullptr, offsetof( W3DLaserDrawModuleData, m_segments ) },
    { "ArcHeight",						INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_arcHeight ) },
		{ "SegmentOverlapRatio",	INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_segmentOverlapRatio ) },
		{ "TilingScalar",					INI::parseReal,									nullptr, offsetof( W3DLaserDrawModuleData, m_tilingScalar ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DLaserDraw::W3DLaserDraw( Thing *thing, const ModuleData* moduleData ) :
	DrawModule( thing, moduleData ),
	m_line3D(nullptr),
	m_texture(nullptr),
	m_textureAspectRatio(1.0f),
	m_selfDirty(TRUE)
{
	Vector3 dummyPos1( 0.0f, 0.0f, 0.0f );
	Vector3 dummyPos2( 1.0f, 1.0f, 1.0f );
	Int i;

	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();

	m_texture = WW3DAssetManager::Get_Instance()->Get_Texture( data->m_textureName.str() );
	if (m_texture)
	{
		if (!m_texture->Is_Initialized())
			m_texture->Init();	//make sure texture is actually loaded before accessing surface.

		SurfaceClass::SurfaceDescription surfaceDesc;
		m_texture->Get_Level_Description(surfaceDesc);
		m_textureAspectRatio = (Real)surfaceDesc.Width/(Real)surfaceDesc.Height;
	}

	//Get the color components for calculation purposes.
	Real innerRed, innerGreen, innerBlue, innerAlpha, outerRed, outerGreen, outerBlue, outerAlpha;
	GameGetColorComponentsReal( data->m_innerColor, &innerRed, &innerGreen, &innerBlue, &innerAlpha );
	GameGetColorComponentsReal( data->m_outerColor, &outerRed, &outerGreen, &outerBlue, &outerAlpha );

	//Make sure our beams range between 1 and the maximum cap.
#ifdef I_WANT_TO_BE_FIRED
// srj sez: this data is const for a reason. casting away the constness because we don't like the values
// isn't an acceptable solution. if you need to constrain the values, do so at parsing time, when
// it's still legal to modify these values. (In point of fact, there's not even really any reason to limit
// the numBeams or segments anymore.)
	data->m_numBeams =		 __min( __max( 1, data->m_numBeams ), MAX_LASER_LINES );
	data->m_segments =		 __min( __max( 1, data->m_segments ), MAX_SEGMENTS );
	data->m_tilingScalar = __max( 0.01f, data->m_tilingScalar );
#endif

	//Allocate an array of lines equal to the number of beams * segments
	m_line3D = NEW SegmentedLineClass *[ data->m_numBeams * data->m_segments ];

	for( UnsignedInt segment = 0; segment < data->m_segments; segment++ )
	{
		//We don't care about segment positioning yet until we actually set the position

		// create all the lines we need at the right transparency level
		for( i = data->m_numBeams - 1; i >= 0; i-- )
		{
			int index = segment * data->m_numBeams + i;

			Real red, green, blue, alpha, width;

			if( data->m_numBeams == 1 )
			{
				width = data->m_innerBeamWidth;
				alpha = innerAlpha;
				red = innerRed * innerAlpha;
				green = innerGreen * innerAlpha;
				blue = innerBlue * innerAlpha;
			}
			else
			{
				//Calculate the scale between min and max values
				//0 means use min value, 1 means use max value
				//0.2 means min value + 20% of the diff between min and max
				Real scale = i / ( data->m_numBeams - 1.0f);

				width		= data->m_innerBeamWidth	+ scale * (data->m_outerBeamWidth - data->m_innerBeamWidth);
				alpha		= innerAlpha							+ scale * (outerAlpha - innerAlpha);
				red			= innerRed								+ scale * (outerRed - innerRed) * innerAlpha;
				green		= innerGreen							+ scale * (outerGreen - innerGreen) * innerAlpha;
				blue		= innerBlue								+ scale * (outerBlue - innerBlue) * innerAlpha;
			}

			m_line3D[ index ] = NEW SegmentedLineClass;

			SegmentedLineClass *line = m_line3D[ index ];
			if( line )
			{
				line->Set_Texture( m_texture );
				line->Set_Shader( ShaderClass::_PresetAdditiveShader );	//pick the alpha blending mode you want - see shader.h for others.
				line->Set_Width( width );
				line->Set_Color( Vector3( red, green, blue ) );
				line->Set_UV_Offset_Rate( Vector2(0.0f, data->m_scrollRate) );	//amount to scroll texture on each draw
				if( m_texture )
				{
					line->Set_Texture_Mapping_Mode(SegLineRendererClass::TILED_TEXTURE_MAP);	//this tiles the texture across the line
				}

				// add to scene
				if (W3DDisplay::m_3DScene != nullptr)
					W3DDisplay::m_3DScene->Add_Render_Object( line );	//add it to our scene so it gets rendered with other objects.

				// hide the render object until the first time we come to draw it and
				// set the correct position
				line->Set_Visible( 0 );
			}


		}

	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
W3DLaserDraw::~W3DLaserDraw()
{
	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();

	for( UnsignedInt i = 0; i < data->m_numBeams * data->m_segments; i++ )
	{

		// remove line from scene
		if (W3DDisplay::m_3DScene != nullptr)
			W3DDisplay::m_3DScene->Remove_Render_Object( m_line3D[ i ] );

		// delete line
		REF_PTR_RELEASE( m_line3D[ i ] );

	}

	delete [] m_line3D;
	// TheSuperHackers @fix Mauller 11/03/2025 Free reference counted material
	REF_PTR_RELEASE(m_texture);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Real W3DLaserDraw::getLaserTemplateWidth() const
{
	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();
	return data->m_outerBeamWidth * 0.5f;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DLaserDraw::doDrawModule(const Matrix3D* transformMtx)
{
	//UnsignedInt currentFrame = TheGameClient->getFrame();
	const W3DLaserDrawModuleData *data = getW3DLaserDrawModuleData();

	//Get the updatemodule that drives it...
	Drawable *draw = getDrawable();
	static NameKeyType key_LaserUpdate = NAMEKEY( "LaserUpdate" );
	LaserUpdate *update = (LaserUpdate*)draw->findClientUpdateModule( key_LaserUpdate );
	if( !update )
	{
		DEBUG_CRASH( ("W3DLaserDraw::doDrawModule() expects its owner drawable %s to have a ClientUpdate = LaserUpdate module.", draw->getTemplate()->getName().str() ));
		return;
	}

	//If the update has moved the laser, it requires a reset of the laser.
	if (update->isDirty() || m_selfDirty)
	{
		update->setDirty(false);
		m_selfDirty = false;

		Vector3 laserPoints[ 2 ];

		for( UnsignedInt segment = 0; segment < data->m_segments; segment++ )
		{
			if( data->m_arcHeight > 0.0f && data->m_segments > 1 )
			{
				//CALCULATE A CURVED LINE BASED ON TOTAL LENGTH AND DESIRED HEIGHT INCREASE
				//To do this we will use a portion of the cos wave ranging between -0.25PI
				//and +0.25PI. 0PI is 1.0 and 0.25PI is 0.70 -- resulting in a somewhat
				//gentle curve depending on the line height and length. We also have to make
				//the line *level* for this phase of the calculations.

				//Get the desired direct line
				Coord3D lineStart, lineEnd, lineVector;
				lineStart.set( *update->getStartPos() );
				lineEnd.set( *update->getEndPos() );
				//This is critical -- in the case we have sloped lines (at the end, we'll fix it)
//				lineEnd.z = lineStart.z;

				//Get the length of the line
				lineVector.set( lineEnd );
				lineVector.sub( lineStart );
				Real lineLength = lineVector.length();

				//Get the middle point (we'll use this to determine how far we are from
				//that to calculate our height -- middle point is the highest).
				Coord3D lineMiddle;
				lineMiddle.set( lineStart );
				lineMiddle.add( lineEnd );
				lineMiddle.scale( 0.5 );

				//The half length is used to scale with the distance from middle to
				//get our cos( 0 to 0.25 PI) cos value
				Real halfLength = lineLength * 0.5f;

				//Now calculate which segment we will use.
				Real startSegmentRatio = segment / ((Real)data->m_segments);
				Real endSegmentRatio = (segment + 1.0f) / ((Real)data->m_segments);

				//Offset the segment ever-so-slightly to minimize overlap -- only apply
				//to segments that are not the start/end point
				if( segment > 0 )
				{
					startSegmentRatio -= data->m_segmentOverlapRatio;
				}
				if( segment < data->m_segments - 1 )
				{
					endSegmentRatio += data->m_segmentOverlapRatio;
				}

				//Calculate our start segment position on the *ground*.
				Coord3D segmentStart, segmentEnd, vector;
				vector.set( lineVector );
				vector.scale( startSegmentRatio );
				segmentStart.set( lineStart );
				segmentStart.add( vector );

				//Calculate our end segment position on the *ground*.
				vector.set( lineVector );
				vector.scale( endSegmentRatio );
				segmentEnd.set( lineStart );
				segmentEnd.add( vector );

				//--------------------------------------------------------------------------------
				//Now at this point, we have our segment line in the level positions that we want.
				//Calculate the raised height for the start/end segment positions using cosine.
				//--------------------------------------------------------------------------------

				//Calculate the distance from midpoint for the start positions.
				vector.set( lineMiddle );
				vector.sub( segmentStart );
				Real dist = vector.length();
				Real scaledRadians = dist / halfLength * PI * 0.5f;
				Real height = cos( scaledRadians );
				height *= data->m_arcHeight;
				segmentStart.z += height;

				//Now do the same thing for the end position.
				vector.set( lineMiddle );
				vector.sub( segmentEnd );
				dist = vector.length();
				scaledRadians = dist / halfLength * PI * 0.5f;
				height = cos( scaledRadians );
				height *= data->m_arcHeight;
				segmentEnd.z += height;

				//This makes the laser skim the ground rather than penetrate it!
				laserPoints[ 0 ].Set( segmentStart.x, segmentStart.y,
					MAX( segmentStart.z, 2.0f + TheTerrainLogic->getGroundHeight(segmentStart.x, segmentStart.y) ) );
				laserPoints[ 1 ].Set( segmentEnd.x, segmentEnd.y,
					MAX( segmentEnd.z, 2.0f + TheTerrainLogic->getGroundHeight(segmentEnd.x, segmentEnd.y) ) );

			}
			else
			{
				//No arc -- way simpler!
				laserPoints[ 0 ].Set( update->getStartPos()->x, update->getStartPos()->y, update->getStartPos()->z );
				laserPoints[ 1 ].Set( update->getEndPos()->x, update->getEndPos()->y, update->getEndPos()->z );
			}

			//Get the color components for calculation purposes.
			Real innerRed, innerGreen, innerBlue, innerAlpha, outerRed, outerGreen, outerBlue, outerAlpha;
			GameGetColorComponentsReal( data->m_innerColor, &innerRed, &innerGreen, &innerBlue, &innerAlpha );
			GameGetColorComponentsReal( data->m_outerColor, &outerRed, &outerGreen, &outerBlue, &outerAlpha );

			for( Int i = data->m_numBeams - 1; i >= 0; i-- )
			{

				Real alpha, width;
				int index = segment * data->m_numBeams + i;

				if( data->m_numBeams == 1 )
				{
					width = data->m_innerBeamWidth * update->getWidthScale();
					alpha = innerAlpha;
				}
				else
				{
					//Calculate the scale between min and max values
					//0 means use min value, 1 means use max value
					//0.2 means min value + 20% of the diff between min and max
					Real scale = i / ( data->m_numBeams - 1.0f);
					Real ultimateScale = update->getWidthScale();
					width		= (data->m_innerBeamWidth	+ scale * (data->m_outerBeamWidth - data->m_innerBeamWidth));
					width *= ultimateScale;
					alpha		= innerAlpha							+ scale * (outerAlpha - innerAlpha);
				}


				//Calculate the number of times to tile the line based on the height of the texture used.
				if( m_texture && data->m_tile )
				{
					//Calculate the length of the line.
					Vector3 lineVector;
					Vector3::Subtract( laserPoints[1], laserPoints[0], &lineVector );
					Real length = lineVector.Length();

					//Adjust tile factor so texture is NOT stretched but tiled equally in both width and length.
					Real tileFactor = length/width*m_textureAspectRatio*data->m_tilingScalar;

					//Set the tile factor
					m_line3D[ index ]->Set_Texture_Tile_Factor( tileFactor );	//number of times to tile texture across each segment
				}

				m_line3D[ index ]->Set_Width( width );
				m_line3D[ index ]->Set_Points( 2, &laserPoints[0] );
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void W3DLaserDraw::crc( Xfer *xfer )
{

	// extend base class
	DrawModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void W3DLaserDraw::xfer( Xfer *xfer )
{

	// version
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DrawModule::xfer( xfer );

	// Kris says there is no data to save for these, go ask him.
	// m_selfDirty is not saved, is runtime only

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void W3DLaserDraw::loadPostProcess()
{

	// extend base class
	DrawModule::loadPostProcess();

	m_selfDirty = true;	// so we update the first time after reload

}
