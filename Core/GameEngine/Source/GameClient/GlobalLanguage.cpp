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

// FILE: GlobalLanguage.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Aug 2002
//
//	Filename: 	GlobalLanguage.cpp
//
//	author:		Chris Huybregts
//
//	purpose:	Contains the member functions for the language munkee
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"

#include <stdio.h>

#include "Common/AddonCompat.h"
#include "Common/INI.h"
#include "Common/Registry.h"
#include "Common/FileSystem.h"
#include "Common/OptionPreferences.h"

#include "GameClient/Display.h"
#include "GameClient/GlobalLanguage.h"

//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
GlobalLanguage *TheGlobalLanguageData = nullptr;

static const LookupListRec ResolutionFontSizeMethodNames[] =
{
	{ "CLASSIC", GlobalLanguage::ResolutionFontSizeMethod_Classic },
	{ "CLASSIC_NO_CEILING", GlobalLanguage::ResolutionFontSizeMethod_ClassicNoCeiling },
	{ "STRICT", GlobalLanguage::ResolutionFontSizeMethod_Strict },
	{ "BALANCED", GlobalLanguage::ResolutionFontSizeMethod_Balanced },
	{ nullptr, 0 }
};

static const FieldParse TheGlobalLanguageDataFieldParseTable[] =
{
	{ "UnicodeFontName",									INI::parseAsciiString,nullptr,									offsetof( GlobalLanguage, m_unicodeFontName ) },
	//{	"UnicodeFontFileName",							INI::parseAsciiString,nullptr,									offsetof( GlobalLanguage, m_unicodeFontFileName ) },
	{ "LocalFontFile",										GlobalLanguage::parseFontFileName,					nullptr,			0},
	{ "MilitaryCaptionSpeed",						INI::parseInt,					nullptr,		offsetof( GlobalLanguage, m_militaryCaptionSpeed ) },
	{ "UseHardWordWrap",						INI::parseBool,					nullptr,		offsetof( GlobalLanguage, m_useHardWrap) },
	{ "ResolutionFontAdjustment",						INI::parseReal,					nullptr,		offsetof( GlobalLanguage, m_resolutionFontSizeAdjustment) },
	{ "ResolutionFontSizeMethod", INI::parseLookupList, ResolutionFontSizeMethodNames, offsetof( GlobalLanguage, m_resolutionFontSizeMethod) },
	{ "CopyrightFont",					GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_copyrightFont ) },
	{ "MessageFont",					GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_messageFont) },
	{ "MilitaryCaptionTitleFont",		GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_militaryCaptionTitleFont) },
	{ "MilitaryCaptionDelayMS",					INI::parseInt,					nullptr,		offsetof( GlobalLanguage, m_militaryCaptionDelayMS ) },
	{ "MilitaryCaptionFont",			GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_militaryCaptionFont) },
	{ "SuperweaponCountdownNormalFont",	GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_superweaponCountdownNormalFont) },
	{ "SuperweaponCountdownReadyFont",	GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_superweaponCountdownReadyFont) },
	{ "NamedTimerCountdownNormalFont",	GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_namedTimerCountdownNormalFont) },
	{ "NamedTimerCountdownReadyFont",	GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_namedTimerCountdownReadyFont) },
	{ "DrawableCaptionFont",			GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_drawableCaptionFont) },
	{ "DefaultWindowFont",				GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_defaultWindowFont) },
	{ "DefaultDisplayStringFont",		GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_defaultDisplayStringFont) },
	{ "TooltipFontName",				GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_tooltipFontName) },
	{ "NativeDebugDisplay",				GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_nativeDebugDisplay) },
	{ "DrawGroupInfoFont",				GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_drawGroupInfoFont) },
	{ "CreditsTitleFont",				GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_creditsTitleFont) },
	{ "CreditsMinorTitleFont",				GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_creditsPositionFont) },
	{ "CreditsNormalFont",				GlobalLanguage::parseFontDesc,	nullptr,	offsetof( GlobalLanguage, m_creditsNormalFont) },

	{ nullptr,					nullptr,						nullptr,						0 }
};

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
void INI::parseLanguageDefinition( INI *ini )
{
	if( !TheGlobalLanguageData )
	{
		DEBUG_ASSERTCRASH(TheGlobalLanguageData, ("INI::parseLanguageDefinition - TheGlobalLanguage Data is not around, please create it before trying to parse the ini file."));
		return;
	}

	ini->initFromINI( TheGlobalLanguageData, TheGlobalLanguageDataFieldParseTable );
}

