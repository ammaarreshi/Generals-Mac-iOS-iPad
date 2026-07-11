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

/***************************************************************************
 ***    C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S     ***
 ***************************************************************************
 *                                                                         *
 *                 Project Name : G                                        *
 *                                                                         *
 *                     $Archive:: /VSS_Sync/ww3d2/pointgr.cpp             $*
 *                                                                         *
 *                    Org Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/26/02 4:04p                                             $*
 *                                                                                             *
 *                    $Revision:: 38                                      $*
 *                                                                         *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 *   PointGroupClass::PointGroupClass -- PointGroupClass CTor.             *
 *   PointGroupClass::~PointGroupClass -- PointGroupClass DTor.            *
 *   PointGroupClass::operator = -- PointGroupClass assignment operator.   *
 *   PointGroupClass::Set_Arrays -- Set point location/color/enable arrays.*
 *   PointGroupClass::Set_Point_Size -- Set default point size.            *
 *   PointGroupClass::Get_Point_Size -- Get default point size.            *
 *   PointGroupClass::Set_Point_Color -- Set default point color.          *
 *   PointGroupClass::Get_Point_Color -- Get default point color.          *
 *   PointGroupClass::Set_Point_Alpha -- Set default point alpha.          *
 *   PointGroupClass::Get_Point_Alpha -- Get default point alpha.          *
 *   PointGroupClass::Set_Point_Orientation -- Set default point orientatio*
 *   PointGroupClass::Get_Point_Orientation -- Get default point orientatio*
 *   PointGroupClass::Set_Point_Frame -- Set default point frame.          *
 *   PointGroupClass::Get_Point_Frame -- Get default point frame.          *
 *   PointGroupClass::Set_Point_Mode -- Set point rendering method.        *
 *   PointGroupClass::Get_Point_Mode -- Get point rendering method.        *
 *   PointGroupClass::Set_Flag -- Set given flag to on or off.             *
 *   PointGroupClass::Get_Flag -- Get current value (on/off) of given flag.*
 *   PointGroupClass::Set_Texture -- Set texture used.                     *
 *   PointGroupClass::Get_Texture -- Get texture used.                     *
 *   PointGroupClass::Set_Shader -- Set shader used.                       *
 *   PointGroupClass::Get_Shader -- Get shader used.                       *
 *   PointGroupClass::Set_Billboard -- Set whether to billboard.           *
 *   PointGroupClass::Get_Billboard -- Get whether to billboard.           *
 *   PointGroupClass::Get_Discrete_Orientation_Count_Log2 -- what it says  *
 *   PointGroupClass::Set_Discrete_Orientation_Count_Log2 -- what it says. *
 *   PointGroupClass::Get_Frame_Row_Column_Count_Log2 -- what it says      *
 *   PointGroupClass::Set_Frame_Row_Column_Count_Log2 -- what it says.     *
 *   PointGroupClass::Get_Polygon_Count -- Get estimated polygon count.    *
 *   PointGroupClass::Render -- draw point group.                          *
 *   PointGroupClass::vInstance -- Create instance of class.               *
 *   PointGroupClass::sGetClassName -- Get name of class.                  *
 *   PointGroupClass::Update_Arrays -- Update all arrays used in rendering *
 *   PointGroupClass::Peek_Texture -- Peeks texture                        *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include "pointgr.h"
#include "vertmaterial.h"
#include "ww3d.h"
#include "aabox.h"
#include "statistics.h"
#include "simplevec.h"
#include "texture.h"
#include "Vector.h"
#include "vp.h"
#include "matrix4.h"
#include "dx8wrapper.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "rinfo.h"
#include "camera.h"
#include "dx8fvf.h"
#include "d3dx8math.h"
#include "sortingrenderer.h"

// Upgraded to DX8 2/2/01 HY

// static data members
Vector3 PointGroupClass::_TriVertexLocationOrientationTable[256][3];
Vector3 PointGroupClass::_QuadVertexLocationOrientationTable[256][4];
Vector2 *PointGroupClass::_TriVertexUVFrameTable[5] = { nullptr, nullptr, nullptr, nullptr, nullptr};
Vector2 *PointGroupClass::_QuadVertexUVFrameTable[5] = { nullptr, nullptr, nullptr, nullptr, nullptr};
VertexMaterialClass *PointGroupClass::PointMaterial=nullptr;

// Static arrays for intermediate calcs (never resized down, just up):
VectorClass<Vector3>		PointGroupClass::compressed_loc;		// point locations 'compressed' by APT
VectorClass<Vector4>		PointGroupClass::compressed_diffuse;	// point colors 'compressed' by APT
VectorClass<float>			PointGroupClass::compressed_size;		// point sizes 'compressed' by APT
VectorClass<unsigned char>	PointGroupClass::compressed_orient;	// point orientations 'compressed' by APT
VectorClass<unsigned char>	PointGroupClass::compressed_frame;		// point frames 'compressed' by APT
VectorClass<Vector3>		PointGroupClass::transformed_loc;		// transformed point locations

// This array has vertex locations for screenspace mode - calculated to cover exactly 1x1 and 2x2 pixels.
Vector3 PointGroupClass::_ScreenspaceVertexLocationSizeTable[2][3] =
{
	Vector3(0.5f, 0.0f, -1.0f),
	Vector3(1.0f, 1.0f, -1.0f),
	Vector3(0.0f, 1.0f, -1.0f),
	Vector3(1.0f, -0.5f, -1.0f),
	Vector3(2.7f, 2.0f, -1.0f),
	Vector3(-0.7f, 2.0f, -1.0f)
};

// useful for particles that aren't aligned with the screen.
static Vector3 GroundMultiplierX(1.0f, 0.0f, 0.0f);
static Vector3 GroundMultiplierY(0.0f, 1.0f, 0.0f);

// Some internal variables
VectorClass<Vector3>			VertexLoc;		// camera-space vertex locations
VectorClass<Vector4>			VertexDiffuse;	// vertex diffuse/alpha colors
VectorClass<Vector2>			VertexUV;		// vertex texture coords

// Some DX 8 variables
#define MAX_VB_SIZE			2048
#define MAX_TRI_POINTS		MAX_VB_SIZE/3
#define MAX_TRI_IB_SIZE		3*MAX_TRI_POINTS
#define MAX_QUAD_POINTS		MAX_VB_SIZE/4
#define MAX_QUAD_IB_SIZE	6*MAX_QUAD_POINTS

DX8IndexBufferClass			*Tris, *Quads;						// Index buffers.
SortingIndexBufferClass		*SortingTris, *SortingQuads;	// Sorting index buffers.

/**************************************************************************
 * PointGroupClass::PointGroupClass -- PointGroupClass CTor.              *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
PointGroupClass::PointGroupClass() :
	PointLoc(nullptr),
	PointDiffuse(nullptr),
	APT(nullptr),
	PointSize(nullptr),
	PointOrientation(nullptr),
	PointFrame(nullptr),
	PointCount(0),
	FrameRowColumnCountLog2(0),
	Texture(nullptr),
	Flags(0),
	Shader(ShaderClass::_PresetAdditiveSpriteShader),
	PointMode(TRIS),
	DefaultPointSize(0.0f),
	DefaultPointColor(1.0f, 1.0f, 1.0f),
	DefaultPointAlpha(1.0f),
	DefaultPointOrientation(0),
	DefaultPointFrame(0),
	VPXMin(0.0f),
	VPYMin(0.0f),
	VPXMax(0.0f),
	VPYMax(0.0f)
{
}

/**************************************************************************
 * PointGroupClass::~PointGroupClass -- PointGroupClass DTor.             *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
PointGroupClass::~PointGroupClass()
{
	if (PointLoc) {
		PointLoc->Release_Ref();
		PointLoc = nullptr;
	}
	if (PointDiffuse) {
		PointDiffuse->Release_Ref();
		PointDiffuse=nullptr;
	}
	if (APT) {
		APT->Release_Ref();
		APT = nullptr;
	}
	if (PointSize) {
		PointSize->Release_Ref();
		PointSize = nullptr;
	}
	if (PointOrientation) {
		PointOrientation->Release_Ref();
		PointOrientation = nullptr;
	}
	if (PointFrame) {
		PointFrame->Release_Ref();
		PointFrame = nullptr;
	}
	if (Texture) {
		REF_PTR_RELEASE(Texture);
		Texture = nullptr;
	}
}

/**************************************************************************
 * PointGroupClass::operator = -- PointGroupClass assignment operator.    *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
PointGroupClass & PointGroupClass::operator = (const PointGroupClass & that)
{
	if (this != &that) {
		WWASSERT(0);	// If you hit this assert implement the function!
	}
	return *this;
}


/**************************************************************************
 * PointGroupClass::Set_Arrays -- Set point location/color/enable arrays. *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * NOTES:	colors, alphas, APT, sizes, orientations and frames are       *
 *          optional. active_point_count can also be used with a nullptr apt.*
 *          In this case active_point_count is ignored if it is -1        *
 *          (default value) and otherwise it indicates the first N active *
 *          points in the arrays.                                         *
 *          The view plane rectangle may optionally be passed as well -   *
 *				this is only used in SCREENSPACE mode.                        *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *   08/25/1999 NH  : Alphas added.                                       *
 *   06/28/2000 NH  : Orientations and frames added.                      *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
void PointGroupClass::Set_Arrays(
	ShareBufferClass<Vector3> *locs,
	ShareBufferClass<Vector4> *diffuse,
	ShareBufferClass<unsigned int> *apt,
	ShareBufferClass<float> *sizes,
	ShareBufferClass<unsigned char> *orientations,
	ShareBufferClass<unsigned char> *frames,
	int active_point_count,
	float vpxmin,
	float vpymin,
	float vpxmax,
	float vpymax)
{
	// The point locations array is NOT optional!
	WWASSERT(locs);

	// Ensure lengths of all arrays are the same:
	WWASSERT(!diffuse || locs->Get_Count() == diffuse->Get_Count());
	WWASSERT(!apt || locs->Get_Count() == apt->Get_Count());
	WWASSERT(!sizes || locs->Get_Count() == sizes->Get_Count());
	WWASSERT(!orientations || locs->Get_Count() == orientations->Get_Count());
	WWASSERT(!frames || locs->Get_Count() == frames->Get_Count());

	REF_PTR_SET(PointLoc,locs);
	REF_PTR_SET(PointDiffuse,diffuse);
	REF_PTR_SET(APT,apt);
	REF_PTR_SET(PointSize,sizes);
	REF_PTR_SET(PointOrientation,orientations);
	REF_PTR_SET(PointFrame,frames);

	if (APT) {
		PointCount = active_point_count;
	} else {
		PointCount = (active_point_count >= 0) ? active_point_count : PointLoc->Get_Count();
	}

	// Store view plane rectangle (only used in SCREENSPACE mode)
	VPXMin = vpxmin;
	VPYMin = vpymin;
	VPXMax = vpxmax;
	VPYMax = vpymax;
}

/**************************************************************************
 * PointGroupClass::Set_Point_Size -- Set default point size.             *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This size is ignored if a size array is present.             *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Set_Point_Size(float size)
{
	DefaultPointSize = size;
}


/**************************************************************************
 * PointGroupClass::Get_Point_Size -- Get default point size.             *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This size is ignored if a size array is present.             *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
float PointGroupClass::Get_Point_Size()
{
	return DefaultPointSize;
}


/**************************************************************************
 * PointGroupClass::Set_Point_Color -- Set default point color.           *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This color is ignored if a color array is present.           *
 *                                                                        *
 * HISTORY:                                                               *
 *   04/20/1999 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Set_Point_Color(Vector3 color)
{
	DefaultPointColor = color;
}

/**************************************************************************
 * PointGroupClass::Get_Point_Color -- Get default point color.           *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This color is ignored if a color array is present.           *
 *                                                                        *
 * HISTORY:                                                               *
 *   04/20/1999 NH  : Created.                                            *
 *========================================================================*/
