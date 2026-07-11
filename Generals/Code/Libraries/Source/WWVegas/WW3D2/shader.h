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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /VSS_Sync/ww3d2/shader.h                                    $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 8/29/01 7:29p                                               $*
 *                                                                                             *
 *                    $Revision:: 16                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"

class DX8Wrapper;
struct W3dMaterial3Struct;
class StringClass;

// Re-written shader class
// Hector Yee 1/24/01

enum ShaderShiftConstants
{
	SHIFT_DEPTHCOMPARE			= 0,	// bit shift for depth comparison setting
	SHIFT_DEPTHMASK				= 3,	// bit shift for depth mask setting
	SHIFT_COLORMASK				= 4,	// bit shift for color mask setting
	SHIFT_DSTBLEND					= 5,	// bit shift for destination blend setting
	SHIFT_FOG						= 8,	// bit shift for fog setting
	SHIFT_PRIGRADIENT				= 10,	// bit shift for primary gradient setting
	SHIFT_SECGRADIENT				= 13,	// bit shift for secondary gradient setting
	SHIFT_SRCBLEND					= 14,	// bit shift for source blend setting
	SHIFT_TEXTURING				= 16,	// bit shift for texturing setting (1 bit)
	SHIFT_NPATCHENABLE			= 17,	// bit shift for npatch enabling
	SHIFT_ALPHATEST				= 18,	// bit shift for alpha test setting
	SHIFT_CULLMODE					= 19,	// bit shift for cullmode setting
	SHIFT_POSTDETAILCOLORFUNC	= 20,	// bit shift for post-detail color function setting
	SHIFT_POSTDETAILALPHAFUNC	= 24	// bit shift for post-detail alpha function setting
};

#define SHADE_CNST(depth_compare, depth_mask, color_mask, src_blend, dst_blend, fog, pri_grad, sec_grad, texture, alpha_test, cullmode, post_det_color, post_det_alpha) \
	(	(depth_compare) << SHIFT_DEPTHCOMPARE | (depth_mask) << SHIFT_DEPTHMASK | \
		(color_mask) << SHIFT_COLORMASK | (dst_blend) << SHIFT_DSTBLEND | (fog) << SHIFT_FOG | \
		(pri_grad) << SHIFT_PRIGRADIENT | (sec_grad) << SHIFT_SECGRADIENT | \
		(src_blend) << SHIFT_SRCBLEND | (texture) << SHIFT_TEXTURING | \
		(alpha_test) << SHIFT_ALPHATEST | (cullmode) << SHIFT_CULLMODE | \
		(post_det_color) << SHIFT_POSTDETAILCOLORFUNC | \
		(post_det_alpha) << SHIFT_POSTDETAILALPHAFUNC)



class ShaderClass
{
	friend DX8Wrapper;

	void	Apply();
public:

	enum AlphaTestType
	{
		ALPHATEST_DISABLE= 0,// disable alpha testing (default)
		ALPHATEST_ENABLE,		// enable alpha testing
	};

	enum DepthCompareType
	{
		PASS_NEVER=0,        	// pass never
		PASS_LESS,        	// pass if incoming less than stored
		PASS_EQUAL,        	// pass if incoming equal to stored
		PASS_LEQUAL,			// pass if incoming less than or equal to stored (default)
		PASS_GREATER,        // pass if incoming greater than stored
		PASS_NOTEQUAL,       // pass if incoming not equal to stored
		PASS_GEQUAL,			// pass if incoming greater than or equal to stored
		PASS_ALWAYS,			// pass always
	};

	enum DepthMaskType
	{
		DEPTH_WRITE_DISABLE=0,	// disable depth buffer writes
		DEPTH_WRITE_ENABLE,		// enable depth buffer writes		(default)
	};

	enum ColorMaskType
	{
		COLOR_WRITE_DISABLE=0,	// disable color buffer writes
		COLOR_WRITE_ENABLE,		// enable color buffer writes		(default)
	};