GlobalLanguage::GlobalLanguage()
{
	m_unicodeFontName.clear();
	m_unicodeFontFileName.clear();
	m_unicodeFontName.clear();
	m_militaryCaptionSpeed = 0;
	m_useHardWrap = FALSE;
	m_resolutionFontSizeAdjustment = 0.7f;
	m_resolutionFontSizeMethod = ResolutionFontSizeMethod_Default;
	m_militaryCaptionDelayMS = 750;

	m_userResolutionFontSizeAdjustment = -1.0f;
}

GlobalLanguage::~GlobalLanguage()
{
	StringList::iterator it = m_localFonts.begin();
	while( it != m_localFonts.end())
	{
		AsciiString font = *it;
		RemoveFontResource(font.str());
		//SendMessage( HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
		++it;
	}
}

void GlobalLanguage::init()
{
	// GeneralsX @bugfix GitHubCopilot 27/05/2026 Implement Language.ini fallback chain so stock values fill missing override keys.
	{
		AsciiString registryLanguage = GetRegistryLanguage();
		AsciiString fname;
		fname.format("Data\\%s\\Language", registryLanguage.str());

		INI ini;
		ini.loadFileDirectory( fname, INI_LOAD_OVERWRITE, nullptr );

		// GeneralsX @bugfix fbraz 04/06/2026 Only fall back to stock English if the primary language
		// did not set UnicodeFontName. This protects two scenarios:
		// 1) russifier mod overrides UnicodeFontName=Arial (no Cyrillic on macOS), so we need a stock
		//    English Language.ini to fill the field with Arial Unicode MS.
		// 2) official localizations (brazilian, etc.) provide their own UnicodeFontName and must not
		//    be overwritten by English.
		// Also skip the fallback entirely when Data\English\Language.ini is not present in the deploy,
		// otherwise INI::loadFileDirectory throws INI_CANT_OPEN_FILE on zero files read.
		if (m_unicodeFontName.isEmpty() && registryLanguage.compare("English") != 0)
		{
			AsciiString stockFname("Data\\English\\Language");
			AsciiString stockFnameWithExt = stockFname;
			stockFnameWithExt.concat(".ini");
			if (TheFileSystem->doesFileExist(stockFnameWithExt.str()))
			{
				ini.loadFileDirectory( stockFname, INI_LOAD_MULTIFILE, nullptr );
			}
		}
	}

	StringList::iterator it = m_localFonts.begin();
	while( it != m_localFonts.end())
	{
		AsciiString font = *it;
		if(AddFontResource(font.str()) == 0)
		{
			DEBUG_CRASH(("GlobalLanguage::init Failed to add font %s", font.str()));
		}
		++it;
	}

	// override values with user preferences
	OptionPreferences optionPref;
	m_userResolutionFontSizeAdjustment = optionPref.getResolutionFontAdjustment();
}

void GlobalLanguage::reset()
{
}

void GlobalLanguage::parseFontDesc(INI *ini, void *instance, void *store, const void *userData)
{
	FontDesc *fontDesc = (FontDesc *)store;
	fontDesc->name = ini->getNextQuotedAsciiString();
	fontDesc->size = ini->scanInt(ini->getNextToken());
	fontDesc->bold = ini->scanBool(ini->getNextToken());
}

void GlobalLanguage::parseFontFileName(INI *ini, void *instance, void *store, const void *userData)
{
	GlobalLanguage *globalLanguage = static_cast<GlobalLanguage *>(instance);
	AsciiString asciiString = ini->getNextAsciiString();
	globalLanguage->m_localFonts.push_front(asciiString);
}

Real GlobalLanguage::getResolutionFontSizeAdjustment() const
{
	if (m_userResolutionFontSizeAdjustment >= 0.0f)
		return m_userResolutionFontSizeAdjustment;
	else
		return m_resolutionFontSizeAdjustment;
}