Vector3 PointGroupClass::Get_Point_Color()
{
	return DefaultPointColor;
}

/**************************************************************************
 * PointGroupClass::Set_Point_Alpha -- Set default point alpha.           *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This alpha is ignored if an alpha array is present.          *
 *                                                                        *
 * HISTORY:                                                               *
 *   08/25/1999 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Set_Point_Alpha(float alpha)
{
	DefaultPointAlpha = alpha;
}


/**************************************************************************
 * PointGroupClass::Get_Point_Alpha -- Get default point alpha.           *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This alpha is ignored if an alpha array is present.          *
 *                                                                        *
 * HISTORY:                                                               *
 *   08/25/1999 NH  : Created.                                            *
 *========================================================================*/
float PointGroupClass::Get_Point_Alpha()
{
	return DefaultPointAlpha;
}


/**************************************************************************
 * PointGroupClass::Set_Point_Orientation -- Set default point orientation*
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This is ignored if an orientation array is present.          *
 *                                                                        *
 * NOTE: No need to ensure value in valid range - it will be masked later.*
 *                                                                        *
 * HISTORY:                                                               *
 *   06/28/2000 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Set_Point_Orientation(unsigned char orientation)
{
	DefaultPointOrientation = orientation;
}


/**************************************************************************
 * PointGroupClass::Get_Point_Orientation -- Get default point orientation*
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This is ignored if an orientation array is present.          *
 *                                                                        *
 * HISTORY:                                                               *
 *   06/28/2000 NH  : Created.                                            *
 *========================================================================*/
unsigned char PointGroupClass::Get_Point_Orientation()
{
	return DefaultPointOrientation;
}


/**************************************************************************
 * PointGroupClass::Set_Point_Frame -- Set default point frame.           *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This frame is ignored if an frame array is present.          *
 *                                                                        *
 * NOTE: No need to ensure value in valid range - it will be masked later.*
 *                                                                        *
 * HISTORY:                                                               *
 *   06/28/2000 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Set_Point_Frame(unsigned char frame)
{
	DefaultPointFrame = frame;
}


/**************************************************************************
 * PointGroupClass::Get_Point_Frame -- Get default point frame.           *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS: This frame is ignored if an frame array is present.          *
 *                                                                        *
 * HISTORY:                                                               *
 *   06/28/2000 NH  : Created.                                            *
 *========================================================================*/
unsigned char PointGroupClass::Get_Point_Frame()
{
	return DefaultPointFrame;
}


/**************************************************************************
 * PointGroupClass::Set_Point_Mode -- Set point rendering method.         *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Set_Point_Mode(PointModeEnum mode)
{
	PointMode = mode;
}


/**************************************************************************
 * PointGroupClass::Get_Point_Mode -- Get point rendering method.         *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
PointGroupClass::PointModeEnum PointGroupClass::Get_Point_Mode()
{
	return PointMode;
}


/**************************************************************************
 * Set_Flag -- PointGroupClass::Set given flag to on or off.              *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Set_Flag(FlagsType flag, bool onoff)
{
	if (onoff) Flags|=1<<flag;
	else
		Flags&=~(1<<flag);
}

/**************************************************************************
 * PointGroupClass::Get_Flag -- Get current value (on/off) of given flag. *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
int PointGroupClass::Get_Flag(FlagsType flag)
{
	return (Flags>>flag) & 0x1;
}

/**************************************************************************
 * PointGroupClass::Set_Texture -- Set texture used.                      *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
void PointGroupClass::Set_Texture(TextureClass* texture)
{
	REF_PTR_SET(Texture,texture);
}

/**************************************************************************
 * PointGroupClass::Get_Texture -- Get texture used.                      *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
TextureClass * PointGroupClass::Get_Texture()
{
	if (Texture) Texture->Add_Ref();
	return Texture;
}


/***********************************************************************************************
 * PointGroupClass::Peek_Texture -- Peeks texture                                              *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/12/2001  hy : Created.                                                                  *
 *=============================================================================================*/
TextureClass * PointGroupClass::Peek_Texture()
{
	return Texture;
}

