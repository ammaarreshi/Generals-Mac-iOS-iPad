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

// FILE: W3DGameFont.cpp //////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    RTS3
//
// File name:  W3DGameFont.cpp
//
// Created:    Colin Day, June 2001
//
// Desc:       W3D implementation for managing font definitions
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Common/Debug.h"
#include "W3DDevice/GameClient/W3DGameFont.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/render2dsentence.h"
#include "GameClient/GlobalLanguage.h"

namespace
{
// GeneralsX @bugfix GitHubCopilot 20/05/2026 Resolve a usable Unicode fallback font on macOS/Linux when localized font names are unavailable.
// GeneralsX @bugfix GitHubCopilot 29/05/2026 Prevent circular Unicode fallback when the localized unicode family equals the base font family.
FontCharsClass *LoadUnicodeFallbackFont(Int size, Bool bold, const char *base_name)
{
	const char *preferred_name = nullptr;

	if (TheGlobalLanguageData && TheGlobalLanguageData->m_unicodeFontName.isNotEmpty()) {
		preferred_name = TheGlobalLanguageData->m_unicodeFontName.str();
	}

	// Build candidate list: the localized preferred name first (may be a limited-coverage font like "Arial"),
	// then known-good Unicode fonts. We iterate all candidates and pick the first that loads AND has a
	// different family from the base font. This avoids returning a font that lacks Cyrillic coverage
	// (e.g., Arial on macOS) when a better universal font like "Arial Unicode MS" is available.
	static const char *kFallbackUnicodeFonts[] = {
		"Arial Unicode MS",
		"Arial Unicode",
		"Arial",
		"Helvetica Neue",
		"Helvetica",
		"Noto Sans",
		"Noto Sans CJK SC",
		"Noto Sans CJK JP",
		"DejaVu Sans"
	};

	for (const char *font_name : kFallbackUnicodeFonts) {
		if (base_name != nullptr && strcmp(font_name, base_name) == 0)
			continue;
		if (preferred_name != nullptr && strcmp(font_name, preferred_name) == 0)
			continue;

		FontCharsClass *font = WW3DAssetManager::Get_Instance()->Get_FontChars(font_name, size, bold);
		if (font != nullptr) {
			return font;
		}
	}

	// Now try the localized preferred name as a last resort (it may load on some platforms)
	if (preferred_name != nullptr && (base_name == nullptr || strcmp(preferred_name, base_name) != 0)) {
		FontCharsClass *font = WW3DAssetManager::Get_Instance()->Get_FontChars(preferred_name, size, bold);
		if (font != nullptr) {
			return font;
		}
	}

	return nullptr;
}
}

// DEFINES ////////////////////////////////////////////////////////////////////

// PRIVATE TYPES //////////////////////////////////////////////////////////////

// PRIVATE DATA ///////////////////////////////////////////////////////////////

// PUBLIC DATA ////////////////////////////////////////////////////////////////

// PRIVATE PROTOTYPES /////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// W3DFontLibrary::loadFontData ===============================================
/** Load a font */
//=============================================================================
Bool W3DFontLibrary::loadFontData( GameFont *font )
{
	// sanity
	if( font == nullptr )
		return FALSE;

	const char* name = font->nameString.str();
	const Int size = font->pointSize;
	const Bool bold = font->bold;

	// get the font data from the asset manager
	FontCharsClass *fontChar = WW3DAssetManager::Get_Instance()->Get_FontChars( name, size, bold );

	if( fontChar == nullptr )
	{
		DEBUG_CRASH(( "Unable to find font '%s' in Asset Manager", name ));
		return FALSE;
	}

	// assign font data
	font->fontData = fontChar;
	font->height = fontChar->Get_Char_Height();

	// load Unicode of same point size
	// GeneralsX @bugfix fbraz 03/06/2026 Prevent circular AlternateUnicodeFont chain.
	// Fonts in the fallback candidate list that DON'T need a fallback (full-coverage fonts)
	// should not get an AlternateUnicodeFont set, otherwise Get_Char_Data enters infinite
	// recursion: e.g. Arial → Arial Unicode MS → Arial → ...
	{
		bool skipFallback = false;
		// Skip fallback for fonts that already have full Unicode coverage themselves
		static const char *kFullCoverageFonts[] = {
			"Arial Unicode MS",
			"Arial Unicode",
			"DejaVu Sans",
			nullptr
		};
		for (int i = 0; kFullCoverageFonts[i]; i++) {
			if (strcmp(name, kFullCoverageFonts[i]) == 0) {
				skipFallback = true;
				break;
			}
		}
		if (!skipFallback) {
			fontChar->AlternateUnicodeFont = LoadUnicodeFallbackFont(size, bold, name);
		}
	}

	return TRUE;
}

// W3DFontLibrary::releaseFontData ============================================
/** Release font data */
//=============================================================================
void W3DFontLibrary::releaseFontData( GameFont *font )
{

	// presently we don't need to do anything because fonts are handled in
	// the W3D asset manager which is all taken for of us
	if (font && font->fontData)
	{
		if(((FontCharsClass *)(font->fontData))->AlternateUnicodeFont)
			((FontCharsClass *)(font->fontData))->AlternateUnicodeFont->Release_Ref();
		((FontCharsClass *)(font->fontData))->Release_Ref();

		font->fontData = nullptr;
	}

}

// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////

