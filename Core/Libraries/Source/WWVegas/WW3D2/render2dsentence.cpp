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

 /***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/render2dsentence.cpp                   $*
 *                                                                                             *
 *                       $Author:: Patrick                  $*
 *                                                                                             *
 *								$Modtime:: 8/29/01 11:16a                                             $*
 *                                                                                             *
 *                    $Revision:: 13                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "render2dsentence.h"
#include "surfaceclass.h"
#include "texture.h"
#include "wwprofile.h"
#include "wwmemlog.h"
#include "dx8wrapper.h"


////////////////////////////////////////////////////////////////////////////////////
//	Local constants
////////////////////////////////////////////////////////////////////////////////////
#define no_TEST_PLACEMENT 1	 // Shows alignment markers for text.

#define TEXTURE_OFFSET 2
////////////////////////////////////////////////////////////////////////////////////
//
//	Render2DSentenceClass
//
////////////////////////////////////////////////////////////////////////////////////
Render2DSentenceClass::Render2DSentenceClass () :
	Font (nullptr),
	Location (0.0F,0.0F),
	Cursor (0.0F,0.0F),
	TextureOffset (0, 0),
	TextureStartX (0),
	CurSurface (nullptr),
	CurrTextureSize (0),
	MonoSpaced (false),
	IsClippedEnabled (false),
	ClipRect (0, 0, 0, 0),
	BaseLocation (0, 0),
	LockedPtr (nullptr),
	LockedStride (0),
	TextureSizeHint (0),
	WrapWidth (0),
	Centered (false),
	DrawExtents (0, 0, 0, 0),
	ParseHotKey( false ),
	useHardWordWrap( false)
{
	Shader = Render2DClass::Get_Default_Shader ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	~Render2DSentenceClass
//
////////////////////////////////////////////////////////////////////////////////////
Render2DSentenceClass::~Render2DSentenceClass ()
{
	REF_PTR_RELEASE (Font);
	Reset ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Font
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Font (FontCharsClass *font)
{
	Reset ();
	REF_PTR_SET (Font, font);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Reset_Polys
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset_Polys ()
{
	for (int index = 0; index < Renderers.Count (); index ++) {
		Renderers[index].Renderer->Reset ();
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Reset
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset ()
{
	//
	//	Make sure we unlock the current surface (if necessary)
	//
	if (LockedPtr != nullptr) {
		CurSurface->Unlock ();
		LockedPtr = nullptr;
	}

	//
	//	Release our hold on the current surface
	//
	REF_PTR_RELEASE (CurSurface);

	//
	//	Free each renderer
	//
	while (Renderers.Count () > 0) {
		delete Renderers[0].Renderer;
		Renderers.Delete(0);
	}

	Cursor.Set (0, 0);
	MonoSpaced = false;
	ParseHotKey = false;

	Release_Pending_Surfaces ();
	Reset_Sentence_Data ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Make_Additive
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Make_Additive ()
{
	Shader.Set_Dst_Blend_Func (ShaderClass::DSTBLEND_ONE);
	Shader.Set_Src_Blend_Func (ShaderClass::SRCBLEND_ONE);
	Shader.Set_Primary_Gradient (ShaderClass::GRADIENT_MODULATE);
	Shader.Set_Secondary_Gradient (ShaderClass::SECONDARY_GRADIENT_DISABLE);

	Set_Shader (Shader);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Make_Additive
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Shader (ShaderClass shader)
{
	Shader = shader;

	//
	//	Change each renderer's shader
	//
	for (int i = 0; i < Renderers.Count (); i ++) {
		ShaderClass *curr_shader = Renderers[i].Renderer->Get_Shader ();
		(*curr_shader) = Shader;
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Render
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Render ()
{
	//
	//	Build any textures that are pending
	//
	Build_Textures ();

	//
	//	Ask each renderer to draw its contents
	//
	for (int i = 0; i < Renderers.Count (); i ++) {
		Renderers[i].Renderer->Render ();
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Base_Location
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Base_Location (const Vector2 &loc)
{
	Vector2 dif		= loc - BaseLocation;
	BaseLocation	= loc;
	for (int i = 0; i < Renderers.Count (); i ++) {
		Renderers[i].Renderer->Move (dif);
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Location
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Location (const Vector2 &loc)
{
	Location	= loc;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Text_Extents
//
////////////////////////////////////////////////////////////////////////////////////
Vector2
Render2DSentenceClass::Get_Text_Extents (const WCHAR *text)
{
	Vector2 extent (0, Font->Get_Char_Height());

	while (*text) {
		WCHAR ch = *text++;

		if ( ch != (WCHAR)'\n' ) {
			extent.X += Font->Get_Char_Spacing( ch );
		}
	}

	return extent;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Formatted_Text_Extents
//
////////////////////////////////////////////////////////////////////////////////////
Vector2
Render2DSentenceClass::Get_Formatted_Text_Extents (const WCHAR *text)
{
	return Build_Sentence_Not_Centered(text, nullptr, nullptr, true);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Reset_Sentence_Data
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset_Sentence_Data ()
{
	//
	//	Release our hold on each texture used in the sentence
	//
	for (int index = 0; index < SentenceData.Count (); index ++) {
		REF_PTR_RELEASE (SentenceData[index].Surface);
	}

	if (SentenceData.Count()>0) {
		SentenceData.Delete_All ();
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Release_Pending_Surfaces
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Release_Pending_Surfaces ()
{
	//
	//	Release our hold on each pending surface
	//
	for (int index = 0; index < PendingSurfaces.Count (); index ++) {
		SurfaceClass *curr_surface = PendingSurfaces[index].Surface;
		REF_PTR_RELEASE (curr_surface);
	}

	if (PendingSurfaces.Count()>0) PendingSurfaces.Delete_All ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Textures
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Build_Textures ()
{
	WWMEMLOG(MEM_TEXTURE);

	//
	//	Make sure we unlock the current surface
	//
	if (LockedPtr != nullptr) {
		CurSurface->Unlock ();
		LockedPtr = nullptr;
	}

	//
	//	Release our hold on the current surface
	//
	REF_PTR_RELEASE (CurSurface);
	TextureOffset.Set (0, 0);
	TextureStartX = 0;

	//
	//	Convert all pending surfaces to textures
	//
	for (int index = 0; index < PendingSurfaces.Count (); index ++) {
		PendingSurfaceStruct &surface_info = PendingSurfaces[index];
		SurfaceClass *curr_surface = surface_info.Surface;

		//
		//	Get the dimensions of the surface
		//
		SurfaceClass::SurfaceDescription desc;
		curr_surface->Get_Description (desc);

		//
		//	Create the new texture
		//
		TextureClass *new_texture = W3DNEW TextureClass (desc.Width, desc.Width, WW3D_FORMAT_A4R4G4B4, MIP_LEVELS_1);
		SurfaceClass *texture_surface = new_texture->Get_Surface_Level ();

		new_texture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		new_texture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		new_texture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_NONE);
		new_texture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_NONE);
		new_texture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

		//
		//	Copy the contents of the texture from the surface
		//
		DX8Wrapper::_Copy_DX8_Rects (curr_surface->Peek_D3D_Surface (), nullptr, 0, texture_surface->Peek_D3D_Surface (), nullptr);
		REF_PTR_RELEASE (texture_surface);

		//
		//	Assign this texture to any renderers that need it
		//
		for (int renderer_index = 0; renderer_index < surface_info.Renderers.Count (); renderer_index ++) {
			Render2DClass *renderer = surface_info.Renderers[renderer_index];
			renderer->Set_Texture (new_texture);
		}

		//
		//	Release our hold on the objects
		//
		REF_PTR_RELEASE (new_texture);
		REF_PTR_RELEASE (curr_surface);
	}

	//
	//	Reset the list
	//
	if (PendingSurfaces.Count()>0) {
		PendingSurfaces.Delete_All ();
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Draw_Sentence
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Draw_Sentence (uint32 color)
{
	Render2DClass *curr_renderer	= nullptr;
	SurfaceClass *curr_surface		= nullptr;

	DrawExtents.Set (0, 0, 0, 0);

	int offset = 0;
	//
	//	Loop over all the parts of the sentence
	//
	for (int index = 0; index < SentenceData.Count (); index ++) {
		SentenceDataStruct &data = SentenceData[index];

		//
		//	Has the surface changed?
		//
		if (data.Surface != curr_surface) {
			curr_surface = data.Surface;

			//
			//	Try to find a renderer that uses the same "texture"
			//
			bool found = false;
			for (int renderer_index = 0; renderer_index < Renderers.Count (); renderer_index ++) {
				if (Renderers[renderer_index].Surface == curr_surface) {
					found = true;
					curr_renderer = Renderers[renderer_index].Renderer;
					break;
				}
			}

			//
			//	Create a new renderer if we couldn't find an appropriate one
			//
			if (found == false) {

				//
				//	Allocate a new renderer
				//
				curr_renderer = W3DNEW Render2DClass;
				curr_renderer->Set_Coordinate_Range (Render2DClass::Get_Screen_Resolution ());
				ShaderClass *curr_shader = curr_renderer->Get_Shader ();
				(*curr_shader) = Shader;

				//
				//	Add it to our list
				//
				RendererDataStruct render_info;
				render_info.Renderer	= curr_renderer;
				render_info.Surface	= curr_surface;
				Renderers.Add (render_info);

				//
				//	Now, add this renderer to the surface pending list
				//
				for (int surface_index = 0; surface_index < PendingSurfaces.Count (); surface_index ++) {
					PendingSurfaceStruct &surface_info = PendingSurfaces[surface_index];
					if (surface_info.Surface == curr_surface) {
						surface_info.Renderers.Add (curr_renderer);
					}
				}
			}
		}

		//
		//	Get the dimensions of the surface
		//
		SurfaceClass::SurfaceDescription desc;
		curr_surface->Get_Description (desc);

		//
		//	Add a quad that contains this sentence chunk
		//
		RectClass screen_rect	= data.ScreenRect;
		screen_rect					+= Location;
		RectClass uv_rect			= data.UVRect;

		//
		//	Clip the quad (as necessary)
		//
		bool add_quad = true;
		if (IsClippedEnabled) {

			//
			//	Check for completely clipped
			//
			if (	screen_rect.Right <= ClipRect.Left ||
					screen_rect.Bottom <= ClipRect.Top)
			{
				add_quad = false;
			} else {

				//
				//	Clip the polygons to the specified area
				//
				RectClass clipped_rect;
				clipped_rect.Left		= max (screen_rect.Left, ClipRect.Left);
				clipped_rect.Right	= min (screen_rect.Right, ClipRect.Right);
				clipped_rect.Top		= max (screen_rect.Top, ClipRect.Top);
				clipped_rect.Bottom	= min (screen_rect.Bottom, ClipRect.Bottom);

				//
				//	Clip the texture to the specified area
				//
				RectClass clipped_uv_rect;
				float percent				= ((clipped_rect.Left - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Left		= uv_rect.Left + (uv_rect.Width () * percent);

				percent						= ((clipped_rect.Right - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Right	= uv_rect.Left + (uv_rect.Width () * percent);

				percent						= ((clipped_rect.Top - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Top		= uv_rect.Top + (uv_rect.Height () * percent);

				percent						= ((clipped_rect.Bottom - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Bottom	= uv_rect.Top + (uv_rect.Height () * percent);

				//
				//	Use the clipped rectangles to render
				//
				screen_rect = clipped_rect;
				uv_rect		= clipped_uv_rect;

				if (screen_rect.Right <= screen_rect.Left ||
						screen_rect.Bottom <= screen_rect.Top)
				{
					add_quad = false;
				}
			}
		}

		if (add_quad) {
			//uv_rect.Bottom += 0.5f;
			uv_rect *=  1.0F / ((float)desc.Width);
#ifdef TEST_PLACEMENT
			screen_rect.Left += offset*3;
			screen_rect.Right += offset*3;
#endif
			offset++;
			curr_renderer->Add_Quad (screen_rect, uv_rect, color);

			//
			//	Add this rectangle to the total draw extents
			//
			if (DrawExtents.Width () == 0) {
				DrawExtents = screen_rect;
			} else {
				DrawExtents += screen_rect;
			}
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Record_Sentence_Chunk
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Record_Sentence_Chunk ()
{
	//
	//	Do we have anything to store?
	//
	int width = TextureOffset.I - TextureStartX;
	if (width > 0) {
		float char_height = Font->Get_Char_Height ();

		//
		//	Build a structure that contains enough information
		// to hold this portion of the sentence
		//
		SentenceDataStruct sentence_data;
		sentence_data.Surface = CurSurface;
		sentence_data.Surface->Add_Ref ();
		sentence_data.ScreenRect.Left		= Cursor.X;
		sentence_data.ScreenRect.Right	= Cursor.X + width;
		sentence_data.ScreenRect.Top		= Cursor.Y;
		sentence_data.ScreenRect.Bottom	= Cursor.Y + char_height;
		sentence_data.UVRect.Left			= TextureStartX;
		sentence_data.UVRect.Top			= TextureOffset.J;
		sentence_data.UVRect.Right			= TextureOffset.I;
		sentence_data.UVRect.Bottom		= TextureOffset.J + char_height;

		//
		//	Add this information to our list
		//
		SentenceData.Add (sentence_data);
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Allocate_New_Surface
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Allocate_New_Surface (const WCHAR *text, bool justCalcExtents)
{
	if (!justCalcExtents)
	{
		//
		//	Unlock the last surface (if necessary)
		//
		if (LockedPtr != nullptr) {
			CurSurface->Unlock ();
			LockedPtr = nullptr;
		}
	}

	//
	// Calculate the width of the text
	//
	int text_width = 0;
	for (int index = 0; text[index] != 0; index ++) {
		text_width += Font->Get_Char_Spacing (text[index]);
	}

	int char_height = Font->Get_Char_Height ();

	//
	//	Find the best texture size for the remaining text
	//
	CurrTextureSize = 256;
	int best_tex_mem_usage = 999999999;
	for (int pow2 = 6; pow2 <= 8; pow2 ++) {

		int size					= 1 << pow2;
		int row_count			= (text_width / size) + 1;
		int rows_per_texture	= size / (char_height + 1);

		//
		//	Can we even fit one character on this texture?
		//
		if (rows_per_texture > 0) {

			//
			//	How many textures (at this size) would it take to render
			// the remaining text?
			//
			int texture_count	= row_count / rows_per_texture;
			texture_count		= max (texture_count, 1);

			//
			//	Is this the best usage of texture memory we've found yet?
			//
			int texture_mem_usage = (texture_count * size * size);
			if (texture_mem_usage < best_tex_mem_usage) {
				CurrTextureSize		= size;
				best_tex_mem_usage	= texture_mem_usage;
			}
		}
	}

	//
	//	Use whichever is larger, the hint or the calculated size
	//
	CurrTextureSize = max (TextureSizeHint, CurrTextureSize);

	if (!justCalcExtents)
	{
		//
		//	Release our extra hold on the old surface
		//
		REF_PTR_RELEASE (CurSurface);

		//
		//	Create the new surface
		//
		CurSurface = NEW_REF (SurfaceClass, (CurrTextureSize, CurrTextureSize, WW3D_FORMAT_A4R4G4B4));
		WWASSERT (CurSurface != nullptr);
		CurSurface->Add_Ref ();

		//
		//	Add this surface to our list
		//
		PendingSurfaceStruct surface_info;
		surface_info.Surface = CurSurface;
		PendingSurfaces.Add (surface_info);
	}

	//
	//	Reset to the upper left corner
	//
	TextureOffset.Set (0, 0);
	TextureStartX = 0;
}

float FindStartingXPos( const WCHAR *text )
{

	return 1;
}
////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence_Centered
//
////////////////////////////////////////////////////////////////////////////////////
void	Render2DSentenceClass::Build_Sentence_Centered (const WCHAR *text, int *hkX, int *hkY)
{
	float char_height = Font->Get_Char_Height ();
	int		wordWidth = 0;
	int notCenteredHotkeyX = 0;
	int notCenteredHotkeyY = 0;
	Vector2 extent = Build_Sentence_Not_Centered(text,&notCenteredHotkeyX, &notCenteredHotkeyY, TRUE); //Get_Formatted_Text_Extents(text);

	//
	//	Start fresh
	//
	Reset_Sentence_Data ();
	Cursor.Set (0, 0);

	//
	//	Ensure we have a surface to start with
	//
	if (CurSurface == nullptr) {
		Allocate_New_Surface (text);
	}



	//
	//	Loop over all the characters in the string
	//
	bool end = false;
	const WCHAR *word;
	int word_width	= 0;
	int line_width	= 0;
	int charCount = 0;
	int wordCount = 0;
	int hotKeyPosX = 0;
	int hotKeyPosY = 0;
	bool calcHotKeyX = false;
	bool dontBlit = false;
	while (!end)
	{
		//
		// Re-init everything for the next line
		//
		word	= text;
		word_width	= 0;
		line_width	= 0;
		charCount = 0;
		wordCount = 0;
		//
		//first find the length of the line till we wrap
		//
		while ( 1 )
		{
			//
			// read a word
			//
			int charWidth = 0;
			while ((*word != 0) && (*word > L' ') && (*word != L'\n')) {
				if( ParseHotKey && (*word == L'&') && (*word+1 != 0) && (*word+1 > L' ') && (*word+1 != L'\n'))
				{
					int offset = 0;
					if (word_width != 0 )
					{
						const WCHAR *word_back = word;
						*word_back--;
						if (*word_back == L' ')
						{
							line_width -= word_width;
							offset =-1;
						}
					}
					*word++;
					calcHotKeyX = true;
				}

				charWidth = Font->Get_Char_Spacing (*word++);
				word_width += charWidth;
				wordCount++;

				if (WrapWidth > 0 && word_width >= WrapWidth && useHardWordWrap)
					break;
			}
			//
			// If this word is unworthy to be on the current line, decrement the space and break
			//
			if(WrapWidth > 0 && (line_width + word_width >= WrapWidth))
			{
				//
				//Take care of the case that the word is too big for the allocated space...
				//If that's the case, drop out and process the word anyway
				//
				if(charCount == 0)
				{
					charCount +=wordCount - 1;
					line_width += word_width - charWidth;
					if(*word == 0)
						end = true;
					break;
				}
				charCount--;
				break;
			}
			//
			// if we reached the end of the text, set the values and break, also set the end flag
			//
			if( *word == 0 )
			{
				charCount +=wordCount;
				line_width += word_width;
				end = true;
				break;
			}
			//
			// otherwise, increment the counts
			//
			charCount +=wordCount + 1;
			line_width += word_width;
			//
			// We were some a new line character break and process
			//
			if(*word != L' ')
				break;
			//
			// add the space to our width
			//
			word_width = Font->Get_Char_Spacing (*word++);
			wordCount = 0;
			line_width += word_width;
		}
		//
		// we now hold the length of the line and it's width lets set our cursor position to center it
		//
		Cursor.X = (int)((extent.X - line_width) / 2);
		if(Cursor.X < 0)
			Cursor.X = 0;
		if(calcHotKeyX)
		{
			calcHotKeyX = false;
			hotKeyPosX = Cursor.X + notCenteredHotkeyX;
		}

		for(int i = 0; i <= charCount; i++) {
			WCHAR ch = *text++;
			dontBlit = false;
			//
			//	Determine how much horizontal space this character requires
			//
			if(ParseHotKey && (ch == L'&') && (*text != 0) && (*text > L' ') && (*text != L'\n'))
			{
				ch = *text++;
				dontBlit = true;
			}
			float char_spacing = Font->Get_Char_Spacing (ch);

			bool exceeded_texture_width	= ((TextureOffset.I + char_spacing) >= CurrTextureSize);
			bool encountered_break_char	= (ch == L' ' || ch == L'\n' || ch == 0);

			//
			//	Do we need to record this portion of the sentence to its own chunk?
			//
			if (exceeded_texture_width || encountered_break_char) {
				Record_Sentence_Chunk ();

				//
				//	Adjust the positions
				//
				Cursor.X			+= (TextureOffset.I - TextureStartX);
				TextureStartX	= TextureOffset.I;

				//
				//	Adjust the output coordinates
				//
				if (ch == L' ') {
					Cursor.X += char_spacing;
				} else if ((ch == 0 )|| (ch == L'\n')) {
					break;
				}

				//
				//	Did the text extend past the edge of the texture?
				//
				if (exceeded_texture_width) {
					TextureStartX		= 0;
					TextureOffset.I	= TextureStartX;
					TextureOffset.J	+= char_height;

					//
					//	Did the text extent completely off the texture?
					//
					if ((TextureOffset.J + char_height) >= CurrTextureSize) {
						Allocate_New_Surface (text);
					}
				}
			}
			//
			//	Adjust the output coordinates
			//
			if (ch != L'\n' && ch != L' ') {

				//
				//	Ensure the surface is locked
				//
				if (LockedPtr == nullptr) {
					LockedPtr = (uint16 *)CurSurface->Lock (&LockedStride);
					WWASSERT (LockedPtr != nullptr);
				}

				//
				//	Check to ensure the text will fit on this texture
				//
				WWASSERT (((TextureOffset.I + char_spacing) < CurrTextureSize) && ((TextureOffset.J + char_height) < CurrTextureSize));

				//
				//	Blit the character to the surface
				//
				if(!dontBlit)
					Font->Blit_Char (ch, LockedPtr, LockedStride, TextureOffset.I, TextureOffset.J);

				if (dontBlit) {
					// we don't blit for a hot key character.  So add extra spacing.
					char_spacing += Font->Get_Extra_Overlap();
					// Brutal hack #27 Gamma - Bolded M's are just a problem.	jba.
					if (ch=='M') {
						char_spacing++;
					}
				}

				TextureOffset.I += char_spacing;
			}
		}
		//
		// reset our cursor and add a line of text to the cursor position
		//
		Cursor.X = 0;
		Cursor.Y += char_height;
		line_width = 0;
		}

		if(hkX)
			*hkX = hotKeyPosX;
		if(hkX)
			*hkY = hotKeyPosY;
}
////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence_NotCentered
//
////////////////////////////////////////////////////////////////////////////////////
Vector2	Render2DSentenceClass::Build_Sentence_Not_Centered (const WCHAR *text, int *hkX, int *hkY, bool justCalcExtents)
{
	Vector2 cursor = Cursor;
	int textureStartX = TextureStartX;
	float maxX = 0;

	int hotKeyPosX = 0;
	int hotKeyPosY = 0;
	bool calcHotKeyX = false;
	bool dontBlit = false;
	Vector2i textureOffset = TextureOffset;


	//
	//	Start fresh
	//
	if (!justCalcExtents)
	{
		Reset_Sentence_Data ();
	}
	Cursor.Set (0, 0);

	//
	//	Ensure we have a surface to start with
	//
	if (CurSurface == nullptr) {
		Allocate_New_Surface (text, justCalcExtents);
	}

	TextureOffset.Set (TEXTURE_OFFSET, 0);
	TextureStartX = TEXTURE_OFFSET;

	float char_height = Font->Get_Char_Height ();

	//
	//	Loop over all the characters in the string
	//
	while (text != nullptr) {
		WCHAR ch = *text++;
		dontBlit = false;
		//
		//	Determine how much horizontal space this character requires
		//
		if(ParseHotKey && (ch == L'&') && (*text != 0) && (*text > L' ') && (*text != L'\n'))
		{
				hotKeyPosY = Cursor.Y;
			if (calcHotKeyX)
				hotKeyPosX = 0;
			else
				hotKeyPosX = Cursor.X + TextureOffset.I -TextureStartX;//TextureOffset.I;

			ch = *text++;
			dontBlit = true;
		}
		float char_spacing = Font->Get_Char_Spacing (ch);

		bool exceeded_texture_width	= ((TextureOffset.I + char_spacing) >= CurrTextureSize);
		bool encountered_break_char	= (ch == L' ' || ch == L'\n' || ch == 0);
		bool wordBiggerThenLine = ((useHardWordWrap) && ( WrapWidth != 0 ) &&((Cursor.X + TextureOffset.I -TextureStartX + char_spacing) >= WrapWidth));
		//
		//	Do we need to record this portion of the sentence to its own chunk?
		//
		if (exceeded_texture_width || encountered_break_char|| wordBiggerThenLine) {
			if (!justCalcExtents)
			{
				Record_Sentence_Chunk ();
			}

			//
			//	Adjust the positions
			//
			Cursor.X			+= (TextureOffset.I - TextureStartX);
			maxX = max(maxX, Cursor.X);
			TextureStartX	= TextureOffset.I;

			//
			//	Adjust the output coordinates
			//
			if (ch == L' ') {
				//Cursor.X += char_spacing;
				//maxX = max(maxX, Cursor.X);

				//
				// Check to see if we need to wrap on this word-break
				//
				if (WrapWidth > 0) {

					//
					//	Find the length of the next word
					//
					const WCHAR *word	= text;
					float word_width	= char_spacing;
					while ((*word != 0) && (*word > L' ')) {
						if(ParseHotKey && (*word == L'&') && (*word+1 != 0) && (*word+1 > L' ') && (*word+1 != L'\n'))
							*word++;
						word_width += Font->Get_Char_Spacing (*word++);
					}

					//
					//	Should we wrap the next word?
					//
					if ((Cursor.X + word_width) >= WrapWidth) {
						Cursor.X = 0;
						Cursor.Y += char_height;
						calcHotKeyX = true;
					}
				}

			} else if (ch == L'\n') {
				Cursor.X = 0;
				Cursor.Y += char_height;
			} else if (ch == 0) {
				break;
			} else if (wordBiggerThenLine){ // we've entered this loop because we're greater then the wordwrap so we need to force a wordwrap
				Cursor.X = 0;
				Cursor.Y += char_height;
			}


			//
			//	Did the text extend past the edge of the texture?
			//
			if (exceeded_texture_width) {
				TextureStartX		= TEXTURE_OFFSET;
				TextureOffset.I	= TextureStartX;
				TextureOffset.J	+= char_height;

				//
				//	Did the text extent completely off the texture?
				//
				if ((TextureOffset.J + char_height) >= CurrTextureSize) {
					Allocate_New_Surface (text, justCalcExtents);
				}
			}
		}

		if (ch != L'\n' ) {

			//
			//	Ensure the surface is locked
			//
			if (!justCalcExtents)
			{
				if (LockedPtr == nullptr) {
					LockedPtr = (uint16 *)CurSurface->Lock (&LockedStride);
					WWASSERT (LockedPtr != nullptr);
				}
			}

			//
			//	Check to ensure the text will fit on this texture
			//
			WWASSERT (((TextureOffset.I + char_spacing) < CurrTextureSize) && ((TextureOffset.J + char_height) < CurrTextureSize));

			//
			//	Blit the character to the surface
			//
			if (!justCalcExtents && !dontBlit )
			{
				Font->Blit_Char (ch, LockedPtr, LockedStride, TextureOffset.I, TextureOffset.J);
			}
			TextureOffset.I += char_spacing;
		}
	}

	Vector2 extent;
	extent.X = maxX + Font->Get_Extra_Overlap();
	extent.Y = Cursor.Y + char_height;

	Cursor = cursor;
	TextureOffset = textureOffset;
	TextureStartX = textureStartX;

	if(hkX)
		*hkX = hotKeyPosX;
	if(hkX)
		*hkY = hotKeyPosY;

	return extent;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Build_Sentence (const WCHAR *text, int *hkX, int *hkY)
{
	if (text == nullptr) {
		return ;
	}

	if (Font == nullptr)
		return;

	if(Centered && (WrapWidth > 0 || wcschr(text,L'\n')))
		Build_Sentence_Centered(text, hkX, hkY);
	else
		Build_Sentence_Not_Centered(text, hkX, hkY);

}


////////////////////////////////////////////////////////////////////////////////////
//
//	FontCharsClass
//
////////////////////////////////////////////////////////////////////////////////////
FontCharsClass::FontCharsClass () :
#ifdef _WIN32
	OldGDIFont(	nullptr ),
	OldGDIBitmap( nullptr ),
	GDIFont( nullptr ),
	GDIBitmap( nullptr ),
	GDIBitmapBits ( nullptr ),
	MemDC( nullptr ),
#endif
#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
	FTLibrary( nullptr ),
	FTFace( nullptr ),
#endif
	CurrPixelOffset( 0 ),
	PointSize( 0 ),
	CharHeight( 0 ),
	UnicodeCharArray( nullptr ),
	FirstUnicodeChar( 0xFFFF ),
	LastUnicodeChar( 0 ),
	IsBold (false)
{
	AlternateUnicodeFont = nullptr;
	::memset( ASCIICharArray, 0, sizeof (ASCIICharArray) );
}


////////////////////////////////////////////////////////////////////////////////////
//
//	~FontCharsClass
//
////////////////////////////////////////////////////////////////////////////////////
FontCharsClass::~FontCharsClass ()
{
	while ( BufferList.Count() ) {
		delete BufferList[0];
		BufferList.Delete(0);
	}

#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
	Free_Freetype_Font();
#else
	Free_GDI_Font();
#endif
	Free_Character_Arrays();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Data
//
////////////////////////////////////////////////////////////////////////////////////
const FontCharsClassCharDataStruct *
FontCharsClass::Get_Char_Data (WCHAR ch)
{
	// GeneralsX @bugfix BenderAI/felipebraz 14/03/2026 Normalize to 16-bit code units to match legacy font array indexing on non-Windows wchar_t
	const uint16 normalized_char = static_cast<uint16>(ch);
	const WCHAR glyph = static_cast<WCHAR>(normalized_char);

	const FontCharsClassCharDataStruct *retval = nullptr;

	if ( normalized_char < 256 )
	{
		retval = ASCIICharArray[normalized_char];
	}
 	else if ( AlternateUnicodeFont && this != AlternateUnicodeFont )
	{
		return AlternateUnicodeFont->Get_Char_Data( glyph );
	}
	else
	{
		Grow_Unicode_Array( glyph );
		retval = UnicodeCharArray[normalized_char - FirstUnicodeChar];
	}

	//
	//	If the character wasn't found, then add it to our list
	//  TheSuperHackers @feature FreeType port 10/02/2026 Dispatch to FreeType on Linux
	//
	if ( retval == nullptr ) {
#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
		retval = Store_Freetype_Char( glyph );
#else
		retval = Store_GDI_Char( glyph );
#endif
	}

	WWASSERT( retval->Value == glyph );
	return retval;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Width
//
////////////////////////////////////////////////////////////////////////////////////
int
FontCharsClass::Get_Char_Width (WCHAR ch)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr ) {
		return data->Width;
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Spacing
//
////////////////////////////////////////////////////////////////////////////////////
int
FontCharsClass::Get_Char_Spacing (WCHAR ch)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr ) {
		if ( data->Width != 0 ) {
			return data->Width - PixelOverlap - CharOverhang;
		}
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Blit_Char
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Blit_Char (WCHAR ch, uint16 *dest_ptr, int dest_stride, int x, int y)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr && data->Width != 0 ) {

		//
		//	Setup the src and destination pointers
		//
		int dest_inc		= (dest_stride >> 1);
		uint16 *src_ptr	= data->Buffer;
		dest_ptr				+= (dest_inc * y) + x;

		//
		//	Simply copy the data from the src buffer to the destination
		//
		for ( int row = 0; row < CharHeight; row ++ ) {
			for ( int col = 0; col < data->Width; col ++ ) {
				uint16 curData = *src_ptr;
				if (col<PixelOverlap) {
					curData |= dest_ptr[col];
				}
				dest_ptr[col] = curData;
				src_ptr++;
			}
			dest_ptr	+= dest_inc;
		}
	}
}


#ifdef _WIN32

////////////////////////////////////////////////////////////////////////////////////
//
//	Store_GDI_Char
//
// GeneralsX @build fbraz 11/02/2026 - Windows-only GDI text rendering
////////////////////////////////////////////////////////////////////////////////////
const FontCharsClassCharDataStruct *
FontCharsClass::Store_GDI_Char (WCHAR ch)
{
	int width	= PointSize * 2;
	int height	= PointSize * 2;

	//
	//	Draw the character into the memory DC
	//
	RECT rect = { 0, 0, width, height };
	int xOrigin = 0;
	if (ch == 'W') {
		xOrigin = 1;
	}
	::ExtTextOutW( MemDC, xOrigin, 0, ETO_OPAQUE, &rect, &ch, 1, nullptr);

	//
	//	Get the size of the character we just drew
	//
	SIZE char_size = { 0 };
	::GetTextExtentPoint32W( MemDC, &ch, 1, &char_size );
	char_size.cx += PixelOverlap + xOrigin;
	//
	//	Get a pointer to the surface that this character should use
	//
	Update_Current_Buffer( char_size.cx );
	uint16* curr_buffer_p = BufferList[BufferList.Count () - 1]->Buffer;
	curr_buffer_p += CurrPixelOffset;

	//
	//	Copy the BMP contents to the buffer
	//
	int stride = (((width * 3) + 3) & ~3);
	for (int row = 0; row < char_size.cy; row ++) {

		//
		//	Compute the indices into the BMP and surface
		//
		int index = (row * stride);

		//
		//	Loop over each column
		//
		for (int col = 0; col < char_size.cx; col ++) {

			//
			//	Get the pixel color at this location
			//
			uint8 pixel_value = GDIBitmapBits[index];
			index += 3;
#ifdef TEST_PLACEMENT
 			if (row==CharHeight-1&&col==0) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-2&&col==1) {
 				pixel_value = 0xff;
 			}
 			if (row==0&&col==0) {
 				pixel_value = 0xff;
 			}
 			if (row==1&&col==1) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-1&&col==char_size.cx-1-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-2&&col==char_size.cx-2-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==0&&col==char_size.cx-1-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==1&&col==char_size.cx-2-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (pixel_value == 0x00) {
 				pixel_value = 0x40;
 			}
#endif

			uint16 pixel_color = 0;
			if (pixel_value != 0) {
				pixel_color = 0x0FFF;
			}

			//
			//	Convert the pixel intensity from 8bit to 4bit and
			// store it in our buffer
			//
			uint8 alpha_value	= ((pixel_value >> 4) & 0xF);
			*curr_buffer_p++	= pixel_color | (alpha_value << 12);
		}
	}

	//
	//	Save information about this character in our list
	//
	FontCharsClassCharDataStruct *char_data	= W3DNEW FontCharsClassCharDataStruct;
	char_data->Value				= ch;
	char_data->Width				= char_size.cx;
	char_data->Buffer				= BufferList[BufferList.Count () - 1]->Buffer + CurrPixelOffset;

	//
	//	Insert this character into our array
	//
	if ( ch < 256 ) {
		ASCIICharArray[ch] = char_data;
	} else {
		UnicodeCharArray[ch - FirstUnicodeChar] = char_data;
	}

	//
	//	Advance the character position
	//
	CurrPixelOffset += ((char_size.cx+PixelOverlap) * CharHeight);

	//
	//	Return the index of the entry we just added
	//
	return char_data;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Create_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Create_GDI_Font (const char *font_name)
{
	HDC screen_dc = ::GetDC ((HWND)WW3D::Get_Window());

	const char *fontToUseForGenerals = "Arial";
	bool doingGenerals = false;
	if (strcmp(font_name, "Generals")==0) {
		font_name = fontToUseForGenerals;
		doingGenerals = true;
	}

	//
	//	Calculate the height of the font in logical units
	//
	const int dotsPerInch = 96; // always use 96.	jba.
	int font_height = -MulDiv (PointSize, dotsPerInch, 72);

	int fontWidth = 0; // use font default.
	if (doingGenerals) {
		//fontWidth = -font_height*0.35f; //2 pixels tighter.
		fontWidth = -font_height*0.40f; // one pixel tighter
	}
	PixelOverlap = (-font_height)/8;

	// Sanity check in case of perversion. :)
	if (PixelOverlap<0) PixelOverlap = 0;
	if (PixelOverlap>4) PixelOverlap = 4;
	//
	//	Create the Windows font
	//
	DWORD bold		= IsBold ? FW_BOLD : FW_NORMAL;
	DWORD italic	= 0;
	GDIFont			= ::CreateFont (font_height, fontWidth, 0, 0, bold, italic,
								FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
								CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
								VARIABLE_PITCH, font_name);

	//
	// Set-up the fields of the BITMAPINFOHEADER
	//	Note: Top-down DIBs use negative height in Win32.
	//
	BITMAPINFOHEADER bitmap_info = { 0 };
	bitmap_info.biSize				= sizeof (BITMAPINFOHEADER);
	bitmap_info.biWidth				= PointSize * 2;
	bitmap_info.biHeight				= -(PointSize * 2);
	bitmap_info.biPlanes				= 1;
	bitmap_info.biBitCount			= 24;
	bitmap_info.biCompression		= BI_RGB;
	bitmap_info.biSizeImage			= ((PointSize * PointSize * 4) * 3);
	bitmap_info.biXPelsPerMeter	= 0;
	bitmap_info.biYPelsPerMeter	= 0;
	bitmap_info.biClrUsed			= 0;
	bitmap_info.biClrImportant		= 0;

	//
	// Create a bitmap that we can access the bits directly of
	//
	GDIBitmap	= ::CreateDIBSection (	screen_dc,
													(const BITMAPINFO *)&bitmap_info,
													DIB_RGB_COLORS,
													(void **)&GDIBitmapBits,
													nullptr,
													0L);

	//
	//	Create a device context we can select the font and bitmap into
	//
	MemDC = ::CreateCompatibleDC (screen_dc);

	//
	// Release our temporary screen DC
	//
	::ReleaseDC ((HWND)WW3D::Get_Window(), screen_dc);

	//
	//	Now select the BMP and font into the DC
	//
	OldGDIBitmap	= (HBITMAP)::SelectObject (MemDC, GDIBitmap);
	OldGDIFont		= (HFONT)::SelectObject (MemDC, GDIFont);
	::SetBkColor (MemDC, RGB (0, 0, 0));
	::SetTextColor (MemDC, RGB (255, 255, 255));

	//
	//	Lookup the pixel height of the font
	//
	TEXTMETRIC text_metric = { 0 };
	::GetTextMetrics (MemDC, &text_metric);
	CharHeight = text_metric.tmHeight;
	CharAscent = text_metric.tmAscent;
	CharOverhang = text_metric.tmOverhang;
	if (doingGenerals) {
		CharOverhang = 0;
	}

	return GDIFont != nullptr && GDIBitmap != nullptr;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Free_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Free_GDI_Font ()
{
	//
	//	Select the old font back into the DC and delete
	// our font object
	//
	if ( GDIFont != nullptr ) {
		::SelectObject( MemDC, OldGDIFont );
		::DeleteObject( GDIFont );
		GDIFont = nullptr;
	}

	//
	//	Select the old bitmap back into the DC and delete
	// our bitmap object
	//
	if ( GDIBitmap != nullptr ) {
		::SelectObject( MemDC, OldGDIBitmap );
		::DeleteObject( GDIBitmap );
		GDIBitmap = nullptr;
	}

	//
	//	Delete our memory DC
	//
	if ( MemDC != nullptr ) {
		::DeleteDC( MemDC );
		MemDC = nullptr;
	}
}

#endif // _WIN32

////////////////////////////////////////////////////////////////////////////////////
//
//	Update_Current_Buffer (Platform-independent text buffer management)
//
// GeneralsX @build fbraz 11/02/2026 - Used by both Windows GDI and Linux FreeType
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Update_Current_Buffer (int char_width)
{
	//
	//	Check to see if we need to allocate a new buffer
	//
	bool needs_new_buffer = (BufferList.Count () == 0);
	if (needs_new_buffer == false) {

		//
		//	Would we extend past this buffer?
		//
		if ( (CurrPixelOffset + (char_width * CharHeight)) > CHAR_BUFFER_LEN ) {
			needs_new_buffer = true;
		}
	}

	//
	//	Do we need to create a new surface?
	//
	if (needs_new_buffer)
	{
		FontCharsBuffer* new_buffer = W3DNEW FontCharsBuffer;
		BufferList.Add( new_buffer );
		CurrPixelOffset = 0;
	}

	return ;
}

#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE

#include <cctype>
#include <cstdio>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////////
//
//	Locate_Font_FontConfig (iOS)
//
// iOS has no fontconfig and no user-accessible system font files. Fonts are
// resolved from a "fonts" directory below the current working directory (the
// app's Documents folder, where game data also lives). The requested face name
// is normalized (lowercase, spaces stripped) and tried as <name>.ttf/.otf/.ttc;
// arial.ttf serves as the universal fallback since the game UI is Arial-based.
////////////////////////////////////////////////////////////////////////////////////
const char *
FontCharsClass::Locate_Font_FontConfig (const char *font_name)
{
	char normalized[128];
	int n = 0;
	for ( const char *p = font_name; *p != '\0' && n < (int)sizeof(normalized) - 1; ++p ) {
		if ( *p == ' ' ) {
			continue;
		}
		normalized[n++] = (char)tolower( (unsigned char)*p );
	}
	normalized[n] = '\0';

	static const char *extensions[] = { ".ttf", ".otf", ".ttc" };
	char candidate[256];
	for ( size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i ) {
		snprintf( candidate, sizeof(candidate), "fonts/%s%s", normalized, extensions[i] );
		if ( access( candidate, R_OK ) == 0 ) {
			FreetypeFontPath = candidate;
			return FreetypeFontPath;
		}
	}

	// Fall back to the Arial-equivalent face shipped with the app
	if ( access( "fonts/arial.ttf", R_OK ) == 0 ) {
		FreetypeFontPath = "fonts/arial.ttf";
		return FreetypeFontPath;
	}

	return nullptr;
}

#else // !TARGET_OS_IPHONE

////////////////////////////////////////////////////////////////////////////////////
//
//	Locate_Font_FontConfig
//
// TheSuperHackers @feature FreeType port 10/02/2026 Locate system font using Fontconfig
////////////////////////////////////////////////////////////////////////////////////
const char *
FontCharsClass::Locate_Font_FontConfig (const char *font_name)
{
	//
	//	Initialize Fontconfig library
	//
	FcConfig *config = FcInitLoadConfigAndFonts();
	if ( config == nullptr ) {
		return nullptr;
	}

	//
	//	Create a pattern for the requested font
	//
	FcPattern *pattern = FcNameParse( (const FcChar8*)font_name );
	if ( pattern == nullptr ) {
		FcConfigDestroy( config );
		return nullptr;
	}

	//
	//	Configure the pattern
	//
	FcConfigSubstitute( config, pattern, FcMatchPattern );
	FcDefaultSubstitute( pattern );

	//
	//	Find the best match
	//
	FcResult result = FcResultNoMatch;
	FcPattern *font = FcFontMatch( config, pattern, &result );

	const char *font_path = nullptr;
	if ( font != nullptr && result == FcResultMatch ) {
		//
		//	Extract the font file path
		//
		FcChar8 *file_path = nullptr;
		if ( FcPatternGetString( font, FC_FILE, 0, &file_path ) == FcResultMatch ) {
			FreetypeFontPath = (const char*)file_path;
			font_path = FreetypeFontPath;
		}

		FcPatternDestroy( font );
	}

	FcPatternDestroy( pattern );
	FcConfigDestroy( config );

	return font_path;
}

#endif // !TARGET_OS_IPHONE


////////////////////////////////////////////////////////////////////////////////////
//
//	Create_Freetype_Font
//
// GeneralsX @build fbraz 11/02/2026 BenderAI -  Initialize FreeType font (fighter19 pattern)
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Create_Freetype_Font (const char *font_name)
{
	//
	//	Initialize FreeType library
	//
	FT_Error error = FT_Init_FreeType( &FTLibrary );
	if ( error != 0 ) {
		return false;
	}

	//
	//	Handle "Generals" font mapping to Arial
	//
	bool doingGenerals = false;
	if ( strcmp( font_name, "Generals" ) == 0 ) {
		font_name = "Arial";
		doingGenerals = true;
	}

	//
	//	Calculate font height in pixels (96 DPI standard)
	//
	const int dotsPerInch = 96;
	int font_height = FT_MulDiv( PointSize, dotsPerInch, 72 );

	//
	//	Locate the font file using Fontconfig
	//
	const char *font_path = Locate_Font_FontConfig( font_name );
	if ( font_path == nullptr ) {
		FT_Done_FreeType( FTLibrary );
		FTLibrary = nullptr;
		return false;
	}

	//
	//	Load the font face
	//
	error = FT_New_Face( FTLibrary, font_path, 0, &FTFace );
	if ( error != 0 ) {
		FT_Done_FreeType( FTLibrary );
		FTLibrary = nullptr;
		return false;
	}

	//
	//	Set the font size (using pixel sizes for simplicity)
	//
	error = FT_Set_Pixel_Sizes( FTFace, 0, font_height );
	if ( error != 0 ) {
		FT_Done_Face( FTFace );
		FT_Done_FreeType( FTLibrary );
		FTFace = nullptr;
		FTLibrary = nullptr;
		return false;
	}

	//
	//	Calculate font metrics (Wine-compatible, same as fighter19)
	//
	if ( FT_IS_SCALABLE( FTFace ) ) {
		CharAscent = FT_MulFix( FTFace->ascender, FTFace->size->metrics.y_scale ) >> 6;
		int descent = -FT_MulFix( FTFace->descender, FTFace->size->metrics.y_scale ) >> 6;
		CharHeight = CharAscent + descent;
		CharOverhang = 0;
	} else {
		//
		//	Non-scalable fonts not supported
		//
		FT_Done_Face( FTFace );
		FT_Done_FreeType( FTLibrary );
		FTFace = nullptr;
		FTLibrary = nullptr;
		return false;
	}

	if ( doingGenerals ) {
		CharOverhang = 0;
	}

	//
	//	Calculate pixel overlap (same logic as GDI version)
	//
	PixelOverlap = (-font_height) / 8;
	if ( PixelOverlap < 0 ) PixelOverlap = 0;
	if ( PixelOverlap > 4 ) PixelOverlap = 4;

	return true;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Store_Freetype_Char
//
// GeneralsX @build fbraz 11/02/2026 BenderAI - FreeType character rendering (fighter19 pattern)
////////////////////////////////////////////////////////////////////////////////////
const FontCharsClassCharDataStruct *
FontCharsClass::Store_Freetype_Char (WCHAR ch)
{
	//
	//	Get the glyph index for the character
	//
	FT_UInt glyph_index = FT_Get_Char_Index( FTFace, ch );

	//
	//	Load the glyph (without rendering yet)
	//
	FT_Error error = FT_Load_Glyph( FTFace, glyph_index, FT_LOAD_DEFAULT );
	if ( error != 0 ) {
		return nullptr;
	}

	//
	//	Convert to an anti-aliased bitmap
	//
	error = FT_Render_Glyph( FTFace->glyph, FT_RENDER_MODE_NORMAL );
	if ( error != 0 ) {
		return nullptr;
	}

	FT_GlyphSlot glyph = FTFace->glyph;

	//
	//	Calculate X position (special case for 'W')
	//
	int x_pos = 0;
	if ( ch == 'W' ) {
		x_pos = 1;
	}

	//
	//	Calculate character width (advance + overlap)
	//
	unsigned int char_width = glyph->advance.x >> 6;

	//
	//	Sometimes bitmap is wider than advancement (fix it)
	//
	if ( char_width < glyph->bitmap.width + glyph->bitmap_left ) {
		char_width = glyph->bitmap.width + glyph->bitmap_left;
	}
	char_width += PixelOverlap + x_pos;

	//
	//	Get a pointer to the buffer for this character (allocates if needed)
	//
	Update_Current_Buffer( char_width );
	uint16 *curr_buffer_p = BufferList[BufferList.Count() - 1]->Buffer;
	curr_buffer_p += CurrPixelOffset;

	//
	//	Calculate bitmap offsets (match GDI baseline)
	//
	int x_offset = glyph->bitmap_left;
	int descent = CharHeight - CharAscent;
	int y_offset = (CharHeight - glyph->bitmap_top) - descent;

	//
	//	Prevent invalid buffer access
	//
	if ( x_offset < 0 ) x_offset = 0;
	if ( y_offset < 0 ) y_offset = 0;

	//
	//	Copy FreeType bitmap to our buffer (convert 8-bit gray → 16-bit format)
	//
	for ( unsigned int row = 0; row < glyph->bitmap.rows; row++ ) {
		int src_index = row * glyph->bitmap.pitch;
		int dst_index = (y_offset + row) * char_width;

		for ( unsigned int col = 0; col < glyph->bitmap.width; col++ ) {
			//
			//	Get 8-bit grayscale pixel
			//
			uint8 pixel_value = glyph->bitmap.buffer[src_index + col];

			uint16 pixel_color = 0;
			if ( pixel_value != 0 ) {
				pixel_color = 0x0FFF;	// White (12-bit RGB444)
			}

			//
			//	Format: 4-bit alpha (top nibble) + 12-bit color (bottom bits)
			//	SAME FORMAT AS GDI IMPLEMENTATION
			//
			uint8 alpha_value = (pixel_value >> 4) & 0xF;
			curr_buffer_p[dst_index + x_offset + col] = pixel_color | (alpha_value << 12);
		}
	}

	//
	//	Save information about this character
	//
	FontCharsClassCharDataStruct *char_data = W3DNEW FontCharsClassCharDataStruct;
	char_data->Value = ch;
	char_data->Width = (short)char_width;
	char_data->Buffer = BufferList[BufferList.Count() - 1]->Buffer + CurrPixelOffset;

	//
	//	Insert into character array (ASCII or Unicode)
	//
	if ( ch < 256 ) {
		ASCIICharArray[ch] = char_data;
	} else {
		UnicodeCharArray[ch - FirstUnicodeChar] = char_data;
	}

	//
	//	Advance the pixel offset for next character
	//
	CurrPixelOffset += (char_width * CharHeight);

	//
	//	Return the character data
	//
	return char_data;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Free_Freetype_Font
//
// GeneralsX @build fbraz 11/02/2026 BenderAI - Cleanup FreeType resources
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Free_Freetype_Font (void)
{
	//
	//	Free the FreeType face
	//
	if ( FTFace != nullptr ) {
		FT_Done_Face( FTFace );
		FTFace = nullptr;
	}

	//
	//	Free the FreeType library
	//
	if ( FTLibrary != nullptr ) {
		FT_Done_FreeType( FTLibrary );
		FTLibrary = nullptr;
	}
}

#endif // SAGE_USE_FREETYPE && !_WIN32


////////////////////////////////////////////////////////////////////////////////////
//
//	Initialize_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Initialize_GDI_Font (const char *font_name, int point_size, bool is_bold)
{
	//
	//	Build a unique name from the font name and its size
	//
	Name.Format ("%s%d", font_name, point_size);

	//
	//	Remember these settings
	//
	GDIFontName	= font_name;
	PointSize	= point_size;
	IsBold		= is_bold;

	//
	//	Create the actual font object (platform-specific)
	//  TheSuperHackers @feature FreeType port 10/02/2026 Dispatch to FreeType on Linux
	//
#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
	return Create_Freetype_Font (font_name);
#else
	return Create_GDI_Font (font_name);
#endif
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Is_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Is_Font (const char *font_name, int point_size, bool is_bold)
{
	bool retval = false;

	//
	//	Check to see if both the name and height matches...
	//
	if (	(GDIFontName.Compare_No_Case (font_name) == 0) &&
			(point_size == PointSize) &&
			(is_bold == IsBold))
	{
		retval = true;
	}

	return retval;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Grow_Unicode_Array
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Grow_Unicode_Array (WCHAR ch)
{
	//
	//	Don't do anything if character is in the ASCII range
	//
	if ( ch < 256 ) {
		return ;
	}

	//
	//	Don't do anything if character is in the currently allocated range
	//
	if ( ch >= FirstUnicodeChar && ch <= LastUnicodeChar ) {
		return ;
	}

	uint16 first_index	= min( FirstUnicodeChar, static_cast<uint16>(ch) );
	uint16 last_index		= max( LastUnicodeChar, static_cast<uint16>(ch) );
	uint16 count			= (last_index - first_index) + 1;

	//
	//	Allocate enough memory to hold the new cells
	//
	FontCharsClassCharDataStruct **new_array = W3DNEWARRAY FontCharsClassCharDataStruct *[count];
	::memset (new_array, 0, sizeof (FontCharsClassCharDataStruct *) * count);

	//
	//	Copy the contents of the old array into the new array
	//
	if ( UnicodeCharArray != nullptr ) {
		int start_offset	= (FirstUnicodeChar - first_index);
		int old_count		= (LastUnicodeChar - FirstUnicodeChar) + 1;
		::memcpy (&new_array[start_offset], UnicodeCharArray, sizeof (FontCharsClassCharDataStruct *) * old_count);

		//
		//	Delete the old array
		//
		delete [] UnicodeCharArray;
		UnicodeCharArray = nullptr;
	}

	FirstUnicodeChar	= first_index;
	LastUnicodeChar	= last_index;
	UnicodeCharArray	= new_array;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Free_Character_Arrays
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Free_Character_Arrays ()
{
	if ( UnicodeCharArray != nullptr ) {

		int count = (LastUnicodeChar - FirstUnicodeChar) + 1;

		//
		//	Delete each member of the unicode array
		//
		for (int index = 0; index < count; index ++) {
			delete UnicodeCharArray[index];
			UnicodeCharArray[index] = nullptr;
		}

		//
		//	Delete the array itself
		//
		delete [] UnicodeCharArray;
		UnicodeCharArray = nullptr;
	}

	//
	//	Delete each member of the ascii character array
	//
	for (int index = 0; index < 256; index ++) {
		delete ASCIICharArray[index];
		ASCIICharArray[index] = nullptr;
	}
}