/**************************************************************************
 * PointGroupClass::Set_Shader -- Set shader used.                        *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:	the primary gradient will be set to MODULATE/DISABLE in    *
 * 				the shader depending on whether a color or alpha array was *
 * 				passed in Set_Point_Arrays. also, texturing will be        *
 * 				enabled or disabled dependent on whether a non-null        *
 * 				texture was set.                                           *
 *					these will override the primary gradient/texturing         *
 *					settings in the given shader.                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
void PointGroupClass::Set_Shader(ShaderClass shader)
{
	Shader = shader;
}


/**************************************************************************
 * PointGroupClass::Get_Shader -- Get shader used.                        *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
ShaderClass PointGroupClass::Get_Shader()
{
	return Shader;
}

/**************************************************************************
 * PointGroupClass::Set_Billboard -- Set whether to Billboard.            *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   04/25/2002 JM  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Set_Billboard(bool shouldBillboard)
{
	Billboard = shouldBillboard;
}

/**************************************************************************
 * PointGroupClass::Get_Billboard -- Get whether to Billboard.            *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   04/25/2002 JM  : Created.                                            *
 *========================================================================*/
bool PointGroupClass::Get_Billboard()
{
	return Billboard;
}

/**************************************************************************
 * PointGroupClass::Get_Frame_Row_Column_Count_Log2 -- what it says       *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   06/28/2000 NH  : Created.                                            *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
unsigned char PointGroupClass::Get_Frame_Row_Column_Count_Log2()
{
	return FrameRowColumnCountLog2;
}


/**************************************************************************
 * PointGroupClass::Set_Frame_Row_Column_Count_Log2 -- what it says.      *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   06/28/2000 NH  : Created.                                            *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
void PointGroupClass::Set_Frame_Row_Column_Count_Log2(unsigned char frccl2)
{
	FrameRowColumnCountLog2 = MIN(frccl2, 4);
}

/**************************************************************************
 * PointGroupClass::Get_Polygon_Count -- Get estimated polygon count.     *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/18/1998 NH  : Created.                                            *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
int PointGroupClass::Get_Polygon_Count()
{
	switch (PointMode) {
		case TRIS:
		case SCREENSPACE:
			return PointCount;
			break;
		case QUADS:
			return PointCount * 2;
			break;
	}
	WWASSERT(0);
	return 0;
}

/**************************************************************************
 * PointGroupClass::Render -- draw point group.                           *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   12/10/1998 NH  : Created.                                            *
 *   02/08/2001 HY  : Upgraded to DX8                                     *
 *========================================================================*/
static SimpleVecClass<unsigned long> remap;
void PointGroupClass::Render(RenderInfoClass &rinfo)
{
	/// @todo lorenzen asks: is particle culling in the shader perhaps faster than in DoParticles? Fix winding and find out...
	// NB: the winding for pointgroups is wrong, but we
	// are disabling culling for particles anyway
	Shader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);

	// If no points, do nothing:
	if (PointCount == 0) return;

	WWASSERT(PointLoc && PointLoc->Get_Array());

	// Process texture reductions:
//	if (Texture) Texture->Process_Reduction();

	// Pointers which point into existing buffers (member or static):
	Vector3 *current_loc = nullptr;
	Vector4 *current_diffuse = nullptr;
	float *current_size = nullptr;
	unsigned char *current_orient = nullptr;
	unsigned char *current_frame = nullptr;

	// If there is a color or alpha array enable gradient in shader - otherwise disable.
   float value_255 = 0.9961f;	//254 / 255
	bool default_white_opaque = (	DefaultPointColor.X > value_255 &&
											DefaultPointColor.Y > value_255 &&
											DefaultPointColor.Z > value_255 &&
											DefaultPointAlpha > value_255);

	// The reason we check for lack of texture here is that SR seems to render black triangles
	// rather than white triangles as would be expected) when there is no texture AND no gradient.
	if (PointDiffuse || !default_white_opaque || !Texture) {
		Shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE);
	} else {
		Shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);
	}

	// If Texture is non-null enable texturing in shader - otherwise disable.
	if (Texture) {
		Shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
	} else {
		Shader.Set_Texturing(ShaderClass::TEXTURING_DISABLE);
	}

	// If there is an active point table, use it to compress the point
	// locations/colors/alphas/sizes/orientations/frames.
	if (APT) {
		// Resize compressed result arrays if needed (2x guardband to prevent
		// frequent reallocations):

		/// @todo lorenzen sez: precompute pointers to indexed array elements, below

		if (compressed_loc.Length() < PointCount) {
			compressed_loc.Resize(PointCount * 2);
		}
		VectorProcessorClass::CopyIndexed(&compressed_loc[0],
			PointLoc->Get_Array(), APT->Get_Array(), PointCount);
		current_loc = &compressed_loc[0];
		if (PointDiffuse) {
			if (compressed_diffuse.Length() < PointCount) {
				compressed_diffuse.Resize(PointCount * 2);
			}
			VectorProcessorClass::CopyIndexed(&compressed_diffuse[0],
				PointDiffuse->Get_Array(), APT->Get_Array(), PointCount);
			current_diffuse = &compressed_diffuse[0];
		}
		if (PointSize) {
			if (compressed_size.Length() < PointCount) {
				compressed_size.Resize(PointCount * 2);
			}
			VectorProcessorClass::CopyIndexed(&compressed_size[0],
				PointSize->Get_Array(), APT->Get_Array(), PointCount);
			current_size = &compressed_size[0];
		}
		if (PointOrientation) {
			if (compressed_orient.Length() < PointCount) {
				compressed_orient.Resize(PointCount * 2);
			}
			VectorProcessorClass::CopyIndexed(&compressed_orient[0],
				PointOrientation->Get_Array(), APT->Get_Array(), PointCount);
			current_orient = &compressed_orient[0];
		}
		if (PointFrame) {
			if (compressed_frame.Length() < PointCount) {
				compressed_frame.Resize(PointCount * 2);
			}
			VectorProcessorClass::CopyIndexed(&compressed_frame[0],
				PointFrame->Get_Array(), APT->Get_Array(), PointCount);
			current_frame = &compressed_frame[0];
		}
	} else {
		current_loc = PointLoc->Get_Array();
		if (PointDiffuse) {
			current_diffuse = PointDiffuse->Get_Array();
		}
		if (PointSize) {
			current_size = PointSize->Get_Array();
		}
		if (PointOrientation) {
			current_orient = PointOrientation->Get_Array();
		}
		if (PointFrame) {
			current_frame = PointFrame->Get_Array();
		}
	}

	// Get the world and view matrices
	Matrix4x4 view;
	DX8Wrapper::Get_Transform(D3DTS_VIEW,view);

	// Transform the point locations from worldspace to camera space if needed
	// (i.e. if they are not already in camera space):

	// need to interrupt this processing. If we are not billboarding, then we need the actual position
	// of the vertice to lay it down flat.

	// (gth) changed this 'if' to use OR rather than AND... The way it was caused all emitters to break
	if (Get_Flag(TRANSFORM) && Billboard) {
		// Resize transformed location array if needed (2x guardband to prevent
		// frequent reallocations):
		if (transformed_loc.Length() < PointCount) {
			transformed_loc.Resize(PointCount * 2);
		}
		// Not using vector processor class because we are discarding w
		// Not using T&L in DX8 because we don't want DX8 to transform
		// 3 times per particle when we can do it once
		for (int i=0; i<PointCount; i++)
		{
			/// @todo lorenzen sez: use pointer arithmetic here and a fast while loop
			Vector4 result=view*current_loc[i];
			transformed_loc[i].X=result.X;
			transformed_loc[i].Y=result.Y;
			transformed_loc[i].Z=result.Z;
		}
		current_loc = &transformed_loc[0];
	}

	// Update the arrays with the offsets.
	int vnum, pnum;

	Update_Arrays(current_loc, current_diffuse, current_size, current_orient, current_frame,
		PointCount, PointLoc->Get_Count(), vnum, pnum);

	// the locations are now in view space
	// so set world and view matrices to identity and render

	Matrix4x4 identity(true);
	DX8Wrapper::Set_Transform(D3DTS_WORLD,identity);
	DX8Wrapper::Set_Transform(D3DTS_VIEW,identity);

	DX8Wrapper::Set_Material(PointMaterial);
	DX8Wrapper::Set_Shader(Shader);
	DX8Wrapper::Set_Texture(0,Texture);

	// Enable sorting if the primitives are translucent and alpha testing is not enabled.
	// TheSuperHackers @bugfix stephanmeesters 30/06/2026 However, do not apply sorting to ground-aligned particles.
	// This improves performance and resolves rendering artifacts caused by clipping between ground-aligned particles and billboard particles.
	const bool sort = Billboard &&
	                  Shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO &&
	                  Shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_DISABLE &&
	                  WW3D::Is_Sorting_Enabled();

	IndexBufferClass *indexbuffer;
	int	verticesperprimitive;/// lorenzen fixed
	int current;
	int delta;

	/// @todo lorenzen sez: if tri-based particles are not supported, elim this test
	if (PointMode == QUADS) {
		verticesperprimitive = 2;
		indexbuffer = sort ? static_cast <IndexBufferClass*> (SortingQuads) : static_cast <IndexBufferClass*> (Quads);
	} else {
		verticesperprimitive = 3;
		indexbuffer = sort ? static_cast <IndexBufferClass*> (SortingTris) : static_cast <IndexBufferClass*> (Tris);
	}

	current = 0;
	while (current<vnum)
	{
		delta=MIN(vnum-current,MAX_VB_SIZE);
		DynamicVBAccessClass PointVerts (sort ? BUFFER_TYPE_DYNAMIC_SORTING : BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, delta);

		// Copy in the data to the VB
		{
			DynamicVBAccessClass::WriteLockClass Lock(&PointVerts);
			int i;
			unsigned char *vb=(unsigned char*)Lock.Get_Formatted_Vertex_Array();
			const FVFInfoClass& fvfinfo=PointVerts.FVF_Info();

			for (i = current; i < current + delta; i++)
			{
				/// @todo lorenzen sez: use pointer arithmetic throughout this block
				/// @todo lorenzen sez: delare thes locals outside this loop
				/// @todo lorenzen sez: use a fast while loop
				// Copy Locations
				*(Vector3*)(vb+fvfinfo.Get_Location_Offset())=VertexLoc[i];
				if (current_diffuse) {
					unsigned color=DX8Wrapper::Convert_Color_Clamp(VertexDiffuse[i]);
					*(unsigned int*)(vb+fvfinfo.Get_Diffuse_Offset())=color;
				}
				else
					*(unsigned int*)(vb+fvfinfo.Get_Diffuse_Offset())=
						DX8Wrapper::Convert_Color_Clamp(Vector4(DefaultPointColor[0],DefaultPointColor[1],DefaultPointColor[2],DefaultPointAlpha));
				*(Vector2*)(vb+fvfinfo.Get_Tex_Offset(0))=VertexUV[i];
				vb+=fvfinfo.Get_FVF_Size();
			}
		}

		DX8Wrapper::Set_Index_Buffer (indexbuffer, 0);
		DX8Wrapper::Set_Vertex_Buffer (PointVerts);

		if ( sort )
		{
				SortingRendererClass::Insert_Triangles (0, delta / verticesperprimitive, 0, delta);
		}
		else
		{
			DX8Wrapper::Draw_Triangles (0, delta / verticesperprimitive, 0, delta);
		}

		current+=delta;
	}

	// restore the matrices
	DX8Wrapper::Set_Transform(D3DTS_VIEW,view);
}