 	enum DetailAlphaFuncType
 	{
		DETAILALPHA_DISABLE=0,	// local (default)
		DETAILALPHA_DETAIL,		// other
		DETAILALPHA_SCALE,		// local * other
		DETAILALPHA_INVSCALE,	// ~(~local * ~other) = local + (1-local)*other
	};

	enum DetailColorFuncType
 	{
 		DETAILCOLOR_DISABLE=0,		// 0000	local (default)
		DETAILCOLOR_DETAIL,			// 0001	other
		DETAILCOLOR_SCALE,			// 0010	local * other
		DETAILCOLOR_INVSCALE,		// 0011	~(~local * ~other) = local + (1-local)*other
		DETAILCOLOR_ADD,				// 0100	local + other
		DETAILCOLOR_SUB,				// 0101	local - other
		DETAILCOLOR_SUBR,				// 0110	other - local
		DETAILCOLOR_BLEND,			// 0111	(localAlpha)*local + (~localAlpha)*other
		DETAILCOLOR_DETAILBLEND,	//	1000	(otherAlpha)*local + (~otherAlpha)*other
		DETAILCOLOR_ADDSIGNED,		// 1001	(local + other - 0.5)
		DETAILCOLOR_ADDSIGNED2X,	// 1010	(local + other - 0.5) * 2
		DETAILCOLOR_SCALE2X,			// 1011	local * other * 2
		DETAILCOLOR_MODALPHAADDCOLOR,	// 1100 local + localAlpha * other
	};

	enum CullModeType
	{
		CULL_MODE_DISABLE=0,
		CULL_MODE_ENABLE,
	};

	enum NPatchEnableType
	{
		NPATCH_DISABLE=0,
		NPATCH_ENABLE,
	};

  	enum DstBlendFuncType
  	{
  		DSTBLEND_ZERO=0,					// destination pixel doesn't affect blending (default)
  		DSTBLEND_ONE,						// destination pixel added unmodified
 		DSTBLEND_SRC_COLOR,				// destination pixel multiplied by fragment RGB components
 		DSTBLEND_ONE_MINUS_SRC_COLOR,	// destination pixel multiplied by one minus (i.e. inverse) fragment RGB components
 		DSTBLEND_SRC_ALPHA,        	// destination pixel multiplied by fragment alpha component
 		DSTBLEND_ONE_MINUS_SRC_ALPHA, // destination pixel multiplied by fragment inverse alpha

		DSTBLEND_MAX
  	};

	enum FogFuncType
 	{
 		FOG_DISABLE=0,			// don't perform fogging (default)
 		FOG_ENABLE,        	// apply fog, f*fogColor + (1-f)*fragment
 		FOG_SCALE_FRAGMENT,  // fog scalar value multiplies fragment, (1-f)*fragment
 		FOG_WHITE,				// fog scalar value replaces fragment, f*fogColor
 	};

 	enum PriGradientType
 	{
 		GRADIENT_DISABLE=0,				//	000	disable primary gradient (same as OpenGL 'decal' texture blend)
		GRADIENT_MODULATE,				//	001	modulate fragment ARGB by gradient ARGB (default)
		GRADIENT_ADD,						//	010	add gradient RGB to fragment RGB, copy gradient A to fragment A
		GRADIENT_BUMPENVMAP,				// 011	environment-mapped bump mapping
		GRADIENT_BUMPENVMAPLUMINANCE,	// 100	environment-mapped bump mapping with luminance control
		GRADIENT_MODULATE2X,				// 101	modulate fragment ARGB by gradient ARGB and multiply RGB by 2
 	};

	enum SecGradientType
	{
		SECONDARY_GRADIENT_DISABLE=0,	// don't draw secondary gradient (default)
		SECONDARY_GRADIENT_ENABLE,    // add secondary gradient RGB to fragment RGB
	};

	enum SrcBlendFuncType
  	{
  		SRCBLEND_ZERO=0,						// fragment not added to color buffer
  		SRCBLEND_ONE,							// fragment added unmodified to color buffer (default)
 		SRCBLEND_SRC_ALPHA,					// fragment RGB components multiplied by fragment A
 		SRCBLEND_ONE_MINUS_SRC_ALPHA,		// fragment RGB components multiplied by fragment inverse (one minus) A