Real GlobalLanguage::getResolutionFontSizeScale(ResolutionFontSizeMethod method, Real scaler)
{
	Real adjustFactor;

	switch (method)
	{
	default:
	case ResolutionFontSizeMethod_Classic:
	{
		// TheSuperHackers @info The original font scaling for this game.
		// Useful for not breaking legacy Addons and Mods. Scales poorly with large resolutions.
		adjustFactor = TheDisplay->getWidth() / (Real)DEFAULT_DISPLAY_WIDTH;
		adjustFactor = 1.0f + (adjustFactor - 1.0f) * scaler;
		if (adjustFactor > 2.0f)
			adjustFactor = 2.0f;
		break;
	}
	case ResolutionFontSizeMethod_ClassicNoCeiling:
	{
		// TheSuperHackers @feature The original font scaling, but without ceiling.
		// Useful for not changing the original look of the game. Scales alright with large resolutions.
		adjustFactor = TheDisplay->getWidth() / (Real)DEFAULT_DISPLAY_WIDTH;
		adjustFactor = 1.0f + (adjustFactor - 1.0f) * scaler;
		break;
	}
	case ResolutionFontSizeMethod_Strict:
	{
		// TheSuperHackers @feature The strict method scales fonts based on the smallest screen
		// dimension so they scale independent of aspect ratio.
		const Real wScale = TheDisplay->getWidth() / (Real)DEFAULT_DISPLAY_WIDTH;
		const Real hScale = TheDisplay->getHeight() / (Real)DEFAULT_DISPLAY_HEIGHT;
		adjustFactor = min(wScale, hScale);
		adjustFactor = 1.0f + (adjustFactor - 1.0f) * scaler;
		break;
	}
	case ResolutionFontSizeMethod_Balanced:
	{
		// TheSuperHackers @feature The balanced method evenly weighs the display width and height
		// for a balanced rescale on non 4:3 resolutions. The aspect ratio scaling is clamped to
		// prevent oversizing.
		constexpr const Real maxAspect = 1.8f;
		constexpr const Real minAspect = 1.0f;
		Real w = TheDisplay->getWidth();
		Real h = TheDisplay->getHeight();
		const Real aspect = w / h;
		Real wScale = w / (Real)DEFAULT_DISPLAY_WIDTH;
		Real hScale = h / (Real)DEFAULT_DISPLAY_HEIGHT;

		if (aspect > maxAspect)
		{
			// Recompute width at max aspect
			w = maxAspect * h;
			wScale = w / (Real)DEFAULT_DISPLAY_WIDTH;
		}
		else if (aspect < minAspect)
		{
			// Recompute height at min aspect
			h = minAspect * w;
			hScale = h / (Real)DEFAULT_DISPLAY_HEIGHT;
		}
		adjustFactor = (wScale + hScale) * 0.5f;
		adjustFactor = 1.0f + (adjustFactor - 1.0f) * scaler;
		break;
	}
	}

	if (adjustFactor < 1.0f)
		adjustFactor = 1.0f;

	return adjustFactor;
}

Int GlobalLanguage::adjustFontSize(Int theFontSize)
{
	// TheSuperHackers @todo This function is called very often.
	// Therefore cache the adjustFactor on resolution change to not recompute it on every call.
	const Real resolutionScaler = getResolutionFontSizeAdjustment();
	const Real adjustFactor = getResolutionFontSizeScale(m_resolutionFontSizeMethod, resolutionScaler);
	const Int pointSize = REAL_TO_INT_FLOOR(theFontSize * adjustFactor);

	return pointSize;
}

void GlobalLanguage::parseCustomDefinition()
{
	if (addon::HasFullviewportDat())
	{
		// TheSuperHackers @tweak xezon 19/08/2025 Force the classic font size adjustment for the old
		// 'Control Bar Pro' Addons because they use manual font upscaling in higher resolution packages.
		m_resolutionFontSizeMethod = ResolutionFontSizeMethod_Classic;
	}
}

FontDesc::FontDesc()
{
	name = "Arial Unicode MS";
	size = 12;
	bold = FALSE;
}
//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