/**************************************************************************
 * PointGroupClass::Update_Arrays -- Update all arrays used in rendering  *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   11/17/1998 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::Update_Arrays(
	Vector3 *point_loc,
	Vector4 *point_diffuse,
	float *point_size,
	unsigned char *point_orientation,
	unsigned char *point_frame,
	int active_points,
	int total_points,
	int &vnum,
	int &pnum)
{
	int verts_per_point = (PointMode == QUADS) ? 4 : 3;
	int polys_per_point = (PointMode == QUADS) ? 2 : 1;

	// total_vnum/pnum reflect the size of the point arrays passed to the
	// point group. These (instead of vnum/pnum, which reflect the number of
	// active points) are used for the resize logic - the idea is that these
	// numbers will vary less often than the active numbers.
	int total_vnum = verts_per_point * total_points;
	vnum = verts_per_point * active_points;
	pnum = polys_per_point * active_points;

	// Resize the arrays if they are too small. We only need to check the length of one array
	// since they always all have the same length.

	/// @todo lorenzen sez: precompute params below

	if (VertexLoc.Length() < total_vnum) {
		// Resize arrays (2x guardband to prevent frequent reallocations).
		VertexLoc.Resize(total_vnum * 2, nullptr);
		VertexUV.Resize(total_vnum * 2, nullptr);
		VertexDiffuse.Resize(total_vnum * 2, nullptr);
	}

	int vert, i, j;

	/*
	** Generate the vertex locations from the point locations (note that both are in camera space).
	** Vertex locations depend on the point mode and the points' orientation and size
	*/

	// This defines the loop we run: the LSB indicates whether there is a size override array, the
	// next bit indicates whether there is an orientation override array, and the higher bits
	// indicate the point mode.
	enum LoopSelectionEnum {
		TRIS_NOSIZE_NOORIENT		= ((int)TRIS << 2) + 0,
		TRIS_SIZE_NOORIENT		= ((int)TRIS << 2) + 1,
		TRIS_NOSIZE_ORIENT		= ((int)TRIS << 2) + 2,
		TRIS_SIZE_ORIENT			= ((int)TRIS << 2) + 3,
		QUADS_NOSIZE_NOORIENT	= ((int)QUADS << 2) + 0,
		QUADS_SIZE_NOORIENT		= ((int)QUADS << 2) + 1,
		QUADS_NOSIZE_ORIENT		= ((int)QUADS << 2) + 2,
		QUADS_SIZE_ORIENT			= ((int)QUADS << 2) + 3,
		SCREEN_NOSIZE_NOORIENT	= ((int)SCREENSPACE << 2) + 0,
		SCREEN_SIZE_NOORIENT		= ((int)SCREENSPACE << 2) + 1,
		SCREEN_NOSIZE_ORIENT		= ((int)SCREENSPACE << 2) + 2,
		SCREEN_SIZE_ORIENT		= ((int)SCREENSPACE << 2) + 3,
	};
	LoopSelectionEnum loop_sel = (LoopSelectionEnum)(((int)PointMode << 2) +
		(point_orientation ? 2 : 0) + (point_size ? 1 : 0));

	vert = 0;
	Vector3 *vertex_loc = &VertexLoc[0];


	/// @todo lorenzen sez: this switch statement may be done more compactly another way... look into it

	switch (loop_sel) {

		case TRIS_NOSIZE_NOORIENT:
			{
				// Setup constant vertex offsets (since size and orientation are invariants)
				Vector3 scaled_offset[3];
				scaled_offset[0] = _TriVertexLocationOrientationTable[DefaultPointOrientation][0] * DefaultPointSize;
				scaled_offset[1] = _TriVertexLocationOrientationTable[DefaultPointOrientation][1] * DefaultPointSize;
				scaled_offset[2] = _TriVertexLocationOrientationTable[DefaultPointOrientation][2] * DefaultPointSize;

				// Add vertex offsets to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					vertex_loc[vert + 0] = point_loc[i] + scaled_offset[0];
					vertex_loc[vert + 1] = point_loc[i] + scaled_offset[1];
					vertex_loc[vert + 2] = point_loc[i] + scaled_offset[2];
					vert += 3;
				}
			}
			break;

		case TRIS_SIZE_NOORIENT:
			{
				// Scale vertex offsets and add them to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					vertex_loc[vert + 0] = point_loc[i] +
						_TriVertexLocationOrientationTable[DefaultPointOrientation][0] * point_size[i];
					vertex_loc[vert + 1] = point_loc[i] +
						_TriVertexLocationOrientationTable[DefaultPointOrientation][1] * point_size[i];
					vertex_loc[vert + 2] = point_loc[i] +
						_TriVertexLocationOrientationTable[DefaultPointOrientation][2] * point_size[i];
					vert += 3;
				}
			}
			break;

		case TRIS_NOSIZE_ORIENT:
			{
				// Scale vertex offsets and add them to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					vertex_loc[vert + 0] = point_loc[i] +
						_TriVertexLocationOrientationTable[point_orientation[i]][0] * DefaultPointSize;
					vertex_loc[vert + 1] = point_loc[i] +
						_TriVertexLocationOrientationTable[point_orientation[i]][1] * DefaultPointSize;
					vertex_loc[vert + 2] = point_loc[i] +
						_TriVertexLocationOrientationTable[point_orientation[i]][2] * DefaultPointSize;
					vert += 3;
				}
			}
			break;

		case TRIS_SIZE_ORIENT:
			{
				// Scale vertex offsets and add them to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					vertex_loc[vert + 0] = point_loc[i] +
						_TriVertexLocationOrientationTable[point_orientation[i]][0] * point_size[i];
					vertex_loc[vert + 1] = point_loc[i] +
						_TriVertexLocationOrientationTable[point_orientation[i]][1] * point_size[i];
					vertex_loc[vert + 2] = point_loc[i] +
						_TriVertexLocationOrientationTable[point_orientation[i]][2] * point_size[i];
					vert += 3;
				}
			}
			break;

		case QUADS_NOSIZE_NOORIENT:
			{
				// Setup constant vertex offsets (since size and orientation are invariants)
				Vector3 scaled_offset[4];
				scaled_offset[0] = _QuadVertexLocationOrientationTable[DefaultPointOrientation][0] * DefaultPointSize;
				scaled_offset[1] = _QuadVertexLocationOrientationTable[DefaultPointOrientation][1] * DefaultPointSize;
				scaled_offset[2] = _QuadVertexLocationOrientationTable[DefaultPointOrientation][2] * DefaultPointSize;
				scaled_offset[3] = _QuadVertexLocationOrientationTable[DefaultPointOrientation][3] * DefaultPointSize;

				// Add vertex offsets to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					vertex_loc[vert + 0] = point_loc[i] + scaled_offset[0];
					vertex_loc[vert + 1] = point_loc[i] + scaled_offset[1];
					vertex_loc[vert + 2] = point_loc[i] + scaled_offset[2];
					vertex_loc[vert + 3] = point_loc[i] + scaled_offset[3];
					vert += 4;
				}
			}
			break;

		case QUADS_SIZE_NOORIENT:
			{
				// Scale vertex offsets and add them to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					vertex_loc[vert + 0] = point_loc[i] +
						_QuadVertexLocationOrientationTable[DefaultPointOrientation][0] * point_size[i];
					vertex_loc[vert + 1] = point_loc[i] +
						_QuadVertexLocationOrientationTable[DefaultPointOrientation][1] * point_size[i];
					vertex_loc[vert + 2] = point_loc[i] +
						_QuadVertexLocationOrientationTable[DefaultPointOrientation][2] * point_size[i];
					vertex_loc[vert + 3] = point_loc[i] +
						_QuadVertexLocationOrientationTable[DefaultPointOrientation][3] * point_size[i];
					vert += 4;
				}
			}
			break;

		case QUADS_NOSIZE_ORIENT:
			{
				// Scale vertex offsets and add them to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					vertex_loc[vert + 0] = point_loc[i] +
						_QuadVertexLocationOrientationTable[point_orientation[i]][0] * DefaultPointSize;
					vertex_loc[vert + 1] = point_loc[i] +
						_QuadVertexLocationOrientationTable[point_orientation[i]][1] * DefaultPointSize;
					vertex_loc[vert + 2] = point_loc[i] +
						_QuadVertexLocationOrientationTable[point_orientation[i]][2] * DefaultPointSize;
					vertex_loc[vert + 3] = point_loc[i] +
						_QuadVertexLocationOrientationTable[point_orientation[i]][3] * DefaultPointSize;
					vert += 4;
				}
			}
			break;

		case QUADS_SIZE_ORIENT:
			{
				Matrix4x4 view;
				Vector4 result;
				if (!Billboard) {
					DX8Wrapper::Get_Transform(D3DTS_VIEW,view);
				}

				// Scale vertex offsets and add them to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					if (!Billboard) {
						// If we're not billboarding, then the coordinate we have is in screen space.
						Matrix4x4 rotMat;
						D3DXMatrixRotationZ(&(D3DXMATRIX&) rotMat, ((float)point_orientation[i] / 255.0f * 2 * D3DX_PI));

						Vector4 orientedVecX = rotMat * GroundMultiplierX;
						Vector4 orientedVecY = rotMat * GroundMultiplierY;

						vertex_loc[vert + 0].X = point_loc[i].X +	(orientedVecX.X + orientedVecY.X) * point_size[i];
						vertex_loc[vert + 0].Y = point_loc[i].Y +	(orientedVecX.Y + orientedVecY.Y) * point_size[i];
						vertex_loc[vert + 0].Z = point_loc[i].Z;

						vertex_loc[vert + 1].X = point_loc[i].X +	(orientedVecX.X - orientedVecY.X) * point_size[i];
						vertex_loc[vert + 1].Y = point_loc[i].Y +	(orientedVecX.Y - orientedVecY.Y) * point_size[i];
						vertex_loc[vert + 1].Z = point_loc[i].Z;

						vertex_loc[vert + 2].X = point_loc[i].X +	-(orientedVecX.X + orientedVecY.X) * point_size[i];
						vertex_loc[vert + 2].Y = point_loc[i].Y +	-(orientedVecX.Y + orientedVecY.Y) * point_size[i];
						vertex_loc[vert + 2].Z = point_loc[i].Z;

						vertex_loc[vert + 3].X = point_loc[i].X +	(-orientedVecX.X + orientedVecY.X) * point_size[i];
						vertex_loc[vert + 3].Y = point_loc[i].Y +	(-orientedVecX.Y + orientedVecY.Y) * point_size[i];
						vertex_loc[vert + 3].Z = point_loc[i].Z;

						// now apply the view transform so that this data is in the format expected
						// upon the functions return.
						result = view*vertex_loc[vert + 0];
						vertex_loc[vert + 0].X = result.X;
						vertex_loc[vert + 0].Y = result.Y;
						vertex_loc[vert + 0].Z = result.Z;

						result = view*vertex_loc[vert + 1];
						vertex_loc[vert + 1].X = result.X;
						vertex_loc[vert + 1].Y = result.Y;
						vertex_loc[vert + 1].Z = result.Z;

						result = view*vertex_loc[vert + 2];
						vertex_loc[vert + 2].X = result.X;
						vertex_loc[vert + 2].Y = result.Y;
						vertex_loc[vert + 2].Z = result.Z;

						result = view*vertex_loc[vert + 3];
						vertex_loc[vert + 3].X = result.X;
						vertex_loc[vert + 3].Y = result.Y;
						vertex_loc[vert + 3].Z = result.Z;
					} else {

						vertex_loc[vert + 0] = point_loc[i] +
							_QuadVertexLocationOrientationTable[point_orientation[i]][0] * point_size[i];
						vertex_loc[vert + 1] = point_loc[i] +
							_QuadVertexLocationOrientationTable[point_orientation[i]][1] * point_size[i];
						vertex_loc[vert + 2] = point_loc[i] +
							_QuadVertexLocationOrientationTable[point_orientation[i]][2] * point_size[i];
						vertex_loc[vert + 3] = point_loc[i] +
							_QuadVertexLocationOrientationTable[point_orientation[i]][3] * point_size[i];
					}
					vert += 4;
				}
			}
			break;

		// Orientations are ignored for screensize pointgroups
		case SCREEN_NOSIZE_NOORIENT:
		case SCREEN_NOSIZE_ORIENT:
			{
				// Offsets need to be scaled to the current screen resolution

   			// First find x and y scale factors (sizes in pixels need to be
   			// normalized to 2D cam viewplane of -1,-1 to 1,1)
   			int xres, yres, bitdepth;
   			bool windowed;
   			WW3D::Get_Render_Target_Resolution(xres, yres, bitdepth, windowed);

   			float x_scale = (VPXMax - VPXMin) / xres;
   			float y_scale = (VPYMax - VPYMin) / yres;

				Vector3 scaled_locs[2][3];
				for (int i = 0; i < 2; i++) {
					for (int j = 0; j < 3; j++) {
						scaled_locs[i][j].X = _ScreenspaceVertexLocationSizeTable[i][j].X * x_scale;
						scaled_locs[i][j].Y = _ScreenspaceVertexLocationSizeTable[i][j].Y * y_scale;
						scaled_locs[i][j].Z = _ScreenspaceVertexLocationSizeTable[i][j].Z;
					}
				}

				// Add vertex offsets to point locations to get vertex locations
				int size_idx = (DefaultPointSize <= 1.0f) ? 0 : 1;
				for (i = 0; i < active_points; i++) {
					vertex_loc[vert + 0] = point_loc[i] + scaled_locs[size_idx][0];
					vertex_loc[vert + 1] = point_loc[i] + scaled_locs[size_idx][1];
					vertex_loc[vert + 2] = point_loc[i] + scaled_locs[size_idx][2];
					vert += 3;
				}
			}
			break;

		case SCREEN_SIZE_NOORIENT:
		case SCREEN_SIZE_ORIENT:
			{
				// Offsets need to be scaled to the current screen resolution

   			// First find x and y scale factors (sizes in pixels need to be
   			// normalized to 2D cam viewplane of -1,-1 to 1,1)
   			int xres, yres, bitdepth;
   			bool windowed;
   			WW3D::Get_Render_Target_Resolution(xres, yres, bitdepth, windowed);

   			float x_scale = (VPXMax - VPXMin) / xres;
   			float y_scale = (VPYMax - VPYMin) / yres;

				Vector3 scaled_locs[2][3];
				for (int i = 0; i < 2; i++) {
					for (int j = 0; j < 3; j++) {
						scaled_locs[i][j].X = _ScreenspaceVertexLocationSizeTable[i][j].X * x_scale;
						scaled_locs[i][j].Y = _ScreenspaceVertexLocationSizeTable[i][j].Y * y_scale;
						scaled_locs[i][j].Z = _ScreenspaceVertexLocationSizeTable[i][j].Z;
					}
				}

				// Add vertex offsets to point locations to get vertex locations
				for (i = 0; i < active_points; i++) {
					int size_idx = (point_size[i] <= 1.0f) ? 0 : 1;
					vertex_loc[vert + 0] = point_loc[i] + scaled_locs[size_idx][0];
					vertex_loc[vert + 1] = point_loc[i] + scaled_locs[size_idx][1];
					vertex_loc[vert + 2] = point_loc[i] + scaled_locs[size_idx][2];
					vert += 3;
				}
			}
			break;

		default:
			WWASSERT(0);
			break;

	}

	/*
	** Fill the UV vertex array
	*/

	unsigned int frame_mask = ~(0xFFFFFFFF << (FrameRowColumnCountLog2 + FrameRowColumnCountLog2));// To ensure frames in range
	if (point_frame) {

	/// @todo lorenzen sez: use pointer arithmetic below

		// Fill UV array according to frame override array:
		Vector2 *vertex_uv = &VertexUV[0];
		if (PointMode != QUADS) {
			// Modes with three vertices per point:
			Vector2 *uv_ptr = _TriVertexUVFrameTable[FrameRowColumnCountLog2];
			int vert = 0;
			for (int i = 0; i < active_points; i++) {
				int uv_idx = (point_frame[i] & frame_mask) * 3;
				vertex_uv[vert++] = uv_ptr[uv_idx + 0];
				vertex_uv[vert++] = uv_ptr[uv_idx + 1];
				vertex_uv[vert++] = uv_ptr[uv_idx + 2];
			}
		} else {
			// Modes with four vertices per point:
			Vector2 *uv_ptr = _QuadVertexUVFrameTable[FrameRowColumnCountLog2];
			int vert = 0;
			for (int i = 0; i < active_points; i++) {
				int uv_idx = (point_frame[i] & frame_mask) * 4;
				vertex_uv[vert++] = uv_ptr[uv_idx + 0];
				vertex_uv[vert++] = uv_ptr[uv_idx + 1];
				vertex_uv[vert++] = uv_ptr[uv_idx + 2];
				vertex_uv[vert++] = uv_ptr[uv_idx + 3];
			}
		}

	} else {
		/// @todo lorenzen sez: use pointer arithmetic below
		// Fill UV array according to frame state:
		Vector2 *vertex_uv = &VertexUV[0];
		if (PointMode != QUADS) {
			// Modes with three vertices per point:
			Vector2 *uv_ptr = _TriVertexUVFrameTable[FrameRowColumnCountLog2] + ((DefaultPointFrame & frame_mask) * 3);
			int vert = 0;
			for (int i = 0; i < active_points; i++) {
				vertex_uv[vert++] = uv_ptr[0];
				vertex_uv[vert++] = uv_ptr[1];
				vertex_uv[vert++] = uv_ptr[2];
			}
		} else {
			// Modes with four vertices per point:
			Vector2 *uv_ptr = _QuadVertexUVFrameTable[FrameRowColumnCountLog2] + ((DefaultPointFrame & frame_mask) * 4);
			int vert = 0;
			for (int i = 0; i < active_points; i++) {
				vertex_uv[vert++] = uv_ptr[0];
				vertex_uv[vert++] = uv_ptr[1];
				vertex_uv[vert++] = uv_ptr[2];
				vertex_uv[vert++] = uv_ptr[3];
			}
		}

	}

	/*
	** If we have a point color array, fill the vertex diffuse array from it.
	*/
	/// @todo lorenzen sez: use a quicker, unwrapped loop, and pointer arithmetic
	/// @todo lorenzen sez: this is a leaf function, dang it
	vert = 0;
	if (point_diffuse) {
		Vector4* vertex_color = &VertexDiffuse[0];
		for (i = 0; i < active_points; i++) {
			for (j = 0; j < verts_per_point; j++) {
				vertex_color[vert + j] = point_diffuse[i];
			}
			vert += verts_per_point;
		}
	}
}