		SRCBLEND_MAX
  	};

	enum TexturingType
	{
		TEXTURING_DISABLE=0, // no texturing (treat fragment initial color as 1,1,1,1)
		TEXTURING_ENABLE,    // enable texturing
	};

	enum StaticSortCategoryType
	{
		SSCAT_OPAQUE=0,
		SSCAT_ALPHA_TEST,
		SSCAT_ADDITIVE,
		SSCAT_SCREEN,
		SSCAT_OTHER
	};

	enum
	{
		MASK_DEPTHCOMPARE			= (7<<0),			// mask for depth comparison setting
		MASK_DEPTHMASK				= (1<<3),			// mask for depth mask setting
		MASK_COLORMASK				= (1<<4),			// mask for color mask setting
		MASK_DSTBLEND				= (7<<5),			// mask for destination blend setting
		MASK_FOG						= (3<<8),			// mask for fog setting
		MASK_PRIGRADIENT			= (7<<10),			// mask for primary gradient setting
		MASK_SECGRADIENT			= (1<<13),			// mask for secondary gradient setting
		MASK_SRCBLEND				= (3<<14),			// mask for source blend setting
		MASK_TEXTURING				= (1<<16),			// mask for texturing setting
		MASK_NPATCHENABLE			= (1<<17),			// mask for npatch enable
		MASK_ALPHATEST				= (1<<18),			// mask for alpha test enable
		MASK_CULLMODE				= (1<<19),			// mask for cullmode setting
		MASK_POSTDETAILCOLORFUNC= (15<<20),			// mask for post detail color function setting
		MASK_POSTDETAILALPHAFUNC= (7<<24)			// mask for post detail alpha function setting
	};

	ShaderClass()
	{	Reset(); }

	ShaderClass(const ShaderClass & s)
	{	ShaderBits=s.ShaderBits; }

	ShaderClass(const unsigned int d)
	{	ShaderBits=d;	}

	bool operator == (const ShaderClass & s) { return ShaderBits == s.ShaderBits; }
	bool operator != (const ShaderClass & s) { return ShaderBits != s.ShaderBits; }

	unsigned int Get_Bits() const
	{	return ShaderBits; }

	int Uses_Alpha() const
	{
		// check if alpha test is enabled
		if (Get_Alpha_Test() != ALPHATEST_DISABLE)
		return true;

		DstBlendFuncType dst = Get_Dst_Blend_Func();
		if (dst == DSTBLEND_SRC_ALPHA || dst == DSTBLEND_ONE_MINUS_SRC_ALPHA)
		return true;

		SrcBlendFuncType src = Get_Src_Blend_Func();

		return (src == SRCBLEND_SRC_ALPHA || src == SRCBLEND_ONE_MINUS_SRC_ALPHA);
	}

	int	Uses_Fog() const
	{
		return (Get_Fog_Func() != FOG_DISABLE);
	}

	int	Uses_Primary_Gradient() const
	{
		return (Get_Primary_Gradient() != GRADIENT_DISABLE);
	}

	int	Uses_Secondary_Gradient() const
	{
		return (Get_Secondary_Gradient() != SECONDARY_GRADIENT_DISABLE);
	}

	int	Uses_Texture() const
	{ return (Get_Texturing() != TEXTURING_DISABLE); }

	int	Uses_Post_Detail_Texture() const
	{
		if (Get_Texturing() == TEXTURING_DISABLE)
		return false;
		return ((Get_Post_Detail_Color_Func() != DETAILCOLOR_DISABLE) || (Get_Post_Detail_Alpha_Func() != DETAILALPHA_DISABLE));
	}

	inline void	Reset();