/**************************************************************************
 * PointGroupClass::_Init -- Create static data.                          *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   06/28/2000 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::_Init()
{
	int i, j;

	/*
	** Fill vertex location orientation tables
	*/

	// Unrotated locations
	Vector3 tri_locs[3] = {
		Vector3(0.0f, -2.0f, 0.0f),
		Vector3(-1.732f, 1.0f, 0.0f),
		Vector3(1.732f, 1.0f, 0.0f)
	};
	Vector3 quad_locs[4] = {
		Vector3(-0.5f, 0.5f, 0.0f),
		Vector3(-0.5f, -0.5f, 0.0f),
		Vector3(0.5f, -0.5f, 0.0f),
		Vector3(0.5f, 0.5f, 0.0f)
	};

/// @todo lorenzen sez: unwrap loop and use pointer arithmetic (if this gets called a lot)

	float angle = 0.0f;	// In radians
	float angle_step = (WWMATH_PI * 2.0f) / 256.0f;	// In radians
	for (i = 0; i < 256; i++) {
		float c = WWMath::Fast_Cos(angle);
		float s = WWMath::Fast_Sin(angle);
		for (j = 0; j < 3; j++) {
			_TriVertexLocationOrientationTable[i][j].X = tri_locs[j].X * c - tri_locs[j].Y * s;
			_TriVertexLocationOrientationTable[i][j].Y = tri_locs[j].X * s + tri_locs[j].Y * c;
			_TriVertexLocationOrientationTable[i][j].Z = tri_locs[j].Z;
		}
		for (j = 0; j < 4; j++) {
			_QuadVertexLocationOrientationTable[i][j].X = quad_locs[j].X * c - quad_locs[j].Y * s;
			_QuadVertexLocationOrientationTable[i][j].Y = quad_locs[j].X * s + quad_locs[j].Y * c;
			_QuadVertexLocationOrientationTable[i][j].Z = quad_locs[j].Z;
		}
		angle += angle_step;
	}

	/*
	** Fill frame UV tables
	*/
	// Unscaled / untranslated UVs
	Vector2 tri_uvs[3] = {
		Vector2(0.5f, 0.0f),
		Vector2(0.0f, 0.866f),
		Vector2(1.0f, 0.866f)
	};
	Vector2 quad_uvs[4] = {
		Vector2(0.0f, 0.0f),
		Vector2(0.0f, 1.0f),
		Vector2(1.0f, 1.0f),
		Vector2(1.0f, 0.0f)
	};

	/// @todo lorenzen sez: umwrap and use pointers
	for (i = 0; i < 5; i++) {

		unsigned int rows = 1 << i;
		unsigned int count = rows * rows;

		Vector2 *tri_table = _TriVertexUVFrameTable[i] = W3DNEWARRAY Vector2[count * 3];
		Vector2 *quad_table = _QuadVertexUVFrameTable[i] = W3DNEWARRAY Vector2[count * 4];

		Vector2 corner(0.0f, 0.0f);
		float scale = 1.0f / (float)rows;

		int tri_idx = 0;
		int quad_idx = 0;
		for (unsigned int v = 0; v < rows; v++) {
			for (unsigned int u = 0; u < rows; u++) {

				tri_table[tri_idx++] = corner + (tri_uvs[0] * scale);
				tri_table[tri_idx++] = corner + (tri_uvs[1] * scale);
				tri_table[tri_idx++] = corner + (tri_uvs[2] * scale);

				quad_table[quad_idx++] = corner + (quad_uvs[0] * scale);
				quad_table[quad_idx++] = corner + (quad_uvs[1] * scale);
				quad_table[quad_idx++] = corner + (quad_uvs[2] * scale);
				quad_table[quad_idx++] = corner + (quad_uvs[3] * scale);

				corner.X += scale;
			}
			corner.Y += scale;
			corner.X = 0.0f;
		}
	}

	// Create the IBs
	Tris=NEW_REF(DX8IndexBufferClass,(MAX_TRI_IB_SIZE));
	Quads=NEW_REF(DX8IndexBufferClass,(MAX_QUAD_IB_SIZE));
	SortingTris=NEW_REF(SortingIndexBufferClass,(MAX_TRI_IB_SIZE));
	SortingQuads=NEW_REF(SortingIndexBufferClass,(MAX_QUAD_IB_SIZE));

	// Fill up the IBs
	{
		DX8IndexBufferClass::WriteLockClass locktris(Tris);
		unsigned short *ib=locktris.Get_Index_Array();
		for (i=0; i<MAX_TRI_IB_SIZE; i++) ib[i]=(unsigned short) i;
	}

	{
		unsigned short vert=0;
		DX8IndexBufferClass::WriteLockClass lockquads(Quads);
		unsigned short *ib=lockquads.Get_Index_Array();
		vert=0;
		for (i=0; i<MAX_QUAD_IB_SIZE; i+=6)
		{
/// @todo lorenzen sez: pointer arithmetic like "++ib=vert+1"

			ib[i]=vert;
			ib[i+1]=vert+1;
			ib[i+2]=vert+2;

			ib[i+3]=vert+2;
			ib[i+4]=vert+3;
			ib[i+5]=vert;
			vert+=4;
		}
	}

	{
		SortingIndexBufferClass::WriteLockClass locktris(SortingTris);
		unsigned short *ib=locktris.Get_Index_Array();
		for (i=0; i<MAX_TRI_IB_SIZE; i++) ib[i]=(unsigned short) i;
	}

	{
		unsigned short vert=0;
		SortingIndexBufferClass::WriteLockClass lockquads(SortingQuads);
		unsigned short *ib=lockquads.Get_Index_Array();
		vert=0;
		for (i=0; i<MAX_QUAD_IB_SIZE; i+=6)
		{
			/// @todo lorenzen sez: pointers!
			ib[i]=vert;
			ib[i+1]=vert+1;
			ib[i+2]=vert+2;

			ib[i+3]=vert+2;
			ib[i+4]=vert+3;
			ib[i+5]=vert;
			vert+=4;
		}
	}

	PointMaterial=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
}