	DepthCompareType		Get_Depth_Compare()	const								{ return (DepthCompareType)(ShaderBits&MASK_DEPTHCOMPARE>>SHIFT_DEPTHCOMPARE); }
	DepthMaskType			Get_Depth_Mask() const									{ return (DepthMaskType)((ShaderBits&MASK_DEPTHMASK)>>SHIFT_DEPTHMASK); }
	ColorMaskType			Get_Color_Mask() const									{ return (ColorMaskType)((ShaderBits&MASK_COLORMASK)>>SHIFT_COLORMASK); }
	DetailAlphaFuncType	Get_Post_Detail_Alpha_Func() const					{ return (DetailAlphaFuncType)((ShaderBits&MASK_POSTDETAILALPHAFUNC)>>SHIFT_POSTDETAILALPHAFUNC); }
	DetailColorFuncType	Get_Post_Detail_Color_Func() const					{ return (DetailColorFuncType)((ShaderBits&MASK_POSTDETAILCOLORFUNC)>>SHIFT_POSTDETAILCOLORFUNC); }
	AlphaTestType			Get_Alpha_Test() const									{ return (AlphaTestType)((ShaderBits&MASK_ALPHATEST)>>SHIFT_ALPHATEST); }
	CullModeType			Get_Cull_Mode() const									{ return (CullModeType)((ShaderBits&MASK_CULLMODE)>>SHIFT_CULLMODE); }
	DstBlendFuncType		Get_Dst_Blend_Func() const							{ return (DstBlendFuncType)((ShaderBits&MASK_DSTBLEND)>>SHIFT_DSTBLEND); }
	FogFuncType			Get_Fog_Func() const									{ return (FogFuncType)((ShaderBits&MASK_FOG)>>SHIFT_FOG); }
	PriGradientType		Get_Primary_Gradient() const							{ return (PriGradientType)((ShaderBits&MASK_PRIGRADIENT)>>SHIFT_PRIGRADIENT); }
	SecGradientType		Get_Secondary_Gradient() const						{ return (SecGradientType)((ShaderBits&MASK_SECGRADIENT)>>SHIFT_SECGRADIENT); }
	SrcBlendFuncType		Get_Src_Blend_Func() const							{ return (SrcBlendFuncType)((ShaderBits&MASK_SRCBLEND)>>SHIFT_SRCBLEND); }
	TexturingType			Get_Texturing() const									{ return (TexturingType)((ShaderBits&MASK_TEXTURING)>>SHIFT_TEXTURING); }
	NPatchEnableType		Get_NPatch_Enable() const								{ return (NPatchEnableType)((ShaderBits&MASK_NPATCHENABLE)>>SHIFT_NPATCHENABLE); }

	void	Set_Depth_Compare(DepthCompareType x)					{ ShaderBits&=~MASK_DEPTHCOMPARE;ShaderBits|=(x<<SHIFT_DEPTHCOMPARE);	}
	void	Set_Depth_Mask(DepthMaskType x)							{ ShaderBits&=~MASK_DEPTHMASK; ShaderBits|=(x<<SHIFT_DEPTHMASK);	}
	void	Set_Color_Mask(ColorMaskType x)							{ ShaderBits&=~MASK_COLORMASK; ShaderBits|=(x<<SHIFT_COLORMASK);	}
	void	Set_Post_Detail_Alpha_Func(DetailAlphaFuncType x)	{ ShaderBits&=~MASK_POSTDETAILALPHAFUNC;ShaderBits|=(x<<SHIFT_POSTDETAILALPHAFUNC);	}
	void	Set_Post_Detail_Color_Func(DetailColorFuncType x)	{ ShaderBits&=~MASK_POSTDETAILCOLORFUNC;ShaderBits|=(x<<SHIFT_POSTDETAILCOLORFUNC);	}
	void	Set_Alpha_Test(AlphaTestType x)							{ ShaderBits&=~MASK_ALPHATEST; ShaderBits|=(x<<SHIFT_ALPHATEST);		}
	void	Set_Cull_Mode(CullModeType x)								{ ShaderBits&=~MASK_CULLMODE; ShaderBits|=(x<<SHIFT_CULLMODE);		}
	void	Set_Dst_Blend_Func(DstBlendFuncType x)					{ ShaderBits&=~MASK_DSTBLEND; ShaderBits|=(x<<SHIFT_DSTBLEND);		}
	void	Set_Fog_Func(FogFuncType x)								{ ShaderBits&=~MASK_FOG; ShaderBits|=(x<<SHIFT_FOG);			}
	void	Set_Primary_Gradient(PriGradientType x)				{ ShaderBits&=~MASK_PRIGRADIENT;ShaderBits|=(x<<SHIFT_PRIGRADIENT);	}
	void	Set_Secondary_Gradient(SecGradientType x)				{ ShaderBits&=~MASK_SECGRADIENT;ShaderBits|=(x<<SHIFT_SECGRADIENT);	}
	void	Set_Src_Blend_Func(SrcBlendFuncType x)					{ ShaderBits&=~MASK_SRCBLEND;ShaderBits|=(x<<SHIFT_SRCBLEND);		}
	void	Set_Texturing(TexturingType x)							{ ShaderBits&=~MASK_TEXTURING; ShaderBits|=(x<<SHIFT_TEXTURING);	}
	void	Set_NPatch_Enable(NPatchEnableType x)					{ ShaderBits&=~MASK_NPATCHENABLE; ShaderBits|=(x<<SHIFT_NPATCHENABLE);	}