/**************************************************************************
 * PointGroupClass::_Shutdown -- Destroy static data.                     *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:                                                              *
 *                                                                        *
 * HISTORY:                                                               *
 *   06/28/2000 NH  : Created.                                            *
 *========================================================================*/
void PointGroupClass::_Shutdown()
{
	for (int i = 0; i < 5; i++) {
		delete [] _TriVertexUVFrameTable[i];
		delete [] _QuadVertexUVFrameTable[i];
	}
	REF_PTR_RELEASE(PointMaterial);
	REF_PTR_RELEASE(SortingQuads);
	REF_PTR_RELEASE(SortingTris);
	REF_PTR_RELEASE(Quads);
	REF_PTR_RELEASE(Tris);
	transformed_loc.Clear();
	VertexLoc.Clear();
	VertexDiffuse.Clear();
	VertexUV.Clear();
}






/**************************************************************************
 * PointGroupClass::RenderVolumeParticles -- draw a point group.sandwich  *
 *                                                                        *
 *	This is a specialized renderer. It will draw the particle repeatedly  *
 *  while attenuating opacity appropriately, and bumping the z-position.  *
 *  This is actually done by cloning the particle over and over at        *
 *  successively closer positions to the camera. Very Slow!               *
 *                                                                        *
 * INPUT:                                                                 *
 *                                                                        *
 * OUTPUT:                                                                *
 *                                                                        *
 * WARNINGS:      USE SPARINGLY, IT IS EXPENSIVE!!!!!                     *
 *                                                                        *
 * HISTORY:                                                               *
 *   12/03/2002	Mark Lorenzen		Created.                                  *
 *																		                                    *
 *========================================================================*/