	void	Init_From_Material3(const W3dMaterial3Struct & mat3);
	void	Enable_Fog (const char *source);

	// helper function for static sort system
	StaticSortCategoryType	Get_SS_Category() const;
	int							Guess_Sort_Level() const;

	// DX 8 state management routines
	static void	Invalidate() { ShaderDirty=true; }

	// Global backface culling invert.  This interface can be used to globally invert all backface
	// culling.  This is a global setting and will affect everything being rendered.  Typically it
	// should be left alone at the default setting.  Renegade uses this feature to render the entire
	// scene's backfacing polygons only; this is used in a VIS-debugging process.  In order for this
	// to work, you will have to ww3d::Flush all rendering before changing the setting back.
	// NORMAL USERS SHOULD NEVER CALL THESE FUNCTIONS!
	static void				Invert_Backface_Culling(bool onoff);
	static bool				Is_Backface_Culling_Inverted();

	const StringClass& Get_Description(StringClass& str) const;

	// These are a bunch of predefined shaders for common cases. None of them
	// have fogging since "no fog" is the surrender default and usage of fog
	// changes from app to app - if you want a fogging shader just grab one of
	// these and add fog to it.

	// Texturing, zbuffer, primary gradient, no blending
	static ShaderClass _PresetOpaqueShader;

	// Texturing, zbuffer, primary gradient, additive blending
	static ShaderClass _PresetAdditiveShader;

	// Texturing, zbuffer, primary gradient, additive blending, bumpenvmap
	static ShaderClass _PresetBumpenvmapShader;

	// Texturing, zbuffer, primary gradient, alpha blending
	static ShaderClass _PresetAlphaShader;

	// Texturing, zbuffer, primary gradient, multiplicative blending
	static ShaderClass _PresetMultiplicativeShader;

	// Texturing, no zbuffer reading/writing, no gradients, no blending, no
	// fogging - mostly for opaque 2D objects.
	static ShaderClass _PresetOpaque2DShader;

	// Texturing, default zbuffer reading, no zbuffer writing, no gradients, no blending, no
	// fogging - mostly for opaque sprites
	static ShaderClass _PresetOpaqueSpriteShader;

	// Texturing, no zbuffer reading/writing, no gradients, additive blending,
	// no fogging - mostly for additive 2D objects.
	static ShaderClass _PresetAdditive2DShader;

	// Texturing, no zbuffer reading/writing, no gradients, alpha blending, no
	// fogging - mostly for alpha-blended 2D objects.
	static ShaderClass _PresetAlpha2DShader;

	// Texturing, default zbuffer reading, no zbuffer writing, no gradients,
	// additive blending, no fogging - mostly for use in additive sprite
	// objects.
	static ShaderClass _PresetAdditiveSpriteShader;