#define MAX_VOLUME_PARTICLE_DEPTH ( 16 )
void PointGroupClass::RenderVolumeParticle(RenderInfoClass &rinfo, unsigned int depth )
{

	if ( depth <= 1 ) //oops,wrong number
	{
		Render( rinfo );
		return;
	}

	if ( depth > MAX_VOLUME_PARTICLE_DEPTH )
		depth = MAX_VOLUME_PARTICLE_DEPTH; // sanity

	Shader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);

	if (PointCount == 0)
		return;

	WWASSERT(PointLoc && PointLoc->Get_Array());

	// Pointers which point into existing buffers (member or static):
	Vector3 *current_loc = nullptr;
	Vector4 *current_diffuse = nullptr;
	float *current_size = nullptr;
	unsigned char *current_orient = nullptr;
	unsigned char *current_frame = nullptr;

	// If there is a color or alpha array enable gradient in shader - otherwise disable.
  float value_255 = 0.9961f;	//254 / 255
	bool default_white_opaque = (	DefaultPointColor.X > value_255 &&
											DefaultPointColor.Y > value_255 &&
											DefaultPointColor.Z > value_255 &&
											DefaultPointAlpha > value_255);

	// The reason we check for lack of texture here is that SR seems to render black triangles
	// rather than white triangles as would be expected) when there is no texture AND no gradient.
	if (PointDiffuse || !default_white_opaque || !Texture) {
		Shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE);
	} else {
		Shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);
	}

	// If Texture is non-null enable texturing in shader - otherwise disable.
	if (Texture) {
		Shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
	} else {
		Shader.Set_Texturing(ShaderClass::TEXTURING_DISABLE);
	}

		// Get the world and view matrices
		Matrix4x4 view;
		DX8Wrapper::Get_Transform(D3DTS_VIEW,view);



	//// VOLUME_PARTICLE LOOP ///////////////
	for ( unsigned int t = 0; t < depth; ++t )
	{





		// If there is an active point table, use it to compress the point
		// locations/colors/alphas/sizes/orientations/frames.
		if (APT) {
			// Resize compressed result arrays if needed (2x guardband to prevent
			// frequent reallocations):

			/// @todo lorenzen sez: precompute pointers to indexed array elements, below

			if (compressed_loc.Length() < PointCount) {
				compressed_loc.Resize(PointCount * 2);
			}
			VectorProcessorClass::CopyIndexed(&compressed_loc[0],
				PointLoc->Get_Array(), APT->Get_Array(), PointCount);
			current_loc = &compressed_loc[0];
			if (PointDiffuse) {
				if (compressed_diffuse.Length() < PointCount) {
					compressed_diffuse.Resize(PointCount * 2);
				}
				VectorProcessorClass::CopyIndexed(&compressed_diffuse[0],
					PointDiffuse->Get_Array(), APT->Get_Array(), PointCount);
				current_diffuse = &compressed_diffuse[0];
			}
			if (PointSize) {
				if (compressed_size.Length() < PointCount) {
					compressed_size.Resize(PointCount * 2);
				}
				VectorProcessorClass::CopyIndexed(&compressed_size[0],
					PointSize->Get_Array(), APT->Get_Array(), PointCount);
				current_size = &compressed_size[0];
			}
			if (PointOrientation) {
				if (compressed_orient.Length() < PointCount) {
					compressed_orient.Resize(PointCount * 2);
				}
				VectorProcessorClass::CopyIndexed(&compressed_orient[0],
					PointOrientation->Get_Array(), APT->Get_Array(), PointCount);
				current_orient = &compressed_orient[0];
			}
			if (PointFrame) {
				if (compressed_frame.Length() < PointCount) {
					compressed_frame.Resize(PointCount * 2);
				}
				VectorProcessorClass::CopyIndexed(&compressed_frame[0],
					PointFrame->Get_Array(), APT->Get_Array(), PointCount);
				current_frame = &compressed_frame[0];
			}
		} else {
			current_loc = PointLoc->Get_Array();
			if (PointDiffuse) {
				current_diffuse = PointDiffuse->Get_Array();
			}
			if (PointSize) {
				current_size = PointSize->Get_Array();
			}
			if (PointOrientation) {
				current_orient = PointOrientation->Get_Array();
			}
			if (PointFrame) {
				current_frame = PointFrame->Get_Array();
			}
		}





		// Transform the point locations from worldspace to camera space if needed
		// (i.e. if they are not already in camera space):

		// need to interrupt this processing. If we are not billboarding, then we need the actual position
		// of the vertice to lay it down flat.
		if (Get_Flag(TRANSFORM) && Billboard) {
			// Resize transformed location array if needed (2x guardband to prevent
			// frequent reallocations):
			if (transformed_loc.Length() < PointCount) {
				transformed_loc.Resize(PointCount * 2);
			}
			// Not using vector processor class because we are discarding w
			// Not using T&L in DX8 because we don't want DX8 to transform
			// 3 times per particle when we can do it once
			float recipDepth = 0.1f / (float)depth;

			float shiftInc = ( t *  *current_size * recipDepth );

			Vector3 volumeLayerShift;
			Vector3 cameraPosition = rinfo.Camera.Get_Position();

			for (int i=0; i<PointCount; i++)
			{
				/// @todo lorenzen sez: use pointer arithmetic here and a fast while loop
				Vector3 cameraToPointDelta;
				Vector3::Subtract( cameraPosition, current_loc[i], &cameraToPointDelta );
				cameraToPointDelta.Normalize();
				cameraToPointDelta.X *= shiftInc;
				cameraToPointDelta.Y *= shiftInc;
				cameraToPointDelta.Z *= shiftInc;

				Vector3 temp;
				temp.X = current_loc[i].X + cameraToPointDelta.X;
				temp.Y = current_loc[i].Y + cameraToPointDelta.Y;
				temp.Z = current_loc[i].Z + cameraToPointDelta.Z;

				Vector4 result= view * temp;
				transformed_loc[i].X=result.X;
				transformed_loc[i].Y=result.Y;
				transformed_loc[i].Z=result.Z;
			}
			current_loc = &transformed_loc[0];
		}

		// Update the arrays with the offsets.
		int vnum, pnum;

		//float attenuator = 1.0f - (1.0f/(float)depth);
		//current_diffuse->X *= attenuator;
		//current_diffuse->Y *= attenuator;
		//current_diffuse->Z *= attenuator;
		//current_diffuse->W *= attenuator;

		Update_Arrays(current_loc, current_diffuse, current_size, current_orient, current_frame,
			PointCount, PointLoc->Get_Count(), vnum, pnum);

		// the locations are now in view space
		// so set world and view matrices to identity and render

		Matrix4x4 identity(true);
		DX8Wrapper::Set_Transform(D3DTS_WORLD,identity);
		DX8Wrapper::Set_Transform(D3DTS_VIEW,identity);

		DX8Wrapper::Set_Material(PointMaterial);
		DX8Wrapper::Set_Shader(Shader);
		DX8Wrapper::Set_Texture(0,Texture);

		// Enable sorting if the primitives are translucent and alpha testing is not enabled.
		// TheSuperHackers @info Volumetric particles, both billboarded and ground-aligned, must have sorting enabled to
		// ensure accurate alpha-blending because these particles have stacked layers that don't face the camera straight on.
		const bool sort = (Shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO) && (Shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_DISABLE) && (WW3D::Is_Sorting_Enabled());

		IndexBufferClass *indexbuffer;
		int	verticesperprimitive;/// lorenzen fixed
		int current;
		int delta;

		/// @todo lorenzen sez: if tri-based particles are not supported, elim this test
		if (PointMode == QUADS) {
			verticesperprimitive = 2;
			indexbuffer = sort ? static_cast <IndexBufferClass*> (SortingQuads) : static_cast <IndexBufferClass*> (Quads);
		} else {
			verticesperprimitive = 3;
			indexbuffer = sort ? static_cast <IndexBufferClass*> (SortingTris) : static_cast <IndexBufferClass*> (Tris);
		}


		float nudge = 0;

		current = 0;
		while (current<vnum)
		{
			delta=MIN(vnum-current,MAX_VB_SIZE);
			DynamicVBAccessClass PointVerts (sort ? BUFFER_TYPE_DYNAMIC_SORTING : BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, delta);

			// Copy in the data to the VB
			{
				DynamicVBAccessClass::WriteLockClass Lock(&PointVerts);
				int i;
				unsigned char *vb=(unsigned char*)Lock.Get_Formatted_Vertex_Array();
				const FVFInfoClass& fvfinfo = PointVerts.FVF_Info();


				for (i = current; i < current + delta; i++)
				{
					/// @todo lorenzen sez: use pointer arithmetic throughout this block
					/// @todo lorenzen sez: delare thes locals outside this loop
					/// @todo lorenzen sez: use a fast while loop
					// Copy Locations
					*(Vector3*)(vb+fvfinfo.Get_Location_Offset()) = VertexLoc[i];

					if (current_diffuse) {
						unsigned color=DX8Wrapper::Convert_Color_Clamp(VertexDiffuse[i]);
						*(unsigned int*)(vb+fvfinfo.Get_Diffuse_Offset())=color;
					}
					else
						*(unsigned int*)(vb+fvfinfo.Get_Diffuse_Offset())=
							DX8Wrapper::Convert_Color_Clamp(Vector4(DefaultPointColor[0],DefaultPointColor[1],DefaultPointColor[2],DefaultPointAlpha));
					*(Vector2*)(vb+fvfinfo.Get_Tex_Offset(0))=VertexUV[i];
					vb+=fvfinfo.Get_FVF_Size();
				}
			}

			DX8Wrapper::Set_Index_Buffer (indexbuffer, 0);
			DX8Wrapper::Set_Vertex_Buffer (PointVerts);

			/// @todo lorenzen sez: precompute these params, above


			if ( sort )
					SortingRendererClass::Insert_Triangles (0, delta / verticesperprimitive, 0, delta);
			else
				DX8Wrapper::Draw_Triangles (0, delta / verticesperprimitive, 0, delta);


			current+=delta;
		}



	}




	// restore the matrices
	DX8Wrapper::Set_Transform(D3DTS_VIEW,view);
}