	// Texturing, default zbuffer reading, no zbuffer writing, no gradients,
	// alpha blending, no fogging - mostly for use in alpha-blended sprite
	// objects.
	static ShaderClass _PresetAlphaSpriteShader;

	// No texturing, default zbuffer reading/writing, primary gradient, no
	// blending, no fogging - mostly for use in solid-colored opaque objects.
	static ShaderClass _PresetOpaqueSolidShader;

	// No texturing, default zbuffer reading, no zbuffer writing, primary
	// gradient, additive blending, no fogging - mostly for use in
	// solid-colored additive objects.
	static ShaderClass _PresetAdditiveSolidShader;

	// No texturing, default zbuffer reading, no zbuffer writing, primary
	// gradient, alpha blending, no fogging - mostly for use in solid-colored
	// alpha-blended objects.
	static ShaderClass _PresetAlphaSolidShader;

	// Texturing, no zbuffer reading/writing, no gradients, no blending, alpha
	// testing, no fogging - mostly for "pure" alpha-tested 2D objects.
	static ShaderClass _PresetATest2DShader;

	// Texturing, default zbuffer reading and writing, no gradients, no
	// blending, alpha testing, no fogging - mostly for "pure" alpha-tested
	// sprite objects.
	static ShaderClass _PresetATestSpriteShader;

	// Texturing, no zbuffer reading/writing, no gradients, alpha blending AND
	// alpha testing, no fogging - mostly for alpha-tested and blended 2D
	// objects.
	static ShaderClass _PresetATestBlend2DShader;

	// Texturing, default zbuffer reading and writing, no gradients, alpha
	// blending AND alpha testing, no fogging - mostly for use in alpha-tested
	// and blended sprite objects.
	static ShaderClass _PresetATestBlendSpriteShader;

	// Texturing, no zbuffer reading/writing, no gradients, screen blending,
	// no fogging - mostly for screen-blended 2D objects.
	static ShaderClass _PresetScreen2DShader;

	// Texturing, default zbuffer reading, no zbuffer writing, no gradients,
	// screen blending, no fogging - mostly for use in screen-blended sprite
	// objects.
	static ShaderClass _PresetScreenSpriteShader;

	// Texturing, no zbuffer reading/writing, no gradients, multiplicative
	// blending, no fogging - mostly for multiplicatively blended 2D objects.
	static ShaderClass _PresetMultiplicative2DShader;

	// Texturing, default zbuffer reading, no zbuffer writing, no gradients,
	// multiplicative blending, no fogging - mostly for use in multiplicatively
	// blended sprite objects.
	static ShaderClass _PresetMultiplicativeSpriteShader;

protected:

	// Debug warning.
	void Report_Unable_To_Fog (const char *source);

	unsigned int ShaderBits;

	static bool ShaderDirty;
	static unsigned long CurrentShader;
};

inline void ShaderClass::Reset()
{
	ShaderBits=0;
	Set_Depth_Compare(PASS_LEQUAL);
	Set_Depth_Mask(DEPTH_WRITE_ENABLE);
	Set_Color_Mask(COLOR_WRITE_ENABLE);
	Set_Dst_Blend_Func(DSTBLEND_ZERO);
	Set_Fog_Func(FOG_DISABLE);
	Set_Primary_Gradient(GRADIENT_MODULATE);
	Set_Secondary_Gradient(SECONDARY_GRADIENT_DISABLE);
	Set_Src_Blend_Func(SRCBLEND_ONE);
	Set_Texturing(TEXTURING_DISABLE);
	Set_Alpha_Test(ALPHATEST_DISABLE);
	Set_Cull_Mode(CULL_MODE_ENABLE);
	Set_Post_Detail_Color_Func(DETAILCOLOR_DISABLE);
	Set_Post_Detail_Alpha_Func(DETAILALPHA_DISABLE);
	Set_NPatch_Enable(NPATCH_DISABLE);
}
